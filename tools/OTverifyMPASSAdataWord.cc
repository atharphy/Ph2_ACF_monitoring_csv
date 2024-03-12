#include "tools/OTverifyMPASSAdataWord.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"
#include "HWInterface/D19cDebugFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyMPASSAdataWord::fCalibrationDescription = "Insert brief calibration description here";

OTverifyMPASSAdataWord::OTverifyMPASSAdataWord() : OTverifyCICdataWord() {}

OTverifyMPASSAdataWord::~OTverifyMPASSAdataWord() {}

void OTverifyMPASSAdataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    fNumberOfIterations = findValueInSettings<double>("OTverifyMPASSAdataWordNumberOfIterations", 1000);

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyMPASSAdataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyMPASSAdataWord::ConfigureCalibration()
{

}

void OTverifyMPASSAdataWord::Running()
{
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

void OTverifyMPASSAdataWord::Pause()
{

}

void OTverifyMPASSAdataWord::Resume()
{

}

void OTverifyMPASSAdataWord::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

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

void OTverifyMPASSAdataWord::injectL1PS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    ReadoutChip* theSSA = nullptr;
    try
    {
        theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId()%8);
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
                                ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theMPA->getId() % 8][0];

    // std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> thePixelClusterList{};
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> thePixelClusterList{std::make_tuple<uint8_t, uint8_t, uint8_t>(0xA, 0x55, 2)};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, thePixelClusterList);

    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theStripClusterList{std::make_tuple<uint8_t, uint8_t>(0x0, 0x25, 2)};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theSSA, theStripClusterList);

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
    for(const auto& theCluster: theStripClusterList) { orderedStripClusterList[std::get<1>(theCluster)] = {std::get<0>(theCluster), std::get<2>(theCluster)}; }

    // CIC ouputs cluster with loower address first
    for(const auto& theCluster: orderedStripClusterList)
    {
        thePatternMatcher.addToPattern(chipIdForCIC, 0x7, 3);
        thePatternMatcher.addToPattern(theCluster.first + 1, 0x7F, 7); // pixel column address starts from 1
        thePatternMatcher.addToPattern(theCluster.second.second - 1, 0x7, 3);
        thePatternMatcher.addToPattern(0x0, 0x0, 1);
    }

    std::map<uint8_t, std::pair<uint8_t, uint8_t>> orderedPixelClusterList;
    for(const auto& theCluster: thePixelClusterList) { orderedPixelClusterList[std::get<1>(theCluster)] = {std::get<0>(theCluster), std::get<2>(theCluster)}; }

    // CIC ouputs cluster with loower address first
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
    thePatternMatcher.addToPattern(0x0, ~(~0u << numberOfPaddingZeros), numberOfPaddingZeros);

    // add CIC trailing 0 and idle pattern
    if(numberOfStripClusters == 1)
        thePatternMatcher.addToPattern(0x00aaaaaa, 0x00ffffff, 32);
    else
        thePatternMatcher.addToPattern(0x00a, 0x00f, 12); // 10G debug output is very often cut
    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        auto lineOutputVector        = theDebugInterface->L1ADebug(1, false);
        auto orderedLineOutputVector = reorderPattern(lineOutputVector, numberOfBytesInSinglePacket);
        // std::cout << "L1 Line -> " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << std::endl;
        if(matchL1Pattern(orderedLineOutputVector, thePatternMatcher, numberOfBytesInSinglePacket))
        {
            ++theL1Efficiency;
            // LOG(INFO) << GREEN << "L1 pattern received " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "OTverifyMPASSAdataWord::injectL1PS - Error, expected L1 pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup " << +theMPA->getOpticalGroupId()
                       << " Hybrid " << +theMPA->getHybridId() << " MPA " << +theMPA->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "L1 data received    " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern expected " << getPatternPrintout(thePatternMatcher.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern mask     " << getPatternPrintout(thePatternMatcher.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
    }

    theL1Efficiency /= fNumberOfIterations;
}

void OTverifyMPASSAdataWord::injectStubsPS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    ReadoutChip* theSSA = nullptr;
    try
    {
        theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId()%8);
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << YELLOW << "            skipping MPA Id " << +theMPA->getId() << " since corresponding SSA is not enabled" << RESET;
        return;
    }
    
    LOG(INFO) << BOLDBLUE << "            injecting stubs on MPA Id " << +theMPA->getId() << " and SSA " << +theSSA->getId() << RESET;

    size_t numberOfLines = 6;

    auto& theStubEfficiency = fPatternMatchingEfficiencyContainer.getObject(theMPA->getBeBoardId())
                                  ->getObject(theMPA->getOpticalGroupId())
                                  ->getObject(theMPA->getHybridId())
                                  ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theMPA->getId() % 8][1];

    uint8_t bendingCode = 0x05; 
    fReadoutChipInterface->WriteChipReg(theMPA, "StubMode", 0); // Use normal stub mode
    fReadoutChipInterface->WriteChipReg(theMPA, "StubWindow", 32);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM10", bendingCode); // bendind = 0 will ouput 101

    // stubs need to be ordered by bending code (remember that the CIC orders them based on the Code[MP]XX values)
    std::vector<std::tuple<uint8_t, uint8_t, int>> theStubVector{{0x0A, 0xAA, 0}};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseStubs(theMPA, theSSA, theStubVector);

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
    // reading 120 bytes from the FPGA FIFO, stub packet is 48 (96) bytes for 5G (10G), but not possible to know when the packet will be recorded
    // -> 5G packet will always fit, 10G packet can contain only 120 - 96 = 34 relevant bytes
    size_t maximumNumberOfBitsToMatch = (120 - 48 * numberOfBytesInSinglePacket) * 8; // 120 bytes is the maximum read from the register, max allowed matching = 120/2
    for(uint8_t bxOffset = 0; bxOffset < 8; ++bxOffset)
    {
        for(auto theStub: theStubVector)
        {
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(0x0, 0x0, 3); // BX offset
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(chipIdForCIC, 0x7, 3); // Chip ID
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 8) break;
            thePattern.addToPattern(std::get<1>(theStub) + 2, 0xFF, 8); // seed
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(bendingCode, 0x7, 3); // bending
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 4) break;
            thePattern.addToPattern(std::get<0>(theStub), 0xF, 4); // z
        }
    }

    for(uint8_t emptyStubCounter = 0; emptyStubCounter < maximumStubNumber - numberOfStubs; ++emptyStubCounter)
    {
        if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - stubSize) break;
        thePattern.addToPattern(0x0, 0x1FFFFF, stubSize); // empty stubs
    }

    // padding 0s
    if(numberOfBytesInSinglePacket == 1) thePattern.addToPattern(0x0, 0xFFFFF, 20); // 5G case only

    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        auto                  lineOutputVector        = theDebugInterface->StubDebug(true, numberOfLines, false);
        std::vector<uint32_t> concatenatedStubPackage = mergeCICStubOuput(lineOutputVector, numberOfBytesInSinglePacket);
        if(matchStubPattern(concatenatedStubPackage, thePattern, numberOfBytesInSinglePacket, numberOfLines))
        {
            ++theStubEfficiency;
            // LOG(INFO) << GREEN << "Stub pattern received " << getPatternPrintout(concatenatedStubPackage, numberOfBytesInSinglePacket) << RESET;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "OTverifyMPASSAdataWord::injectStubsPS - Error, expected stub pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup "
                       << +theMPA->getOpticalGroupId() << " Hybrid " << +theMPA->getHybridId() << " MPA " << +theMPA->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "Stub data received    " << getPatternPrintout(concatenatedStubPackage, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern expected " << getPatternPrintout(thePattern.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern mask     " << getPatternPrintout(thePattern.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
        // for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
        // { LOG(INFO) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector[lineIndex], numberOfBytesInSinglePacket) << RESET; }
    }

    theStubEfficiency /= fNumberOfIterations;
}
