
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
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/BackEndAlignment.h"
#include "../tools/Tool.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"
#include "tools/CicFEAlignment.h"
#include "tools/PSAlignment.h"
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <numeric> // for std::accumulate

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

    ArgvParser cmd;
    cmd.defineOption("file", "Hw Description File . Default value: settings/PS_HalfModulePSAS.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/PS_HalfModulePSAS.xml";

    // std::string       cHWFile = "settings/PS_HalfModulePSAS.xml";
    std::cout << cHWFile << std::endl;
    std::stringstream outp;
    Tool              cTool;

    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    cTool.ConfigureHw();
    /*
        // align ASICs on PS module
        PSAlignment cPSAlignment;
        cPSAlignment.Inherit(&cTool);
        cPSAlignment.Initialise();
        cPSAlignment.MapMPAOutputs();
        cPSAlignment.Reset();


        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(&cTool);
        cCicAligner.Start(0);
        cCicAligner.waitForRunToBeCompleted();
        cCicAligner.Reset();
        cCicAligner.dumpConfigFiles();

        BackEndAlignment cBackEndAligner;
        cBackEndAligner.Inherit(&cTool);
        cBackEndAligner.Start(0);
        cBackEndAligner.waitForRunToBeCompleted();
        cBackEndAligner.Reset();

        cPSAlignment.Align();

    */

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);
    std::chrono::milliseconds MEGAWait(70000);
    // should be done from configure hw

    // std::pair<uint32_t, uint32_t> rows = {1, 17};
    // std::pair<uint32_t, uint32_t> cols = {1, 121};
    std::pair<uint32_t, uint32_t> th = {190, 191};

    std::vector<TH2F*> scurves2D;

    std::string title;
    auto        theMPAInterface = static_cast<PSInterface*>(cTool.fReadoutChipInterface);

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    pBoard->setEventType(EventType::PSAS);
    HybridContainer* ChipVec = pBoard->at(0)->at(0);

    if(pBoard->getEventType() == EventType::PSAS)
    {
        LOG(INFO) << BOLDRED << "PSAS" << RESET;
        LOG(INFO) << BOLDRED << "PSAS" << RESET;
        LOG(INFO) << BOLDRED << "PSAS" << RESET;
        LOG(INFO) << BOLDRED << "PSAS" << RESET;
    }
    else
    {
        LOG(INFO) << BOLDRED << "BADEVENT" << RESET;
        LOG(INFO) << BOLDRED << "BADEVENT" << RESET;
        LOG(INFO) << BOLDRED << "BADEVENT" << RESET;
        LOG(INFO) << BOLDRED << "BADEVENT" << RESET;
    }
    std::cout << "1" << std::endl;
    std::vector<int> totalev;
    std::vector<int> totalevPRE;
    int              impa = 0;

    for(auto cMPA: *ChipVec)
    {
        std::cout << "2" << std::endl;
        totalev.push_back(0);
        totalevPRE.push_back(0);
        uint16_t npix = 1920;
        if(cMPA->getFrontEndType() == FrontEndType::SSA) npix = 120;
        if(cMPA->getFrontEndType() == FrontEndType::MPA)
        {
            theMPAInterface->WriteChipReg(cMPA, "ReadoutMode", 0x1);
            theMPAInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0xd7);
            theMPAInterface->WriteChipReg(cMPA, "AnalogueAsync", 0x1);
            theMPAInterface->WriteChipReg(cMPA, "Threshold", 0x20);
            theMPAInterface->WriteChipReg(cMPA, "InjectedCharge", 0x0);
        }
        else
        {
            theMPAInterface->WriteChipReg(cMPA, "ReadoutMode", 0x1);
            theMPAInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x5);
            theMPAInterface->WriteChipReg(cMPA, "AnalogueAsync", 0x1);
            theMPAInterface->WriteChipReg(cMPA, "Threshold", 0x20);
            theMPAInterface->WriteChipReg(cMPA, "InjectedCharge", 0x0);
        }

        title = "mpa" + std::to_string(impa);
        scurves2D.push_back(new TH2F(title.c_str(), title.c_str(), float(npix), -0.5, float(npix) - 0.5, 255, -0.5, 254.5));
        impa += 1;
    }

    std::vector<uint16_t> countersfifo;
    // uint32_t curpnum = 0;
    // uint32_t totalevents     = 0;
    uint32_t nrep = 0;
    for(uint16_t ith = th.first; ith < th.second; ith++)
    {
        // if (not (ith%10==0)) continue;
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();

        std::cout << "ITH= " << ith << std::endl;
        for(auto cMPA: *ChipVec)
        {
            // MPA* theMPA = static_cast<MPA*>(cMPA);
            // theMPAInterface->Set_threshold(theMPA, ith);
            theMPAInterface->WriteChipReg(cMPA, "Threshold", ith);
            // if(cMPA->getFrontEndType() == FrontEndType::SSA)theMPAInterface->Set_threshold(theMPA, ith);
        }

        while(true)
        {
            std::cout << "" << std::endl;
            std::cout << "Shutter Open" << std::endl;
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Open_shutter(8);
            // std::this_thread::sleep_for(LongPOWait*2*60);
            std::this_thread::sleep_for(LongPOWait * 2 * 10);
            // static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Send_pulses(1000);
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Close_shutter(8);
            // static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Start_counters_read(8);
            // static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Start_counters_read(8);
            // std::cout << "Shutter close " << std::endl;
            std::cout << "Shutter Close" << std::endl;

            scurvecsv << ith << ",";

            // I2C readout
            // std::cout << "Get Data" << std::endl;
            std::vector<uint32_t> countersfifo;
            impa = 0;
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->GetData(pBoard, countersfifo);
            auto asev = D19cPSEventAS(pBoard, countersfifo);

            for(auto cMPA: *ChipVec)
            {
                std::cout << "MPA " << impa << " " << asev.GetNHits(0, cMPA->getId()) << std::endl;

                // std::vector<uint32_t> countersfifo;
                // static_cast<MPAInterface*>(cTool.fReadoutChipInterface)->ReadASEvent(cMPA,countersfifo,std::pair<uint32_t, uint32_t> {1,1920});
                // static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->ReadASEvent(cMPA,countersfifo);
                // totalevents = std::accumulate(countersfifo.begin() + 1, countersfifo.end(), 0);
                // std::cout <<"MPA "<<impa<<" "<<totalevents<<std::endl;
                impa += 1;
            }
        }

        continue;
        // std::vector<uint32_t> countersfifo;

        // std::cout << "Get Data Done" << std::endl;

        // countersfifo = [0];
        // Randomly the counters fail
        // this fixes the issue but this needs to be looked at further

        //
        // std::cout << "3 "<<countersfifo.size()<<" "<<totalevents<< std::endl;
        // std::cout << "totalevents "<<totalevents << std::endl;

        bool     torepeat = false;
        int      impa     = 0;
        uint32_t curindex = 0;
        for(auto cMPA: *ChipVec)
        {
            // int mpacount=0;

            uint16_t npix = 1920;
            if(cMPA->getFrontEndType() == FrontEndType::SSA)
            {
                std::cout << "SSA" << std::endl;
                npix = 120;
            }

            for(size_t icc = 0; icc < (npix); icc++)
            {
                int curc = countersfifo[curindex];
                totalev[impa] += curc;
                // std::cout <<icc<<","<<impa<<curc<<std::endl;

                scurves2D[impa]->SetBinContent(scurves2D[impa]->GetXaxis()->FindBin(icc), scurves2D[impa]->GetYaxis()->FindBin(ith), curc);
                // std::pair<uint32_t, uint32_t>pnlocal = static_cast<MPA*>(cMPA)->PNlocal(icc+1);
                // std::cout << "pix "<<icc<<","<<pnlocal.first<<","<<pnlocal.second<<","<<curc<< std::endl;
                // std::cout << "REGLOBAL "<<icc<<","<<static_cast<MPA*>(cMPA)->PNglobal(pnlocal)<< std::endl;

                // scurves[impa*npix+icc]->SetBinContent(scurves[icc]->FindBin(ith), curc);
                scurvecsv << curc << ",";
                curindex += 1;
            }
            /*if (ith%3==0)
            {
                std::cout << "Oh no! " << std::endl;
                totalev[impa]=0;
            }*/

            std::cout << "totalevPRE " << totalevPRE[impa] << " totalev " << totalev[impa] << std::endl;
            if(totalev[impa] == 0 and totalevPRE[impa] > 50)
            {
                std::cout << "To Repeat " << std::endl;
                torepeat = true;
            }
            else
            {
                totalevPRE[impa] = totalev[impa];
            }

            impa += 1;
        }

        if(torepeat)
        {
            if(nrep < 2)
            {
                ith -= 1;
                nrep += 1;
                std::cout << "Repeat " << nrep << std::endl;
                continue;
            }

            nrep = 0;
        }
        scurvecsv << "\n";
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters(8);
    }
    // TFile *curf = TFile::Open("scurves.root","RECREATE");
    // curf->cd();

    uint32_t ihist = 0;
    for(auto& hist: scurves2D)
    {
        TFile* curf = TFile::Open(("scurves_MPA" + std::to_string(ihist) + ".root").c_str(), "RECREATE");
        curf->cd();
        std::string curt;
        curt        = "scurvetempmpa" + std::to_string(ihist);
        TCanvas* c2 = new TCanvas(curt.c_str(), curt.c_str(), 1000, 500);
        hist->SetLineColor(1);
        hist->SetTitle(";pix num;Thresh DAC");
        // hist->SetMaximum(3001);
        hist->SetStats(0);
        hist->Draw("COLZ");

        c2->Print(("c1" + curt).c_str(), "root");
        c2->Write(("c1" + curt).c_str());
        hist->Write(curt.c_str());

        ihist += 1;
        curf->Write();
    }

    scurvecsv.close();

} // int main
