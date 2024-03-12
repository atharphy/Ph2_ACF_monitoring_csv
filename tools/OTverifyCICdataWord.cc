#include "tools/OTverifyCICdataWord.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/CbcInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/PSInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"
#include "Utils/Utilities.h"
#include <bitset>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyCICdataWord::fCalibrationDescription = "Inject L1 and stubs for each CBC/MPA and verify that CIC output corresponds to the expected pattern";

OTverifyCICdataWord::OTverifyCICdataWord() : Tool() {}

OTverifyCICdataWord::~OTverifyCICdataWord() {}

void OTverifyCICdataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations = findValueInSettings<double>("OTverifyCICdataWordNumberOfIterations", 1000);

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyCICdataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyCICdataWord::ConfigureCalibration() {}

void OTverifyCICdataWord::Running()
{
    LOG(INFO) << "Starting OTverifyCICdataWord measurement.";
    Initialise();
    runIntegrityTest();
    fillHistograms();
    LOG(INFO) << "Done with OTverifyCICdataWord.";
    Reset();
}

void OTverifyCICdataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTverifyCICdataWord measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTverifyCICdataWord.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTverifyCICdataWord stopped.";
}

void OTverifyCICdataWord::Pause() {}

void OTverifyCICdataWord::Resume() {}

void OTverifyCICdataWord::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTverifyCICdataWord::runIntegrityTest()
{
    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

    LOG(INFO) << BOLDYELLOW << "OTverifyCICdataWord::runIntegrityTest ... start integrity test" << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cDebugFWInterface* theDebugInterface = cInterface->getDebugInterface();

    for(auto theBoard: *fDetectorContainer)
    {
        runStubIntegrityTest(theBoard, theDebugInterface);
        runL1IntegrityTest(theBoard, theDebugInterface);
    }
}

void OTverifyCICdataWord::fillHistograms()
{
#ifdef __USE_ROOT__
    fDQMHistogramOTverifyCICdataWord.fillPatternMatchingEfficiencyResults(fPatternMatchingEfficiencyContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTverifyCICdataWordPatternMatchingEfficiency");
        thePatternMatchinEfficiencyContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer);
    }
#endif
}

void OTverifyCICdataWord::runL1IntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    bool isA2Smodule = theBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S; // only 1 module type per board

    LOG(INFO) << BOLDMAGENTA << "Running runL1IntegrityTest" << RESET;
    // // Set board trigger configuration for L1 alignment
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", isA2Smodule ? 100 : 1});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);

    for(auto theOpticalGroup: *theBoard)
    {
        LOG(INFO) << BOLDMAGENTA << "    Optical Group " << +theOpticalGroup->getId() << RESET;
        uint8_t numberOfBytesInSinglePacket = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10) ? 2 : 1;
        for(auto theHybrid: *theOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "        Hybrid " << +theHybrid->getId() << RESET;
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, true);

            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            auto theChipToCICMapping = fCicInterface->getMapping(cCic);

            for(auto theChip: *theHybrid)
            {
                if(theChip->getFrontEndType() == FrontEndType::SSA2) continue;
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
                fCicInterface->SelectOutput(cCic, false);

                uint8_t chipIdForCIC = theChipToCICMapping[theChip->getId() % 8];
                fCicInterface->EnableFEs(cCic, {uint8_t(theChip->getId() % 8)}, true);
                if(isA2Smodule)
                    injectL12S(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket);
                else
                    injectL1PS(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket);
            }
        }
    }
}

void OTverifyCICdataWord::injectL12S(Ph2_HwDescription::ReadoutChip* theChip, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    LOG(INFO) << BOLDBLUE << "            injecting clusters on CBC Id " << +theChip->getId() << RESET;

    auto& theL1Efficiency = fPatternMatchingEfficiencyContainer.getObject(theChip->getBeBoardId())
                                ->getObject(theChip->getOpticalGroupId())
                                ->getObject(theChip->getHybridId())
                                ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theChip->getId()][0];

    std::vector<std::pair<uint8_t, uint8_t>> theClusterList{{0xAA, 2}};
    fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);
    static_cast<CbcInterface*>(fReadoutChipInterface)->injectClusters(theChip, theClusterList);

    uint8_t numberOfClusters = theClusterList.size();

    PatternMatcher thePatternMatcher;
    thePatternMatcher.addToPattern(0x0ffffffe, 0xffffffff, 32); // CIC header plus 0 in front added in the transmission
    thePatternMatcher.addToPattern(0x0, 0x1ff, 9);
    thePatternMatcher.addToPattern(0x0, 0x0, 9);
    thePatternMatcher.addToPattern(numberOfClusters, 0x7F, 7);
    thePatternMatcher.addToPattern(0x0, 0x1, 1);

    std::map<uint8_t, uint8_t> orderedClusterList;
    for(const auto& theCluster: theClusterList) { orderedClusterList[theCluster.first] = theCluster.second; }

    // CIC ouputs cluster with loower address first
    for(const auto& theCluster: orderedClusterList)
    {
        thePatternMatcher.addToPattern(chipIdForCIC, 0x7, 3);
        thePatternMatcher.addToPattern(theCluster.first, 0xFF, 8);
        thePatternMatcher.addToPattern(theCluster.second - 1, 0x7, 3);
    }

    // add extra zeros for padding
    size_t numberOfPatternBits  = thePatternMatcher.getNumberOfPatternBits();
    size_t numberOfPaddingZeros = numberOfPatternBits % 4;
    thePatternMatcher.addToPattern(0x0, ~(~0u << numberOfPaddingZeros), numberOfPaddingZeros);

    // add extra 0x0000 if a byte is not full
    size_t numberOfPatternBitsAfterPadding = thePatternMatcher.getNumberOfPatternBits();
    size_t numberOfHalfByteZeros           = numberOfPatternBitsAfterPadding % 8;
    thePatternMatcher.addToPattern(0x0, ~(~0u << numberOfHalfByteZeros), numberOfHalfByteZeros);

    // add CIC trailing 0 and idle pattern
    thePatternMatcher.addToPattern(0x00aaaaaa, 0x00ffffff, 32);
    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        auto lineOutputVector        = theDebugInterface->L1ADebug(1, false);
        auto orderedLineOutputVector = reorderPattern(lineOutputVector, numberOfBytesInSinglePacket);
        // std::cout << "L1 Line -> " << getPatternPrintout(orderedLineOutputVector, orderedLineOutputVector) << std::endl;
        if(matchL1Pattern(orderedLineOutputVector, thePatternMatcher, numberOfBytesInSinglePacket))
        {
            ++theL1Efficiency;
            // LOG(INFO) << GREEN << "L1 pattern received " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "OTverifyCICdataWord::injectL12S - Error, expected L1 pattern not found for Board " << +theChip->getBeBoardId() << " OpticalGroup "
                       << +theChip->getOpticalGroupId() << " Hybrid " << +theChip->getHybridId() << " CBC " << +theChip->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern received " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern expected " << getPatternPrintout(thePatternMatcher.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern mask     " << getPatternPrintout(thePatternMatcher.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
    }

    theL1Efficiency /= fNumberOfIterations;
}

void OTverifyCICdataWord::injectL1PS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    LOG(INFO) << BOLDBLUE << "            injecting clusters on MPA Id " << +theMPA->getId() << RESET;

    auto& theL1Efficiency = fPatternMatchingEfficiencyContainer.getObject(theMPA->getBeBoardId())
                                ->getObject(theMPA->getOpticalGroupId())
                                ->getObject(theMPA->getHybridId())
                                ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theMPA->getId() % 8][0];

    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList{std::make_tuple<uint8_t, uint8_t, uint8_t>(0xA, 0x55, 2)};

    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, theClusterList);

    uint8_t numberOfPixelClusters = theClusterList.size();
    uint8_t numberOfStripClusters = 0;

    // Create expected pattern
    PatternMatcher thePatternMatcher;
    thePatternMatcher.addToPattern(0x0ffffffe, 0xffffffff, 32); // CIC header plus 0 in front added in the transmission
    thePatternMatcher.addToPattern(0x0, 0x1ff, 9);
    thePatternMatcher.addToPattern(0x0, 0x0, 9);
    thePatternMatcher.addToPattern(numberOfStripClusters, 0x7F, 7);
    thePatternMatcher.addToPattern(0x0, 0x1, 1);
    thePatternMatcher.addToPattern(numberOfPixelClusters, 0x7F, 7);

    std::map<uint8_t, std::pair<uint8_t, uint8_t>> orderedClusterList;
    for(const auto& theCluster: theClusterList) { orderedClusterList[std::get<1>(theCluster)] = {std::get<0>(theCluster), std::get<2>(theCluster)}; }

    // CIC ouputs cluster with loower address first
    for(const auto& theCluster: orderedClusterList)
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
            LOG(DEBUG) << BOLDRED << "OTverifyCICdataWord::injectL1PS - Error, expected L1 pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup " << +theMPA->getOpticalGroupId()
                       << " Hybrid " << +theMPA->getHybridId() << " MPA " << +theMPA->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "L1 data received    " << getPatternPrintout(orderedLineOutputVector, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern expected " << getPatternPrintout(thePatternMatcher.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "L1 pattern mask     " << getPatternPrintout(thePatternMatcher.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
    }

    theL1Efficiency /= fNumberOfIterations;
}

bool OTverifyCICdataWord::matchL1Pattern(std::vector<uint32_t> theWordVector, PatternMatcher thePatternMatcher, uint8_t numberOfBytesInSinglePacket)
{
    uint32_t                header          = 0x0ffffffe;
    uint32_t                headerMask      = 0xffffffff;
    std::pair<bool, size_t> isFoundAndWhere = matchPattern(theWordVector, numberOfBytesInSinglePacket, header, headerMask);
    if(!isFoundAndWhere.first) return false;

    size_t numberOfWordsToSkip = isFoundAndWhere.second / (sizeof(uint32_t));
    size_t numberOfBytesToSkip = isFoundAndWhere.second % (sizeof(uint32_t));

    if(numberOfWordsToSkip > 0) theWordVector.erase(theWordVector.begin(), theWordVector.begin() + numberOfWordsToSkip);

    std::vector<uint32_t> theShiftedWordVector = applyByteShift(theWordVector, numberOfBytesInSinglePacket, numberOfBytesToSkip);

    return thePatternMatcher.isMatched(theShiftedWordVector);
}

void OTverifyCICdataWord::runStubIntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest" << RESET;

    for(auto theOpticalGroup: *theBoard)
    {
        LOG(INFO) << BOLDMAGENTA << "    Optical Group " << +theOpticalGroup->getId() << RESET;
        bool    isA2Smodule                 = theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S;
        uint8_t numberOfBytesInSinglePacket = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10) ? 2 : 1;
        for(auto theHybrid: *theOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "        Hybrid " << +theHybrid->getId() << RESET;
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            auto theFeConfigRegisterValue = fCicInterface->ReadChipReg(cCic, "FE_CONFIG");
            theFeConfigRegisterValue |= 0x04; // Force bending to be sent out in the stub stream
            fCicInterface->WriteChipReg(cCic, "FE_CONFIG", theFeConfigRegisterValue);
            auto theChipToCICMapping = fCicInterface->getMapping(cCic);
            for(auto theChip: *theHybrid)
            {
                if(theChip->getFrontEndType() == FrontEndType::SSA2) continue;
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);

                uint8_t chipIdForCIC = theChipToCICMapping[theChip->getId() % 8];
                fCicInterface->EnableFEs(cCic, {uint8_t(theChip->getId() % 8)}, true);
                if(isA2Smodule)
                    injectStubs2S(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket);
                else
                    injectStubsPS(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket);
            }
        }
    }
}

void OTverifyCICdataWord::injectStubs2S(ReadoutChip* theChip, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    LOG(INFO) << BOLDBLUE << "            injecting stubs on CBC Id " << +theChip->getId() << RESET;

    fReadoutChipInterface->WriteChipReg(theChip, "PtCut", 14);
    fReadoutChipInterface->WriteChipReg(theChip, "ClusterCut", 4);
    static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);

    std::vector<std::pair<std::string, uint16_t>> theRegisterVector;
    theRegisterVector.push_back({"Bend7", fBendingAndCode[0]}); // bendind = 0 will ouput 5
    theRegisterVector.push_back({"Bend8", fBendingAndCode[2]}); // bendind = 2 will ouput A
    theRegisterVector.push_back({"Bend9", fBendingAndCode[4]}); // bendind = 4 will ouput F
    theRegisterVector.push_back({"CoincWind&Offset12", 0x00});  // set stub window offset to 0
    theRegisterVector.push_back({"CoincWind&Offset34", 0x00});  // set stub window offset to 0
    fReadoutChipInterface->WriteChipMultReg(theChip, theRegisterVector);

    // inject stubs on CBC to CIC stub lines 0 (first stub address) lines 1 (second stub address), line 3 (first and second stub bend)
    std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVectorFirstPattern{{0x0A, 0}, {0xA0, 2}, {0xAA, 4}};
    float matchingEfficiencyFirstPattern = injectAndMatch2SstubPatterns(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket, stubSeedAndBendingVectorFirstPattern);

    // // inject stubs on CBC to CIC stub lines 2 (third stub address), line 4 (thirt stub bend)
    std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVectorSecondPattern{{0x0A, 4}, {0xA0, 0}, {0xAA, 2}};
    float matchingEfficiencySecondPattern = injectAndMatch2SstubPatterns(theChip, chipIdForCIC, theDebugInterface, numberOfBytesInSinglePacket, stubSeedAndBendingVectorSecondPattern);

    fPatternMatchingEfficiencyContainer.getObject(theChip->getBeBoardId())
        ->getObject(theChip->getOpticalGroupId())
        ->getObject(theChip->getHybridId())
        ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theChip->getId()][1] = (matchingEfficiencyFirstPattern + matchingEfficiencySecondPattern) / 2;

    return;
}

float OTverifyCICdataWord::injectAndMatch2SstubPatterns(ReadoutChip*                         theChip,
                                                        uint8_t                              chipIdForCIC,
                                                        D19cDebugFWInterface*                theDebugInterface,
                                                        uint8_t                              numberOfBytesInSinglePacket,
                                                        std::vector<std::pair<uint8_t, int>> stubSeedAndBendingVector)
{
    size_t numberOfLines      = 5;
    bool   isKickoff          = true;
    float  matchingEfficiency = 0;
    fReadoutChipInterface->MaskAllChannels(theChip, true);
    static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theChip, stubSeedAndBendingVector);

    uint8_t maximumStubNumber = 16;

    // Order stub by bending
    std::map<uint8_t, uint8_t> orderedStubBendingCodeAndSeedVector;
    for(const auto& stubSeedAndBending: stubSeedAndBendingVector) { orderedStubBendingCodeAndSeedVector[fBendingAndCode.at(stubSeedAndBending.second)] = stubSeedAndBending.first; }
    if(orderedStubBendingCodeAndSeedVector.size() != stubSeedAndBendingVector.size())
    {
        std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] orderedStubBendingCodeAndSeedVector and stubSeedAndBendingVector sizes to not match!" << std::endl;
        abort();
    }

    uint8_t  numberOfStubs = stubSeedAndBendingVector.size() * 8; // times 8 because the packet contains stubs from 8 BXs
    uint16_t status        = 0x0;
    if(numberOfStubs > maximumStubNumber)
    {
        status        = 0x1;
        numberOfStubs = maximumStubNumber;
    }

    PatternMatcher thePattern;
    thePattern.addToPattern(0x0, 0x1, 1);      // is PS flag
    thePattern.addToPattern(status, 0x1FF, 9); // status bits
    thePattern.addToPattern(0x000, 0x000, 12); // Bx ID
    thePattern.addToPattern(numberOfStubs, 0x3F, 6);

    size_t totalNumberOfStubs = 0;
    for(auto theStub: orderedStubBendingCodeAndSeedVector)
    {
        for(uint8_t bxOffset = 0; bxOffset < 8; ++bxOffset)
        {
            thePattern.addToPattern(0x0, 0x0, 3);             // BX offset
            thePattern.addToPattern(chipIdForCIC, 0x7, 3);    // Chip ID
            thePattern.addToPattern(theStub.second, 0xFF, 8); // seed
            thePattern.addToPattern(theStub.first, 0xF, 4);   // bending
            ++totalNumberOfStubs;
            if(totalNumberOfStubs >= maximumStubNumber) break;
        }
        if(totalNumberOfStubs >= maximumStubNumber) break;
    }

    for(uint8_t emptyStubCounter = 0; emptyStubCounter < maximumStubNumber - numberOfStubs; ++emptyStubCounter)
    {
        thePattern.addToPattern(0x0, 0x3FFFF, 18); // empty stubs
    }

    // padding 0s
    thePattern.addToPattern(0x0, 0xF, 4);

    if(isKickoff && theChip->getHybridId() % 2 == 0) thePattern.maskStubFor2Skickoff();

    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
    {
        auto                  lineOutputVector        = theDebugInterface->StubDebug(true, numberOfLines, false);
        std::vector<uint32_t> concatenatedStubPackage = mergeCICStubOuput(lineOutputVector, numberOfBytesInSinglePacket);
        if(matchStubPattern(concatenatedStubPackage, thePattern, numberOfBytesInSinglePacket, numberOfLines))
        {
            ++matchingEfficiency;
            // LOG(INFO) << GREEN << "Stub pattern received " << getPatternPrintout(concatenatedStubPackage, numberOfBytesInSinglePacket) << RESET;
        }
        else
        {
            LOG(DEBUG) << BOLDRED << "OTverifyCICdataWord::injectStubsPS - Error, expected stub pattern not found for Board " << +theChip->getBeBoardId() << " OpticalGroup "
                       << +theChip->getOpticalGroupId() << " Hybrid " << +theChip->getHybridId() << " CBC " << +theChip->getId() << RESET;
            LOG(DEBUG) << BOLDRED << "Stub data received    " << getPatternPrintout(concatenatedStubPackage, numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern expected " << getPatternPrintout(thePattern.getPattern(), numberOfBytesInSinglePacket) << RESET;
            LOG(DEBUG) << BOLDRED << "Stub pattern mask     " << getPatternPrintout(thePattern.getMask(), numberOfBytesInSinglePacket) << RESET;
        }
        // for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
        // { LOG(INFO) << BOLDRED << "Line " << lineIndex << " -> " << getPatternPrintout(lineOutputVector[lineIndex], numberOfBytesInSinglePacket) << RESET; }
    }

    matchingEfficiency /= fNumberOfIterations;

    return matchingEfficiency;
}

void OTverifyCICdataWord::injectStubsPS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cDebugFWInterface* theDebugInterface, uint8_t numberOfBytesInSinglePacket)
{
    LOG(INFO) << BOLDBLUE << "            injecting stubs on MPA Id " << +theMPA->getId() << RESET;

    size_t numberOfLines = 6;

    auto& theStubEfficiency = fPatternMatchingEfficiencyContainer.getObject(theMPA->getBeBoardId())
                                  ->getObject(theMPA->getOpticalGroupId())
                                  ->getObject(theMPA->getHybridId())
                                  ->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 2>>()[theMPA->getId() % 8][1];

    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList{std::make_tuple<uint8_t, uint8_t, uint8_t>(0xA, 0x55, 1)};
    static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theMPA, theClusterList);
    fReadoutChipInterface->WriteChipReg(theMPA, "StubMode", 2); // Use pixel mode to exclude possible SSA communication issues
    fReadoutChipInterface->WriteChipReg(theMPA, "StubWindow", 31);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM10", 0x0); // bendind = 0 will ouput 0
    size_t numberOfStubs     = 8 * theClusterList.size();
    size_t maximumStubNumber = (numberOfBytesInSinglePacket == 1) ? 16 : 35; // 16 if a 5G, 35 if a 10G
    if(numberOfStubs > maximumStubNumber)                                    // CIC aligns stubs by bending, but in pixel-pixel mode bending is 0 and it is not possible to know what the CIC will drop
    {
        std::cerr << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] PS stube injected using pixel-pixel mode, more stubs than the maximum allowed!" << std::endl;
        abort();
    }

    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> stubInformationList;
    for(const auto& theCluster: theClusterList)
    {
        uint8_t seedColumn = std::get<1>(theCluster) * 2 + 1 + std::get<2>(theCluster) % 2;
        uint8_t bending    = 0;
        uint8_t zPosition  = std::get<0>(theCluster);
        stubInformationList.push_back({seedColumn, bending, zPosition});
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
        for(auto theStub: stubInformationList)
        {
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(0x0, 0x0, 3); // BX offset
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(chipIdForCIC, 0x7, 3); // Chip ID
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 8) break;
            thePattern.addToPattern(std::get<0>(theStub), 0xFF, 8); // seed
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 3) break;
            thePattern.addToPattern(std::get<1>(theStub), 0x7, 3); // bending
            if(thePattern.getNumberOfPatternBits() >= maximumNumberOfBitsToMatch - 4) break;
            thePattern.addToPattern(std::get<2>(theStub), 0xF, 4); // z
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
            LOG(DEBUG) << BOLDRED << "OTverifyCICdataWord::injectStubsPS - Error, expected stub pattern not found for Board " << +theMPA->getBeBoardId() << " OpticalGroup "
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

bool OTverifyCICdataWord::matchStubPattern(std::vector<uint32_t> theWordVector, PatternMatcher thePatternMatcher, uint8_t numberOfBytesInSinglePacket, size_t numberOfLines)
{
    for(uint8_t numberOfPacketsToSkip = 0; numberOfPacketsToSkip < numberOfLines * 8; ++numberOfPacketsToSkip)
    {
        std::vector<uint32_t> theShiftedWordVector = applyByteShift(theWordVector, numberOfBytesInSinglePacket, numberOfPacketsToSkip);
        if(thePatternMatcher.isMatched(theShiftedWordVector)) return true;
    }
    return false;
}

std::vector<uint32_t> OTverifyCICdataWord::mergeCICStubOuput(const std::vector<std::vector<uint32_t>>& stubLineDataList, uint8_t numberOfBytesInSinglePacket)
{
    std::vector<std::vector<uint32_t>> orderedStubLineDataList;

    for(const auto& stubLineData: stubLineDataList) { orderedStubLineDataList.push_back(reorderPattern(stubLineData, numberOfBytesInSinglePacket)); }

    // all stublines have the same number of bits
    size_t numberOfBitsPerWord                    = 8 * sizeof(uint32_t);
    size_t numberOfBitsPerStubLine                = orderedStubLineDataList.at(0).size() * numberOfBitsPerWord;
    size_t numberOfBitsInConcatenatedStubPackage  = numberOfBitsPerStubLine * orderedStubLineDataList.size();
    size_t numberOfWordsInConcatenatedStubPackage = numberOfBitsInConcatenatedStubPackage / numberOfBitsPerWord;
    if(numberOfBitsInConcatenatedStubPackage % numberOfBitsPerWord != 0) ++numberOfWordsInConcatenatedStubPackage;
    std::vector<uint32_t> concatenatedStubPackage(numberOfWordsInConcatenatedStubPackage);

    size_t currentConcatenatedBit = 0;

    for(size_t bitNumber = 0; bitNumber < numberOfBitsPerStubLine; ++bitNumber)
    {
        for(const auto& stubLineData: orderedStubLineDataList)
        {
            uint32_t stubBit = (stubLineData[bitNumber / numberOfBitsPerWord] >> (numberOfBitsPerWord - 1 - bitNumber % numberOfBitsPerWord)) & 0x1;
            concatenatedStubPackage[currentConcatenatedBit / numberOfBitsPerWord] |= (stubBit << (numberOfBitsPerWord - 1 - currentConcatenatedBit % numberOfBitsPerWord));
            ++currentConcatenatedBit;
        }
    }

    return concatenatedStubPackage;
}