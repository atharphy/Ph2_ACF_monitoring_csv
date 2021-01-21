#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../HWInterface/ReadoutChipInterface.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../tools/SSALatencyScan.h"
#include "TApplication.h"
#include "TROOT.h"
#include "tools/BackEndAlignment.h"
#include <cstring>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  calibration routine using K. Uchida's algorithm or a fast algorithm");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Hw Description File", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19C_2xSSA_PreCalibSYNC.xml";

    TApplication cApp("Root Application", &argc, argv);

    Timer t;

    // create a genereic Tool Object, I can then construct all other tools from that using the Inherit() method
    // this tool stays on the stack and lives until main finishes - all other tools will update the HWStructure from
    // cTool
    Tool              cTool;
    std::stringstream outp;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");
    cTool.ConfigureHw();
    cTool.InitResultFile("SSALatencyScanResults");
    // cTool.StartHttpServer();

    // align back-end .. if this moves to firmware then we can get rid of this step
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(&cTool);
    cBackEndAligner.Initialise();
    bool cAligned = cBackEndAligner.Align();
    cBackEndAligner.resetPointers();
    if(!cAligned)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
        exit(0);
    }

    // cTool.ConfigureHw ();
    // if ( !cOld )
    //{
    t.start();

    // now create a SSAPedestalEqualization object
    SSALatencyScan cSSALatencyScan;
    cSSALatencyScan.Inherit(&cTool);
    // second parameter disables stub logic on CBC3
    // cPedestalEqualization.Initialise ( false, true );
    cSSALatencyScan.Initialise();

    cSSALatencyScan.run();
    // cTool.SaveResults();
    // cTool.WriteRootFile();
    // cTool.CloseResultFile();
    cTool.Destroy();

    return 0;
}
