#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/Tool.h"
#include "tools/BackEndAlignment.h"

#include "../tools/PSROHTester.h"

#ifdef __USE_ROOT__
#include "TApplication.h"
#include "TROOT.h"
#endif

#define __NAMEDPIPE__

#ifdef __NAMEDPIPE__
#include "gui_logger.h"
#endif

#include <cstring>
#include <fstream>
#include <inttypes.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

#define CHIPSLAVE 4

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Data acquisition test and Data dump");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");
    cmd.defineOption("file", "Hw Description file. Default value: settings/D19CDescription_ROH_EFC7.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");
    // Load pattern
    cmd.defineOption("internal-pattern", "Internally Generated LpGBT Pattern", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("internal-pattern", "ip");
    cmd.defineOption("external-pattern", "Externlly Generated LpGBT Pattern using the Data Player for Control FC7", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("external-pattern", "ep");

    cmd.defineOption("cic-pattern", "Externlly Generated LpGBT Pattern using CIC output", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("cic-pattern", "cp");

    // Test Reset lines
    cmd.defineOption("testReset", "Test Reset lines");
    cmd.defineOptionAlternative("testReset", "r");
    // test I2C Masters
    cmd.defineOption("testI2C", "Test I2C LpGBT Masters on ROH");
    cmd.defineOptionAlternative("testI2C", "i");
    // test ADC channels
    cmd.defineOption("testADC", "Test LpGBT ADCs on ROH");
    cmd.defineOptionAlternative("testADC", "a");
    // run Eye Opening Monitor
    cmd.defineOption("eye-monitor", "Run Eye Opening Monitor test");
    cmd.defineOptionAlternative("eye-monitor", "eom");
    // run Bit Error Test
    cmd.defineOption("bit-error-rate", "Run Bit Error Rate test");
    cmd.defineOptionAlternative("bit-error-rate", "ber");
    cmd.defineOption("ber-pattern", "Define pattern to be used for Bit Error Rate test", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("ber-pattern", "bp");
    // clock test
    cmd.defineOption("clock-test", "Run clock tests", ArgvParser::NoOptionAttribute);
    // fast command test
    cmd.defineOption("scope-fcmd", "Scope fast commands [de-serialized]");
    cmd.defineOption("fcmd-pattern", "Injected pattern (simulates FCMD) on the DownLink", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("fcmd-pattern", "fp");
    //
    cmd.defineOption("fcmd-test", "Run fast command tests", ArgvParser::NoOptionAttribute);
    cmd.defineOption("fcmd-test-start-pattern", "Fast command FSM test start pattern", ArgvParser::OptionRequiresValue);
    cmd.defineOption("fcmd-test-userfile", "User file with fastcommands for testing", ArgvParser::OptionRequiresValue);
    // FCMD check in BRAM
    cmd.defineOption("bramfcmd-check", "Access to written data in BRAM", ArgvParser::OptionRequiresValue);
    // Write reference patterns to BRAM
    cmd.defineOption("bramreffcmd-write", "Write reference patterns to BRAM", ArgvParser::OptionRequiresValue);
    // convert user file to fw format
    cmd.defineOption("convert-userfile", "Convert user defined file to fw compliant format", ArgvParser::OptionRequiresValue);
    // read single ref FCMD BRAM addr
    cmd.defineOption("read-ref-bram", "Read single ref FCMD BRAM address", ArgvParser::OptionRequiresValue);
    // read single check FCMD BRAM addr
    cmd.defineOption("read-check-bram", "Read single check FCMD BRAM address", ArgvParser::OptionRequiresValue);
    // Flush check BRAM
    cmd.defineOption("clear-check-bram", "", ArgvParser::NoOptionAttribute);
    // Flush ref BRAM
    cmd.defineOption("clear-ref-bram", "", ArgvParser::NoOptionAttribute);
    // debug
    cmd.defineOption("debug", "Run debug", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("debug", "d");
    // scope
    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    int result = cmd.parse(argc, argv);
    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    std::string       cHWFile               = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19CDescription_ROH_OFC7.xml";
    bool              batchMode             = (cmd.foundOption("batch")) ? true : false;
    const std::string cSSAPair              = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    std::string       cDirectory            = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string       cHybridId             = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    bool              cDebug                = (cmd.foundOption("debug"));
    //Test to perform
    bool              cFCMDTest             = (cmd.foundOption("fcmd-test")) ? true : false;
    std::string       cFCMDTestStartPattern = (cmd.foundOption("fcmd-test-start-pattern")) ? cmd.optionValue("fcmd-test-start-pattern") : "11000001";
    std::string       cFCMDTestUserFileName = (cmd.foundOption("fcmd-test-userfile")) ? cmd.optionValue("fcmd-test-userfile") : "fcmd_file.txt";
    std::string       cBRAMFCMDLine         = (cmd.foundOption("bramfcmd-check")) ? cmd.optionValue("bramfcmd-check") : "fe_for_ps_roh_fcmd_SSA_l_check";
    std::string       cBRAMFCMDFileName     = (cmd.foundOption("bramreffcmd-write")) ? cmd.optionValue("bramreffcmd-write") : "fcmd_file.txt";
    std::string       cConvertUserFileName  = (cmd.foundOption("convert-userfile")) ? cmd.optionValue("convert-userfile") : "fcmd_file.txt";
    std::string       cRefBRAMAddr          = (cmd.foundOption("read-ref-bram")) ? cmd.optionValue("read-ref-bram") : "0";
    std::string       cCheckBRAMAddr        = (cmd.foundOption("read-check-bram")) ? cmd.optionValue("read-check-bram") : "0";

    cDirectory += Form("PS_ROH_%s", cHybridId.c_str());

    TApplication cApp("Root Application", &argc, argv);
    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    std::string cResultfile = "Hybrid";
    // Timer t;

    // Initialize and Configure Back-End (Optical) FC7
    Tool cTool;

    std::stringstream outp;
    LOG(INFO) << BOLDYELLOW << "Initializing FC7" << RESET;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
    LOG(INFO) << BOLDYELLOW << "Configuring FC7" << RESET;
    cTool.ConfigureHw();

    // Initialize BackEnd & Control LpGBT Tester
    PSROHTester cPSROHTester;
    cPSROHTester.Inherit(&cTool);
    cPSROHTester.FindUSBHandler();


    /***************/
    /* TEST UPLINK */
    /***************/
    if(cmd.foundOption("internal-pattern") || cmd.foundOption("external-pattern"))
    {
        /* INTERNALLY GENERATED PATTERN */
        if(cmd.foundOption("internal-pattern"))
        {
            uint8_t           cInternalPattern8     = (cmd.foundOption("internal-pattern")) ? convertAnyInt(cmd.optionValue("internal-pattern").c_str()) : 0;
            uint32_t          cInternalPattern32    = cInternalPattern8 << 24 | cInternalPattern8 << 16 | cInternalPattern8 << 8 | cInternalPattern8 << 0;
            cPSROHTester.LpGBTInjectULInternalPattern(cInternalPattern32);
            cPSROHTester.LpGBTCheckULPattern(false);
        }
        /* EXTERNALLY GENERATED PATTERN */
        else if(cmd.foundOption("external-pattern"))
        {
            uint8_t           cExternalPattern      = (cmd.foundOption("external-pattern")) ? convertAnyInt(cmd.optionValue("external-pattern").c_str()) : 0;
            cPSROHTester.LpGBTInjectULExternalPattern(true, cExternalPattern);
            cPSROHTester.LpGBTCheckULPattern(true);
            cPSROHTester.LpGBTInjectULExternalPattern(false, cExternalPattern);
        }
    }
    /****************************/
    /* TEST RESET LINES (GPIOs) */
    /****************************/
    if(cmd.foundOption("testReset"))
    {
        std::vector<std::pair<string, uint8_t>> cLevels = {{"High", 1}, {"Low", 0}};
        std::vector<uint8_t>                    cGPIOs  = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        for(auto cLevel: cLevels)
        {
            cPSROHTester.LpGBTSetGPIOLevel(cGPIOs, cLevel.second);
            bool cStatus = cPSROHTester.TestResetLines(cLevel.second);
            if(cStatus)
                LOG(INFO) << BOLDBLUE << "Set levels to " << cLevel.first << " : test " << BOLDGREEN << " passed." << RESET;
            else
                LOG(INFO) << BOLDRED << "Set levels to " << cLevel.first << " : test " << BOLDRED << " failed." << RESET;
        }
    }

    /********************/
    /* TEST I2C MASTERS */
    /********************/
    if(cmd.foundOption("testI2C"))
    {
        std::vector<uint8_t> cMasters = {0, 2};
        bool                 cStatus  = cPSROHTester.LpGBTTestI2CMaster(cMasters);
        if(cStatus)
            LOG(INFO) << BOLDBLUE << "I2C test " << BOLDGREEN << " passed" << RESET;
        else
            LOG(INFO) << BOLDBLUE << "I2C test " << BOLDRED << " failed" << RESET;
    }

    /**********************************/
    /* TEST ANALOG-DIGITAL-CONVERTERS */
    /**********************************/
    if(cmd.foundOption("testADC"))
    {
        std::vector<std::string> cADCs = {"ADC0", "ADC1", "ADC3"};
        cPSROHTester.LpGBTTestADC(cADCs, 0, 1000, 20);
    }

    /********************/
    /* TEST EYE OPENING */
    /********************/
    if(cmd.foundOption("eye-monitor"))
    {
        cPSROHTester.LpGBTRunEyeOpeningMonitor(7);
    }

    /***********************/
    /* TEST BIT ERROR RATE */
    /***********************/
    if(cmd.foundOption("bit-error-rate"))
    {
        uint32_t cBERTPattern32 = cmd.foundOption("ber-pattern") ? convertAnyInt(cmd.optionValue("ber-pattern").c_str()) : 0x00000000;
        uint8_t cCoarseSource = 1, cFineSource = 4, cMeasTime = 5;
        cPSROHTester.LpGBTRunBitErrorRateTest(cCoarseSource, cFineSource, cMeasTime, cBERTPattern32);
    }

    /***************/
    /* TEST CLOCKS */
    /***************/
    if(cmd.foundOption("clock-test"))
    {
        LOG(INFO) << BOLDBLUE << "Clock test" << RESET;
        cPSROHTester.CheckClocks();
    }

    /*********************/
    /* TEST FAST COMMAND */
    /*********************/
    if(cmd.foundOption("scope-fcmd"))
    {
        if(cmd.foundOption("fcmd-pattern")) 
        {
            uint8_t           cFCMDPattern          = (cmd.foundOption("fcmd-pattern")) ? convertAnyInt(cmd.optionValue("fcmd-pattern").c_str()) : 0;
            cPSROHTester.LpGBTInjectDLInternalPattern(cFCMDPattern);
        }
        cPSROHTester.FastCommandScope();
    }

    if(cDebug)
    {
        LOG(INFO) << "Start debugging" << RESET;
        cPSROHTester.PSROHInputsDebug();
    }

    if(cFCMDTest && !cFCMDTestStartPattern.empty() && !cFCMDTestUserFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Fast command test" << RESET;
        cPSROHTester.CheckFastCommands(cFCMDTestStartPattern, cFCMDTestUserFileName);
    }

    if(cmd.foundOption("bramfcmd-check") && !cBRAMFCMDLine.empty())
    {
        LOG(INFO) << BOLDBLUE << "Access to written data in BRAM" << RESET;
        cPSROHTester.CheckFastCommandsBRAM(cBRAMFCMDLine);
    }

    if(cmd.foundOption("bramreffcmd-write") && !cBRAMFCMDFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Write reference patterns to BRAM" << RESET;
        cPSROHTester.WritePatternToBRAM(cBRAMFCMDFileName);
    }

    if(cmd.foundOption("convert-userfile") && !cConvertUserFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Convert user file to fw compliant format" << RESET;
        cPSROHTester.UserFCMDTranslate(cConvertUserFileName);
    }

    if(cmd.foundOption("read-ref-bram"))
    {
        int cAddr = std::atoi(cRefBRAMAddr.c_str());
        LOG(INFO) << BOLDBLUE << "Read single ref FCMD BRAM address: " << cmd.optionValue("read-ref-bram") << RESET;
        cPSROHTester.ReadRefAddrBRAM(cAddr);
    }

    if(cmd.foundOption("read-check-bram"))
    {
        int cAddr = std::atoi(cCheckBRAMAddr.c_str());
        LOG(INFO) << BOLDBLUE << "Read single check FCMD BRAM address: " << cmd.optionValue("read-check-bram") << RESET;
        cPSROHTester.ReadCheckAddrBRAM(cAddr);
    }

    if(cmd.foundOption("clear-ref-bram"))
    {
        LOG(INFO) << BOLDBLUE << "Flushing ref BRAM!" << RESET;
        cPSROHTester.ClearBRAM(std::string("ref"));
    }

    if(cmd.foundOption("clear-check-bram"))
    {
        LOG(INFO) << BOLDBLUE << "Flushing check BRAM!" << RESET;
        cPSROHTester.ClearBRAM(std::string("test"));
    }

    // Save Result File
    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    // Destroy Tools
    cTool.Destroy();
    cTool.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
