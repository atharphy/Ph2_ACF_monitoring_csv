#include "tools/OTCICphaseAlignment.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "HWInterface/TriggerInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include <sstream>

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

    fNumberOfLockCheckIterations = findValueInSettings<double>("OTCICphaseAlignmentNumberOfLockCheckIterations", 100);
    fMinLockingSuccessRate = findValueInSettings<double>("OTCICphaseAlignmentMinLockingSuccessRate", 1.);

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
    ContainerFactory::copyAndInitHybrid<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, uint8_t>>(*fDetectorContainer, theBestPhaseContainer);

    DetectorDataContainer theLockingEfficiencyContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, float>>(*fDetectorContainer, theLockingEfficiencyContainer);

    for(auto theBoard: *fDetectorContainer)
    {
        bool cWithCBC = (theBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S);
        if(cWithCBC)
        {
            std::vector<std::pair<std::string, uint32_t>> cRegVec;
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
            cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNTriggers});
            cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            fBeBoardInterface->WriteBoardMultReg(theBoard, cRegVec);
        }
        // generate alignment pattern on all stub lines
        LOG(INFO) << BOLDBLUE << "Generating Patterns needed for phase alignment of CIC inputs." << RESET;
        fBeBoardInterface->setBoard(theBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->SetAutomaticPhaseAlignment(cCic, true);
                // configure Chips to produce phase alignment patterns
                for(auto cChip: *theHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA2) continue;
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
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                // enable automatic phase aligner
                auto& cCic    = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
                
                auto& cLockingEfficiency = theLockingEfficiencyContainer.getObject(theBoard->getId())
                                                ->getObject(theOpticalGroup->getId())
                                                ->getObject(theHybrid->getId())
                                                ->getSummary<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, float>>();

                cLockingEfficiency = fCicInterface->getAllLockedEfficiencies(cCic, fNumberOfLockCheckIterations);
                bool  cLocked = true;
                for(auto theChip: *theHybrid)
                {
                    for(uint8_t line=0; line<NUMBER_OF_LINES_PER_CIC_PORTS; ++line)
                    {
                        if(cLockingEfficiency(theChip->getId(), line) < fMinLockingSuccessRate)
                        {
                            std::stringstream errorMessage;
                            errorMessage << "OTCICphaseAlignment::phaseAlignment - Error in aligning CIC on ";
                            if(line == 0) errorMessage << "L1 line";
                            else errorMessage << "Stub line " << +(line-1);
                            errorMessage << " - locking efficiency = " << cLockingEfficiency(theChip->getId(), line) << " less then minimum requited (" << fMinLockingSuccessRate << ")";
                            errorMessage << " - Chip  " << +theChip->getId() << " Hybrid " << +theHybrid->getId() << " OpticalGroup " << +theOpticalGroup->getId() << " BeBoard " << +theBoard->getId();
                            LOG(ERROR) << BOLDRED << errorMessage.str() << RESET;
                            cLocked = false;
                        }
                    }
                }
                std::stringstream message;
                message << BOLDBLUE << "Phase aligner on CIC" << +theHybrid->getId();
                if(cLocked) message << BOLDGREEN << " LOCKED ";
                else message << BOLDRED << " FAILED to LOCK ";
                message << BOLDBLUE << " ... storing values and switching to static phase " << RESET;
                LOG(INFO) << message.str();
                if(!cLocked)
                {
                    LOG(INFO) << BOLDRED << "FAILED to lock CIC inputs on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id" << +theHybrid->getId() << " --- OpticalGroup will be disabled"
                            << RESET;
                    ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
                    continue;
                }
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;

                auto& cPhaseAlignmentVals = theBestPhaseContainer.getObject(theBoard->getId())
                                                ->getObject(theOpticalGroup->getId())
                                                ->getObject(theHybrid->getId())
                                                ->getSummary<GenericDataArray_2D<NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS, uint8_t>>();

                cPhaseAlignmentVals = fCicInterface->getAllOptimalTaps(cCic);
                fCicInterface->SetStaticPhaseAlignment(cCic);
            } // CICs
        }     // OG
    }

    #ifdef __USE_ROOT__
    fDQMHistogramOTCICphaseAlignment.fillBestPhaseResults(theBestPhaseContainer);
    fDQMHistogramOTCICphaseAlignment.fillLockingEfficiencyResults(theLockingEfficiencyContainer);
    #else
        if(fDQMStreamerEnabled)
        {
            ContainerSerialization theBestPhaseContainerSerialization("OTCICphaseAlignmentBestPhase");
            theBestPhaseContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestPhaseContainer);

            ContainerSerialization theLockingEfficiencyContainerSerialization("OTCICphaseAlignmentLockingEfficiency");
            theLockingEfficiencyContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theLockingEfficiencyContainer);
        }
    #endif

    // check
    for(auto theBoard: *fDetectorContainer)
    {
        if(!cDebug) continue;

        fBeBoardInterface->setBoard(theBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

        D19cDebugFWInterface* cDebugInterface = cInterface->getDebugInterface();
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
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
