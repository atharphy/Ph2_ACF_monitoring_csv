#include "SEHTester.h"
#include "linearFitter.h"
#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/split.hpp>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

// initialize the static member

SEHTester::SEHTester() : OTHybridTester() {}

SEHTester::~SEHTester() {}

void SEHTester::Initialise()
{
    // reset I2C
    // fc7_daq_ctrl
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.physical_interface_block.fe_for_ps_roh.i2c_slave_reset", 0x01);
    }
}

void SEHTester::readTestParameters(std::string file)
{
    std::ifstream myReadFile(file);
    if(!myReadFile.is_open())
    {
        LOG(ERROR) << BOLDRED << "Parameter File " << file << " could not be opened. Check file path!" << RESET;
        throw std::runtime_error(std::string("Bad parameter file"));
    }
    // std::map<std::string,float> myRes;
    std::string line;
    while(std::getline(myReadFile, line))
    {
        std::stringstream             ss(line);
        std::pair<std::string, float> param;
        if(ss >> param.first >> param.second >> std::dec)
        {
            std::map<std::string, float>::iterator it = fDefaultParameters.find(param.first);
            if(it != fDefaultParameters.end()) { it->second = param.second; }
            else
            {
                fDefaultParameters.insert(param);
            }
        }
    }
}

void SEHTester::RampPowerSupply(std::string powerSupplyId, std::string channelId)
{
    if(fPowerSupplyClient == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Not connected to the power supply!!! RampPowerSupply cannot be executed" << RESET;
        throw std::runtime_error("RampPowerSupply cannot be executed");
    }

#ifdef __USE_ROOT__
    // Create TTree for Iout to Iin conversion in DC/DC
    auto cUinIinTree = new TTree("tUinIinTree", "Uin to Iin during power-up");

    // Create variables for TTree branches
    std::vector<float> cUinValVect;
    std::vector<float> cIinValVect;
    // Create TTree Branches
    cUinIinTree->Branch("Uin", &cUinValVect);
    cUinIinTree->Branch("Iin", &cIinValVect);

    auto cObj1 = gROOT->FindObject("mgUinIin");
    if(cObj1) delete cObj1;

    float cVolts = 0;
    float I_SEH;
    float U_SEH;
    while(cVolts < 10.01)
    {
        std::string setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(cVolts) + ",";
        fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

#ifdef __TCUSB__
#ifdef __SEH_USB__
#ifdef __TCP_SERVER__
        I_SEH = this->getMeasurement("read_supply:I_SEH");
        U_SEH = this->getMeasurement("read_supply:U_SEH");
#else
        fTC_USB->read_supply(fTC_USB->I_SEH, I_SEH);
        fTC_USB->read_supply(fTC_USB->U_SEH, U_SEH);
#endif
#endif
#endif

        cIinValVect.push_back(I_SEH);
        cUinValVect.push_back(U_SEH);
        cVolts += 0.1;
    }
    cUinIinTree->Fill();

    auto cUinIinGraph = new TGraph(cUinValVect.size(), cUinValVect.data(), cIinValVect.data());
    cUinIinGraph->SetName("gUinIin");
    cUinIinGraph->SetTitle("Uin to Iin during power-up");
    cUinIinGraph->SetLineWidth(3);
    cUinIinGraph->SetMarkerStyle(70);
    cUinIinTree->Write();

    auto cUinIinCanvas = new TCanvas("cUinIin", "Uin to Iin during power-up", 750, 500);

    cUinIinGraph->Draw("APL");
    cUinIinGraph->GetXaxis()->SetTitle("Uin [V]");
    cUinIinGraph->GetYaxis()->SetTitle("Iin [A]");

    cUinIinCanvas->Write();

#endif
}

int SEHTester::exampleFit()
{
    std::vector<float>              X{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<float>              Y{1, 3, 2, 5, 7, 8, 8, 9, 10, 12};
    std::vector<float>              Yerrors{0, 2, 5, 2, 1, 1, 2, 0, 2, 3};
    std::vector<int>                Xint{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<int>                Yint{1, 3, 2, 5, 7, 8, 8, 9, 10, 12};
    std::vector<std::vector<float>> Z(10);
    Z[0] = X;
    Z[1] = Y;
    std::vector<std::vector<int>> Zint(10);
    Zint[0] = Xint;
    Zint[1] = Yint;

    fitter::Linear_Regression<float> Reg_Class;
    Reg_Class.fit(X, Y, Yerrors);
    std::cout << "\n";
    std::cout << "Estimated Coefficients:\nb_0 = { " << Reg_Class.b_0 << " }  \
          \nb_1 = { "
              << Reg_Class.b_1 << " }" << std::endl;
    fitter::Linear_Regression<int> Reg_Classint;
    Reg_Classint.fit(Xint, Yint);
    std::cout << "\n";
    std::cout << "Estimated Coefficients:\nb_0 = { " << Reg_Classint.b_0 << " }  \
          \nb_1 = { "
              << Reg_Classint.b_1 << " }" << std::endl;
    LOG(INFO) << BOLDBLUE << "Using custom class: Parameter 1  " << Reg_Class.b_0 << " +/- " << Reg_Class.b_0_error << "  Parameter 2   " << Reg_Class.b_1 << " +/- " << Reg_Class.b_1_error << RESET;
    LOG(INFO) << BOLDBLUE << "Using custom class: Parameter 1  " << Reg_Classint.b_0 << " +/- " << Reg_Classint.b_0_error << "  Parameter 2   " << Reg_Classint.b_1 << " +/- " << Reg_Classint.b_1_error
              << RESET;
#ifdef __USE_ROOT__
    auto cGraph = new TGraphErrors(X.size(), X.data(), Y.data(), 0, Yerrors.data());
    cGraph->Fit("pol1");
    cGraph->SetName("test");
    cGraph->SetTitle("test");
    cGraph->SetLineColor(2);
    cGraph->SetFillColor(0);
    cGraph->SetLineWidth(3);
    auto cCanvas = new TCanvas("test", "test", 1600, 900);
    cGraph->Draw("AL*");

    TF1* cFit = (TF1*)cGraph->GetListOfFunctions()->FindObject("pol1");
    LOG(INFO) << BOLDBLUE << "Using ROOT: Parameter 1  " << cFit->GetParameter(0) << " +/- " << cFit->GetParError(0) << "  Parameter 2   " << cFit->GetParameter(1) << " +/- " << cFit->GetParError(1)
              << RESET;

    // cEfficencyCanvas->BuildLegend();
    cCanvas->Write();

#endif
    return 0;
}

void SEHTester::TestBiasVoltage(uint16_t pBiasVoltage)
{
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    float cUMon  = 0;
    float cVHVJ7 = 0;
    float cVHVJ8 = 0;

    /* fTC_USB->set_HV(false, true, true, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    fTC_USB->set_HV(true, true, true, pBiasVoltage); // 0x155 = 100V
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(15000));
#ifdef __TCP_SERVER__
    cUMon  = this->getMeasurement("read_hvmon:Mon");
    cVHVJ7 = this->getMeasurement("read_hvmon:VHVJ7");
    cVHVJ8 = this->getMeasurement("read_hvmon:VHVJ8");
#else
    fTC_USB->read_hvmon(fTC_USB->Mon, cUMon);
    fTC_USB->read_hvmon(fTC_USB->VHVJ7, cVHVJ7);
    fTC_USB->read_hvmon(fTC_USB->VHVJ8, cVHVJ8); */
    //----------------------------------------------------
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, true, true, 0);
#endif
    std::vector<float> cDACValVect;
    std::vector<float> cVHVJ7ValVect;
    std::vector<float> cVHVJ8ValVect;
    std::vector<float> cUMonValVect;
    auto               cBiasVoltageTree = new TTree("tBiasVoltageTree", "Bias Voltage Sensor Side");
    cBiasVoltageTree->Branch("DAC", &cDACValVect);
    cBiasVoltageTree->Branch("VHVJ7", &cVHVJ7ValVect);
    cBiasVoltageTree->Branch("VHVJ8", &cVHVJ8ValVect);
    cBiasVoltageTree->Branch("MON", &cUMonValVect);

    for(int cDACValue = 0; cDACValue <= 3500; cDACValue += 0x155)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#ifdef __TCP_SERVER__
        fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:1,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:" + std::to_string(cDACValue) + ",");
#else
        fTC_USB->set_HV(true, true, true, cDACValue); // 0x155 = 100V
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(15000));
#ifdef __TCP_SERVER__
        cUMon  = this->getMeasurement("read_hvmon:Mon");
        cVHVJ7 = this->getMeasurement("read_hvmon:VHVJ7");
        cVHVJ8 = this->getMeasurement("read_hvmon:VHVJ8");
#else
        fTC_USB->read_hvmon(fTC_USB->Mon, cUMon);
        fTC_USB->read_hvmon(fTC_USB->VHVJ7, cVHVJ7);
        fTC_USB->read_hvmon(fTC_USB->VHVJ8, cVHVJ8);
#endif
        LOG(INFO) << BOLDBLUE << "DAC value = " << +cDACValue << " --- Mon = " << +cUMon << " --- VHVJ7 = " << +cVHVJ7 << " --- VHVJ8 = " << +cVHVJ8 << RESET;
        cDACValVect.push_back(cDACValue);
        cVHVJ7ValVect.push_back(cVHVJ7);
        cVHVJ8ValVect.push_back(cVHVJ8);
        cUMonValVect.push_back(cUMon);
    }

    auto cDACtoHVCanvas = new TCanvas("cDACtoHV", "Bias voltage sensor side", 1600, 900);
    auto cObj           = gROOT->FindObject("mgDACtoHV");
    if(cObj) delete cObj;
    auto cDACtoHVMultiGraph = new TMultiGraph();
    cDACtoHVMultiGraph->SetName("mgDACtoHV");
    cDACtoHVMultiGraph->SetTitle("Bias voltage sensor side");

    auto cDACtoVHVJ7Graph = new TGraph(cDACValVect.size(), cDACValVect.data(), cVHVJ7ValVect.data());
    cDACtoVHVJ7Graph->SetName("gVHVJ7");
    cDACtoVHVJ7Graph->SetTitle("VHVJ7");
    cDACtoVHVJ7Graph->SetLineColor(1);
    cDACtoVHVJ7Graph->SetFillColor(0);
    cDACtoVHVJ7Graph->SetLineWidth(3);
    cDACtoVHVJ7Graph->SetMarkerStyle(20);
    cDACtoHVMultiGraph->Add(cDACtoVHVJ7Graph);

    auto cDACtoVHVJ8Graph = new TGraph(cDACValVect.size(), cDACValVect.data(), cVHVJ8ValVect.data());
    cDACtoVHVJ8Graph->SetName("gVHVJ8");
    cDACtoVHVJ8Graph->SetTitle("VHVJ8");
    cDACtoVHVJ8Graph->SetLineColor(2);
    cDACtoVHVJ8Graph->SetFillColor(0);
    cDACtoVHVJ8Graph->SetLineWidth(3);
    cDACtoVHVJ8Graph->SetMarkerStyle(21);
    cDACtoHVMultiGraph->Add(cDACtoVHVJ8Graph);

    auto cDACtoMonGraph = new TGraph(cDACValVect.size(), cDACValVect.data(), cUMonValVect.data());
    cDACtoMonGraph->SetName("gUMon");
    cDACtoMonGraph->SetTitle("UMon");
    cDACtoMonGraph->SetLineColor(3);
    cDACtoMonGraph->SetFillColor(0);
    cDACtoMonGraph->SetLineWidth(3);
    cDACtoMonGraph->SetMarkerStyle(22);
    cDACtoHVMultiGraph->Add(cDACtoMonGraph);
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, true, true, 0);
#endif
    cDACtoHVMultiGraph->Draw("ALP");
    cDACtoHVMultiGraph->GetXaxis()->SetTitle("HV DAC");
    cDACtoHVMultiGraph->GetYaxis()->SetTitle("Voltage [V]");

    cDACtoHVCanvas->BuildLegend();
    cDACtoHVCanvas->Write();
    cBiasVoltageTree->Fill();
    cBiasVoltageTree->Write();
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, false, false, 0);
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    fillSummaryTree("BiasDone", 1);
#endif
#endif
#endif
}
void SEHTester::ExternalTestLeakageCurrent(uint16_t pHvSet, double measurementTime, std::string powerSupplyId, std::string channelId)
{
    // time_t startTime;
    // time(&startTime);
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    struct timespec startTime, timer;
    srand(time(NULL));

    /* generate secret number between 1 and 10: */
    // int iSecond;
    // int iMilli;

    // start timer.
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
    // clock_gettime(CLOCK_REALTIME, &start);
    clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:1,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:" + std::to_string(0) + ",");
#else
    fTC_USB->set_HV(true, false, false, 0);
#endif
    std::string setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(-1 * static_cast<float>(pHvSet)) + ",";
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    setVoltageMessage = "TurnOn,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId;
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    // Create TTree for leakage current
    auto cLeakTree = new TTree("tExternalLeakTree", "Leakage Current");
    // Create variables for TTree branches
    std::vector<double> cILeakValVect;
    std::vector<double> cHvMeaValVect;
    std::vector<double> cIMeaValVect;
    std::vector<double> cTimeValVect;
    // Create TTree Branches
    cLeakTree->Branch("ILeak", &cILeakValVect);
    cLeakTree->Branch("HvMea", &cHvMeaValVect);
    cLeakTree->Branch("IMea", &cIMeaValVect);
    cLeakTree->Branch("Time", &cTimeValVect);

    // for(int cPoint = 0; cPoint <= (int)pPoints; cPoint += 1)
    double time_taken;
    do
    {
        // iSecond = rand() % 2;
        // iMilli  = rand() % 1000;
        // LOG(INFO) << BOLDBLUE << "Seconds " << +iSecond << " Milli " << +iMilli << RESET;
        float ILeak = 0;
        float HvMea = 0;
        float IMea  = 0;
        // time_t timer;
        // time(&timer);
        clock_gettime(CLOCK_MONOTONIC, &timer);
        std::string buffer = fPowerSupplyClient->sendAndReceivePacket("GetStatus");
        HvMea              = std::stof(getVariableValue(powerSupplyId + "_" + channelId + "_Voltage", buffer));
        IMea               = 1e9 * std::stof(getVariableValue(powerSupplyId + "_" + channelId + "_Current", buffer));
#ifdef __TCP_SERVER__
        // UMon = this->getMeasurement("read_hvmon:Mon");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ILeak = this->getMeasurement("read_hvmon:HV_meas");
#else
        // fTC_USB->read_hvmon(fTC_USB->Mon, UMon);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fTC_USB->read_hvmon(fTC_USB->HV_meas, ILeak);
#endif
        cILeakValVect.push_back(double(ILeak));
        cHvMeaValVect.push_back(HvMea);
        cIMeaValVect.push_back(IMea);
        // cTimeValVect.push_back(timer-startTime);

        time_taken = (timer.tv_sec - startTime.tv_sec) * 1e9;
        time_taken = (time_taken + (timer.tv_nsec - startTime.tv_nsec)) * 1e-9;
        cTimeValVect.push_back(time_taken);

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    } while(time_taken < measurementTime);
    cLeakTree->Fill();
    fResultFile->cd();
    cLeakTree->Write();

    auto cLeakMultiGraph = new TMultiGraph();
    cLeakMultiGraph->SetName("mgILeak");
    cLeakMultiGraph->SetTitle("Leakage Current");
    auto cleakGraph = new TGraph(cTimeValVect.size(), cTimeValVect.data(), cILeakValVect.data());
    cleakGraph->SetName("gILeakTC");
    cleakGraph->SetTitle("Leakage Current Test Card");
    cleakGraph->SetLineColor(2);
    cleakGraph->SetFillColor(0);
    cleakGraph->SetLineWidth(3);
    cLeakMultiGraph->Add(cleakGraph);
    auto cPSleakGraph = new TGraph(cTimeValVect.size(), cTimeValVect.data(), cIMeaValVect.data());
    cPSleakGraph->SetName("gILeakPS");
    cPSleakGraph->SetTitle("Leakage Current Power Supply");
    cPSleakGraph->SetLineColor(3);
    cPSleakGraph->SetFillColor(0);
    cPSleakGraph->SetLineWidth(3);
    cLeakMultiGraph->Add(cPSleakGraph);
    auto cLeakCanvas = new TCanvas("cLeak", "Bias Voltage Leakage Current", 1600, 900);
    cLeakMultiGraph->Draw("AL*");
    cLeakMultiGraph->GetXaxis()->SetTitle("Time [s]");
    cLeakMultiGraph->GetYaxis()->SetTitle("Leakage Current [nA]");

    cLeakCanvas->BuildLegend();
    cLeakMultiGraph->Write();
    cLeakCanvas->Write();

    auto cMonGraph = new TGraph(cTimeValVect.size(), cTimeValVect.data(), cHvMeaValVect.data());
    cMonGraph->SetName("gHvMea");
    cMonGraph->SetTitle("Monitoring Voltage");
    cMonGraph->SetLineColor(2);
    cMonGraph->SetFillColor(0);
    cMonGraph->SetLineWidth(3);
    auto cMonCanvas = new TCanvas("cMon", "Bias Voltage Monitoring Voltage", 1600, 900);
    cMonGraph->Draw("AL*");
    cMonGraph->GetXaxis()->SetTitle("Time [s]");
    cMonGraph->GetYaxis()->SetTitle("High Voltage [V]");

    // cEfficencyCanvas->BuildLegend();
    cMonGraph->Write();
    cMonCanvas->Write();
    setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(0) + ",";
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    setVoltageMessage = "TurnOff,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId;
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, false, false, 0);

    fillSummaryTree("ExternalLeakDone", 1);
#endif
#endif
#endif
#endif
}
void SEHTester::ExternalTestBiasVoltage(std::string powerSupplyId, std::string channelId)
{
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    // float cHvSet  = 0;
    float cHvMea = 0;
    float cVHVJ7 = 0;
    float cVHVJ8 = 0;
    if(fPowerSupplyClient == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Not connected to the power supply!!! ExternalTestBiasVoltage cannot be executed" << RESET;
        throw std::runtime_error("ExternalTestBiasVoltage cannot be executed");
    }

#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, true, true, 0);
#endif
    std::vector<float> cHvSetValVect;
    std::vector<float> cVHVJ7ValVect;
    std::vector<float> cVHVJ8ValVect;
    std::vector<float> cHvMeaValVect;
    std::vector<float> cPlotHvMeaValVect;
    auto               cBiasVoltageTree = new TTree("tExtBiasVoltageTree", "Bias Voltage Sensor Side");
    cBiasVoltageTree->Branch("HvSet", &cHvSetValVect);
    cBiasVoltageTree->Branch("VHVJ7", &cVHVJ7ValVect);
    cBiasVoltageTree->Branch("VHVJ8", &cVHVJ8ValVect);
    cBiasVoltageTree->Branch("HvMea", &cHvMeaValVect);
    std::string setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(0) + ",";
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    setVoltageMessage = "TurnOn,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId;
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    for(int cHvSet = 0; cHvSet <= 1000; cHvSet += 100)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
#ifdef __TCP_SERVER__
        fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:1,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:" + std::to_string(0) + ",");
#else
        fTC_USB->set_HV(true, true, true, 0); // 0x155 = 100V
#endif

        setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(-1 * cHvSet) + ",";
        fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
        std::this_thread::sleep_for(std::chrono::milliseconds(15000));
        std::string buffer = fPowerSupplyClient->sendAndReceivePacket("GetStatus");
        cHvMea             = std::stof(getVariableValue(powerSupplyId + "_" + channelId + "_Voltage", buffer));
#ifdef __TCP_SERVER__
        // cUMon  = this->getMeasurement("read_hvmon:Mon");
        cVHVJ7 = this->getMeasurement("read_hvmon:VHVJ7");
        cVHVJ8 = this->getMeasurement("read_hvmon:VHVJ8");
#else
        // fTC_USB->read_hvmon(fTC_USB->Mon, cUMon);
        fTC_USB->read_hvmon(fTC_USB->VHVJ7, cVHVJ7);
        fTC_USB->read_hvmon(fTC_USB->VHVJ8, cVHVJ8);
#endif
        LOG(INFO) << BOLDBLUE << "Set HV value = " << +cHvSet << " --- VHVJ7 = " << +cVHVJ7 << " --- VHVJ8 = " << +cVHVJ8 << RESET;
        cHvSetValVect.push_back(cHvSet);
        cVHVJ7ValVect.push_back(cVHVJ7);
        cVHVJ8ValVect.push_back(cVHVJ8);
        cHvMeaValVect.push_back(cHvMea);
        cPlotHvMeaValVect.push_back(-1 * cHvMea / 1000.);
    }

    auto cDACtoHVCanvas = new TCanvas("cDACtoHV", "Bias voltage sensor side", 1600, 900);
    auto cObj           = gROOT->FindObject("mgDACtoHV");
    if(cObj) delete cObj;
    auto cDACtoHVMultiGraph = new TMultiGraph();
    cDACtoHVMultiGraph->SetName("mgDACtoHV");
    cDACtoHVMultiGraph->SetTitle("Bias voltage sensor side");

    auto cDACtoVHVJ7Graph = new TGraph(cHvSetValVect.size(), cHvSetValVect.data(), cVHVJ7ValVect.data());
    cDACtoVHVJ7Graph->SetName("gVHVJ7");
    cDACtoVHVJ7Graph->SetTitle("VHVJ7");
    cDACtoVHVJ7Graph->SetLineColor(1);
    cDACtoVHVJ7Graph->SetFillColor(0);
    cDACtoVHVJ7Graph->SetLineWidth(3);
    cDACtoVHVJ7Graph->SetMarkerStyle(20);
    cDACtoHVMultiGraph->Add(cDACtoVHVJ7Graph);

    auto cDACtoVHVJ8Graph = new TGraph(cHvSetValVect.size(), cHvSetValVect.data(), cVHVJ8ValVect.data());
    cDACtoVHVJ8Graph->SetName("gVHVJ8");
    cDACtoVHVJ8Graph->SetTitle("VHVJ8");
    cDACtoVHVJ8Graph->SetLineColor(2);
    cDACtoVHVJ8Graph->SetFillColor(0);
    cDACtoVHVJ8Graph->SetLineWidth(3);
    cDACtoVHVJ8Graph->SetMarkerStyle(21);
    cDACtoHVMultiGraph->Add(cDACtoVHVJ8Graph);

    auto cDACtoMonGraph = new TGraph(cHvSetValVect.size(), cHvSetValVect.data(), cPlotHvMeaValVect.data());
    cDACtoMonGraph->SetName("gHvMea*1/1000");
    cDACtoMonGraph->SetTitle("HvMea*1/1000");
    cDACtoMonGraph->SetLineColor(3);
    cDACtoMonGraph->SetFillColor(0);
    cDACtoMonGraph->SetLineWidth(3);
    cDACtoMonGraph->SetMarkerStyle(22);
    cDACtoHVMultiGraph->Add(cDACtoMonGraph);
    setVoltageMessage = "SetVoltage,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId + ",Value:" + std::to_string(0) + ",";
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
    setVoltageMessage = "TurnOff,PowerSupplyId:" + powerSupplyId + ",ChannelId:" + channelId;
    fPowerSupplyClient->sendAndReceivePacket(setVoltageMessage);
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:1,hvmonx8Relay:1,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, true, true, 0);
#endif
    cDACtoHVMultiGraph->Draw("ALP");
    cDACtoHVMultiGraph->GetXaxis()->SetTitle("Set HV [V]");
    cDACtoHVMultiGraph->GetYaxis()->SetTitle("Voltage [V]");

    cDACtoHVCanvas->BuildLegend();
    cDACtoHVMultiGraph->Write();
    cDACtoHVCanvas->Write();
    cBiasVoltageTree->Fill();
    cBiasVoltageTree->Write();
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, false, false, 0);
#endif
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    fillSummaryTree("ExternalBiasDone", 1);
#endif
#endif
#endif
}

void SEHTester::TurnOn()
{
#ifdef __TCUSB__
#ifdef __SEH_USB__
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("TurnOn");
#else
    fTC_USB->set_SehSupply(fTC_USB->sehSupply_On);
#endif
#endif
#endif
}
void SEHTester::TurnOff()
{
#ifdef __TCUSB__
#ifdef __SEH_USB__
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("TurnOff");
#else
    fTC_USB->set_SehSupply(fTC_USB->sehSupply_Off);
#endif
#endif
#endif
}
void SEHTester::TestLeakageCurrent(uint32_t pHvDacValue, double measurementTime)
{
    // time_t startTime;
    // time(&startTime);
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    struct timespec startTime, timer;
    srand(time(NULL));

    /* generate secret number between 1 and 10: */
    // int iSecond;
    // int iMilli;

    // start timer.
    // clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);
    // clock_gettime(CLOCK_REALTIME, &start);
    clock_gettime(CLOCK_MONOTONIC, &startTime);
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:1,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:" + std::to_string(pHvDacValue) + ",");
#else
    fTC_USB->set_HV(true, false, false, pHvDacValue);
#endif
    // Create TTree for leakage current
    auto cLeakTree = new TTree("tLeakTree", "Leakage Current");
    // Create variables for TTree branches
    std::vector<double> cILeakValVect;
    std::vector<double> cUMonValVect;
    std::vector<double> cTimeValVect;
    // Create TTree Branches
    cLeakTree->Branch("ILeak", &cILeakValVect);
    cLeakTree->Branch("UMon", &cUMonValVect);
    cLeakTree->Branch("Time", &cTimeValVect);

    // for(int cPoint = 0; cPoint <= (int)pPoints; cPoint += 1)
    double time_taken;
    do
    {
        // iSecond = rand() % 2;
        // iMilli  = rand() % 1000;
        // LOG(INFO) << BOLDBLUE << "Seconds " << +iSecond << " Milli " << +iMilli << RESET;
        float ILeak = 0;
        float UMon  = 0;
        // time_t timer;
        // time(&timer);
        clock_gettime(CLOCK_MONOTONIC, &timer);
#ifdef __TCP_SERVER__
        UMon = this->getMeasurement("read_hvmon:Mon");
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ILeak = this->getMeasurement("read_hvmon:HV_meas");
#else
        fTC_USB->read_hvmon(fTC_USB->Mon, UMon);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        fTC_USB->read_hvmon(fTC_USB->HV_meas, ILeak);
#endif
        cILeakValVect.push_back(double(ILeak));
        cUMonValVect.push_back(UMon);
        // cTimeValVect.push_back(timer-startTime);

        time_taken = (timer.tv_sec - startTime.tv_sec) * 1e9;
        time_taken = (time_taken + (timer.tv_nsec - startTime.tv_nsec)) * 1e-9;
        cTimeValVect.push_back(time_taken);

        std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    } while(time_taken < measurementTime);
    cLeakTree->Fill();
    fResultFile->cd();
    cLeakTree->Write();

    auto cleakGraph = new TGraph(cTimeValVect.size(), cTimeValVect.data(), cILeakValVect.data());
    cleakGraph->SetName("ILeak");
    cleakGraph->SetTitle("Leakage Current");
    cleakGraph->SetLineColor(2);
    cleakGraph->SetFillColor(0);
    cleakGraph->SetLineWidth(3);
    auto cLeakCanvas = new TCanvas("cLeak", "Bias Voltage Leakage Current", 1600, 900);
    cleakGraph->Draw("AL*");
    cleakGraph->GetXaxis()->SetTitle("Time [s]");
    cleakGraph->GetYaxis()->SetTitle("Leakage Current [nA]");

    cLeakCanvas->Write();

    auto cMonGraph = new TGraph(cTimeValVect.size(), cTimeValVect.data(), cUMonValVect.data());
    cMonGraph->SetName("Umon");
    cMonGraph->SetTitle("Monitoring Voltage");
    cMonGraph->SetLineColor(2);
    cMonGraph->SetFillColor(0);
    cMonGraph->SetLineWidth(3);
    auto cMonCanvas = new TCanvas("cMon", "Bias Voltage Monitoring Voltage", 1600, 900);
    cMonGraph->Draw("AL*");
    cMonGraph->GetXaxis()->SetTitle("Time [s]");
    cMonGraph->GetYaxis()->SetTitle("Monitoring Voltage [V]");

    cMonCanvas->Write();
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_HV,hvRelay:0,hvmonx7Relay:0,hvmonx8Relay:0,HVDAC_setvalue:0,");
#else
    fTC_USB->set_HV(false, false, false, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    fillSummaryTree("LeakDone", 1);
#endif
#endif
#endif
#endif
}

void SEHTester::TestEfficiency(uint32_t pMinLoadValue, uint32_t pMaxLoadValue, uint32_t pStep)
{
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    // Create TTree for Iout to Iin conversion in DC/DC
    auto cEfficiencyTree = new TTree("tEfficiency", "DC/DC Efficiency");
    // Create variables for TTree branches
    std::vector<float>       cUoutRValVect;
    std::vector<float>       cUoutLValVect;
    std::vector<float>       cU2v5ValVect;
    std::vector<float>       cIoutRValVect;
    std::vector<float>       cIoutLValVect;
    std::vector<float>       cIoutValVect;
    std::vector<float>       cIinValVect;
    std::vector<float>       cUinValVect;
    std::vector<float>       cEfficiencyValVect;
    std::vector<std::string> cSideValVect;
    // Create TTree Branches
    cEfficiencyTree->Branch("Uout_R", &cUoutRValVect);
    cEfficiencyTree->Branch("Uout_L", &cUoutLValVect);
    cEfficiencyTree->Branch("Uout_2v5", &cU2v5ValVect);
    cEfficiencyTree->Branch("Iin", &cIinValVect);
    cEfficiencyTree->Branch("Uin", &cUinValVect);
    cEfficiencyTree->Branch("Iout_R", &cIoutRValVect);
    cEfficiencyTree->Branch("Iout_L", &cIoutLValVect);
    cEfficiencyTree->Branch("Iout", &cIoutValVect);
    cEfficiencyTree->Branch("Efficiency", &cEfficiencyValVect);
    cEfficiencyTree->Branch("side", &cSideValVect);
    std::vector<std::string> pSides = {"both", "right", "left"};

    auto cObj1 = gROOT->FindObject("mgIouttoIin");
    auto cObj2 = gROOT->FindObject("mgEfficiency");
    auto cObj3 = gROOT->FindObject("mgUouttoIout");
    if(cObj1) delete cObj1;
    if(cObj2) delete cObj2;
    if(cObj3) delete cObj3;

    auto cIouttoIinMultiGraph = new TMultiGraph();
    cIouttoIinMultiGraph->SetName("mgIouttoIin");
    cIouttoIinMultiGraph->SetTitle("DC/DC - Iout to Iin conversion");

    auto cUouttoIoutMultiGraph = new TMultiGraph();
    cUouttoIoutMultiGraph->SetName("mgUouttoIout");
    cUouttoIoutMultiGraph->SetTitle("DC/DC - Uout vs Iout conversion");

    auto cEfficiencyMultiGraph = new TMultiGraph();
    cEfficiencyMultiGraph->SetName("mgEfficiency");
    cEfficiencyMultiGraph->SetTitle("DC/DC conversion efficiency");

    LOG(INFO) << BOLDMAGENTA << "Testing DC/DC" << RESET;
    int iterator = 1;
    // We run three times; Only right side, only left side and load on both sides
    for(const auto& cSide: pSides)
    {
#ifdef __TCP_SERVER__
        fTestcardClient->sendAndReceivePacket("set_load1,enable:0,path:0,value:0,");
        fTestcardClient->sendAndReceivePacket("set_load2,enable:0,path:0,value:0,");
#else
        fTC_USB->set_load1(false, false, 0);
        fTC_USB->set_load2(false, false, 0);
#endif
        cIoutRValVect.clear(), cIinValVect.clear(), cUoutRValVect.clear(), cUoutLValVect.clear();
        cEfficiencyValVect.clear(), cU2v5ValVect.clear(), cIoutLValVect.clear();
        cSideValVect.clear(), cUinValVect.clear(), cIoutValVect.clear();

        for(int cLoadValue = pMinLoadValue; cLoadValue <= (int)pMaxLoadValue; cLoadValue += pStep)
        {
            float I_SEH;
            float U_SEH;
            float I_P1V2_R;
            float I_P1V2_L;
            float U_P1V2_R;
            float U_P1V2_L;
            float U_P2V5 = 0;
#ifdef __TCP_SERVER__
            if(cSide == "both")
            {
                // fTestcardClient->sendAndReceivePacket("set_load2,enable:1,path:0,value:" + std::to_string(cLoadValue) + ",");
                // fTestcardClient->sendAndReceivePacket("set_load1,enable:1,path:0,value:" + std::to_string(cLoadValue) + ",");
                fTestcardClient->sendAndReceivePacket("set_load2,enable:1,path:0,value:171");
                fTestcardClient->sendAndReceivePacket("set_load1,enable:1,path:0,value:205");
            }
            if(cSide == "left") { fTestcardClient->sendAndReceivePacket("set_load1,enable:1,path:0,value:" + std::to_string(cLoadValue) + ","); }
            if(cSide == "right") { fTestcardClient->sendAndReceivePacket("set_load2,enable:1,path:0,value:" + std::to_string(cLoadValue) + ","); }
#else
            if(cSide == "both")
            {
                fTC_USB->set_load1(true, false, cLoadValue);
                fTC_USB->set_load2(true, false, cLoadValue);
            }
            if(cSide == "left") { fTC_USB->set_load2(true, false, cLoadValue); }
            if(cSide == "right") { fTC_USB->set_load1(true, false, cLoadValue); }
#endif
            // Delay needs to be optimized during functional testing
            std::this_thread::sleep_for(std::chrono::milliseconds(5000));
#ifdef __TCP_SERVER__
            I_P1V2_R = this->getMeasurement("read_load:I_P1V2_R");
            I_P1V2_L = this->getMeasurement("read_load:I_P1V2_L");
            I_SEH    = this->getMeasurement("read_supply:I_SEH");
            U_P1V2_R = this->getMeasurement("read_load:U_P1V2_R");
            U_P1V2_L = this->getMeasurement("read_load:U_P1V2_L");
            U_SEH    = this->getMeasurement("read_supply:U_SEH");
            U_P2V5   = this->getMeasurement("read_load:P2V5_VTRx_MON");
#else
            fTC_USB->read_load(fTC_USB->I_P1V2_R, I_P1V2_R);
            fTC_USB->read_load(fTC_USB->I_P1V2_L, I_P1V2_L);
            fTC_USB->read_supply(fTC_USB->I_SEH, I_SEH);
            fTC_USB->read_load(fTC_USB->U_P1V2_R, U_P1V2_R);
            fTC_USB->read_load(fTC_USB->U_P1V2_L, U_P1V2_L);
            fTC_USB->read_supply(fTC_USB->U_SEH, U_SEH);
            fTC_USB->read_load(fTC_USB->P2V5_VTRx_MON, U_P2V5);
#endif
            // The input binning is performed in DAC values, the result is binned in the measured current
            cIoutValVect.push_back(I_P1V2_R + I_P1V2_L);
            cIinValVect.push_back(I_SEH);
            cUinValVect.push_back(U_SEH);
            cU2v5ValVect.push_back(U_P2V5);
            cIoutRValVect.push_back(I_P1V2_R);
            cUoutRValVect.push_back(U_P1V2_R);
            cIoutLValVect.push_back(I_P1V2_L);
            cUoutLValVect.push_back(U_P1V2_L);
            cSideValVect.push_back(cSide);
            if(I_SEH * U_SEH == 0) { cEfficiencyValVect.push_back(-1); }
            else
            {
                cEfficiencyValVect.push_back((I_P1V2_R * U_P1V2_R + I_P1V2_L * U_P1V2_L) / (I_SEH * U_SEH));
            }
        }
        cEfficiencyTree->Fill();

        auto    cIouttoIinGraph = new TGraph(cIoutValVect.size(), cIoutValVect.data(), cIinValVect.data());
        TString str             = cSide;
        cIouttoIinGraph->SetName(str);
        cIouttoIinGraph->SetTitle(str);
        cIouttoIinGraph->SetLineColor(iterator);
        cIouttoIinGraph->SetFillColor(0);
        cIouttoIinGraph->SetLineWidth(3);
        cIouttoIinGraph->SetMarkerStyle(iterator + 20);
        cIouttoIinMultiGraph->Add(cIouttoIinGraph);

        auto cEfficiencyGraph = new TGraph(cIoutValVect.size(), cIoutValVect.data(), cEfficiencyValVect.data());
        cEfficiencyGraph->SetName(str);
        cEfficiencyGraph->SetTitle(str);
        cEfficiencyGraph->SetLineColor(iterator);
        cEfficiencyGraph->SetFillColor(0);
        cEfficiencyGraph->SetLineWidth(3);
        cEfficiencyGraph->SetMarkerStyle(iterator + 20);
        cEfficiencyMultiGraph->Add(cEfficiencyGraph);

        auto cUoutRtoIoutRGraph = new TGraph(cIoutRValVect.size(), cIoutRValVect.data(), cUoutRValVect.data());
        str                     = "Voltage right side current drawn " + cSide;
        cUoutRtoIoutRGraph->SetName(str);
        cUoutRtoIoutRGraph->SetTitle(str);
        cUoutRtoIoutRGraph->SetLineColor(iterator);
        cUoutRtoIoutRGraph->SetFillColor(0);
        cUoutRtoIoutRGraph->SetLineWidth(3);
        cUoutRtoIoutRGraph->SetMarkerStyle(iterator + 20);
        cUouttoIoutMultiGraph->Add(cUoutRtoIoutRGraph);

        auto cUoutLtoIoutLGraph = new TGraph(cIoutLValVect.size(), cIoutLValVect.data(), cUoutLValVect.data());
        str                     = "Voltage left side current drawn " + cSide;
        cUoutLtoIoutLGraph->SetName(str);
        cUoutLtoIoutLGraph->SetTitle(str);
        cUoutLtoIoutLGraph->SetLineColor(iterator);
        cUoutLtoIoutLGraph->SetFillColor(0);
        cUoutLtoIoutLGraph->SetLineWidth(3);
        cUoutLtoIoutLGraph->SetMarkerStyle(iterator + 30);
        cUouttoIoutMultiGraph->Add(cUoutLtoIoutLGraph);
        iterator++;
    }
#ifdef __TCP_SERVER__
    fTestcardClient->sendAndReceivePacket("set_load1,enable:0,path:0,value:0,");
    fTestcardClient->sendAndReceivePacket("set_load2,enable:0,path:0,value:0,");
#else
    fTC_USB->set_load1(false, false, 0);
    fTC_USB->set_load2(false, false, 0);
#endif
    fResultFile->cd();
    cEfficiencyTree->Write();

    auto cUouttoIoutCanvas = new TCanvas("cUouttoIout", "Uout versus Iout DC/DC", 750, 500);
    cUouttoIoutMultiGraph->Draw("ALP");
    cUouttoIoutMultiGraph->GetXaxis()->SetTitle("Iout [A]");
    cUouttoIoutMultiGraph->GetYaxis()->SetTitle("Uout [V]");
    cUouttoIoutCanvas->BuildLegend();
    cUouttoIoutCanvas->Write();

    auto cEfficiencyCanvas = new TCanvas("cEfficiency", "DC/DC conversion efficiency", 750, 500);
    cEfficiencyMultiGraph->Draw("ALP");
    cEfficiencyMultiGraph->GetXaxis()->SetTitle("Iout [A]");
    cEfficiencyMultiGraph->GetYaxis()->SetTitle("Efficiency");
    cEfficiencyCanvas->BuildLegend();
    cEfficiencyCanvas->Write();

    auto cIouttoIinCanvas = new TCanvas("cIouttoIin", "Iout to Iin conversion in DC/DC", 750, 500);
    cIouttoIinMultiGraph->Draw("ALP");
    cIouttoIinMultiGraph->GetXaxis()->SetTitle("Iout [A]");
    cIouttoIinMultiGraph->GetYaxis()->SetTitle("Iin [A]");
    cIouttoIinCanvas->BuildLegend();
    cIouttoIinCanvas->Write();

    fillSummaryTree("EfficiencyDone", 1);
#endif
#endif
#endif
}
// Fixed in this context means: The ADC pin is not an AMUX pin
// Need statistics on spread of RSSI and temperature sensors
/* bool SEHTester::TestFixedADCs()
{
    bool cReturn;
#ifdef __USE_ROOT__
    auto cFixedADCsTree = new TTree("FixedADCs", "lpGBT ADCs not tied to AMUX");
    gStyle->SetOptStat(0);
    auto cADCHistogram = new TH2I("cADCHistogram", "Fixed ADC Histogram", 6, 0, 6, 1024, 0, 1024);
    cADCHistogram->GetZaxis()->SetTitle("Number of entries");
    std::map<std::string, std::string> cADCsMap         = {{"VMON_P1V25_L", "VMON_P1V25_L_Nominal"},
                                                   {"VMIN", "VMIN_Nominal"},
                                                   {"TEMPP", "TEMPP_Nominal"},
                                                   {"VTRX+_RSSI_ADC", "VTRX+_RSSI_ADC_Nominal"},
                                                   {"PTAT_BPOL2V5", "PTAT_BPOL2V5_Nominal"},
                                                   {"PTAT_BPOL12V", "PTAT_BPOL12V_Nominal"}};
    auto                               cADCsMapIterator = cADCsMap.begin();
    int                                cADCValue;
    int                                cBinCount         = 1;
    float                              cConversionFactor = 1. / 1024.;
    std::vector<int>                   cADCValueVect;
    fillSummaryTree("ADC conversion factor", cConversionFactor);
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT == nullptr)
        {
            LOG(INFO) << BOLDRED << "No lpGBT to test ADCs!" << RESET;
            cReturn = false;
            continue;
        }
        for(auto cOpticalGroup: *cBoard)
        {
            D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
            // Configure Temperature sensor
            clpGBTInterface->ConfigureCurrentDAC(cOpticalGroup->flpGBT, std::vector<std::string>{"ADC4"}, 0xff);
            do
            {
                cADCValueVect.clear();
                cADCHistogram->GetXaxis()->SetBinLabel(cBinCount, cADCsMapIterator->first.c_str());

                for(int cIteration = 0; cIteration < 10; ++cIteration)
                {
                    cADCValue = clpGBTInterface->ReadADC(cOpticalGroup->flpGBT, cADCsMapIterator->first);
                    cADCValueVect.push_back(cADCValue);
                    cADCHistogram->Fill(cADCsMapIterator->first.c_str(), cADCValue, 1);
                }
                // fTC_2SSEH->read_supply(c2SSEHMapIterator->second, k);

                fillSummaryTree(cADCsMapIterator->first, cADCValue * cConversionFactor);
                float sum           = std::accumulate(cADCValueVect.begin(), cADCValueVect.end(), 0.0);
                float mean          = sum / cADCValueVect.size();
                float cDifference_V = std::fabs((fDefaultParameters[cADCsMapIterator->second]) - mean * cConversionFactor);

                // Still hard coded threshold for imidiate boolean result, actual values are stored
                if(cDifference_V > 0.1)
                {
                    LOG(INFO) << BOLDRED << "Mismatch in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << cADCValue * cConversionFactor << " V, nominal value is "
                              << fDefaultParameters[cADCsMapIterator->second] << " V" << RESET;
                    cReturn = false;
                }
                else
                {
                    LOG(INFO) << BOLDGREEN << "Match in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << cADCValue * cConversionFactor << " V, nominal value is "
                              << fDefaultParameters[cADCsMapIterator->second] << " V" << RESET;
                }

                cADCsMapIterator++;
                cBinCount++;

            } while(cADCsMapIterator != cADCsMap.end());
        }
    }
    auto cADCCanvas = new TCanvas("tFixedADCs", "lpGBT ADCs not tied to AMUX", 1600, 900);
    cADCCanvas->SetRightMargin(0.2);
    cADCHistogram->GetXaxis()->SetTitle("ADC channel");
    cADCHistogram->GetYaxis()->SetTitle("ADC count");

    cADCHistogram->Draw("colz");
    cADCCanvas->Write();
#endif
    return cReturn;
} */

/* bool SEHTester::ToyTestFixedADCs()
{
    bool cReturn;
#ifdef __USE_ROOT__
    auto                               cFixedADCsTree   = new TTree("ToyFixedADCs", "ToylpGBT ADCs not tied to AMUX");
     gStyle->SetOptStat(0);
    auto cADCHistogram= new TH2I("cToyADCHistogram","Toy Fixed ADC Histogram",6,0,6,1024,0,1024);

    cADCHistogram->GetZaxis()->SetTitle("Number of entries");
    std::map<std::string, std::string> cADCsMap         = {{"VMON_P1V25_L", "VMON_P1V25_L_Nominal"},
                                                   {"VMIN", "VMIN_Nominal"},
                                                   {"TEMPP","TEMPP_Nominal"},
                                                   {"VTRX+_RSSI_ADC", "VTRX+_RSSI_ADC_Nominal"},
                                                   {"PTAT_BPOL2V5", "PTAT_BPOL2V5_Nominal"},
                                                   {"PTAT_BPOL12V", "PTAT_BPOL12V_Nominal"}};
    auto                               cADCsMapIterator = cADCsMap.begin();
    float                                cADCValue;
    int cBinCount=1;
    float                              cConversionFactor = 1. / 1024.;
    std::vector<float> cADCValueVect;
    fillSummaryTree("ADC conversion factor", cConversionFactor);
    auto gRandom = new TRandom3();
        do
            {
                cADCValueVect.clear();
                cADCHistogram->GetXaxis()->SetBinLabel(cBinCount,cADCsMapIterator->first.c_str());

                for (int cIteration=0;cIteration<1000;++cIteration)
                {
                    cADCValue = gRandom->Gaus(550.0, 50.0);
                    cADCValueVect.push_back(cADCValue);
                    cADCHistogram->Fill(cADCsMapIterator->first.c_str(),cADCValue,1);
                }
                    // fTC_2SSEH->read_supply(c2SSEHMapIterator->second, k);

                    fillSummaryTree(cADCsMapIterator->first, cADCValue * cConversionFactor);
                    float cDifference_V = std::fabs((fDefaultParameters[cADCsMapIterator->second]) - cADCValue * cConversionFactor);

                // Still hard coded threshold for imidiate boolean result, actual values are stored
                if(cDifference_V > 0.1)
                {
                    LOG(INFO) << BOLDRED << "Mismatch in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << cADCValue * cConversionFactor << " V, nominal value is "
                              << fDefaultParameters[cADCsMapIterator->second] << " V" << RESET;
                    cReturn = false;
                }
                else
                {
                    LOG(INFO) << BOLDGREEN << "Match in fixed ADC channel " << cADCsMapIterator->first << " measured value is " << cADCValue * cConversionFactor << " V, nominal value is "
                              << fDefaultParameters[cADCsMapIterator->second] << " V" << RESET;
                }

                cADCsMapIterator++;cBinCount++;

            } while(cADCsMapIterator != cADCsMap.end());

    auto cToyADCCanvas = new TCanvas("tToyFixedADCs", "ToylpGBT ADCs not tied to AMUX", 1600, 900);
    cToyADCCanvas->SetRightMargin(0.2);
    cADCHistogram->GetXaxis()->SetTitle("ADC channel");
    cADCHistogram->GetYaxis()->SetTitle("ADC count");

    cADCHistogram->Draw("colz");
    cToyADCCanvas->Write();
#endif

    return cReturn;
} */

void SEHTester::TestCardVoltages()
{
#ifdef __TCUSB__
#ifdef __SEH_USB__
    float k;
    auto  c2SSEHMapIterator = f2SSEHSupplyMeasurements.begin();
    do
    {
#ifdef __TCP_SERVER__
        k = this->getMeasurement("read_supply:" + c2SSEHMapIterator->first);
#else
        fTC_USB->read_supply(c2SSEHMapIterator->second, k);
#endif
#ifdef __USE_ROOT__
        fillSummaryTree(c2SSEHMapIterator->first, k);
#endif
        c2SSEHMapIterator++;

    } while(c2SSEHMapIterator != f2SSEHSupplyMeasurements.end());
    // fTC_USB->set_SehSupply(fTC_USB->sehSupply_On);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    auto d2SSEHMapIterator = f2SSEHSupplyMeasurements.begin();
    do
    {
#ifdef __TCP_SERVER__
        k = this->getMeasurement("read_supply:" + d2SSEHMapIterator->first);
#else
        fTC_USB->read_supply(d2SSEHMapIterator->second, k);
#endif
#ifdef __USE_ROOT__
        fillSummaryTree(d2SSEHMapIterator->first, k);
#endif
        d2SSEHMapIterator++;

    } while(d2SSEHMapIterator != f2SSEHSupplyMeasurements.end());
    // fTC_USB->set_SehSupply(fTC_USB->sehSupply_Off);
#endif
#endif
}

void SEHTester::DCDCOutputEvaluation()
{
#ifdef __USE_ROOT__
#ifdef __TCUSB__
#ifdef __SEH_USB__
    std::map<std::string, TC_2SSEH::loadMeasurement> c2SSEHOutputVoltageMeasurements = {
        {"U_P1V2_R", TC_2SSEH::loadMeasurement::U_P1V2_R}, {"U_P1V2_L", TC_2SSEH::loadMeasurement::U_P1V2_L}, {"P2V5_VTRx_MON", TC_2SSEH::loadMeasurement::P2V5_VTRx_MON}};
    std::vector<float> cDCDCValueVect;
    float              cDCDCValue;
    auto               cDCDCOutputTree  = new TTree("DCDCOutput", "lpGBT ADCs not tied to AMUX");
    auto               cDCDCMapIterator = c2SSEHOutputVoltageMeasurements.begin();
    gStyle->SetOptStat(0);

    auto cStackedHistogramm = new THStack("cDCDCOutput", "DC/DC Output Voltages");
    int  cIt                = 0;
    // auto gRandom            = new TRandom3();
    do
    {
        cDCDCOutputTree->Branch(cDCDCMapIterator->first.c_str(), &cDCDCValueVect);
        auto cHistogramm = new TH1F(cDCDCMapIterator->first.c_str(), cDCDCMapIterator->first.c_str(), 30, 0, 3);
        cHistogramm->SetFillColor(cIt + 1);
        cHistogramm->SetMarkerStyle(cIt + 21);
        cHistogramm->SetMarkerColor(cIt + 1);
        cDCDCValueVect.clear();
        for(int cIteration = 0; cIteration < 10; ++cIteration)
        {
#ifdef __TCP_SERVER__
            cDCDCValue = this->getMeasurement("read_load:" + cDCDCMapIterator->first);
#else
            fTC_USB->read_load(cDCDCMapIterator->second, cDCDCValue);
#endif
            // cDCDCValue += gRandom->Rndm();
            cDCDCValueVect.push_back(cDCDCValue);
            cHistogramm->Fill(cDCDCValue);
            std::this_thread::sleep_for(std::chrono::milliseconds(1200));
        }
        cStackedHistogramm->Add(cHistogramm);

        cDCDCOutputTree->Fill();
        cDCDCMapIterator++;
        cIt++;
    } while(cDCDCMapIterator != c2SSEHOutputVoltageMeasurements.end());

    auto cDCDCOutputCanvas = new TCanvas("cDCDcOutput", "DC DC Output voltages", 10, 10, 700, 700);
    gPad->SetGrid();
    cStackedHistogramm->Draw("PLC nostack");
    cStackedHistogramm->GetXaxis()->SetTitle("Output Voltage");
    cStackedHistogramm->GetYaxis()->SetTitle("Count");
    cDCDCOutputCanvas->BuildLegend();
    fResultFile->cd();
    cDCDCOutputCanvas->Write();
    cDCDCOutputTree->Write();
#endif
#endif
#endif
}

void SEHTester::UserFCMDTranslate(const std::string& userFilename = "fcmd_file.txt")
{
    const std::string cUserFilenameFull    = "fcmd_files/user_files/" + userFilename;
    const std::string cRefFCMDFilenameFull = "fcmd_files/" + userFilename;

    std::ifstream            cFCMDUserFileHandle(cUserFilenameFull);
    std::vector<std::string> cUserRequests;
    std::string              cLine;
    while(std::getline(cFCMDUserFileHandle, cLine))
    {
        boost::trim_right(cLine);
        cUserRequests.push_back(cLine);
    }

    std::map<int, std::string> cFCMDvsBX;
    std::vector<std::string>   tokens;
    for(auto cUserRequest: cUserRequests)
    {
        boost::split(tokens, cUserRequest, boost::is_any_of(" "));
        int        cBX   = std::atoi(tokens[0].c_str());
        const auto cFCMD = std::string(tokens[1]);
        cFCMDvsBX[cBX]   = std::string("101") + cFCMD + std::string("0");
        // cFCMDvsBX[cBX] = std::string("110")+cFCMD+std::string("1");
    }

    int cMaxNumBXs = -1;
    for(auto cItem: cFCMDvsBX) cMaxNumBXs = (cItem.first > cMaxNumBXs) ? cItem.first : cMaxNumBXs;

    std::ofstream cRefFCMDHandle(cRefFCMDFilenameFull);
    for(int cBXNum = 1; cBXNum < cMaxNumBXs + 1; ++cBXNum)
    {
        auto cIt   = cFCMDvsBX.find(cBXNum);
        auto cFCMD = (cIt == cFCMDvsBX.end()) ? "11000001" : cIt->second;
        cRefFCMDHandle << cFCMD << std::endl;
    }
    cRefFCMDHandle.close();
}

void SEHTester::ClearBRAM(BeBoard* pBoard, const std::string& sBRAMToReset)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    std::string cRegNameData;
    std::string cRegNameAddr;
    std::string cRegNameWrite;
    if(sBRAMToReset == std::string("ref"))
    {
        cRegNameData  = "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.ref_data_to_bram";
        cRegNameAddr  = "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.ref_data_bram_addr";
        cRegNameWrite = "fc7_daq_ctrl.physical_interface_block.fe_for_ps_roh.write_ref_fcmd_to_bram";
    }
    else if(sBRAMToReset == std::string("test"))
    {
        cRegNameData  = "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_check.test_data_to_bram";
        cRegNameAddr  = "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_check.test_data_bram_addr";
        cRegNameWrite = "fc7_daq_ctrl.physical_interface_block.fe_for_ps_roh.write_test_fcmd_to_bram";
    }
    for(unsigned int cBRAMAddress = 0; cBRAMAddress < NBRAMADDR; ++cBRAMAddress)
    {
        fBeBoardInterface->WriteBoardReg(pBoard, cRegNameData.c_str(), 0x00);
        fBeBoardInterface->WriteBoardReg(pBoard, cRegNameAddr, cBRAMAddress);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fBeBoardInterface->WriteBoardReg(pBoard, cRegNameWrite, 0x01);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void SEHTester::ClearBRAM(const std::string& sBramToReset)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->ClearBRAM(cBoard, sBramToReset);
    }
}

void SEHTester::WritePatternToBRAM(BeBoard* pBoard, const std::string& filename = "fcmd_file.txt")
{
    fBeBoardInterface->setBoard(pBoard->getId());
    //        this -> UserFCMDTranslate(filename);
    this->ClearBRAM("ref");
    bool             cIsSSAlFCMDBRAMGood = true;
    bool             cIsSSArFCMDBRAMGood = true;
    bool             cIsCIClFCMDBRAMGood = true;
    bool             cIsCICrFCMDBRAMGood = true;
    std::vector<int> cFailedAddrSSAl;
    std::vector<int> cFailedAddrSSAr;
    std::vector<int> cFailedAddrCICl;
    std::vector<int> cFailedAddrCICr;

    const std::string cRefFCMDFilenameFull = "fcmd_files/" + filename;
    std::ifstream     cUserHandle(cRefFCMDFilenameFull);
    std::string       cLine;
    int               cBRAMAddress = 0;
    while(std::getline(cUserHandle, cLine))
    {
        // std::cout << cBRAMAddress << std::endl;
        // std::cout << std::atoi(cLine.c_str()) << " " << std::stoi(cLine.c_str(),nullptr,2) << std::endl;
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.ref_data_to_bram", std::stoi(cLine.c_str(), nullptr, 2));
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.ref_data_bram_addr", cBRAMAddress);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.fe_for_ps_roh.write_ref_fcmd_to_bram", 0x01);

        // Verify write operation is correct
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        int cRefSSAlFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_l_ref");
        // std::cout << "SSAl:" << cRefSSAlFCMDBRAMData << " <-> " << cLine << std::endl;
        if(cRefSSAlFCMDBRAMData != std::stoi(cLine.c_str(), nullptr, 2))
        {
            cFailedAddrSSAl.push_back(cBRAMAddress);
            cIsSSAlFCMDBRAMGood = false;
        }

        int cRefSSArFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_r_ref");
        // std::cout << "SSAr:" << cRefSSArFCMDBRAMData << " <-> " << cLine << std::endl;
        if(cRefSSArFCMDBRAMData != std::stoi(cLine.c_str(), nullptr, 2))
        {
            cFailedAddrSSAr.push_back(cBRAMAddress);
            cIsSSArFCMDBRAMGood = false;
        }

        int cRefCIClFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_l_ref");
        // std::cout << "CICl:" << cRefCIClFCMDBRAMData << " <-> " << cLine << std::endl;
        if(cRefCIClFCMDBRAMData != std::stoi(cLine.c_str(), nullptr, 2))
        {
            cFailedAddrCICl.push_back(cBRAMAddress);
            cIsCIClFCMDBRAMGood = false;
        }

        int cRefCICrFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_r_ref");
        // std::cout << "CICr:" << cRefCICrFCMDBRAMData << " <-> " << cLine << std::endl;
        if(cRefCICrFCMDBRAMData != std::stoi(cLine.c_str(), nullptr, 2))
        {
            cFailedAddrCICr.push_back(cBRAMAddress);
            cIsCICrFCMDBRAMGood = false;
        }

        cBRAMAddress++;
    }
    if(cIsSSAlFCMDBRAMGood)
        LOG(INFO) << "SSA l reference FCMD writing to BRAM ->" << BOLDGREEN << " Successful" << RESET;
    else
    {
        LOG(ERROR) << "SSA l reference FCMD writing to BRAM ->" << BOLDRED << " Failed" << RESET;
        LOG(INFO) << "Failed address ";
        std::stringstream cssFailedAddrList;
        for(auto el: cFailedAddrSSAl) cssFailedAddrList << el << " ";
        LOG(INFO) << BOLDBLUE << cssFailedAddrList.str() << RESET;
    }

    if(cIsSSArFCMDBRAMGood)
        LOG(INFO) << "SSA r reference FCMD writing to BRAM ->" << BOLDGREEN << " Successful" << RESET;
    else
    {
        LOG(ERROR) << "SSA r reference FCMD writing to BRAM ->" << BOLDRED << " Failed" << RESET;
        LOG(INFO) << "Failed address ";
        std::stringstream cssFailedAddrList;
        for(auto el: cFailedAddrSSAr) cssFailedAddrList << el << " ";
        LOG(INFO) << BOLDBLUE << cssFailedAddrList.str() << RESET;
    }

    if(cIsCIClFCMDBRAMGood)
        LOG(INFO) << "CIC l reference FCMD writing to BRAM ->" << BOLDGREEN << " Successful" << RESET;
    else
    {
        LOG(ERROR) << "CIC l reference FCMD writing to BRAM ->" << BOLDRED << " Failed" << RESET;
        LOG(INFO) << "Failed address ";
        std::stringstream cssFailedAddrList;
        for(auto el: cFailedAddrCICl) cssFailedAddrList << el << " ";
        LOG(INFO) << BOLDBLUE << cssFailedAddrList.str() << RESET;
    }

    if(cIsCICrFCMDBRAMGood)
        LOG(INFO) << "CIC r reference FCMD writing to BRAM ->" << BOLDGREEN << " Successful" << RESET;
    else
    {
        LOG(ERROR) << "CIC r reference FCMD writing to BRAM ->" << BOLDRED << " Failed" << RESET;
        LOG(INFO) << "Failed address ";
        std::stringstream cssFailedAddrList;
        for(auto el: cFailedAddrCICr) cssFailedAddrList << el << " ";
        LOG(INFO) << BOLDBLUE << cssFailedAddrList.str() << RESET;
    }
}

void SEHTester::WritePatternToBRAM(const std::string& sFileName = "fcmd_file.txt")
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->WritePatternToBRAM(cBoard, sFileName);
    }
}

void SEHTester::CheckFastCommandsBRAM(BeBoard* pBoard, const std::string& sFCMDLine)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    std::string                     cOutputErrorsFileName = sFCMDLine;
    std::ofstream                   cBRAMErrorsFileHandle(cOutputErrorsFileName);
    std::map<int, std::vector<int>> cPatterns;
    for(int cBRAMAddress = 0; cBRAMAddress < NBRAMADDR; ++cBRAMAddress)
    {
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_check.test_data_bram_addr", cBRAMAddress);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        std::string cRegName("fc7_daq_stat.physical_interface_block.");
        cRegName += sFCMDLine;
        int              cCheckFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, cRegName.c_str());
        std::vector<int> temp;
        auto             cIt = cPatterns.find(cCheckFCMDBRAMData);
        if(cIt != cPatterns.end()) temp = cIt->second;
        temp.push_back(cBRAMAddress);
        cPatterns[cCheckFCMDBRAMData] = temp;
        cBRAMErrorsFileHandle << std::setw(10) << cBRAMAddress << std::setw(10) << std::bitset<8>(cCheckFCMDBRAMData) << std::endl;
    }
    cBRAMErrorsFileHandle.close();

    LOG(INFO) << BOLDBLUE << "Patterns: " << RESET;
    for(auto cIt: cPatterns)
    {
        LOG(INFO) << BOLDBLUE << std::bitset<8>(cIt.first) << " appears " << cIt.second.size() << " times " << RESET;
        std::stringstream csCorruptedAddrList;
        for(auto el: cIt.second) csCorruptedAddrList << el << " ";
        LOG(INFO) << BOLDBLUE << "Addresses list: " << csCorruptedAddrList.str() << RESET;
    }
}

void SEHTester::CheckFastCommandsBRAM(const std::string& sFCMDLine)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->CheckFastCommandsBRAM(cBoard, sFCMDLine);
    }
}

void SEHTester::CheckFastCommands(BeBoard* pBoard, const std::string& sFastCommand, const std::string& filename = "fcmd_file.txt")
{
    fBeBoardInterface->setBoard(pBoard->getId());
    this->ClearBRAM("test");
    this->WritePatternToBRAM(pBoard, filename);
    // fcmd test
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.start_pattern", std::stoi(sFastCommand.c_str(), nullptr, 2));
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.fe_for_ps_roh.start_fe_for_ps_roh_fcmd_test", 0x01);

    bool cSSAlFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_l_test_done") == 1);
    bool cSSArFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_r_test_done") == 1);
    bool cCIClFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_l_test_done") == 1);
    bool cCICrFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_r_test_done") == 1);

    LOG(INFO) << GREEN << "============================" << RESET;
    LOG(INFO) << BOLDGREEN << "Fast commands test" << RESET;

    LOG(INFO) << "Waiting for FCMD test";
    const auto MAXNRETRY = 100;
    auto       NTrials   = 0;
    while(!cSSAlFCMDCheckDone && NTrials < MAXNRETRY)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cSSAlFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_l_test_done") == 1);
        NTrials++;
    }
    if(cSSAlFCMDCheckDone)
    {
        bool SSAlFCMDStat = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_l_stat");
        if(SSAlFCMDStat) { LOG(INFO) << "SSA l FCMD test ->" << BOLDGREEN << " PASSED" << RESET; }
        else
        {
            LOG(ERROR) << "SSA l FCMD test ->" << BOLDRED << " FAILED" << RESET;
            this->CheckFastCommandsBRAM(pBoard, std::string("fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_l_check"));
        }
    }
    else
        LOG(INFO) << "SSA l FCMD test ->" << BOLDGREEN << " time out" << RESET;

    NTrials = 0;
    while(!cSSArFCMDCheckDone && NTrials < MAXNRETRY)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cSSArFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_r_test_done") == 1);
        NTrials++;
    }
    if(cSSArFCMDCheckDone)
    {
        bool SSArFCMDStat = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_SSA_r_stat");
        if(SSArFCMDStat) { LOG(INFO) << "SSA r FCMD test ->" << BOLDGREEN << " PASSED" << RESET; }
        else
        {
            LOG(ERROR) << "SSA r FCMD test ->" << BOLDRED << " FAILED" << RESET;
            this->CheckFastCommandsBRAM(pBoard, std::string("fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_r_check"));
        }
    }
    else
        LOG(INFO) << "SSA r FCMD test ->" << BOLDGREEN << " time out" << RESET;

    NTrials = 0;
    while(!cCIClFCMDCheckDone && NTrials < MAXNRETRY)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cCIClFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_l_test_done") == 1);
        NTrials++;
    }
    if(cCIClFCMDCheckDone)
    {
        bool CIClFCMDStat = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_l_stat");
        if(CIClFCMDStat) { LOG(INFO) << "CIC l FCMD test ->" << BOLDGREEN << " PASSED" << RESET; }
        else
        {
            LOG(ERROR) << "CIC l FCMD test ->" << BOLDRED << " FAILED" << RESET;
            this->CheckFastCommandsBRAM(pBoard, std::string("fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_l_check"));
        }
    }
    else
        LOG(INFO) << "CIC l FCMD test ->" << BOLDGREEN << " time out" << RESET;

    NTrials = 0;
    while(!cCICrFCMDCheckDone && NTrials < MAXNRETRY)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        cCICrFCMDCheckDone = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_r_test_done") == 1);
        NTrials++;
    }
    if(cCICrFCMDCheckDone)
    {
        bool CICrFCMDStat = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_data_player.fe_for_ps_roh_fcmd_CIC_r_stat");
        if(CICrFCMDStat) { LOG(INFO) << "CIC r FCMD test ->" << BOLDGREEN << " PASSED" << RESET; }
        else
        {
            LOG(ERROR) << "CIC r FCMD test ->" << BOLDRED << " FAILED" << RESET;
            this->CheckFastCommandsBRAM(pBoard, std::string("fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_r_check"));
        }
    }
    else
        LOG(INFO) << "CIC r FCMD test ->" << BOLDGREEN << " time out" << RESET;

    LOG(INFO) << GREEN << "============================" << RESET;
}

void SEHTester::CheckFastCommands(const std::string& sFastCommand, const std::string& filename = "fcmd_file.txt")
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->CheckFastCommands(cBoard, sFastCommand, filename);
    }
}

void SEHTester::ReadRefAddrBRAM(BeBoard* pBoard, int iRefBRAMAddr)
{
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_test.ref_data_bram_addr", iRefBRAMAddr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int cRefSSAlFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_l_ref");
    LOG(INFO) << BOLDGREEN << "SSA l " << cRefSSAlFCMDBRAMData << RESET;

    int cRefSSArFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_r_ref");
    LOG(INFO) << BOLDGREEN << "SSA r " << cRefSSArFCMDBRAMData << RESET;

    int cRefCIClFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_l_ref");
    LOG(INFO) << BOLDGREEN << "CIC l " << cRefCIClFCMDBRAMData << RESET;

    int cRefCICrFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_r_ref");
    LOG(INFO) << BOLDGREEN << "CIC r " << cRefCICrFCMDBRAMData << RESET;
}
void SEHTester::ReadRefAddrBRAM(int iRefBRAMAddr)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->ReadRefAddrBRAM(cBoard, iRefBRAMAddr);
    }
}

void SEHTester::ReadCheckAddrBRAM(BeBoard* pBoard, int iCheckBRAMAddr)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.fe_for_ps_roh_fcmd_check.test_data_bram_addr", iCheckBRAMAddr);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    int cCheckSSAlFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_l_check");
    LOG(INFO) << BOLDGREEN << "SSA l " << cCheckSSAlFCMDBRAMData << RESET;

    int cCheckSSArFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_ssa_fcmd_test.fe_for_ps_roh_fcmd_SSA_r_check");
    LOG(INFO) << BOLDGREEN << "SSA r " << cCheckSSArFCMDBRAMData << RESET;

    int cCheckCIClFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_l_check");
    LOG(INFO) << BOLDGREEN << "CIC l " << cCheckCIClFCMDBRAMData << RESET;

    int cCheckCICrFCMDBRAMData = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fe_for_ps_roh_cic_fcmd_test.fe_for_ps_roh_fcmd_CIC_r_check");
    LOG(INFO) << BOLDGREEN << "CIC r " << cCheckCICrFCMDBRAMData << RESET;
}

void SEHTester::ReadCheckAddrBRAM(int iCheckBRAMAddr)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->ReadCheckAddrBRAM(cBoard, iCheckBRAMAddr);
    }
}

void SEHTester::CheckClocks(BeBoard* pBoard)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    // clk test
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.multiplexing_bp.check_return_clock", 0x01);
    auto cMapIterator = f2SSEHClockMap.begin();
    bool cClkTestDone;
    bool cClkStat;

    LOG(INFO) << GREEN << "============================" << RESET;
    LOG(INFO) << BOLDGREEN << "Clock test" << RESET;

    do
    {
        cClkTestDone = (fBeBoardInterface->ReadBoardReg(pBoard, cMapIterator->second + "_test_done") == 1);
        LOG(INFO) << "Waiting for clock test";
        while(!cClkTestDone)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            cClkTestDone = (fBeBoardInterface->ReadBoardReg(pBoard, cMapIterator->second + "_test_done") == 1);
        }
        if(cClkTestDone)
        {
            cClkStat = fBeBoardInterface->ReadBoardReg(pBoard, cMapIterator->second + "_stat");

            if(cClkStat)
                LOG(INFO) << cMapIterator->first << " test ->" << BOLDGREEN << " PASSED" << RESET;
            else
                LOG(ERROR) << cMapIterator->first << " test ->" << BOLDRED << " FAILED" << RESET;
#ifdef __USE_ROOT__
            fillSummaryTree(cMapIterator->first, cClkStat);
#endif
        }
        cMapIterator++;
    } while(cMapIterator != f2SSEHClockMap.end());
}

void SEHTester::CheckClocks()
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->CheckClocks(cBoard);
    }
}
void SEHTester::FastCommandScope(BeBoard* pBoard)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    // uint32_t cSSA_L = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_ssa_l");
    uint32_t cCIC_R = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_ssa_r");
    uint32_t cCIC_L = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_cic_l");
    // uint32_t cCIC_R = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_cic_r");

    // LOG(INFO) << BOLDBLUE << "Scoped output on SSA_L : " << std::bitset<32>(cSSA_L) << RESET;
    // LOG(INFO) << BOLDBLUE << "Scoped output on SSA_R : " << std::bitset<32>(cSSA_R) << RESET;
    LOG(INFO) << BOLDBLUE << "Scoped output on CIC_L : " << std::bitset<32>(cCIC_L) << RESET;
    LOG(INFO) << BOLDBLUE << "Scoped output on CIC_R : " << std::bitset<32>(cCIC_R) << RESET;
}
bool SEHTester::FastCommandChecker(BeBoard* pBoard, uint8_t pPattern)
{
    fBeBoardInterface->setBoard(pBoard->getId());
    // uint32_t cSSA_L = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_ssa_l");
    uint32_t cCIC_R = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_ssa_r");
    uint32_t cCIC_L = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_cic_l");
    // uint32_t cCIC_R = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.fcmd_debug_cic_r");

    // LOG(INFO) << BOLDBLUE << "Scoped output on SSA_L : " << std::bitset<32>(cSSA_L) << RESET;
    // LOG(INFO) << BOLDBLUE << "Scoped output on SSA_R : " << std::bitset<32>(cSSA_R) << RESET;
    LOG(INFO) << BOLDBLUE << "Scoped output on CIC_L : " << std::bitset<32>(cCIC_L) << RESET;
    LOG(INFO) << BOLDBLUE << "Scoped output on CIC_R : " << std::bitset<32>(cCIC_R) << RESET;
    LOG(INFO) << BOLDBLUE << "Checking against : " << std::bitset<8>(pPattern) << RESET;
    uint8_t  cWrappedByte;
    uint32_t cWrappedData;
    uint8_t  cMatchR = 32;
    uint8_t  cShiftR = 0;
    uint8_t  cMatchL = 32;
    uint8_t  cShiftL = 0;
    for(uint8_t shift = 0; shift < 8; shift++)
    {
        cWrappedByte = (pPattern >> shift) | (pPattern << (8 - shift));
        cWrappedData = (cWrappedByte << 24) | (cWrappedByte << 16) | (cWrappedByte << 8) | (cWrappedByte << 0);
        LOG(INFO) << BOLDBLUE << std::bitset<8>(cWrappedByte) << RESET;
        LOG(INFO) << BOLDBLUE << std::bitset<32>(cWrappedData) << RESET;
        int popcountR = __builtin_popcountll(cWrappedData ^ cCIC_R);
        int popcountL = __builtin_popcountll(cWrappedData ^ cCIC_L);
        if(popcountR < cMatchR)
        {
            cMatchR = popcountR;
            cShiftR = shift;
        }
        if(popcountL < cMatchL)
        {
            cMatchL = popcountL;
            cShiftL = shift;
        }
        LOG(INFO) << BOLDBLUE << "Loop " << +shift << " MatchL " << +popcountL << " MatchR " << +popcountR << RESET;
    }
    LOG(INFO) << BOLDBLUE << "Found for CIC_L a minimal bit difference of " << +cMatchL << " for a bit shift of " << +cShiftL << RESET;
    LOG(INFO) << BOLDBLUE << "Found for CIC_R a minimal bit difference of " << +cMatchR << " for a bit shift of " << +cShiftR << RESET;

#ifdef __USE_ROOT__
    fillSummaryTree("FCMD_CIC_R_match", cMatchR);
    fillSummaryTree("FCMD_CIC_L_match", cMatchL);
    fillSummaryTree("FCMD_CIC_R_shift", cShiftR);
    fillSummaryTree("FCMD_CIC_L_shift", cShiftL);
#endif
    if((cMatchR == 0) & (cMatchL == 0))
    {
        LOG(INFO) << BOLDGREEN << "FCMD Test passed" << RESET;
        return true;
    }
    else
    {
        LOG(INFO) << BOLDRED << "FCMD Test failed" << RESET;
        return false;
    }
}
void SEHTester::FastCommandScope()
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->FastCommandScope(cBoard);
    }
}
bool SEHTester::FastCommandChecker(uint8_t pPattern)
{
    bool re = false;
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        re = this->FastCommandChecker(cBoard, pPattern);
    }
    return re;
}
void SEHTester::CheckHybridInputs(BeBoard* pBoard, std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    fBeBoardInterface->setBoard(pBoard->getId());
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
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // stop
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // check counters
    pCounters.clear();
    pCounters.resize(cIndices.size());
    for(auto cIndex: cIndices)
    {
        char cBuffer[19];
        sprintf(cBuffer, "debug_blk_counter%02d", cIndex);
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}
void SEHTester::CheckHybridInputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->CheckHybridInputs(cBoard, pInputs, pCounters);
    }
}

void SEHTester::CheckHybridOutputs(BeBoard* pBoard, std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters)
{
    fBeBoardInterface->setBoard(pBoard->getId());
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
        char cBuffer[19];
        sprintf(cBuffer, "debug_blk_counter%02d", cIndex);
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}

void SEHTester::SEHInputsDebug()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_input", 0x00003FFF);
        // start
        LOG(INFO) << BOLDBLUE << "Do you want to start test? [y/n]" << RESET;
        char Answer;
        std::cin >> Answer;
        if(Answer == 'y') { fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_input", 1); }
        else if(Answer == 'n')
        {
            exit(1);
        }
        else
        {
            LOG(ERROR) << "Wrong option!" << std::endl;
            exit(1);
        }
        // stop
        LOG(INFO) << BOLDBLUE << "Do you want to stop test? [y/n]" << RESET;
        std::cin >> Answer;
        while(Answer != 'y') { LOG(INFO) << BOLDBLUE << "Do you want to stop test? " << RESET; }
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        // results
        LOG(INFO) << BOLDBLUE << "Input lines debug done:" << fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.physical_interface_block.input_lines_debug_done");
        LOG(INFO) << BOLDBLUE << "Results for line:" << RESET;
        std::vector<std::string> RegisterTable = {{"fc7_daq_stat.physical_interface_block.debug_blk_counter00"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter01"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter02"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter03"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter04"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter05"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter06"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter07"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter08"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter09"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter10"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter11"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter12"},
                                                  {"fc7_daq_stat.physical_interface_block.debug_blk_counter13"}};

        std::map<std::string, std::string> RegisterAlias = {{"fc7_daq_stat.physical_interface_block.debug_blk_counter00", "l_fcmd_cic"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter01", "r_fcmd_cic"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter02", "l_fcmd_ssa"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter03", "r_fcmd_ssa"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter04", "l_clk_320"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter05", "r_clk_640"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter06", "l_clk_320"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter07", "r_clk_640"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter08", "l_i2c_scl"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter09", "r_i2c_scl"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter10", "l_i2c_sda_o"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter11", "r_i2c_sda_o"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter12", "cpg"},
                                                            {"fc7_daq_stat.physical_interface_block.debug_blk_counter13", "bpg"}};

        for(const auto& RegName: RegisterTable)
        {
            auto result = fBeBoardInterface->ReadBoardReg(cBoard, RegName.c_str());
            LOG(INFO) << BOLDBLUE << std::setw(20) << RegisterAlias[RegName] << std::setw(10) << result << RESET;
        }
    }
}

void SEHTester::CheckHybridOutputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(cBoard->at(0)->flpGBT != nullptr) continue;
        this->CheckHybridOutputs(cBoard, pInputs, pCounters);
    }
}

void SEHTester::Start(int currentRun)
{
    LOG(INFO) << BOLDBLUE << "Starting 2S SEH Tester" << RESET;
    Initialise();
}

void SEHTester::Stop()
{
    LOG(INFO) << BOLDBLUE << "Stopping 2S SEH Tester" << RESET;
    // writeObjects();
    dumpConfigFiles();
    Destroy();
}

void SEHTester::Pause() {}

void SEHTester::Resume() {}

float SEHTester::PowerSupplyGetMeasurement(std::string name)
{
    LOG(INFO) << BOLDBLUE << name << RESET;
    float value = std::stof(this->getVariableValue("value", name));
    LOG(INFO) << BOLDBLUE << value << RESET;
    return value;
}
std::string SEHTester::getVariableValue(std::string variable, std::string buffer)
{
    size_t begin = buffer.find(variable) + variable.size() + 1;
    size_t end   = buffer.find(',', begin);
    if(end == std::string::npos) end = buffer.size();
    return buffer.substr(begin, end - begin);
}
#ifdef __TCP_SERVER__
float SEHTester::getMeasurement(std::string name)
{
    std::string buffer = fTestcardClient->sendAndReceivePacket(name);
    std::cout << buffer << std::endl;
    float value = std::stof(this->getVariableValue("value", buffer));
    return value;
}

#endif
