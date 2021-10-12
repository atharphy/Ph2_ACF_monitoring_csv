#include "boost/tokenizer.hpp"
#include <boost/filesystem.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/spirit/include/qi_numeric.hpp>
#include <boost/spirit/include/qi_parse.hpp>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Hybrid.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/AntennaTester.h"
#include "../tools/HybridTester.h"
#include "../tools/PedeNoise.h"
#include "../tools/PedestalEqualization.h"
#include "../tools/RegisterTester.h"
#include "../tools/ShortFinder.h"
#include "TApplication.h"
#include "TROOT.h"

#ifdef __USBINST__
#include "../../Ph2_USBInstDriver/HMP4040/HMP4040Client.h"
#include "../../Ph2_USBInstDriver/HMP4040/HMP4040Controller.h"
#include "../../Ph2_USBInstDriver/Utils/zmqutils.h"
#include "../Utils/AppLock.cc"
#include <zmq.hpp>
using namespace Ph2_UsbInst;
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

// need this to reset terminal output
const std::string rst("\033[0m");

// Typedefs for Containers
typedef std::map<std::string, double>       HMP4040_currents;
typedef std::map<double, std::string>       HMP4040_voltages;
typedef std::pair<time_t, HMP4040_currents> HMP4040_measurement;

// generic tokenize string function  - blatantly copied from G.Auzinger
std::vector<std::string> tokenize_input(std::string& cInput, const char* cSeperator)
{
    std::vector<std::string> cOutput;

    boost::char_separator<char>                   sep(cSeperator);
    boost::tokenizer<boost::char_separator<char>> tokens(cInput, sep);

    for(const auto& t: tokens) cOutput.push_back(t);

    return cOutput;
}
// function to return home directory
std::string getHomeDirectory()
{
    char                     buffer[256];
    std::string              currentDir  = getcwd(buffer, sizeof(buffer));
    std::vector<std::string> directories = tokenize_input(currentDir, "/");
    std::string              homeDir     = "/" + directories[0] + "/" + directories[1];
    return homeDir;
}
// function to create bash script which launches the HMP4040 server in a tmux session called HMP4040_Server
void create_HMP4040server_tmuxSession(std::string pHostname = "localhost", int pZmqPortNumber = 8081, int pHttpPortNumber = 8080, int pMeasureInterval_s = 2)
{
    char        buffer[256];
    std::string currentDir    = getcwd(buffer, sizeof(buffer));
    std::string baseDirectory = getHomeDirectory() + "/Ph2_USBInstDriver";

    // create bash script to launch HMP4040 sessions
    sprintf(buffer, "%s/start_HMP4040.sh", baseDirectory.c_str());
    std::ofstream starterScript(buffer);
    starterScript << "#!/bin/bash" << std::endl;

    // check if the tmux session with the name HMP4040_Server already exists... if it doesn't create one with that name.
    starterScript << "SESSION_NAME=HMP4040_Server" << std::endl << std::endl;
    starterScript << "tmux list-session 2>&1 | grep -q \"^$SESSION_NAME\" || tmux new-session -s $SESSION_NAME -d" << std::endl;
    // send chdir via tmux session
    sprintf(buffer, "tmux send-keys -t $SESSION_NAME \"cd %s\" Enter", baseDirectory.c_str());
    starterScript << buffer << std::endl << std::endl;
    // set-up environment for Ph2_USB_InstDriver
    starterScript << "tmux send-keys -t $SESSION_NAME \". ./setup.sh\" Enter" << std::endl;
    // launch HMP4040 server
    sprintf(buffer, "tmux send-keys -t $SESSION_NAME \"lvSupervisor -r %d -p %d -i %d\" Enter", pZmqPortNumber, pHttpPortNumber, pMeasureInterval_s);
    starterScript << buffer << std::endl;
    starterScript.close();
}

// function to check if a string is alphanumeric ...
// shamelessly copied from a google search
bool is_numeric(std::string const& str)
{
    std::string::const_iterator first(str.begin()), last(str.end());
    return boost::spirit::qi::parse(first, last, boost::spirit::double_) && first == last;
}

// check if the HMP4040 server has been launched
// if not launch it in a tmux session using a bask script created by create_HMP4040server_tmuxSession
//
int launch_HMP4040server(std::string pHostname, int& pZmqPortNumber, int& pHttpPortNumber, int pMeasureInterval_s)
{
#ifdef __USBINST__
    LOG(INFO) << "Check if HMP4040 server is not launched and if so launch it.";
    char        buffer[256];
    std::string currentDir    = getcwd(buffer, sizeof(buffer));
    std::string baseDirectory = getHomeDirectory() + "/Ph2_USBInstDriver";

    // first check if process if running
    // before I do anything else, try to find an existing lock and if so, terminate
    AppLock* cLock = new AppLock("/tmp/lvSupervisor.lock");

    // server already running
    if(cLock->getLockDescriptor() < 0)
    {
        // might as well retreive the info before quitting!
        std::string cInfo = cLock->get_info();
        LOG(INFO) << "Retreived the following parameters from the info file: " << cInfo;
        LOG(INFO) << "HMP4040 server already running .... so do nothing!";

        if(cLock) delete cLock;

        // tokenize the cInfo string to recover the port numbers so the client can be smart enough to connect to the
        // correct port!
        std::vector<std::string> cTokens = tokenize_input(cInfo, " ");
        std::vector<int>         cPorts;
        cPorts.clear();

        for(auto token: cTokens)
        {
            if(is_numeric(token)) cPorts.push_back(boost::lexical_cast<int>(token));
        }

        // ports passed as reference so they can be passed on in main()
        // THttp port comes first, then ZMQ
        pHttpPortNumber = cPorts[0];
        pZmqPortNumber  = cPorts[1];
    }
    else
    {
        // have to do this here because actually lvSupervisor attempts to access the LOCK file as well...
        if(cLock) delete cLock;

        LOG(INFO) << "HMP4040 server not running .... so try and launch it.";
        // launch the server in the background with nohup... probably not the smartest way of doing this but the only
        // way I know how without using screen/tmux
        // // sprintf(cmd, "nohup bin/lvSupervisor -r %d -p %d -i %d  0< /dev/null", pZmqPortNumber, pHttpPortNumber,
        // cMeasureInterval_s); nohup has a problem that i cannot seem to make it ignore std_in ... which seems to cause
        // the server to crash/time-out ... so do this with tmux instead
        create_HMP4040server_tmuxSession(pHostname, pZmqPortNumber, pHttpPortNumber, pMeasureInterval_s);
        char cmd[120];
        sprintf(cmd, ". %s/start_HMP4040.sh", baseDirectory.c_str());
        system(cmd);

        // start monitoring the voltages and currents on the HMP4040
        HMP4040Client* cClient = new HMP4040Client(pHostname, pZmqPortNumber);
        cClient->StartMonitoring();
        std::this_thread::sleep_for(std::chrono::seconds(pMeasureInterval_s * 2));

        if(cClient) delete cClient;
    }

#endif
    return 0;
}
// check that the currents drawn on the low voltage lines of the hybrid are within the "normal" range
HMP4040_measurement get_HMP4040currents(std::string pHostname = "localhost", int pZmqPortNumber = 8081, int pHttpPortNumber = 8080)
{
    HMP4040_measurement cMeasurement;
    HMP4040_currents    cCurrents;
    HMP4040_voltages    cVoltages = {{5.0, "pLVDS"}, {5.0, "nLVDS"}, {3.3, "VREG"}, {1.2, "CBC"}};

#ifdef __USBINST__
    HMP4040Client* cClient = new HMP4040Client(pHostname, pZmqPortNumber);

    int iterations = 0;
    // get the latest reading from the logged using the HMP4040 monitoring function.
    cClient->GetLatestReadValues();
    MeasurementValues cValues = cClient->fValues;
    cMeasurement.first        = cValues.fTimestamp;

    for(int i = 0; i < 4; i += 1)
    {
        auto search = cVoltages.find(cValues.fVoltages.at(i));

        if(search != cVoltages.end()) cCurrents.insert(std::pair<std::string, double>(search->second, cValues.fCurrents.at(i) * 1e3));
    }

    cMeasurement.second = cCurrents;

    if(cClient) delete cClient;

#endif
    return cMeasurement;
}
bool check_CurrentConsumption(Tool pTool, int pNCBCs = 2, std::string pHostname = "localhost", int pZmqPortNumber = 8081, int pHttpPortNumber = 8080, int pMeasureInterval_s = 2)
{
    // nominal currents for the 4 different low voltage lines on the hybrid : all in mA
    // nominal current drawn by one CBC
    double                   ncCBC          = 60;
    HMP4040_currents         cCurrentLimits = {{"pLVDS", 14.0}, {"nLVDS", 14.0}, {"VREG", 160.0}, {"CBC", ncCBC * pNCBCs}};
    std::vector<std::string> cChannelNames  = {"pLVDS", "nLVDS", "VREG", "CBC"};

#ifdef __USBINST__
    int         cNumReads = 3;
    std::string message;
    int         iterations = 0;
    // get the latest reading from the logged using the HMP4040 monitoring function.
    HMP4040_measurement cMeasurement           = get_HMP4040currents(pHostname, pZmqPortNumber, pHttpPortNumber);
    time_t              cTimeStamp             = cMeasurement.first;
    HMP4040_currents    cCurrentsMeasured      = cMeasurement.second;
    int                 cNumTimes_limitReached = 0;

    do
    {
        message           = "";
        bool limitReached = false;

        for(auto channelName: cChannelNames)
        {
            // auto cCurrentMeasurement = cCurrentLimits.at("pLVDS");
            auto srch_cLimits = cCurrentLimits.find(channelName);

            if(srch_cLimits != cCurrentLimits.end())
            {
                auto srch_cMeasurements = cCurrentsMeasured.find(channelName);

                if(srch_cMeasurements != cCurrentsMeasured.end())
                {
                    double deviationFromNominalValue = std::fabs(srch_cLimits->second - srch_cMeasurements->second) / srch_cLimits->second;
                    limitReached                     = (deviationFromNominalValue > 0.33) ? true : false;
                    char buffer[120];
                    sprintf(buffer, "# Current measured on %s = %.3f mA.\n", (srch_cMeasurements->first).c_str(), (double)(srch_cMeasurements->second));
                    message += buffer;
                    // LOG (INFO) << srch_cLimits->first << " : " << srch_cLimits->second ;
                    // LOG (INFO) << srch_cMeasurements->first << " : " << srch_cMeasurements->second;
                }
            }
        }

        // wait for 5s before checking for a new value
        std::this_thread::sleep_for(std::chrono::seconds(pMeasureInterval_s * 2));
        // check for a new value
        cMeasurement      = get_HMP4040currents(pHostname, pZmqPortNumber, pHttpPortNumber);
        cCurrentsMeasured = cMeasurement.second;

        if(cTimeStamp < cMeasurement.first) iterations++;

        cTimeStamp = cMeasurement.first;

        if(limitReached)
        {
            cNumTimes_limitReached++;
            LOG(INFO) << BOLDRED << "Nominal current consumption limits exceeded!!" << rst;
        }
    } while(iterations < cNumReads);

    message += "#";
    pTool.AmmendReport(message);
    return (cNumTimes_limitReached >= 2) ? false : true;
#else
    return true;
#endif
}
// check that the CBC registers can be written
// tool tries to write 2 bit patterns (0xAA , 0x55) to the CBCs and checks how many write operations have failed
bool check_Registers(Tool* pTool)
{
    bool cRegTest = false;
#ifdef __USE_ROOT__
    // first check that the registers could be read/written to correctly
    RegisterTester cRegisterTester;
    cRegisterTester.Inherit(pTool);
    LOG(INFO) << "Running registers testing tool ... ";
    cRegisterTester.TestRegisters();

    cRegisterTester.PrintTestReport();
    // once we've finished checking the registers reload the default values into the CBCs
    cRegisterTester.ReconfigureRegisters();
    // this was here to check that the reconfiguration worked...
    // cRegisterTester.dumpConfigFiles();
    // and now get the results (pass/fail) of the register test
    cRegTest = cRegisterTester.PassedTest();

    std::string line = cRegTest ? ("# Register test passed.") : ("# Register test failed : " + std::to_string(cRegisterTester.GetNumFails()) + " registers could not be written to.");
    cRegisterTester.AmmendReport(line);
#endif

    return cRegTest;
}
// perform the CBC Vplus, Voffset calibration
void perform_PedestalEqualization(Tool* pTool)
{
    PedestalEqualization cPedestalEqualization;
    cPedestalEqualization.Inherit(pTool);
    cPedestalEqualization.Initialise(false);

    cPedestalEqualization.FindVplus();
    cPedestalEqualization.FindOffsets();
    cPedestalEqualization.writeObjects();
    cPedestalEqualization.dumpConfigFiles();
}
// find the shorts on the DUT
bool check_Shorts(Tool* pTool, uint32_t cMaxNumShorts)
{
    // legacy
    // uint32_t cNShorts;
    // #ifdef __USE_ROOT__
    //     ShortFinder cShortFinder;
    //     cShortFinder.Inherit (pTool);
    //     cShortFinder.ConfigureHw();
    //     //reload the calibration values for the CBCs
    //     //cShortFinder.ReconfigureRegisters();
    //     // I don't think this is neccesary ... but here for now
    //     //cShortFinder.ConfigureVcth (0x78);

    //     cShortFinder.Initialise();
    //     cShortFinder.FindShorts();
    //     //cShortFinder.writeObjects();
    //     cShortFinder.SaveResults();
    //     cNShorts = cShortFinder.GetNShorts() ;
    //     char line[120];
    //     sprintf (line, "# %d shorts found on hybrid", cNShorts);
    //     cShortFinder.AmmendReport (line);
    //     cShortFinder.AmmendReport ( ( cNShorts <= cMaxNumShorts) ? ("# Shorts test passed.") : ("# Shorts test
    //     failed.") );

    //     LOG (INFO) << GREEN << "\t\t" + std::to_string (cNShorts) + " shorts found on hybrid." << rst ;
    // #endif

    // return ( cNShorts <= cMaxNumShorts) ? true : false;
    return false;
}
// measure the occupancy on the TOP/BOTTOM pads of the DUT
void perform_OccupancyMeasurment(Tool* pTool)
{
    LOG(INFO) << "Starting noise occupancy test.";

#ifdef __USE_ROOT__
    HybridTester cHybridTester;
    cHybridTester.Inherit(pTool);
    cHybridTester.ConfigureHw();
    cHybridTester.Initialize();

    // re-configure CBC regsiters with values from the calibration
    // cHybridTester.ReconfigureCBCRegisters();
    // I don't think this is neccesary ... but here for now
    // cHybridTester.ConfigureVcth (0x78);

    // measure occupancy
    cHybridTester.Measure();
    // display noisy/dead channels
    cHybridTester.DisplayNoisyChannels();
    cHybridTester.DisplayDeadChannels();

    // save results
    cHybridTester.writeObjects();

    char line[120];
    sprintf(line, "# Top Pad Occupancy = %.2f ± %.3f", cHybridTester.GetMeanOccupancyTop(), cHybridTester.GetRMSOccupancyTop());
    cHybridTester.AmmendReport(line);
    sprintf(line, "# Bottom Pad Occupancy = %.2f ± %.3f", cHybridTester.GetMeanOccupancyBottom(), cHybridTester.GetRMSOccupancyBottom());
    cHybridTester.AmmendReport(line);
#endif

    // measure pedestal
}
void perform_AntennaOccupancyMeasurement(Tool* pTool)
{
    LOG(INFO) << "Starting occupancy measurement using the antenna.";

#ifdef __USE_ROOT__
    AntennaTester cAntennaTester;
    cAntennaTester.Inherit(pTool);
    cAntennaTester.ConfigureHw();
    cAntennaTester.Initialize();

    // re-configure CBC regsiters with values from the calibration
    // cAntennaTester.ReconfigureCBCRegisters();
    cAntennaTester.ConfigureVcth(0x78);

    // measure occupancy
    cAntennaTester.Measure(10);

    // save results
    cAntennaTester.writeObjects();
    // char line[120];
    // sprintf(line, "# Top Pad Occupancy = %.2f ± %.3f" , cHybridTester.GetMeanOccupancyTop() ,
    // cHybridTester.GetRMSOccupancyTop() ); cHybridTester.AmmendReport(line); sprintf(line, "# Bottom Pad Occupancy =
    // %.2f ± %.3f" , cHybridTester.GetMeanOccupancyBottom() , cHybridTester.GetRMSOccupancyBottom() );
    // cHybridTester.AmmendReport(line);
#endif
}

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Integrated validation test performs the following actions:");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("numCBCs", "Number of CBCs. Default is 2", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("numCBCs", "n");

    cmd.defineOption("file", "Hw Description File . Default value: settings/Calibration2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("batch", "Run the application in batch mode", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("batch", "b");

    cmd.defineOption("hport", "Port number on which to run THttpServer for HMP4040 logger., Default: 8080", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("hport", "p");

    cmd.defineOption("cport", "Port number on which to run ZMQ server for HMP4040 client., Default: 8081", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("cport", "r");

    cmd.defineOption("settingsHMP4040", "Hw Description File for HMP4040. Default value: ../Ph2_USBInstDriver/settings/HMP4040.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("settingsHMP4040", "s");

    cmd.defineOption("logHMP4040", "Save the data to a text file. Default value: LV_log.txt", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("logHMP4040", "l");

    cmd.defineOption("interval", "Read Interval for the Power supply monitoring in seconds, Default: 2s", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("interval", "i");

    cmd.defineOption("checkCurrents", "Check the consumption of the DUT.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("checkRegisters", "Check WRITE/READ to and from registers on DUT.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("calibrate", "Calibration CBCs' Vplus/Voffset on DUT.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("checkShorts", "Identify shorts on DUT.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("measureOccupancy", "Measure (NOISE) occuapncy on DUT.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("antennaTest", "Measure occuapncy on the DUT using the Antenna.", ArgvParser::NoOptionAttribute);

    cmd.defineOption("all", "Perform ALL tests on DUT : checkCurrents , checkRegisters, calibrate, checkShorts, and measureOccupancy. ", ArgvParser::NoOptionAttribute);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    int         cNumCBCs = (cmd.foundOption("numCBCs")) ? atoi(cmd.optionValue("numCBCs").c_str()) : 2;
    std::string cHWFile  = (cmd.foundOption("file")) ? cmd.optionValue("file") : ("settings/Calibration" + std::to_string(cNumCBCs) + "CBC.xml");

    std::string cDirectory = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/IntegratedTester";

    bool batchMode = (cmd.foundOption("batch")) ? true : false;

    std::string cHostname              = (cmd.foundOption("hostname")) ? cmd.optionValue("hostname") : "localhost";
    int         httpPortNumber         = (cmd.foundOption("hport")) ? atoi(cmd.optionValue("hport").c_str()) : 8080;
    int         zmqPortNumber          = (cmd.foundOption("cport")) ? atoi(cmd.optionValue("cport").c_str()) : 8081;
    std::string cPowerSupplyHWFile     = (cmd.foundOption("settingsHMP4040")) ? cmd.optionValue("settingsHMP4040") : "../Ph2_USBInstDriver/settings/HMP4040.xml";
    std::string cPowerSupplyOutputFile = (cmd.foundOption("logHMP4040")) ? cmd.optionValue("logHMP4040") : "LV_log.txt";
    int         cInterval              = (cmd.foundOption("interval")) ? atoi(cmd.optionValue("interval").c_str()) : 2;
    // cHWFile =  "settings/Calibration" + std::to_string(cNumCBCs) + "CBC.xml";

    bool cCurrents           = (cmd.foundOption("checkCurrents")) ? true : false;
    bool cRegisters          = (cmd.foundOption("checkRegisters")) ? true : false;
    bool cCalibrate          = (cmd.foundOption("calibrate")) ? true : false;
    bool cShorts             = (cmd.foundOption("checkShorts")) ? true : false;
    bool cOccupancy          = (cmd.foundOption("measureOccupancy")) ? true : false;
    bool cAntennaMeasurement = (cmd.foundOption("antennaTest")) ? true : false;
    bool cAll                = (cmd.foundOption("all")) ? true : false;

    uint32_t cMaxNumShorts = 10;

    TApplication cApp("Root Application", &argc, argv);

    if(batchMode)
        gROOT->SetBatch(true);
    else
        TQObject::Connect("TCanvas", "Closed()", "TApplication", &cApp, "Terminate()");

    // set all test flags ON if all flag set in arguments passed to tester.
    if(cAll)
    {
        cCurrents  = true;
        cRegisters = true;
        cCalibrate = true;
        cShorts    = true;
        cOccupancy = true;
    }

    // Start server to communicate with HMP404 instrument via usbtmc and SCPI
    pid_t childPid; // the child process that the execution will soon run inside of.
    childPid = fork();

    if(childPid < 0) // fork failed
    {
        // log the error
        exit(1);
    }
    else if(childPid == 0) // fork succeeded
    {
        if(cCurrents)
        {
            // launch HMP4040 server
            launch_HMP4040server(cHostname, zmqPortNumber, httpPortNumber, cInterval);
        }
        exit(0);
    }
    else // Main (parent) process after fork succeeds
    {
        int returnStatus = -1;
        waitpid(childPid, &returnStatus, 0); // Parent process waits here for child to terminate.

        if(returnStatus == 0) // Verify child process terminated without error.
        {
            if(cCurrents)
            {
#if __ZMQ__
                LOG(INFO) << "HMP4040 server launcher terminated normally ... so can now start integrated tester.";
#endif
            }

            Timer tGlobal;
            tGlobal.start();
            Timer t;

            // create a genereic Tool Object, I can then construct all other tools from that using the Inherit() method
            // this tool stays on the stack and lives until main finishes - all other tools will update the HWStructure
            // from cTool
            std::stringstream outp;
            Tool              cTool;
            cTool.InitializeHw(cHWFile, outp);
            cTool.InitializeSettings(cHWFile, outp);
            LOG(INFO) << outp.str();
            cTool.CreateResultDirectory(cDirectory);
            cTool.InitResultFile("Summary");
            cTool.StartHttpServer();
            cTool.ConfigureHw();
            cTool.CreateReport();

            char line[120];

            // perform current consumption test if --checkCurrents flag set in arguments passed to tester
            if(cCurrents)
            {
                t.start();
                bool currentConsumptionTest_passed = check_CurrentConsumption(cTool, cNumCBCs, cHostname, zmqPortNumber, httpPortNumber, cInterval);
                cTool.AmmendReport((currentConsumptionTest_passed) ? ("# Current consumption test passed.") : ("# Current consumption test failed."));
                t.stop();
                sprintf(line, "# %.3f s required to check current consumption on low voltage power supply.", t.getElapsedTime());
                cTool.AmmendReport(line);
                t.show("Current consumption test of the DUT");

                // if DUT failscurrent consumption test then stop here
                if(!currentConsumptionTest_passed)
                {
                    LOG(INFO) << BOLDRED << "Hybrid did not pass current consumption test. Stopping tester.";
                    LOG(INFO) << BOLDBLUE << "Stopping DUT tester!" << rst;
                    // add cTool destroy here!
                    // have to destroy the tool at the end of the program
                    cTool.SaveResults();
                    cTool.CloseResultFile();
                    cTool.Destroy();
                    exit(0);
                }
            }

            // perform register R&W test if --checkRegisters flag set in arguments passed to tester
            if(cRegisters)
            {
                t.start();
                bool registerReadWriteTest_passed = check_Registers(&cTool);
                t.stop();
                sprintf(line, "# %.3f s required to check register WRITE operation.", t.getElapsedTime());
                cTool.AmmendReport(line);
                t.show("Read/Write register test of the DUT");

                // if DUT fails register R&W test then stop here.
                if(!registerReadWriteTest_passed)
                {
                    LOG(INFO) << BOLDRED << "Hybrid did not pass register check. Stopping tester." << rst;
                    LOG(INFO) << BOLDBLUE << "Stopping DUT tester!" << rst;
                    // have to destroy the tool at the end of the program
                    cTool.SaveResults();
                    cTool.CloseResultFile();
                    cTool.Destroy();
                    exit(0);
                }
            }

            // perform calibration of the CBCs on the DUT if --calibrate flag set in  arguments passed to tester
            if(cCalibrate)
            {
                LOG(INFO) << GREEN << "Hybrid passed register check. Moving on to calibration of the CBCs on the DUT." << rst;
                t.start();
                perform_PedestalEqualization(&cTool);
                LOG(INFO) << "Calibration finished.";
                t.stop();
                sprintf(line, "# %.3f s required to calibrate Vplus,Voffset on CBCs.", t.getElapsedTime());
                cTool.AmmendReport(line);

                t.show("Calibration of the DUT");
            }

            // look for shorts on the DUT if --checkShorts flag set in arguments passed to tester
            if(cShorts)
            {
                // if calibrate flag has not been set then make sure that the calibration is done before the occupancy
                // measurement is performed!
                if(!cCalibrate)
                {
                    LOG(INFO) << GREEN << "Calibrating CBCs before starting antenna test of the CBCs on the DUT." << rst;
                    t.start();
                    perform_PedestalEqualization(&cTool);
                    LOG(INFO) << "Calibration finished.";
                    t.stop();
                    sprintf(line, "# %.3f s required to calibrate Vplus,Voffset on CBCs.", t.getElapsedTime());
                    cTool.AmmendReport(line);
                    t.show("Calibration of the DUT");
                }

                LOG(INFO) << "Starting short(s) test.";
                t.start();
                bool shortFinder_passed = check_Shorts(&cTool, cMaxNumShorts);
                t.stop();
                sprintf(line, "# %.3f s required to identify shorts on DUT.", t.getElapsedTime());
                cTool.AmmendReport(line);

                t.show("Short finding on the DUT");

                if(!shortFinder_passed)
                {
                    LOG(INFO) << BOLDRED << "Hybrid did not pass shorts check. Stopping tester.";
                    LOG(INFO) << BOLDBLUE << "Stopping DUT tester!" << rst;

                    cTool.SaveResults();
                    cTool.CloseResultFile();
                    cTool.Destroy();
                    exit(0);
                }
            }

            //  measure (noise) occupancy on dut if --measureOccupancy flag set in arguments passed to tester
            if(cOccupancy)
            {
                // if calibrate flag has not been set then make sure that the calibration is done before the occupancy
                // measurement is performed!
                if(!cCalibrate)
                {
                    LOG(INFO) << GREEN << "Calibrating CBCs before starting antenna test of the CBCs on the DUT." << rst;
                    t.start();
                    perform_PedestalEqualization(&cTool);
                    LOG(INFO) << "Calibration finished.";
                    t.stop();
                    sprintf(line, "# %.3f s required to calibrate Vplus,Voffset on CBCs.", t.getElapsedTime());
                    cTool.AmmendReport(line);
                    t.show("Calibration of the DUT");
                }

                t.start();
                perform_OccupancyMeasurment(&cTool);
                t.stop();
                sprintf(line, "# %.3f s required to measure (NOISE) occupancy on DUT.", t.getElapsedTime());
                cTool.AmmendReport(line);
                t.show("(Noise) Occupancy measurement");
            }

            // measure occupancy on dut whilst using the antenna to capacitively couple charge into the CBCs
            // --antennaTest flag must be set in the arguments sent to the tester
            if(cAntennaMeasurement)
            {
                // if calibrate flag has not been set then make sure that the calibration is done before the occupancy
                // measurement is performed!
                if(!cCalibrate)
                {
                    LOG(INFO) << GREEN << "Calibrating CBCs before starting antenna test of the CBCs on the DUT." << rst;
                    t.start();
                    perform_PedestalEqualization(&cTool);
                    LOG(INFO) << "Calibration finished.";
                    t.stop();
                    sprintf(line, "# %.3f s required to calibrate Vplus,Voffset on CBCs.", t.getElapsedTime());
                    cTool.AmmendReport(line);
                    t.show("Calibration of the DUT");
                }

                t.start();
                perform_AntennaOccupancyMeasurement(&cTool);
                t.stop();
                sprintf(line, "# %.3f s required to measure occupancy on the DUT with the Antenna.", t.getElapsedTime());
                cTool.AmmendReport(line);
                t.show("(Antenna) Occupancy measurement");
            }

            tGlobal.stop();
            tGlobal.show("Complete system test");
            sprintf(line, "# %.3f s required to test DUT.", tGlobal.getElapsedTime());
            cTool.AmmendReport(line);

            // have to destroy the tool at the end of the program
            // cTool.SaveResults(); // if this is here then you end up with 2 copies of all histograms and canvases in
            // the root file ... so I've removed it
            cTool.CloseResultFile();
            cTool.Destroy();
        }
        else if(returnStatus == 1)
        {
            LOG(INFO) << "The child process terminated with an error!.";
            exit(1);
        }
    }

    if(!batchMode) cApp.Run();

    return 0;
}
