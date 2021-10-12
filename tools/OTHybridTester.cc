#include "OTHybridTester.h"
#include "linearFitter.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#ifdef __USE_ROOT__

OTHybridTester::OTHybridTester() : Tool() {}

OTHybridTester::~OTHybridTester()
{
#ifdef __TCUSB__
#ifdef __TCP_SERVER__
#else
    if(fTC_USB != nullptr) delete fTC_USB;
#endif
#endif
}

void OTHybridTester::FindUSBHandler()
{
#ifdef __TCUSB__
    bool cThereIsLpGBT = false;
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr)
        {
            LOG(DEBUG) << BOLDYELLOW << "Found lpGBT" << RESET;
            cThereIsLpGBT = true;
        }
        else
        {
            LOG(DEBUG) << BOLDYELLOW << "Did not find lpGBT" << RESET;
            cThereIsLpGBT = false;
        }
    }
    if(!cThereIsLpGBT)
    {
#ifdef __ROH_USB__
        fTC_USB = new TC_PSROH();
#elif __SEH_USB__
#ifdef __TCP_SERVER__
        if(fTestcardClient == nullptr)
        {
            LOG(ERROR) << BOLDRED << "Not connected to the test card! Test cannot be executed" << RESET;
            throw std::runtime_error("Test card cannot be reached");
        }
#else
        fTC_USB = new TC_2SSEH();
#endif
#endif
    }

#ifdef __TCP_SERVER__
#else
    else
        fTC_USB = static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetTCUSBHandler();
#endif
#endif
}

void OTHybridTester::LpGBTInjectULInternalPattern(uint32_t pPattern)
{
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            clpGBTInterface->ConfigureRxPRBS(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, {0, 2}, false);
            LOG(INFO) << BOLDGREEN << "Internal LpGBT pattern generation" << RESET;
            clpGBTInterface->ConfigureRxSource(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, 4);
            clpGBTInterface->ConfigureDPPattern(cOpticalGroup->flpGBT, pPattern);
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }
}

void OTHybridTester::LpGBTInjectULExternalPattern(bool pStart, uint8_t pPattern)
{
    DPInterface cDPInterfacer;
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        BeBoardFWInterface* pInterface = dynamic_cast<BeBoardFWInterface*>(fBeBoardFWMap.find(cBoard->getId())->second);
        if(pStart)
        {
            LOG(INFO) << BOLDGREEN << "Electrical FC7 pattern generation" << RESET;
            // Check if Emulator is running
            for(int i = 0; i < 5; i++)
            {
                if(cDPInterfacer.IsRunning(pInterface, 1))
                {
                    LOG(INFO) << BOLDYELLOW << " STATUS : Data Player is running and will be stopped " << RESET;
                    cDPInterfacer.Stop(pInterface);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // Configure and Start DataPlayer
                cDPInterfacer.Configure(pInterface, pPattern);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                cDPInterfacer.Start(pInterface, 1);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                if(cDPInterfacer.IsRunning(pInterface, 1))
                {
                    LOG(INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET;
                    break;
                }
                else
                    LOG(INFO) << BOLDRED << "Could not start FE data player" << RESET;
            }
        }
        else
        {
            LOG(INFO) << BOLDYELLOW << " Data Player will be stopped " << RESET;
            cDPInterfacer.Stop(pInterface);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        return;
    }
}

bool OTHybridTester::LpGBTCheckULPattern(bool pIsExternal, uint8_t pPattern)
{
    uint8_t  cMatch;
    uint8_t  cShift;
    uint8_t  cWrappedByte;
    uint32_t cWrappedData;
    LOG(INFO) << BOLDGREEN << "Checking against : " << std::bitset<8>(pPattern) << RESET;
    bool                res             = true;
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            for(int hybridNumber = 0; hybridNumber < 2; hybridNumber++)
            {
                if(pIsExternal)
                {
                    clpGBTInterface->ConfigureRxPRBS(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, {0, 2}, false);
                    clpGBTInterface->ConfigureRxSource(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, 0);
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                fBeBoardInterface->setBoard(cBoard->getId());
                D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

                cFWInterface->selectLink(cOpticalGroup->getId());
                LOG(INFO) << BOLDBLUE << "Stub lines " << RESET;
                // cFWInterface->StubDebug(true, 6);
                // enable stub debug - allows you to 'scope' the stub output

                cFWInterface->WriteReg("fc7_daq_cnfg.stub_debug.enable", 0x01);
                cFWInterface->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", hybridNumber);
                cFWInterface->ChipTestPulse();
                auto                     cWords = cFWInterface->ReadBlockReg("fc7_daq_stat.physical_interface_block.stub_debug", 80);
                std::vector<std::string> cLines(0);
                size_t                   cLine = 0;
                do
                {
                    uint32_t cCicOutOutput = cWords[cLine * 10];
                    LOG(INFO) << BOLDBLUE << "Scoped output on Stub Line " << BOLDGREEN << +cLine << BOLDBLUE << ": " << std::bitset<32>(cCicOutOutput) << " for hybrid side " << +hybridNumber
                              << RESET;

                    cMatch = 32;
                    cShift = 0;
                    for(uint8_t shift = 0; shift < 8; shift++)
                    {
                        cWrappedByte = (pPattern >> shift) | (pPattern << (8 - shift));
                        cWrappedData = (cWrappedByte << 24) | (cWrappedByte << 16) | (cWrappedByte << 8) | (cWrappedByte << 0);
                        LOG(DEBUG) << BOLDBLUE << std::bitset<8>(cWrappedByte) << RESET;
                        LOG(DEBUG) << BOLDBLUE << std::bitset<32>(cWrappedData) << RESET;
                        int popcount = __builtin_popcountll(cWrappedData ^ cCicOutOutput);
                        if(popcount < cMatch)
                        {
                            cMatch = popcount;
                            cShift = shift;
                        }
                        LOG(DEBUG) << BOLDBLUE << "Line " << +cLine << " Shift " << +shift << " Match " << +popcount << RESET;
                    }
                    LOG(INFO) << BOLDBLUE << "Found for stub line " << BOLDWHITE << +cLine << BOLDBLUE << " a minimal bit difference of " << BOLDWHITE << +cMatch << BOLDBLUE << " for a bit shift of "
                              << BOLDWHITE << +cShift << RESET;

#ifdef __USE_ROOT__
                    fillSummaryTree(Form("stub_%d_hybrid_%d_match", int(cLine), hybridNumber), cMatch);
                    fillSummaryTree(Form("stub_%d_hybrid_%d_shift", int(cLine), hybridNumber), cShift);
#endif
                    if((cMatch == 0)) { LOG(INFO) << BOLDGREEN << "CIC Out Test passed for stub line " << +cLine << " for hybrid side " << +hybridNumber << RESET; }
                    else
                    {
                        LOG(INFO) << BOLDRED << "CIC Out Test failed for stub line " << +cLine << " for hybrid side " << +hybridNumber << RESET;
                        res = false;
                    }

                    /* std::vector<std::string> cOutputWords(0);
                    for(size_t cIndex = 0; cIndex < 5; cIndex++)
                    {
                    auto cWord   = cWords[cLine * 10 + cIndex];
                        LOG(INFO) << "cLine: " <<+cLine<< " cIndex: " << +cIndex <<" cWord "<< std::bitset<32>(cWord)<< RESET;
                        auto cString = std::bitset<32>(cWord).to_string();
                        for(size_t cOffset = 0; cOffset < 4; cOffset++) { cOutputWords.push_back(cString.substr(cOffset * 8, 8)); }
                    }

                    std::string cOutput_wSpace = "";
                    std::string cOutput        = "";
                    for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
                    {
                        cOutput_wSpace += *cIt + " ";
                        cOutput += *cIt;
                    }
                    LOG(INFO) << BOLDBLUE << "Line " << +cLine << " : " << cOutput_wSpace << RESET;
                    cLines.push_back(cOutput); */
                    // cStrLength = cOutput.length();
                    cLine++;
#ifdef __SEH_USB__
                } while(cLine < 5); // makeing sure missing stub line pair is skipped in 2S case
#else
                } while(cLine < 6);
#endif

                // disable stub debug
                cFWInterface->WriteReg("fc7_daq_cnfg.stub_debug.enable", 0x00);
                cFWInterface->ResetReadout();

                LOG(INFO) << BOLDBLUE << "L1 data " << RESET;
                // cFWInterface->L1ADebug();
                // cFWInterface->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", 0xff);
                cFWInterface->ConfigureTriggerFSM(0, 10, 3);

                // disable back-pressure
                cFWInterface->WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0);
                cFWInterface->Start();
                std::this_thread::sleep_for(std::chrono::microseconds(1 * 1000));
                cFWInterface->Stop();

                auto     cWordsL1A        = cFWInterface->ReadBlockReg("fc7_daq_stat.physical_interface_block.l1a_debug", 50);
                uint32_t cCicOutOutputL1A = cWordsL1A[0];
                LOG(INFO) << BOLDBLUE << "Scoped output on L1A Line: " << std::bitset<32>(cCicOutOutputL1A) << " for hybrid side " << +hybridNumber << RESET;

                cMatch = 32;
                cShift = 0;
                for(uint8_t shift = 0; shift < 8; shift++)
                {
                    cWrappedByte = (pPattern >> shift) | (pPattern << (8 - shift));
                    cWrappedData = (cWrappedByte << 24) | (cWrappedByte << 16) | (cWrappedByte << 8) | (cWrappedByte << 0);
                    LOG(DEBUG) << BOLDBLUE << std::bitset<8>(cWrappedByte) << RESET;
                    LOG(DEBUG) << BOLDBLUE << std::bitset<32>(cWrappedData) << RESET;
                    int popcount = __builtin_popcountll(cWrappedData ^ cCicOutOutputL1A);
                    if(popcount < cMatch)
                    {
                        cMatch = popcount;
                        cShift = shift;
                    }
                    LOG(DEBUG) << BOLDBLUE << "Line L1A Shift " << +shift << " Match " << +popcount << RESET;
                }
                LOG(INFO) << BOLDBLUE << "Found for L1A a minimal bit difference of " << BOLDWHITE << +cMatch << BOLDBLUE << " for a bit shift of " << BOLDWHITE << +cShift << RESET;
                if((cMatch == 0))
                {
                    LOG(INFO) << BOLDGREEN << "CIC Out Test passed for L1A line"
                              << " for hybrid side " << +hybridNumber << RESET;
                }
                else
                {
                    LOG(INFO) << BOLDRED << "CIC Out Test failed for L1A line"
                              << " for hybrid side " << +hybridNumber << RESET;
                    res = false;
                }
#ifdef __USE_ROOT__
                fillSummaryTree(Form("L1A_hybrid_%d_match", hybridNumber), cMatch);
                fillSummaryTree(Form("L1A_hybrid_%d_shift", hybridNumber), cShift);
#endif
                uint32_t cL1ATotalWrong = 0;
                uint32_t cL1ATotal      = 0;
                cWrappedByte            = (pPattern >> cShift) | (pPattern << (8 - cShift));
                cWrappedData            = (cWrappedByte << 24) | (cWrappedByte << 16) | (cWrappedByte << 8) | (cWrappedByte << 0);
                for(uint32_t cWord: cWordsL1A)
                {
                    cL1ATotalWrong += __builtin_popcountll(cWrappedData ^ cWord);
                    cL1ATotal += 32;
                }
                LOG(INFO) << "L1A total wrong bits: " << BOLDBLUE << +cL1ATotalWrong << " in a total of: " << +cL1ATotal << RESET;

                cFWInterface->ResetReadout();
            }
        }
    }
    return res;
}

void OTHybridTester::LpGBTInjectDLInternalPattern(uint8_t pPattern)
{
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            uint8_t cSource = 3;
            clpGBTInterface->ConfigureDPPattern(cOpticalGroup->flpGBT, pPattern << 24 | pPattern << 16 | pPattern << 8 | pPattern);
            clpGBTInterface->ConfigureTxSource(cOpticalGroup->flpGBT, {0, 1, 2, 3}, cSource); // 0 --> link data, 3 --> constant pattern
        }
    }
}

bool OTHybridTester::LpGBTTestI2CMaster(const std::vector<uint8_t>& pMasters)
{
    bool                cTestSuccess    = true;
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            clpGBTInterface->ResetI2C(cOpticalGroup->flpGBT, {0, 1, 2});
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            for(const auto cMaster: pMasters)
            {
                uint8_t cSlaveAddress = 0x60;

                uint8_t cSuccess = clpGBTInterface->WriteI2C(cOpticalGroup->flpGBT, cMaster, cSlaveAddress, 0x0901, 2);
                cSuccess         = clpGBTInterface->WriteI2C(cOpticalGroup->flpGBT, cMaster, cSlaveAddress, 0x9, 1);
                if(cSuccess)
                    LOG(INFO) << BOLDGREEN << "I2C Master " << +cMaster << " PASSED" << RESET;
                else
                    LOG(INFO) << BOLDRED << "I2C Master " << +cMaster << " FAILED" << RESET;
                cTestSuccess &= cSuccess;
                fillSummaryTree(Form("i2cmaster%i", cMaster), cSuccess);
            }
        }
    }
    return cTestSuccess;
}

void OTHybridTester::LpGBTTestADC(const std::vector<std::string>& pADCs, uint32_t pMinDACValue, uint32_t pMaxDACValue, uint32_t pStep)
{
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    int                 cTrim           = -1;
#ifdef __USE_ROOT__
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            // Create TTree for DAC to ADC conversion in lpGBT
            auto cDACtoADCTree = new TTree("tDACtoADC", "DAC to ADC conversion in lpGBT");
            // Create variables for TTree branches
            int              cADCId = -1;
            std::vector<int> cDACValVect;
            std::vector<int> cADCValVect;
            // Create TTree Branches
            cDACtoADCTree->Branch("Id", &cADCId);
            cDACtoADCTree->Branch("DAC", &cDACValVect);
            cDACtoADCTree->Branch("ADC", &cADCValVect);

            // Create TCanvas & TMultiGraph
            auto cDACtoADCCanvas = new TCanvas("cDACtoADC", "DAC to ADC conversion", 500, 500);
            auto cObj            = gROOT->FindObject("mgDACtoADC");
            if(cObj) delete cObj;
            auto cDACtoADCMultiGraph = new TMultiGraph();
            cDACtoADCMultiGraph->SetName("mgDACtoADC");
            cDACtoADCMultiGraph->SetTitle("lpGBT - DAC to ADC conversion");
            auto dieLegende = new TLegend(0.1, 0.7, 0.48, 0.9);

            LOG(INFO) << BOLDMAGENTA << "Testing ADC channels" << RESET;

            fitter::Linear_Regression<int> cReg_Class;
            std::vector<std::vector<int>>  cfitDataVect(2);
            for(const auto& cADC: pADCs)
            {
                cDACValVect.clear(), cADCValVect.clear();
                cfitDataVect.clear();
                // uint32_t cNValues = (cMaxDAC-cMinDAC)/cStep;
                cADCId = cADC[3] - '0';
                for(int cDACValue = pMinDACValue; cDACValue <= (int)pMaxDACValue; cDACValue += pStep)
                {
#ifdef __TCUSB__

// Need to confirm conversion factor for 2S-SEH
// fTC_2SSEH->set_AMUX(cDACValue, cDACValue);
// example to program current Dac for temperature sensor clpGBTInterface->ConfigureCurrentDAC(cOpticalGroup->flpGBT, pADCs,0);
#ifdef __ROH_USB__
                    fTC_USB->dac_output(cDACValue);
#elif __SEH_USB__
#ifdef __TCP_SERVER__
                    fTestcardClient->sendAndReceivePacket("set_AMUX,rightValue:" + std::to_string(cDACValue) + ",leftValue:" + std::to_string(cDACValue) + ",");
#else
                    fTC_USB->set_AMUX(cDACValue, cDACValue);
                    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
#endif
#endif
#endif
                    int cADCValue = clpGBTInterface->ReadADC(cOpticalGroup->flpGBT, cADC);

                    LOG(INFO) << BOLDBLUE << "DAC value = " << +cDACValue << " --- ADC value = " << +cADCValue << RESET;
                    cDACValVect.push_back(cDACValue);
                    cADCValVect.push_back(cADCValue);
                }
                cDACtoADCTree->Fill();
                auto cDACtoADCGraph = new TGraph(cDACValVect.size(), cDACValVect.data(), cADCValVect.data());
                cDACtoADCGraph->SetName(Form("gADC%i", cADCId));
                cDACtoADCGraph->SetTitle(Form("ADC%i", cADCId));
                cDACtoADCGraph->SetLineColor(cADCId + 1);
                cDACtoADCGraph->SetFillColor(0);
                cDACtoADCGraph->SetLineWidth(3);
                cDACtoADCMultiGraph->Add(cDACtoADCGraph);
                cfitDataVect[0] = cDACValVect;
                cfitDataVect[1] = cADCValVect;
                cReg_Class.fit(cDACValVect, cADCValVect);
                cDACtoADCGraph->Fit("pol1");
                cDACtoADCGraph->GetFunction("pol1")->SetLineColor(cADCId + 2);

                // TF1* cFit = (TF1*)cDACtoADCGraph->GetListOfFunctions()->FindObject("pol1");
                TF1* cFit = cDACtoADCGraph->GetFunction("pol1");
                dieLegende->AddEntry(cDACtoADCGraph);
                dieLegende->AddEntry(cFit, Form("Fit ADC%i", cADCId), "lpf");
                // LOG(INFO) << BOLDBLUE << "Using ROOT for ADC " << cADCId << ": Parameter 1  " << cFit->GetParameter(0) << "  Parameter 2   " << cFit->GetParameter(1) << RESET;
                // LOG(INFO) << BOLDBLUE << "Using custom class for ADC " << cADCId << ": Parameter 1  " << cReg_Class.b_0 << "  Parameter 2   " << cReg_Class.b_1 << RESET;
                LOG(INFO) << BOLDBLUE << "Using custom class for ADC " << cADCId << ": Parameter 1  " << cReg_Class.b_0 << " +/- " << cReg_Class.b_0_error << "  Parameter 2   " << cReg_Class.b_1
                          << " +/- " << cReg_Class.b_1_error << RESET;
                LOG(INFO) << BOLDBLUE << "Using ROOT for ADC " << cADCId << ": Parameter 1  " << cFit->GetParameter(0) << " +/- " << cFit->GetParError(0) << "  Parameter 2   " << cFit->GetParameter(1)
                          << " +/- " << cFit->GetParError(1) << " Chi^2 " << cFit->GetChisquare() << " NDF " << cFit->GetNDF() << RESET;
                cTrim = clpGBTInterface->ReadChipReg(cOpticalGroup->flpGBT, "VREFCNTR");
                LOG(INFO) << BOLDBLUE << "Trim value " << cTrim << RESET;
                // ---Information also included in ROOT file of the fit
                fillSummaryTree(Form("ADC%i_p0", cADCId), cReg_Class.b_0);
                fillSummaryTree(Form("ADC%i_p1", cADCId), cReg_Class.b_1);
                fillSummaryTree(Form("ADC%i_p0_sigma", cADCId), cReg_Class.b_0_error);
                fillSummaryTree(Form("ADC%i_p1_sigma", cADCId), cReg_Class.b_1_error);
                fillSummaryTree(Form("ADC%i_chisquare", cADCId), cFit->GetChisquare());
                fillSummaryTree(Form("ADC%i_ndf", cADCId), cFit->GetNDF());
            }
            fillSummaryTree("VREFCNTR", cTrim);
            fResultFile->cd();
            cDACtoADCTree->Write();
            cDACtoADCMultiGraph->Draw("AL");
            cDACtoADCMultiGraph->GetXaxis()->SetTitle("DAC");
            cDACtoADCMultiGraph->GetYaxis()->SetTitle("ADC");
            dieLegende->Draw();
            // TLegend* dieLegende= cDACtoADCCanvas->BuildLegend();
            // dieLegende->AddEntry("pol1","Fit x","lpf");
            cDACtoADCCanvas->Write();
            // cDACtoADCMultiGraph->Write();
        }
    }
#endif
}

// Fixed in this context means: The ADC pin is not an AMUX pin
// Need statistics on spread of RSSI and temperature sensors
bool OTHybridTester::LpGBTTestFixedADCs()
{
    bool cReturn = true;
#ifdef __USE_ROOT__
#ifdef __TCUSB__
    std::map<std::string, std::string>  cADCsMap;
    std::map<std::string, float>*       cDefaultParameters;
    std::map<std::string, std::string>* cADCNametoPinMapping;
    std::string                         cADCNameString;
    std::vector<int>                    cADCValueVect;

    auto cFixedADCsTree = new TTree("tFixedADCs", "lpGBT ADCs not tied to AMUX");
    cFixedADCsTree->Branch("Id", &cADCNameString);
    cFixedADCsTree->Branch("AdcValue", &cADCValueVect);
    gStyle->SetOptStat(0);
#ifdef __SEH_USB__

    cADCsMap             = {{"VMON_P1V25_L", "VMON_P1V25_L_Nominal"},
                {"VMIN", "VMIN_Nominal"},
                {"TEMPP", "TEMPP_Nominal"},
                {"VTRX+_RSSI_ADC", "VTRX+_RSSI_ADC_Nominal"},
                {"PTAT_BPOL2V5", "PTAT_BPOL2V5_Nominal"},
                {"PTAT_BPOL12V", "PTAT_BPOL12V_Nominal"}};
    cDefaultParameters   = &f2SSEHDefaultParameters;
    cADCNametoPinMapping = &f2SSEHADCInputMap;
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_P1V25_L_Sense:On");
#else
    fTC_USB->set_P1V25_L_Sense(TC_2SSEH::P1V25SenseState::P1V25SenseState_On);
#endif
#elif __ROH_USB__

    cADCsMap = {{"12V_MONITOR_VD", "12V_MONITOR_VD_Nominal"},
                {"TEMP", "TEMP_Nominal"},
                {"VTRX+.RSSI_ADC", "VTRX+.RSSI_ADC_Nominal"},

                {"1V25_MONITOR", "1V25_MONITOR_Nominal"},
                {"2V55_MONITOR", "2V55_MONITOR_Nominal"}};
    cDefaultParameters = &fPSROHDefaultParameters;
    cADCNametoPinMapping = &fPSROHADCInputMap;
#endif

    auto cADCHistogram = new TH2I("hADCHistogram", "Fixed ADC Histogram", cADCsMap.size(), 0, cADCsMap.size(), 1024, 0, 1024);
    cADCHistogram->GetZaxis()->SetTitle("Number of entries");

    auto  cADCsMapIterator = cADCsMap.begin();
    int   cADCValue;
    int   cBinCount         = 1;
    float cConversionFactor = 1. / 1024.;

    fillSummaryTree("ADC conversion factor", cConversionFactor);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr)
        {
            cReturn = false;
            continue;
        }
        for(auto cOpticalGroup: *cBoard)
        {
            D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
            // Configure Temperature sensor
            // clpGBTInterface->ConfigureCurrentDAC(cOpticalGroup->flpGBT, std::vector<std::string>{"ADC4"}, 0x1c); // current chosen according to measurement range
            do
            {
                cADCValueVect.clear();
                cADCNameString = cADCsMapIterator->first;
                cADCHistogram->GetXaxis()->SetBinLabel(cBinCount, cADCsMapIterator->first.c_str());

                for(int cIteration = 0; cIteration < 10; ++cIteration)
                {
                    cADCValue = clpGBTInterface->ReadADC(cOpticalGroup->flpGBT, (*cADCNametoPinMapping)[cADCsMapIterator->first]);
                    // cADCValue-=34;
                    cADCValueVect.push_back(cADCValue);
                    cADCHistogram->Fill(cADCsMapIterator->first.c_str(), cADCValue, 1);
                }

                float sum           = std::accumulate(cADCValueVect.begin(), cADCValueVect.end(), 0.0);
                float mean          = sum / cADCValueVect.size();
                float cDifference_V = std::fabs((*cDefaultParameters)[cADCsMapIterator->second] - mean * cConversionFactor);
                fillSummaryTree(cADCsMapIterator->first.c_str(), mean * cConversionFactor);
                // Still hard coded threshold for imidiate boolean result, actual values are stored
                if(cDifference_V > 0.1)
                {
                    LOG(INFO) << BOLDRED << "Mismatch in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << mean * cConversionFactor << " V, nominal value is "
                              << (*cDefaultParameters)[cADCsMapIterator->second] << " V" << RESET;
                    cReturn = false;
                }
                else
                {
                    LOG(INFO) << BOLDGREEN << "Match in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << mean * cConversionFactor << " V, nominal value is "
                              << (*cDefaultParameters)[cADCsMapIterator->second] << " V" << RESET;
                }
                cFixedADCsTree->Fill();
                cADCsMapIterator++;
                cBinCount++;

            } while(cADCsMapIterator != cADCsMap.end());
        }
    }
    auto cADCCanvas = new TCanvas("cFixedADCs", "lpGBT ADCs not tied to AMUX", 1600, 900);
    cADCCanvas->SetRightMargin(0.2);
    cADCHistogram->GetXaxis()->SetTitle("ADC channel");
    cADCHistogram->GetYaxis()->SetTitle("ADC count");

    cADCHistogram->Draw("colz");
    cADCCanvas->Write();
    cFixedADCsTree->Write();

#ifdef __SEH_USB__
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_P1V25_L_Sense:Off");
#else
    fTC_USB->set_P1V25_L_Sense(TC_2SSEH::P1V25SenseState::P1V25SenseState_Off);
#endif
#endif
#endif
#endif
    return cReturn;
}

void OTHybridTester::LpGBTSetGPIOLevel(const std::vector<uint8_t>& pGPIOs, uint8_t pLevel)
{
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            LOG(INFO) << BOLDBLUE << "Set levels to " << +pLevel << RESET;
            clpGBTInterface->ConfigureGPIODirection(cOpticalGroup->flpGBT, pGPIOs, 1);
            clpGBTInterface->ConfigureGPIOLevel(cOpticalGroup->flpGBT, pGPIOs, pLevel);
        }
    }
}

bool OTHybridTester::LpGBTTestResetLines()
{
    bool cValid = true;

    std::vector<std::pair<std::string, uint8_t>> cLevels = {{"High", 1}, {"Low", 0}};
#ifdef __TCUSB__
    float cMeasurement;
#ifdef __ROH_USB__
    std::map<std::string, TC_PSROH::measurement> cResetLines = fResetLines;
    std::vector<uint8_t>                         cGPIOs      = {0, 1, 3, 6, 9, 12};
#elif __SEH_USB__
    std::map<std::string, TC_2SSEH::resetMeasurement> cResetLines = f2SSEHResetLines;
    std::vector<uint8_t>                              cGPIOs      = {0, 3, 6, 8};
#endif

    for(auto cLevel: cLevels)
    {
        LpGBTSetGPIOLevel(cGPIOs, cLevel.second);
        std::this_thread::sleep_for(std::chrono::milliseconds(12000));
        auto cMapIterator = cResetLines.begin();
        bool cStatus      = true;
        do
        {
#ifdef __ROH_USB__
            fTC_USB->adc_get(cMapIterator->second, cMeasurement);
            float cDifference_mV = std::fabs((cLevel.second * 1200) - cMeasurement);
#elif __SEH_USB__
#ifdef __TCP_SERVER__
            cMeasurement         = this->getMeasurement("read_reset:" + cMapIterator->first);
#else
            fTC_USB->read_reset(cMapIterator->second, cMeasurement);

#endif
            float cDifference_mV = std::fabs((cLevel.second * 1300) - cMeasurement * 1000.); // 1300
#endif
            cStatus = cStatus && (cDifference_mV <= 100);
            cValid  = cValid && cStatus;

            if(cDifference_mV > 100)
            {
                LOG(INFO) << BOLDRED << "Mismatch in GPIO connected to " << cMapIterator->first << RESET;
                fillSummaryTree(cMapIterator->first.c_str() + cLevel.first, 0);
            }
            else
            {
                LOG(INFO) << BOLDGREEN << "Match in GPIO connected to " << cMapIterator->first << RESET;
                fillSummaryTree(cMapIterator->first.c_str() + cLevel.first, 1);
            }
            cMapIterator++;
        } while(cMapIterator != cResetLines.end());
        if(cStatus)
            LOG(INFO) << BOLDBLUE << "Set levels to " << cLevel.first << " : test " << BOLDGREEN << " passed." << RESET;
        else
            LOG(INFO) << BOLDRED << "Set levels to " << cLevel.first << " : test " << BOLDRED << " failed." << RESET;
    }
#endif
    if(cValid) { LOG(INFO) << BOLDGREEN << "Reset test passed." << RESET; }
    else
    {
        LOG(INFO) << BOLDRED << "Reset test failed." << RESET;
    }
    return cValid;
}

bool OTHybridTester::LpGBTTestGPILines()
{
    std::map<std::string, uint8_t> fGPILines;
#ifdef __SEH_USB__
    fGPILines = f2SSEHGPILines;
#elif __ROH_USB__
    fGPILines = fPSROHGPILines; // On the TC the PWRGOOD is connected to a switch!
#endif
    bool                cValid = true;
    bool                cReadGPI;
    auto                cMapIterator    = fGPILines.begin();
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            while(cMapIterator != fGPILines.end())
            {
                cReadGPI = clpGBTInterface->ReadGPIO(cOpticalGroup->flpGBT, cMapIterator->second);
                cValid   = cValid && cReadGPI;
                if(!cReadGPI) { LOG(INFO) << BOLDRED << "GPIO connected to " << cMapIterator->first << " is low!" << RESET; }
                else
                {
                    LOG(INFO) << BOLDGREEN << "GPIO connected to " << cMapIterator->first << " is high!" << RESET;
                }
                fillSummaryTree(cMapIterator->first.c_str(), cReadGPI);
                cMapIterator++;
            }
        }
    }
    return cValid;
}

bool OTHybridTester::LpGBTTestVTRx()
{
    bool                cSuccess = true;
    bool                cRecent;
    uint32_t            cResult         = 0;
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            clpGBTInterface->WriteChipReg(cOpticalGroup->flpGBT, "I2CM1Config", 8);
            auto cMapIterator = fVTRxplusDefaultRegisters.begin();
            do
            {
                cRecent  = clpGBTInterface->WriteI2C(cOpticalGroup->flpGBT, 1, 0x50, cMapIterator->first, 1, 2);
                cResult  = clpGBTInterface->ReadI2C(cOpticalGroup->flpGBT, 1, 0x50, 1, 2);
                cSuccess = cSuccess && cRecent && (cResult == cMapIterator->second);
                if(cRecent && (cResult == cMapIterator->second))
                { LOG(INFO) << BOLDGREEN << "VTRx+ register " << +(cMapIterator->first) << " contains the default value " << +cResult << " ." << RESET; }
                else
                {
                    LOG(INFO) << BOLDRED << "Error in VTRx+ register " << +(cMapIterator->first) << " ." << RESET;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                cMapIterator++;
            } while(cMapIterator != fVTRxplusDefaultRegisters.end());
        }
    }
    fillSummaryTree("vtrxplusslowcontrol", cSuccess);
    return cSuccess;
}

bool OTHybridTester::LpGBTFastCommandChecker(uint8_t pPattern)
{
    uint8_t  cMatch;
    uint8_t  cShift;
    uint8_t  cWrappedByte;
    uint32_t cWrappedData;
    bool     res = false;

    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        fBeBoardInterface->setBoard(cBoard->getId());

        std::map<std::string, std::string> fFCMDLines;
#ifdef __SEH_USB__
        fFCMDLines = f2SSEHFCMDLines;
#elif __ROH_USB__
        fFCMDLines = fPSROHFCMDLines;
#endif

        auto cMapIterator = fFCMDLines.begin();
        LOG(INFO) << BOLDBLUE << "Checking against : " << std::bitset<8>(pPattern) << RESET;
        res = true;
        do
        {
            uint32_t cFCMDOutput = fBeBoardInterface->ReadBoardReg(cBoard, cMapIterator->second);
            LOG(INFO) << BOLDBLUE << "Scoped output on " << cMapIterator->first << ": " << std::bitset<32>(cFCMDOutput) << RESET;

            cMatch = 32;
            cShift = 0;
            for(uint8_t shift = 0; shift < 8; shift++)
            {
                cWrappedByte = (pPattern >> shift) | (pPattern << (8 - shift));
                cWrappedData = (cWrappedByte << 24) | (cWrappedByte << 16) | (cWrappedByte << 8) | (cWrappedByte << 0);
                LOG(DEBUG) << BOLDBLUE << std::bitset<8>(cWrappedByte) << RESET;
                LOG(DEBUG) << BOLDBLUE << std::bitset<32>(cWrappedData) << RESET;
                int popcount = __builtin_popcountll(cWrappedData ^ cFCMDOutput);
                if(popcount < cMatch)
                {
                    cMatch = popcount;
                    cShift = shift;
                }
                LOG(DEBUG) << BOLDBLUE << "Line " << cMapIterator->first << " Shift " << +shift << " Match " << +popcount << RESET;
            }
            LOG(INFO) << BOLDBLUE << "Found for " << cMapIterator->first << " a minimal bit difference of " << +cMatch << " for a bit shift of " << +cShift << RESET;

#ifdef __USE_ROOT__
            fillSummaryTree(cMapIterator->first + "_match", cMatch);
            fillSummaryTree(cMapIterator->first + "_shift", cShift);
#endif
            if((cMatch == 0)) { LOG(INFO) << BOLDGREEN << "FCMD Test passed for " << cMapIterator->first << RESET; }
            else
            {
                LOG(INFO) << BOLDRED << "FCMD Test failed for " << cMapIterator->first << RESET;
                res = false;
            }
            cMapIterator++;
        } while(cMapIterator != fFCMDLines.end());
    }
    return res;
}

void OTHybridTester::LpGBTRunEyeOpeningMonitor(uint8_t pEndOfCountSelect)
{
#ifdef __USE_ROOT__
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            LOG(INFO) << BOLDRED << "VDDRX read value = " << +clpGBTInterface->ReadADC(cOpticalGroup->flpGBT, "VDDRX") << RESET;
            // ROOT Tree for Eye Diagram from lpGBT Eye Opening Monitor
            auto cEyeDiagramTree = new TTree(Form("tEyeDiagram%i", cOpticalGroup->getOpticalGroupId()), "Eye Diagram form lpGBT Eye Opening Monitor");
            // vectors for Tree
            std::vector<int> cVoltageVector;
            std::vector<int> cTimeVector;
            std::vector<int> cCounterVector;
            // TBranches
            cEyeDiagramTree->Branch("VoltageStep", &cVoltageVector);
            cEyeDiagramTree->Branch("TimeStep", &cTimeVector);
            cEyeDiagramTree->Branch("Counter", &cCounterVector);
            // Create TCanvas & TH2I
            auto cEyeDiagramCanvas = new TCanvas(Form("cEyeDiagram%i", cOpticalGroup->getOpticalGroupId()), "Eye Opening Image", 500, 500);
            auto cObj              = gROOT->FindObject(Form("hEyeDiagram%i", cOpticalGroup->getOpticalGroupId()));
            if(cObj) delete cObj;
            auto cEyeDiagramHist = new TH2I(Form("hEyeDiagram%i", cOpticalGroup->getOpticalGroupId()), "Eye Opening Image", 64, 0, 63, 32, 0, 31);
            clpGBTInterface->ConfigureEOM(cOpticalGroup->flpGBT, pEndOfCountSelect, false, true);
            for(uint8_t cVoltageStep = 0; cVoltageStep < 31; cVoltageStep++)
            {
                clpGBTInterface->SelectEOMVof(cOpticalGroup->flpGBT, cVoltageStep);
                for(uint8_t cTimeStep = 0; cTimeStep < 64; cTimeStep++)
                {
                    clpGBTInterface->SelectEOMPhase(cOpticalGroup->flpGBT, cTimeStep);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    clpGBTInterface->StartEOM(cOpticalGroup->flpGBT, true);
                    uint8_t cEOMStatus = clpGBTInterface->GetEOMStatus(cOpticalGroup->flpGBT);
                    while((cEOMStatus & (0x1 << 1) >> 1) && !(cEOMStatus & (0x1 << 0))) { cEOMStatus = clpGBTInterface->GetEOMStatus(cOpticalGroup->flpGBT); }
                    uint16_t cCounterValue    = clpGBTInterface->GetEOMCounter(cOpticalGroup->flpGBT);
                    uint16_t c40MCounterValue = clpGBTInterface->ReadChipReg(cOpticalGroup->flpGBT, "EOMCounter40MH") << 8 | clpGBTInterface->ReadChipReg(cOpticalGroup->flpGBT, "EOMCounter40ML");
                    LOG(INFO) << YELLOW << "voltage step " << +cVoltageStep << ", time step " << +cTimeStep << ", counter value " << +cCounterValue << ", 40M counter " << +c40MCounterValue << RESET;
                    clpGBTInterface->StartEOM(cOpticalGroup->flpGBT, false);
                    cVoltageVector.push_back(cVoltageStep * 40); // 40 mV step
                    cTimeVector.push_back(cTimeStep * 6.1);      // 6.1 ps step
                    cCounterVector.push_back(cCounterValue);
                    // ROOT related filling
                    cEyeDiagramHist->Fill(cTimeStep, cVoltageStep, cCounterValue);
                    cEyeDiagramTree->Fill();
                }
            }
            cEyeDiagramHist->SetTitle("Eye Opening Diagram");
            cEyeDiagramHist->GetXaxis()->SetTitle("Time [ps]");
            cEyeDiagramHist->GetYaxis()->SetTitle("Vof [mV]");
            fResultFile->cd();
            cEyeDiagramTree->Write();
            cEyeDiagramHist->Write();
            cEyeDiagramCanvas->cd();
            cEyeDiagramHist->Draw("COLZ");
        }
    }
#endif
}

void OTHybridTester::LpGBTRunBitErrorRateTest(uint8_t pCoarseSource, uint8_t pFineSource, uint8_t pMeasTime, uint32_t pPattern)
{
    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
    if(pPattern != 0x00000000)
    {
        LOG(INFO) << BOLDMAGENTA << "Performing BER Test with constant pattern 0x" << std::hex << +pPattern << std::dec << RESET;
        LpGBTInjectULExternalPattern(true, pPattern & 0xFF);
    }
    // Run Bit Error Rate Test
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr) continue;
        for(auto cOpticalGroup: *cBoard)
        {
            // Configure BERT Pattern for comparision
            if(pPattern != 0x00000000) { clpGBTInterface->ConfigureBERTPattern(cOpticalGroup->flpGBT, pPattern); }
            else
            {
                LOG(INFO) << BOLDMAGENTA << "Performing BER Test with PRBS7" << RESET;
                clpGBTInterface->ConfigureRxPRBS(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, {0, 2}, true);
            }
            // Configure BERT block
            clpGBTInterface->ConfigureBERT(cOpticalGroup->flpGBT, pCoarseSource, pFineSource, pMeasTime);
            uint8_t cRxTerm = 1, cRxAcBias = 0, cRxInvert = 1;
            for(uint8_t cRxEqual = 0; cRxEqual < 4; cRxEqual++)
            {
                for(uint16_t cRxPhase = 0; cRxPhase < 16; cRxPhase++)
                {
                    clpGBTInterface->ConfigureRxChannels(cOpticalGroup->flpGBT, {0}, {0}, cRxEqual, cRxTerm, cRxAcBias, cRxInvert, cRxPhase);
                    // Run BERT and get result (fraction of errors)
                    float cBERTResult = 100 * clpGBTInterface->GetBERTResult(cOpticalGroup->flpGBT);
                    LOG(INFO) << BOLDWHITE << "\tBit Error Rate [RxEqual=" << +cRxEqual << ":RxPhase=" << +cRxPhase << "] = " << +cBERTResult << "%" << RESET;
                }
            }
            if(pPattern == 0x00000000) { clpGBTInterface->ConfigureRxPRBS(cOpticalGroup->flpGBT, {0, 1, 2, 3, 4, 5, 6}, {0, 2}, false); }
        }
    }
}
#ifdef __TCP_SERVER__
float OTHybridTester::getMeasurement(std::string name)
{
    std::string buffer = fTestcardClient->sendAndReceivePacket(name);
    float       value  = std::stof(this->getVariableValue("value", buffer));
    return value;
}
std::string OTHybridTester::getVariableValue(std::string variable, std::string buffer)
{
    size_t begin = buffer.find(variable) + variable.size() + 1;
    size_t end   = buffer.find(',', begin);
    if(end == std::string::npos) end = buffer.size();
    return buffer.substr(begin, end - begin);
}
#endif
#endif
