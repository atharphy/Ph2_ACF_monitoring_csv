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

// reference volage for lpgBT 
float VREF_LPGBT = 1.0; 
float cConversionFactor = VREF_LPGBT/ 1024.;
                
#ifndef Measurement
typedef std::pair<float,float> Measurement;
#endif

std::vector<uint8_t> getSides(std::string pArgsStr )
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

    cmd.defineOption("powerSupply", "Name of the power supply as described in the HW file", ArgvParser::OptionRequiresValue);
    cmd.defineOption("enableCIC", "Disable CIC reset", ArgvParser::OptionRequiresValue);
    cmd.defineOption("enableCICclock", "Enable CIC clock", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("prepareCIC", "CIC start-up sequence", ArgvParser::NoOptionAttribute);
    //
    cmd.defineOption("registerTest","run register test", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("enableSSA", "Disable SSA reset", ArgvParser::OptionRequiresValue);
    cmd.defineOption("enableSSAclock", "Enable SSA clock", ArgvParser::OptionRequiresValue);
    cmd.defineOption("disableMPAclock", "Disable MPA clock for hybrid configuration 0", ArgvParser::OptionRequiresValue);
    
    //
    cmd.defineOption("enableMPA", "Disable MPA reset", ArgvParser::OptionRequiresValue);
    // 
    cmd.defineOption("configureCIC", "Apply default configuration to CIC", ArgvParser::NoOptionAttribute);
    cmd.defineOption("configureSSA", "Apply default configuration to SSA", ArgvParser::NoOptionAttribute);
    cmd.defineOption("configureMPA", "Apply default configuration to MPA", ArgvParser::NoOptionAttribute);
    cmd.defineOption("configureHybrid", "Apply default start-up sequence for hybrid", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("resetCIC", "Send a reset to the CIC", ArgvParser::NoOptionAttribute);
    cmd.defineOption("resetSSA", "Send a reset to the SSA", ArgvParser::NoOptionAttribute);
    cmd.defineOption("resetMPA", "Send a reset to the MPA", ArgvParser::NoOptionAttribute);
    cmd.defineOption("resetHybrid", "Send a reset to the hybrid", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("readoutRate", "Readout rate [320 or 640]", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("clockDriveCIC", "Clock drive strength for CIC", ArgvParser::OptionRequiresValue);
    cmd.defineOption("clockDriveSSA", "Clock drive strength for SSA", ArgvParser::OptionRequiresValue);
    //
    cmd.defineOption("monitor", "ADC monitoring", ArgvParser::OptionRequiresValue);
   
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
    uint16_t    cSsaClockDrive = (cmd.foundOption("clockDriveCIC")) ? convertAnyInt(cmd.optionValue("clockDriveSSA").c_str()) : 7;
    std::string cCicsToEnable = (cmd.foundOption("enableCIC")) ? cmd.optionValue("enableCIC") : "" ;
    std::string cCicsToClk = (cmd.foundOption("enableCICclock")) ? cmd.optionValue("enableCICclock") : "" ;
    std::string cSsasToEnable = (cmd.foundOption("enableSSA")) ? cmd.optionValue("enableSSA") : "" ;
    std::string cSsasToClk = (cmd.foundOption("enableSSAclock")) ? cmd.optionValue("enableSSAclock") : "" ;
    std::string cMPAsToEnable = (cmd.foundOption("enableMPA")) ? cmd.optionValue("enableMPA") : "" ;
    std::string cHybridsToReset = (cmd.foundOption("resetHybrid")) ? cmd.optionValue("resetHybrid") : "" ;  
    std::string cMPAsToDisableClock = (cmd.foundOption("disableMPAclock")) ? cmd.optionValue("resetHybrid") : "" ;  
    std::string cMonitor = (cmd.foundOption("monitor")) ? cmd.optionValue("monitor") : "none" ;
    uint16_t    cHybrifCnfg = (cmd.foundOption("configureHybrid")) ? convertAnyInt(cmd.optionValue("configureHybrid").c_str()) : 0;
    
    float cStartUpMontior=0;
    float cEndMonitor=0;
    std::string cChipType  = (cmd.foundOption("checkAsync")) ? cmd.optionValue("checkAsync") : "SSA";
    if(!(cmd.foundOption("checkAsync"))) cChipType = (cmd.foundOption("checkSync")) ? cmd.optionValue("checkSync") : "SSA";

    cDirectory += Form("FEH_PS_%s", cHybridId.c_str());

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    Timer       t, T;

    
    T.start();
    std::stringstream outp;
    // use a generic tool 
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
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
            
            // correct Vref
            // and monitor
            if( cmd.foundOption("monitor") )
            {
                // enable voltage 
                std::vector<std::string> cADCs_VoltageMonitors{"ADC1","ADC2","ADC6","ADC7","VDD"};
                std::vector<std::string> cADCs_Names{"1V_Monitor","12V_Monitor","1V25_Monitor","2V55_Monitor"};
                std::vector<std::string> cModuleSide{"left","left","right","left","internal"};
                std::vector<float>       cADCs_Refs{1.0, 12, 0.645 * 1.146  , 2.55, 1.25*0.42};
                // use Vddd as reference 
                size_t cIndx=4;
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", (1 << 4 ) );
                // find correction 
                std::vector<float> cVals(10,0);
                uint8_t cEnableVref=1;
                std::string cADCsel = cADCs_VoltageMonitors[cIndx];
                std::vector<uint8_t> cRefPoints{0, 0x05, 0x10, 0x20, 0x3F };
                std::vector<float> cMeasurements(0);
                std::vector<float> cSlopes(0);
                for( auto cRef : cRefPoints) 
                {
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, cRef);
                    for(size_t cM=0; cM < cVals.size(); cM++)
                    {
                        cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel)*cConversionFactor;
                    }
                    float cMean = std::accumulate(cVals.begin(),cVals.end(),0.)/cVals.size();
                    float cDifference_V = (cADCs_Refs[cIndx] - cMean );
                    LOG (DEBUG) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT "
                            << " correction applied is " << +cRef 
                            << " reading [mean] is "
                            << +cMean*1e3 
                            << " milli-volts."
                            << "\t...Difference between expected and measured "
                            << " values is "
                            << cDifference_V*1e3 
                            << " milli-volts." << RESET;

                    cMeasurements.push_back(cDifference_V);
                    if( cMeasurements.size() > 1 ) 
                    {
                        for(int cI=cMeasurements.size()-2; cI>=0; cI--)
                        {
                            float cSlope = (cMeasurements[cMeasurements.size()-1] - cMeasurements[cI])/(cRefPoints[cMeasurements.size()-1 ]-cRefPoints[cI]);
                            LOG (DEBUG) << BOLDBLUE << "Index " << +(cMeasurements.size()-1 )
                                << " -- index " << cI
                                << " slope is " << cSlope 
                                << RESET;
                            cSlopes.push_back(cSlope);
                        }
                    }
                }
                float cMeanSlope = std::accumulate(cSlopes.begin(),cSlopes.end(),0.)/cSlopes.size();
                float cIntcpt = cMeasurements[0]; 
                int cCorr = std::min( std::floor(-1.0*cIntcpt/cMeanSlope), 63. );
                LOG (DEBUG) << BOLDBLUE << "Mean slope is " << cMeanSlope 
                    << " , intercept is " << cIntcpt 
                    << " correction is " << cCorr
                    << RESET;
                // apply correction and check
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, (uint8_t)cCorr);
                for(size_t cM=0; cM < cVals.size(); cM++)
                {
                    cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel)*cConversionFactor;
                }
                // turn off ADC mon
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->WriteChipReg(clpGBT,"ADCMon", 0x00 );
                
                float cMean = std::accumulate(cVals.begin(),cVals.end(),0.)/cVals.size();
                float cDifference_V = std::fabs(cADCs_Refs[cIndx] - cMean );
                LOG (INFO) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT "
                            << +cMean*1e3 
                            << " milli-volts."
                            << "\t...Difference between expected and measured "
                            << " values is "
                            << cDifference_V*1e3 
                            << " milli-volts." << RESET;

                for( size_t cIndx=0; cIndx < cADCs_VoltageMonitors.size(); cIndx++)
                {
                    std::string cADCsel = cADCs_VoltageMonitors[cIndx];
                    if( cModuleSide[cIndx].find( cMonitor ) == std::string::npos ) continue;
                    
                    for(size_t cM=0; cM < cVals.size(); cM++)
                    {
                        cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel)*cConversionFactor;
                    }
                    float cMean = std::accumulate(cVals.begin(),cVals.end(),0.)/cVals.size();
                    cStartUpMontior = cMean;
                    LOG (INFO) << BOLDBLUE << "ADC_ " << cADCs_Names[cIndx] << " reading from lpGBT "
                        << +cMean*1e3 
                        << " milli-volts. This is monitored via the " << cModuleSide[cIndx]
                        << " side of the module" << RESET;
                }//read all monitors 
            } 
            // read all ADCs 
        }// configure lpGBT 

        
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;

            // de-activate reset for CIC 
            if( cmd.foundOption("enableCIC") && !cmd.foundOption("configureHybrid")) 
            {
                auto cSides = getSides(cCicsToEnable);

                for(auto cSide : cSides ) 
                {
                    LOG(INFO) << BOLDBLUE << "Disabling CIC reset [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicReset(clpGBT, false,cSide);
                }
            }
            // enable clock for CIC 
            if( cmd.foundOption("enableCICclock")  && !cmd.foundOption("configureHybrid")) 
            {
                // import from xml at some point 
                lpGBTClockConfig cClkCnfg; 
                cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                cClkCnfg.fClkDriveStr = cCicClockDrive; 
                cClkCnfg.fClkInvert = 0;
                cClkCnfg.fClkPreEmphWidth = 0; 
                cClkCnfg.fClkPreEmphMode = 0; 
                cClkCnfg.fClkPreEmphStr = 0;
                

                auto cSides = getSides(cCicsToClk);
                for(auto cSide : cSides ) 
                {
                    LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);
                }
            }
            
            // de-activate reset for SSA 
            if( cmd.foundOption("enableSSA")  && !cmd.foundOption("configureHybrid")) 
            {
                auto cSides = getSides( cSsasToEnable );
                for(auto cSide : cSides ) 
                {
                    LOG(INFO) << BOLDBLUE << "Disabling SSA reset [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ssaReset(clpGBT, false,cSide);
                }
            }
            // de-activate reset for MPA 
            if( cmd.foundOption("enableMPA")  && !cmd.foundOption("configureHybrid")) 
            {
                auto cSides = getSides( cMPAsToEnable );
                for(auto cSide : cSides ) 
                {
                    LOG(INFO) << BOLDBLUE << "Disabling MPA reset [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->mpaReset(clpGBT, false,cSide);
                }
            }
            
            
            if( cmd.foundOption("enableSSAclock")  && !cmd.foundOption("configureHybrid")) 
            {
                auto cSides = getSides( cSsasToClk );
                
                lpGBTClockConfig cClkCnfg; 
                cClkCnfg.fClkFreq = 4;  
                cClkCnfg.fClkDriveStr = cSsaClockDrive; 
                cClkCnfg.fClkInvert = 1;
                cClkCnfg.fClkPreEmphWidth = 0; 
                cClkCnfg.fClkPreEmphMode = 0; 
                cClkCnfg.fClkPreEmphStr = 0;
                for(auto cSide : cSides ) 
                {
                    LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                }
            }
            if( cmd.foundOption("resetHybrid"))
            {
                auto cSides = getSides( cHybridsToReset );
                
                // then send reset 
                for(auto cSide : cSides ) 
                {
                    LOG (INFO) << BOLDBLUE  << "Resetting hybrid [Side == " << +cSide  << "]" << RESET;
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetMPA(clpGBT,cSide);
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetSSA(clpGBT,cSide);
                    static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetCic(clpGBT,cSide);
                }
            }
            if( cmd.foundOption("resetSSA"))
            {
                // then send reset 
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetSSA(clpGBT);
            }
            if( cmd.foundOption("resetMPA"))
            {
                // then send reset 
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetMPA(clpGBT);
            }
            if( cmd.foundOption("resetCIC"))
            {
                // then send reset 
                static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetCic(clpGBT);
            }
            
        } // enable ROCs 

        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT =  cOpticalGroup->flpGBT ;
            if(clpGBT == nullptr) continue;

            // CIC configure 
            if( cmd.foundOption("configureCIC") && !cmd.foundOption("configureHybrid"))
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

            if( cmd.foundOption("configureSSA") && !cmd.foundOption("configureHybrid"))
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


            if( cmd.foundOption("configureHybrid") ) 
            {
                if( cHybrifCnfg == 0 )
                {
                    // now .. configure all SSAs 
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // first .. send clock to the SSAs on this hybrid  
                        uint8_t cSide=cHybrid->getId()%2;
                        
                        lpGBTClockConfig cClkCnfg; 
                        cClkCnfg.fClkFreq = 4;  
                        cClkCnfg.fClkDriveStr = cSsaClockDrive; 
                        cClkCnfg.fClkInvert = 1;
                        cClkCnfg.fClkPreEmphWidth = 0; 
                        cClkCnfg.fClkPreEmphMode = 0; 
                        cClkCnfg.fClkPreEmphStr = 0;
                        LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        // then .. reset SSAs on this hybrid  
                        // reset is asynchronous but .. I prefer resetting after the clock is there 
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetSSA(clpGBT, cSide);

                        // Configure SSAs on this hybrid 
                        std::vector<uint8_t> pIds(0);
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                            {
                                LOG(INFO) << BOLDBLUE << "Configuring SSA [chip id " << +cChip->getId() << " ]" << RESET;
                                cTool.fReadoutChipInterface->ConfigureChip(cChip);
                                pIds.push_back( cChip->getId() );
                            }//SSAs
                        }//ROCs

                        // now .. reset MPAs on this hybrid 
                        if(cmd.foundOption("configureMPA") )
                        {
                            LOG (INFO) << BOLDBLUE << "Resetting MPA before configuration.." << RESET;
                            static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetMPA(clpGBT, cSide);
                        }

                        for( auto cId : pIds )
                        {
                            // first . . enable clock out to one MPA at a time 
                            for(auto cReadoutChip: *cHybrid)
                            {
                                if( cmd.foundOption("configureMPA") && cReadoutChip->getFrontEndType() == FrontEndType::SSA && cReadoutChip->getId() == cId)
                                {
                                    LOG (INFO) << BOLDBLUE << "Setting SLVS_pad_current on SSA#" << +cId << " to 0x07" << RESET;
                                    cTool.fReadoutChipInterface->WriteChipReg(cReadoutChip,"SLVS_pad_current",0x7);
                                }//SSAs
                                else if(cReadoutChip->getFrontEndType() == FrontEndType::SSA && cReadoutChip->getId() == cId)
                                {
                                    LOG (INFO) << BOLDBLUE << "Setting SLVS_pad_current on SSA#" << +cId << " to 0x00" << RESET;
                                    cTool.fReadoutChipInterface->WriteChipReg(cReadoutChip,"SLVS_pad_current",0x0);
                                }//SSAs
                            }//ROCs

                            // then .. configure that MPA 
                            for(auto cReadoutChip: *cHybrid)
                            {
                                if(cmd.foundOption("configureMPA") && !cmd.foundOption("disableMPAclock") && cReadoutChip->getFrontEndType() == FrontEndType::MPA && cReadoutChip->getId() == cId)
                                {
                                    LOG (INFO) << BOLDBLUE << "Configuring MPA#" << +cId << RESET;
                                    cTool.fReadoutChipInterface->ConfigureChip(cReadoutChip);
                                }//MPAs
                            }//ROCs
                        }

                        // disable clock from SSA 
                        if( cmd.foundOption("disableMPAclock") )
                        {
                            cClkCnfg.fClkFreq = 0;  
                            cClkCnfg.fClkDriveStr = 0;
                            LOG (INFO) << BOLDBLUE << "Disabling SSA clock " << RESET;
                            static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        }

                        // enable clock to CIC 
                        cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                        cClkCnfg.fClkDriveStr = cCicClockDrive; 
                        cClkCnfg.fClkInvert = 0;
                        cClkCnfg.fClkPreEmphWidth = 0; 
                        cClkCnfg.fClkPreEmphMode = 0; 
                        cClkCnfg.fClkPreEmphStr = 0;
                        LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);

                        // now .. reset CICs on this hybrid 
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->resetCic(clpGBT, cSide);


                        LOG(INFO) << BOLDBLUE << "Configuring CIC(s)" << RESET;
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        cTool.fCicInterface->ConfigureChip(cCic);
                        static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();

                        // CIC start-up
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
                    }//OG
                }//safe
                else if( cHybrifCnfg == 1 )
                {
                    // now .. configure all SSAs 
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // first .. send clock to the SSAs on this hybrid  
                        uint8_t cSide=cHybrid->getId()%2;
                        
                        lpGBTClockConfig cClkCnfg; 
                        cClkCnfg.fClkFreq = 4;  
                        cClkCnfg.fClkDriveStr = cSsaClockDrive; 
                        cClkCnfg.fClkInvert = 1;
                        cClkCnfg.fClkPreEmphWidth = 0; 
                        cClkCnfg.fClkPreEmphMode = 0; 
                        cClkCnfg.fClkPreEmphStr = 0;
                        LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                        cClkCnfg.fClkDriveStr = cCicClockDrive; 
                        LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);

                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->mpaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ssaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicReset(clpGBT, false,cSide);

                        // Configure SSAs+MPAs on this hybrid 
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                                LOG(INFO) << BOLDBLUE << "Configuring SSA [chip id " << +cChip->getId() << " ]" << RESET;
                            else
                                LOG(INFO) << BOLDBLUE << "Configuring MPA [chip id " << +cChip->getId() << " ]" << RESET;
                            
                            cTool.fReadoutChipInterface->ConfigureChip(cChip);
                        }//ROCs

                        // disable clock from SSA 
                        if( !cmd.foundOption("enableSSAclock") )
                        {
                            cClkCnfg.fClkFreq = 0;  
                            cClkCnfg.fClkDriveStr = 0;
                            LOG (INFO) << BOLDBLUE << "Disabling SSA clock " << RESET;
                            static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        }

                        // configuring CIC 
                        LOG(INFO) << BOLDBLUE << "Configuring CIC(s)" << RESET;
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        cTool.fCicInterface->ConfigureChip(cCic);
                        static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();

                        // CIC start-up
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
                    }//OG
                }//inter
                else if( cHybrifCnfg == 2 )
                {
                    auto cMPAsToDisable  = getSides(cMPAsToDisableClock);
                    // now .. configure all SSAs 
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // first .. send clock to the SSAs on this hybrid  
                        uint8_t cSide=cHybrid->getId()%2;
                        
                        lpGBTClockConfig cClkCnfg; 
                        cClkCnfg.fClkFreq = 4;  
                        cClkCnfg.fClkDriveStr = cSsaClockDrive; 
                        cClkCnfg.fClkInvert = 1;
                        cClkCnfg.fClkPreEmphWidth = 0; 
                        cClkCnfg.fClkPreEmphMode = 0; 
                        cClkCnfg.fClkPreEmphStr = 0;
                        LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                        cClkCnfg.fClkDriveStr = cCicClockDrive; 
                        LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);

                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->mpaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ssaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicReset(clpGBT, false,cSide);

                        // Configure SSAs+MPAs on this hybrid 
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                                LOG(INFO) << BOLDBLUE << "Configuring SSA [chip id " << +cChip->getId() << " ]" << RESET;
                            else
                                LOG(INFO) << BOLDBLUE << "Configuring MPA [chip id " << +cChip->getId() << " ]" << RESET;
                            
                            cTool.fReadoutChipInterface->ConfigureChip(cChip);
                        }//ROCs

                        // disable clock from one MPA 
                        if( cmd.foundOption("disableMPAclock") )
                        {

                            // first . . all connected SSAs produce a clock 
                            for(auto cReadoutChip: *cHybrid)
                            {
                                if( cReadoutChip->getFrontEndType() == FrontEndType::SSA )
                                {
                                    LOG (INFO) << BOLDBLUE << "Setting SLVS_pad_current on SSA#" << +cReadoutChip->getId() << " to 0x07" << RESET;
                                    cTool.fReadoutChipInterface->WriteChipReg(cReadoutChip,"SLVS_pad_current",0x7);
                                }//SSAs
                            }//ROC

                            // then disable selected 
                            for(auto cMpaId : cMPAsToDisable )
                            {
                               for(auto cReadoutChip: *cHybrid)
                                {
                                    if( cReadoutChip->getFrontEndType() == FrontEndType::SSA && cReadoutChip->getId() == cMpaId)
                                    {
                                        LOG (INFO) << BOLDBLUE << "Setting SLVS_pad_current on SSA#" << +cMpaId << " to 0x00" << RESET;
                                        cTool.fReadoutChipInterface->WriteChipReg(cReadoutChip,"SLVS_pad_current",0x0);
                                    }//SSAs
                                }//ROCs
                            }
                        }

                        // configuring CIC 
                        LOG(INFO) << BOLDBLUE << "Configuring CIC(s)" << RESET;
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        cTool.fCicInterface->ConfigureChip(cCic);
                        static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();

                        // CIC start-up
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
                    }//OG
                }//inter2
                else if( cHybrifCnfg == 3 )
                {
                    // now .. configure all SSAs 
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        // first .. send clock to the SSAs on this hybrid  
                        uint8_t cSide=cHybrid->getId()%2;
                        
                        lpGBTClockConfig cClkCnfg; 
                        cClkCnfg.fClkFreq = 4;  
                        cClkCnfg.fClkDriveStr = cSsaClockDrive; 
                        cClkCnfg.fClkInvert = 1;
                        cClkCnfg.fClkPreEmphWidth = 0; 
                        cClkCnfg.fClkPreEmphMode = 0; 
                        cClkCnfg.fClkPreEmphStr = 0;
                        LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);
                        cClkCnfg.fClkFreq = (cReadoutRate == 320) ? 4 : 5; 
                        cClkCnfg.fClkDriveStr = cCicClockDrive; 
                        LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide  << "]" << RESET;
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);

                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->mpaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ssaReset(clpGBT, false,cSide);
                        static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->cicReset(clpGBT, false,cSide);

                        // configuring CIC 
                        LOG(INFO) << BOLDBLUE << "Configuring CIC(s)" << RESET;
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        cTool.fCicInterface->ConfigureChip(cCic);
                        static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();

                        // CIC start-up
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


                        // Configure SSAs+MPAs on this hybrid 
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                                LOG(INFO) << BOLDBLUE << "Configuring SSA [chip id " << +cChip->getId() << " ]" << RESET;
                            else
                                LOG(INFO) << BOLDBLUE << "Configuring MPA [chip id " << +cChip->getId() << " ]" << RESET;
                            
                            cTool.fReadoutChipInterface->ConfigureChip(cChip);
                        }//ROCs
                    }//OG
                }//original 
            }// configure hybrid 
        } // configure ROCs + CICs

        if( cmd.foundOption("configureSSA")&& !cmd.foundOption("configureHybrid"))     
            static_cast<SSAInterface*>(cTool.fReadoutChipInterface)->printErrorSummary();
        
        if( cmd.foundOption("monitor") )
        {
            for(auto cOpticalGroup: *cBoard)
            {
                auto& clpGBT =  cOpticalGroup->flpGBT ;
                if(clpGBT == nullptr) continue;

                // enable voltage 
                std::vector<std::string> cADCs_VoltageMonitors{"ADC1","ADC2","ADC6","ADC7","VDD"};
                std::vector<std::string> cADCs_Names{"1V_Monitor","12V_Monitor","1V25_Monitor","2V55_Monitor"};
                std::vector<std::string> cModuleSide{"left","left","right","left","internal"};
                std::vector<float>       cADCs_Refs{1.0, 12, 0.645 * 1.146  , 2.55, 1.25*0.42};
                
                LOG (INFO) << BOLDBLUE << "Reading monitoring voltaages : " << RESET;
                for( size_t cIndx=0; cIndx < cADCs_VoltageMonitors.size(); cIndx++)
                {
                    std::string cADCsel = cADCs_VoltageMonitors[cIndx];
                    if( cModuleSide[cIndx].find( cMonitor ) == std::string::npos ) continue;
                    
                    std::vector<float> cVals(10,0);
                    for(size_t cM=0; cM < cVals.size(); cM++)
                    {
                        cVals[cM] = static_cast<D19clpGBTInterface*>(cTool.flpGBTInterface)->ReadADC(clpGBT, cADCsel)*cConversionFactor;
                    }
                    float cMean = std::accumulate(cVals.begin(),cVals.end(),0.)/cVals.size();
                    float cSqSum = std::inner_product(cVals.begin(), cVals.end(), cVals.begin(), 0.0);
                    float cStdDev = std::sqrt(cSqSum / cVals.size() - cMean * cMean);


                    cEndMonitor = cMean;
                    LOG (INFO) << BOLDBLUE << "ADC_ " << cADCs_Names[cIndx] << " reading from lpGBT "
                        << +cMean*1e3 
                        << " milli-volt [ RMS = "
                        << cStdDev*1e3 << " ] milli-volts." 
                        << RESET;
                }//read all monitors 
            }// configure lpGBT 
        }

        if( cmd.foundOption("registerTest"))
        {
            // reset error summaries
            static_cast<CicInterface*>(cTool.fCicInterface)->resetErrorSummary();
            // try and configure all chips N times 
            for( size_t cAttempt=0; cAttempt < cConfigurationAttempts; cAttempt++)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        OuterTrackerHybrid* cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                        auto& cCic = cOuterTrackerHybrid->fCic;
                        LOG (INFO) << BOLDBLUE << "Configuring CIC [ Attempt#"<< +cAttempt << " ]" << RESET;
                        cTool.fCicInterface->ConfigureChip(cCic);
                    }//OG
                }//board
            }// configuration attempts 

            // summarize 
            static_cast<CicInterface*>(cTool.fCicInterface)->printErrorSummary();
            auto cCicErrSummary = static_cast<CicInterface*>(cTool.fCicInterface)->getReadBackErrorSummary();
            auto cCicWriteErrSummary = static_cast<CicInterface*>(cTool.fCicInterface)->getWriteErrorSummary();
            float cCicCrct = (float)(cCicErrSummary.second - cCicErrSummary.first); 
            float cCicCrctW = (float)(cCicWriteErrSummary.second - cCicWriteErrSummary.first); 
            LOG (INFO) << BOLDBLUE << "CIC register read-back successes : " << cCicCrct << " out of " << cCicErrSummary.second << RESET;
            LOG (INFO) << BOLDBLUE << "CIC register write successes : " << cCicCrctW << " out of " << cCicWriteErrSummary.second << RESET;
        }// register test
        

        if( cmd.foundOption("monitor") )
        {
            LOG (INFO) << BOLDBLUE << "Start-up .. monitor : " 
                << cStartUpMontior*1e3  << " mV, after configuration monitor shows " 
                << cEndMonitor*1e3  << " mV." << RESET;
        }
    }

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    T.stop();
    T.show("Total time = ");
    return 0;
}
