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
    std::pair<uint32_t, uint32_t> th   = {0, 40};

    std::vector<TH1F*> scurves;
    std::string        title;
    LOG(INFO) << BOLDRED << "6" << RESET;
    for(auto cMPA: *ChipVec)
    {
        MPA* theMPA = static_cast<MPA*>(cMPA);

        auto theMPAInterface = static_cast<MPAInterface*>(cTool.fReadoutChipInterface);
        theMPAInterface->Activate_async(cMPA);
        theMPAInterface->Set_calibration(cMPA, 50);

        uint32_t npixtot = 0;
        for(uint16_t row = rows.first; row < rows.second; row++)
        {
            for(uint16_t col = cols.first; col < cols.second; col++)
            {
                uint32_t gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                theMPAInterface->Enable_pix_counter(theMPA, gpix);
                title = std::to_string(row) + "," + std::to_string(col);
                scurves.push_back(new TH1F(title.c_str(), title.c_str(), 255, -0.5, 254.5));
                npixtot += 1;
            }
        }
        std::cout << "Numpix -- " << npixtot << std::endl;
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
            theMPAInterface->Set_threshold(theMPA, ith);

            std::this_thread::sleep_for(ShortWait);
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Send_pulses(2000);
            std::this_thread::sleep_for(ShortWait);
            // curpnum = 0;
            scurvecsv << ith << ",";

            // FIFO readout
            // TURNED OFF
            // countersfifo = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ReadoutCounters_MPA(0);
            // countersfifo = [0];
            // Randomly the counters fail
            // this fixes the issue but this needs to be looked at further
            totalevents = std::accumulate(countersfifo.begin() + 1, countersfifo.end(), 0);
            std::cout << totalevents << std::endl;
            if(totaleventsprev > 50 and totalevents == 0)
            {
                ith -= 1;
                nrep += 1;
                std::cout << "Repeat " << nrep << std::endl;
                if(nrep < 5) continue;
                totaleventsprev = 0;
            }

            for(size_t icc = 0; icc < 1920; icc++)
            {
                scurves[icc]->SetBinContent(scurves[icc]->FindBin(ith), countersfifo[icc]);
                scurvecsv << countersfifo[icc] << ",";
            }
            nrep = 0;

            // I2C readout
            /*for(int row=rows.first; row<rows.second; row++)
                {
                for(int col=cols.first; col<cols.second; col++)
                    {
                        counters[curpnum]=fMPAInterface->Read_pixel_counter(row, col);

                        scurves[curpnum]->SetBinContent(scurves[curpnum]->FindBin(ith), counters[curpnum]);
                        scurvecsv << counters[curpnum]<<",";
                        curpnum+=1;
                    }
                }

            std::cout<<"Thresh "<<ith<<" - Counts[0] "<<counters[0]<<" - Counts[1] "<<counters[1]<<std::endl;
            */
            scurvecsv << "\n";
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
            totaleventsprev = totalevents;
        }
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
    scurvecsv.close();

} // int main
