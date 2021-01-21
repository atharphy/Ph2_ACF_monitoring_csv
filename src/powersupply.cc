#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"

#ifdef __POWERSUPPLY__
#include "PowerSupply.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char** argv)
{
#ifdef __POWERSUPPLY__
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  system test application");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("name", "Name of the power supply as described in the HW file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("name", "n");

    cmd.defineOption("voltage", "Voltage to be set", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("voltage", "v");

    cmd.defineOption("i_max", "Maximum current setting", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("i_max", "i_max");

    cmd.defineOption("v_max", "Maximum voltage setting", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("v_max", "v_max");

    cmd.defineOption("off", "Turn off power supply");
    cmd.defineOptionAlternative("off", "o");

    // cmd.defineOption ( "GUI", "Named pipe for GUI communication", ArgvParser::OptionRequiresValue /*|
    // ArgvParser::OptionRequired*/ ); cmd.defineOptionAlternative ( "GUI", "g" );

    cmd.defineOption("file", "Hw Description File . Default value: settings/D19CDescription_Cic2.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // now query the parsing results
    std::string cHWFile      = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19CDescription_Cic2.xml";
    std::string cPowerSupply = (cmd.foundOption("name")) ? cmd.optionValue("name") : "";
    double      cVoltsLimit  = (cmd.foundOption("v_max")) ? std::stod(cmd.optionValue("v_max").c_str()) : 10.5;
    double      cAmpsLimit   = (cmd.foundOption("i_max")) ? std::stod(cmd.optionValue("i_max").c_str()) : 1.3;
    double      cVolts       = (cmd.foundOption("v")) ? std::stod(cmd.optionValue("v").c_str()) : 0;
    bool        cTurnOff     = cmd.foundOption("o");

    std::string docPath = cHWFile;
    LOG(INFO) << "Init PS with " << docPath;
    pugi::xml_document docSettings;

    PowerSupply::PS_settings ps_settings = PowerSupply::readSettings(docPath, docSettings);
    PowerSupply::PS_map      ps_map      = PowerSupply::Initialize(ps_settings);

    if(ps_map.size() == 0) { std::cout << "No configurable power supply has been found" << std::endl; }
    else
    {
        std::cout << "Number of power supplies: " << ps_map.size() << std::endl;
        std::cout << "ps_map content:" << std::endl;
        for(auto it = ps_map.begin(); it != ps_map.end(); ++it) { std::cout << it->first << std::endl; }
    }

    if(ps_map[cPowerSupply]->isOpen())
    {
        if(cTurnOff)
        {
            LOG(INFO) << "Turn off " << cPowerSupply;
            ps_map[cPowerSupply]->turnOff();
        }
        else
        {
            if(cmd.foundOption("v_max")) { ps_map[cPowerSupply]->setVoltsLimit(cVoltsLimit); }
            if(cmd.foundOption("i_max")) { ps_map[cPowerSupply]->setAmpsLimit(cAmpsLimit); }
            if(cmd.foundOption("v"))
            {
                ps_map[cPowerSupply]->setVolts(cVolts);
                ps_map[cPowerSupply]->turnOn();
            }
        }
        sleep(1);
        LOG(INFO) << BOLDWHITE << cPowerSupply << " status:" RESET;
        LOG(INFO) << "\tV(set):\t\t" << BOLDWHITE << ps_map[cPowerSupply]->getVolts() << RESET;
        LOG(INFO) << "\tV(meas):\t" << BOLDWHITE << ps_map[cPowerSupply]->measureVolts() << RESET;
        LOG(INFO) << "\tI_max(set):\t" << BOLDWHITE << ps_map[cPowerSupply]->getAmps() << RESET;
        LOG(INFO) << "\tI(meas):\t" << BOLDWHITE << ps_map[cPowerSupply]->measureAmps() << RESET;
    }
    else
    {
        LOG(INFO) << cPowerSupply << " not found!";
    }

#endif

    return 0;
}
