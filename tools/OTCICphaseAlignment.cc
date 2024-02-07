#include "tools/OTCICphaseAlignment.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/TriggerInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

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

void OTCICphaseAlignment::ConfigureCalibration() {}

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

void OTCICphaseAlignment::Pause() {}

void OTCICphaseAlignment::Resume() {}

void OTCICphaseAlignment::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTCICphaseAlignment::phaseAlignment()
{
    uint32_t pNTriggers = 500;
    bool cDebug   = false;
    LOG(INFO) << BOLDBLUE << "Starting CIC automated phase alignment procedure for CBCs .... " << RESET;
    DetectorDataContainer theBestPhaseContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, int>>(*fDetectorContainer, theBestPhaseContainer);

    for(auto cBoard: *fDetectorContainer)
    {
        bool cWithCBC = (cBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S);
        if(cWithCBC)
        {
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNTriggers});
            cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        }
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
                    fReadoutChipInterface->producePhaseAlignmentPattern(cChip, 10);
                }
            }
        }
        // send N triggers on L1 lines
        if(cWithCBC)
        {
            LOG(INFO) << BOLDBLUE << "Sending triggers to FEs to align L1 output from CBCs.." << RESET;
            cInterface->getTriggerInterface()->SendNTriggers(pNTriggers);
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
                if(cLocked) LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() << BOLDGREEN << " LOCKED " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                else LOG(INFO) << BOLDBLUE << "Phase aligner on CIC" << +cHybrid->getId() << BOLDRED << " FAILED to LOCK " << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                
                fCicInterface->GetOptimalTaps(cCic);
                auto& cPhaseAlignmentVals = theBestPhaseContainer.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getSummary<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, int>>();
                for(size_t chipId = 0; chipId < NUMBER_OF_CIC_PORTS; ++chipId) // not using the chipID because I want always to read all phases
                {
                    auto cPhaseTapsThisFE = fCicInterface->GetOptimalTaps(cCic, chipId);
                    for(size_t cLineId = 0; cLineId < NUMBER_OF_LINES_PER_CIC_PORTS; cLineId++)
                    {
                        cPhaseAlignmentVals(chipId, cLineId) = cPhaseTapsThisFE[cLineId];
                    }
                }
                fCicInterface->SetStaticPhaseAlignment(cCic);
            } // CICs
        }     // OG
    }

    #ifdef __USE_ROOT__
    fDQMHistogramOTCICphaseAlignment.fillBestPhasePhaseResults(theBestPhaseContainer);
    #else
        if(fDQMStreamerEnabled)
        {
            ContainerSerialization theBestPhaseContainerSerialization("OTCICphaseAlignmentBestPhase");
            theBestPhaseContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestPhaseContainer);
        }
    #endif

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
