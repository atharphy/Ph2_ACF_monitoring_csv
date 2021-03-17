#include <cstring>

#include "DPInterface.h"
#include "OpenFinder.h"
#include "PSHybridTester.h"
#include "PedeNoise.h"
#include "PedestalEqualization.h"
#include "ShortFinder.h"
#include "Utils/Timer.h"
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "tools/BackEndAlignment.h"
#include "tools/CicFEAlignment.h"
#include "tools/DataChecker.h"
#include "tools/PSAlignment.h"

#include "../tools/Channel.h"
// #ifdef __POWERSUPPLY__
// // Libraries
// #include "DeviceHandler.h"
// #include "PowerSupply.h"
// #include "PowerSupplyChannel.h"
// #endif

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#define __NAMEDPIPE__

#ifdef __NAMEDPIPE__
#include "gui_logger.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

#define CHIPSLAVE 4

#ifndef Measurement
typedef std::pair<float,float> Measurement;
#endif

// int MeasureCurrent(std::string pHwFile, std::string pPowerSupply
//     , std::vector<Measurement> &pMeasurements)
// {
//     // power supply 
//     pugi::xml_document cSettings;
//     DeviceHandler cInstrumentHandler;
//     cInstrumentHandler.readSettings(pHwFile, cSettings);
//     try
//     {
//         cInstrumentHandler.getPowerSupply(pPowerSupply);
//     }
//     catch(const std::out_of_range& oor)
//     {
//         std::cerr << "Out of Range error: " << oor.what() << '\n';
//         return -1;
//     }

//     pMeasurements.clear();
//     // Get all channels of the powersupply
//     std::vector<std::pair<std::string, bool>> channelNames;
//     pugi::xml_document                        doc;
//     if(!doc.load_file(pHwFile.c_str())) return -1;
//     pugi::xml_node devices = doc.child("Devices");
//     for(pugi::xml_node ps = devices.first_child(); ps; ps = ps.next_sibling())
//     {
//         std::string s(ps.attribute("ID").value());
//         if(s == pPowerSupply)
//         {
//             for(pugi::xml_node channel = ps.child("Channel"); channel; channel = channel.next_sibling("Channel"))
//             {
//                 std::string name(channel.attribute("ID").value());
//                 std::string use(channel.attribute("InUse").value());

//                 channelNames.push_back(std::make_pair(name, use == "Yes"));
//             }
//         }
//     }
//     doc.reset();

//     LOG(INFO) << BOLDBLUE << "Measuring current consumption on all channels.." << RESET;
//     for(auto channelName: channelNames) {
//         Measurement cMeasurement;
//         cMeasurement.first = cInstrumentHandler.getPowerSupply(pPowerSupply)->getChannel(channelName.first)->getVoltage();
//         cMeasurement.second = cInstrumentHandler.getPowerSupply(pPowerSupply)->getChannel(channelName.first)->getCurrent();
//         //std::string current = std::to_string(pInstrumentHandler.getPowerSupply(pPowerSupply)->getChannel(channelName.first)->getCurrent()); 
//         //std::string voltage = std::to_string(pInstrumentHandler.getPowerSupply(pPowerSupply)->getChannel(channelName.first)->getVoltage());
//         LOG(INFO) << "\tV(meas) [Ch#" << channelName.first << "] :\t" << BOLDWHITE << cMeasurement.first 
//             << "\tI(meas) [Ch#" << channelName.first << "] :\t" << BOLDWHITE << cMeasurement.second << RESET;
//     }
//     return 0;
// }
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

    cmd.defineOption("powerSupply", "Name of the power supply as described in the HW file", ArgvParser::OptionRequiresValue);
    cmd.defineOption("enableCIC", "Disable CIC reset", ArgvParser::NoOptionAttribute);
    cmd.defineOption("enableCICclock", "Enable CIC clock", ArgvParser::NoOptionAttribute);
    cmd.defineOption("configureCIC", "Apply default configuration", ArgvParser::NoOptionAttribute);
    cmd.defineOption("prepareCIC", "CIC start-up sequence", ArgvParser::NoOptionAttribute);
    cmd.defineOption("registerTest","run register test", ArgvParser::OptionRequiresValue);
    cmd.defineOption("enableSSA", "Disable SSA reset", ArgvParser::NoOptionAttribute);
    cmd.defineOption("enableSSAclock", "Enable SSA clock", ArgvParser::NoOptionAttribute);
    cmd.defineOption("configureSSA", "Apply default configuration", ArgvParser::NoOptionAttribute);
    cmd.defineOption("readoutRate", "Readout rate [320 or 640]", ArgvParser::OptionRequiresValue);

    cmd.defineOption("clockDriveCIC", "Clock drive strength for CIC", ArgvParser::OptionRequiresValue);
    
    // cmd.defineOption("tuneOffsets", "tune offsets on readout chips connected to CIC.");
    // cmd.defineOptionAlternative("tuneOffsets", "t");

    // cmd.defineOption("measurePedeNoise", "measure pedestal and noise on readout chips connected to CIC.");
    // cmd.defineOptionAlternative("measurePedeNoise", "m");

    // cmd.defineOption("findShorts", "look for shorts", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("findOpens", "perform latency scan with antenna on UIB", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("mpaTest", "Check MPA input with Data Player Pattern [provide pattern]", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    // cmd.defineOption("ssapair", "Debug selected SSA pair. Possible options: 01, 12, 23, 34, 45, 56, 67", ArgvParser::OptionRequiresValue);

    // cmd.defineOption("threshold", "Threshold value to set on chips for open and short finding", ArgvParser::OptionRequiresValue);
    // cmd.defineOption("hybridId", "Serial Number of front-end hybrid. Default value: xxxx", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);

    // cmd.defineOption("pattern", "Data Player Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    // cmd.defineOptionAlternative("pattern", "p");

    // cmd.defineOption("withCIC", "Perform CIC alignment steps", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("eyeScanCic", "Perform CIC eye scan", ArgvParser::NoOptionAttribute);
    // cmd.defineOption("checkAsync", "Check async readout", ArgvParser::OptionRequiresValue);
    // cmd.defineOption("checkSync", "Check sync readout", ArgvParser::OptionRequiresValue);

    // cmd.defineOption("perType", "perform pedeNoise per chip flavour [MPA/SSA]");
    // cmd.defineOptionAlternative("perType", "a");

    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/Commissioning.xml";
    // bool cFindOpens = (cmd.foundOption ("findOpens") )? true : false;
    // bool cShortFinder = ( cmd.foundOption ( "findShorts" ) ) ? true : false;
    bool        batchMode  = (cmd.foundOption("batch")) ? true : false;
    std::string cPowerSupply = (cmd.foundOption("powerSupply")) ? cmd.optionValue("powerSupply") : "";
    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string cHybridId  = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    uint16_t    cReadoutRate = (cmd.foundOption("readoutRate")) ? convertAnyInt(cmd.optionValue("readoutRate").c_str()) : 320;
    uint16_t    cConfigurationAttempts = (cmd.foundOption("registerTest")) ? convertAnyInt(cmd.optionValue("registerTest").c_str()) : 10;
    uint16_t    cCicClockDrive = (cmd.foundOption("clockDriveCIC")) ? convertAnyInt(cmd.optionValue("clockDriveCIC").c_str()) : 7;
    
    //std::string cChipType  = (cmd.foundOption("checkAsync")) ? cmd.optionValue("checkAsync") : "SSA";
    //if(!(cmd.foundOption("checkAsync"))) cChipType = (cmd.foundOption("checkSync")) ? cmd.optionValue("checkSync") : "SSA";

    //uint8_t           cPattern = (cmd.foundOption("mpaTest")) ? convertAnyInt(cmd.optionValue("mpaTest").c_str()) : 0;
    //const std::string cSSAPair = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    cDirectory += Form("FEH_PS_%s", cHybridId.c_str());

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t, T;

    
    // std::vector<Measurement> cMeasurements;
    // MeasureCurrent(cHWFile, cPowerSupply , cMeasurements);
        

    T.start();
    std::stringstream outp;
    // use a generic tool 
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
    //cTool.ConfigureHw();
    //cTool.ConfigureHw();
    // first ..configure BeBoard
    // setting up back-end board
    for( auto cBoard: *cTool.fDetectorContainer)
    {
        BeBoard* cBeBoard = static_cast<BeBoard*>(cBoard);
        cTool.fBeBoardInterface->ConfigureBoard(cBeBoard);

        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;

            // are these needed?
            uint8_t cLinkId = cOpticalGroup->getId();
            static_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->selectLink(cLinkId);
            static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ConfigureChip(clpGBT);
        }// configure lpGBT 

        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;

            // de-activate reset for CIC 
            if( cmd.foundOption("enableCIC") )
            {
                LOG(INFO) << BOLDBLUE << "Disabling CIC reset" << RESET;
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicReset(clpGBT, false);
            }
            // enable clock for CIC 
            if( cmd.foundOption("enableCICclock"))
            {
                LOG(INFO) << BOLDBLUE << "Enabling CIC clock" << RESET;
                // import from xml at some point 
                lpGBTClockConfig cClkCnfg; 
                cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                cClkCnfg.fClkDriveStr = cCicClockDrive; 
                cClkCnfg.fClkInvert = 0;
                cClkCnfg.fClkPreEmphWidth = 0; 
                cClkCnfg.fClkPreEmphMode = 0; 
                cClkCnfg.fClkPreEmphStr = 0;
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg);
            }
            
            // de-activate reset for SSA 
            if( cmd.foundOption("enableSSA"))
            {
                LOG(INFO) << BOLDBLUE << "Disabling SSA reset" << RESET;
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ssaReset(clpGBT, false);
            }
            if( cmd.foundOption("enableSSAclock"))
            {
                LOG(INFO) << BOLDBLUE << "Enabling hybrid clock" << RESET;
                lpGBTClockConfig cClkCnfg; 
                cClkCnfg.fClkFreq = 4;  
                cClkCnfg.fClkDriveStr = 1; 
                cClkCnfg.fClkInvert = 1;
                cClkCnfg.fClkPreEmphWidth = 0; 
                cClkCnfg.fClkPreEmphMode = 0; 
                cClkCnfg.fClkPreEmphStr = 0;
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg);
            }
        } // enable ROCs 

        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;

            // CIC configure 
            if( cmd.foundOption("configureCIC") )
            {
                // configure CIC 
                for(auto cHybrid: *cOpticalGroup)
                {
                    LOG(INFO) << BOLDBLUE << "Configuring CIC(s)" << RESET;
                    OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                    auto& cCic = cOuterTrackerHybrid->fCic;
                    cTool.fCicInterface->ConfigureChip(cCic);
                    static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();

                    // CIC start-up
                    // configure mode 
                    if( !cmd.foundOption("prepareCIC") ) continue;
                    
                    bool cSuccess = true;
                    if(cOuterTrackerHybrid->size() > 0) 
                    {
                        auto         cFirstROC = static_cast<ReadoutChip*>(cOuterTrackerHybrid->at(0));
                        FrontEndType cType     = FrontEndType::CBC3;
                        if(cFirstROC != nullptr) cType = cFirstROC->getFrontEndType();
                        uint8_t cModeSelect = (cType != FrontEndType::CBC3); // 0 --> CBC , 1 --> MPA
                        // select CIC mode
                        cSuccess = cTool.fCicInterface->SelectMode(cCic, cModeSelect);
                        if(!cSuccess)
                        {
                            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to configure CIC mode.." << RESET;
                            exit(0);
                        }
                        LOG(INFO) << BOLDMAGENTA << "CIC configured for " << ((cModeSelect == 0) ? "2S" : "PS") << " readout." << RESET;
                    }

                    // then start-up CIC 
                    // first  
                    // select CIC FE enable register
                    std::vector<uint8_t> cFeIds(0);
                    for(auto cReadoutChip: *cHybrid)
                    {
                        if(cReadoutChip->getFrontEndType() == FrontEndType::SSA) continue;
                        cFeIds.push_back(cReadoutChip->getId());
                    }
                    cTool.fCicInterface->EnableFEs(cCic, cFeIds, true);

                    // CIC start-up sequence
                    uint8_t cDriveStrength = 1;
                    cSuccess               = cTool.fCicInterface->StartUp(cCic, cDriveStrength);
                    cTool.fBeBoardInterface->ChipReSync(cBoard);
                    if( cSuccess )
                        LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " performed start-up sequence on CIC" << +(cOuterTrackerHybrid->getId() % 2) << " connected to link "
                              << +cOuterTrackerHybrid->getLinkId() << RESET;
                }
            }

            if( cmd.foundOption("configureSSA"))
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    // Configure readout-chips [CBCs, MPAs, SSAs]
                    for(auto cReadoutChip: *cHybrid)
                    {
                        LOG(INFO) << BOLDBLUE << "Configuring readout chip [chip id " << +cReadoutChip->getId() << " ]" << RESET;
                        if(cReadoutChip->getFrontEndType() == FrontEndType::SSA)
                        {
                            cTool.fReadoutChipInterface->ConfigureChip(cReadoutChip);
                        }//SSAs
                    }//ROCs
                }//OG
            }//configure SSA 
        } // configure ROCs + CICs

        if( cmd.foundOption("configureSSA"))     static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->printErrorSummary();
        
        if( cmd.foundOption("registerTest"))
        {
            // reset error summaries
            static_cast<CicInterface*>(cTool.fCicInterface)->resetErrorSummary();
            static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->resetErrorSummary();
            // try and configure all chips N times 
            for( size_t cAttempt=0; cAttempt < cConfigurationAttempts; cAttempt++)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        LOG (INFO) << BOLDBLUE << "Configuring CIC"<< RESET;
                        cTool.fCicInterface->ConfigureChip(cCic);
                        for(auto cReadoutChip: *cHybrid)
                        {
                            LOG(INFO) << BOLDBLUE << "Configuring readout chip [chip id " << +cReadoutChip->getId() << " ]" << RESET;
                            if(cReadoutChip->getFrontEndType() == FrontEndType::SSA)
                            {
                                cTool.fReadoutChipInterface->ConfigureChip(cReadoutChip);
                            }//SSAs
                        }//ROCs
                    }//OG
                }//board
            }// configuration attempts 

            // summarize 
            // static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();
            // auto cCicErrSummary = static_cast<CicInterface*>(cTool.fCicInterface)->getReadBackErrorSummary();
            // auto cCicWriteErrSummary = static_cast<CicInterface*>(cTool.fCicInterface)->getWriteErrorSummary();
            // float cCicCrct = (float)(cCicErrSummary.second - cCicErrSummary.first); 
            // float cCicCrctW = (float)(cCicWriteErrSummary.second - cCicWriteErrSummary.first); 

            static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->printErrorSummary();
            auto cSsaErrSummary = static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->getReadBackErrorSummary();
            auto cSssaWriteErrSummary = static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->getWriteErrorSummary();
            float cSsaCrct = (float)(cSsaErrSummary.second - cSsaErrSummary.first); 
            float cSsaCrctW = (float)(cSssaWriteErrSummary.second - cSssaWriteErrSummary.first); 
            //LOG (INFO) << BOLDBLUE << "CIC register read-back successes : " << cCicCrct << " out of " << cSsaErrSummary.second << RESET;
            //LOG (INFO) << BOLDBLUE << "CIC register write successes : " << cCicCrct << " out of " << cSsaErrSummary.second << RESET;
            LOG (INFO) << BOLDBLUE << "SSA register read-back successes : " << cSsaCrct << " out of " << cSsaErrSummary.second << RESET;
            LOG (INFO) << BOLDBLUE << "SSA register write successes : " << cSsaCrctW << " out of " << cSssaWriteErrSummary.second << RESET;

                    // std::vector<float> cErrors( cConfigurationAttempts, 0 );
                    // std::vector<float> cRelErrorUnc( cConfigurationAttempts, 0 );
                    // for( size_t cAttempt=0; cAttempt < cConfigurationAttempts; cAttempt++)
                    // {
                    //     cTool.fCicInterface->ConfigureChip(cCic);
                    //     std::pair<uint16_t,uint16_t>  cErrorSummary =  static_cast<CicInterface*>(cTool.fCicInterface)->getReadBackErrorSummary();
                    //     float cCorrect = (float)(cErrorSummary.second - cErrorSummary.first); 
                    //     float cFrcCrct = cCorrect/cErrorSummary.second;
                    //     float cFrcCrctErr  = std::pow( std::sqrt(cCorrect)/cCorrect,2.0);
                    //     cFrcCrctErr += std::pow( std::sqrt(cErrorSummary.second)/(float)cErrorSummary.second,2.0);
                    //     cFrcCrctErr = std::sqrt( cFrcCrctErr );
                    //     static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();
                    //     static_cast<CicInterface*>(cTool.fCicInterface)->resetErrorSummary();
                    //     cErrors[cAttempt]=cFrcCrct;
                    //     cRelErrorUnc[cAttempt]=cFrcCrctErr;
                    //     LOG (INFO) << BOLDBLUE << "\t...Configuration Attempt#" << +cAttempt 
                    //         << " number of correct transactions is " << cCorrect
                    //         << " fraction correct is " << cFrcCrct
                    //         << " relative error is " << cFrcCrctErr
                    //         << RESET;
                    // }
                    // // summarize 
                    // // summarize noise hits
                    // auto cSum = std::accumulate(cErrors.begin(), cErrors.end(), 0.0);
                    // auto cMean = cSum/cErrors.size();
                    // auto cMax = std::max_element(cErrors.begin(), cErrors.end());
                    // auto cMin = std::min_element(cErrors.begin(), cErrors.end());
                    // double cSqSum = std::inner_product(cErrors.begin(), cErrors.end(), cErrors.begin(), 0.0);
                    // double cStdDev = std::sqrt(cSqSum / cErrors.size() - cMean * cMean);
                    // LOG (INFO) << BOLDBLUE << "Summary configuration test " 
                    //         << " fraction of correct transactions is " << cMean
                    //         << " standard deviation is  " << cStdDev
                    //         << " maximum error fraction found is " << *cMax 
                    //         << " minimum error fraction found is " << *cMin
                    //         << RESET;
                    //static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();
                //}
            
        }// register test
       
    }

    // // align ASICs on PS module
    // PSAlignment cPSAlignment;
    // cPSAlignment.Inherit(&cTool);
    // cPSAlignment.Initialise();
    // // map MPA outputs for PS module
    // cPSAlignment.MapMPAOutputs();
    // // reset all chip and board registers
    // // not configured by the tool
    // // back to their original values
    // cPSAlignment.Reset();

    // // interface to data player
    // DPInterface         cDPInterfacer;
    // BeBoardFWInterface* cInterface = dynamic_cast<BeBoardFWInterface*>(cHybridTester.fBeBoardFWMap.find(0)->second);

    // // need to do this if
    // // reading out CIC
    // // or testing MPA
    // if(cmd.foundOption("withCIC") || cmd.foundOption("mpaTest"))
    // {
    //     // TO-DO
    //     // add condirion to check if USB is being used
    //     cHybridTester.SelectCIC(true);

    //     CicFEAlignment cCicAligner;
    //     cCicAligner.Inherit(&cHybridTester);
    //     cCicAligner.Start(0);
    //     cCicAligner.waitForRunToBeCompleted();
    //     // reset all chip and board registers
    //     // to what they were before this tool was called
    //     cCicAligner.Reset();
    //     cCicAligner.dumpConfigFiles();

    //     // align back-end
    //     BackEndAlignment cBackEndAligner;
    //     cBackEndAligner.Inherit(&cHybridTester);
    //     cBackEndAligner.Start(0);
    //     cBackEndAligner.waitForRunToBeCompleted();
    //     // reset all chip and board registers
    //     // to what they were before this tool was called
    //     cBackEndAligner.Reset();

    //     // Check if data player is running
    //     // if(cDPInterfacer.IsRunning(cInterface))
    //     // {
    //     //     LOG(INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
    //     //     cDPInterfacer.Stop(cInterface);
    //     // }

    //     // Configure and Start DataPlayer
    //     // to send phase alignment pattern
    //     // uint8_t cPhaseAlignmentPattern = 0x55;
    //     // cDPInterfacer.Configure(cInterface, cPhaseAlignmentPattern);
    //     // cDPInterfacer.Start(cInterface);
    //     // if(cDPInterfacer.IsRunning(cInterface)) { LOG(INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET; }
    //     // else
    //     //     LOG(INFO) << BOLDRED << "Could not start FE data player" << RESET;

    //     // align CIC inputs
    //     // CicFEAlignment cCicAligner;
    //     // cCicAligner.Inherit(&cHybridTester);
    //     // cCicAligner.PhaseAlignmentMPA(100);
    //     // cDPInterfacer.Stop(cInterface);
    //     // cDPInterfacer.CheckNPatterns(cInterface);

    //     // // still needs to be de-bugged!!
    //     // // does not work yet
    //     // // Configure and Start DataPlayer
    //     // // to send word alignment pattern
    //     // uint8_t cWordAlignmentPattern = 0x75;
    //     // cDPInterfacer.ConfigureEmulator(cInterface, cWordAlignmentPattern);
    //     // cDPInterfacer.StartEmulator(cInterface);
    //     // if( cDPInterfacer.EmulatorIsRunning(cInterface) )
    //     // {
    //     //     LOG (INFO) << BOLDBLUE << "FE data player " << BOLDGREEN << " running correctly!" << RESET;
    //     // }
    //     // else
    //     //     LOG (INFO) << BOLDRED << "Could not start FE data player" << RESET;

    //     // cCicAligner.WordAlignmentMPA(100);
    //     // reset all chip and board registers
    //     // to what they were before this tool was called
    //     // cCicAligner.dumpConfigFiles();
    // }

    // // // now go back to PS alignment and align inputs
    // // // need to do this if you're going to do any kind
    // // // of data tests
    // // cPSAlignment.Align();
    // // cPSAlignment.Reset();

    // if(cmd.foundOption("checkAsync") || cmd.foundOption("checkSync"))
    // {
    //     DataChecker cDataChecker;
    //     // front end type to tool
    //     FrontEndType cFrontEndType;
    //     if(cChipType == "MPA")
    //     {
    //         LOG(INFO) << "Checking data for MPAs only.." << RESET;
    //         cFrontEndType = FrontEndType::MPA;
    //     }
    //     else if(cChipType == "SSA")
    //     {
    //         LOG(INFO) << "Checking data for SSAs only.." << RESET;
    //         cFrontEndType = FrontEndType::SSA;
    //     }
    //     else
    //     {
    //         LOG(INFO) << "Checking data for SSAs only.." << RESET;
    //         cFrontEndType = FrontEndType::SSA;
    //     }
    //     auto cSelectFunction = [cFrontEndType](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == (FrontEndType)cFrontEndType); };
    //     cHybridTester.fDetectorContainer->setReadoutChipQueryFunction(cSelectFunction);
    //     cDataChecker.Inherit(&cHybridTester);
    //     cDataChecker.Initialise();
    //     if(cmd.foundOption("checkAsync"))
    //         cDataChecker.AsyncTest();
    //     else
    //         cDataChecker.ReadNeventsTest();
    //     // reset
    //     cHybridTester.fDetectorContainer->resetReadoutChipQueryFunction();
    //     cDataChecker.writeObjects();
    //     // cDataChecker.resetPointers();
    // }

    // // eye scan for CIC inputs
    // if(cmd.foundOption("eyeScanCic"))
    // {
    //     DataChecker cDataChecker;
    //     auto        cSelectFunction = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA); };
    //     cHybridTester.fDetectorContainer->setReadoutChipQueryFunction(cSelectFunction);
    //     cDataChecker.Inherit(&cHybridTester);
    //     cDataChecker.Initialise();
    //     cDataChecker.Eye_CIC();
    //     // reset
    //     cHybridTester.fDetectorContainer->resetReadoutChipQueryFunction();
    //     cDataChecker.writeObjects();
    // }

    // // // equalize thresholds on readout chips
    // if(cmd.foundOption("tuneOffsets"))
    // {
    //     t.start();
    //     // now create a PedestalEqualization object
    //     PedestalEqualization cPedestalEqualization;
    //     cPedestalEqualization.Inherit(&cHybridTester);
    //     // second parameter disables stub logic on CBC3
    //     cPedestalEqualization.Initialise(true, true);
    //     cPedestalEqualization.FindVplus();
    //     cPedestalEqualization.FindOffsets();
    //     cPedestalEqualization.writeObjects();
    //     cPedestalEqualization.dumpConfigFiles();
    //     cPedestalEqualization.resetPointers();
    //     t.show("Time to tune the front-ends on the system: ");
    // }
    // // measure noise on FE chips
    // if(cmd.foundOption("measurePedeNoise"))
    // {
    //     t.start();
    //     // if this is true, I need to create an object of type PedeNoise from the members of Calibration
    //     // tool provides an Inherit(Tool* pTool) for this purpose
    //     PedeNoise cPedeNoise;
    //     // hard coded for now
    //     FrontEndType cFrontEndType   = FrontEndType::MPA;
    //     auto         cSelectFunction = [cFrontEndType](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == (FrontEndType)cFrontEndType); };
    //     cHybridTester.fDetectorContainer->setReadoutChipQueryFunction(cSelectFunction);
    //     cPedeNoise.Inherit(&cHybridTester);
    //     // second parameter disables stub logic on CBC3
    //     cPedeNoise.Initialise(true, true); // canvases etc. for fast calibration
    //     cPedeNoise.measureNoise();
    //     cPedeNoise.writeObjects();
    //     cPedeNoise.dumpConfigFiles();
    //     t.stop();
    //     t.show("Time to Scan Pedestals and Noise");

    //     // reset
    //     cHybridTester.fDetectorContainer->resetReadoutChipQueryFunction();
    // }

    // if(cmd.foundOption("findOpens"))
    // {
    //     OpenFinder cOpenFinder;
    //     cOpenFinder.Inherit(&cHybridTester);
    //     cOpenFinder.FindOpensPS();
    // }
    // if(cmd.foundOption("findShorts"))
    // {
    //     ShortFinder cShortFinder;
    //     cShortFinder.Inherit(&cHybridTester);
    //     cShortFinder.Initialise();
    //     cShortFinder.FindShorts();
    // }
    // // test MPA outputs
    // if(cmd.foundOption("mpaTest"))
    // {
    //     cHybridTester.SelectCIC(true);
    //     // Configure and Start DataPlayer
    //     for(uint8_t cAttempt = 0; cAttempt < 1; cAttempt++)
    //     {
    //         // Check if data player is running
    //         if(cDPInterfacer.IsRunning(cInterface))
    //         {
    //             LOG(INFO) << BOLDBLUE << " STATUS : Data Player is running and will be stopped " << RESET;
    //             cDPInterfacer.Stop(cInterface);
    //         }

    //         if(cAttempt == 0)
    //             LOG(INFO) << BOLDBLUE << "Attempt " << +cAttempt << RESET;
    //         else if(cAttempt == 1)
    //             LOG(INFO) << BOLDGREEN << "Attempt " << +cAttempt << RESET;
    //         else if(cAttempt == 2)
    //             LOG(INFO) << BOLDMAGENTA << "Attempt " << +cAttempt << RESET;
    //         else if(cAttempt == 3)
    //             LOG(INFO) << BOLDYELLOW << "Attempt " << +cAttempt << RESET;

    //         cDPInterfacer.Configure(cInterface, cPattern);
    //         cDPInterfacer.Start(cInterface);
    //         cHybridTester.MPATest(cPattern);
    //         cDPInterfacer.Stop(cInterface);
    //         cDPInterfacer.CheckNPatterns(cInterface);
    //     }
    //     cHybridTester.SelectCIC(false);
    // }
    // // ssa pair tests
    // if(!cSSAPair.empty())
    // {
    //     LOG(INFO) << BOLDRED << "SSAOutput POGO debug" << RESET;
    //     // configure SSA to output something on stub lines
    //     cHybridTester.SSATestStubOutput(cSSAPair);
    //     // still needs to be debugged
    //     // configure SSA to output something on L1 lines
    //     // cHybridTester.SSATestL1Output(cSSAPair);
    //     // put it back in normal readout mode
    //     // and make sure we're in normal readout mode
    //     // i.e. synchronous
    //     // auto cNevents  = 9;//cTool.findValueInSettings("Nevents" ,10);
    //     // for(auto cBoard : *cHybridTester.fDetectorContainer)
    //     // {
    //     //     BeBoard* cBeBoard = static_cast<BeBoard*>( cBoard );
    //     //     for(auto cOpticalGroup : *cBoard)
    //     //     {
    //     //         for(auto cHybrid : *cOpticalGroup)
    //     //         {
    //     //             for (auto cReadoutChip : *cHybrid)
    //     //             {
    //     //                 if( cReadoutChip->getFrontEndType() != FrontEndType::SSA )
    //     //                     continue;
    //     //                 cHybridTester.fReadoutChipInterface->WriteChipReg(cReadoutChip, "Sync",1);
    //     //                 cHybridTester.fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern7/FIFOconfig",
    //     //                 0x3);
    //     //             }//chip
    //     //         }//hybrid
    //     //     }// hybrid
    //     //     // check if i can read anything
    //     //     for( uint32_t cThreshold=0; cThreshold < 20; cThreshold++)
    //     //     {
    //     //         cHybridTester.setSameDac("Threshold", cThreshold);
    //     //         LOG (INFO) << BOLDRED << "Threshold is " << +cThreshold << RESET;
    //     //         cHybridTester.ReadNEvents( cBeBoard , cNevents);
    //     //     }
    //     // }
    // }

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    T.stop();
    T.show("Total time = ");
    return 0;
}
