#include <cstring>
#include "Utils/Utilities.h"
#include "Utils/argvparser.h"
#include "../tools/WorkerTester.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
  // configure the logger
  el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
  el::Loggers::reconfigureAllLoggers(conf);

  ArgvParser cmd;
  // init
  cmd.setIntroductoryDescription("CMS Ph2_ACF Tool for Command Processor Block Workers testing");
  // error codes
  cmd.addErrorCode(0, "Success");
  cmd.addErrorCode(1, "Error");
  // options
  cmd.setHelpOption("h", "help", "Print this help page");

  cmd.defineOption("file", "Hw Description File . Default value: settings/DESY_FullModule.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
  cmd.defineOptionAlternative("file", "f");

  cmd.defineOption("test-ic-read", "Test lpGBT IC Read");
  cmd.defineOptionAlternative("test-ic-read", "ticr");

  cmd.defineOption("test-ic-write", "Test lpGBT IC Write");
  cmd.defineOptionAlternative("test-ic-write", "ticw");

  cmd.defineOption("test-i2c-read", "Test lpGBT I2C Read");
  cmd.defineOptionAlternative("test-i2c-read", "ti2cr");

  cmd.defineOption("test-i2c-write", "Test lpGBT I2C Write");
  cmd.defineOptionAlternative("test-i2c-write", "ti2cw");

  int result = cmd.parse(argc, argv);

  if(result != ArgvParser::NoParserError)
  {
      LOG(INFO) << cmd.parseErrorDescription(result);
      exit(1);
  }

  std::stringstream outp;
  Tool              cTool;

  std::string cHWFile          = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/DESY_FullModule.xml";
  cTool.InitializeHw(cHWFile, outp);
  cTool.InitializeSettings(cHWFile, outp);
  LOG(INFO) << outp.str();
  // bool cIgnoreI2c    = false;
  // bool cReInitialize = true;
  // cTool.ConfigureHw(cIgnoreI2c, cReInitialize);

  std::string cDirectory       = (cmd.foundOption("output")) ? cmd.optionValue("output") : "Results/";
  cTool.CreateResultDirectory(cDirectory, false, false);

  std::string cResultfile = "Worker";
  cTool.InitResultFile(cResultfile);

  WorkerTester cWorkerTester;
  cWorkerTester.Inherit(&cTool);

  cWorkerTester.PrepareForTests();

  if(cmd.foundOption("test-ic-read")) cWorkerTester.TestICRead();
  if(cmd.foundOption("test-ic-write")) cWorkerTester.TestICWrite();
  if(cmd.foundOption("test-i2c-read")) cWorkerTester.TestI2CRead();
  if(cmd.foundOption("test-i2c-write")) cWorkerTester.TestI2CWrite();

  return 0;
}