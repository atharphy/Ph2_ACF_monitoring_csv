#include <cstring>

#include "HWInterface/D19cDebugFWInterface.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "boost/format.hpp"
#include "tools/BackEndAlignment.h"
#include "tools/CheckCbcNeighbors.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/LatencyScan.h"
#include "tools/LinkTestOT.h"
#include "tools/MemoryCheck2S.h"
#include "tools/OpenFinder.h"
#include "tools/PedeNoise.h"
#include "tools/PedestalEqualization.h"
#include "tools/RegisterTester.h"
#include "tools/ShortFinder.h"
#include "tools/StubBackEndAlignment.h"

#ifdef __POWERSUPPLY__
// Libraries
#include "DeviceHandler.h"
#include "PowerSupply.h"
#include "PowerSupplyChannel.h"
#endif

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#define __NAMEDPIPE__

#ifdef __NAMEDPIPE__
#include "Utils/gui_logger.h"
#endif

#ifdef __ANTENNA__
#include "Antenna.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

#define CHIPSLAVE 4

uint16_t returnRunNumber(std::string cFileName)
{
    std::string   cLine;
    int           cRunNumber = -1;
    std::ifstream cStream(cFileName);
    if(cStream.is_open())
    {
        while(std::getline(cStream, cLine))
        {
            std::istringstream cIStream(cLine);
            cIStream >> cRunNumber;
            LOG(DEBUG) << BOLDMAGENTA << cRunNumber << RESET;
        }
    }
    return (uint16_t)(cRunNumber + 1);
}

std::vector<uint8_t> getArgs(std::string pArgsStr)
{
    std::vector<uint8_t> cSides;
    std::stringstream    cArgsSS(pArgsStr);
    int                  i;
    while(cArgsSS >> i)
    {
        cSides.push_back(i);
        if(cArgsSS.peek() == ',') cArgsSS.ignore();
    };
    return cSides;
}

int main(int argc, char* argv[])
{
#if defined(__TCUSB__) && defined(__USE_ROOT__) & defined(__ANTENNA__)
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Commissioning tool to perform the following procedures:\n-Timing / "
                                   "Latency scan\n-Threshold Scan\n-Stub Latency Scan");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Commission_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    cmd.defineOptionAlternative("tuneOffsets", "t");

    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("scanNoiseTime", "measure pedestal and noise on readout chips connected to CIC.");

    cmd.defineOption("findOpens", "perform latency scan with antenna on UIB", ArgvParser::NoOptionAttribute);
    cmd.defineOption("findShorts", "look for shorts", ArgvParser::NoOptionAttribute);

    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("skipAlignment", "Skip the back-end alignment step for stubs", ArgvParser::NoOptionAttribute);

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("allChan", "Do pedestal and noise measurement using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("hybridId", "Serial Number of mezzanine . Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOption("threshold", "Threshold value to set on chips for open and short finding", ArgvParser::OptionRequiresValue);

    cmd.defineOption("checkData", "Compare injected hits and stubs with output [please provide a comma seperated list of chips to check]", ArgvParser::OptionRequiresValue);

    cmd.defineOption("antennaDelay", "Delay between the antenna pulse and the delay [25 ns]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("latencyRange", "Range of latencies around pulse to scan [25 ns]", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("evaluate", "e");

    cmd.defineOption("withCIC", "With CIC. Default : false", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkClusters", "Check CIC2 sparsification... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkSLink", "Check S-link ... data saved to file ", ArgvParser::OptionRequiresValue);
    cmd.defineOption("checkStubs", "Check Stubs... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadData", "Check ReadData method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check Async readout methods [PS objects only]... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("noiseInjection", "Check noise injection...", ArgvParser::NoOptionAttribute);
    cmd.defineOption("calibrateADC", "Calibrate ADC on lpGBT....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("monitorAMUX", "Calibrate ADC on lpGBT....", ArgvParser::OptionRequiresValue);
    cmd.defineOption("monitorSEH", "Calibrate ADC on lpGBT....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("readIDs", "Read chip ids....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("testTune", "Test tuning ....", ArgvParser::OptionRequiresValue);
    cmd.defineOption("memCheck", "Check memories of the following CBCs", ArgvParser::NoOptionAttribute);
    cmd.defineOption("completeDataCheck", "Complete data check for the following CBCs", ArgvParser::OptionRequiresValue);
    cmd.defineOption("cyclePower", "Cycle Power", ArgvParser::NoOptionAttribute);
    cmd.defineOption("powerState", "Get State of power supply", ArgvParser::NoOptionAttribute);
    cmd.defineOption("registerTest", "Test I2C registers on Chips", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkL1Timing", "Check L1 timing for hybrid# [please provide hybrid number]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("linkTest", "Check data quality on L1/stub data", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkSharedStubs", "Check stubs at boundary between chips", ArgvParser::NoOptionAttribute);
    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        cWithCIC          = (cmd.foundOption("withCIC"));
    bool        cTune             = (cmd.foundOption("tuneOffsets"));
    bool        cMeasurePedeNoise = (cmd.foundOption("measurePedeNoise"));
    bool        cFindOpens        = (cmd.foundOption("findOpens")) ? true : false;
    bool        cShortFinder      = (cmd.foundOption("findShorts")) ? true : false;
    bool        batchMode         = (cmd.foundOption("batch")) ? true : false;
    bool        cAllChan          = (cmd.foundOption("allChan")) ? true : false;
    bool        cCheckData        = (cmd.foundOption("checkData"));

    bool cSaveToFile = cmd.foundOption("save");

    uint32_t      cThreshold = (cmd.foundOption("threshold")) ? convertAnyInt(cmd.optionValue("threshold").c_str()) : 560;
    std::string   cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "Skeleton";
    std::string   cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    auto          cRunNumber = returnRunNumber("RunNumbers.dat");
    std::ofstream cRunLog;
    cRunLog.open("RunNumbers.dat", std::fstream::app);
    cRunLog << cRunNumber << "\n";
    cRunLog.close();
    LOG(INFO) << BOLDBLUE << "Run number is " << +cRunNumber << RESET;
    cDirectory += Form("FEH_2S_%s_Run%d", cHybridId.c_str(), cRunNumber);

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;
    Timer       cGlobalTimer;
    cGlobalTimer.start();

    // measure hybrid current and temperature
    // remove for now
    // #ifdef __ANTENNA__
    //     char    cBuffer[120];
    //     Antenna cAntenna;
    //     // cAntenna.setId("UIBV2-CMSPH2-BRD00050");
    //     cAntenna.ConfigureSlaveADC(CHIPSLAVE);
    //     float cTemp    = cAntenna.GetHybridTemperature(CHIPSLAVE);
    //     float cCurrent = cAntenna.GetHybridCurrent(CHIPSLAVE);
    //     sprintf(cBuffer, "Hybrid %s [pre-configuration with default setttings]: temperature reading %.2f °C, current reading %.2f mA", cHybridId.c_str(), cTemp, cCurrent);
    //     LOG(INFO) << BOLDBLUE << cBuffer << RESET;
    // #endif

#ifdef __POWERSUPPLY__
    DeviceHandler                             cPowerSupplyHandler;
    std::vector<std::pair<std::string, bool>> cPowerSupplyChannels;
    std::string                               cPowerSupply = "MyRohdeSchwarz";
    pugi::xml_document                        docSettings;

    cPowerSupplyHandler.readSettings("settings/PSskeleton.xml", docSettings);
    try
    {
        cPowerSupplyHandler.getPowerSupply(cPowerSupply);
    }
    catch(const std::out_of_range& oor)
    {
        std::cerr << "Out of Range error: " << oor.what() << '\n';
        exit(0);
    }
    // Get all channels of the powersupply
    pugi::xml_document doc;
    if(!doc.load_file("settings/PSskeleton.xml")) return -1;
    pugi::xml_node devices = doc.child("Devices");
    for(pugi::xml_node ps = devices.first_child(); ps; ps = ps.next_sibling())
    {
        std::string s(ps.attribute("ID").value());
        if(s == cPowerSupply)
        {
            for(pugi::xml_node channel = ps.child("Channel"); channel; channel = channel.next_sibling("Channel"))
            {
                std::string name(channel.attribute("ID").value());
                std::string use(channel.attribute("InUse").value());

                cPowerSupplyChannels.push_back(std::make_pair(name, use == "Yes"));
            }
        }
    }
    if(cmd.foundOption("cyclePower"))
    {
        LOG(INFO) << BOLDRED << "Turn off all channels : " << cPowerSupply << RESET;
        for(auto channelName: cPowerSupplyChannels)
        {
            if(!channelName.second) continue;
            cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->turnOff();
        }
        std::this_thread::sleep_for(std::chrono::seconds(60));
        LOG(INFO) << BOLDGREEN << "Turn on all channels : " << cPowerSupply << RESET;
        for(auto channelName: cPowerSupplyChannels)
        {
            if(!channelName.second) continue;
            cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->turnOn();
        }
        std::this_thread::sleep_for(std::chrono::seconds(180));
    }
#endif

    std::stringstream outp;
    Tool              cTool;
    if(cSaveToFile)
    {
        char cRawFileName[80];
        std::snprintf(cRawFileName, sizeof(cRawFileName), "Run%.05d.raw", cRunNumber);
        std::string cRawFile = cRawFileName;
        cTool.addFileHandler(cRawFile, 'w');
        LOG(INFO) << BOLDBLUE << "Writing Binary Rawdata to:   " << cRawFile;
    }
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory, false, false);
    cTool.InitResultFile(cResultfile);
    cTool.initializeExceptionHandler();

    std::ofstream cPowerLog;
#ifdef __POWERSUPPLY__
    const auto cStart     = std::chrono::system_clock::now();
    int        cStartTime = (int)std::chrono::duration_cast<std::chrono::seconds>(cStart.time_since_epoch()).count();
    cPowerLog.open(cTool.getDirectoryName() + "/PowerLog.tab", std::ios::app);
    cPowerLog << cStartTime << "\t";
    if(cmd.foundOption("powerState"))
    {
        // Give complete status reoort for all channels in the power supply
        for(auto channelName: cPowerSupplyChannels)
        {
            if(channelName.second)
            {
                LOG(INFO) << BOLDYELLOW << cPowerSupply << " status of channel " << channelName.first << ":" RESET;
                bool        isOn       = cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->isOn();
                std::string isOnResult = isOn ? "1" : "0";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string voltageCompliance = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getVoltageCompliance());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string voltage = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getVoltage());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string currentCompliance = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getCurrentCompliance());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string current = "-";
                if(isOn) { current = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getCurrent()); }
                LOG(INFO) << "\tIsOn:\t\t" << BOLDYELLOW << isOnResult << RESET;
                LOG(INFO) << "\tV_max(set):\t\t" << BOLDYELLOW << voltageCompliance << RESET;
                LOG(INFO) << "\tV(meas):\t" << BOLDYELLOW << voltage << RESET;
                LOG(INFO) << "\tI_max(set):\t" << BOLDYELLOW << currentCompliance << RESET;
                LOG(INFO) << "\tI(meas):\t" << BOLDYELLOW << current << RESET;
                gui::data((channelName.first + ">IsOn").c_str(), isOnResult.c_str());
                gui::data((channelName.first + ">v_max_set").c_str(), voltageCompliance.c_str());
                gui::data((channelName.first + ">v_meas").c_str(), voltage.c_str());
                gui::data((channelName.first + ">i_max_set").c_str(), currentCompliance.c_str());
                gui::data((channelName.first + ">i_meas").c_str(), current.c_str());
                cPowerLog << voltage << "\t" << current << "\t";
            }
        }
        cPowerLog << "\n";
    }
    cPowerLog.close();
#endif
    // for some reason this does not work
    // error I get is new TRootSnifferFull("sniff");
    // cTool.StartHttpServer();
    cTool.ConfigureHw();

    if(cmd.foundOption("calibrateADC"))
    {
        LOG(INFO) << BOLDBLUE << "Calibrating ADC.." << RESET;
        for(const auto cBoard: *cTool.fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                auto& clpGBT = cOpticalGroup->flpGBT;
                if(clpGBT == nullptr) continue;

                // use Vin as the reference
                // this I know does not change with anything
                std::vector<std::string> cADCs_VoltageMonitors{"ADC2"};
                std::vector<float>       cADCs_Refs{10.4 * 0.49 / 10.0};
                size_t                   cIndx = cADCs_VoltageMonitors.size() - 1;
                // static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", (1 << 4 ) );
                // find correction
                std::vector<float>   cVals(10, 0);
                uint8_t              cEnableVref = 1;
                std::string          cADCsel     = cADCs_VoltageMonitors[cIndx];
                std::vector<uint8_t> cRefPoints{0, 0x05, 0x10, 0x20, 0x3F};
                std::vector<float>   cMeasurements(0);
                std::vector<float>   cSlopes(0);
                for(auto cRef: cRefPoints)
                {
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, cRef);
                    // wait until Vref is stable
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel) * CONVERSION_FACTOR; }
                    float cMean         = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                    float cDifference_V = (cADCs_Refs[cIndx] - cMean);
                    // LOG (DEBUG) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT "
                    //         << " correction applied is " << +cRef
                    //         << " reading [mean] is "
                    //         << +cMean*1e3
                    //         << " milli-volts."
                    //         << "\t...Difference between expected and measured "
                    //         << " values is "
                    //         << cDifference_V*1e3
                    //         << " milli-volts." << RESET;
                    cMeasurements.push_back(cDifference_V);
                    if(cMeasurements.size() > 1)
                    {
                        for(int cI = cMeasurements.size() - 2; cI >= 0; cI--)
                        {
                            float cSlope = (cMeasurements[cMeasurements.size() - 1] - cMeasurements[cI]) / (cRefPoints[cMeasurements.size() - 1] - cRefPoints[cI]);
                            LOG(DEBUG) << BOLDBLUE << "Index " << +(cMeasurements.size() - 1) << " -- index " << cI << " slope is " << cSlope << RESET;
                            cSlopes.push_back(cSlope);
                        }
                    }
                }
                float cMeanSlope = std::accumulate(cSlopes.begin(), cSlopes.end(), 0.) / cSlopes.size();
                float cIntcpt    = cMeasurements[0];
                int   cCorr      = std::min(std::floor(-1.0 * cIntcpt / cMeanSlope), 63.);
                // LOG (INFO) << BOLDMAGENTA << "Mean slope is " << cMeanSlope
                //     << " , intercept is " << cIntcpt
                //     << " correction is " << cCorr
                //     << RESET;
                // apply correction and check
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, (uint8_t)cCorr);
                // wait until Vref is stable
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel) * CONVERSION_FACTOR; }
                float cMeanValue = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                LOG(INFO) << BOLDMAGENTA << "Measured V_min after correction is " << std::setprecision(2) << std::fixed << cMeanValue * 1e3 << " mV , expected value is " << cADCs_Refs[cIndx] * 1e3
                          << " difference is " << std::fabs(cMeanValue - cADCs_Refs[cIndx]) * 1e3 << " mV, correction needed to acheive this was  " << +cCorr << RESET;

                // turn off ADC mon
                // static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", 0x00 );
            } // configure lpGBT
        }
    }
    if(cmd.foundOption("monitorSEH"))
    {
        std::vector<std::string> cADCNames{"V_12V5", "V_Min", "PTAT_BPOL2V5", "PTAT_BPOL12V"};
        std::vector<uint8_t>     cADCsels{1, 2, 6, 7};
        LOG(INFO) << BOLDMAGENTA << "Looking at voltages and temperatures on SEH.." << RESET;
        for(const auto cBoard: *cTool.fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                auto& clpGBT = cOpticalGroup->flpGBT;
                if(clpGBT == nullptr) continue;

                for(size_t cIndx = 0; cIndx < cADCsels.size(); cIndx++)
                {
                    std::vector<float> cVals(10);
                    // char               cADC[4];
                    std::string cADC = "ADC" + (boost::format("%|01|") % cADCsels[cIndx]).str();
                    // sprintf(cADC, "ADC%.1d", cADCsels[cIndx]);
                    for(size_t cM = 0; cM < cVals.size(); cM++) cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADC.c_str()) * CONVERSION_FACTOR;
                    float cMean = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                    LOG(INFO) << BOLDMAGENTA << "\t...ADC#" << +cADCsels[cIndx] << " " << cADCNames[cIndx] << " reading from lpGBT "
                              << " is " << +cMean * 1e3 << " milli-volts. " << RESET;
                }
            }
        }
    }

    // read chip ids
    if(cmd.foundOption("readIDs"))
    {
        for(const auto cBoard: *cTool.fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cFusedId = cTool.fReadoutChipInterface->ReadChipReg(cChip, "ChipId");
                        LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cChip->getId() << " CBC#" << +cChip->getId() << " Fused Id is " << +cFusedId << RESET;
                    }
                }
            }
        }
    }

    if(cmd.foundOption("registerTest"))
    {
        RegisterTester cRegTester;
        cRegTester.Inherit(&cTool);
        cRegTester.RegisterTest();
    }

    // // measure hybrid current and temperature
    // #ifdef __ANTENNA__
    //     cTemp    = cAntenna.GetHybridTemperature(CHIPSLAVE);
    //     cCurrent = cAntenna.GetHybridCurrent(CHIPSLAVE);
    //     sprintf(cBuffer, "Hybrid %s [after configuration with default setttings]: temperature reading %.2f °C, current reading %.2f mA", cHybridId.c_str(), cTemp, cCurrent);
    //     LOG(INFO) << BOLDBLUE << cBuffer << RESET;
    //     cAntenna.close();
    // #endif

    if(cmd.foundOption("monitorAMUX"))
    {
        auto cMuxSels = getArgs(cmd.optionValue("monitorAMUX"));
        for(auto cMuxSel: cMuxSels)
        {
            for(const auto cBoard: *cTool.fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& clpGBT = cOpticalGroup->flpGBT;
                    if(clpGBT == nullptr) continue;

                    // enable voltage
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        uint8_t cADCsel = (cHybrid->getId() % 2 == 0) ? 3 : 0;
                        // char    cADC[4];
                        // sprintf(cADC, "ADC%.1d", cADCsel);
                        std::string          cADC = "ADC" + (boost::format("%|01|") % cADCsel).str();
                        std::vector<uint8_t> cChipIds(0);
                        for(auto cChip: *cHybrid) { cChipIds.push_back(cChip->getId()); }
                        for(auto cChipId: cChipIds)
                        {
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, "AmuxOutput", 0);
                                if(cChip->getId() != cChipId) continue;
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, "AmuxOutput", cMuxSel);
                            } // all FEs set to floating, except 0
                            std::vector<float> cVals(10);
                            for(size_t cM = 0; cM < cVals.size(); cM++) cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADC.c_str()) * CONVERSION_FACTOR;
                            float cMean = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                            LOG(INFO) << BOLDMAGENTA << "\t...CBC#" << +cChipId << " " << cADC << " reading from lpGBT "
                                      << " while monitoring AMUX#" << +cMuxSel << " is " << +cMean * 1e3 << " milli-volts. " << RESET;
                        }
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                            cTool.fReadoutChipInterface->WriteChipReg(cChip, "AmuxOutput", 1);
                        } // all FEs set to floating, except 0
                    }     // hybrid
                }         // OG
            }             // board
        }                 // mux sel
    }                     // monitor AMUX

    // Align lpGBT-CIC first
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Initialise();
    cCicAligner.CicLpGbtAlignment();

    // // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Start(cRunNumber);
    cBackEndAligner.waitForRunToBeCompleted();

    // if CIC is enabled then align CIC first
    if(cWithCIC) { cCicAligner.AlignInputs(); }
    cCicAligner.Reset();
    cCicAligner.dumpConfigFiles();

    // time align stubs in back-end
    StubBackEndAlignment cStubBackEndAligner;
    cStubBackEndAligner.Inherit(&cTool);
    cStubBackEndAligner.Initialise();
    if(!cmd.foundOption("skipAlignment"))
    {
        cStubBackEndAligner.FindPackageDelay();
        cStubBackEndAligner.FindStubLatency();
    }
    cStubBackEndAligner.Reset();

    // quickly check the new capture
    if(cmd.foundOption("checkL1Timing"))
    {
        uint8_t              cChipId = (cmd.foundOption("checkL1Timing")) ? convertAnyInt(cmd.optionValue("checkL1Timing").c_str()) : 0;
        std::vector<uint8_t> cHybridIds(0);
        // get unique hybrid Ids
        for(const auto cBoard: *cTool.fDetectorContainer)
        {
            for(const auto cOpticalGroup: *cBoard)
            {
                for(const auto cHybrid: *cOpticalGroup)
                {
                    if(std::find(cHybridIds.begin(), cHybridIds.end(), cHybrid->getId()) != cHybridIds.end()) continue;
                    cHybridIds.push_back(cHybrid->getId());
                }
            }
        }

        for(auto cHybridId: cHybridIds)
        {
            auto cInterface      = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
            auto cDebugInterface = cInterface->getDebugInterface();
            for(const auto cBoard: *cTool.fDetectorContainer)
            {
                cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybridId);
                cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
            }
            LOG(INFO) << BOLDYELLOW << "Scoping L1 data on hybrid" << +cHybridId << RESET;
            cDebugInterface->L1ADebug(1, false);
            for(const auto cBoard: *cTool.fDetectorContainer)
            {
                LOG(INFO) << BOLDMAGENTA << "First header found after " << +cTool.fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_stat.physical_interface_block.slvs_debug.first_header_delay")
                          << " 40 MHz clock cycles." << RESET;
                // size_t cReadBackEvents = cTool.ReadData(cBoard, true);
                // LOG(INFO) << BOLDMAGENTA << "Read-back " << +cReadBackEvents << " events from Board#" << +cBoard->getId() << RESET;
            }
        }
    }

    if(cmd.foundOption("linkTest"))
    {
        LinkTestOT cLinkTest;
        cLinkTest.Inherit(&cTool);
        cLinkTest.TestL1ALines();
    }

    // equalize thresholds on readout chips
    if(cTune)
    {
        // for(auto cBoard: *cTool.fDetectorContainer)
        // {
        //     for(auto cOpticalGroup: *cBoard)
        //     {
        //         for(auto cHybrid: *cOpticalGroup)
        //         {
        //             // set all SSAs + MPAs to output data in async mode
        //             for(auto cChip: *cHybrid)
        //             {
        //                 // TBC - what about MPA here?
        //                 if( cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2 )
        //                 {
        //                     cTool.fReadoutChipInterface->WriteChipReg(cChip, "AnalogueSync", 1);
        //                 }
        //             }
        //         }
        //     }
        //     cTool.fBeBoardInterface->setBoard(cBoard->getId());
        //     auto cInterface = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
        //     cTool.fBeBoardInterface->WriteBoardReg(cBoard,"fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select" , 0) ;
        //     cTool.fBeBoardInterface->WriteBoardReg(cBoard,"fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select" , 0) ;
        //     cInterface->L1ADebug();
        // }

        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        // second parameter disables stub logic on CBC3
        cPedestalEqualization.Initialise(cAllChan, true);
        cPedestalEqualization.FindVplus();
        cPedestalEqualization.FindOffsets();
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show("Time to tune the front-ends on the system: ");
        // // reset
        // cTool.fDetectorContainer->resetReadoutChipQueryFunction();
    }

    // measure noise on FE chips
    if(cMeasurePedeNoise)
    {
        LOG(INFO) << BOLDMAGENTA << "Measuring pedestal and noise" << RESET;
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        // second parameter disables stub logic on CBC3
        // auto myFunction = [](const ChipContainer *theChip){return (theChip->getId()==0);};
        // auto myFunction = [](const ChipContainer *theChip){return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA);};
        // cTool.fDetectorContainer->setReadoutChipQueryFunction(myFunction);
        cPedeNoise.Initialise(true, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        // cTool.fDetectorContainer->resetReadoutChipQueryFunction();
        t.stop();
        t.show("Time to Scan Pedestals and Noise");
    }
    // inject hits and stubs using mask and compare input against output
    if(cmd.foundOption("memCheck"))
    {
        MemoryCheck2S cMemoryChecker;
        cMemoryChecker.Inherit(&cTool);
        cMemoryChecker.Initialise();

        // configure reference voltage
        cMemoryChecker.ConfigureVref();
        cMemoryChecker.MonitorTemperature();
        cMemoryChecker.MonitorInputVoltage();
        // find pedestal and set threshold
        if(cmd.foundOption("completeDataCheck"))
        {
            std::string          cArgsStr      = cmd.optionValue("completeDataCheck");
            std::vector<uint8_t> cChipsToCheck = getArgs(cArgsStr);
            cMemoryChecker.EvaluatePedeNoise(100); // find pedestal + noise
            cMemoryChecker.SetThreshold(-2.0);     // set threshold to 3 sigma away from pedestal
            int cTriggerGap = cTool.findValueInSettings<double>("TriggerSeparation", 500);
            cMemoryChecker.DataCheck(cChipsToCheck, cTriggerGap);
        }
        cMemoryChecker.MemoryCheck2SRaw(true);  // all ones
        cMemoryChecker.MemoryCheck2SRaw(false); // all zeros

        cMemoryChecker.MonitorAnalogue();
        cMemoryChecker.SaveOptimalTaps();
        cMemoryChecker.writeObjects();
        cMemoryChecker.resetPointers();
    }

    if(cmd.foundOption("checkSharedStubs"))
    {
        LOG(INFO) << BOLDMAGENTA << "Checking stubs across CBC neighbors" << RESET;
        t.start();

        MemoryCheck2S cMemoryChecker;
        cMemoryChecker.Inherit(&cTool);
        cMemoryChecker.Initialise();
        cMemoryChecker.EvaluatePedeNoise(10); // find pedestal + noise
        cMemoryChecker.SetThreshold(-2.0);    // set threshold to 3 sigma away from pedestal

        CheckCbcNeighbors cCheckCbcNeighbors;
        cCheckCbcNeighbors.Inherit(&cTool);
        cCheckCbcNeighbors.Initialise();
        cCheckCbcNeighbors.TestCbcNeighbors();

        t.stop();
        t.show("Time to check stubs on shared channels");
    }
    if(cCheckData)
    {
        std::string          cArgsStr = cmd.optionValue("checkData");
        std::vector<uint8_t> cArgs;
        std::stringstream    cArgsSS(cArgsStr);
        int                  i;
        while(cArgsSS >> i)
        {
            cArgs.push_back(i);
            if(cArgsSS.peek() == ',') cArgsSS.ignore();
        };

        t.start();
        DataChecker cDataChecker;
        cDataChecker.Inherit(&cTool);
        // cDataChecker.Initialise();
        if(cmd.foundOption("checkClusters")) cDataChecker.ClusterCheck(cArgs);
        if(cmd.foundOption("checkSLink")) cDataChecker.WriteSlinkTest(cmd.optionValue("checkSLink"));
        if(cmd.foundOption("checkStubs")) cDataChecker.StubCheck(cArgs);
        if(cmd.foundOption("noiseInjection")) cDataChecker.StubCheckWNoise(cArgs);
        if(cmd.foundOption("checkReadData")) cDataChecker.ReadDataTest();
        if(cmd.foundOption("checkAsync")) cDataChecker.AsyncTest();
        if(cmd.foundOption("checkReadNEvents")) cDataChecker.ReadNeventsTest();
        if(cSaveToFile) cDataChecker.CollectEvents();

        // cDataChecker.ReadNeventsTest();
        // cDataChecker.DataCheck(cFEsToCheck,0,0);
        // cDataChecker.ReadDataTest();
        // cDataChecker.HitCheck();
        // cDataChecker.writeObjects();
        // cDataChecker.resetPointers();
        t.show("Time to check data of the front-ends on the system: ");
    }

    // For next step... set all thresholds on CBCs to 560
    if(cmd.foundOption("threshold"))
    {
        cTool.setSameDac("VCth", cThreshold);
        LOG(INFO) << BOLDBLUE << "Threshold for next steps is set to " << +cThreshold << " DAC units." << RESET;
    }

    // Inject charge with antenna circuit and look for opens
    if(cFindOpens)
    {
#ifdef __ANTENNA__
        int cAntennaDelay = (cmd.foundOption("antennaDelay")) ? convertAnyInt(cmd.optionValue("antennaDelay").c_str()) : -1;
        int cLatencyRange = (cmd.foundOption("latencyRange")) ? convertAnyInt(cmd.optionValue("latencyRange").c_str()) : -1;

        OpenFinder::Parameters cOfp;
        // hard coded for now TODO: make this configurable
        cOfp.potentiometer = 0x265;
        // antenna group
        auto cSetting     = cTool.fSettingsMap.find("AntennaGroup");
        cOfp.antennaGroup = (cSetting != std::end(cTool.fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : (0);

        // antenna delay
        if(cAntennaDelay > 0)
            cOfp.antennaDelay = cAntennaDelay;
        else
        {
            auto cSetting     = cTool.fSettingsMap.find("AntennaDelay");
            cOfp.antennaDelay = (cSetting != std::end(cTool.fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : (200);
        }

        // scan range for latency
        if(cLatencyRange > 0)
            cOfp.latencyRange = cLatencyRange;
        else
        {
            auto cSetting     = cTool.fSettingsMap.find("ScanRange");
            cOfp.latencyRange = (cSetting != std::end(cTool.fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : (10);
        }

        OpenFinder cOpenFinder;
        cOpenFinder.Inherit(&cTool);
        cOpenFinder.Initialise(cOfp);
        LOG(INFO) << BOLDBLUE << "Starting open finding measurement [antenna potentiometer set to 0x" << std::hex << cOfp.potentiometer << std::dec << " written to the potentiometer" << RESET;
        cOpenFinder.FindOpens2S();
        // TODO: write this one from cLatencyScan.writeObjects();
        // cOpenFinder.writeObjects();
#endif
    }
    // inject charge with TP and look for shorts
    if(cShortFinder)
    {
        ShortFinder cShortFinder;
        cShortFinder.Inherit(&cTool);
        cShortFinder.Initialise();
        cShortFinder.Start(cRunNumber);
        cShortFinder.waitForRunToBeCompleted();
        cShortFinder.Stop();
    }
    // cTool.PrintRegCount();
    cTool.dumpConfigFiles();
    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    // system("/home/modtest/Programming/power_supply/bin/TurnOff -c /home/modtest/Programming/power_supply/config/config.xml ");

    if(!batchMode) cApp.Run();
    cGlobalTimer.stop();
    cGlobalTimer.show("Total execution time: ");

    std::ofstream cGoodRuns;
    cGoodRuns.open("GoodRunNumbers.dat", std::fstream::app);
    cGoodRuns << cRunNumber << "\n";
    cGoodRuns.close();
#ifdef __POWERSUPPLY__
    cPowerLog.open(cTool.getDirectoryName() + "/PowerLog.tab", std::ios::app);
    const auto cStop     = std::chrono::system_clock::now();
    int        cStopTime = (int)std::chrono::duration_cast<std::chrono::seconds>(cStop.time_since_epoch()).count();
    if(cmd.foundOption("powerState"))
    {
        cPowerLog << cStopTime << "\t";
        // Give complete status reoort for all channels in the power supply
        for(auto channelName: cPowerSupplyChannels)
        {
            if(channelName.second)
            {
                LOG(INFO) << BOLDYELLOW << cPowerSupply << " status of channel " << channelName.first << ":" RESET;
                bool        isOn       = cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->isOn();
                std::string isOnResult = isOn ? "1" : "0";
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string voltageCompliance = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getVoltageCompliance());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string voltage = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getVoltage());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string currentCompliance = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getCurrentCompliance());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                std::string current = "-";
                if(isOn) { current = std::to_string(cPowerSupplyHandler.getPowerSupply(cPowerSupply)->getChannel(channelName.first)->getCurrent()); }
                LOG(INFO) << "\tIsOn:\t\t" << BOLDYELLOW << isOnResult << RESET;
                LOG(INFO) << "\tV_max(set):\t\t" << BOLDYELLOW << voltageCompliance << RESET;
                LOG(INFO) << "\tV(meas):\t" << BOLDYELLOW << voltage << RESET;
                LOG(INFO) << "\tI_max(set):\t" << BOLDYELLOW << currentCompliance << RESET;
                LOG(INFO) << "\tI(meas):\t" << BOLDYELLOW << current << RESET;
                cPowerLog << voltage << "\t" << current << "\t";
            }
        }
        cPowerLog << "\n";
    }
    cPowerLog.close();
#endif
#endif

    return 0;
}
