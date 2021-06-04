
// Simple test script to demonstrate use of middleware for the purposes of usercode development

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/FrontEndDescription.h"
#include "../HWDescription/Hybrid.h"
#include "../HWDescription/MPA.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../HWInterface/MPAInterface.h"
#include "../Utils/Utilities.h"
#include "../tools/Tool.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <numeric> // for std::accumulate

#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"
#include <inttypes.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    ofstream myfile;
    ofstream scurvecsv;
    scurvecsv.open("scurvetemp.csv");

    LOG(INFO) << BOLDRED << "=============" << RESET;

    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_MPA_PreCalib.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << BOLDRED << "1" << RESET;
    // D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a
    // better way! IB->PSInterfaceBoard_PowerOff_SSA();
    cTool.ConfigureHw();
    LOG(INFO) << BOLDRED << "2" << RESET;
    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    LOG(INFO) << BOLDRED << "3" << RESET;

    HybridContainer* ChipVec = pBoard->at(0)->at(0);

    LOG(INFO) << BOLDRED << "4" << RESET;

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);

    // should be done from configure hw

    LOG(INFO) << BOLDRED << "5" << RESET;

    std::pair<uint32_t, uint32_t> rows = {0, 16};
    std::pair<uint32_t, uint32_t> cols = {0, 120};
    std::pair<uint32_t, uint32_t> th   = {100, 140};

    std::vector<TH1F*> scurves;
    std::vector<TH2F*> scurves2D;
    std::string        title;
    LOG(INFO) << BOLDRED << "6" << RESET;
    auto theMPAInterface = static_cast<PSInterface*>(cTool.fReadoutChipInterface);
    int  impa            = 0;
    for(auto cMPA: *ChipVec)
    {
        if(cMPA->getFrontEndType() == FrontEndType::SSA) continue;
        MPA* theMPA = static_cast<MPA*>(cMPA);

        theMPAInterface->Activate_async(cMPA);
        theMPAInterface->Set_calibration(cMPA, 200);
        title = "mpa" + std::to_string(impa);
        scurves2D.push_back(new TH2F(title.c_str(), title.c_str(), 1920, -0.5, 1919.5, 255, -0.5, 254.5));
        uint32_t npixtot = 0;
        for(uint16_t row = rows.first; row < rows.second; row++)
        {
            for(uint16_t col = cols.first; col < cols.second; col++)
            {
                uint32_t gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                // std::cout <<  "r "<<row<<" c " << col<<" png "<< gpix<< std::endl;

                theMPAInterface->Enable_pix_counter(theMPA, gpix - 1);
                if(impa == 6 or impa == 1)
                {
                    if(gpix > 200 and gpix < 800) theMPAInterface->Disable_pixel(theMPA, gpix - 1);
                }
                title = "mpa" + std::to_string(impa) + ":" + std::to_string(row) + "," + std::to_string(col);
                scurves.push_back(new TH1F(title.c_str(), title.c_str(), 255, -0.5, 254.5));
                npixtot += 1;
            }
        }
        std::cout << "Numpix -- " << npixtot << std::endl;
        impa += 1;
    }
    int                   nmpas = impa;
    std::vector<uint16_t> countersfifo;
    // uint32_t curpnum = 0;
    uint32_t totalevents     = 0;
    uint32_t totaleventsprev = 0;
    uint32_t nrep            = 0;
    for(uint16_t ith = th.first; ith < th.second; ith++)
    {
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();

        std::cout << "ITH= " << ith << std::endl;
        for(auto cMPA: *ChipVec)
        {
            if(cMPA->getFrontEndType() == FrontEndType::SSA) continue;
            MPA* theMPA = static_cast<MPA*>(cMPA);
            theMPAInterface->Set_threshold(theMPA, ith);
        }

        std::cout << "1" << std::endl;

        std::this_thread::sleep_for(ShortWait);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
        // open shutter
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Open_shutter(8);

        // sleep            // close shutter
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Send_pulses(3000);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Close_shutter(8);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Start_counters_read(8);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Start_counters_read(8);

        std::cout << "2" << std::endl;

        std::this_thread::sleep_for(ShortWait);
        // uint16_t curpnum = 0;
        scurvecsv << ith << ",";

        // FIFO readout
        // TURNED OFF

        // I2C readout
        /*std::vector<uint32_t> counters;


        theMPAInterface->ReadASEvent(cMPA,counters);
        for(uint16_t row=rows.first; row<rows.second; row++)
            {
            for(uint16_t col=cols.first; col<cols.second; col++)
                {
                    std::cout <<row<<","<<col<<" "<<counters[curpnum]<< std::endl;
                    curpnum+=1;
                }
            }*/

        std::vector<uint32_t> countersfifo;
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->GetData(pBoard, countersfifo);

        // countersfifo = [0];
        // Randomly the counters fail
        // this fixes the issue but this needs to be looked at further

        totalevents = std::accumulate(countersfifo.begin() + 1, countersfifo.end(), 0);
        std::cout << "3 " << countersfifo.size() << " " << totalevents << std::endl;
        std::cout << totalevents << std::endl;
        if(totaleventsprev > 50 and totalevents == 0)
        {
            ith -= 1;
            nrep += 1;
            std::cout << "Repeat " << nrep << std::endl;
            if(nrep < 5) continue;
            totaleventsprev = 0;
        }
        int icf = 0;
        for(auto cf: countersfifo)
        {
            std::cout << "CF " << icf << " " << cf << std::endl;
            icf += 1;
        }
        int impa = 0;
        for(auto cMPA: *ChipVec)
        {
            if(cMPA->getFrontEndType() == FrontEndType::SSA) continue;

            for(size_t icc = 0; icc < 1920; icc++)
            {
                int curc = countersfifo[impa * (1920) + 60 * nmpas + icc];
                std::cout << "i " << icc << " " << curc << std::endl;
                scurves2D[impa]->SetBinContent(icc, scurves[icc]->FindBin(ith), curc);
                scurves[impa * 1920 + icc]->SetBinContent(scurves[icc]->FindBin(ith), curc);
                scurvecsv << curc << ",";
            }
            nrep = 0;
            impa += 1;
        }

        scurvecsv << "\n";
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
        totaleventsprev = totalevents;
    }

    TCanvas* c1    = new TCanvas("c1", "c1", 1000, 500);
    int      ihist = 0;
    for(auto& hist: scurves)
    {
        // std::cout<<"drawing "<<ihist<<hist->>Integral()<<std::endl;
        if(ihist == 0)
        {
            hist->SetLineColor(1);
            hist->SetTitle(";Thresh DAC;Counts");
            hist->SetMaximum(3001);
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

    ihist = 0;
    for(auto& hist: scurves2D)
    {
        std::string curt;
        curt        = "scurvetempmpa" + std::to_string(ihist);
        TCanvas* c2 = new TCanvas(curt.c_str(), curt.c_str(), 1000, 500);
        hist->SetLineColor(1);
        hist->SetTitle(";pix num;Thresh DAC");
        hist->SetMaximum(3001);
        hist->SetStats(0);
        hist->Draw("COLZ");
        curt += ".root";
        c2->Print(curt.c_str(), "root");

        ihist += 1;
    }

    scurvecsv.close();

} // int main
