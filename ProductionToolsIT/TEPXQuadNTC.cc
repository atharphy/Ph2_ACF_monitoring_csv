/*!
  \file                  TEPXQuadNTC.cc
  \brief                 Read out TEPX Quad NTC
  \author                PSI team
  \version               1.0
  \date                  04/03/2024
  Support:               none
*/

#include "TEPXQuadNTC.h"
#include <vector>

#include "HWInterface/RD53FWInterface.h"
#include "HWInterface/RegManager.h"
#include "System/SystemController.h"
#include "Utils/ConfigureInfo.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/StartInfo.h"
#include "Utils/argvparser.h"

// ##################
// # Default values #
// ##################
#define RUNNUMBER 0
#define FILERUNNUMBER "./RunNumber.txt"
#define BASEDIR "PH2ACF_BASE_DIR"
#define TESTSUBDETECTOR false

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

int main(int argc, char** argv)
{
    // #############################
    // # Initialize command parser #
    // #############################
    CommandLineProcessing::ArgvParser cmd;

    cmd.setIntroductoryDescription("@@@ TEPXQuadNTC @@@");

    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hardware description file", CommandLineProcessing::ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("file", "f");

    int result = cmd.parse(argc, argv);
    if(result != CommandLineProcessing::ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(EXIT_FAILURE);
    }

    // ####################
    // # Retrieve options #
    // ####################
    std::string configFile = cmd.foundOption("file") == true ? cmd.optionValue("file") : "";

    // ########################
    // # Configure the logger #
    // ########################
    el::Configurations conf(std::string(std::getenv(BASEDIR)) + "/settings/logger.conf");
    conf.set(el::Level::Global, el::ConfigurationType::Format, "|%datetime{%h:%m:%s}|%levshort|%msg");
    el::Loggers::reconfigureAllLoggers(conf);

    SystemController mySysCntr;

    // ##################################
    // # Configure the SystemController #
    // ##################################

    // #######################
    // # Initialize Hardware #
    // #######################
    // LOG(INFO) << BOLDMAGENTA << "@@@ Initializing the Hardware @@@" << RESET;
    // ConfigureInfo theConfigureInfo;
    // theConfigureInfo.setConfigurationFiles(configFile, calibSettingsFile);
    // mySysCntr.Configure(theConfigureInfo, !skipcfg);
    // LOG(INFO) << BOLDMAGENTA << "@@@ Hardware initialization done @@@" << RESET;
    // LOG(INFO) << RESET;

    // ###################
    // # Run Calibration #
    // ###################
    // if((program == false) && (dumpRegs == false))
    // {
    //     if(whichCalib == "")
    //         LOG(ERROR) << BOLDRED << "Error: calibration not specified" << RESET;
    //     else
    //         LOG(ERROR) << BOLDRED << "Error: option not recognized (" << BOLDYELLOW << whichCalib << BOLDRED << ")" << RESET;

    //     mySysCntr.Destroy();
    //     exit(EXIT_FAILURE);
    // }
    std::string fileName("Run_TEPXNTC");
    TEPXQuadNTC yo;
    yo.Inherit(&mySysCntr);
    yo.localConfigure(fileName, 0);
    yo.run();

    // ######################################################
    // # Disable all channels and destroy System Controller #
    // ######################################################
    mySysCntr.disableAllChannels();
    mySysCntr.Destroy();

    LOG(INFO) << BOLDMAGENTA << "@@@ End of TEPXQuadNTC @@@" << RESET;

    return EXIT_SUCCESS;
}

// define function for Linear Regression with least mean squares method
std::pair<double, double> LinReg(const std::vector<double>& x, const std::vector<double>& y, bool verbose = false)
{
    int    n = x.size();
    int    i;
    double x_sum = 0, x2_sum = 0, y_sum = 0, xy_sum = 0;

    // iterating through x and y
    for(i = 0; i < n; i++)
    {
        x_sum += x[i];
        y_sum += y[i];
        x2_sum += pow(x[i], 2);
        xy_sum += x[i] * y[i];
    }

    double slope, intercept;
    slope     = (n * xy_sum - x_sum * y_sum) / (n * x2_sum - x_sum * x_sum);
    intercept = (x2_sum * y_sum - x_sum * xy_sum) / (x2_sum * n - x_sum * x_sum);
    return std::make_pair(slope, intercept);
}

float ntc_funct(double voltage, double current)
{
    const float T0C         = 273.15; // [Kelvin]
    const float T25C        = 298.15; // [Kelvin]
    const float R25C        = 10;     // [kOhm]
    const int   beta        = 3435;
    float       resistance  = 1e3 * voltage / current;                                // [kOhm]
    float       temperature = 1. / (1. / T25C + log(resistance / R25C) / beta) - T0C; // [Celsius]
    return temperature;
}

void TEPXQuadNTC::ConfigureCalibration() { LOG(INFO) << GREEN << "[TEPXQuadNTC::ConfigureCalibration]" << RESET; }

void TEPXQuadNTC::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[TEPXQuadNTC::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    TEPXQuadNTC::run();
    // TEPXQuadNTC::analyze();
    // TEPXQuadNTC::sendData();
}

void TEPXQuadNTC::sendData() {}

void TEPXQuadNTC::Stop()
{
    LOG(INFO) << GREEN << "[TEPXQuadNTC::Stop] Stopping" << RESET;

    Tool::Stop();

    TEPXQuadNTC::draw();
    // this->closeFileHandler();

    RD53RunProgress::reset();
}

void TEPXQuadNTC::localConfigure(const std::string& histoFileName, int currentRun) { LOG(INFO) << GREEN << "[TEPXQuadNTC::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET; }
/*
float TEPXQuadNTC::TfromR(float RkOhm){
    // convert resistance to temperature for Murata NTHCG83
    const float a = 673.441, b= -47.219, c=0.076;
    double x = RkOhm *1000.;// convert to Ohm
    return a + b* log(x) + c * pow(log(x),3) - 273.15;

    }
*/

float TEPXQuadNTC::TfromR(float RkOhm)
{
    const float T0C  = 273.15; // [Kelvin]
    const float T25C = 298.15; // [Kelvin]
    const float R25C = 10;     // [kOhm]
    // conversion for NTHCG83, fit to table from Murata
    const int beta = 3380; // Murata B(50/25)
    double    y    = RkOhm - R25C;
    float     RB   = 10.0 + 1.01884 * y + 0.00238757 * pow(y, 2) - 1.5062e-05 * pow(y, 3) + 5.02968e-08 * pow(y, 4);
    return 1. / (1. / T25C + log(RB / R25C) / beta) - T0C; // [Celsius]
}

void TEPXQuadNTC::run()
{
    fVerbose           = true;
    auto chipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    CalibBase::prepareChipQueryForEnDis("chipSubset"); //  ??

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
            {
                std::map<int, double> ntc_temp_from_slope_map;
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "[TEPXQuadNTC::run]  board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET;

                    // raw ADC: for a list of "observables" see RD53BInterface::getADCobservable  in HWInterface/RD53BInterface.cc
                    // create arrays to store values of each chip
                    std::vector<double> ntc_adc_list;
                    std::vector<double> r_ref_list;
                    std::vector<double> dac_ntc_list;
                    // for(unsigned int dac_ntc 100; dac_ntc < 600; dac_ntc += 100){
                    if(fVerbose) { LOG(INFO) << "DAC     ADC_ntc  ADC_ref    R_ntc (kOhm)  T(R_ntc) (C)"; }
                    for(unsigned int dac_ntc = 50; dac_ntc < 300; dac_ntc += 50)
                    {
                        chipInterface->WriteChipReg(cChip, "DAC_NTC", dac_ntc);
                        const auto ntc_adc   = chipInterface->ReadChipADC(cChip, "NTC_VOLT");
                        const auto r_ref_adc = chipInterface->ReadChipADC(cChip, "NTC_CURR");
                        r_ref_list.push_back(r_ref_adc);
                        if(r_ref_adc > 0)
                        {
                            ntc_adc_list.push_back(ntc_adc);
                            dac_ntc_list.push_back(dac_ntc);
                            if(fVerbose)
                            {
                                float R_ntc = fR_ref * ntc_adc / r_ref_adc;
                                LOG(INFO) << std::fixed << std::setw(4) << dac_ntc << " " << std::fixed << std::setw(8) << ntc_adc << " " << std::fixed << std::setw(8) << r_ref_adc << " "
                                          << std::fixed << std::setw(12) << std::setprecision(3) << R_ntc << std::fixed << std::setw(12) << std::setprecision(1) << TfromR(R_ntc);
                            }
                        }
                        else { LOG(INFO) << RED << "ERROR reading ntc" << RESET; }
                    }

                    // for each chip calculate temperature  from slopes of ntc_adc/r_ref_adc values
                    const auto fit_ntc     = LinReg(dac_ntc_list, ntc_adc_list);
                    const auto fit_ref     = LinReg(dac_ntc_list, r_ref_list);
                    double     R_ntc_slope = fR_ref * fit_ntc.first / fit_ref.first;
                    if(fVerbose)
                    {
                        LOG(INFO) << "offset  " << std::fixed << std::setw(8) << std::setprecision(2) << fit_ntc.second << " " << std::fixed << std::setw(8) << std::setprecision(2) << fit_ref.second
                                  << " ";
                        LOG(INFO) << "slope" << std::fixed << std::setw(8) << std::setprecision(2) << fit_ntc.first << " " << std::fixed << std::setw(8) << std::setprecision(2) << fit_ref.first << " "
                                  << std::fixed << std::setw(12) << std::setprecision(3) << R_ntc_slope << std::fixed << std::setw(12) << std::setprecision(1) << TfromR(R_ntc_slope);
                    }
                    ntc_temp_from_slope_map[cChip->getId()] = TfromR(R_ntc_slope);

                    chipInterface->WriteChipReg(cChip, "DAC_NTC", 100);
                }
                LOG(INFO) << "NTC result for ChipID 15-12  (C)   : " << std::setw(8) << std::setprecision(1) << std::fixed << ntc_temp_from_slope_map[15] << std::setw(8) << std::setprecision(1)
                          << std::fixed << ntc_temp_from_slope_map[14] << std::setw(8) << std::setprecision(1) << std::fixed << ntc_temp_from_slope_map[13] << std::setw(8) << std::setprecision(1)
                          << std::fixed << ntc_temp_from_slope_map[12];
            }
    // ##################
    // # Reset sequence #
    // ##################
    LOG(INFO) << "TEPXQuadNTC resetting/reconfiguring board" << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ResetBoard();
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureBoard(cBoard);
        this->ConfigureIT(cBoard);
        this->ConfigureFrontendIT(cBoard);
    }

    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
    LOG(INFO) << "TEPXQuadNTC done" << RESET;
}

void TEPXQuadNTC::draw(bool saveData) { LOG(INFO) << GREEN << "[TEPXQuadNTC::draw]" << RESET; }

void TEPXQuadNTC::analyze() { LOG(INFO) << GREEN << "[TEPXQuadNTC::analyze]" << RESET; }

void TEPXQuadNTC::fillHisto() { LOG(INFO) << GREEN << "[TEPXQuadNTC::fillHisto]" << RESET; }
