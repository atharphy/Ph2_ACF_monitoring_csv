#include "tools/OTCICphaseAlignment.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/TriggerInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCICphaseAlignment::fCalibrationDescription = "Insert brief calibration description here";

OTCICphaseAlignment::OTCICphaseAlignment() : Tool() {}

OTCICphaseAlignment::~OTCICphaseAlignment() {}

void OTCICphaseAlignment::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "PHY_PORT_CONFIG");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "^scPhaseSelectB[0-3]i[0-5]$");

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCICphaseAlignment.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCICphaseAlignment::ConfigureCalibration()
{

}

void OTCICphaseAlignment::Running()
{
    LOG(INFO) << "Starting OTCICphaseAlignment measurement.";
    Initialise();
    phaseAlignment();
    LOG(INFO) << "Done with OTCICphaseAlignment.";
    Reset();
}

void OTCICphaseAlignment::Stop(void)
{
    LOG(INFO) << "Stopping OTCICphaseAlignment measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTCICphaseAlignment.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCICphaseAlignment stopped.";
}

void OTCICphaseAlignment::Pause()
{

}


void OTCICphaseAlignment::Resume()
{

}


void OTCICphaseAlignment::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTCICphaseAlignment::phaseAlignment(uint16_t pWait_us, uint32_t pNTriggers)
{
    bool cDebug   = false;
    bool cAligned = true;
    LOG(INFO) << BOLDBLUE << "Starting CIC automated phase alignment procedure for CBCs .... " << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        bool cWithCBC = false;
        // generate alignment pattern on all stub lines
        LOG(INFO) << BOLDBLUE << "Generating Patterns needed for phase alignment of CIC inputs." << RESET;
        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SetAutomaticPhaseAlignment(cCic, true);
                // configure Chips to produce phase alignment patterns
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::CBC3) cWithCBC = true;
                    fReadoutChipInterface->producePhaseAlignmentPattern(cChip, 10);
                }
            }
        }
        // send N triggers on L1 lines
        if(cWithCBC)
        {
            LOG(INFO) << BOLDBLUE << "Sending triggers to FEs to align L1 output from CBCs.." << RESET;
            uint16_t cTriggerSrc = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
            // if external or async triggers are used then revert to internal here
            bool                                          cReconfigureTrigger = (cTriggerSrc == 4 || cTriggerSrc || 5 || cTriggerSrc == 10);
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            if(cReconfigureTrigger)
            {
                uint16_t cSrc = 3;
                if(cTriggerSrc != cSrc)
                {
                    LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source is set to " << +cSrc << RESET;
                    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cSrc});
                }
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNTriggers});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
            }
            auto cTriggerInterface = cInterface->getTriggerInterface();
            cTriggerInterface->SendNTriggers(pNTriggers);

            // set trigger source back
            if(cReconfigureTrigger)
            {
                LOG(INFO) << BOLDBLUE << "\t.. Changing trigger source back to " << +cTriggerSrc << RESET;
                std::vector<std::pair<std::string, uint32_t>> cRegVec;
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
            }
        } // in the CBC case you need to send triggers to get alignment data on L1 line
        // check alignment
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // enable automatic phase aligner
                auto& cCic    = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                bool  cLocked = fCicInterface->CheckPhaseAlignerLock(cCic);
                // if locked .. switch to automatic phase aligner mode with best values
                if(cLocked)
                { LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() << BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and switching to static phase " << RESET; }
                else
                    LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() << BOLDRED << " FAILED to LOCK " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                cAligned = cAligned && cLocked;
            } // CICs
        }     // OG
    }
    if(cAligned) this->SetStaticPhaseAlignment();

    // check
    for(auto cBoard: *fDetectorContainer)
    {
        if(!cDebug) continue;

        fBeBoardInterface->setBoard(cBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

        D19cDebugFWInterface* cDebugInterface = cInterface->getDebugInterface();
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                for(uint8_t cPhyPort = 0; cPhyPort < 12; cPhyPort++)
                {
                    fCicInterface->SelectMux(cCic, cPhyPort);
                    cDebugInterface->StubDebug(true, 4);
                }
                fCicInterface->ControlMux(cCic, 0);
            }
        }
    }
}

void OTCICphaseAlignment::SetStaticPhaseAlignment()
{
    LOG(INFO) << BOLDBLUE << "Setting CIC phase to static mode.." << RESET;

    DetectorDataContainer thePhaseValueContainer;
    std::vector<uint8_t> initialPhaseVector(6, 0);
    std::string theQueryFunction = "skipSSAQuery";
    auto theSkipSSAquery = [](const ChipContainer *theReadoutChip)
    {
        if(static_cast<const ReadoutChip*>(theReadoutChip)->getFrontEndType() == FrontEndType::SSA2) return false;
        return true;
    };
    fDetectorContainer->addReadoutChipQueryFunction(theSkipSSAquery, theQueryFunction);
    ContainerFactory::copyAndInitChip<std::vector<uint8_t>>(*fDetectorContainer, thePhaseValueContainer, initialPhaseVector);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->GetOptimalTaps(cCic);
                for(auto cChip: *cHybrid)
                {
                    auto& cPhaseAlignmentVals     = thePhaseValueContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<std::vector<uint8_t>>();
                    auto              cPhaseTapsThisFE = fCicInterface->GetOptimalTaps(cCic, cChip->getId() % 8);
                    std::stringstream cOutput;
                    for(uint8_t cLineId = 0; cLineId < 6; cLineId++)
                    {
                        cPhaseAlignmentVals[cLineId] = cPhaseTapsThisFE[cLineId];
                        cOutput << +cPhaseAlignmentVals[cLineId] << " ";
                    }
                    LOG(INFO) << BOLDBLUE << "Optimal tap found on CIC#" << +cChip->getHybridId() << " FE" << +cChip->getId() << " : " << cOutput.str() << RESET;
                }
                fCicInterface->SetStaticPhaseAlignment(cCic);
            }
        }
    }
    fDetectorContainer->removeReadoutChipQueryFunction(theQueryFunction);
}