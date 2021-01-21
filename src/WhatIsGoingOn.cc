#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/FrontEndDescription.h"
#include "../HWDescription/OuterTrackerModule.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/CalibrationExample.h"
#include "../tools/Tool.h"
#include "TApplication.h"
#include "TCanvas.h"
#include "TH1.h"
#include "TH2.h"
#include "TLine.h"
#include "TROOT.h"
#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <iostream>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    std::string       cHWFile = "settings/D19C_2xSSA_Calib.xml";
    std::stringstream outp;
    LOG(INFO) << BOLDRED << "== 1 =============" << RESET;
    Tool cTool;
    LOG(INFO) << BOLDRED << "== 2 =============" << RESET;
    cTool.InitializeHw(cHWFile, outp);
    LOG(INFO) << BOLDRED << "== 3 =============" << RESET;
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << BOLDRED << "== 4 =============" << RESET;
    cTool.ConfigureHw();
    LOG(INFO) << BOLDRED << "== 5 =============" << RESET;
}
