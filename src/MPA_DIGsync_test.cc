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

    // D19cFWInterface* IB = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardFWMap.find(0)->second); // There has to be a
    // better way! IB->PSInterfaceBoard_PowerOff_SSA();

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));

    HybridContainer* ChipVec = pBoard->at(0)->at(0);

    std::chrono::milliseconds LongPOWait(500);
    std::chrono::milliseconds ShortWait(10);

    auto thePSInterface = static_cast<PSInterface*>(cTool.fReadoutChipInterface);

    // thePSInterface->activate_I2C_chip();

    std::pair<uint32_t, uint32_t> rows = {5, 6};
    std::pair<uint32_t, uint32_t> cols = {68, 69};
    // std::pair<uint32_t, uint32_t> rows = {5,7};
    // std::pair<uint32_t, uint32_t> cols = {1,5};

    std::vector<TH1F*> scurves;
    std::string        title;
    std::cout << "Setup" << std::endl;
    // pBoard->setFrontEndType(FrontEndType::MPA);
    for(auto cMPA: *ChipVec)
    {
        if(cMPA->getFrontEndType() == FrontEndType::MPA)
        {
            MPA* theMPA = static_cast<MPA*>(cMPA);

            thePSInterface->Set_calibration(theMPA, 200);
            thePSInterface->Set_threshold(theMPA, 200);
            thePSInterface->Activate_ps(theMPA, 4);
            thePSInterface->Activate_sync(theMPA);
            thePSInterface->WriteChipReg(cMPA, "ClusterCut_ALL", 5);
        }
        if(cMPA->getFrontEndType() == FrontEndType::SSA)
        {
            thePSInterface->WriteChipReg(cMPA, "ReadoutMode", 0);
            thePSInterface->WriteChipReg(cMPA, "ClusterCut", 5);
            thePSInterface->WriteChipReg(cMPA, "FE_Calibration", 1);
            thePSInterface->WriteChipReg(cMPA, "Bias_CALDAC", 200);
            thePSInterface->WriteChipReg(cMPA, "Bias_THDAC", 250);
        }
    }
    Stubs    curstub;
    uint32_t npixtot = 0;

    // dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM( 0, 1, 6);
    dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(10, 60, 90, 0, 1, 1);
    uint32_t gpix = 0;
    for(size_t row = rows.first; row < rows.second; row++)
    {
        for(size_t col = cols.first; col < cols.second; col++)
        {
            std::cout << "TSST " << row << "," << col << "," << gpix << std::endl;
            for(auto cMPA: *ChipVec)
            {
                if(cMPA->getFrontEndType() == FrontEndType::MPA)
                {
                    MPA* theMPA = static_cast<MPA*>(cMPA);

                    gpix = theMPA->PNglobal(std::pair<uint32_t, uint32_t>(row, col));
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x0);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix-1), 0x37);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix), 0x37);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_P" + std::to_string(gpix+1), 0x37);

                    thePSInterface->WriteChipReg(cMPA, "DigPattern_ALL", 0xFF);
                    // thePSInterface->WriteChipReg(cMPA, "DigPattern_P" + std::to_string(gpix),0xFF);
                }
                if(cMPA->getFrontEndType() == FrontEndType::SSA)
                {
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_ALL", 0x0);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col-1), 0x9);
                    thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col), 0x9);
                    // thePSInterface->WriteChipReg(cMPA, "ENFLAGS_S" + std::to_string(col+1), 0x9);

                    // thePSInterface->WriteChipReg(cMPA, "SAMPLINGMODE_ALL",0x1);
                    thePSInterface->WriteChipReg(cMPA, "DigCalibPattern_L_ALL", 0x01);
                    thePSInterface->WriteChipReg(cMPA, "DigCalibPattern_H_ALL", 0x00);
                }
            }

            for(size_t ilat = 0; ilat < 30; ilat++)
            {
                std::cout << "ilat " << ilat << std::endl;
                for(auto cMPA: *ChipVec)
                {
                    // L1OffsetPeri_1  	0x0 	 0x882A 0x5 	0x5
                    // L1OffsetPeri_2  	0x0 	 0x882B 0x5 	0x5
                    // SSAOffset_1  	0x0 	 0x882C 0x5 	0x5
                    // SSAOffset_2  	0x0 	 0x882D 0x5 	0x5
                    size_t writelat = 58;

                    // size_t  writelat=60;
                    // if (cMPA->getFrontEndType() == FrontEndType::SSA)writelat-=2;
                    thePSInterface->WriteChipReg(cMPA, "TriggerLatency", writelat, false);
                    // thePSInterface->WriteChipReg(cMPA, "L1Offset_2_ALL", (0x0100 & ilat) >> 8,false);

                    if(cMPA->getFrontEndType() == FrontEndType::MPA)
                    {
                        thePSInterface->WriteChipReg(cMPA, "LatencyRx320", 1); // 3
                        thePSInterface->WriteChipReg(cMPA, "EdgeSelT1Raw", 2);
                        thePSInterface->WriteChipReg(cMPA, "LatencyRx40", ilat);
                    }
                }
                // cTool.fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", ilat);
                // for(size_t ion = 0; ion <1; ion++)
                //{

                cTool.fBeBoardInterface->ChipReSync(pBoard);
                static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->ResetReadout();

                cTool.ReadNEvents(pBoard, 50);

                std::this_thread::sleep_for(ShortWait);

                const std::vector<Event*>& events = cTool.GetEvents();
                int                        nev    = 0;
                // int nevtot=0;
                std::cout << "TSST " << row << "," << col << "," << gpix << std::endl;

                uint32_t NPclustot = 0;
                uint32_t NSclustot = 0;

                for(__attribute__((unused)) auto& ev: events)
                {
                    for(auto cMPA: *ChipVec)
                    {
                        if(cMPA->getFrontEndType() != FrontEndType::MPA) continue;

                        NPclustot += static_cast<D19cCic2Event*>(ev)->GetNPixelClusters(0);
                        NSclustot += static_cast<D19cCic2Event*>(ev)->GetNStripClusters(0);

                        std::vector<PCluster> Pclus = static_cast<D19cCic2Event*>(ev)->GetPixelClusters(0, cMPA->getId());
                        std::vector<SCluster> Sclus = static_cast<D19cCic2Event*>(ev)->GetStripClusters(0, cMPA->getId());

                        // std::cout << "NPclus "<<+NPclus<<" NSclus "<<+NSclus <<std::endl;

                        for(auto& pc: Pclus)
                        {
                            std::cout << "-------------------------------PIXELS-------------------------------" << std::endl;
                            std::cout << "fAddress " << +pc.fAddress << std::endl;
                            std::cout << "fWidth " << +pc.fWidth << std::endl;
                            std::cout << "fZpos " << +pc.fZpos << std::endl << std::endl;
                        }
                        for(auto& sc: Sclus)
                        {
                            std::cout << "-------------------------------STRIPS-------------------------------" << std::endl;
                            std::cout << "fAddress? " << +sc.fAddress << std::endl;
                            std::cout << "fWidth " << +sc.fWidth << std::endl;
                            std::cout << "fMip " << +sc.fMip << std::endl << std::endl;
                        }

                        // std::cout << "stubs "<< std::endl;
                        // for( auto& st: stubs)
                        //  {
                        // std::cout << "-------------------------------STUBS-------------------------------"<< std::endl;
                        //   std::cout << "getPosition "<<+st.getPosition()<< std::endl;
                        // std::cout << "getBend "<<+st.getBend()<< std::endl;
                        //   std::cout << "getRow "<<+st.getRow()<< std::endl;
                        //   std::cout << "getCenter "<<+st.getCenter()<< std::endl<< std::endl;
                        //}
                    }
                    nev += 1;
                }
                std::cout << "NPclustot " << NPclustot << std::endl;
                std::cout << "NSclustot " << NSclustot << std::endl;

                npixtot += 1;
            }
        }

        std::cout << "Numpix -- " << npixtot << std::endl;
    }

} // int main
