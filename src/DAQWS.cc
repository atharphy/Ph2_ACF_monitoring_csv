//
#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/FrontEndDescription.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/CalibrationExample.h"
#include "../tools/Tool.h"
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

int main(int argc, char* argv[])
{
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_2xSSA_onechip.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a better way!
    IB->PSInterfaceBoard_PowerOn_SSA(1.25, 1.25, 1.25, 0.3, 0.0, 145);
    IB->ReadPower_SSA();
    cTool.ConfigureHw();

    BeBoard*         pBoard  = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    HybridContainer* ChipVec = pBoard->at(0)->at(0);
    cTool.setFWTestPulse();
    TH1I* h1 = new TH1I("h1", "S-CURVE", 256, 0, 256);
    for(int thd = 0; thd <= 256; thd++)
    {
        for(auto cSSA: *ChipVec)
        {
            ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "Bias_CALDAC", 30);
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ReadoutMode", 0x1); // sync mode = 0
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "Bias_THDAC", thd);
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "FE_Calibration", 1);
            for(int i = 1; i <= 120; i++) // loop over all strips
            {
                // cTool.fReadoutChipInterface->WriteChipReg(theSSA, "THTRIMMING_S" + std::to_string(i), 31);
                cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ENFLAGS_S" + std::to_string(i), 5); // 17 = 10001 (enable strobe)
            }
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "L1-Latency_LSB", 0x44);
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "L1-Latency_MSB", 0x0);
        }

        cTool.SystemController::Start(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        cTool.SystemController::Stop();
        for(auto cSSA: *ChipVec)
        {
            ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
            uint8_t      cRP1   = cTool.fReadoutChipInterface->ReadChipReg(theSSA, "ReadCounter_LSB_S18");
            uint8_t      cRP2   = cTool.fReadoutChipInterface->ReadChipReg(theSSA, "ReadCounter_MSB_S18");
            uint16_t     cRP    = (cRP2 * 256) + cRP1;

            LOG(INFO) << BOLDRED << "THDAC = " << thd << ", HITS = " << cRP << RESET;
            h1->Fill(thd, cRP);
        }
        /*
        cTool.ReadNEvents(pBoard, 200);
        const std::vector<Event*> &eventVector = cTool.GetEvents(pBoard);

        for ( auto &event : eventVector ) //for on events - begin
        {
            for(auto hybrid: *pBoard) // for on hybrid - begin
            {
                for(auto chip: *hybrid) // for on chip - begin
                {
                    unsigned int channelNumber = 0;
                    for (int i = 1; i<=120;i++ ) // loop over all strips
                    {
                        h1->Fill(thd, event->DataBit ( hybrid->getId(), chip->getId(), channelNumber));
                        LOG (INFO) << BOLDBLUE << "hits on channel "<<channelNumber<< " = " << event->DataBit (
        hybrid->getId(), chip->getId(), channelNumber) <<RESET; channelNumber++; } // for on channel - end } // for on
        chip - end } // for on hybrid - end } // for on events - end*/
    }
    TCanvas* c1 = new TCanvas("c", "c", 600, 600);
    c1->cd();
    h1->Draw("hist");
    c1->Print("INJ.png");

    IB->PSInterfaceBoard_PowerOff_SSA();
}