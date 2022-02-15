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
#include "L1ReadoutInterface.h"

#include "PedeNoise.h"
#include "PedestalEqualization.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"

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
#include <unistd.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    LOG(INFO) << BOLDRED << "=============" << RESET;
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_2xSSA2.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    cTool.ConfigureHw();
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cBackEndAligner.Align();
    cBackEndAligner.Reset();
    cBackEndAligner.resetPointers();

    BeBoard*         pBoard  = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    HybridContainer* ChipVec = pBoard->at(0)->at(0);
    for(auto cSSA: *ChipVec)
    {
        ReadoutChip* iSSA = static_cast<ReadoutChip*>(cSSA);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "control_1", 0x0);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "control_2", 0xf0);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "mask_strip", 0xff);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "ENFLAGS", 0x0);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "THTRIMMING", 0x15);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "Bias_THDAC", 50);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "Bias_CALDAC", 100);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "ENFLAGS_S38", 0x9);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "ENFLAGS_S50", 0x9);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "ENFLAGS_S72", 0x9);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "DigCalibPattern_L", 0xff);
        cTool.fReadoutChipInterface->WriteChipReg(iSSA, "SLVS_pad_current_L1", 0x7);
    }
    std::ofstream myfile;
    myfile.open("Output_M8.csv");
    int DELAY = 400;
    while(DELAY > 19)
    {
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(100, 10, DELAY - 10);
        auto cInterface          = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
        auto cL1ReadoutInterface = cInterface->getL1ReadoutInterface();
        cL1ReadoutInterface->ResetReadout();
        int nGoodBest = 0;
        for(int lat = 5; lat < 6; lat++)
        {
            int n3Good = 0;
            for(auto cSSA: *ChipVec)
            {
                ReadoutChip* iSSA = static_cast<ReadoutChip*>(cSSA);
                cTool.fReadoutChipInterface->WriteChipReg(iSSA, "control_3", lat);
            }
            cTool.ReadNEvents(pBoard, 50);
            const std::vector<Event*>& eventVector = cTool.GetEvents();
            for(auto& event: eventVector) // for on events - begin
            {
                for(auto opt: *pBoard) // for on hybrid - begin
                {
                    for(auto hybrid: *opt) // for on hybrid - begin
                    {
                        for(auto chip: *hybrid) // for on chip - begin
                        {
                            if(event->GetHits(hybrid->getId(), chip->getId()).size() == 3) { n3Good++; }
                        }
                    }
                    if(n3Good > nGoodBest) { nGoodBest = n3Good; }
                }
            }
            usleep(50000); // sleeps for 1/16 second
        }
        LOG(INFO) << BOLDBLUE << "\n\n\n" << nGoodBest << " successes at a delay of " << DELAY << RESET;
        myfile << nGoodBest << ", " << DELAY << "\n";
        DELAY = DELAY - 1;
    }
    cTool.Destroy();
}
