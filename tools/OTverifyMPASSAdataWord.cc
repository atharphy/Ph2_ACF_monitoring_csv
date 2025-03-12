#include "tools/OTverifyMPASSAdataWord.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyMPASSAdataWord::fCalibrationDescription = "Inject L1 and stubs for each MPA + SSA and verify that CIC output corresponds to the expected pattern (calibration skipped for 2S)";

OTverifyMPASSAdataWord::OTverifyMPASSAdataWord() : OTverifyCICdataWord() {}

OTverifyMPASSAdataWord::~OTverifyMPASSAdataWord() {}

void OTverifyMPASSAdataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    fNumberOfStubBits = findValueInSettings<double>("OTverifyMPASSAdataWord_NumberOfTestedStubBits", 1e8);
    fNumberOfL1Bits   = findValueInSettings<double>("OTverifyMPASSAdataWord_NumberOfTestedL1Bits", 1e6);
    fDoMatchingInFirmware = findValueInSettings<double>("OTverifyMPASSAdataWord_DoMatchingInFirmware", 1) > 0;

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9, 2>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

    setUpPatternMatching();
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyMPASSAdataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyMPASSAdataWord::ConfigureCalibration() {}

void OTverifyMPASSAdataWord::Running()
{
    // Assumes 1 board per Ph2_ACF instance
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;
    LOG(INFO) << "Starting OTverifyMPASSAdataWord measurement.";
    Initialise();
    runIntegrityTest();
    fillHistograms();
    LOG(INFO) << "Done with OTverifyMPASSAdataWord.";
    Reset();
}

void OTverifyMPASSAdataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTverifyMPASSAdataWord measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTverifyMPASSAdataWord.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTverifyMPASSAdataWord stopped.";
}

void OTverifyMPASSAdataWord::Pause() {}

void OTverifyMPASSAdataWord::Resume() {}

void OTverifyMPASSAdataWord::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTverifyMPASSAdataWord::fillHistograms()
{
#ifdef __USE_ROOT__
    fDQMHistogramOTverifyMPASSAdataWord.fillPatternMatchingEfficiencyResults(fPatternMatchingEfficiencyContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTverifyMPASSAdataWordPatternMatchingEfficiency");
        thePatternMatchinEfficiencyContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer);
    }
#endif
}

void OTverifyMPASSAdataWord::injectL1PS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket)
{
    ReadoutChip* theSSA = nullptr;
    try
    {
        theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId() % 8);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << YELLOW << "            skipping MPA Id " << +theMPA->getId() << " since corresponding SSA is not enabled" << RESET;
        return;
    }

    LOG(INFO) << BOLDBLUE << "            injecting clusters on MPA Id " << +theMPA->getId() << " and SSA " << +theSSA->getId() << RESET;

    auto& theL1Efficiency = fPatternMatchingEfficiencyContainer.getObject(theMPA->getBeBoardId())
                                ->getObject(theMPA->getOpticalGroupId())
                                ->getObject(theMPA->getHybridId())
                                ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9, 2>>()
                                .at(theMPA->getId() % 8)
                                .at(0);

    std::vector<Cluster> thePixelClusterList{Cluster(0xA, 0x55, 2)};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, thePixelClusterList);

    std::vector<Cluster> theStripClusterList{Cluster(0x0, 0x25, 2)};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theSSA, theStripClusterList);

    PatternMatcher thePatternMatcher  = produceL1PatternMatcher(thePixelClusterList, theStripClusterList, numberOfBytesInSinglePacket, chipIdForCIC);
    float          testedBitNumber    = thePatternMatcher.getNumberOfMaskedBits();
    size_t         numberOfIterations = std::ceil(fNumberOfL1Bits / testedBitNumber);

    for(size_t iteration = 0; iteration < numberOfIterations; iteration++)
    {
        auto lineOutputVector        = theFWInterface->L1ADebug(1, false);
        auto orderedLineOutputVector = reorderPattern(lineOutputVector, numberOfBytesInSinglePacket);
        // std::cout << "L1 Line -> " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << std::endl;
        if(matchL1Pattern(orderedLineOutputVector, thePatternMatcher, numberOfBytesInSinglePacket))
        {
            ++theL1Efficiency.at(1);
            // LOG(INFO) << GREEN << "L1 pattern received " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "OTverifyMPASSAdataWord::injectL1PS - Error, expected L1 pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup "
                       << +theMPA->getOpticalGroupId() << " Hybrid " << +theMPA->getHybridId() << " MPA " << +theMPA->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "L1 data received    " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern expected " << getPatternPrintout(thePatternMatcher.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern mask     " << getPatternPrintout(thePatternMatcher.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
    }

    theL1Efficiency.at(0) = testedBitNumber;
    theL1Efficiency.at(1) = theL1Efficiency.at(1) - theL1Efficiency.at(0);
}

void OTverifyMPASSAdataWord::setStubLogicParameters(ReadoutChip* theMPA)
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

std::vector<std::vector<Stub>> OTverifyMPASSAdataWord::createPSstubList()
{
    std::vector<std::vector<Stub>> theListOfStubInjections;

    for(const auto& theStripCluster: fListOfInjectedStrips)
    {
        std::vector<Stub> theSubList {Stub(theStripCluster.fFirstCol * 2 + theStripCluster.fColWidth / 2, fBendingToCode.at(0), 5)};
        theListOfStubInjections.push_back(theSubList);
    }

    return theListOfStubInjections;
}

void OTverifyMPASSAdataWord::prepareForStubInjection(Ph2_HwDescription::BeBoard* theBoard)
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
                    fReadoutChipInterface->WriteChipReg(theChip, "StripOffset_byte0", 0);
                    fReadoutChipInterface->WriteChipReg(theChip, "StripOffset_byte1", 0);
                    fReadoutChipInterface->WriteChipReg(theChip, "StripOffset_byte2", 0);
                    fReadoutChipInterface->WriteChipReg(theChip, "StripOffset_byte3", 0);
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

void OTverifyMPASSAdataWord::injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t numberOfBytesInSinglePacket, const std::vector<Stub>& listOfStubs)
{
    if(listOfStubs.size() != 1)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] listOfStubs must be exactly one to test SSA to MPA cluster lines! Aborting..." << std::endl;
        abort();
    }

    LOG(INFO) << BOLDBLUE << "            injecting stubs on MPA " << +theMPA->getId() << " and SSA " << theMPA->getId() - 8 <<  " on OpticalGroup " << +theMPA->getOpticalGroupId() << " Hybdrid " << +theMPA->getHybridId() << RESET;

    uint8_t rowCoordinate = listOfStubs.at(0).fZ;
    uint8_t colCoordinate = listOfStubs.at(0).fSeed;

    std::vector<Cluster> thePixelClusterList = produceMatchingPixelClusterList(rowCoordinate, colCoordinate);
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, thePixelClusterList);
}

PatternMatcher OTverifyMPASSAdataWord::producePatternMatcherPS(uint8_t chipIdForCIC, uint8_t numberOfBytesInSinglePacket, const std::vector<Stub>& listOfStubs)
{
    if(listOfStubs.size() != 1)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] listOfStubs must be exactly one to test SSA to MPA cluster lines! Aborting..." << std::endl;
        abort();
    }
    size_t numberOfStubs     = 8 * listOfStubs.size();

    PatternMatcher thePattern;
    thePattern.addToPattern(0x1, 0x1, 1);      // is PS flag
    thePattern.addToPattern(0x0, 0x1FF, 9);    // status bits
    thePattern.addToPattern(0x000, 0x000, 12); // Bx ID
    thePattern.addToPattern(numberOfStubs, 0x3F, 6);

    for(uint8_t bxOffset = 0; bxOffset < 8; ++bxOffset)
    {
        for(auto theStub: listOfStubs)
        {
            thePattern.addToPattern(0x0, 0x0, 3);                                     // BX offset
            thePattern.addToPattern(chipIdForCIC, 0x7, 3);                            // Chip ID
            thePattern.addToPattern(theStub.fSeed + 2, 0xFF, 8);               // seed
            thePattern.addToPattern(theStub.fBend, 0x7, 3); // bending
            thePattern.addToPattern(theStub.fZ, 0xF, 4);                    // z
        }
    }

    thePattern.addTrailingZeros(numberOfBytesInSinglePacket * 64 * 6);
    return thePattern;
}

PatternMatcher OTverifyMPASSAdataWord::injectStubsPSOld(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t chipIdForCIC, uint8_t numberOfBytesInSinglePacket)
{
    ReadoutChip* theSSA = nullptr;
    try
    {
        theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId() % 8);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << YELLOW << "            skipping MPA Id " << +theMPA->getId() << " since corresponding SSA is not enabled" << RESET;
        return PatternMatcher();
    }

    LOG(INFO) << BOLDBLUE << "            injecting stubs on MPA Id " << +theMPA->getId() << " and SSA " << +theSSA->getId() << RESET;

    // size_t numberOfLines = 6;

    auto& theLineEfficiencyArray = fPatternMatchingEfficiencyContainer.getObject(theMPA->getBeBoardId())
                                       ->getObject(theMPA->getOpticalGroupId())
                                       ->getObject(theMPA->getHybridId())
                                       ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>()
                                       .at(theMPA->getId() % 8);

    fReadoutChipInterface->WriteChipReg(theMPA, "StubMode", 0); // Use normal stub mode

    fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte0", 0);
    fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte1", 0);
    fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte2", 0);
    fReadoutChipInterface->WriteChipReg(theSSA, "StripOffset_byte3", 0);
    setStubLogicParameters(theMPA);

    auto theStripClusterList = produceStripClusterList();
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theSSA, theStripClusterList);

    size_t stripClusterLine = 0;
    for(auto stripCluster: theStripClusterList)
    {
        auto& theStubEfficiency = theLineEfficiencyArray.at(stripClusterLine + 1);
        LOG(INFO) << BOLDBLUE << "                injecting stub for strip cluster line #" << stripClusterLine << RESET;
        uint8_t colCoordinate = stripCluster.fFirstCol;

        std::vector<Cluster> thePixelClusterList = produceMatchingPixelClusterList(fStubRowCoordinate, colCoordinate);
        static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, thePixelClusterList);

        // stubs need to be ordered by bending code (remember that the CIC orders them based on the Code[MP]XX values)
        std::vector<std::vector<Stub>> possibleStubVectorList = producePossibleStubVectorList(thePixelClusterList);

        std::vector<std::pair<PatternMatcher, float>> thePatternAndEfficiencyList;
        for(auto& theStubVector: possibleStubVectorList)
            thePatternAndEfficiencyList.emplace_back(std::make_pair(produceStubPatternMatcher(theStubVector, numberOfBytesInSinglePacket, chipIdForCIC), 0.));

        float testedBitNumber = thePatternAndEfficiencyList.at(0).first.getNumberOfMaskedBits();
        // size_t numberOfIterations = std::ceil(fNumberOfStubBits / testedBitNumber);

        // for(size_t iteration = 0; iteration < numberOfIterations; iteration++)
        // {
        //     auto                  lineOutputVector        = theFWInterface->StubDebug(true, numberOfLines, false);
        //     std::vector<uint32_t> concatenatedStubPackage = mergeCICStubOuput(lineOutputVector, numberOfBytesInSinglePacket);
        //     matchAllPossibleStubPatterns(numberOfBytesInSinglePacket, numberOfLines, thePatternAndEfficiencyList, concatenatedStubPackage, theMPA);
        // }
        ++stripClusterLine;
        float maximumEfficiency = 0;
        for(auto& thePatternAndEfficiency: thePatternAndEfficiencyList)
        {
            if(thePatternAndEfficiency.second > maximumEfficiency) maximumEfficiency = thePatternAndEfficiency.second;
        }
        theStubEfficiency = maximumEfficiency / testedBitNumber;
    }
    return PatternMatcher();
}

std::vector<Cluster> OTverifyMPASSAdataWord::produceStripClusterList()
{
    size_t                                             numberOfSSAstubClusterLines = 8;
    std::vector<Cluster> theStripClusterList;
    for(size_t stripIt = 0; stripIt < numberOfSSAstubClusterLines; ++stripIt) // injecting 8 clusters of size 1 15 strips spaced
    {
        theStripClusterList.push_back(Cluster(0, fFirstStrip + fStripGap * stripIt, 1));
    }

    return theStripClusterList;
}

std::vector<Cluster> OTverifyMPASSAdataWord::produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed)
{
    std::vector<Cluster> thePixelClusterList;
    thePixelClusterList.push_back(Cluster(stubRow, stubSeed / 2, 1 + stubSeed % 2));
    return thePixelClusterList;
}

std::vector<std::vector<Stub>> OTverifyMPASSAdataWord::producePossibleStubVectorList(const std::vector<Cluster>& thePixelClusterList)
{
    std::vector<std::vector<Stub>> possibleStubVectorList;
    for(const auto& thePixelCluster: thePixelClusterList)
    {
        std::vector<Stub> theStubVector{Stub(thePixelCluster.fFirstCol * 2 + thePixelCluster.fColWidth - 1, 0, fStubRowCoordinate)};
        possibleStubVectorList.push_back(theStubVector);
    }

    return possibleStubVectorList;
}

PatternMatcher OTverifyMPASSAdataWord::produceStubPatternMatcher(const std::vector<Stub>& theStubVector, uint8_t numberOfBytesInSinglePacket, uint8_t chipIdForCIC)
{
    size_t numberOfStubs     = 8 * theStubVector.size();
    size_t maximumStubNumber = (numberOfBytesInSinglePacket == 1) ? 16 : 35; // 16 if a 5G, 35 if a 10G
    if(numberOfStubs > maximumStubNumber)                                    // CIC aligns stubs by bending, but in pixel-pixel mode bending is 0 and it is not possible to know what the CIC will drop
    {
        std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] PS stube injected using pixel-pixel mode, more stubs than the maximum allowed!" << std::endl;
        abort();
    }

    PatternMatcher thePattern;
    thePattern.addToPattern(0x1, 0x1, 1);      // is PS flag
    thePattern.addToPattern(0x0, 0x1FF, 9);    // status bits
    thePattern.addToPattern(0x000, 0x000, 12); // Bx ID
    thePattern.addToPattern(numberOfStubs, 0x3F, 6);

    uint8_t stubSize = 21;
    for(uint8_t bxOffset = 0; bxOffset < 8; ++bxOffset)
    {
        for(auto theStub: theStubVector)
        {
            thePattern.addToPattern(0x0, 0x0, 3);                                     // BX offset
            thePattern.addToPattern(chipIdForCIC, 0x7, 3);                            // Chip ID
            thePattern.addToPattern(theStub.fSeed + 2, 0xFF, 8);               // seed
            thePattern.addToPattern(fBendingToCode.at(theStub.fBend), 0x7, 3); // bending
            thePattern.addToPattern(theStub.fZ, 0xF, 4);                    // z
        }
    }

    for(uint8_t emptyStubCounter = 0; emptyStubCounter < maximumStubNumber - numberOfStubs; ++emptyStubCounter)
    {
        thePattern.addToPattern(0x0, 0x1FFFFF, stubSize); // empty stubs
    }

    // padding 0s
    if(numberOfBytesInSinglePacket == 1)
        thePattern.addToPattern(0x0, 0xFFFFF, 20); // 5G case
    else
        thePattern.addToPattern(0x0, 0x1F, 5); // 10G case

    return thePattern;
}

void OTverifyMPASSAdataWord::matchAllPossibleStubPatterns(uint8_t                                        numberOfBytesInSinglePacket,
                                                          size_t                                         numberOfLines,
                                                          std::vector<std::pair<PatternMatcher, float>>& thePatternAndEfficiencyList,
                                                          const std::vector<uint32_t>&                   concatenatedStubPackage,
                                                          ReadoutChip*                                   theMPA)
{
    for(auto& thePatternAndEfficiency: thePatternAndEfficiencyList)
    {
        if(matchStubPattern(concatenatedStubPackage, thePatternAndEfficiency.first, numberOfBytesInSinglePacket, numberOfLines)) { thePatternAndEfficiency.second++; }
        else if(thePatternAndEfficiencyList.size() == 1)
        {
            LOG(DEBUG) << BOLDRED << "OTverifyMPASSAdataWord::injectStubsPS - Error, expected stub pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup "
                       << +theMPA->getOpticalGroupId() << " Hybrid " << +theMPA->getHybridId() << " MPA " << +theMPA->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "Stub data received    " << getPatternPrintout(concatenatedStubPackage, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern expected " << getPatternPrintout(thePatternAndEfficiency.first.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern mask     " << getPatternPrintout(thePatternAndEfficiency.first.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
    }
}

PatternMatcher OTverifyMPASSAdataWord::produceL1PatternMatcher(const std::vector<Cluster>& thePixelClusterList,
                                                               const std::vector<Cluster>& theStripClusterList,
                                                               uint8_t                                                   numberOfBytesInSinglePacket,
                                                               uint8_t                                                   chipIdForCIC)
{
    uint8_t numberOfPixelClusters = thePixelClusterList.size();
    uint8_t numberOfStripClusters = theStripClusterList.size();

    // Create expected pattern
    PatternMatcher thePatternMatcher;
    thePatternMatcher.addToPattern(0x0ffffffe, 0xffffffff, 32); // CIC header plus 0 in front added in the transmission
    thePatternMatcher.addToPattern(0x0, 0x1ff, 9);
    thePatternMatcher.addToPattern(0x0, 0x0, 9);
    thePatternMatcher.addToPattern(numberOfStripClusters, 0x7F, 7);
    thePatternMatcher.addToPattern(0x0, 0x1, 1);
    thePatternMatcher.addToPattern(numberOfPixelClusters, 0x7F, 7);

    std::map<uint8_t, std::pair<uint8_t, uint8_t>> orderedStripClusterList;
    for(const auto& theCluster: theStripClusterList) { orderedStripClusterList[theCluster.fFirstCol] = {theCluster.fRow, theCluster.fColWidth}; }

    // CIC ouputs cluster with loower address first
    for(const auto& theCluster: orderedStripClusterList)
    {
        thePatternMatcher.addToPattern(chipIdForCIC, 0x7, 3);
        thePatternMatcher.addToPattern(theCluster.first + 1, 0x7F, 7); // pixel column address starts from 1
        thePatternMatcher.addToPattern(theCluster.second.second - 1, 0x7, 3);
        thePatternMatcher.addToPattern(0x0, 0x0, 1);
    }

    std::map<uint8_t, std::pair<uint8_t, uint8_t>> orderedPixelClusterList;
    for(const auto& theCluster: thePixelClusterList) { orderedPixelClusterList[theCluster.fFirstCol] = {theCluster.fRow, theCluster.fColWidth}; }

    // CIC ouputs cluster with lower address first
    for(const auto& theCluster: orderedPixelClusterList)
    {
        thePatternMatcher.addToPattern(chipIdForCIC, 0x7, 3);
        thePatternMatcher.addToPattern(theCluster.first + 1, 0x7F, 7); // pixel column address starts from 1
        thePatternMatcher.addToPattern(theCluster.second.second - 1, 0x7, 3);
        thePatternMatcher.addToPattern(theCluster.second.first, 0xF, 4);
    }

    // add extra zeros for padding
    size_t numberOfPatternBits  = thePatternMatcher.getNumberOfPatternBits();
    size_t numberOfPaddingZeros = numberOfPatternBits % 4;
    if(numberOfPaddingZeros > 0) thePatternMatcher.addToPattern(0x0, ~(~0u << numberOfPaddingZeros), numberOfPaddingZeros);

    // add CIC trailing 0 and idle pattern
    if(numberOfStripClusters == 1)
        thePatternMatcher.addToPattern(0x00aaaaaa, 0x00ffffff, 32);
    else
        thePatternMatcher.addToPattern(0x00a, 0x00f, 12); // 10G debug output is very often cut

    return thePatternMatcher;
}

GenericDataArray<float, 2>& OTverifyMPASSAdataWord::getStorageForStubErrorRate(Hybrid* theHybrid, uint8_t chipId, uint8_t line, size_t stubPatternCounter)
{
    return fPatternMatchingEfficiencyContainer.getHybrid(theHybrid->getBeBoardId(), theHybrid->getOpticalGroupId(), theHybrid->getId())
                            ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9, 2>>()
                            .at(chipId)
                            .at(1 + stubPatternCounter);
}
