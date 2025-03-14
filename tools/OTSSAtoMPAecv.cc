#include "tools/OTSSAtoMPAecv.h"
#include "HWDescription/ReadoutChip.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/MPA2Interface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTSSAtoMPAecv::fCalibrationDescription = "Scan phase and strenght on the SSA to MPA lines";

OTSSAtoMPAecv::OTSSAtoMPAecv() : OTverifyMPASSAdataWord() {}

OTSSAtoMPAecv::~OTSSAtoMPAecv() {}

void OTSSAtoMPAecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fFirstStrip = 6;
    fStripGap   = 6;
    // free the registers in case any
    fNumberOfStubBits      = findValueInSettings<double>("OTverifyCICdataWord_NumberOfTestedStubBits", 1e8);
    fNumberOfL1Bits        = findValueInSettings<double>("OTverifyCICdataWord_NumberOfTestedL1Bits", 1e6);
    fListOfSSAslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>("OTSSAtoMPAecv_ListOfSSAslvsCurrents", "1, 4, 7"));

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTSSAtoMPAecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTSSAtoMPAecv::ConfigureCalibration() {}

void OTSSAtoMPAecv::Running()
{
    LOG(INFO) << "Starting OTSSAtoMPAecv measurement.";
    Initialise();
    runSSAtoMPAecvScan();
    LOG(INFO) << "Done with OTSSAtoMPAecv.";
    Reset();
}

void OTSSAtoMPAecv::Stop(void)
{
    LOG(INFO) << "Stopping OTSSAtoMPAecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTSSAtoMPAecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTSSAtoMPAecv stopped.";
}

void OTSSAtoMPAecv::Pause() {}

void OTSSAtoMPAecv::Resume() {}

void OTSSAtoMPAecv::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTSSAtoMPAecv::runSSAtoMPAecvScan()
{
    auto        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string selectMPAfunctionName = "SelectMPAfunction";
    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);
    ContainerFactory::copyAndInitChip<std::pair<uint8_t, uint8_t>>(*fDetectorContainer, fOriginalPhaseContainer);

    // Reading original phases
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    auto& theOriginalPhasePair =
                        fOriginalPhaseContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::pair<uint8_t, uint8_t>>();
                    uint8_t phase320registerValue = fReadoutChipInterface->ReadChipReg(theChip, "LatencyRx320") & 0x7;
                    uint8_t phase40registerValue  = fReadoutChipInterface->ReadChipReg(theChip, "LatencyRx40");
                    theOriginalPhasePair.first    = (phase320registerValue & 0x7) | (phase40registerValue & 0x3) << 3; // Start phase
                    theOriginalPhasePair.second   = (phase320registerValue & 0x7) | (phase40registerValue & 0xC) << 1; // Restart phase
                }
            }
        }
    }

    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);
    for(uint8_t slvsCurrent: fListOfSSAslvsCurrents)
    {
        LOG(INFO) << BOLDGREEN << "Scanning SSA SLVS current = " << +slvsCurrent << RESET;
        auto        SSAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
        std::string theSSAqueryFunctionString = "SSAqueryFunction";
        // settings for SSAs
        fDetectorContainer->addReadoutChipQueryFunction(SSAqueryFunction, theSSAqueryFunctionString);
        setSameDac("SLVS_pad_current_L1", slvsCurrent);
        setSameDac("SLVS_pad_current_Stub_0_1", slvsCurrent | (slvsCurrent << 3));
        setSameDac("SLVS_pad_current_Stub_2_3", slvsCurrent | (slvsCurrent << 3));
        setSameDac("SLVS_pad_current_Stub_4_5", slvsCurrent | (slvsCurrent << 3));
        setSameDac("SLVS_pad_current_Stub_6_7", slvsCurrent | (slvsCurrent << 3));
        fDetectorContainer->removeReadoutChipQueryFunction(theSSAqueryFunctionString);
        runSSAtoMPAecvScanForStubs(slvsCurrent);
        runSSAtoMPAecvScanForL1(slvsCurrent);
    }
}

void OTSSAtoMPAecv::runSSAtoMPAecvScanForStubs(uint8_t slvsCurrent)
{
    LOG(INFO) << BOLDGREEN << "Scanning SSA to MPA stub phases" << RESET;
    for(uint8_t clockEdge = 0; clockEdge < 2; ++clockEdge)
    {
        LOG(INFO) << BOLDGREEN << "MPA sampling clockEdge = " << +clockEdge << RESET;

        resetPatternMatchingEfficiencyContainer();

        // setting phases and driver strenghts
        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theChip: *theHybrid)
                    {
                        if(theChip->getFrontEndType() == FrontEndType::MPA2)
                        {
                            auto    theMPAInterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;
                            uint8_t registerValue;
                            if(clockEdge == 0)
                                registerValue = 0x00;
                            else
                                registerValue = 0xFF;
                            theMPAInterface->WriteChipReg(theChip, "EdgeSelTrig", registerValue);
                        }
                    }
                }
            }
            // auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
            // runStubIntegrityTestPS(theBoard, theFWInterface);
        }

#ifdef __USE_ROOT__
        fDQMHistogramOTSSAtoMPAecv.fillStubPatternEfficiencyScan(fPatternMatchingEfficiencyContainer, clockEdge, slvsCurrent);
#else
        if(fDQMStreamerEnabled)
        {
            ContainerSerialization thePatternMatchingEfficiencyContainerSerialization("OTSSAtoMPAecvStubPatternMatchingEfficiency");
            thePatternMatchingEfficiencyContainerSerialization.streamByHybridContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer, clockEdge, slvsCurrent);
        }
#endif
    }
}

void OTSSAtoMPAecv::runSSAtoMPAecvScanForL1(uint8_t slvsCurrent)
{
    LOG(INFO) << BOLDGREEN << "Scanning SSA to MPA L1 phases" << RESET;

    for(uint8_t clockEdge = 0; clockEdge < 2; ++clockEdge)
    {
        for(int samplingPhaseOffset = fMinimum320PhaseShift; samplingPhaseOffset <= fMaximum320PhaseShift; ++samplingPhaseOffset)
        {
            LOG(INFO) << BOLDGREEN << "MPA sampling clockEdge = " << +clockEdge << " and samplingPhaseOffset = " << samplingPhaseOffset << RESET;
            DetectorDataContainer thePossiblePhaseFlagContainer;
            bool                  initialFlags          = true;
            auto                  selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
            std::string           selectMPAfunctionName = "SelectMPAfunction";
            fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);
            ContainerFactory::copyAndInitChip<bool>(*fDetectorContainer, thePossiblePhaseFlagContainer, initialFlags);
            fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);
            resetPatternMatchingEfficiencyContainer();
            for(auto theBoard: *fDetectorContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        for(auto theChip: *theHybrid)
                        {
                            if(theChip->getFrontEndType() == FrontEndType::MPA2)
                            {
                                auto    theMPAInterface = static_cast<PSInterface*>(fReadoutChipInterface)->fTheMPA2Interface;
                                uint8_t registerValue;
                                if(clockEdge == 0)
                                    registerValue = 0x0;
                                else
                                    registerValue = 0x1;
                                theMPAInterface->WriteChipRegBits(theChip, "EdgeSelT1Raw", registerValue, "Mask", 0x01);
                                auto theOriginalPhasePair =
                                    fOriginalPhaseContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::pair<uint8_t, uint8_t>>();
                                uint8_t newOffsetStart   = theOriginalPhasePair.first + samplingPhaseOffset;
                                uint8_t newOffsetRestart = theOriginalPhasePair.second + samplingPhaseOffset;
                                if(newOffsetStart > 0x1F || newOffsetStart < (-samplingPhaseOffset) || newOffsetRestart > 0x1F || newOffsetRestart < (-samplingPhaseOffset))
                                {
                                    LOG(ERROR) << BOLDYELLOW << "ERROR: impossible to apply samplingPhaseOffset = " << samplingPhaseOffset << " - going out of range" << RESET;
                                    thePossiblePhaseFlagContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<bool>() = false;
                                    continue;
                                }
                                theMPAInterface->WriteChipRegBits(theChip, "LatencyRx320", newOffsetStart & 0x7, "Mask", 0x07);
                                uint8_t phase40LatencyRegister = ((newOffsetStart & 0x18) >> 3) | ((newOffsetRestart & 0x18) >> 1);
                                theMPAInterface->WriteChipRegBits(theChip, "LatencyRx40", phase40LatencyRegister, "Mask", 0x0F);
                            }
                        }
                    }
                }
                auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
                runL1IntegrityTest(theBoard, theFWInterface);
            }

            // removing not possible phase
            for(auto theBoard: thePossiblePhaseFlagContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        for(auto theChip: *theHybrid)
                        {
                            if(!theChip->getSummary<bool>())
                                fPatternMatchingEfficiencyContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())
                                    ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>()
                                    .at(theChip->getId() % 8)
                                    .at(0) = -1;
                        }
                    }
                }
            }
#ifdef __USE_ROOT__
            fDQMHistogramOTSSAtoMPAecv.fillL1PatternEfficiencyScan(fPatternMatchingEfficiencyContainer, clockEdge, slvsCurrent, samplingPhaseOffset);
#else
            if(fDQMStreamerEnabled)
            {
                ContainerSerialization thePatternMatchingEfficiencyContainerSerialization("OTSSAtoMPAecvL1PatternMatchingEfficiency");
                thePatternMatchingEfficiencyContainerSerialization.streamByHybridContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer, clockEdge, slvsCurrent, samplingPhaseOffset);
            }
#endif
        }
    }
}

std::vector<Cluster> OTSSAtoMPAecv::produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed)
{
    std::vector<Cluster> thePixelClusterList;
    thePixelClusterList.push_back(Cluster(stubRow, stubSeed / 2, 1)); // matching strip cluster
    uint8_t              centroidCode              = stubSeed + 9;
    uint8_t              centroidCodeNegativeShift = centroidCode >> 1;
    uint8_t              centroidCodePositiveShift = centroidCode << 1;
    std::vector<uint8_t> centroidList{centroidCodeNegativeShift, centroidCodePositiveShift};
    for(auto centroid: centroidList) { thePixelClusterList.push_back(Cluster(fStubRowCoordinate, (centroid - 9) >> 1, (centroid - 9) % 2 + 1)); }

    return thePixelClusterList;
}

void OTSSAtoMPAecv::matchAllPossibleStubPatterns(uint8_t                                        numberOfBytesInSinglePacket,
                                                 size_t                                         numberOfLines,
                                                 std::vector<std::pair<PatternMatcher, float>>& thePatternAndEfficiencyList,
                                                 const std::vector<uint32_t>&                   concatenatedStubPackage,
                                                 Ph2_HwDescription::ReadoutChip*                theMPA)
{
    for(auto& thePatternAndEfficiency: thePatternAndEfficiencyList)
    {
        float maximumNumberOfMatchedStubs = 0;
        for(uint8_t numberOfPacketsToSkip = 0; numberOfPacketsToSkip < numberOfLines * 8; ++numberOfPacketsToSkip)
        {
            float                 currentNumberOfMatchedStubs = 0;
            std::vector<uint32_t> theShiftedWordVector        = applyByteShift(concatenatedStubPackage, numberOfBytesInSinglePacket, numberOfPacketsToSkip);
            for(size_t stubNumber = 0; stubNumber < 8; ++stubNumber)
            {
                if(thePatternAndEfficiency.first.isSubsetMatched(theShiftedWordVector, 29 + 21 * stubNumber, 21)) ++currentNumberOfMatchedStubs;
            }
            if(maximumNumberOfMatchedStubs < currentNumberOfMatchedStubs) maximumNumberOfMatchedStubs = currentNumberOfMatchedStubs;
            if(currentNumberOfMatchedStubs == 8) break;
        }
        thePatternAndEfficiency.second += (maximumNumberOfMatchedStubs / 8);
    }

    return;
}

void OTSSAtoMPAecv::resetPatternMatchingEfficiencyContainer()
{
    for(auto theBoard: fPatternMatchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup) { theHybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>() = GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>(); }
        }
    }
}
