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
    std::string       cHWFile = "settings/D19C_MPA_PreCalibSYNC.xml";
    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);

    cTool.ConfigureHw();

    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    cBackEndAligner.Align();
    cBackEndAligner.resetPointers();

    // D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a
    // better way! IB->PSInterfaceBoard_PowerOff_SSA();

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));

    HybridContainer* ChipVec = pBoard->at(0)->at(0);

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);

    auto theMPAInterface = static_cast<MPAInterface*>(cTool.fReadoutChipInterface);

    // theMPAInterface->activate_I2C_chip();

    std::pair<uint32_t, uint32_t> rows = {5, 10};
    std::pair<uint32_t, uint32_t> cols = {7, 80};
    // std::pair<uint32_t, uint32_t> rows = {5,7};
    // std::pair<uint32_t, uint32_t> cols = {1,5};

    std::vector<TH1F*> scurves;
    std::string        title;
    std::cout << "Setup" << std::endl;

    for(auto cMPA: *ChipVec)
    {
        MPA* theMPA = static_cast<MPA*>(cMPA);
        // ReadoutChip* theMPA = static_cast<ReadoutChip*>(cMPA);

        theMPAInterface->Set_calibration(theMPA, 200);
        theMPAInterface->Set_threshold(theMPA, 200);
        theMPAInterface->Activate_sync(theMPA);
        theMPAInterface->Activate_pp(theMPA);

        Stubs    curstub;
        uint32_t npixtot = 0;
        // theMPAInterface->WriteChipReg(cMPA, "ClusterCut_ALL",1);
        // theMPAInterface->WriteChipReg(cMPA,"EdgeSelT1Raw", 0x0);
        // theMPAInterface->WriteChipReg(cMPA,"RetimePix", 0x1);
        // theMPAInterface->WriteChipReg(cMPA,"EdgeSelTrig", 0xff);

        /*for(size_t row = rows.first; row < rows.second; row++)
        {
            for(size_t col = cols.first; col < cols.second; col++)
            {
                uint32_t gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                theMPAInterface->Enable_pix_BRcal(cMPA, gpix, "rise", "edge");
                theMPAInterface->Disable_pixel(cMPA, gpix);
            }
        }*/
        // mysyscontroller.theMPAInterface->Start ( pBoard );
        // theMPAInterface->Enable_pix_BRcal(cMPA, 0, "rise", "level");
        // for(uint32_t apix = 1; apix <= 1920; apix++) theMPAInterface->Enable_pix_BRcal(cMPA, apix, "rise", "edge");
        theMPAInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x57);

        for(size_t row = rows.first; row < rows.second; row++)
        {
            for(size_t col = cols.first; col < cols.second; col++)
            {
                static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();
                static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();

                // theMPAInterface->enableInjection(cMPA,true);

                std::this_thread::sleep_for(ShortWait);
                uint32_t gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                // theMPAInterface->Disable_pixel(cMPA, 0);
                theMPAInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x57);
                theMPAInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix), 0x57);
                // theMPAInterface->WriteChipReg(cMPA, "ClusterCut_P" + std::to_string(gpix), 0x57);
                std::cout << row << "," << col << "," << gpix << std::endl;
                // theMPAInterface->Enable_pix_BRcal(cMPA, gpix, "rise", "edge");

                // std::this_thread::sleep_for(ShortWait);

                // std::this_thread::sleep_for(ShortWait);

                for(size_t ilat = 0; ilat < 1000; ilat++)
                {
                    // ilat=28;
                    std::cout << "ilat " << ilat << std::endl;
                    // for(size_t rr = 1; rr < 17; rr++)
                    //  {
                    //  theMPAInterface->WriteChipReg(cMPA, "L1Offset_1_ALL", (0x00FF & ilat) >> 0 );
                    // theMPAInterface->WriteChipReg(cMPA, "L1Offset_2_ALL", (0x0100 & ilat) >> 8);
                    // }

                    // theMPAInterface->WriteChipReg(cMPA, "PhaseShift", ilat);
                    theMPAInterface->WriteChipReg(cMPA, "L1Offset_1_ALL", (0x00FF & 56) >> 0);
                    theMPAInterface->WriteChipReg(cMPA, "L1Offset_2_ALL", (0x0100 & 56) >> 8);
                    cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", ilat);

                    // theMPAInterface->WriteChipReg(cMPA, "PhaseShift", ilat);
                    // cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.zero_suppression_enable", 0);
                    // cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.stub_trigger_delay_value", ilat);

                    cTool.fBeBoardInterface->ChipReSync(pBoard);
                    static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ResetReadout();

                    // theMPAInterface->WriteChipReg(cMPA, "LatencyRx40", ilat);
                    // theMPAInterface->enableInjection(cMPA,true);
                    cTool.ReadNEvents(pBoard, 100);
                    std::this_thread::sleep_for(ShortWait);
                    std::this_thread::sleep_for(ShortWait);
                    std::this_thread::sleep_for(ShortWait);

                    auto rawstubs =
                        static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ReadBlockRegValue("fc7_daq_stat.physical_interface_block.stat_slvs_debug_mpa_stub_0", 80);
                    std::vector<std::vector<uint8_t>> stubs(5, vector<uint8_t>(40, 0));
                    uint32_t                          line  = 0;
                    uint32_t                          cycle = 0;

                    for(size_t ist = 0; ist < 50; ist++)
                    {
                        // LOG(INFO) <<BOLDRED<<rawstubs[ist]<<std::dec<< RESET;
                        for(size_t ib = 0; ib < 4; ib++)
                        {
                            // LOG(INFO) <<BOLDRED<<std::bitset<32>(rawstubs[ist])<<std::dec<< RESET;
                            stubs[line][cycle] = ((rawstubs[ist]) & (0xFF << ib * 8)) >> ib * 8; // to_number(reverse_mask(word),(i+1)*8,i*8)
                            // LOG(INFO) <<BOLDRED<<line<<","<<cycle<<" "<<std::bitset<8>(stubs[line][cycle])<<std::dec<< RESET;
                            cycle += 1;
                            if(cycle == 40)
                            {
                                line += 1;
                                cycle = 0;
                            }
                        }
                    }

                    Stubs    fst = theMPAInterface->Format_stubs(stubs);
                    uint32_t nst = 0;
                    for(auto& st1: fst.nst)
                    {
                        if(st1 != 0 and false)
                        {
                            std::cout << "CYCLE " << uint32_t(nst) << std::endl;
                            std::cout << "pos " << uint32_t(fst.pos[nst][0]) << std::endl;
                            // for( auto& st2: fst.pos[nst])std::cout << "pos "<<uint32_t(st2)<< std::endl;
                            // for( auto& st2: fst.row[nst])std::cout << "row "<<uint32_t(st2)<< std::endl;
                            // for( auto& st2: fst.cur[nst])std::cout << "cur "<<uint32_t(st2)<< std::endl;
                        }
                        nst += 1;
                    }

                    // std::this_thread::sleep_for(ShortWait);
                    // static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->Send_pulses(1500);
                    // std::this_thread::sleep_for(ShortWait);

                    // std::this_thread::sleep_for(ShortWait);
                    /*for(uint32_t cpix = 1; cpix <= 1920; cpix++)
                        {
                        uint8_t cRP1 = theMPAInterface->ReadChipReg(cMPA, "ReadCounter_LSB_P" + std::to_string(cpix));
                        uint8_t cRP2 = theMPAInterface->ReadChipReg(cMPA, "ReadCounter_MSB_P" + std::to_string(cpix));

                        std::cout<<"counts"<<cpix<<" "<<(cRP2*256) + cRP1<<std::endl;
                        }*/
                    const std::vector<Event*>& events = cTool.GetEvents();

                    // const std::vector<Event*> &eventVector = cTool.GetEvents();
                    int nev    = 0;
                    int nevtot = 0;
                    int nstub  = 0;
                    for(__attribute__((unused)) auto& ev: events)
                    {
                        // std::cout << clus << std::endl;
                        std::vector<PCluster> clus = static_cast<D19cMPAEvent*>(ev)->GetPixelClusters(0, 0);
                        // nevtot+=clus.size();
                        nevtot += static_cast<D19cMPAEvent*>(ev)->GetNHits(0, 0);

                        std::cout << "GetNStubs -- " << +static_cast<D19cMPAEvent*>(ev)->GetNStubs(0, 0) << std::endl;

                        std::vector<Stub> stubs = static_cast<D19cMPAEvent*>(ev)->StubVector(0, 0);
                        nstub += stubs.size();
                        // std::cout << "pclus "<< std::endl;
                        /*for( auto& pc: clus)
                            {
                             std::cout << "---------------------------------------------------------------------------------------------------------------------------------------"<< std::endl;
                             std::cout << "fAddress "<<+pc.fAddress<< std::endl;
                             std::cout << "fWidth "<<+pc.fWidth<< std::endl;
                             std::cout << "fZpos "<<+pc.fZpos << std::endl<< std::endl;
                            }*/
                        // std::cout << "stubs "<< std::endl;
                        /*for( auto& st: stubs)
                            {
                               std::cout << "getPosition "<<+st.getPosition()<< std::endl;
                               std::cout << "getBend "<<+st.getBend()<< std::endl;
                               std::cout << "getRow "<<+st.getRow()<< std::endl;
                               std::cout << "getCenter "<<+st.getCenter()<< std::endl<< std::endl;
                            }*/
                        nev += 1;
                    }
                    if(nevtot != 0) std::cout << "nevtot " << nevtot << std::endl;
                    if(nstub != 0) std::cout << "nstub " << nstub << std::endl;
                }

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
