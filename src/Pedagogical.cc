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
#include "../tools/BackEndAlignment.h"
#include "../tools/CalibrationExample.h"
#include "../tools/Tool.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"
#include "TLine.h"
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
    LOG(INFO) << BOLDRED << "=============" << RESET;
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_2xSSA_PreCalibSYNC.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    // D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a
    // better way! IB->PSInterfaceBoard_PowerOff_SSA();
    cTool.ConfigureHw();

    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cBackEndAligner.Align();
    cBackEndAligner.resetPointers();

    BeBoard*         pBoard           = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    HybridContainer* ChipVec          = pBoard->at(0)->at(0);
    TH2I*            strip_v_thdac_31 = new TH2I("strip_v_thdac_31", "All TRIMDACs = 31;strip # ; THDAC (lsb)", 360, -60, 300, 25, 0, 25);
    strip_v_thdac_31->SetStats(0);
    for(auto cSSA: *ChipVec)
    {
        LOG(INFO) << BOLDRED << "ROC" << RESET;

        ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
        cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ReadoutMode", 0x0); // sync mode = 0
        cTool.fReadoutChipInterface->WriteChipReg(theSSA, "EdgeSel_T1", 0x1);  // edge select (Mykyta thinks this has an effect)
        for(int i = 1; i <= 120; i++)                                          // loop over all strips
        {
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "THTRIMMING_S" + std::to_string(i), 31); // MAXIMIZE THE TRIM
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "ENFLAGS_S" + std::to_string(i), 1);     // ENABLE THE STRIP
        }
    }
    for(int thd = 20; thd <= 20; thd++)
    {
        for(auto cSSA: *ChipVec)
        {
            ReadoutChip* theSSA = static_cast<ReadoutChip*>(cSSA);
            std::cout << "Setting threshold to " << thd << std::endl;
            cTool.fReadoutChipInterface->WriteChipReg(theSSA, "Bias_THDAC", thd);
        }
        cTool.ReadNEvents(pBoard, 500);
        const std::vector<Event*>& eventVector = cTool.GetEvents();
        for(auto& event: eventVector) // for on events - begin
        {
            LOG(INFO) << BOLDRED << "L1N: " << static_cast<D19cSSAEvent*>(event)->GetL1Number() << RESET;
            LOG(INFO) << BOLDRED << "L1T: " << static_cast<D19cSSAEvent*>(event)->GetTrigID() << RESET;
            for(auto opt: *pBoard) // for on hybrid - begin
            {
                for(auto hybrid: *opt) // for on hybrid - begin
                {
                    for(auto chip: *hybrid) // for on chip - begin
                    {
                        unsigned int channelNumber = 0;
                        for(int i = 0; i <= 120; i++) // loop over all strips
                        {
                            // if (event->DataBit ( hybrid->getId(), chip->getId(), channelNumber)) LOG (INFO) << RED <<
                            // i << ", " << int(chip->getId()) <<  RESET;
                            strip_v_thdac_31->Fill(channelNumber + (120 * int(chip->getId())), thd, event->DataBit(hybrid->getId(), chip->getId(), channelNumber));
                            channelNumber++;
                        } // for on channel - end

                        LOG(INFO) << BOLDRED << "L1C " << hybrid->getId() << "," << chip->getId() << " : " << static_cast<D19cSSAEvent*>(event)->GetSSAL1Counter(hybrid->getId(), chip->getId())
                                  << RESET;
                        // for (auto S: event->GetHits(hybrid->getId(), chip->getId()))
                        //{
                        //	LOG(INFO) << BOLDRED << "stub: " << float(S)/2. << RESET;
                        //}
                    }
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on events - end
    }
    TLine* L1 = new TLine(120, 0, 120, 25);
    TLine* L2 = new TLine(240, 0, 240, 25);
    L1->SetLineColor(2);
    L2->SetLineColor(2);

    TCanvas* C_svd = new TCanvas("C_svd", "C_svd", 600, 600);
    C_svd->cd();
    strip_v_thdac_31->Draw("colz");
    L1->Draw();
    L2->Draw();
    C_svd->Print("STRIP_DAC_MAP.png");

    // IB->PSInterfaceBoard_PowerOff_SSA();
}
