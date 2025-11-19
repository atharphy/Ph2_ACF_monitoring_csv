#include "DQMUtils/ParseEventFile.h"
#include "HWDescription/BeBoard.h"
#include "Parser/FileParser.h"
#include "Utils/Container.h"
#include "Utils/argvparser.h"

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_Parser;
using namespace CommandLineProcessing;

int main(int argc, char* argv[])
{
    if(std::getenv("PH2ACF_BASE_DIR") == nullptr)
    {
        std::cout << "You must source setup.sh or export the PH2ACF_BASE_DIR environmental variable. Exiting..." << std::endl;
        exit(EXIT_FAILURE);
    }
    std::string baseDir = std::string(std::getenv("PH2ACF_BASE_DIR")) + "/";
    std::string binDir  = baseDir + "bin/";

    // configure the logger
    el::Configurations conf(baseDir + "settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF main script");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("directory", "Result directory", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("directory", "d");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(EXIT_FAILURE);
    }

    std::string folderPath = cmd.optionValue("directory");

    std::regex  run_regex(R"(Run_(\d+))");
    std::regex  module_regex(R"(ModuleTest_(\d+))");
    std::smatch match;

    auto extract_number = [&](const std::string& input) -> int
    {
        if(std::regex_search(input, match, run_regex)) { return std::stoi(match[1]); }
        else if(std::regex_search(input, match, module_regex)) { return std::stoi(match[1]); }
        else { throw std::runtime_error("Unknown format: " + input); }
    };

    ParseEventFile theParseEventFile(folderPath, extract_number(folderPath));

    FileParser        theFileParser;
    std::stringstream out;
    DetectorContainer theDetectorStructure;
    theFileParser.parseHW(folderPath + "/Configuration.xml", &theDetectorStructure, out);

    theParseEventFile.parseEventFiles(&theDetectorStructure);
    return 0;
}