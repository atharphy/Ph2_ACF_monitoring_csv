//
#include "HWDescription/BeBoard.h"
#include "HWDescription/Chip.h"
#include "HWDescription/Definition.h"
#include "HWDescription/FrontEndDescription.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWDescription/ReadoutChip.h"
#include "HWInterface/BeBoardInterface.h"
#include "System/SystemController.h"
#include "Utils/CommonVisitors.h"
#include "Utils/ConsoleColor.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "tools/BackEndAlignment.h"
#include "tools/CalibrationExample.h"
#include "tools/Tool.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cPSTestBoardFWInterface.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TROOT.h"
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <iostream>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

#ifndef InjectionEvent
struct InjectionEvent
{
    //
    uint8_t fChipId = 0;
    // event information
    uint32_t fEventId   = 0;
    uint32_t fTriggerId = 0;
    uint32_t fL1Id      = 0;
    //
    uint32_t fExpectedL1Id = 0;
    //
    uint32_t fExpectedHits = 0;
    uint32_t fDetectedHits = 0;
    uint8_t  fMatch        = 0;
};
typedef std::vector<InjectionEvent> InjectionEvents;
#endif
int main(int argc, char* argv[])
{
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_2xSSA2.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    // //
    // cTool.fBeBoardInterface->setBoard(0);
    // D19cPSTestBoardFWInterface* IB = dynamic_cast<D19cPSTestBoardFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
    // // power cycle board - jic
    // IB->PSInterfaceBoard_PowerOff_SSA();
    // IB->ReadPower_SSA();
    // IB->PSInterfaceBoard_PowerOn_SSA(1.25, 1.0, 1.25, 0.3, 0.0, 145);
    // IB->ReadPower_SSA();
    cTool.ConfigureHw();

    // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Start(0);
    cBackEndAligner.waitForRunToBeCompleted();
    cBackEndAligner.Reset();

    // look at L1 debug
    // auto cDebugInterface = static_cast<D19cDebugFWInterface*>(dynamic_cast<D19cPSTestBoardFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface()););
    // cDebugInterface->L1ADebug();
    // collect events
    size_t cNevents = 10;
    for(uint16_t cDelayBeforeNext = 150; cDelayBeforeNext > 10; cDelayBeforeNext -= 10)
    {
        for(uint16_t cDelayAfterTP = 100; cDelayAfterTP > 10; cDelayAfterTP -= 10)
        {
            for(auto cBeBoard: *cTool.fDetectorContainer)
            {
                cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", cDelayBeforeNext);
                cTool.fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cDelayAfterTP);
                uint16_t cNclks       = cDelayBeforeNext + cDelayAfterTP;
                float    cRate        = 1.0e-3 / (cNclks * 25e-9);
                size_t   cTriggerMult = cTool.fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
                uint8_t  cPattern     = 0x00;
                for(uint8_t cId = 0; cId < cTriggerMult; cId++) { cPattern = cPattern | (1 << cId); }
                uint16_t cTriggerDelay = cTool.fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
                uint16_t cTriggerLat   = cTriggerDelay - 2;
                LOG(DEBUG) << BOLDBLUE << "Trigger latency will be set to " << cTriggerLat << "... and also enabling digitalSync for all strips" << RESET;
                for(auto opticalGroup: *cBeBoard)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            if(chip->getFrontEndType() == FrontEndType::SSA2)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "TriggerLatency", cTriggerLat, false);
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "DigitalSync", 0x1, false);
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "DigitalDuration", 1);
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "DigCalibPattern_L", cPattern, true);
                            }
                        }
                    }
                }

                cTool.ReadNEvents(cBeBoard, cNevents);
                const std::vector<Event*>& cPh2Events = cTool.GetEvents();
                LOG(INFO) << BOLDBLUE << "Read-back " << +cPh2Events.size() << " events from the FC7.. Time btwn bursts is " << cNclks << " clocks [ " << 1e-3 / (cNclks * 25e-9) << " kHz ]" << RESET;
                cTool.ReadNEvents(cBeBoard, cNevents);
                const std::vector<Event*>& cEvents = cTool.GetEvents();
                // loop over triggers in the burst
                std::vector<uint16_t> cExpectedHits(1 + cTriggerMult, 120);
                cExpectedHits[cTriggerMult] = 0;
                std::vector<uint8_t> cMatches(0);
                InjectionEvents      cInjectionEvents;
                cInjectionEvents.clear();
                for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
                {
                    auto   cEventIter   = cEvents.begin() + cTriggerId;
                    bool   cEventMatch  = true;
                    size_t cTriggerNmbr = 0;
                    do
                    {
                        for(auto opticalGroup: *cBeBoard)
                        {
                            for(auto hybrid: *opticalGroup)
                            {
                                for(auto chip: *hybrid)
                                {
                                    InjectionEvent cInjectionEvent;
                                    cInjectionEvent.fChipId       = chip->getId();
                                    cInjectionEvent.fL1Id         = (static_cast<D19cSSA2Event*>(*cEventIter))->L1Id(chip->getHybridId(), chip->getId());
                                    cInjectionEvent.fTriggerId    = (static_cast<D19cSSA2Event*>(*cEventIter))->GetTrigID();
                                    cInjectionEvent.fEventId      = (static_cast<D19cSSA2Event*>(*cEventIter))->GetL1Number();
                                    cInjectionEvent.fExpectedL1Id = cInjectionEvent.fEventId + 1;
                                    auto cHits                    = (*cEventIter)->GetHits(chip->getHybridId(), chip->getId());
                                    cInjectionEvent.fExpectedHits = cExpectedHits[cTriggerId];
                                    cInjectionEvent.fDetectedHits = cHits.size();
                                    cEventMatch                   = cEventMatch && (cExpectedHits[cTriggerId] == cHits.size()) && (cInjectionEvent.fExpectedL1Id == cInjectionEvent.fL1Id);
                                    cInjectionEvent.fMatch        = cEventMatch ? 1 : 0;
                                    cInjectionEvents.push_back(cInjectionEvent);
                                    LOG(INFO) << BOLDBLUE << "\t..SSA#" << +chip->getId() << " trigger#" << cInjectionEvent.fTriggerId << " L1 FW Counter" << cInjectionEvent.fEventId
                                              << "  L1I FE Counter " << cInjectionEvent.fL1Id << " [ " << cInjectionEvent.fExpectedL1Id << " ] "
                                              << " found " << +cHits.size() << " hits in Trigger#" << +cTriggerId << " of a burst of " << (cTriggerMult + 1) << " I expected to see "
                                              << cExpectedHits[cTriggerId] << " hits." << RESET;
                                } // chip
                            }     // hybrid
                        }         // OG
                        if(cEventMatch)
                            cMatches.push_back(1);
                        else
                            cMatches.push_back(0);
                        cTriggerNmbr++;
                        cEventIter += (1 + cTriggerMult);
                    } while(cEventIter < cEvents.end());
                } // trigger id
                float cEventsMatched    = std::accumulate(cMatches.begin(), cMatches.end(), 0.);
                float cMatchingFraction = cEventsMatched / cEvents.size();
                if(cMatchingFraction == 1.)
                    LOG(INFO) << BOLDGREEN << "For " << 1 + cTriggerMult << " triggers in a burst and " << cNclks << " clock cycles between the start of a burst .. found " << cEventsMatched
                              << " events with the expected number of hits out of " << cEvents.size() << " readout events [ " << cRate << " ]" << RESET;
                else
                    LOG(INFO) << BOLDRED << "For " << 1 + cTriggerMult << " triggers in a burst and " << cNclks << " clock cycles between the start of a burst .. found " << cEventsMatched
                              << " events with the expected number of hits out of " << cEvents.size() << " readout events [ " << cRate << " ]" << RESET;
                std::sort(std::begin(cInjectionEvents), std::end(cInjectionEvents), [](InjectionEvent a, InjectionEvent b) { return a.fEventId < b.fEventId; });
                std::sort(std::begin(cInjectionEvents), std::end(cInjectionEvents), [](InjectionEvent a, InjectionEvent b) { return a.fL1Id < b.fL1Id; });
                for(auto cInjectionEvent: cInjectionEvents)
                {
                    if(cInjectionEvent.fMatch == 1)
                        LOG(INFO) << BOLDGREEN << "\t..SSA#" << cInjectionEvent.fChipId << " trigger#" << cInjectionEvent.fTriggerId << " L1 FW Counter" << cInjectionEvent.fEventId
                                  << "  L1I FE Counter " << cInjectionEvent.fL1Id << " [ " << cInjectionEvent.fExpectedL1Id << " ] "
                                  << " found " << cInjectionEvent.fDetectedHits << " I expected to see " << cInjectionEvent.fExpectedHits << " hits." << RESET;
                    else
                        LOG(INFO) << BOLDRED << "\t..SSA#" << cInjectionEvent.fChipId << " trigger#" << cInjectionEvent.fTriggerId << " L1 FW Counter" << cInjectionEvent.fEventId
                                  << "  L1I FE Counter " << cInjectionEvent.fL1Id << " [ " << cInjectionEvent.fExpectedL1Id << " ] "
                                  << " found " << cInjectionEvent.fDetectedHits << " I expected to see " << cInjectionEvent.fExpectedHits << " hits." << RESET;
                }

            } // board
        }     // loop over delays
    }
    // BeBoard*         pBoard  = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    // HybridContainer* ChipVec = pBoard->at(0)->at(0);
    // cTool.setFWTestPulse();
    // TH1I* h1 = new TH1I("h1", "S-CURVE", 256, 0, 256);
    // for(int thd = 0; thd <= 256; thd++)
    // {
    //     for(auto cSSA: *ChipVec)
    //     {
    //         ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "Bias_CALDAC", 30);
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ReadoutMode", 0x1); // sync mode = 0
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "Bias_THDAC", thd);
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "FE_Calibration", 1);
    //         for(int i = 1; i <= 120; i++) // loop over all strips
    //         {
    //             // cTool.fReadoutChipInterface->WriteChipReg(theSSA, "THTRIMMING_S" + std::to_string(i), 31);
    //             cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ENFLAGS_S" + std::to_string(i), 5); // 17 = 10001 (enable strobe)
    //         }
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "L1-Latency_LSB", 0x44);
    //         cTool.fReadoutChipInterface->WriteChipReg(theSSA, "L1-Latency_MSB", 0x0);
    //     }

    //     cTool.SystemController::Start(0);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(50));
    //     cTool.SystemController::Stop();
    //     for(auto cSSA: *ChipVec)
    //     {
    //         ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
    //         uint8_t      cRP1   = cTool.fReadoutChipInterface->ReadChipReg(theSSA, "ReadCounter_LSB_S18");
    //         uint8_t      cRP2   = cTool.fReadoutChipInterface->ReadChipReg(theSSA, "ReadCounter_MSB_S18");
    //         uint16_t     cRP    = (cRP2 * 256) + cRP1;

    //         LOG(INFO) << BOLDRED << "THDAC = " << thd << ", HITS = " << cRP << RESET;
    //         h1->Fill(thd, cRP);
    //     }
    //     /*
    //     cTool.ReadNEvents(pBoard, 200);
    //     const std::vector<Event*> &eventVector = cTool.GetEvents(pBoard);

    //     for ( auto &event : eventVector ) //for on events - begin
    //     {
    //         for(auto hybrid: *pBoard) // for on hybrid - begin
    //         {
    //             for(auto chip: *hybrid) // for on chip - begin
    //             {
    //                 unsigned int channelNumber = 0;
    //                 for (int i = 1; i<=120;i++ ) // loop over all strips
    //                 {
    //                     h1->Fill(thd, event->DataBit ( hybrid->getId(), chip->getId(), channelNumber));
    //                     LOG (INFO) << BOLDBLUE << "hits on channel "<<channelNumber<< " = " << event->DataBit (
    //     hybrid->getId(), chip->getId(), channelNumber) <<RESET; channelNumber++; } // for on channel - end } // for on
    //     chip - end } // for on hybrid - end } // for on events - end*/
    // }
    // TCanvas* c1 = new TCanvas("c", "c", 600, 600);
    // c1->cd();
    // h1->Draw("hist");
    // c1->Print("INJ.png");
    // IB->PSInterfaceBoard_PowerOff_SSA();
}