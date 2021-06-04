// Simple test script to demonstrate use of middleware for the purposes of usercode development

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/FrontEndDescription.h"
#include "../HWDescription/MPA.h"
//#include "../HWDescription/OuterTrackerModule.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../HWInterface/MPAInterface.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/D19cMPAEvent.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/BackEndAlignment.h"
#include "../tools/Tool.h"
#include "TCanvas.h"
#include "TH1.h"
#include "tools/CicFEAlignment.h"
#include "tools/PSAlignment.h"
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
    std::string       cHWFile = "settings/PS_HalfModule.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);

    cTool.ConfigureHw();

    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Start(0);
    cCicAligner.waitForRunToBeCompleted();

    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cBackEndAligner.Align();
    cBackEndAligner.resetPointers();

    PSAlignment cPSAligner;
    cPSAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cPSAligner.Align();
    // cPSAligner.Reset(); something noot working here -- for next push

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));

    HybridContainer* ChipVec = pBoard->at(0)->at(0);

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);

    auto thePSInterface = static_cast<PSInterface*>(cTool.fReadoutChipInterface);

    bool     do_ss   = false;
    bool     do_pp   = true;
    uint32_t truelat = cTool.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");

    // uint32_t dvallat   = cTool.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
    // auto cStubOffset = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->getStubOffset();
    std::pair<uint32_t, uint32_t> rows = {5, 8};
    std::pair<uint32_t, uint32_t> cols = {40, 45};

    std::vector<TH1F*> scurves;
    std::string        title;

    // uint32_t RTR=0;
    for(auto cMPA: *ChipVec)
    {
        if(cMPA->getFrontEndType() == FrontEndType::MPA)
        {
            MPA* theMPA = static_cast<MPA*>(cMPA);
            thePSInterface->Activate_ps(theMPA, 8);
            if(do_ss) thePSInterface->Activate_ss(theMPA, 1);
            if(do_pp) thePSInterface->Activate_pp(theMPA, 1);
            // RTR=thePSInterface->ReadChipReg(cMPA, "RetimePix");
        }
    }

    // size_t  writeslat=truelat-1-cStubOffset-RTR;
    // std::cout <<"Writing stublat "<<writeslat<< std::endl;
    // cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", writeslat);

    Stubs                 curstub;
    uint32_t              npixtot        = 0;
    std::vector<uint32_t> maxvals        = std::vector<uint32_t>(5, 0);
    uint32_t              maxNSclustot   = 0;
    uint32_t              maxNPclustot   = 0;
    uint32_t              maxNStubs      = 0;
    uint32_t              maxSUM         = 0;
    uint32_t              maxSUMSclustot = 0;
    uint32_t              maxSUMPclustot = 0;
    uint32_t              maxSUMNStubs   = 0;
    std::vector<uint32_t> exp;
    std::vector<uint32_t> obs;
    std::vector<bool>     functionarr;

    uint32_t gpix = 0;

    cTool.fBeBoardInterface->ChipReSync(pBoard);
    static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ResetReadout();
    std::this_thread::sleep_for(ShortWait);

    for(size_t row = rows.first; row < rows.second; row++)
    {
        for(size_t col = cols.first; col < cols.second; col++)
        {
            for(auto cMPA: *ChipVec)
            {
                if(cMPA->getFrontEndType() == FrontEndType::MPA)
                {
                    MPA* theMPA = static_cast<MPA*>(cMPA);

                    gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x0);
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix), 0x37);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix+1), 0x37);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix+2), 0x37);
                    thePSInterface->WriteChipReg(cMPA, "DigPattern_ALL", 0x01);
                    thePSInterface->WriteChipReg(cMPA, "TriggerLatency", truelat - 1, false);
                }
                if(cMPA->getFrontEndType() == FrontEndType::SSA)
                {
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x0);
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col), 0x9);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col+2), 0x9);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col+3), 0x9);
                    thePSInterface->WriteChipReg(cMPA, "DigCalibPattern_L_ALL", 0x01);
                    thePSInterface->WriteChipReg(cMPA, "DigCalibPattern_H_ALL", 0x00);
                    thePSInterface->WriteChipReg(cMPA, "TriggerLatency", truelat - 2, false);
                }
            }

            std::this_thread::sleep_for(ShortWait);

            cTool.ReadNEvents(pBoard, 5);

            std::this_thread::sleep_for(ShortWait);

            const std::vector<Event*>& events = cTool.GetEvents();
            int                        nev    = 0;
            std::cout << "ROW,COL " << row << "," << col << "," << gpix << std::endl;

            uint32_t NPclustot      = 0;
            uint32_t NSclustot      = 0;
            uint32_t Nstub          = 0;
            uint32_t MatchNPclustot = 0;
            uint32_t MatchNSclustot = 0;
            uint32_t MatchNstub     = 0;
            for(auto& ev: events)
            {
                for(auto cMPA: *ChipVec)
                {
                    if(cMPA->getFrontEndType() != FrontEndType::MPA) continue;

                    NPclustot += static_cast<D19cCic2Event*>(ev)->GetNPixelClusters(0);
                    NSclustot += static_cast<D19cCic2Event*>(ev)->GetNStripClusters(0);

                    std::vector<PCluster> Pclus = static_cast<D19cCic2Event*>(ev)->GetPixelClusters(0, cMPA->getId());
                    std::vector<SCluster> Sclus = static_cast<D19cCic2Event*>(ev)->GetStripClusters(0, cMPA->getId());
                    std::cout << "----- " << std::endl;
                    std::cout << "CHIP " << +cMPA->getId() << std::endl;
                    std::cout << "----- " << std::endl;
                    std::cout << "NPIX " << static_cast<D19cCic2Event*>(ev)->GetNPixelClusters(0) << std::endl;
                    for(auto& pc: Pclus)
                    {
                        if((col) == pc.fAddress and (row - 1) == pc.fZpos) MatchNPclustot += 1;
                        std::cout << "-------------------------------PIXELS-------------------------------" << std::endl;
                        std::cout << "fAddress " << +pc.fAddress << " vs " << col << std::endl;
                        std::cout << "fWidth " << +pc.fWidth << std::endl;
                        std::cout << "fZpos " << +pc.fZpos << " vs " << row - 1 << std::endl; // Why row start from 0??
                    }
                    std::cout << "NSTRIPS " << static_cast<D19cCic2Event*>(ev)->GetNStripClusters(0) << std::endl;
                    for(auto& sc: Sclus)
                    {
                        if((col) == sc.fAddress) MatchNSclustot += 1;
                        std::cout << "-------------------------------STRIPS-------------------------------" << std::endl;
                        std::cout << "fAddress? " << +sc.fAddress << " vs " << col << std::endl;
                        std::cout << "fWidth " << +sc.fWidth << std::endl;
                        std::cout << "fMip " << +sc.fMip << std::endl << std::endl;
                    }
                    std::vector<Stub> stubs = static_cast<D19cCic2Event*>(ev)->StubVector(0, cMPA->getId());
                    Nstub += stubs.size();
                    for(auto& st: stubs)
                    {
                        std::cout << "-------------------------------STUBS-------------------------------" << std::endl;
                        std::cout << "getPosition " << +st.getPosition() << " vs " << (col)*2 << std::endl;
                        obs.push_back(st.getPosition());
                        exp.push_back(col);
                        if(st.getPosition() == ((col)*2)) MatchNstub += 1;
                        std::cout << "getBend " << +st.getBend() << std::endl;
                        std::cout << "getRow " << +st.getRow() << " vs " << row - 1 << std::endl;
                    }
                }
                nev += 1;
            }
            uint32_t totsum = MatchNstub;
            if(NSclustot == MatchNSclustot) totsum += MatchNSclustot;
            if(NPclustot == MatchNPclustot) totsum += MatchNPclustot;
            if(totsum == 13)
                functionarr.push_back(true);
            else
                functionarr.push_back(false);

            if((MatchNSclustot > maxNSclustot) and (NSclustot == MatchNSclustot)) { maxNSclustot = MatchNSclustot; }
            if(MatchNPclustot > maxNPclustot and (NPclustot == MatchNPclustot)) { maxNPclustot = MatchNPclustot; }
            if(MatchNstub > maxNStubs) { maxNStubs = MatchNstub; }
            if(totsum > maxSUM)
            {
                maxSUMSclustot = MatchNSclustot;
                maxSUMPclustot = MatchNPclustot;
                maxSUMNStubs   = MatchNstub;
            }

            std::cout << "NPclustot:" << NPclustot << " MatchNPclustot:" << MatchNPclustot << std::endl;
            std::cout << "NSclustot:" << NSclustot << " MatchNSclustot:" << MatchNSclustot << std::endl;
            std::cout << "maxSUMSclustot " << maxSUMSclustot << " maxSUMPclustot " << maxSUMPclustot << " maxSUMNStubs " << maxSUMNStubs << std::endl;
            std::cout << "Nstub:" << Nstub << " MatchNstub " << MatchNstub << " max:" << maxNStubs << std::endl;
            for(auto fc: functionarr) std::cout << "fc:" << fc << " ";
            std::cout << std::endl;
            npixtot += 1;
        }

        std::cout << "Numpix -- " << npixtot << std::endl;
    }

} // int main
