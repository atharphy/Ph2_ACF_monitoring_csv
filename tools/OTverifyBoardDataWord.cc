#include "tools/OTverifyBoardDataWord.h"
#include "System/RegisterHelper.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"

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

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyBoardDataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyBoardDataWord::ConfigureCalibration()
{

}

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

void OTverifyBoardDataWord::Pause()
{

}


void OTverifyBoardDataWord::Resume()
{

}


void OTverifyBoardDataWord::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTverifyBoardDataWord::runIntegrityTest()
{
    LOG(INFO) << BOLDYELLOW << "OTverifyBoardDataWord::runIntegrityTest ... start integrity test" << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cDebugFWInterface* theDebugInterface   = cInterface->getDebugInterface();

    for(auto theBoard : *fDetectorContainer)
    {
        runStubIntegrityTest(theBoard, theDebugInterface);
        runL1IntegrityTest(theBoard, theDebugInterface);
    }
}

void OTverifyBoardDataWord::runStubIntegrityTest(BeBoard* theBoard, D19cDebugFWInterface* theDebugInterface)
{
    for(auto theOpticalGroup : *theBoard)
    {
        size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
        for(auto theHybrid : *theOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "Stub debug output - hybrid#" << +theHybrid->getId() << RESET;
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, true);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            for(size_t cIter = 0; cIter < 1; cIter++)
            {
                LOG(INFO) << BOLDYELLOW << "Debug capture Iteration#" << cIter << RESET;
                theDebugInterface->StubDebug(true, cNlines);
            }
        }
    }
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

    for(auto theOpticalGroup : *theBoard)
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