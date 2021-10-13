#include <cstring>

#include "ExtraChecks.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "boost/format.hpp"
#include "tools/BackEndAlignment.h"
#include "tools/StubBackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/MemoryCheck2S.h"
#include "tools/PSAlignment.h"
#include "tools/PedeNoise.h"
#include "tools/PedeNoiseTime.h"
#include "tools/PedestalEqualization.h"
#include "tools/RegisterTester.h"
#include "tools/LinkAlignmentOT.h"
#include "tools/LatencyScan.h"

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
#include "gui_logger.h"
#endif

#ifdef __ANTENNA__
#include "Antenna.h"
#endif

// reference volage for lpgBT
float VREF_LPGBT        = 1.0;
float cConversionFactor = VREF_LPGBT / 1024.;

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
            // LOG(INFO) << BOLDMAGENTA << cRunNumber << RESET;
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
    cmd.defineOption("linkTest", "Check data coming over link....", ArgvParser::OptionRequiresValue);
    cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    cmd.defineOptionAlternative("measurePedeNoise", "m");

    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("skipAlignment", "Skip the back-end alignment step ", ArgvParser::OptionRequiresValue);
    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("allChan", "Do pedestal and noise measurement using all channels? Default: false", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("allChan", "a");

    cmd.defineOption("moduleId", "Serial Number of module . Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOption("checkData", "Compare injected hits and stubs with output [please provide a comma seperated list of chips to check]", ArgvParser::OptionRequiresValue);
    cmd.defineOption("alignPS", "Perform SSA-MPA alignment steps", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkClusters", "Check CIC2 sparsification... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("psDataTest", "....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkSLink", "Check S-link ... data saved to file ", ArgvParser::OptionRequiresValue);
    cmd.defineOption("checkStubs", "Check Stubs... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadData", "Check ReadData method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkAsync", "Check Async readout methods [PS objects only]... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("checkReadNEvents", "Check ReadNEvents method... ", ArgvParser::NoOptionAttribute);
    cmd.defineOption("noiseInjection", "Check noise injection...", ArgvParser::NoOptionAttribute);
    cmd.defineOption("calibrateADC", "Calibrate ADC on lpGBT....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("readIDs", "Read chip ids....", ArgvParser::NoOptionAttribute);
    cmd.defineOption("memCheck", "Check memories of the following CBCs", ArgvParser::NoOptionAttribute);
    cmd.defineOption("completeDataCheck", "Complete data check for the following CBCs", ArgvParser::OptionRequiresValue);
    cmd.defineOption("registerTest", "Test I2C registers on ROCs", ArgvParser::NoOptionAttribute);
    cmd.defineOption("manualScan", "Manual scan of threshold", ArgvParser::NoOptionAttribute);
    cmd.defineOption("injectionTest", "Manual scan of threshold", ArgvParser::OptionRequiresValue);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile           = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    bool        cTune             = (cmd.foundOption("tuneOffsets"));
    bool        cMeasurePedeNoise = (cmd.foundOption("measurePedeNoise"));
    bool        batchMode         = (cmd.foundOption("batch")) ? true : false;
    bool        cAllChan          = (cmd.foundOption("allChan")) ? true : false;
    bool        cCheckData        = (cmd.foundOption("checkData"));
    bool cSaveToFile = cmd.foundOption("save");
    std::string   cSkip       = (cmd.foundOption("skipAlignment")) ? cmd.optionValue("skipAlignment") : "";
    std::string   cInjectionSource = (cmd.foundOption("injectionTest")) ? cmd.optionValue("injectionTest") : "digital";
    std::string   cSrcLnkTst  = (cmd.foundOption("linkTest")) ? cmd.optionValue("linkTest") : "lpGBT";
    std::string   cModuleId  = (cmd.foundOption("moduleId")) ? cmd.optionValue("moduleId") : "ModuleOT";
    std::string   cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    auto          cRunNumber = returnRunNumber("RunNumbers.dat");
    std::ofstream cRunLog;
    cRunLog.open("RunNumbers.dat", std::fstream::app);
    cRunLog << cRunNumber << "\n";
    cRunLog.close();
    LOG(INFO) << BOLDBLUE << "Run number is " << +cRunNumber << RESET;
    cDirectory += Form("OT_ModuleTest_%s_Run%d", cModuleId.c_str(), cRunNumber);

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t;
    Timer       cGlobalTimer;
    cGlobalTimer.start();

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
                    for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel) * cConversionFactor; }
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
                for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel) * cConversionFactor; }
                float cMeanValue = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                LOG(INFO) << BOLDMAGENTA << "Measured V_min after correction is " << std::setprecision(2) << std::fixed << cMeanValue * 1e3 << " mV , expected value is " << cADCs_Refs[cIndx] * 1e3
                          << " difference is " << std::fabs(cMeanValue - cADCs_Refs[cIndx]) * 1e3 << " mV, correction needed to acheive this was  " << +cCorr << RESET;

                // turn off ADC mon
                // static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", 0x00 );
            } // configure lpGBT
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

    // map MPA outputs for PS module
    PSAlignment cPSAlignment;
    cPSAlignment.Inherit(&cTool);
    cPSAlignment.Initialise();
    cPSAlignment.MapMPAOutputs();

    // align CIC-lpGBT-BE 
    LinkAlignmentOT cLinkAlignment; 
    cLinkAlignment.Inherit(&cTool);
    try
    {
        cLinkAlignment.Start(0);
    }
    catch(const std::exception& e)
    {
        LOG (INFO) << BOLDRED << "Could not align link in the BE... stopping here." << RESET;
        return(666);
    }
    cLinkAlignment.waitForRunToBeCompleted();
    cLinkAlignment.dumpConfigFiles();
    if( !cLinkAlignment.getStatus() ) 
    {
        LOG (INFO) << BOLDRED << "Could not align link in the BE... stopping here." << RESET;
        return(666);
    }

    // align FEs - CIC
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(&cTool);
    cCicAligner.Start(0);
    cCicAligner.waitForRunToBeCompleted();
    cCicAligner.dumpConfigFiles();
    
    // time align stubs with L1 data in the BE 
    bool cSkipBEstubs = (cmd.foundOption("skipAlignment")) && ( cSkip.find("all") != std::string::npos || cSkip.find("beStubs") != std::string::npos );
    if( cSkipBEstubs ) LOG (INFO) << BOLDBLUE << "Will skip time alignment of stub data with L1 data in the BE " << RESET;
    else
    { 
        LOG (INFO) << BOLDBLUE << "Performing time alignment of stub data with L1 data in the BE " << RESET;
        StubBackEndAlignment cStubBackEndAligner;
        cStubBackEndAligner.Inherit(&cTool);
        cStubBackEndAligner.Start(0);
        cStubBackEndAligner.waitForRunToBeCompleted();
    }

    // now align data between SSA-MPA
    bool cSkipMPAin = (cmd.foundOption("skipAlignment")) && ( cSkip.find("all") != std::string::npos || cSkip.find("mpaInputs") != std::string::npos );
    if( cSkipMPAin ) LOG (INFO) << BOLDBLUE << "Will skip alignment of SSA output data (L1+stubs) to MPAs " << RESET;
    else
    { 
        LOG (INFO) << BOLDBLUE << "Performing alignment of SSA output data (L1+stubs) to MPAs " << RESET;
        cPSAlignment.Align();
    }
    cPSAlignment.dumpConfigFiles();


    // equalize thresholds on readout chips
    if(cTune)
    {
        // uint8_t cFeId=0;
        // auto cSelectFunction = [cFeId](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getId() == cFeId); };
        // cTool.fDetectorContainer->setReadoutChipQueryFunction(cSelectFunction);

        t.start();
        // now create a PedestalEqualization object
        PedestalEqualization cPedestalEqualization;
        cPedestalEqualization.Inherit(&cTool);
        //std::vector<FrontEndType> cTypes{FrontEndType::SSA};
        //for(auto cType: cTypes)
        //{
            //auto cSelectFunction = [cType](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == cType); };
            //cTool.fDetectorContainer->setReadoutChipQueryFunction(cSelectFunction);
            cPedestalEqualization.Inherit(&cTool);
            cPedestalEqualization.Initialise(cAllChan, true);
            cPedestalEqualization.FindVplus();
            cPedestalEqualization.FindOffsets();
            //cTool.fDetectorContainer->resetReadoutChipQueryFunction();
        //}
        cPedestalEqualization.Reset();
        // second parameter disables stub logic on CBC3
        cPedestalEqualization.writeObjects();
        cPedestalEqualization.dumpConfigFiles();
        cPedestalEqualization.resetPointers();
        t.show("Time to tune the front-ends on the system: ");
        // // reset
        // cTool.fDetectorContainer->resetReadoutChipQueryFunction();
    }

    if( cmd.foundOption("linkTest"))
    {
        auto cInterface = static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
        for(auto cBoard: *cTool.fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                if( cSrcLnkTst == "lpGBT" ) 
                {
                    auto& clpGBT = cOpticalGroup->flpGBT;
                    // configure lpGBT to produce constant pattern 
                    cTool.flpGBTInterface->ConfigureRxSource(clpGBT, {0,1,2,3,4,5,6}, 4);
                    cTool.flpGBTInterface->ConfigureDPPattern(clpGBT,0xE0E0E0E0);
                    D19cFWInterface::PhaseTuner cTuner;
                    for(size_t cLineId=1; cLineId<=6;cLineId++)
                    {
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            cTuner.AlignWord(cInterface, cHybrid->getId(), 0, cLineId, 0xE0, 8, true);
                        }
                    }
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                        cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
                        for(size_t cAttempt=0; cAttempt<100; cAttempt++)
                        {
                            (static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 6);
                        }
                    }
                    continue;
                }
                for(auto cHybrid: *cOpticalGroup)
                {
                    cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
                    cTool.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    if( cSrcLnkTst == "CIC" ) 
                    {
                        // CIC alignment pattern 
                        cTool.fCicInterface->SelectOutput(cCic, true);
                    }
                    else
                    {
                        LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << RESET;
                        // MPA shift pattern 
                        // enable MPA alignment pattern
                        LOG(INFO) << GREEN << "Enabling MPA Alignment pattern" << RESET;
                        std::vector<uint8_t>     cOriginalValues;
                        std::vector<std::string> cRegs;
                        uint8_t                  cAlignmentPattern = 0xE0;
                        std::vector<uint8_t>     cRegValues{0x2, cAlignmentPattern};
                        std::vector<std::string> cRegNames{"ReadoutMode", "LFSR_data"};
                        for(size_t cIndex = 0; cIndex < cRegValues.size(); cIndex++)
                        {
                            for(auto cChip: *cHybrid)
                            {
                                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                                cOriginalValues.push_back(cTool.fReadoutChipInterface->ReadChipReg(cChip, cRegNames[cIndex]));
                                cRegs.push_back(cRegNames[cIndex]);
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, cRegNames[cIndex], cRegValues[cIndex]);
                            } // loop over MPAs
                        }// loop over registers
                        for( uint8_t cPhyPort=8; cPhyPort<9; cPhyPort++)
                        {
                            LOG (INFO) << BOLDMAGENTA << "PhyPort#" << +cPhyPort << RESET;
                            cTool.fCicInterface->SelectMux(cCic, cPhyPort);
                            // align line
                            D19cFWInterface::PhaseTuner cTuner;
                            for(size_t cLineId=1; cLineId<=3;cLineId++)
                            {
                                cTuner.AlignWord(cInterface, cHybrid->getId(), 0, cLineId,cAlignmentPattern , 8, true);
                            }
                            for(size_t cAttempt=0; cAttempt<100; cAttempt++)
                            {
                                (static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 3);
                            }
                        }
                    }
                    if( cSrcLnkTst == "CIC" || cSrcLnkTst == "lpGBT" )
                    {
                        for(size_t cAttempt=0; cAttempt<100; cAttempt++)
                        {
                            (static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 6);
                        }
                    }
                }//hybrid 
            }//OG
        }//board
    }
    // if( cmd.foundOption("injectionTest"))
    // {
    //     auto cSetting       = cTool.fSettingsMap.find("PSmoduleSSAthreshold");
    //     int  cPSmoduleSSAth = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
    //     cSetting            = cTool.fSettingsMap.find("PSmoduleMPAthreshold");
    //     int cPSmoduleMPAth  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
    //     cSetting            = cTool.fSettingsMap.find("PSOccupancyPulseAmplitude");
    //     int cInjectionAmpl  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0xFF;
    //     cSetting            = cTool.fSettingsMap.find("Nevents");
    //     //auto cNevents       = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 10;
    //     for(auto board: *cTool.fDetectorContainer)
    //     {
    //         // configure trigger 
    //         uint8_t                  cMult            = 0;
    //         uint8_t                  cTriggerSource   = 6;
    //         uint16_t                 cDelayAfterReset = 100;
    //         uint16_t                 cDelayAfterTP    = 300;
    //         uint16_t                 cDelayTillNext   = 400;
    //         std::vector<std::string> cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_after_test_pulse", "test_pulse.delay_before_next_pulse", "misc.trigger_multiplicity"};
    //         std::vector<uint16_t>    cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayAfterTP, cDelayTillNext, cMult};
    //         std::vector<uint16_t>    cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    //         std::vector<std::pair<std::string, uint32_t>> cRegVec;
    //         cRegVec.clear();
    //         for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    //         {
    //             std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
    //             cFcmdRegOrigVals[cIndx] = cTool.fBeBoardInterface->ReadBoardReg(board, cRegName);
    //             cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    //         }
    //         cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    //         cTool.fBeBoardInterface->WriteBoardMultReg(board, cRegVec);
    //         cTool.fBeBoardInterface->WriteBoardReg(board, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    //         if( cInjectionSource.find("digital") != std::string::npos )
    //         {
    //             // first inject digitally 
    //             Injection              cInjection;
    //             std::vector<Injection> cInjections;
    //             // in principle here I would like to make sure all lines are aligned 
    //             // for now I will do just four 
    //             cInjection.fRow    = 10;
    //             cInjection.fColumn = 2;
    //             cInjections.push_back(cInjection);//0
    //             // cInjection.fRow    = 20;
    //             // cInjection.fColumn = 3;
    //             // cInjections.push_back(cInjection);//1
    //             // cInjection.fRow    = 30;
    //             // cInjection.fColumn = 4;
    //             // cInjections.push_back(cInjection);//2
    //             // cInjection.fRow    = 40;
    //             // cInjection.fColumn = 5;
                
    //             // inject 
    //             for(auto cOpticalReadout: *board)
    //             {
    //                 for(auto cHybrid: *cOpticalReadout)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         // make sure L1 latency is configured
    //                         if(cChip->getFrontEndType() == FrontEndType::MPA)
    //                         {
    //                             (static_cast<PSInterface*>(cTool.fReadoutChipInterface))->digiInjection(cChip, cInjections,0x01);
    //                         }
    //                         if(cChip->getFrontEndType() == FrontEndType::SSA)
    //                         {
    //                             cTool.fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
    //                             cTool.fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01); 
    //                             cTool.fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x01);
    //                             for(auto cInjection: cInjections) { cTool.fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9); }
    //                         }
    //                     } // chip
    //                 }     // hybrid
    //             }// optica]l group
    //         }

    //         if( cInjectionSource.find("analogue") != std::string::npos )
    //         {
    //             // analogue injection 
    //             cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "InjectedCharge", cInjectionAmpl);
    //             cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "AnalogueSync", 1);
    //             for(auto opticalGroup: *board)
    //             {
    //                 for(auto hybrid: *opticalGroup)
    //                 {
    //                     for(auto chip: *hybrid)
    //                     {
                
    //                         if(chip->getFrontEndType() == FrontEndType::SSA)
    //                         {
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleSSAth); 
    //                         }
    //                         if(chip->getFrontEndType() == FrontEndType::MPA)
    //                         {
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleMPAth); 
    //                         }
    //                     }
    //                 }
    //             }
    //         }
            
    //         // scan latency 
            
    //         for( int cLatencyOffset = -10; cLatencyOffset <= 10; cLatencyOffset++)
    //         {
    //             for(auto opticalGroup: *board)
    //             {
    //                 for(auto hybrid: *opticalGroup)
    //                 {
    //                     for(auto chip: *hybrid)
    //                     {
                
    //                         if(chip->getFrontEndType() == FrontEndType::SSA)
    //                         {
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "TriggerLatency", cLatency + cLatencyOffset - 1);
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleSSAth); 
    //                         }
    //                         if(chip->getFrontEndType() == FrontEndType::MPA)
    //                         {
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "TriggerLatency", cLatency + cLatencyOffset);
    //                             cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleMPAth); 
    //                         }
    //                     }
    //                 }
    //             }
    //             // send a ReSync since the latency was changed 
    //             cTool.fBeBoardInterface->ChipReSync(board);
                

    //             // look at data 
    //             cTool.ReadNEvents(board, cNevents);
    //             const std::vector<Event*>& cEvents         = cTool.GetEvents();
    //             for(size_t cTriggerId = 0; cTriggerId < (size_t)(cMult + 1); cTriggerId++)
    //             {
    //                 auto   cEventIter     = cEvents.begin() + cTriggerId;
    //                 // replace with data container
    //                 std::vector<PCluster> cPclusters;
    //                 std::vector<SCluster> cSclusters;
    //                 do 
    //                 {
    //                     if(cEventIter >= cEvents.end()) break;
    //                     for(auto cOpticalReadout: *board)
    //                     {
    //                         for(auto cHybrid: *cOpticalReadout)
    //                         {
    //                             for(auto cChip: *cHybrid)
    //                             {
    //                                 if(cChip->getFrontEndType() == FrontEndType::SSA ) continue;

    //                                 if( cChip->getFrontEndType() == FrontEndType::MPA)
    //                                 {
    //                                     auto cPclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetPixelClusters(cChip->getHybridId(), cChip->getId());
    //                                     auto cSclus  = static_cast<D19cCic2Event*>(*cEventIter)->GetStripClusters(cChip->getHybridId(), cChip->getId());
    //                                     auto cStubs  = static_cast<D19cCic2Event*>(*cEventIter)->StubVector(cChip->getHybridId(), cChip->getId());
                            
    //                                     struct
    //                                     {
    //                                         bool operator()(PCluster a, PCluster b) const { return a.fAddress < b.fAddress; }
    //                                     } customSortPclus;
    //                                     struct
    //                                     {
    //                                         bool operator()(SCluster a, SCluster b) const { return a.fAddress < b.fAddress; }
    //                                     } customSortSclus;
    //                                     struct
    //                                     {
    //                                         bool operator()(Stub a, Stub b) const { return a.fPosition < b.fPosition; }
    //                                     } customSortStubs;

    //                                     // sort P clusters by row 
    //                                     std::sort(cPclus.begin(), cPclus.end(), customSortPclus);
    //                                     // sort S clusters by row
    //                                     std::sort(cSclus.begin(), cSclus.end(), customSortSclus);
    //                                     // sort stubs by seed 
    //                                     std::sort(cStubs.begin(), cStubs.end(), customSortStubs);
                                        
    //                                     for(auto cPcluster : cPclus)
    //                                     {
    //                                         cPclusters.push_back(cPcluster);
    //                                         LOG(DEBUG) << BOLDBLUE << "\t\t Trigger#" << +cTriggerId << " Event#" << (*cEventIter)->GetEventCount() 
    //                                             << " in a burst of " << (1 + cMult) << " MPA" << +cChip->getId() 
    //                                             << " pixel cluster row " << +cPcluster.fAddress << " Z-pos " << +cPcluster.fZpos << " width is " << +cPcluster.fWidth
    //                                             << RESET;
    //                                     }
    //                                     for(auto cSCluster : cSclus)
    //                                     {
    //                                         cSclusters.push_back(cSCluster);
    //                                         LOG(DEBUG) << BOLDMAGENTA << "\t\t Trigger#" << +cTriggerId << " Event#" << (*cEventIter)->GetEventCount() 
    //                                             << " in a burst of " << (1 + cMult) << " MPA" << +cChip->getId() 
    //                                             << " strip cluster row " << +cSCluster.fAddress << " width is " << +cSCluster.fWidth
    //                                             << RESET;
    //                                     }
    //                                     size_t cStubCntr = 0;
    //                                     for(auto cStub: cStubs)
    //                                     {
    //                                         LOG(DEBUG) << BOLDCYAN << "\t\tStub#" << +cStubCntr << " Position " << +cStub.getPosition() << " - Row " << +cStub.getRow() << " - Bend " << +cStub.getBend()
    //                                                   << RESET;
    //                                         cStubCntr++;
    //                                     }
    //                                 }
    //                             }
    //                         }
    //                     }
    //                     cEventIter += (1 + cMult);
    //                 } while(cEventIter < cEvents.end());
    //                 LOG(INFO) << BOLDBLUE << "Trigger#" << +cTriggerId << " in a burst of " << (1 + cMult) 
    //                     << " latency set to " << cLatency + cLatencyOffset 
    //                     << " latency offset of " << +cLatencyOffset 
    //                     << " found " << cSclusters.size() << " S clusters and "
    //                     << cPclusters.size() << " P clusters in L1 data." << RESET;
                                        
    //             }//event loop 
    //         }
    //     }

    //     LatencyScan cLatencyScan;
    //     cLatencyScan.Inherit(&cTool);
    //     cLatencyScan.Initialize();
    //     cLatencyScan.ScanLatency();
    // }
    if( cmd.foundOption("injectionTest"))
    {
        for(auto board: *cTool.fDetectorContainer)
        {
            cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "ReadoutMode", 0);
            if( cInjectionSource.find("digital") != std::string::npos )
            {
                // configure trigger 
                uint8_t                  cTriggerSource   = 6;
                uint16_t                 cDelayAfterReset = 100;
                uint16_t                 cDelayTillNext   = 400;
                std::vector<std::string> cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_before_next_pulse" };
                std::vector<uint16_t>    cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayTillNext};
                std::vector<uint16_t>    cFcmdRegOrigVals(cFcmdRegs.size(), 0);
                std::vector<std::pair<std::string, uint32_t>> cRegVec;
                cRegVec.clear();
                for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
                {
                    std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
                    cFcmdRegOrigVals[cIndx] = cTool.fBeBoardInterface->ReadBoardReg(board, cRegName);
                    cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
                }
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                cTool.fBeBoardInterface->WriteBoardMultReg(board, cRegVec);
                cTool.fBeBoardInterface->WriteBoardReg(board, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);


                // first inject digitally 
                Injection              cInjection;
                std::vector<Injection> cInjections;
                // inject one cluster into each MPA-SSA pair
                cInjection.fRow    = 10;
                cInjection.fColumn = 2;
                cInjections.push_back(cInjection);//0
                // inject 
                for(auto cOpticalReadout: *board)
                {
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            // and that readout mode is set 
                            // make sure L1 latency is configured
                            if(cChip->getFrontEndType() == FrontEndType::MPA)
                            {
                                (static_cast<PSInterface*>(cTool.fReadoutChipInterface))->digiInjection(cChip, cInjections,0x01);
                            }
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01); 
                                cTool.fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", 0x01);
                                for(auto cInjection: cInjections) { cTool.fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cInjection.fRow), 0x9); }
                            }
                        } // chip
                    }     // hybrid
                }// optica]l group
            }
            else if( cInjectionSource.find("analogue") != std::string::npos )
            {
                // configure trigger 
                uint8_t                  cTriggerSource   = 6;
                uint16_t                 cDelayAfterReset = 100;
                uint16_t                 cDelayTillNext   = 400;
                std::vector<std::string> cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_before_next_pulse" };
                std::vector<uint16_t>    cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayTillNext};
                std::vector<uint16_t>    cFcmdRegOrigVals(cFcmdRegs.size(), 0);
                std::vector<std::pair<std::string, uint32_t>> cRegVec;
                cRegVec.clear();
                for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
                {
                    std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
                    cFcmdRegOrigVals[cIndx] = cTool.fBeBoardInterface->ReadBoardReg(board, cRegName);
                    cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
                }
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
                cTool.fBeBoardInterface->WriteBoardMultReg(board, cRegVec);
                cTool.fBeBoardInterface->WriteBoardReg(board, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

                auto cSetting       = cTool.fSettingsMap.find("PSmoduleSSAthreshold");
                int  cPSmoduleSSAth = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
                cSetting            = cTool.fSettingsMap.find("PSmoduleMPAthreshold");
                int cPSmoduleMPAth  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
                cSetting            = cTool.fSettingsMap.find("PSOccupancyPulseAmplitude");
                int cInjectionAmpl  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0xFF;
                cSetting            = cTool.fSettingsMap.find("SamplingModeSSA");
                int cSamplingSSA  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0;
                cSetting            = cTool.fSettingsMap.find("SamplingModeMPA");
                int cSamplingMPA  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0;
                // analogue injection 
                cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "InjectedCharge", cInjectionAmpl);
                cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "AnalogueSync", 1);
                LOG (INFO) << BOLDBLUE << cInjectionAmpl << " injected charge, " 
                    << cPSmoduleSSAth << " as SSA threshold "
                    << cPSmoduleMPAth << " as MPA threshold "
                    << RESET;
                // disable injection on all pixels MPAs 
                for(auto opticalGroup: *board)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            cTool.fReadoutChipInterface->WriteChipReg(chip, "ENFLAGS_ALL", 0x00, false); //0x5E
                        }
                    }
                }
                // enable a few 
                std::vector<uint32_t> cPxls; 
                uint16_t cFirstRow=10; 
                uint16_t cNCols=2;
                uint16_t cNRows=1;
                std::vector<uint16_t> cStrps;
                for( uint16_t cNRow=0; cNRow < cNRows; cNRow++)
                {
                    uint16_t cRow = cFirstRow + cNRow*2; 
                    cStrps.push_back( cRow );
                    for( uint16_t cCol=0; cCol < cNCols; cCol++)
                    {
                        cPxls.push_back( (uint32_t)(cCol) * NSSACHANNELS + (uint32_t)cRow );
                    }
                }
                LOG (INFO) << BOLDBLUE << "Enabling analogue injection in " << cPxls.size() << " pixels and " << cStrps.size() << " strips." << RESET;
                for(auto opticalGroup: *board)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            if(chip->getFrontEndType() == FrontEndType::MPA)
                            {
                                for( auto cPxl : cPxls )
                                {
                                    std::stringstream cRegName;
                                    cRegName << "ENFLAGS_P" << +cPxl; 
                                    cTool.fReadoutChipInterface->WriteChipReg(chip,cRegName.str(), 0x4F, false); 
                                    std::stringstream cRegNameTrim;
                                    cRegNameTrim << "TrimDAC_P" << +cPxl; 
                                    cTool.fReadoutChipInterface->WriteChipReg(chip,cRegNameTrim.str(), 0x0); 
                                }
                            }
                            if(chip->getFrontEndType() == FrontEndType::SSA)
                            {
                                for( auto sStrp : cStrps )
                                {
                                    std::stringstream cRegName;
                                    cRegName << "ENFLAGS_S" << +sStrp; 
                                    cTool.fReadoutChipInterface->WriteChipReg(chip,cRegName.str(), 0x13, false); 
                                }
                            }
                        }
                    }
                }
                
                for(auto opticalGroup: *board)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            if(chip->getFrontEndType() == FrontEndType::SSA)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleSSAth); 
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "SAMPLINGMODE_ALL", cSamplingSSA);
                            }
                            if(chip->getFrontEndType() == FrontEndType::MPA)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleMPAth); 
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "ModeSel_ALL", cSamplingMPA);
                            }
                        }
                    }
                }
            }
            else
            {
                auto cTriggerSource = cTool.fBeBoardInterface->ReadBoardReg(board, "fc7_daq_cnfg.fast_command_block.trigger_source");
                LOG (INFO) << BOLDBLUE << "Injection test with trigger source " << +cTriggerSource << RESET;

                auto cSetting       = cTool.fSettingsMap.find("PSmoduleSSAthreshold");
                int  cPSmoduleSSAth = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
                cSetting            = cTool.fSettingsMap.find("PSmoduleMPAthreshold");
                int cPSmoduleMPAth  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 100;
                cSetting            = cTool.fSettingsMap.find("SamplingModeSSA");
                int cSamplingSSA  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0;
                cSetting            = cTool.fSettingsMap.find("SamplingModeMPA");
                int cSamplingMPA  = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 0;
                
                
                // force TP to be off 
                cTool.setSameDacBeBoard(static_cast<BeBoard*>(board), "AnalogueSync", 0);
                // analogue injection 
                for(auto opticalGroup: *board)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            if(chip->getFrontEndType() == FrontEndType::SSA)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "SAMPLINGMODE_ALL", cSamplingSSA);
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleSSAth); 
                            }
                            if(chip->getFrontEndType() == FrontEndType::MPA)
                            {
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "ModeSel_ALL", cSamplingMPA);
                                cTool.fReadoutChipInterface->WriteChipReg(chip, "Threshold", cPSmoduleMPAth); 
                            }
                        }
                    }
                }
            }
        }
        
        LatencyScan cLatencyScan;
        cLatencyScan.Inherit(&cTool);
        cLatencyScan.Initialize();
        cLatencyScan.ScanLatency();
    }
    // measure noise on FE chips
    if(cMeasurePedeNoise)
    {
        // auto cSetting       = cTool.fSettingsMap.find("Nevents");
        // auto cNevents       = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 10;
        
        // // figure out what I want to do 
        // bool cForcePSasync = true;
        // for(auto cBoard: *cTool.fDetectorContainer)
        // {
        //     if(cForcePSasync) cBoard->setEventType(EventType::PSAS);
        //     cTool.setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", 40);
        //     cTool.setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "Threshold", 20);
        //     cTool.setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "AnalogueAsync", 0x1);
        // }
        // for(auto cBoard: *cTool.fDetectorContainer)
        // {
        //     for( uint8_t cAttempt=0; cAttempt < 10; cAttempt++)
        //     {
        //         LOG (INFO) << BOLDBLUE << "Attempt#" << +cAttempt << RESET;
        //         cTool.ReadNEvents(cBoard, cNevents);
        //         const std::vector<Event*>& cEvents = cTool.GetEvents();
        //         for( auto cEvent : cEvents)
        //         {
        //             for(auto opticalGroup: *cBoard)
        //             {
        //                 for(auto hybrid: *opticalGroup)
        //                 {
        //                     for(auto chip: *hybrid)
        //                     {
        //                         auto cHits = cEvent->GetHits(chip->getHybridId(), chip->getId() );
        //                         float cOcc=0;
        //                         for(auto cHit : cHits) cOcc += cHit;
        //                         LOG (INFO) << BOLDMAGENTA << "\t.. " << cOcc/chip->size() << "[ " << chip->size() << " ] " << RESET;
        //                     }
        //                 }
        //             } 
        //         }
        //     }
        // }

        LOG(INFO) << BOLDMAGENTA << "Measuring pedestal and noise" << RESET;
        t.start();
        // if this is true, I need to create an object of type PedeNoise from the members of Calibration
        // tool provides an Inherit(Tool* pTool) for this purpose
        PedeNoise cPedeNoise;
        cPedeNoise.Inherit(&cTool);
        //auto myFunction = [](const ChipContainer *theChip){return (theChip->getId()==15);};
        //auto myFunction = [](const ChipContainer *theChip){return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA);};
        //cTool.fDetectorContainer->setReadoutChipQueryFunction(myFunction);
        cPedeNoise.Initialise(cAllChan, true); // canvases etc. for fast calibration
        cPedeNoise.measureNoise();
        cPedeNoise.writeObjects();
        cPedeNoise.dumpConfigFiles();
        //cTool.fDetectorContainer->resetReadoutChipQueryFunction();
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
            std::string          cArgsStr    = cmd.optionValue("completeDataCheck");
            std::vector<uint8_t> cFesToCheck = getArgs(cArgsStr);
            cMemoryChecker.EvaluatePedeNoise(10); // find pedestal + noise
            cMemoryChecker.SetThreshold(-2.0);    // set threshold to 3 sigma away from pedestal
            auto cSetting    = cTool.fSettingsMap.find("TriggerSeparation");
            int  cTriggerGap = (cSetting != std::end(cTool.fSettingsMap)) ? cSetting->second : 500;
            cMemoryChecker.DataCheck(cFesToCheck, cTriggerGap);
        }
        cMemoryChecker.MemoryCheck2SRaw(true);  // all ones
        cMemoryChecker.MemoryCheck2SRaw(false); // all zeros

        cMemoryChecker.MonitorAnalogue();
        cMemoryChecker.SaveOptimalTaps();
        cMemoryChecker.writeObjects();
        cMemoryChecker.resetPointers();
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
        cDataChecker.Initialise();
        if(cmd.foundOption("psDataTest"))
        {
            // auto cInjections = cDataChecker.GeneratePSInjections(4);
            // for(auto cInjection : cInjections )
            // {
            //     LOG (INFO) << BOLDMAGENTA << "injection in pixel " << +cInjection.fColumn
            //         << " and row " << +cInjection.fRow
            //         << RESET;
            // }
            cDataChecker.InjectionTestPS(100);
        }
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
        cDataChecker.writeObjects();
        cDataChecker.resetPointers();
        t.show("Time to check data of the front-ends on the system: ");
    }
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
    return 0;
}
