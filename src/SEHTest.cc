#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/Tool.h"
//#include "../Utils/gui_logger.h"
#include "tools/BackEndAlignment.h"

#include "../tools/SEHTester.h"

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
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("Binary to test a 2S-SEH with a test card");
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
    cmd.defineOption("external-pattern",
                     "Externally Generated LpGBT Pattern using the Data Player for Control FC7; Also, an automated comparision is performed",
                     ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("external-pattern", "ep");

    cmd.defineOption("cic-pattern", "Externally Generated LpGBT Pattern using CIC output", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequires*/);
    cmd.defineOptionAlternative("cic-pattern", "cp");

    // Test Reset lines
    cmd.defineOption("testReset", "Test Reset lines");
    cmd.defineOptionAlternative("testReset", "r");
    // test I2C Masters
    cmd.defineOption("testI2C", "Test I2C LpGBT Masters on SEH");
    cmd.defineOptionAlternative("testI2C", "i");
    // test ADC channels
    cmd.defineOption("testADC", "Test LpGBT ADCs on SEH");
    cmd.defineOptionAlternative("testADC", "a");
    // clock test
    cmd.defineOption("clock-test", "Run clock tests", ArgvParser::NoOptionAttribute);
    // fast command test
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
    cmd.defineOption("scope-fcmd", "Scope fast commands [de-serialized]");
    // efficiency
    cmd.defineOption("eff", "Measure the DC/DC efficiency");
    // Test VTRx+ registers
    cmd.defineOption("powersupply", "Use remote control of the 10V power supply in order to ramp up the voltage", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("powersupply", "ps");
    // Bias voltage leakage current
    cmd.defineOption("leak", "Measure the Bias voltage leakage current ", ArgvParser::OptionRequiresValue);
    // Bias voltage on sensor side
    cmd.defineOption("bias", "Measure the Bias voltage on sensor side ", ArgvParser::OptionRequiresValue);
    // Load values defining a test from file
    cmd.defineOption("ext-leak", "Measure the Bias voltage leakage current using an external power supply", ArgvParser::OptionRequiresValue);
    // Bias voltage on sensor side
    cmd.defineOption("ext-bias", "Measure the Bias voltage on sensor side using an external power supply", ArgvParser::OptionRequiresValue);
    // Load values defining a test from file
    cmd.defineOption("test-parameter", "Use user file with test parameters, otherwise (or if file is missing it) default parameters will be used", ArgvParser::OptionRequiresValue);
    // Test VTRx+ registers
    cmd.defineOption("testVTRxplus", "Test testVTRx+ slow control");
    cmd.defineOptionAlternative("testVTRxplus", "v");
    // general
    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");
    // Naming
    cmd.defineOption("output", "Result File directory", ArgvParser::OptionRequiresValue);
    cmd.defineOption("hybridId", "Hybrid ID", ArgvParser::OptionRequiresValue);

    int result = cmd.parse(argc, argv);
    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    std::string       cHWFile               = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19CDescription_ROH_OFC7.xml";
    std::string       cPowerSupply          = (cmd.foundOption("powersupply")) ? cmd.optionValue("powersupply") : "";
    bool              batchMode             = (cmd.foundOption("batch")) ? true : false;
    const std::string cSSAPair              = (cmd.foundOption("ssapair")) ? cmd.optionValue("ssapair") : "";
    std::string       cDirectory            = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
    std::string       cHybridId             = (cmd.foundOption("hybridId")) ? cmd.optionValue("hybridId") : "xxxx";
    bool              cDebug                = (cmd.foundOption("debug"));
    bool              cClockTest            = (cmd.foundOption("clock-test")) ? true : false;
    bool              cFCMDTest             = (cmd.foundOption("fcmd-test")) ? true : false;
    std::string       cFCMDTestStartPattern = (cmd.foundOption("fcmd-test-start-pattern")) ? cmd.optionValue("fcmd-test-start-pattern") : "11000001";
    std::string       cFCMDTestUserFileName = (cmd.foundOption("fcmd-test-userfile")) ? cmd.optionValue("fcmd-test-userfile") : "fcmd_file.txt";
    std::string       cBRAMFCMDLine         = (cmd.foundOption("bramfcmd-check")) ? cmd.optionValue("bramfcmd-check") : "fe_for_ps_roh_fcmd_SSA_l_check";
    std::string       cBRAMFCMDFileName     = (cmd.foundOption("bramreffcmd-write")) ? cmd.optionValue("bramreffcmd-write") : "fcmd_file.txt";
    std::string       cConvertUserFileName  = (cmd.foundOption("convert-userfile")) ? cmd.optionValue("convert-userfile") : "fcmd_file.txt";
    std::string       cRefBRAMAddr          = (cmd.foundOption("read-ref-bram")) ? cmd.optionValue("read-ref-bram") : "0";
    std::string       cCheckBRAMAddr        = (cmd.foundOption("read-check-bram")) ? cmd.optionValue("read-check-bram") : "0";
    // uint8_t           cExternalPattern       = (cmd.foundOption("external-pattern")) ? convertAnyInt(cmd.optionValue("external-pattern").c_str()) : 0;
    // uint8_t           cInternalPattern8      = (cmd.foundOption("internal-pattern")) ? convertAnyInt(cmd.optionValue("internal-pattern").c_str()) : 0;
    // uint32_t          cInternalPattern32     = cInternalPattern8 << 24 | cInternalPattern8 << 16 | cInternalPattern8 << 8 | cInternalPattern8 << 0;
    uint8_t     cFCMDPattern           = (cmd.foundOption("fcmd-pattern")) ? convertAnyInt(cmd.optionValue("fcmd-pattern").c_str()) : 0;
    std::string cTestParameterFileName = (cmd.foundOption("test-parameter")) ? cmd.optionValue("test-parameter") : "testParameters.txt";
    uint16_t    cBiasVoltage           = (cmd.foundOption("bias")) ? convertAnyInt(cmd.optionValue("bias").c_str()) : 0;
    uint16_t    cLeakVoltage           = (cmd.foundOption("leak")) ? convertAnyInt(cmd.optionValue("leak").c_str()) : 0;
    uint16_t    cExtLeakVoltage        = (cmd.foundOption("ext-leak")) ? convertAnyInt(cmd.optionValue("ext-leak").c_str()) : 0;
    cDirectory += Form("2S_SEH_%s", cHybridId.c_str());

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
    LOG(INFO) << BOLDYELLOW << "Initializing Settings" << RESET;
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << BOLDYELLOW << "Initializing FC7" << RESET;
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.CreateResultDirectory(cDirectory);
    cTool.InitResultFile(cResultfile);
    cTool.bookSummaryTree();
    LOG(INFO) << BOLDYELLOW << "Configuring FC7" << RESET;
    SEHTester cSEHTester;
    cSEHTester.Inherit(&cTool);

    cSEHTester.FindUSBHandler();
    cSEHTester.TurnOn();
    uint8_t cExternalPattern = (cmd.foundOption("external-pattern")) ? convertAnyInt(cmd.optionValue("external-pattern").c_str()) : 0;
    cSEHTester.LpGBTInjectULExternalPattern(true, cExternalPattern);
    cTool.ConfigureHw();
    // Initialize BackEnd & Control LpGBT Tester
    // cSEHTester.exampleFit();
    // cSEHTester.DCDCOutputEvaluation();
    if(cmd.foundOption("powersupply"))
    {
        LOG(INFO) << BOLDYELLOW << "Switching on SEH using remote power supply control" << RESET;
        cSEHTester.TurnOn();
        cSEHTester.RampPowerSupply("MyRohdeSchwarz", "LV_Module3");
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Switching on SEH without remote power supply control" << RESET;
        cSEHTester.TurnOn();
    }

    // cSEHTester.TestCardVoltages();
    if(cmd.foundOption("test-parameter"))
    {
        cSEHTester.readTestParameters(cTestParameterFileName);
        LOG(INFO) << BOLDYELLOW << "You are using the parameters from " << cTestParameterFileName << " if provided there" << RESET;
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "You are using the default parameter set stored in fDefaultParameters" << RESET;
    }

    /*******************/
    /*   TEST UPLINK   */
    /* E-links CIC_OUT */
    /*******************/
    if(cmd.foundOption("internal-pattern") || cmd.foundOption("external-pattern"))
    {
        /* INTERNALLY GENERATED PATTERN */
        if(cmd.foundOption("internal-pattern"))
        {
            uint8_t  cInternalPattern8  = (cmd.foundOption("internal-pattern")) ? convertAnyInt(cmd.optionValue("internal-pattern").c_str()) : 0;
            uint32_t cInternalPattern32 = cInternalPattern8 << 24 | cInternalPattern8 << 16 | cInternalPattern8 << 8 | cInternalPattern8 << 0;
            cSEHTester.LpGBTInjectULInternalPattern(cInternalPattern32);
            cSEHTester.LpGBTCheckULPattern(false);
        }
        /* EXTERNALLY GENERATED PATTERN */
        else if(cmd.foundOption("external-pattern"))
        {
            // cSEHTester.LpGBTInjectULExternalPattern(true, cExternalPattern);
            bool cStatus = cSEHTester.LpGBTCheckULPattern(true, cExternalPattern);
            cSEHTester.LpGBTInjectULExternalPattern(false, cExternalPattern);
            if(cStatus) { LOG(INFO) << BOLDGREEN << "CIC_Out test passed." << RESET; }
            else
            {
                LOG(INFO) << BOLDRED << "CIC_Out test failed." << RESET;
            }
        }
    }

    /****************************/
    /* TEST RESET LINES (GPIOs) */
    /*     And test GPIs        */
    /****************************/
    if(cmd.foundOption("testReset"))
    {
        bool cStatus = cSEHTester.LpGBTTestResetLines();
        if(cStatus) { LOG(INFO) << BOLDGREEN << "Reset test passed." << RESET; }
        else
        {
            LOG(INFO) << BOLDRED << "Reset test failed." << RESET;
        }
        cStatus = cSEHTester.LpGBTTestGPILines();
        if(cStatus) { LOG(INFO) << BOLDGREEN << "Power Good test passed." << RESET; }
        else
        {
            LOG(INFO) << BOLDRED << "Power Good test failed." << RESET;
        }
    }

    /****************************/
    /*  Test VTRx+ slow control */
    /****************************/
    if(cmd.foundOption("testVTRxplus"))
    {
        bool cStatus = cSEHTester.LpGBTTestVTRx();

        if(cStatus)
            LOG(INFO) << BOLDBLUE << "VTRx+ slow control test passed." << RESET;
        else
            LOG(INFO) << BOLDRED << "VTRx+ slow control test failed." << RESET;
    }

    /****************************/
    /*  Test LpGBT I2C Masters */
    /****************************/
    if(cmd.foundOption("testI2C"))
    {
        std::vector<uint8_t> cMasters = {0, 2};
        bool                 cStatus  = cSEHTester.LpGBTTestI2CMaster(cMasters);
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
        // cSEHTester.ToyTestFixedADCs();
        cSEHTester.LpGBTTestFixedADCs();
        std::vector<std::string> cADCs = {"ADC0", "ADC3"};
        cSEHTester.LpGBTTestADC(cADCs, 0, 0xe00, 300); // DAC *should* be 16 bit with 1V reference, ROH is 12 bit something, needs to be included somewhere
    }

    // Test Fast Commands
    if(cDebug)
    {
        LOG(INFO) << "Start debugging" << RESET;
        cSEHTester.SEHInputsDebug();
    }

    if(cClockTest)
    {
        LOG(INFO) << BOLDBLUE << "Clock test" << RESET;
        cSEHTester.CheckClocks();
    }

    if(cmd.foundOption("scope-fcmd"))
    {
        if(cmd.foundOption("fcmd-pattern"))
        {
            LOG(INFO) << BOLDBLUE << "FCMD pattern test" << RESET;
            cSEHTester.LpGBTInjectDLInternalPattern(cFCMDPattern);
            cSEHTester.LpGBTFastCommandChecker(cFCMDPattern);
        }
        else
        {
            cSEHTester.FastCommandScope();
        }
    }

    if(cmd.foundOption("eff"))
    {
        // cSEHTester.exampleFit();
        LOG(INFO) << BOLDBLUE << "Efficiency Test" << RESET;
        cSEHTester.TestEfficiency(0, 2500, 500);
    }

    if(cmd.foundOption("leak"))
    {
        LOG(INFO) << BOLDBLUE << "Measuring leakage current" << RESET;
        cSEHTester.TestLeakageCurrent(cLeakVoltage, 600);
    }

    if(cmd.foundOption("bias"))
    {
        LOG(INFO) << BOLDBLUE << "Measuring bias voltage on sensor side" << RESET;
        cSEHTester.TestBiasVoltage(cBiasVoltage);
    }

    if(cmd.foundOption("ext-leak"))
    {
        LOG(INFO) << BOLDBLUE << "Measuring leakage current with external power supply" << RESET;
        cSEHTester.ExternalTestLeakageCurrent(cExtLeakVoltage, 600, "MyIsegSHR4220", "HV_Module1");
    }

    if(cmd.foundOption("ext-bias"))
    {
        LOG(INFO) << BOLDBLUE << "Measuring bias voltage on sensor side with external power supply" << RESET;
        cSEHTester.ExternalTestBiasVoltage("MyIsegSHR4220", "HV_Module1");
    }

    if(cFCMDTest && !cFCMDTestStartPattern.empty() && !cFCMDTestUserFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Fast command test" << RESET;
        cSEHTester.CheckFastCommands(cFCMDTestStartPattern, cFCMDTestUserFileName);
    }

    if(cmd.foundOption("bramfcmd-check") && !cBRAMFCMDLine.empty())
    {
        LOG(INFO) << BOLDBLUE << "Access to written data in BRAM" << RESET;
        cSEHTester.CheckFastCommandsBRAM(cBRAMFCMDLine);
    }

    if(cmd.foundOption("bramreffcmd-write") && !cBRAMFCMDFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Write reference patterns to BRAM" << RESET;
        cSEHTester.WritePatternToBRAM(cBRAMFCMDFileName);
    }

    if(cmd.foundOption("convert-userfile") && !cConvertUserFileName.empty())
    {
        LOG(INFO) << BOLDBLUE << "Convert user file to fw compliant format" << RESET;
        cSEHTester.UserFCMDTranslate(cConvertUserFileName);
    }

    if(cmd.foundOption("read-ref-bram"))
    {
        int cAddr = std::atoi(cRefBRAMAddr.c_str());
        LOG(INFO) << BOLDBLUE << "Read single ref FCMD BRAM address: " << cmd.optionValue("read-ref-bram") << RESET;
        cSEHTester.ReadRefAddrBRAM(cAddr);
    }

    if(cmd.foundOption("read-check-bram"))
    {
        int cAddr = std::atoi(cCheckBRAMAddr.c_str());
        LOG(INFO) << BOLDBLUE << "Read single check FCMD BRAM address: " << cmd.optionValue("read-check-bram") << RESET;
        cSEHTester.ReadCheckAddrBRAM(cAddr);
    }

    if(cmd.foundOption("clear-ref-bram"))
    {
        LOG(INFO) << BOLDBLUE << "Flushing ref BRAM!" << RESET;
        cSEHTester.ClearBRAM(std::string("ref"));
    }

    if(cmd.foundOption("clear-check-bram"))
    {
        LOG(INFO) << BOLDBLUE << "Flushing check BRAM!" << RESET;
        cSEHTester.ClearBRAM(std::string("test"));
    }
    if(cmd.foundOption("cic-pattern"))
    {
        LOG(INFO) << BOLDBLUE << "Checking back-end alignment with CIC.." << RESET;
        // align back-end
        BackEndAlignment cBackEndAligner;
        cBackEndAligner.Inherit(&cTool);
        cBackEndAligner.Align();
        // cBackEndAligner.Start(0);
        // reset all chip and board registers
        // to what they were before this tool was called
        // cBackEndAligner.Reset();
    }
    /*
        D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface());
        LOG(INFO) << BOLDBLUE << "Stub lines " << RESET;
        cFWInterface->StubDebug(true, 6);
        LOG(INFO) << BOLDBLUE << "L1 data " << RESET;
        cFWInterface->L1ADebug();
    */
    // Save Result File
    cSEHTester.TurnOff();
    cSEHTester.LpGBTInjectULExternalPattern(false, 170);

    cTool.SaveResults();
    cTool.WriteRootFile();
    cTool.CloseResultFile();
    // Destroy Tools
    cTool.Destroy();
    // cTool.Destroy();

    if(!batchMode) cApp.Run();
    return 0;
}
