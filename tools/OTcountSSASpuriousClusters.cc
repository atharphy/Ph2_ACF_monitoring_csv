#include "tools/OTcountSSASpuriousClusters.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTcountSSASpuriousClusters::fCalibrationDescription = "Inject L1 and stubs for MPA8 + SSA0 and measure the ration of missing clusters";

OTcountSSASpuriousClusters::OTcountSSASpuriousClusters() : Tool() {}

OTcountSSASpuriousClusters::~OTcountSSASpuriousClusters() {}

void OTcountSSASpuriousClusters::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    fNumberOfEvents     = findValueInSettings<double>("OTcountSSASpuriousClusters_NumberOfEvents", 1e6);

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, 8>>(*fDetectorContainer, fStubMissingCountContainer);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTcountSSASpuriousClusters.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTcountSSASpuriousClusters::ConfigureCalibration() {}

void OTcountSSASpuriousClusters::Running()
{
    // Assumes 1 board per Ph2_ACF instance
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;
    LOG(INFO) << "Starting OTcountSSASpuriousClusters measurement.";
    Initialise();
    runIntegrityTest();
    fillHistograms();
    LOG(INFO) << "Done with OTcountSSASpuriousClusters.";
    Reset();
}

void OTcountSSASpuriousClusters::Stop(void)
{
    LOG(INFO) << "Stopping OTcountSSASpuriousClusters measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTcountSSASpuriousClusters.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTcountSSASpuriousClusters stopped.";
}

void OTcountSSASpuriousClusters::Pause() {}

void OTcountSSASpuriousClusters::Resume() {}

void OTcountSSASpuriousClusters::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTcountSSASpuriousClusters::runIntegrityTest()
{
    LOG(INFO) << BOLDYELLOW << "OTverifyCICdataWord::runIntegrityTest ... start integrity test" << RESET;

    for(auto theBoard: *fDetectorContainer)
    {
        auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
        runStubIntegrityTest(theBoard, theFWInterface);
    }
}

void OTcountSSASpuriousClusters::runStubIntegrityTest(BeBoard* theBoard, D19cFWInterface* theFWInterface)
{
    LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest" << RESET;

    prepareForStubInjection(theBoard);

    bool                           is10G      = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theBoard->getFirstObject()->flpGBT) == 10);
    std::vector<std::vector<Stub>> stubInformationList;
    uint8_t                        numberOfBytesInSinglePacket;

    stubInformationList         = createPSstubList();
    numberOfBytesInSinglePacket = is10G ? 2 : 1;

    size_t stubPatternCounter = 0;
    size_t numberOfPatterns   = stubInformationList.size();
    for(auto theStubList: stubInformationList)
    {
        LOG(INFO) << BOLDMAGENTA << "    Stub pattern " << stubPatternCounter + 1 << " out of " << numberOfPatterns << RESET;
        uint8_t chipId = 0;
        LOG(INFO) << BOLDBLUE << "        Injecting stubs on " << "MPA" << " " << chipId + 8 << " for all hybrids" << RESET;
        BoardDataContainer thePatternMatcherContainer;
        ContainerFactory::copyAndInitHybrid<PatternMatcher>(*theBoard, thePatternMatcherContainer);

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                prepareCICforStubIntegrityTest(theHybrid, chipId);
                injectStubsPS(theHybrid->getObject(chipId + 8), numberOfBytesInSinglePacket, theStubList);
            }
        }

        uint32_t burstNumbers;
        uint32_t lastBurstNumberOfEvents;
        uint32_t numberOfEventsPerBurst = 65000;

        burstNumbers            = fNumberOfEvents / numberOfEventsPerBurst;
        lastBurstNumberOfEvents = numberOfEventsPerBurst;
        if(fNumberOfEvents % numberOfEventsPerBurst > 0)
        {
            ++burstNumbers;
            lastBurstNumberOfEvents = fNumberOfEvents % numberOfEventsPerBurst;
        }

        while(burstNumbers > 0)
        {
            uint32_t currentNumberOfEvents = uint32_t(numberOfEventsPerBurst);
            if(burstNumbers == 1) currentNumberOfEvents = lastBurstNumberOfEvents;
            
            if(Tool::ifUseReadNEvents())
                Tool::ReadNEvents(fDetectorContainer->getObject(theBoard->getId()), currentNumberOfEvents);
            // Loop over Events from this Acquisition
            const std::vector<Event*>& eventList = Tool::GetEvents();
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    float& theMissingClusterCounter = fStubMissingCountContainer.getHybrid(theHybrid->getBeBoardId(), theHybrid->getOpticalGroupId(), theHybrid->getId())
                    ->getSummary<GenericDataArray<float, 8>>().at(stubPatternCounter);
                    for(auto event: eventList)
                    {
                        auto chipStubVector = static_cast<D19cCic2Event*>(event)->StubVector(theHybrid->getId(), chipId + 8);
                        if(chipStubVector.size() == 1)
                        {
                            if(chipStubVector[0].getBend() == theStubList[0].fBend && chipStubVector[0].getPosition() == theStubList[0].fPosition, chipStubVector[0].getRow() == theStubList[0].fRow)
                            {
                                ++theMissingClusterCounter;
                            }
                        }
                    }
                }
            }
            --burstNumbers;
        }
        ++stubPatternCounter;
    }

    for(auto theBoard: fStubMissingCountContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto& theValue: theHybrid->getSummary<GenericDataArray<float, 8>>())
                {
                    theValue = float((fNumberOfEvents - theValue)/fNumberOfEvents);
                }
            }
        }
    }
}

uint8_t OTcountSSASpuriousClusters::prepareCICforStubIntegrityTest(Hybrid* theHybrid, uint8_t chipId)
{
    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    fCicInterface->SelectOutput(cCic, false);
    auto theFeConfigRegisterValue = fCicInterface->ReadChipReg(cCic, "FE_CONFIG");
    theFeConfigRegisterValue |= 0x04; // Force bending to be sent out in the stub stream
    fCicInterface->WriteChipReg(cCic, "FE_CONFIG", theFeConfigRegisterValue);
    auto theChipToCICMapping = cCic->getMapping();
    fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    uint8_t chipIdForCIC = theChipToCICMapping.at(chipId);
    fCicInterface->EnableFEs(cCic, {uint8_t(chipId)}, true);

    return chipIdForCIC;
}


void OTcountSSASpuriousClusters::fillHistograms()
{
#ifdef __USE_ROOT__
    fDQMHistogramOTcountSSASpuriousClusters.fillStubMissingCountContainer(fStubMissingCountContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theStubMissingCountContainerSerialization("OTcountSSASpuriousClusterstubMissingCountContainer");
        theStubMissingCountContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fStubMissingCountContainer);
    }
#endif
}

void OTcountSSASpuriousClusters::setStubLogicParameters(ReadoutChip* theMPA)
{
    fReadoutChipInterface->WriteChipReg(theMPA, "StubWindow", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "StubMode", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM10", fBendingToCode.at(0));
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeDM8", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM76", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM54", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM32", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP12", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP34", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP56", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP78", 0);
}

std::vector<std::vector<Stub>> OTcountSSASpuriousClusters::createPSstubList()
{
    std::vector<std::vector<Stub>> theListOfStubInjections;

    for(const auto& theStripCluster: fListOfInjectedStrips)
    {
        std::vector<Stub> theSubList{Stub(theStripCluster.fFirstCol * 2 + theStripCluster.fColWidth / 2, fBendingToCode.at(0), 0xf)};
        theListOfStubInjections.push_back(theSubList);
    }

    return theListOfStubInjections;
}

void OTcountSSASpuriousClusters::setStripOffsetParameters(Ph2_HwDescription::ReadoutChip* theSSA)
{
    std::vector<std::pair<std::string, uint16_t>> stripOffsetRegisters{{"StripOffset_byte0", 0}, {"StripOffset_byte1", 0}, {"StripOffset_byte2", 0}, {"StripOffset_byte3", 0}};

    fReadoutChipInterface->WriteChipMultReg(theSSA, stripOffsetRegisters);

    if(theSSA->getId() == 0 || theSSA->getId() == 7)
    {
        if(theSSA->getId() == 0)
        {
            auto theStripOffsetByte0 = fReadoutChipInterface->ReadChipReg(theSSA, "StripOffset_byte0");
            theStripOffsetByte0 = (theStripOffsetByte0 & 0x7) | (0x10 << 3);
            fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte0", theStripOffsetByte0);
        }
        else if (theSSA->getId() == 7)
        {
            auto theStripOffsetByte3 = fReadoutChipInterface->ReadChipReg(theSSA, "StripOffset_byte3");
            theStripOffsetByte3 = (theStripOffsetByte3 & 0x80) | (0x0F << 2);
            fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte3", theStripOffsetByte3);
        }
    }
}

void OTcountSSASpuriousClusters::prepareForStubInjection(Ph2_HwDescription::BeBoard* theBoard)
{
    fListOfInjectedStrips = produceStripClusterList();

    for(auto theOpticalGroup: *theBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            for(auto theChip: *theHybrid)
            {
                if(theChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    setStripOffsetParameters(theChip);
                    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, fListOfInjectedStrips);
                }
                else if(theChip->getFrontEndType() == FrontEndType::MPA2)
                {
                    fReadoutChipInterface->WriteChipReg(theChip, "StubMode", 0); // Use normal stub mode
                    setStubLogicParameters(theChip);
                }
            }
        }
    }
}

void OTcountSSASpuriousClusters::injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t numberOfBytesInSinglePacket, const std::vector<Stub>& listOfStubs)
{
    if(listOfStubs.size() != 1)
    {
        std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] listOfStubs must be exactly one to test SSA to MPA cluster lines! Aborting..." << std::endl;
        abort();
    }

    uint8_t rowCoordinate = listOfStubs.at(0).fRow;
    uint8_t colCoordinate = listOfStubs.at(0).fPosition;

    std::vector<Cluster> thePixelClusterList = produceMatchingPixelClusterList(rowCoordinate, colCoordinate);
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, thePixelClusterList);
}

std::vector<Cluster> OTcountSSASpuriousClusters::produceStripClusterList()
{
    size_t               numberOfSSAstubClusterLines = 8;
    std::vector<Cluster> theStripClusterList;
    for(size_t stripIt = 0; stripIt < numberOfSSAstubClusterLines; ++stripIt) // injecting 8 clusters of size 1 15 strips spaced
    {
        theStripClusterList.push_back(Cluster(0, fFirstStrip + fStripGap * stripIt, 1));
    }

    return theStripClusterList;
}

std::vector<Cluster> OTcountSSASpuriousClusters::produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed)
{
    std::vector<Cluster> thePixelClusterList;
    thePixelClusterList.push_back(Cluster(stubRow, stubSeed / 2, 1 + stubSeed % 2));
    return thePixelClusterList;
}
