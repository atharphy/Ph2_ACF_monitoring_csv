#include "tools/OTverifyBoardDataWord.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#include <sstream>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyBoardDataWord::fCalibrationDescription = "Use features of the CIC and FW to verify correct alignment of L1 and stub words in the FW";

OTverifyBoardDataWord::OTverifyBoardDataWord() : Tool() {}

OTverifyBoardDataWord::~OTverifyBoardDataWord() {}

void OTverifyBoardDataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations = findValueInSettings<double>("OTverifyBoardDataWord_NumberOfIterations", 1000);
    fIsKickoff          = findValueInSettings<double>("isKickoff", 0) > 0;

    size_t             numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    std::vector<float> initialEmptyVector(numberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer, initialEmptyVector);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyBoardDataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyBoardDataWord::ConfigureCalibration() {}

void OTverifyBoardDataWord::Running()
{
    LOG(INFO) << "Starting OTverifyBoardDataWord measurement.";
    Initialise();
    runIntegrityTest();
    LOG(INFO) << "Done with OTverifyBoardDataWord.";
    Reset();
}

void OTverifyBoardDataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTverifyBoardDataWord measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTverifyBoardDataWord.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTverifyBoardDataWord stopped.";
}

void OTverifyBoardDataWord::Pause() {}

void OTverifyBoardDataWord::Resume() {}

void OTverifyBoardDataWord::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTverifyBoardDataWord::runIntegrityTest()
{
    LOG(INFO) << BOLDYELLOW << "OTverifyBoardDataWord::runIntegrityTest ... start integrity test" << RESET;
    auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    for(auto theBoard: *fDetectorContainer)
    {
        runStubIntegrityTest(theBoard, theFWInterface);
        runL1IntegrityTest(theBoard, theFWInterface);
    }

    // normalize
    for(auto theBoard: fPatternMatchingEfficiencyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto& theNumberOfMatches: theHybrid->getSummary<std::vector<float>>()) theNumberOfMatches /= fNumberOfIterations;
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTverifyBoardDataWord.fillPatternMatchingEfficiency(fPatternMatchingEfficiencyContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theMatchingEfficiencyContainerSerialization("OTverifyBoardDataWordMatchingEfficiency");
        theMatchingEfficiencyContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer);
    }
#endif
}

void OTverifyBoardDataWord::runStubIntegrityTest(BeBoard* theBoard, D19cFWInterface* theFWInterface)
{
    LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest" << RESET;

    for(auto theOpticalGroup: *theBoard)
    {
        uint8_t numberOfBytesInSinglePacket = getNumberOfBytesInSinglePacket(theOpticalGroup);
        if(fIsKickoff && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
            LOG(INFO) << BOLDYELLOW << "Attention! ignoring failures on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
        size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theHybridPatternMatchingEfficiency =
                fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

            prepareHybridForStubIntegrityTest(theHybrid);

            for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
            {
                auto lineOutputVector = theFWInterface->StubDebug(true, cNlines, false);
                for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
                {
                    if(isStubPatternMatched(lineOutputVector[lineIndex], numberOfBytesInSinglePacket, flagCharacter, idleCharacter))
                        ++theHybridPatternMatchingEfficiency[lineIndex + 1];
                    else if(!(fIsKickoff && ((theHybrid->getId() % 2) == 0) && ((lineIndex) == 4) && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)))
                        LOG(DEBUG) << BOLDRED << "Error on stub line " << lineIndex + 1 << " occurred in iteration number " << +iteration << RESET;
                }
            }
        }
    }
}

void OTverifyBoardDataWord::prepareHybridForStubIntegrityTest(Hybrid* theHybrid)
{
    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    fCicInterface->SelectOutput(cCic, true);
    fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
}

bool OTverifyBoardDataWord::isStubPatternMatched(const std::vector<uint32_t>& theWordVector, uint8_t numberOfBytesInSinglePacket, uint8_t flagCharacter, uint8_t idleCharacter)
{
    // create a mask that is 0xFF for 5G and 0xFFFF for 10G modules
    uint16_t mask = 0xFF;
    if(numberOfBytesInSinglePacket == 2) mask = 0xFFFF;

    uint8_t numberOfIdleCharacters = (numberOfBytesInSinglePacket == 1) ? 7 : 15;

    enum SearchPatternStatus
    {
        Idle,
        FlagFound,
        Error
    } status = Idle;

    uint8_t  numberOfSinglePacketsInOneWord    = sizeof(uint32_t) / numberOfBytesInSinglePacket;
    uint16_t totalNumberOfSinglePackets        = theWordVector.size() * numberOfSinglePacketsInOneWord;
    uint16_t currentSinglePacketNumber         = 0;
    uint8_t  numberOfConsecutiveIdleCharacters = 0;
    bool     firstFlagCharacterFound           = false;
    while(currentSinglePacketNumber < totalNumberOfSinglePackets)
    {
        uint16_t currentSinglePacket =
            ((theWordVector.at(currentSinglePacketNumber / numberOfSinglePacketsInOneWord)) >> (currentSinglePacketNumber % numberOfSinglePacketsInOneWord * 8 * numberOfBytesInSinglePacket)) & mask;
        ++currentSinglePacketNumber;
        for(int byteShift = 0; byteShift < numberOfBytesInSinglePacket; ++byteShift)
        {
            uint8_t currentByte = (currentSinglePacket >> (8 * byteShift)) & 0xFF;
            switch(status)
            {
            case SearchPatternStatus::Idle: // I am in Idle, looking for flagCharacter
            {
                if(currentByte == idleCharacter)
                {
                    ++numberOfConsecutiveIdleCharacters;
                    if(numberOfConsecutiveIdleCharacters > numberOfIdleCharacters) // too many Idle characters!!!
                    {
                        status = SearchPatternStatus::Error;
                    }
                }
                else if(currentByte == flagCharacter)
                {
                    if(firstFlagCharacterFound && numberOfConsecutiveIdleCharacters != numberOfIdleCharacters) // not enough idle characters!!!
                    {
                        status = SearchPatternStatus::Error;
                    }
                    else
                    {
                        firstFlagCharacterFound = true;
                        status                  = SearchPatternStatus::FlagFound;
                    }
                }
                else // unrecognized character!!!
                {
                    status = SearchPatternStatus::Error;
                }

                break;
            }

            case SearchPatternStatus::FlagFound: // I found the flag, now I expect to fo back to Idle
            {
                numberOfConsecutiveIdleCharacters = 0;
                if(currentByte == idleCharacter)
                {
                    ++numberOfConsecutiveIdleCharacters;
                    status = SearchPatternStatus::Idle;
                }
                else // no idle character found after flag!!!
                {
                    status = SearchPatternStatus::Error;
                }
                break;
            }

            case SearchPatternStatus::Error: // error case
            {
                LOG(DEBUG) << BOLDRED << "OTverifyBoardDataWord::isStubPatternMatched - Error, expected pattern not found" << RESET;
                LOG(DEBUG) << BOLDRED << getPatternPrintout(theWordVector, numberOfBytesInSinglePacket, true) << RESET;
                return false;
            }

            default: // this shold never happen
            {
                LOG(ERROR) << BOLDRED << "OTverifyBoardDataWord::isStubPatternMatched - Error, state machine went into default state, it should never happen" << RESET;
                return false;
            }
            }
        }
    }

    return true;
}

void OTverifyBoardDataWord::runL1IntegrityTest(BeBoard* theBoard, D19cFWInterface* theFWInterface)
{
    LOG(INFO) << BOLDMAGENTA << "Running runL1IntegrityTest" << RESET;
    prepareFWForL1IntegrityTest(theBoard);

    for(auto theOpticalGroup: *theBoard)
    {
        uint8_t numberOfBytesInSinglePacket = getNumberOfBytesInSinglePacket(theOpticalGroup);
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theHybridPatternMatchingEfficiency =
                fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

            prepareHybridForL1IntegrityTest(theHybrid);

            for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
            {
                auto lineOutputVector = theFWInterface->L1ADebug(1, false);
                if(isL1HeaderFound(lineOutputVector, numberOfBytesInSinglePacket, header, headerMask))
                    ++theHybridPatternMatchingEfficiency[0];
                else { LOG(DEBUG) << BOLDRED << "Error occurred in iteration number " << +iteration << RESET; }
            }
        }
    }
}

bool OTverifyBoardDataWord::isL1HeaderFound(const std::vector<uint32_t>& theWordVector, uint8_t numberOfBytesInSinglePacket, uint32_t header, uint32_t headerMask)
{
    auto orderedLineOutputVector = reorderPattern(theWordVector, numberOfBytesInSinglePacket);

    std::pair<bool, size_t> isFoundAndWhere = matchPattern(orderedLineOutputVector, numberOfBytesInSinglePacket, header, headerMask);
    return isFoundAndWhere.first;
}

void OTverifyBoardDataWord::prepareHybridForL1IntegrityTest(Ph2_HwDescription::Hybrid* theHybrid)
{
    // select lines for slvs debug
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
    fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theHybrid->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
}
void OTverifyBoardDataWord::prepareFWForL1IntegrityTest(Ph2_HwDescription::BeBoard* theBoard)
{
    // Set board trigger configuration for L1 alignment
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 100});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);
}

uint8_t OTverifyBoardDataWord::getNumberOfBytesInSinglePacket(OpticalGroup* cOpticalGroup) const
{
    return (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(cOpticalGroup->flpGBT) == 10) ? 2 : 1;
}