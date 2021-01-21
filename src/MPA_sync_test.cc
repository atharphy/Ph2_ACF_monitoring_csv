// Simple test script to demonstrate use of middleware for the purposes of usercode development

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/FrontEndDescription.h"
#include "../HWDescription/MPA.h"
#include "../HWDescription/OuterTrackerModule.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../HWInterface/MPAInterface.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/Tool.h"
#include "TCanvas.h"
#include "TH1.h"
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
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_MPA_PreCalib.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    // D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a
    // better way! IB->PSInterfaceBoard_PowerOff_SSA();
    cTool.ConfigureHw();

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));

    ModuleContainer* ChipVec = pBoard->at(0)->at(0);

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);

    auto theMPAInterface = static_cast<MPAInterface*>(cTool.fReadoutChipInterface);

    // theMPAInterface->activate_I2C_chip();

    std::pair<uint32_t, uint32_t> rows = {0, 16};
    std::pair<uint32_t, uint32_t> cols = {0, 120};
    // std::pair<uint32_t, uint32_t> rows = {5,7};
    // std::pair<uint32_t, uint32_t> cols = {1,5};

    std::vector<TH1F*> scurves;
    std::string        title;
    std::cout << "Setup" << std::endl;

    for(auto cMPA: *ChipVec)
    {
        MPA* theMPA = static_cast<MPA*>(cMPA);
        // ReadoutChip* theMPA = static_cast<ReadoutChip*>(cMPA);

        theMPAInterface->Set_threshold(cMPA, 100);
        theMPAInterface->Activate_sync(cMPA);
        theMPAInterface->Activate_pp(cMPA);
        theMPAInterface->Set_calibration(cMPA, 100);
        Stubs    curstub;
        uint32_t npixtot = 0;
        // mysyscontroller.theMPAInterface->Start ( pBoard );
        for(size_t row = rows.first; row < rows.second; row++)
        {
            for(size_t col = cols.first; col < cols.second; col++)
            {
                std::cout << row << "," << col << std::endl;

                std::this_thread::sleep_for(ShortWait);
                uint32_t gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                theMPAInterface->Disable_pixel(cMPA, 0);
                theMPAInterface->Enable_pix_BRcal(cMPA, gpix, "rise", "edge");
                std::this_thread::sleep_for(ShortWait);
                static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Send_pulses(1000);
                std::this_thread::sleep_for(ShortWait);
                // theMPAInterface->ReadData ( pBoard );
                const std::vector<Event*>& events = cTool.GetEvents(pBoard);
                // const std::vector<Event*> &eventVector = cTool.GetEvents(pBoard);

                for(__attribute__((unused)) auto& ev: events) { std::cout << "tst" << std::endl; }

                npixtot += 1;
            }
        }
        // mysyscontroller.theMPAInterface->Stop ( pBoard );

        std::cout << "Numpix -- " << npixtot << std::endl;

        TCanvas* c1    = new TCanvas("c1", "c1", 1000, 500);
        int      ihist = 0;
        for(auto& hist: scurves)
        {
            // std::cout<<"drawing "<<ihist<<hist->>Integral()<<std::endl;
            if(ihist == 0)
            {
                hist->SetLineColor(1);
                hist->SetTitle(";Thresh DAC;Counts");
                hist->SetMaximum(40000);
                hist->SetStats(0);
                hist->Draw("L");
            }
            else
            {
                hist->SetLineColor(ihist % 60 + 1);
                hist->Draw("sameL");
            }
            ihist += 1;
        }
        c1->Print("scurvetemp.root", "root");

        std::this_thread::sleep_for(LongPOWait);
    }

} // int main
