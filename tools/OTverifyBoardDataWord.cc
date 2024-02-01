#include "tools/OTverifyBoardDataWord.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include <sstream>
#include "Utils/ContainerSerialization.h"

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

    fNumberOfIterations = findValueInSettings<double>("OTverifyBoardDataWordNumberOfIterations", 1000);

    size_t               numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    std::vector<float> initialEmptyVector(numberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer, initialEmptyVector);

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
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
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cDebugFWInterface* theDebugInterface = cInterface->getDebugInterface();

    for(auto theBoard: *fDetectorContainer)
    {
        runStubIntegrityTest(theBoard, theDebugInterface);
        runL1IntegrityTest(theBoard, theDebugInterface);
    }

    // normalize
    for(auto theBoard: fPatternMatchingEfficiencyContainer)
    {
        for(auto theOpticalGroup : *theBoard)
        {
            for(auto theHybrid : *theOpticalGroup)
            {
                for(auto& theNumberOfMatches : theHybrid->getSummary<std::vector<float>>()) theNumberOfMatches/=fNumberOfIterations;
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

void OTverifyBoardDataWord::runStubIntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    for(auto theOpticalGroup: *theBoard)
    {
        size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
        for(auto theHybrid: *theOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "Stub debug output - hybrid#" << +theHybrid->getId() << RESET;
            auto& theHybridPatternMatchingEfficiency =fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<float>>();

            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, true);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);

            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

            for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
            {
                auto lineOutputVector = theDebugInterface->StubDebug(true, cNlines, false);
                for(size_t lineIndex=0; lineIndex < lineOutputVector.second.size(); ++lineIndex)
                {
                    if(isStubPatternMatched( lineOutputVector.second[lineIndex])) ++theHybridPatternMatchingEfficiency[lineIndex + 1];
                }
            }
        }
    }
}

bool OTverifyBoardDataWord::isStubPatternMatched(std::vector<uint32_t> theWordVector)
{
    uint8_t flagCharacter = 0xe;
    uint8_t idleCaracter  = 0xa;
    uint8_t numberOfIdleCharacters = 15;

    enum SearchPatternStatus {Idle, FlagFound, Error} status = Idle;

    uint8_t  numberOf4bitCharactersInOneWord = sizeof(uint32_t)*2;
    uint16_t totalNumberOf4bitCharacters = theWordVector.size() * numberOf4bitCharactersInOneWord;
    uint16_t current4BitCharacter = 0;
    uint8_t numberOfConsecutiveIdleCharacters = 0;
    bool firstFlagCharacterFound = false;
    while(current4BitCharacter < totalNumberOf4bitCharacters)
    {
        uint8_t current4BitWord = ((theWordVector[current4BitCharacter / numberOf4bitCharactersInOneWord]) >> (current4BitCharacter % numberOf4bitCharactersInOneWord * 4)) & 0xF;
        ++current4BitCharacter;
        switch (status)
        {
            case SearchPatternStatus::Idle: // I am in Idle, looking for flagCharacter
            {
                if(current4BitWord == idleCaracter)
                {
                    ++numberOfConsecutiveIdleCharacters;
                    if(numberOfConsecutiveIdleCharacters> numberOfIdleCharacters) // too many Idle characters!!!
                    {
                        status = SearchPatternStatus::Error;
                    }
                }
                else if (current4BitWord == flagCharacter)
                {

                    if(firstFlagCharacterFound && numberOfConsecutiveIdleCharacters != numberOfIdleCharacters) // not enough idle characters!!!
                    {
                        status = SearchPatternStatus::Error;
                    }
                    else
                    {
                        firstFlagCharacterFound = true;
                        status = SearchPatternStatus::FlagFound;
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
                if(current4BitWord == idleCaracter)
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
                LOG(ERROR) << BOLDRED << "OTverifyBoardDataWord::isStubPatternMatched - Error, expected pattern not found" << RESET;
                std::stringstream thePattern;

                thePattern << "Received pattern: " << std::hex;
                for(auto theWord : theWordVector)
                {
                    for(uint8_t the4bitshift = 0; the4bitshift <numberOf4bitCharactersInOneWord; ++the4bitshift)
                    {
                        thePattern << +((theWord >> (the4bitshift*4)) & 0xF);
                    }
                }
                thePattern << std::dec;
                LOG(ERROR) << BOLDRED << thePattern.str() << RESET;

                return false;
            }

            default: // this shold never happen
            {
                LOG(ERROR) << BOLDRED << "OTverifyBoardDataWord::isStubPatternMatched - Error, state machine went into default state, it should never happen" << RESET;
                return false;
            }
        }
    }

    return true;
}


void OTverifyBoardDataWord::runL1IntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
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

    for(auto theOpticalGroup: *theBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            if(cCic == nullptr) continue;

            // select lines for slvs debug
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            theDebugInterface->L1ADebug();
        }
    }
}