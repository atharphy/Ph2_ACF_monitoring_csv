#include "PSHybridTester.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
#define MIN(x, y) ((x) < (y) ? (x) : (y)) // calculate minimum between two values

// initialize the static member

PSHybridTester::PSHybridTester() : Tool() {}

PSHybridTester::~PSHybridTester() {}

void PSHybridTester::Initialise()
{
#ifdef __TCUSB__
    LOG(INFO) << BOLDBLUE << "Selecting antenna channel to "
              << " disable all charge injection" << RESET;
    TC_PSFE cTC_PSFE;
    cTC_PSFE.antenna_fc7(uint16_t(513), TC_PSFE::ant_channel::NONE);
#endif
}
void PSHybridTester::SSAOutputsPogoScope(std::vector<std::vector<std::string>>& cReadLines, std::string pSSAPairSel, bool pTrigger, bool pPrintScoped)
{
    for(auto cBoard: *fDetectorContainer) { this->SSAOutputsPogoScope(cReadLines, pSSAPairSel, cBoard, pTrigger, pPrintScoped); }
}
void PSHybridTester::MPATest()
{
    for(auto cBoard: *fDetectorContainer) { this->MPATest(cBoard); }
}
void PSHybridTester::CheckI2C()
{
    for(auto cBoard: *fDetectorContainer) { this->CheckI2C(cBoard); }
}
void PSHybridTester::CheckCounters()
{
    for(auto cBoard: *fDetectorContainer) { this->CheckCounters(cBoard); }
}
void PSHybridTester::ReadAntennaVoltage() { this->ReadHybridVoltage("AntennaPullUp"); }
void PSHybridTester::SSAPairSelect(const std::string& SSAPairSel)
{
    for(auto cBoard: *fDetectorContainer) { this->SSAPairSelect(cBoard, SSAPairSel); }
}
void PSHybridTester::SSAOutputsPogoScope(BeBoard* pBoard, bool pTrigger)
{
    uint32_t cNtriggers = this->findValueInSettings<double>("PSHybridDebugDuration");
    if(pTrigger)
        LOG(INFO) << BOLDBLUE << "Going to send " << +cNtriggers << " triggers to debug L1 SSA output " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Going to capture for " << +cNtriggers * 10 << " ms to debug stub SSA output " << RESET;

    // pair id
    for(uint8_t cPairId = 0; cPairId < 2; cPairId++)
    {
        uint8_t cAlignmentPattern = (cPairId == 0) ? 0x05 : 0x01;
        // first I would like to align the lines in the back-end
        if(!pTrigger)
        {
            bool cAligned = true;
            for(uint8_t cLineId = 1; cLineId < 8; cLineId++)
            {
                cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, 0, cPairId, cLineId, cAlignmentPattern, 8);
                if(!cAligned)
                {
                    LOG(INFO) << BOLDRED << "Alignment failed on line " << +cLineId << ". Retrying with pattern for SSA3... " << +cLineId << RESET;
                    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, 0, cPairId, cLineId, 0x04, 8);
                    if(!cAligned) { LOG(INFO) << BOLDRED << "Alignment failed on line " << +cLineId << RESET; }
                }
            }
            // if aligned then try and scope
            LOG(INFO) << "SLVS debug [stub lines] : Chip " << +cPairId << RESET;
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "SLVS debug [L1 line] : Chip " << +cPairId << RESET;
        }
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cPairId);
        if(pTrigger)
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1ADebug(false);
        else
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 8);
        }
    }
}

void PSHybridTester::SSAOutputsPogoScope(std::vector<std::vector<std::string>>& cReadLines, std::string pSSAPairSel, BeBoard* pBoard, bool pTrigger, bool pPrintScoped)
{
    for(uint8_t cPairId = 0; cPairId < 2; cPairId++)
    {
       
        if(!pTrigger)
        {
            LOG(INFO) << "SLVS debug [stub lines] : Chip " << +cPairId << RESET;
        }
        else
        {
            if(pPrintScoped){ LOG(INFO) << BOLDBLUE << "SLVS debug [L1 line] : Chip " << +cPairId << RESET; }
        }
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cPairId);
        if(pTrigger)
        {
            std::string cReadLine;
            std::vector<std::string> cReadLineVector(0);
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1ADebug((uint8_t)1, cReadLine, pPrintScoped, false);
            cReadLineVector.push_back(cReadLine);
            cReadLines.push_back(cReadLineVector);
        }
        else
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 8, cReadLines, pPrintScoped);
        }
    }
}

void PSHybridTester::FillSSATree(std::string pParameter, std::string pValue)
{
    #ifdef __USE_ROOT__
        fResultFile->cd();

        if(gROOT->FindObject("SSATree") != nullptr) 
        { 
            fSSATree = static_cast<TTree*>(gROOT->FindObject("SSATree")); 
            // TBranch* cParameterBranch = fSSATree->GetBranch("Parameter");
            // TBranch* cValueBranch = fSSATree->GetBranch("Value");

            // cParameterBranch->SetAddress(&cParameter);
            // cValueBranch->SetAddress(&cValue);
        }
        else
        {
            fSSATree = new TTree("SSATree", "Bad Lines in the SSA test");
            fSSATree->Branch("Parameter", &fSSATreeParameter);
            fSSATree->Branch("Value", &fSSATreeValue);
        }
        // TBranch* cParameterBranch = SSATree->GetBranch("Parameter");
        // TBranch* cValueBranch = SSATree->GetBranch("Value");
        // LOG(INFO) << "cParameterBranch " << +&cParameterBranch << RESET;
        // LOG(INFO) << "cValueBranch " << +&cValueBranch << RESET;

        fSSATreeParameter = pParameter;
        fSSATreeValue = pValue;
        fSSATree->Fill();
        // LOG(INFO) << "Stored value " << cValue << " as parameter " << cParameter << RESET;
        // SSATree->Write();
        // delete fSSATree;
    #endif
}


void PSHybridTester::SSAOutputsPogoDebug(BeBoard* pBoard, bool pTrigger)
{
    uint32_t cNtriggers = this->findValueInSettings<double>("PSHybridDebugDuration");
    if(pTrigger)
        LOG(INFO) << BOLDBLUE << "Going to send " << +cNtriggers << " triggers to debug L1 SSA output " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Going to capture for " << +cNtriggers * 10 << " ms to debug stub SSA output " << RESET;

    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_input", 0xFFFFFFFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_input", 1);
    // send N triggers
    uint8_t cTriggerCounter = 0;
    do
    {
        if(pTrigger) fBeBoardInterface->ChipTrigger(pBoard);
        // fBeBoardInterface->ChipTestPulse(pBoard);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cTriggerCounter++;
    } while(cTriggerCounter < cNtriggers);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
    auto cDebugDone = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.input_lines_debug_done");
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cDebugDone = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.input_lines_debug_done");
    } while(cDebugDone != 0xFFFFFFFF);
    LOG(INFO) << BOLDBLUE << "Input lines debug done: 0x" << std::hex << cDebugDone << std::dec << RESET;
    auto cMapIterator = fInputDebugMap.begin();
    do
    {
        auto cRegisterName = cMapIterator->first;
        // only print out registers that are of interest
        bool cPrintMapItem = (cRegisterName.find("ssa") != std::string::npos);
        if(!cPrintMapItem)
        {
            cMapIterator++;
            continue;
        }

        if(pTrigger)
            cPrintMapItem = cPrintMapItem && (cRegisterName.find("l1") != std::string::npos);
        else
            cPrintMapItem = cPrintMapItem && (cRegisterName.find("trig") != std::string::npos);

        cPrintMapItem = cPrintMapItem || (cRegisterName.find("clk") != std::string::npos);
        cPrintMapItem = cPrintMapItem || (cRegisterName.find("fcmd") != std::string::npos);

        if(cPrintMapItem)
        {
            char cRegName[80];
            sprintf(cRegName, "fc7_daq_stat.physical_interface_block.debug_blk_counter%2d", cMapIterator->second);
            auto cResult = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
            LOG(INFO) << BOLDBLUE << "Register is " << cRegisterName << " counter address is " << +cMapIterator->second << " counter value is " << cResult << RESET;
        }
        cMapIterator++;
    } while(cMapIterator != fInputDebugMap.end());
}
void PSHybridTester::SSAPairSelect(BeBoard* pBoard, const std::string& SSAPairSel)
{
    try
    {
        auto BitPattern = fSSAPairSelMap.at(SSAPairSel);
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select", BitPattern);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto cRegister = this->fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select");
        LOG(INFO) << BLUE << "SSA pair " << SSAPairSel << " is selected register value is " << std::bitset<4>(cRegister) << RESET;
    }
    catch(const std::out_of_range& e)
    {
        LOG(ERROR) << BOLDRED << "Invalid SSA pair select option: " << SSAPairSel << RESET;
        LOG(INFO) << BLUE << "Possible options are: "
                  << "01, 12, 23, 34, 45, 56, 67" << RESET;
    }
}
void PSHybridTester::SSATestStubOutput(const std::string& cSSAPairSel)
{
    for(auto cBoard: *fDetectorContainer) { this->SSATestStubOutput(cBoard, cSSAPairSel); }
}
void PSHybridTester::SSATestL1Output(const std::string& cSSAPairSel)
{
    for(auto cBoard: *fDetectorContainer) { this->SSATestL1Output(cBoard, cSSAPairSel); }
}
void PSHybridTester::SSATestLateralCommunication(const std::string& cSSAPairSel, bool pSweepPhaseSelector)
{
    for(auto cBoard: *fDetectorContainer) { this->SSATestLateralCommunication(cBoard, cSSAPairSel, pSweepPhaseSelector); }
}
void PSHybridTester::SelectCIC(bool pSelect)
{
#ifdef __TCUSB__
    TC_PSFE cTC_PSFE;
    // enable CIC in mode of front-end hybrid
    if(pSelect)
        cTC_PSFE.mode_control(TC_PSFE::mode::CIC_IN);
    else
        cTC_PSFE.mode_control(TC_PSFE::mode::SSA_OUT);
#endif
}
void PSHybridTester::AlignCICout(uint8_t pPattern)
{
    this->SelectCIC(true);
    bool cRetry = true;
    bool cSuccess;
    int  cBadLines[4] = {0, 0, 0, 0};
    for(int cTries = 0; (cTries < 2) && cRetry; cTries++)
    {
        cRetry = false;

        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fCicInterface->SelectMux(cCic, 6 + cTries);
                } // hybrid
            }     // module
            for(int cLine = 1; cLine < 5; cLine++)
            {
                cSuccess = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, 0, 0, cLine, pPattern, 8);
                if(!cSuccess)
                {
                    LOG(INFO) << BOLDRED << "CIC OUT Line " << +cLine << " was not aligned correctly." << RESET;
                    cBadLines[cLine - 1]++;
                }
                else
                {
                    LOG(DEBUG) << BOLDGREEN << "CIC OUT Line " << +cLine << " was aligned correctly." << RESET;
                }
                cRetry |= !cSuccess;
            }
        }
    }
    for(int cLine = 1; cLine < 5; cLine++)
    {
        if(cBadLines[cLine - 1] == 2)
        {
#ifdef __USE_ROOT__
            fillSummaryTree("Bad_CIC_OUT_Line", (double)cLine);
#endif
        }
    }
}
void PSHybridTester::MPATest(BeBoard* pBoard)
{
    uint32_t cTestPatterns[4] = {0xAA, 0xCC, 0x00, 0xFF};
    // String with the binary representation of the pattern
    int         cTotalBadLines = 0;                // Number of bad CIC in lines
    std::string cParameter[4]  = {"", "", "", ""}; // Placeholder for the name of the summaryTree parameter name
    std::string cValue[4]      = {"", "", "", ""};

    DPInterface         cDPInterfacer;
    BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>(this->fBeBoardFWMap.find(0)->second);

    bool    cRun     = true;
    uint8_t cRuns    = 0;
    uint8_t cMaxRuns = 1;

    TTree* CICinTree[4];

    // Create TTrees to contain bad lines
    for(int cPatternId = 0; cPatternId < 4; cPatternId++)
    {
        uint32_t cPattern = cTestPatterns[cPatternId];

        std::stringstream sstream;
        std::string       cPattern_str;
        std::string       cPattern_str_hex;

        cPattern_str = std::bitset<8>(cPattern).to_string();

        sstream << std::hex << cPattern;
        cPattern_str_hex = sstream.str();

        fResultFile->cd();
        std::string cTitle    = Form("CICinTree0x%s", cPattern_str_hex.c_str());
        std::string cDesc     = Form("Bad Lines in the CIC IN test for pattern 0x%s", cPattern_str_hex.c_str());
        CICinTree[cPatternId] = new TTree(cTitle.c_str(), cDesc.c_str());
        CICinTree[cPatternId]->Branch("Parameter", &cParameter[cPatternId]);
        CICinTree[cPatternId]->Branch("Value", &cValue[cPatternId]);
        cParameter[cPatternId] = "Pattern";
        cValue[cPatternId]     = cPattern_str;
        CICinTree[cPatternId]->Fill();
    }

    // enable CIC mux - phyPort 0 - 9 are stub lines. phyPort 10 and 11 are L1 lines.
    for(uint8_t cPhyPort = 0; cPhyPort < 12; cPhyPort++)
    {
        cRuns = 0;
        cRun  = true;

        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SelectMux(cCic, cPhyPort);
            } // hybrid
        }     // module

        std::vector<std::vector<std::string>> cReadLines; // Container for the received lines
        uint8_t                               cPhyPortBadLines = 0;

        while(cRun && (cRuns <= cMaxRuns))
        {
            cPhyPortBadLines = 0;
            cRun             = false;

            if(cRuns > 0) LOG(INFO) << BOLDRED << "Retrying test on phyPort " << +cPhyPort << "." << RESET;

            // align lines 1,2,3 and 4 (first 4 stub lines from CIC )
            cDPInterfacer.Stop(cInterface);
            cDPInterfacer.Configure(cInterface, 0xAA);
            cDPInterfacer.Start(cInterface);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            for(uint8_t cLineId = 1; cLineId < 5; cLineId++)
            {
                uint8_t cHybridId      = 0;
                uint8_t cChipId        = 0;
                uint8_t cPatternPeriod = 8;
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybridId, cChipId, cLineId, 0xAA, cPatternPeriod);
            }

            uint8_t cBadLines[4] = {0, 0, 0, 0};

            // Stop 0xAA pattern and use test patterns
            for(int cPatternId = 0; cPatternId < 4; cPatternId++)
            {
                cReadLines.clear();

                uint32_t cPattern = cTestPatterns[cPatternId];

                std::stringstream sstream;
                std::string       cPattern_str;
                std::string       cPattern_str_hex;

                cPattern_str = std::bitset<8>(cPattern).to_string();
                sstream << std::hex << cPattern;
                cPattern_str_hex = sstream.str();

                // fResultFile->cd();
                // std::string cTitle = Form("CICinTree0x%s",cPattern_str_hex.c_str());
                // std::string cDesc = Form("Bad Lines in the CIC IN test for pattern %s", cPattern_str_hex.c_str());
                // TTree* CICinTree = new TTree( cTitle.c_str() , cDesc.c_str() );
                // CICinTree->Branch("Parameter", &cParameter);
                // CICinTree->Branch("Value", &cValue);
                // cParameter = "Pattern";
                // cValue     = cPattern_str;
                // CICinTree->Fill();

                LOG(INFO) << BOLDCYAN << "Testing phyPort " << +cPhyPort << " using pattern " << cPattern_str << RESET;

                cDPInterfacer.Stop(cInterface);
                cDPInterfacer.Configure(cInterface, cPattern);
                cDPInterfacer.Start(cInterface);

                std::this_thread::sleep_for(std::chrono::milliseconds(10));

                // check output
                fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 4, cReadLines);

                for(int a = 0; a < (int)cReadLines.size(); a++)
                {
                    std::string cLine;
                    int         badLines;
                    std::string cSubLine;
                    // bool bad = false;
                    for(int b = 0; b < (int)cReadLines[a].size(); b++)
                    {
                        badLines = 0;
                        cLine    = cReadLines[a][b];
                        // Go throught the read line and compare with pattern
                        for(int k = 0; (k + cPattern_str.length()) < cLine.length(); k += cPattern_str.length())
                        {
                            cSubLine           = cLine.substr(k, cPattern_str.length());
                            bool cPatternFound = false;
                            for(int j = 0; j < (int)cPattern_str.length(); j++) cPatternFound |= ((cPattern_str.substr(j, cPattern_str.length() - j) + cPattern_str.substr(0, j)) == cSubLine);
                            if(!cPatternFound)
                            {
                                badLines++;
                                cRun = true;
                            }
                        }

                        std::string recovered = "";
                        for(int k = 0; (k + cPattern_str.length()) < cLine.length(); k += cPattern_str.length()) { recovered += cLine.substr(k, cPattern_str.length()) + "  "; }
                        if(badLines > 2) // 35
                        {
                            cBadLines[b] = 1;
                            LOG(INFO) << "The pattern " << cPattern_str << " was" << BOLDRED << " NOT" << RESET << " recovered correctly on" << BOLDRED << " PhyPort " << +cPhyPort << " line " << b
                                      << "." << RESET;
                            std::string recovered = "";
                            for(int k = 0; (k + cPattern_str.length()) < cLine.length(); k += cPattern_str.length()) { recovered += cLine.substr(k, cPattern_str.length()) + "  "; }
                            LOG(INFO) << "Recovered:  " << recovered << RESET;
                            cParameter[cPatternId] = "";
                            cParameter[cPatternId] = std::to_string(cPhyPort) + "_" + std::to_string(b);
                            cValue[cPatternId]     = cLine;
                            CICinTree[cPatternId]->Fill();
                        }
                        else
                        {
                            // cBadLines[b] = 0
                            LOG(DEBUG) << "The pattern 0x" << cPattern_str_hex << " was" << BOLDGREEN << " recovered correctly " << RESET << "on PhyPort " << +cPhyPort << " line " << b << "."
                                       << RESET;
                            LOG(DEBUG) << "Recovered:  " << recovered << RESET;
                        }
                    }
                }
            }
            cRuns++;
            for(int i = 0; i < 4; i++) cPhyPortBadLines += cBadLines[i];
        }
        cTotalBadLines += cPhyPortBadLines;
    }
    LOG(INFO) << BOLDYELLOW << "***************************************Bad CIC IN lines in the hybrid : " << cTotalBadLines << "*************************************" << RESET;
#ifdef __USE_ROOT__
    fillSummaryTree("CIC IN bad lines", cTotalBadLines);
#endif
}
void PSHybridTester::SweepPhaseAlignment(uint8_t pPhase)
{
#ifdef __USE_ROOT__
    TString cParameter;
    TString cValue;
    fResultFile->cd();

    TTree* PATree = nullptr;
    if(gROOT->FindObject("PATree") != nullptr) { PATree = static_cast<TTree*>(gROOT->FindObject("PATree")); }
    else
    {
        PATree = new TTree("PATree", "Phase alignment test");
        PATree->Branch("Parameter", &cParameter);
        PATree->Branch("Value", &cValue);
    }
#endif

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                if(cCic != NULL)
                {
                    for(int cPhyPort = 0; cPhyPort < 10; cPhyPort++)
                    {
                        fCicInterface->SelectMux(cCic, cPhyPort);
                        // align lines 1,2,3 and 4 (first 4 stub lines from CIC )
                        for(uint8_t cLineId = 1; cLineId < 5; cLineId++)
                        {
                            uint8_t cHybridId      = 0;
                            uint8_t cChipId        = 0;
                            uint8_t cPatternPeriod = 8;
                            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(cBoard, cHybridId, cChipId, cLineId, 0xAA, cPatternPeriod);
                        }

                        std::vector<std::vector<std::string>> cReadLines; // Container for the received lines
                        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 4, cReadLines);

#ifdef __USE_ROOT__
                        for(int i = 0; i < (int)cReadLines.size(); i++)
                        {
                            cParameter.Clear();
                            cValue.Clear();
                            for(int j = 0; j < (int)cReadLines[i].size(); j++)
                            {
                                cParameter = "Phase_" + std::to_string(pPhase) + "_Phy_" + std::to_string(cPhyPort) + "_" + std::to_string(j);
                                LOG(INFO) << cParameter << RESET;
                                cValue = cReadLines[i][j];
                                PATree->Fill();
                            }
                        }
#endif
                    }
                }
            }
        }
    }
}

void PSHybridTester::SSATestStubOutput(BeBoard* pBoard, const std::string& cSSAPairSel)
{
    #ifdef __TC_USB__
        this->SelectCIC(false);
    #endif
    this->SSAPairSelect(pBoard, cSSAPairSel);
    // now cycle through chips one at a time ..
    // and configure chips to output a fixed pattern
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;

                // uint8_t cPattern= (uint8_t)cReadoutChip->getId()+1;
                uint8_t cPattern = (cReadoutChip->getId() % 2 == 0) ? 0xFA : 0xF5;

                // make sure SSA is configured to output a test pattern on SLVS out
                if(cReadoutChip->getId() == (int)cSSAPairSel.at(1) - '0' || cReadoutChip->getId() == (int)cSSAPairSel.at(0) - '0')
                {
                    LOG(INFO) << BOLDBLUE << "Chip " << +cReadoutChip->getId() << " configured to output " << std::bitset<8>(cPattern) << " on SLVS output" << RESET;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 1);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine0", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine1", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine2", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine3", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine4", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine5", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine6", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine7/FIFOconfig", cPattern);
                }
                else
                {
                    cPattern = cReadoutChip->getId();
                    LOG(INFO) << BOLDBLUE << "Chip " << +cReadoutChip->getId() << " configured to output " << std::bitset<8>(cPattern) << " on SLVS output" << RESET;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 1);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine0", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine1", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine2", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine3", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine4", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine5", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine6", cPattern);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternStubLine7/FIFOconfig", cPattern);
                }
            } // chip
        }     // hybrid
    }         // module
    // now capture output on pogo sockets and store them
    std::vector<std::vector<std::string>> cReadLines;                  // Container for the scoped lines
    this->SSAOutputsPogoScope(cReadLines, cSSAPairSel, pBoard, false); // Recover Scoped lines
    std::string pPattern_str;

    std::string cParameter = "";
    std::string cValue = "";
    // fResultFile->cd();

    // TTree* SSATree = nullptr;
    // if(gROOT->FindObject("SSATree") != nullptr) 
    // { 
    //     SSATree = static_cast<TTree*>(gROOT->FindObject("SSATree")); 
    //     TBranch* cParameterBranch = SSATree->GetBranch("Parameter");
    //     TBranch* cValueBranch = SSATree->GetBranch("Value");

    //     cParameterBranch->SetAddress(&cParameter);
    //     cValueBranch->SetAddress(&cValue);
    // }
    // else
    // {
    //     SSATree = new TTree("SSATree", "Bad Lines in the SSA test");
    //     SSATree->Branch("Parameter", &cParameter);
    //     SSATree->Branch("Value", &cValue);
    // }

    // Process scoped lines
    for(int a = 0; a < (int)cReadLines.size(); a++)
    {
        if((((int)cSSAPairSel.at(0) - '0') % 2 == 0))
        {
            pPattern_str = (((int)cSSAPairSel.at(a) - '0') % 2 == 0) ? std::bitset<8>(0xF5).to_string() : std::bitset<8>(0xFA).to_string();
            // if((int)cSSAPairSel.at(1 - a) - '0' == 3) pPattern_str = std::bitset<8>(0x04).to_string(); // SSA3 is configured to output the same pattern as SSA4. Used on the hybrids with the SSA3/SSA4 bug
        }
        else
        {
            pPattern_str = (((int)cSSAPairSel.at(a) - '0') % 2 == 0) ? std::bitset<8>(0xFA).to_string() : std::bitset<8>(0xF5).to_string();
            // if((int)cSSAPairSel.at(a) - '0' == 3) pPattern_str = std::bitset<8>(0x04).to_string(); // SSA3 is configured to output the same pattern as SSA4. Used on the hybrids with the SSA3/SSA4 bug.
        }
        // pPattern_str = ( ((int)cSSAPairSel.at(0)-'0')%2!=0 ) ? std::bitset<8>(  (int)cSSAPairSel.at(a) - '0' + 1  ).to_string() : std::bitset<8>(  (int)cSSAPairSel.at(1-a) - '0' + 1  ).to_string()
        // ;
        LOG(INFO) << "Checking for " << pPattern_str << RESET;
        std::string cLine    = "";
        float       distance = 0.0;
        int         badLines = 0;
        std::string cSubLine;
        for(int b = 0; b < (int)cReadLines[a].size(); b++)
        {
            cLine = cReadLines[a][b]; // Get scoped line
            LOG(DEBUG) << cLine << RESET;
            bool ok = false;
            // Go throught the read line and compare with pattern
            for(int k = 0; (k + pPattern_str.length()) < cLine.length(); k += pPattern_str.length())
            {
                cSubLine = cLine.substr(k, pPattern_str.length());
                LOG(DEBUG) << BOLDBLUE << cSubLine << RESET;
                // distance += FuzzyCompareStrings(cSubLine, pPattern_str);
                // aux = k + 1;
                for(int i = 0; i < (int)cSubLine.length() - 1; i++)
                {
                    bool cPatternFound = false;
                    for(int j = 0; j < (int)pPattern_str.length(); j++) cPatternFound |= ((pPattern_str.substr(j, pPattern_str.length() - j) + pPattern_str.substr(0, j)) == cSubLine);
                    // if ( ( cSubLine != pPattern_str && cSubLine != (pPattern_str.substr(1, 7) + pPattern_str.front()) && cSubLine != pPattern_str.back() + pPattern_str.substr(0, 7) ) && ( (cSubLine
                    // != pPattern_str.substr(2, 6) + pPattern_str.substr(0,2)) && (cSubLine != pPattern_str.substr(3,5) + pPattern_str.substr(0, 3)) && (cSubLine != pPattern_str.substr(4,4) +
                    // pPattern_str.substr(0, 4)) ) ) {
                    if(!cPatternFound) { LOG(DEBUG) << BOLDRED << cSubLine.substr(i, cSubLine.size() - i) + cSubLine.substr(0, i) << RESET; }
                    else
                    {
                        ok = true;
                        // LOG (INFO) << BOLDMAGENTA << cSubLine.substr(i, cSubLine.size()-i ) + cSubLine.substr(0,i) << " equals " << pPattern_str << RESET;
                        LOG(INFO) << BOLDMAGENTA << pPattern_str << " pattern found." << RESET;
                        break;
                    }
                }
                if(ok) break;
            }
            if(!ok)
            {
                cParameter = "";
                if((((int)cSSAPairSel.at(0) - '0') % 2 == 0))
                { // Check this
                    cParameter = "FE" + std::to_string((int)cSSAPairSel.at(1 - a) - '0') + "_" + std::to_string(b);
                }
                else
                {
                    cParameter = "FE" + std::to_string((int)cSSAPairSel.at(a) - '0') + "_" + std::to_string(b);
                }
                cValue = cLine;
                LOG(INFO) << cParameter << "  " << cValue << RESET;
#ifdef __USE_ROOT__
                // SSATree->Fill();
                FillSSATree(cParameter, cValue);
#endif
                badLines++;
            }
        }
        if((((int)cSSAPairSel.at(0) - '0') % 2 == 0))
        { // Check this
#ifdef __USE_ROOT__
            fillSummaryTree(Form("SSA%d_stub", (int)cSSAPairSel.at(1 - a) - '0'), badLines);
#endif
        }
        else
        {
#ifdef __USE_ROOT__
            fillSummaryTree(Form("SSA%d_stub", (int)cSSAPairSel.at(a) - '0'), badLines);
#endif
        }
    }
    // SSATree->Write();

    // Disable SLVSTestOutput
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0);
            } // chip
        }     // hybrid
    }         // module
}
void PSHybridTester::SSATestL1Output(BeBoard* pBoard, const std::string& cSSAPairSel)
{
    std::string cParameter = "";
    std::string cValue = "";
    // fResultFile->cd();

    // TTree* SSATree = nullptr;
    // if(gROOT->FindObject("SSATree") != nullptr) 
    // { 
    //     SSATree = static_cast<TTree*>(gROOT->FindObject("SSATree")); 
        // TBranch* cParameterBranch = SSATree->GetBranch("Parameter");
        // TBranch* cValueBranch = SSATree->GetBranch("Value");

        // cParameterBranch->SetAddress(&cParameter);
        // cValueBranch->SetAddress(&cValue);
    // }
    // else
    // {
    //     SSATree = new TTree("SSATree", "Bad Lines in the SSA test");
    //     SSATree->Branch("Parameter", &cParameter);
    //     SSATree->Branch("Value", &cValue);
    // }

    // select SSA pair
    this->SSAPairSelect(pBoard, cSSAPairSel);
    // now cycle through chips one at a time ..
    std::vector<bool> cLinesInPairOK = {false, false};
    bool cWithSSA2 = false;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;
                if( ( cReadoutChip->getId() != (int)(cSSAPairSel[0]-'0') ) && ( cReadoutChip->getId() != (int)(cSSAPairSel[1]-'0') ) ) // Check only the chips in the pair
                    continue;
                if(cReadoutChip->getFrontEndType() == FrontEndType::SSA2 ) // SSA2 has a test feature for the L1 line. A pattern can be configured and outputed on the line.
                {
                    cWithSSA2 = true;
                    int cPattern = ( cReadoutChip->getId() % 2 == 0 )? 0xFA : 0xF5;
                    std::string cPattern_str = ( cReadoutChip->getId() % 2 == 0 )? std::bitset<8>(0xFA).to_string() : std::bitset<8>(0xF5).to_string();
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 1);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPatternL1Line", cPattern);
                    LOG(INFO) << BOLDBLUE << "SSA#" << +cReadoutChip->getId() << " configured to output " << cPattern_str << " on the L1 line." << RESET;
                }
                if(cReadoutChip->getFrontEndType() == FrontEndType::SSA)
                {
                    //SSA1 : Scan phase alignment values.
                    LOG(INFO) << "SSA1. Testing the L1 line of SSA#" << +cReadoutChip->getId() << RESET;
                    D19cFWInterface::PhaseTuner cTuner;   
                    bool cPhaseTuned = false;       

                    uint8_t cL1LineId = 0;

                    cTuner.GetLineStatus(dynamic_cast<BeBoardFWInterface*>(this->fBeBoardFWMap.find(0)->second), cHybrid->getId(), cReadoutChip->getId(), cL1LineId);

                    for(int cDelay = 0; cDelay < 31 && !cPhaseTuned; cDelay++ )
                    {                        
                        std::vector<std::vector<std::string>> cReadLines; // Container for the scoped line

                        cTuner.SetLineMode(dynamic_cast<BeBoardFWInterface*>(this->fBeBoardFWMap.find(0)->second), cHybrid->getId(), cReadoutChip->getId(), cL1LineId, 2, cDelay, 0, 1, 0);
                        LOG(INFO) << BOLDMAGENTA << "Delay set to " << +cDelay << " ." << RESET;
                        
                        this->SSAOutputsPogoScope(cReadLines, cSSAPairSel, pBoard, true, false); // Don't print the scoped lines
                        std::string cReadLine = cReadLines[1-cReadoutChip->getId()%2][0];
                        
                        int cFirstBXCounter=0;

                        for (int cL1PacketId = 1 ; cL1PacketId < 4 ; cL1PacketId++)
                        {
                            std::size_t cL1HeaderPosition = cReadLine.find("0011" + std::bitset<4>(cL1PacketId).to_string() );
                            if( cL1HeaderPosition!=std::string::npos)
                            {
                                cL1HeaderPosition += 2;
                                LOG(DEBUG) << "Possible L1 packet position: " << cL1HeaderPosition << RESET;
                                std::size_t cL1PacketEndPosition = cReadLine.find("10000000",cL1HeaderPosition+2+4+9+120+24);
                                if(cL1PacketEndPosition!=std::string::npos && cL1PacketEndPosition == cL1HeaderPosition+2+4+9+120+24 )
                                {
                                    LOG(DEBUG) << "Header: " << cReadLine.substr(cL1HeaderPosition, 2) << RESET;
                                    if( cReadLine.substr(cL1HeaderPosition+2, 4) == std::bitset<4>(cL1PacketId).to_string() )
                                    {
                                        LOG(DEBUG) << "L1 Counter: " << cReadLine.substr(cL1HeaderPosition+2, 4) << RESET;
                                        if(cL1PacketId == 3) 
                                            cPhaseTuned = true;
                                    }
                                    else
                                    {
                                        LOG(INFO) << BOLDRED << "L1 Counter is wrong" << RESET;
                                        cPhaseTuned = false;
                                        continue;
                                    }
                                    LOG(DEBUG) << "BX Counter: " << cReadLine.substr(cL1HeaderPosition+2+4, 9) << RESET;
                                    int cBXCounter = std::stol(cReadLine.substr(cL1HeaderPosition+2+4, 9),0,2);
                                    if(cL1PacketId == 1)
                                        cFirstBXCounter = cBXCounter;
                                    else if(cL1PacketId == 3)
                                        cPhaseTuned &= ( cFirstBXCounter+2 == cBXCounter );
                                    LOG(DEBUG) << "Strips : " << cReadLine.substr(cL1HeaderPosition+2+4+9, 120) << RESET;
                                    LOG(DEBUG) << "MIP flags : " << cReadLine.substr(cL1HeaderPosition+2+4+9+120, 24) << RESET;
                                    LOG(DEBUG) << BOLDGREEN << +cL1PacketId << " L1 packet position: " << cL1HeaderPosition << " to " << +cL1PacketEndPosition << "." <<  RESET;
                                }
                            }
                            else
                            {
                                LOG(INFO) << BOLDRED << "L1 Header not found" << RESET;
                            }
                        }
                    }
                    cLinesInPairOK[1-cReadoutChip->getId()%2] = cPhaseTuned;
                }
            } // chip
        }     // hybrid
    }         // opticalGroup
    std::vector<std::vector<std::string>> cReadLines; // Container for the scoped line
    this->SSAOutputsPogoScope(cReadLines, cSSAPairSel, pBoard, true);

    if(cWithSSA2)
    {
        //Search for the transmitted pattern on the L1 lines
        for ( int cPairId = 0 ; cPairId < 2 ; cPairId++ )
        { 
            std::string cPattern_str = ( cPairId == 0 )? std::bitset<8>(0xFA).to_string() : std::bitset<8>(0xF5).to_string();
            LOG(INFO) << "SSA2. Checking for pattern " << cPattern_str << " on L1 line of chip " << +cPairId << " in pair." << RESET; 
            std::size_t cPatternLocation = cReadLines[cPairId][0].find(cPattern_str);
            if(cPatternLocation!=std::string::npos)
            {
                cLinesInPairOK[cPairId] = true;
            }
        }
    }
    for ( int cPairId = 0 ; cPairId < 2 ; cPairId++ )
    {
        int cChipId = ((int)(cSSAPairSel[0]-'0')%2==0) ? (int)(cSSAPairSel[1-cPairId]-'0') : (int)(cSSAPairSel[cPairId]-'0');
        LOG(DEBUG) << "Chip " << +cPairId << " in pair is SSA# " << +cChipId << ". Pair is " << cSSAPairSel << "." << RESET;
        if(cLinesInPairOK[cPairId])
        {
            LOG(INFO) << "L1 line in SSA#" << +cChipId << " (chip " << +cPairId << " in pair) is " << BOLDGREEN << "OK." << RESET; 
            fillSummaryTree("SSA"+std::to_string(cChipId)+"_L1", 0);
        }
        else
        {
            LOG(INFO) << "L1 line in SSA#" << +cChipId << " (chip " << +cPairId << " in pair) is " << BOLDRED << "BAD." << RESET;
            fillSummaryTree("SSA"+std::to_string(cChipId)+"_L1", 1);
            cParameter = " ";
            cParameter = "FE"+std::to_string(cChipId)+"_L1";
            cValue = "  ";
            // SSATree->Fill();
            FillSSATree(cParameter, cValue);
        }
    }

    // Disable SLVSTestOutput
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 0);
            } // chip
        }     // hybrid
    }         // module
}
void PSHybridTester::SSATestLateralCommunication(Ph2_HwDescription::BeBoard* pBoard, const std::string& pSSAPairSel, bool pSweepPhaseSelector)
{
    std::string cParameter = "";
    std::string cValue = "";

    this->SSAPairSelect(pBoard, pSSAPairSel);
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "EnableSLVSTestOutput", 0);
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "DigitalSync", 0);

    pBoard->setEventType(EventType::SCAS); //needed?

    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 6});
    uint8_t cLatency = 100;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cLatency});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    for ( uint8_t cPairId=0; cPairId<2; cPairId++)
    {        
        int cInjectedSSAId = (int)(pSSAPairSel[cPairId]-'0');
        int cAdjacentSSAId = (int)(pSSAPairSel[1-cPairId]-'0');
        uint8_t cInjectedStrip = 0;
        std::string cAdjacentSSACentroid = ""; // Centroid that should be generated on the adjacent SSA.
        LOG(INFO) << BOLDMAGENTA << "Injected chip is SSA#" << +cInjectedSSAId << " (chip " << +(1 - cInjectedSSAId%2) << " in StubDebug). Adjacent chip is SSA#" << cAdjacentSSAId << " (chip " << +(1 - cAdjacentSSAId%2) << " in StubDebug)." << RESET;

        bool cLineGood = false;

        bool cLateralPhaseSelectionSuccess = false;
        int cLateralPhase = 0;
        for ( cLateralPhase = 0; ( cLateralPhase < 8  && !cLateralPhaseSelectionSuccess ) && !( !pSweepPhaseSelector && cLateralPhase > 0 ); cLateralPhase++ )
        {
            if(pSweepPhaseSelector) { LOG(INFO) << "cLateralPhase " << std::bitset< 3 >( cLateralPhase ).to_string() << RESET; }
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    for(auto cReadoutChip: *cHybrid)
                    {
                        if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;
                        if( ( cReadoutChip->getId() != (int)(pSSAPairSel[0]-'0') ) && ( cReadoutChip->getId() != (int)(pSSAPairSel[1]-'0') ) ) // Check only the chips in the pair
                            continue;
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "TriggerLatency", cLatency - 2);
                        //configure Digital Injection
                        if( cReadoutChip->getId() == cInjectedSSAId )
                        {
                            LOG(INFO) << "Configuring SSA#" << +cReadoutChip->getId() << " to inject digital pulses for lateral communication test" << RESET;
                            if(cPairId == 1)
                            {
                                // cInjectedStrip = 0;
                                cInjectedStrip = 1;
                            }
                            else
                            {
                                // cInjectedStrip = 113;
                                cInjectedStrip = 118;
                            }

                            for(uint8_t strip = 0; strip < cReadoutChip->size(); strip++)
                            {
                                std::string cRegisterName = "DigCalibPattern_L_S" + std::to_string(strip+1);
                                // uint8_t cValue = ( strip == cInjectedStrip || strip == cInjectedStrip + 2 || strip == cInjectedStrip + 4 || strip == cInjectedStrip + 6  ) ? 1 : 0;
                                uint8_t cValue = ( strip == cInjectedStrip )? 1 : 0;
                                fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cValue);                                
                                cRegisterName = "ENFLAGS_S" + std::to_string(strip+1);
                                uint8_t cMask         = 0;
                                uint8_t cPolarity     = 0;
                                uint8_t cHitCounter   = 0;
                                uint8_t cDigitalCalib = cValue;
                                uint8_t cAnalogCalib  = 0;
                                uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
                                fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cEnFlags);
                            }
                        }
                        if( cReadoutChip->getId() == cAdjacentSSAId )
                        {
                            if ( pSweepPhaseSelector )
                            {
                                uint8_t cRegisterValue;
                                if(cPairId == 1)
                                {
                                    cRegisterValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, "LateralRX_sampling");
                                    LOG(INFO) << "LateralRX_sampling was " << std::bitset< 8 >( cRegisterValue ).to_string() << RESET;
                                    cRegisterValue = (cRegisterValue & 0x8F) | (cLateralPhase<<4);
                                    LOG(INFO) << "LateralRX_sampling (Right rx) set to " << std::bitset< 8 >( cRegisterValue ).to_string() << " on chip " << +cReadoutChip->getId() << RESET;
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "LateralRX_R_PhaseData", cRegisterValue, false); // Set right receiver of adjacent SSA
                                }
                                else
                                {
                                    cRegisterValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, "LateralRX_sampling");
                                    LOG(INFO) << "LateralRX_sampling was " << std::bitset< 8 >( cRegisterValue ).to_string() << RESET;
                                    cRegisterValue = (cRegisterValue & 0xF8) | cLateralPhase;
                                    LOG(INFO) << "LateralRX_sampling (Left rx) set to " << std::bitset< 8 >( cRegisterValue ).to_string() << " on chip " << +cReadoutChip->getId() << RESET;
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, "LateralRX_L_PhaseData", cRegisterValue, false); // Set left receiver of adjacent SSA
                                }
                            }
                            LOG(INFO) << "Configuring SSA#" << +cReadoutChip->getId() << " to NOT inject digital pulses for lateral communication test" << RESET;
                            for(uint8_t strip = 0; strip < cReadoutChip->size(); strip++)
                            {
                                std::string cRegisterName = "DigCalibPattern_L_S" + std::to_string(strip+1);
                                // uint8_t cValue = ( strip == 42 || strip == 44  || strip == 46 || strip == 48 || strip == 50 || strip == 52) ? 1 : 0;
                                uint8_t cValue = 0;
                                fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cValue);

                                cRegisterName = "ENFLAGS_S" + std::to_string(strip+1);
                                uint8_t cMask         = 0;
                                uint8_t cPolarity     = 0;
                                uint8_t cHitCounter   = 0;
                                uint8_t cDigitalCalib = cValue;
                                uint8_t cAnalogCalib  = 0;
                                uint8_t cEnFlags      = (cAnalogCalib << 4 | cDigitalCalib << 3 | cHitCounter << 2 | cPolarity << 1 | cMask);
                                fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cEnFlags);
                            }
                        }
                    } // chip
                } // hybrid
            } // optical group
            
            // Scope lines
            std::vector<std::vector<std::string>> cReadL1Lines;
            this->SSAOutputsPogoScope(cReadL1Lines, pSSAPairSel, pBoard, true, !pSweepPhaseSelector); //Scope SSA L1 lines
            std::vector<std::vector<std::string>> cReadStubLines;
            this->SSAOutputsPogoScope(cReadStubLines, pSSAPairSel, pBoard, false, !pSweepPhaseSelector); //Scope SSA stub lines

            //Check that the channels were actually injected in the injected SSA.
            std::string cInjectedSSAL1Data = (cInjectedSSAId %2 == 0 ) ? cReadL1Lines[1][0] : cReadL1Lines[0][0];
            std::size_t cL1PacketPosition = cInjectedSSAL1Data.find("0011");
            if( cL1PacketPosition != std::string::npos ) 
            {
                cL1PacketPosition += 2; //Set cL1PacketPosition to the real start of the packet
                std::size_t cL1PacketPositionEnd = cInjectedSSAL1Data.find("11110000000", cL1PacketPosition+2+9+9+120+24);
                if (cL1PacketPositionEnd!=std::string::npos && cL1PacketPositionEnd == cL1PacketPosition+2+9+9+120+24)
                {
                    std::vector<std::string> cDecodedL1Packet = this->DecodeSSAL1Packet((int)2,cInjectedSSAL1Data.substr(cL1PacketPosition, 2+9+9+120+24)); //TODO Pass SSA Version correctly with Readout type
                    std::string cStripsL1Packet = cDecodedL1Packet[3]; //Recover strips

                    LOG(INFO) << "Strips on injected SSA: " << cStripsL1Packet << RESET;

                    bool cInjectionSuccesfull = true;
                    for (uint8_t strip=0; strip < cStripsL1Packet.length() ; strip++)
                    {
                        LOG(DEBUG) << "Starting at 0: strip #" << +strip << "should be " << +( cInjectedStrip == strip ) << RESET;
                        cInjectionSuccesfull &=  (uint8_t)( cInjectedStrip == strip ) == (uint8_t)(cStripsL1Packet[cStripsL1Packet.length() - strip - 1]-'0');
                    }
                    if(!cInjectionSuccesfull)
                    {
                        LOG(INFO) << BOLDRED << "Digital injection failed on injected SSA (hits not present on L1 data)" << RESET; // The rest of the test is skipped
                        continue;
                    }
                }
                else
                {
                    LOG(ERROR) << "Couldn't find L1 packet on injected SSA L1 data." << RESET;
                    continue;
                }
            }
            else
            {
                LOG(ERROR) << "Couldn't find L1 packet injected SSA L1 data." << RESET;
                continue;
            }

            LOG(INFO) << "Injected chip is SSA#" << +cInjectedSSAId << " (chip " << +(1 - cInjectedSSAId%2) << " in StubDebug). Adjacent chip is SSA#" << cAdjacentSSAId << " (chip " << +(1 - cAdjacentSSAId%2) << " in StubDebug)." << RESET;
            int cAdjacentSSAStripNumber;
            if(cPairId == 1)
                cAdjacentSSAStripNumber = 119+cInjectedStrip+1;
            else
                cAdjacentSSAStripNumber = cInjectedStrip-120;
            LOG(INFO) << "cInjectedStrip " << +cInjectedStrip << ". cAdjacentSSAStripNumber " << +cAdjacentSSAStripNumber << RESET;
            cAdjacentSSACentroid = std::bitset< 8 >( cAdjacentSSAStripNumber*2+9 ).to_string();
            LOG(INFO) << "Centroid that should be generated on the adjacent SSA: " << cAdjacentSSACentroid << RESET;
            LOG(DEBUG) << "cInjectedSSACentroid " << std::bitset< 8 >( cInjectedStrip*2+9 ).to_string() << RESET;

            // Search the scoped lines for data on the non-injected chip
            std::vector<std::string> cAdjacentSSAOutputs = (cAdjacentSSAId %2 == 0 ) ? cReadStubLines[1] : cReadStubLines[0] ;
            for(uint8_t cLine = 0; cLine < cAdjacentSSAOutputs.size(); cLine++ )
            {
                cLineGood |= (cAdjacentSSAOutputs[cLine].find(cAdjacentSSACentroid) != std::string::npos);
            } 
            cLateralPhaseSelectionSuccess = cLineGood;
        }
        
        if(pSweepPhaseSelector)
        {
            // Reconfigure the chips to show output with correct phase selection values
            for(auto cOpticalReadout: *pBoard)
                {
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        for(auto cReadoutChip: *cHybrid)
                        {
                            if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;
                            if( ( cReadoutChip->getId() != (int)(pSSAPairSel[0]-'0') ) && ( cReadoutChip->getId() != (int)(pSSAPairSel[1]-'0') ) ) // Check only the chips in the pair
                                continue;
                            // LOG(INFO) << "Configuring SSA#" << +cReadoutChip->getId() << " for lateral communication test" << RESET;
                            fReadoutChipInterface->WriteChipReg(cReadoutChip, "TriggerLatency", cLatency - 2);

                            //configure Digital Injection
                            if( cReadoutChip->getId() == cInjectedSSAId )
                            {
                                LOG(DEBUG) << "Configuring SSA#" << +cReadoutChip->getId() << " to inject digital pulses for lateral communication test" << RESET;
                                if(cPairId == 1)
                                    cInjectedStrip = 1;
                                else
                                    cInjectedStrip = 118;

                                for(uint8_t strip = 0; strip < cReadoutChip->size(); strip++)
                                {
                                    std::string cRegisterName = "DigCalibPattern_L_S" + std::to_string(strip+1);
                                    uint8_t cValue = ( strip == cInjectedStrip )? 1 : 0;
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cValue);
                                    cRegisterName = "DigCalibPattern_L_S" + std::to_string(strip);
                                }
                            }
                            if( cReadoutChip->getId() == cAdjacentSSAId )
                            {
                                LOG(DEBUG) << "Configuring SSA#" << +cReadoutChip->getId() << " to NOT inject digital pulses for lateral communication test" << RESET;
                                for(uint8_t strip = 0; strip < cReadoutChip->size(); strip++)
                                {
                                    std::string cRegisterName = "DigCalibPattern_L_S" + std::to_string(strip+1);
                                    uint8_t cValue = 0;                        
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegisterName, cValue);
                                    LOG(DEBUG) << cRegisterName << " on chip " << +cReadoutChip->getId() << " set to " << +cValue << RESET;  
                                }
                            }
                        } // chip
                    } // hybrid
                } // optical group
                // // Scope lines
                std::vector<std::vector<std::string>> cReadL1Lines;
                this->SSAOutputsPogoScope(cReadL1Lines, pSSAPairSel, pBoard, true, true); //Scope SSA L1 lines
                std::vector<std::vector<std::string>> cReadStubLines;
                this->SSAOutputsPogoScope(cReadStubLines, pSSAPairSel, pBoard, false, true); //Scope SSA stub lines
        }
        if( cLineGood )
        {
            LOG(INFO) << "Lateral communication line from SSA#" << +cInjectedSSAId << " to SSA# " << +cAdjacentSSAId << " is " << BOLDGREEN << "GOOD." << RESET;
#ifdef __USE_ROOT__
            fillSummaryTree(Form("SSA%d_to_%d_lateral_line", cInjectedSSAId, cAdjacentSSAId), 0.0);
#endif
        }
        else
        {
            LOG(INFO) << "Lateral communication line from SSA#" << +cInjectedSSAId << " to SSA#" << +cAdjacentSSAId << " is " << BOLDRED << "BAD." << RESET;
#ifdef __USE_ROOT__
            fillSummaryTree(Form("SSA%d_to_%d_lateral_line", cInjectedSSAId, cAdjacentSSAId), 1.0);
            cParameter = " ";
            // cParameter = "FE"+std::to_string(cInjectedSSAId)+"_to_"+std::to_string(cAdjacentSSAId)+"_lateral_line";
            cParameter = "lateral_FE"+std::to_string(cInjectedSSAId)+"_to_"+std::to_string(cAdjacentSSAId);
            cValue = "  ";
            FillSSATree(cParameter, cValue);
#endif
        }
    }
}
std::vector<double> PSHybridTester::DecodeSSACentroids(std::vector<std::string> cStubLines) 
{
    LOG(INFO) << "Not implemented yet" << RESET;
    std::vector<double> cDecodedCentroids = {0.0, 1.0, 2.0};
    return cDecodedCentroids;
}
std::vector<std::string> PSHybridTester::DecodeSSAL1Packet(int pSSAType, std::string pL1Data)
{
    std::vector<std::string> cDecodedL1Packet;
    if ( pSSAType == 1 )
    {
        cDecodedL1Packet.push_back(pL1Data.substr(0, 2));            //[0] Header 
        cDecodedL1Packet.push_back(pL1Data.substr(2, 4));            //[1] L1 Counter
        cDecodedL1Packet.push_back(pL1Data.substr(2+4, 9));          //[2] BX Counter
        cDecodedL1Packet.push_back(pL1Data.substr(2+4+9, 120));      //[3] Strips
        cDecodedL1Packet.push_back(pL1Data.substr(2+4+9+120, 24));   //[4] MIP Flags
    }
    else if ( pSSAType == 2)
    {
        cDecodedL1Packet.push_back(pL1Data.substr(0, 2));            //[0] Header
        cDecodedL1Packet.push_back(pL1Data.substr(2, 9));            //[1] L1 Counter
        cDecodedL1Packet.push_back(pL1Data.substr(2+9, 9));          //[2] BX Counter
        cDecodedL1Packet.push_back(pL1Data.substr(2+9+9+24, 120));   //[3] Strips
        cDecodedL1Packet.push_back(pL1Data.substr(2+9+9, 24));       //[4] MIP Flags
    }
    else
    {
        LOG(INFO) << BOLDRED << "Wrong SSA type!" << RESET;
        exit(80);
    }        
    
    LOG(DEBUG) << "Header " << cDecodedL1Packet[0] << RESET;
    LOG(DEBUG) << "L1 Counter " << cDecodedL1Packet[1] << RESET;
    LOG(DEBUG) << "BX Counter " << cDecodedL1Packet[2] << RESET;
    LOG(DEBUG) << "Strips " << cDecodedL1Packet[3] << RESET;
    LOG(DEBUG) << "MIP flags " << cDecodedL1Packet[4] << RESET;
    return cDecodedL1Packet;
}
void PSHybridTester::SetHybridVoltage(uint32_t pUsbBus, uint8_t pUsbDev)
{
#ifdef __TCUSB__
    LOG(INFO) << "Setting hybrid voltage..." << RESET;
    // TC_PSFE cTC_PSFE( pUsbBus, pUsbDev );
    TC_PSFE cTC_PSFE;
    // cTC_PSFE.set_voltage(cTC_PSFE._1100mV,cTC_PSFE._1250mV);
    cTC_PSFE.set_voltage(cTC_PSFE._1150mV, cTC_PSFE._1250mV);
    // LOG(INFO) <<BOLDGREEN << "Set" << RESET;
#endif
}

void PSHybridTester::CheckI2C(BeBoard* pBoard)
{
#ifdef __ROOT__
    TH1I* fI2CTransactions = new TH1I("I2CTransactions", "I2C transactions status");
#endif

    int total = 0;
    int value = 0;
    int bad   = 0;
    do
    {
        for(int i = 0; i < 256; i++)
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    // set AMUX on all SSAs to highZ
                    for(auto cReadoutChip: *cHybrid)
                    {
                        // add check for SSA
                        if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                        fReadoutChipInterface->WriteChipReg(cReadoutChip, "Threshold", i);
                        // fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        value = fReadoutChipInterface->ReadChipReg(cReadoutChip, "Threshold");
                        if(value == i) { LOG(DEBUG) << "Successful read" << RESET; }
                        else
                        {
                            LOG(INFO) << BOLDRED << "Failed read after " << total + 1 << "tries. Real value: " << i << "Read value: " << value << RESET;
                            bad++;
                            // TString parameter = Form("Bad I2C after ", total);
                            // parameter += Form("tries, for value", i);
                            // FillSummaryTree(parameter, value);
                        }
                        total++;
                    }
                } // hybrid
            }     // board
        }         // value
        LOG(INFO) << "Out of " << +total << " transactions, a total of " << RED << +bad << " failed." << RESET;
    } while(false);
    LOG(INFO) << "FINAL. Out of " << +total << " transactions, a total of " << RED << +bad << " failed." << RESET;
}
void PSHybridTester::CheckCounters(BeBoard* pBoard)
{
    int fEventsPerPoint = this->findValueInSettings<double>("Nevents");

    auto cSetting            = fSettingsMap.find("ShortsPulseAmplitude");
    auto fTestPulseAmplitude = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 0;
    // make sure that the correct trigger source is enabled
    // async injection trigger
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // make sure async mode is enabled
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "AnalogueAsync", 1);
    // first .. set injection amplitude to 0 and find pedestal
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "InjectedCharge", 0);

    // find pedestal
    float cOccTarget = 0.5;
    // this->bitWiseScan("Threshold", fEventsPerPoint, cOccTarget);
    float cMeanValue       = 0;
    int   cThresholdOffset = 5;
    int   cNchips          = 0;
    for(auto cOpticalGroupData: *pBoard) // for on opticalGroup - begin
    {
        for(auto cHybridData: *cOpticalGroupData) // for on module - begin
        {
            cNchips += cHybridData->size();
            for(auto cROCData: *cHybridData) // for on chip - begin
            {
                ReadoutChip* cChip = static_cast<ReadoutChip*>(fDetectorContainer->at(pBoard->getIndex())->at(cOpticalGroupData->getIndex())->at(cHybridData->getIndex())->at(cROCData->getIndex()));
                auto         cThreshold = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                // set threshold a little bit lower than 90% level
                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
            } // for on chip - end
        }     // for on module - end
    }         // for on opticalGroup - end
    LOG(INFO) << BOLDBLUE << "Mean Threshold at " << std::setprecision(2) << std::fixed << 100 * cOccTarget << " percent occupancy value " << cMeanValue / cNchips << RESET;

    // now configure injection amplitude to
    // whatever will be used for short finding
    // this is in the xml
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "InjectedCharge", boost::any_cast<int>(fTestPulseAmplitude) );

    // configure injection
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << RESET;

                // let's say .. only enable injection in even channels first
                for(uint8_t cChnl = 0; cChnl < cReadoutChip->size(); cChnl++)
                {
                    char    cRegName[100];
                    uint8_t cEnable = (uint8_t)(1);
                    std::sprintf(cRegName, "ENFLAGS_S%d", static_cast<int>(1 + cChnl));
                    auto    cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                    uint8_t cNewValue = (cRegValue & 0xF) | (cEnable << 4);
                    LOG(DEBUG) << BOLDBLUE << "\t\t..ENGLAG reg on channel#" << +cChnl << " is set to " << std::bitset<5>(cRegValue) << " want to set injection to : " << +cEnable
                               << " so new value would be " << std::bitset<5>(cNewValue) << RESET;
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cNewValue);
                }
            } // chip
        }     // hybrid
    }         // module

    int event_loop = 0;
    int bad_events = 0;
    while(event_loop < 1500)
    {
        this->ReadNEvents(pBoard, fEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents();
        // const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
        // iterate over FE objects and check occupancy
        for(auto cEvent: cEvents)
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    int cTotalCountInjectedChnls = 0;
                    // set AMUX on all SSAs to highZ
                    for(auto cReadoutChip: *cHybrid)
                    {
                        int cChipCountInjectedChnls = 0;
                        // add check for SSA
                        if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                        LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << RESET;
                        auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());
                        for(uint8_t cChnl = 0; cChnl < cReadoutChip->size(); cChnl++)
                        {
                            // LOG (DEBUG) << cHitVector[cChnl];
                            cChipCountInjectedChnls += cHitVector[cChnl];
                            cTotalCountInjectedChnls += cHitVector[cChnl];
                        } // chnl
                        if(cChipCountInjectedChnls == 0) LOG(INFO) << BOLDMAGENTA << "All injected channels on chip " << +cReadoutChip->getId() << " have 0 hits." << RESET;
                    } // chip

                    if(cTotalCountInjectedChnls == 0)
                    {
                        LOG(DEBUG) << BOLDRED << "All injected channels on all chips have 0 hits." << RESET;
                        LOG(INFO) << BOLDRED << "Event number " << +event_loop << " is \'empty\'." << RESET;
#ifdef __USE_ROOT__
                        fillSummaryTree("Empty event", event_loop);
#endif
                        bad_events++;
                    }
                    else
                        LOG(INFO) << BOLDGREEN << "Event number " << +event_loop
                                  << " is not \'empty\'. Occupancy is: " << ((float)(cTotalCountInjectedChnls * 100) / (fEventsPerPoint * 6 * cHybrid->at(0)->size())) << "%." << RESET;
                    LOG(DEBUG) << fEventsPerPoint * 6 * cHybrid->at(0)->size() << RESET;
                    LOG(DEBUG) << cTotalCountInjectedChnls << RESET;
                    event_loop++;
                } // hybrid
            }     // module
        }         // event loop
    }             // while
    LOG(INFO) << "Out of " << +event_loop << " readouts, " << +bad_events << " were \'empty\'" << RESET;
}

/*!
    Checks the hybrid and test card measurements using the TC USB library, and compares the measurement to the nominal value, allowing for a percentage of variation, defined in the settings file.
*/
void PSHybridTester::RunHybridETest()

{
#ifdef __TCUSB__
    TC_PSFE cTC_PSFE;
    float   result;

    double cAcceptancePercentage = this->findValueInSettings<double>("EMeasurementAcceptance") / 100;
    LOG(INFO) << "Running electrical test on the hybrid. Accepted deviation: +- " << +this->findValueInSettings<double>("EMeasurementAcceptance") << " %" << RESET;

    for(auto cMapIterator: fHybridVoltageMap)
    {
        auto  cNominalValue = fHybridNominalValues.find(cMapIterator.first);
        auto& cMeasurement  = cMapIterator.second;
        cTC_PSFE.adc_get(cMeasurement, result);
        LOG(INFO) << cMapIterator.first << " : " << result << RESET;
        std::string cMeasurementName = (cMapIterator.first);
#ifdef __USE_ROOT__
        fillSummaryTree(cMeasurementName, result);
#endif
        if(cNominalValue != fHybridNominalValues.end())
        {
            if(cNominalValue->second != 0 && cNominalValue->second != 1)
            {
#ifdef __USE_ROOT__
                fillSummaryTree(cMeasurementName + "dev", cNominalValue->second - result);
#endif
                if(cAcceptancePercentage != 0)
                {
                    if(result < cNominalValue->second * (1 + cAcceptancePercentage) && result > cNominalValue->second * (1 - cAcceptancePercentage)) { LOG(INFO) << BOLDGREEN << "OK" << RESET; }
                    else
                    {
                        LOG(INFO) << BOLDRED << "BAD" << RESET;
                    }
                }
            }
        }
    }

    for(auto cMapIterator: fHybridCurrentMap)
    {
        auto& cMeasurement = cMapIterator.second;
        cTC_PSFE.adc_get(cMeasurement, result);
        LOG(INFO) << cMapIterator.first << " : " << result << RESET;
#ifdef __USE_ROOT__
        fillSummaryTree(cMapIterator.first, result);
#endif

        if(cMapIterator.first == "Hybrid1V00_current" || cMapIterator.first == "Hybrid1V25_current")
        {
            if(result == 0)
            {
                LOG(ERROR) << BOLDRED << "Hybrid is not connected! Check the jumper cable between hybrid and test card" << RESET;
                exit(-6);
            }
        }
    }

    for(auto cMapIterator: fHybridOtherMap)
    {
        auto& cMeasurement = cMapIterator.second;
        cTC_PSFE.adc_get(cMeasurement, result);
        LOG(INFO) << cMapIterator.first << " : " << result << RESET;
#ifdef __USE_ROOT__
        fillSummaryTree(cMapIterator.first, result);
#endif
    }
#endif
}
void PSHybridTester::ReadHybridVoltage(const std::string& pVoltageName)
{
#ifdef __TCUSB__
    auto cMapIterator = fHybridVoltageMap.find(pVoltageName);
    if(cMapIterator != fHybridVoltageMap.end())
    {
        auto&              cMeasurement = cMapIterator->second;
        TC_PSFE            cTC_PSFE;
        std::vector<float> cMeasurements(fNreadings, 0.);
        for(int cIndex = 0; cIndex < fNreadings; cIndex++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
            cTC_PSFE.adc_get(cMeasurement, cMeasurements[cIndex]);
            LOG(INFO) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                      << " reading from test card  : " << cMeasurements[cIndex] << " mV." << RESET;
        }
        fVoltageMeasurement = this->getStats(cMeasurements);
    }
#endif
}
void PSHybridTester::ReadHybridCurrent(const std::string& pVoltageName)
{
#ifdef __TCUSB__
    // auto cMapIterator = fHybridCurrentMap.find(pVoltageName);
    // if( cMapIterator != fHybridCurrentMap.end() )
    // {
    //     auto& cMeasurement = cMapIterator->second;
    TC_PSFE            cTC_PSFE;
    std::vector<float> cMeasurements(fNreadings, 0.);
    for(int cIndex = 0; cIndex < fNreadings; cIndex++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
        cTC_PSFE.adc_get(TC_PSFE::measurement::ISEN_1V, cMeasurements[cIndex]);
        LOG(INFO) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                  << " reading from test card 1V  : " << cMeasurements[cIndex] << " mA." << RESET;
    }

    std::vector<float> cMeasurements1(fNreadings, 0.);
    for(int cIndex = 0; cIndex < fNreadings; cIndex++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
        cTC_PSFE.adc_get(TC_PSFE::measurement::ISEN_1V25, cMeasurements1[cIndex]);
        LOG(INFO) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                  << " reading from test card  1V25 : " << cMeasurements1[cIndex] << " mA." << RESET;
    }
    // fCurrentMeasurement = this->getStats(cMeasurements);
    //}
#endif
}
void PSHybridTester::CheckHybridCurrents()
{
    ReadHybridCurrent("Hybrid1V00");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 1V00 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;

    // ReadHybridCurrent("Hybrid1V25");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 1V25 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;

    // ReadHybridCurrent("Hybrid3V30");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 3V30 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;
}
void PSHybridTester::CheckHybridVoltages()
{
#ifdef __TCUSB__
#ifdef __USE_ROOT__
    ReadHybridVoltage("TC_GND");
    LOG(INFO) << BOLDBLUE << "Test card ground : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    fillSummaryTree("TestCardGroundavg", fVoltageMeasurement.first);
    fillSummaryTree("TestCardGroundrms", fVoltageMeasurement.second);

    ReadHybridVoltage("ROH_GND");
    LOG(INFO) << BOLDBLUE << "ROH connector ground : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    fillSummaryTree("PanasonicGroundavg", fVoltageMeasurement.first);
    fillSummaryTree("PanasonicGroundrms", fVoltageMeasurement.second);

    ReadHybridVoltage("Hybrid3V3");
    LOG(INFO) << BOLDBLUE << "Hybrid 3V30 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    fillSummaryTree("Hybrid3V3avg", fVoltageMeasurement.first);
    fillSummaryTree("Hybrid3V3rms", fVoltageMeasurement.second);

    ReadHybridVoltage("Hybrid1V00");
    LOG(INFO) << BOLDBLUE << "Hybrid 1V00 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    fillSummaryTree("Hybrid1V00avg", fVoltageMeasurement.first);
    fillSummaryTree("Hybrid1V00rms", fVoltageMeasurement.second);

    ReadHybridVoltage("Hybrid1V25");
    LOG(INFO) << BOLDBLUE << "Hybrid 1V25 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    fillSummaryTree("Hybrid1V25avg", fVoltageMeasurement.first);
    fillSummaryTree("Hybrid1V25rms", fVoltageMeasurement.second);

    if(fVoltageMeasurement.first * 1e-3 >= PSHYBRIDMAXV) { throw std::runtime_error(std::string("Exceeded maximum voltage of 1V25 of PS FEH")); }
#endif
#endif
}
void PSHybridTester::CalibrateSSABias(BeBoard* pBoard)
{
#ifdef __TCUSB__
    TC_PSFE cTC_PSFE;
    // now cycle through chips one at a time ..
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // First set the AMUX on every chip to HiZ to avoid shorts
            for(auto cReadoutChip: *cHybrid) fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1);
            std::this_thread::sleep_for(std::chrono::microseconds(50));

            for(auto cReadoutChip: *cHybrid)
            {
                LOG(INFO) << BOLDMAGENTA << "----------------------------------------------------- Calibrating bias DACs on SSA #" << +cReadoutChip->getId()
                          << "-----------------------------------------------------" << RESET;

                // Measure GND on the chip
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "GND", 1);
                float ground;
                cTC_PSFE.adc_get(TC_PSFE::measurement::AMUX, ground);

                float result;

                // Iterate over the bias DACs that need to be calibrated.
                for(auto cDAC: fDACsCalibrationMap)
                {
                    std::string cAdjustmentRegister = cDAC.second;
                    auto        cTargetIterator     = fDACsCalibrationTargetMap.find(cDAC.first);
                    float       cAdjustmentTarget   = cTargetIterator->second;

                    LOG(INFO) << BOLDMAGENTA << "Setting " << cDAC.first << "." << RESET;

                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cDAC.first, 1);
                    std::this_thread::sleep_for(std::chrono::microseconds(50));
                    cTC_PSFE.adc_get(TC_PSFE::measurement::AMUX, result);
                    LOG(INFO) << BOLDBLUE << "Value before calibrating " << result - ground << "mV. Target value: " << cAdjustmentTarget << "mV." << RESET;
                    bool cCalibrated = false;
                    int  iterations  = 0;
                    while(!cCalibrated && iterations < 25)
                    {
                        if(result - ground > cAdjustmentTarget + 1)
                        { fReadoutChipInterface->WriteChipReg(cReadoutChip, cAdjustmentRegister, fReadoutChipInterface->ReadChipReg(cReadoutChip, cAdjustmentRegister) - 1); }
                        else if(result - ground < cAdjustmentTarget - 1)
                        {
                            fReadoutChipInterface->WriteChipReg(cReadoutChip, cAdjustmentRegister, fReadoutChipInterface->ReadChipReg(cReadoutChip, cAdjustmentRegister) + 1);
                        }
                        else if(result - ground >= cAdjustmentTarget - 1.1 && result - ground <= cAdjustmentTarget + 1.1)
                        {
                            cCalibrated = true;
                        }
                        std::this_thread::sleep_for(std::chrono::microseconds(50));
                        cTC_PSFE.adc_get(TC_PSFE::measurement::AMUX, result);
                        iterations++;
                    }
                    if(!cCalibrated) fReadoutChipInterface->WriteChipReg(cReadoutChip, cAdjustmentRegister, 15);
                    // for(int cValue = 0; cValue < 32; cValue++)
                    // {
                    //     fReadoutChipInterface->WriteChipReg(cReadoutChip, cAdjustmentRegister, cValue);
                    //     std::this_thread::sleep_for(std::chrono::microseconds(50));
                    //     cTC_PSFE.adc_get(TC_PSFE::measurement::AMUX, result);
                    //     if( cAdjustmentTarget - 1 < (result-ground) && (result-ground) < cAdjustmentTarget + 1 )
                    //         break;
                    // }
                    LOG(INFO) << BOLDGREEN << "Value after calibrating " << result - ground << "mV. Target value: " << cAdjustmentTarget << "mV." << RESET;
                }

                fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1); // Set the AMUX to Hiz again.

                // fReadoutChipInterface->WriteChipReg(cReadoutChip, "GAINTRIMMING_S15", 2);
            }
        }
    }
#endif
}
void PSHybridTester::ReadSSABias(BeBoard* pBoard, const std::string& pBiasName)
{
#ifdef __TCUSB__
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1);

                break;
            }

            ReadHybridVoltage("ADC");
            LOG(INFO) << BOLDBLUE << "[AmuxHigh] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

            // then select bias
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                LOG(INFO) << BOLDMAGENTA << "Selecting TestBias on SSA " << +cReadoutChip->getId() << RESET;
                // select bias
                fReadoutChipInterface->WriteChipReg(cReadoutChip, pBiasName, 1);

                std::this_thread::sleep_for(std::chrono::microseconds(50));
                ReadHybridVoltage("ADC");
                LOG(INFO) << BOLDBLUE << "[ " << pBiasName << " ] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

                // back to high Z
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1);
                std::this_thread::sleep_for(std::chrono::microseconds(50));
                ReadHybridVoltage("ADC");
                LOG(DEBUG) << BOLDBLUE << "[AmuxHigh] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;
            }
        } // hybrid
    }     // board
#else
    LOG(ERROR) << BOLDRED << "Can't read SSA bias value, the TC USB library is not built. Check the installation." << RESET;
#endif
}
void PSHybridTester::CalibrateGainTrim(BeBoard* pBoard)
{
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // for( int i = 0; i < 16; i++)
            // {
            //     LOG(INFO) << BOLDMAGENTA << "Value: " << +i << RESET;
            //     for(auto cReadoutChip: *cHybrid)
            //     {
            //         for(uint32_t channel=0; channel < cReadoutChip->size(); channel++)
            //         {
            //             std::string cRegName = Form("GAINTRIMMING_S%d", channel+1);
            //             int cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
            //             if (cRegValue+i < 16)
            //                 fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue+i);
            //         }
            //     }
            //     this->ReadNEvents(pBoard, 1000);
            //     const std::vector<Event *> &cEvents = this->GetEvents(pBoard);
            //     for (auto cEvent : cEvents)
            //     {
            //         for(auto cReadoutChip: *cHybrid)
            //         {
            //             auto cNhits = cEvent->GetNHits(cHybrid->getId(), cReadoutChip->getId());
            //             auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());
            //             uint32_t max_value = 0;
            //             uint32_t min_value = 0;
            //             double avg_value = 0;
            //             for (uint32_t iChannel = 0; iChannel < cReadoutChip->size(); ++iChannel)
            //             {
            //                 avg_value += cHitVector[iChannel];
            //                 if( max_value < cHitVector[iChannel] )
            //                     max_value = cHitVector[iChannel];
            //                 if( min_value > cHitVector[iChannel] )
            //                     min_value = cHitVector[iChannel];
            //             } //chnl
            //             LOG(INFO) << "Max value is: " << max_value << " , min value is " << min_value << " and avg is " << (avg_value/cReadoutChip->size()) << RESET;
            //         }
            //     }
            // }

            for(auto cReadoutChip: *cHybrid)
            {
                for(auto cReadoutChip: *cHybrid)
                {
                    for(uint32_t channel = 0; channel < cReadoutChip->size(); channel++)
                    {
                        std::string cRegName  = Form("GAINTRIMMING_S%d", channel + 1);
                        int         cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 7);
                    }
                }

                // fReadoutChipInterface->WriteChipReg(cReadoutChip, "AnalogueAsync", 1);
                // fReadoutChipInterface->WriteChipReg(cReadoutChip, "Threshold", 7);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "InjectedCharge", 100);
                fChannelGroupHandler = new SSAChannelGroupHandler();
                fChannelGroupHandler->setChannelGroupParameters(16, 2);
                this->bitWiseScan("Bias_THDAC", 1000, 0.56, -1);
                int cThresholdValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, "Bias_THDAC");
                ReadSSABias("CalLevel");

                // int currentThreshold = 0;
                // int previousThreshold = 0;
                for(uint32_t channel = 0; channel < cReadoutChip->size(); channel++)
                {
                    bool cGainCalibrated = false;
                    while(!cGainCalibrated)
                    {
                        // for (int i = 0; i < 256; i++)
                        // {
                        // fReadoutChipInterface->WriteChipReg(cReadoutChip, "Threshold", i);
                        this->ReadNEvents(pBoard, 1000);
                        const std::vector<Event*>& cEvents = this->GetEvents();
                        // const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
                        for(auto cEvent: cEvents)
                        {
                            auto cNhits     = cEvent->GetNHits(cHybrid->getId(), cReadoutChip->getId());
                            auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());
                            // uint32_t max_value = 0;
                            // uint32_t min_value = 1000;
                            // double avg_value = 0;
                            // double stdev_aux = 0;

                            LOG(INFO) << "Threshold: " << cThresholdValue << " Occupancy: " << cHitVector[channel];

                            std::string cRegName  = Form("GAINTRIMMING_S%d", channel + 1);
                            int         cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);

                            if(cHitVector[channel] / 1000 < 0.55)
                            {
                                if(cRegValue - 1 >= 0)
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue - 1);
                                else
                                {
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 0);
                                    cGainCalibrated = true;
                                }
                            }
                            else if(cHitVector[channel] / 1000 > 0.57)
                            {
                                if(cRegValue + 1 < 16)
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue + 1);
                                else
                                {
                                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                                    cGainCalibrated = true;
                                }
                            }
                            else
                                cGainCalibrated = true;
                        }
                        // }

                        // if(channel !=0 )
                        // {
                        //     if ( currentThreshold < previousThreshold )
                        //     {
                        //         std::string cRegName = Form("GAINTRIMMING_S%d", channel+1);
                        //         int cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        //         if (cRegValue-1 >= 0)
                        //                 fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue-1);
                        //             else
                        //                 fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 0);
                        //     }
                        //     else if ( currentThreshold > previousThreshold )
                        //     {
                        //         std::string cRegName = Form("GAINTRIMMING_S%d", channel+1);
                        //         int cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        //         if (cRegValue+1 < 16)
                        //                 fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue+1);
                        //             else
                        //                 fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                        //     }
                        // }
                        // previousThreshold = currentThreshold;
                        // for (uint32_t iChannel = 0; iChannel < cReadoutChip->size(); ++iChannel)
                        // {
                        //     avg_value += cHitVector[iChannel];
                        //     if( max_value < cHitVector[iChannel] )
                        //         max_value = cHitVector[iChannel];
                        //     if( min_value > cHitVector[iChannel] )
                        //         min_value = cHitVector[iChannel];
                        // } //chnl
                        // LOG(INFO) << "InjectedCharge: " << i*10+5 << ". Max value is: " << max_value << " , min value is " << min_value << " and avg is " << (avg_value/cReadoutChip->size()) <<
                        // RESET;
                        // // avg_value = avg_value/cReadoutChip->size();
                        // for (uint32_t iChannel = 0; iChannel < cReadoutChip->size(); ++iChannel)
                        // {
                        //     stdev_aux += (cHitVector[iChannel] - (avg_value/cReadoutChip->size()))*(cHitVector[iChannel] - (avg_value/cReadoutChip->size()));
                        // } //chnl

                        // Double_t stdev = sqrt((double)stdev_aux);

                        // LOG(INFO) << BOLDMAGENTA << "stdev: " << +stdev << RESET;

                        // cGainCalibrated = true;

                        // int badch = 0;
                        // for (uint32_t iChannel = 0; iChannel < cReadoutChip->size(); ++iChannel)
                        // {
                        //     // LOG(INFO) << "Channel " << +iChannel << ": " << cHitVector[iChannel] << RESET;
                        //     std::string cRegName = Form("GAINTRIMMING_S%d", iChannel+1);
                        //     int cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        //     // LOG(INFO) << "GainTrim " << +iChannel << ": " << cRegValue << RESET;

                        //     // if( cHitVector[iChannel] < (avg_value/cReadoutChip->size())-50) {
                        //     //     if (cRegValue-1 >= 0)
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue-1);
                        //     //     else
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 0);
                        //     //     cGainCalibrated = false;
                        //     //     badch++;
                        //     // }
                        //     // else if( cHitVector[iChannel] > (avg_value/cReadoutChip->size())+50) {
                        //     //     if (cRegValue+1 < 16)
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue+1);
                        //     //     else
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                        //     //     cGainCalibrated = false;
                        //     //     badch++;
                        //     // }
                        //     if( cHitVector[iChannel] < max_value ) {
                        //     //     if (cRegValue+1 < 16)
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue+1);
                        //     //     else
                        //     //         fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                        //     //     cGainCalibrated = false;
                        //     //     badch++;
                        //         if (cRegValue+1 < 16)
                        //             fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cRegValue+1);
                        //         else
                        //             fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, 15);
                        //         cGainCalibrated = false;
                        //         badch++;
                        //     }
                        // } //chnl

                        // LOG(INFO) << +badch << RESET;

                        // LOG(INFO) << "Max value is: " << max_value << " , min value is " << min_value << " and avg is " << (avg_value/cReadoutChip->size()) << RESET;
                    }
                }
            }
        }
    }
}
void PSHybridTester::CheckFastCommands(BeBoard* pBoard, const std::string& pFastCommand, uint8_t pDuration)
{
    LOG(DEBUG) << BOLDBLUE << "Sending " << pFastCommand << RESET;
    if(pFastCommand == "ReSync") { this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 16) | (pDuration << 28)); }
    else if(pFastCommand == "Trigger" || pFastCommand == "OpenShutter")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 18) | (pDuration << 28));
    }
    else if(pFastCommand == "TestPulse")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 17) | (pDuration << 28));
    }
    else if(pFastCommand == "BC0" || pFastCommand == "CloseShutter")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (pDuration << 28));
    }
    else if(pFastCommand == "ReSync&BC0" || pFastCommand == "StartReadout")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 16) | (pDuration << 28));
    }
    else if(pFastCommand == "Trigger&BC0" || pFastCommand == "ClearCounters")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 18) | (pDuration << 28));
    }
    else if(pFastCommand == "TestPulse&BC0")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 17) | (pDuration << 28));
    }
}
void PSHybridTester::CheckHybridInputs(BeBoard* pBoard, std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    uint32_t             cRegisterValue = 0;
    std::vector<uint8_t> cIndices(0);
    for(auto cInput: pInputs)
    {
        auto cMapIterator = fInputDebugMap.find(cInput);
        if(cMapIterator != fInputDebugMap.end())
        {
            auto& cIndex   = cMapIterator->second;
            cRegisterValue = cRegisterValue | (1 << cIndex);
            cIndices.push_back(cIndex);
        }
    }
    // select input lines
    LOG(INFO) << BOLDBLUE << "Configuring debug register : " << std::bitset<32>(cRegisterValue) << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_input", cRegisterValue);
    // start
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_input", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // stop
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    // check counters
    pCounters.clear();
    pCounters.resize(cIndices.size());
    for(auto cIndex: cIndices)
    {
        char cBuffer[20];
        sprintf(cBuffer, "debug_blk_counter%02d", (cIndex & 0x63)); // max value can be 99, to avoid GCC 8 warning
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}
void PSHybridTester::CheckHybridInputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckHybridInputs(cBoard, pInputs, pCounters); }
}

void PSHybridTester::SetTrim(BeBoard* pBoard, std::string pTrimRegister, uint16_t pTrimValue)
{
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                for(uint cChannel = 0; cChannel < cReadoutChip->size(); cChannel++)
                {
                    std::string cRegName = Form("%s_S%d", pTrimRegister.c_str(), cChannel + 1);
                    fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, pTrimValue);
                }
            }
        }
    }
}

void PSHybridTester::CheckHybridOutputs(BeBoard* pBoard, std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters)
{
    uint32_t             cRegisterValue = 0;
    std::vector<uint8_t> cIndices(0);
    for(auto cInput: pOutputs)
    {
        auto cMapIterator = fOutputDebugMap.find(cInput);
        if(cMapIterator != fOutputDebugMap.end())
        {
            auto& cIndex   = cMapIterator->second;
            cRegisterValue = cRegisterValue | (1 << cIndex);
            cIndices.push_back(cIndex);
        }
    }
    // select input lines
    LOG(INFO) << BOLDBLUE << "Configuring debug register : " << std::bitset<32>(cRegisterValue) << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_output", cRegisterValue);
    // start
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_output", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // stop
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_output", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // check counters
    pCounters.clear();
    pCounters.resize(cIndices.size());
    for(auto cIndex: cIndices)
    {
        char cBuffer[20];
        sprintf(cBuffer, "debug_blk_counter%02d", (cIndex & 0x63)); // max value can be 99, to avoid GCC 8 warning
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}
void PSHybridTester::ReadSSABias(const std::string& pBiasName)
{
#ifdef __TCUSB__
    LOG(DEBUG) << BOLDBLUE << "TC USB built." << RESET;
    for(auto cBoard: *fDetectorContainer) { this->ReadSSABias(cBoard, pBiasName); }
#else
    LOG(INFO) << BOLDRED << "TC USB not built .. check that you have the lib installed!" << RESET;
#endif
}

void PSHybridTester::CalibrateSSABias()
{
#ifdef __TCUSB__
    LOG(DEBUG) << BOLDBLUE << "TC USB built." << RESET;
    for(auto cBoard: *fDetectorContainer) { this->CalibrateSSABias(cBoard); }
#else
    LOG(INFO) << BOLDRED << "TC USB not built .. check that you have the lib installed!" << RESET;
#endif
}

void PSHybridTester::CalibrateGainTrim()
{
    for(auto cBoard: *fDetectorContainer) { this->CalibrateGainTrim(cBoard); }
}

void PSHybridTester::SetTrim(std::string pTrimRegister, uint16_t pTrimValue)
{
    for(auto cBoard: *fDetectorContainer) { this->SetTrim(cBoard, pTrimRegister, pTrimValue); }
}

void PSHybridTester::CheckHybridOutputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckHybridOutputs(cBoard, pInputs, pCounters); }
}
void PSHybridTester::CheckFastCommands(const std::string& pFastCommand, uint8_t pDuartion)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckFastCommands(cBoard, pFastCommand, pDuartion); }
}
// State machine control functions
void PSHybridTester::Running()
{
    LOG(INFO) << BOLDBLUE << "Starting PS Hybrid tester" << RESET;
    Initialise();
}

void PSHybridTester::Stop()
{
    LOG(INFO) << BOLDBLUE << "Stopping PS Hybrid tester" << RESET;
    // writeObjects();
    dumpConfigFiles();
    Destroy();
}

void PSHybridTester::Pause() {}

void PSHybridTester::Resume() {}
