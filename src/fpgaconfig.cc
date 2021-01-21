#include <cstring>
#include <inttypes.h>

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../System/SystemController.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;
using namespace std;

INITIALIZE_EASYLOGGINGPP

class AcqVisitor : public HwInterfaceVisitor
{
    int cN;

  public:
    AcqVisitor() { cN = 0; }
    virtual void visit(const Ph2_HwInterface::Event& pEvent)
    {
        cN++;
        LOG(INFO) << ">>> Event #" << cN;
        LOG(INFO) << pEvent;
    }
};

void verifyImageName(const string& strImage, const vector<string>& lstNames)
{
    if(lstNames.empty())
    {
        if(strImage.compare("1") != 0 && strImage.compare("2") != 0)
        {
            LOG(ERROR) << "Error, invalid image name, should be 1 (golden) or 2 (user)";
            exit(1);
        }
    }
    else
    {
        bool bFound = false;

        for(size_t iName = 0; iName < lstNames.size(); iName++)
        {
            if(!strImage.compare(lstNames[iName]))
            {
                bFound = true;
                break;
            }
        }

        if(!bFound)
        {
            LOG(ERROR) << "Error, this image name: " << strImage << " is not available on SD card";
            exit(1);
        }
    }
}

int main(int argc, char* argv[])
{
    std::string loggerConfigFile = std::getenv("BASE_DIR");
    loggerConfigFile += "/settings/logger.conf";
    el::Configurations conf(loggerConfigFile);
    el::Loggers::reconfigureAllLoggers(conf);

    SystemController cSystemController;
    ArgvParser       cmd;

    cmd.setIntroductoryDescription("CMS Ph2_ACF  Data acquisition test and Data dump");

    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("list", "Print the list of available firmware images on SD card (works only with CTA boards)");
    cmd.defineOptionAlternative("list", "l");

    cmd.defineOption("delete", "Delete a firmware image on SD card (works only with CTA boards)", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("delete", "d");

    cmd.defineOption("file", "Local FPGA Bitstream file (*.mcs format for GLIB or *.bit/*.bin format for CTA boards)", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("download", "Download an FPGA configuration from SD card to file (only for CTA boards)", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("download", "o");

    cmd.defineOption("config", "Hw Description File . Default value: settings/HWDescription_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("config", "c");

    cmd.defineOption("image",
                     "Without -f: load image from SD card to FPGA\nWith -f: name of image written to SD card (-f "
                     "specified the source filename)",
                     ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("image", "i");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    std::string        cHWFile = (cmd.foundOption("config")) ? cmd.optionValue("config") : "settings/HWDescription_2CBC.xml";
    std::ostringstream cStr;
    cSystemController.InitializeHw(cHWFile, cStr);
    BeBoard*       pBoard   = cSystemController.fDetectorContainer->at(0);
    vector<string> lstNames = cSystemController.fBeBoardInterface->getFpgaConfigList(pBoard);
    std::string    cFWFile;
    string         strImage("1");

    if(cmd.foundOption("list"))
    {
        LOG(INFO) << lstNames.size() << " firmware images on SD card:";

        for(auto& name: lstNames) LOG(INFO) << " - " << name;

        exit(0);
    }
    else if(cmd.foundOption("file"))
    {
        cFWFile = cmd.optionValue("file");

        if(lstNames.size() == 0 && cFWFile.find(".mcs") == std::string::npos)
        {
            LOG(ERROR) << "Error, the specified file is not a .mcs file";
            exit(1);
        }
        else if(lstNames.size() > 0 && cFWFile.compare(cFWFile.length() - 4, 4, ".bit") && cFWFile.compare(cFWFile.length() - 4, 4, ".bin"))
        {
            LOG(ERROR) << "Error, the specified file is neither a .bit nor a .bin file";
            exit(1);
        }
    }
    else if(cmd.foundOption("delete") && !lstNames.empty())
    {
        strImage = cmd.optionValue("delete");
        verifyImageName(strImage, lstNames);
        cSystemController.fBeBoardInterface->DeleteFpgaConfig(pBoard, strImage);
        LOG(INFO) << "Firmware image: " << strImage << " deleted from SD card";
        exit(0);
    }
    else if(!cmd.foundOption("image"))
    {
        cFWFile = "";
        LOG(ERROR) << "Error, no FW image specified";
        exit(1);
    }

    if(cmd.foundOption("image"))
    {
        strImage = cmd.optionValue("image");

        if(!cmd.foundOption("file"))
        {
            verifyImageName(strImage, lstNames);
            LOG(INFO) << ">>> Done <<<";
        }
    }
    else if(!lstNames.empty())
        strImage = "GoldenImage.bin";

    if(!cmd.foundOption("file") && !cmd.foundOption("download"))
    {
        cSystemController.fBeBoardInterface->JumpToFpgaConfig(pBoard, strImage);
        exit(0);
    }

    bool cDone = 0;

    if(cmd.foundOption("download"))
        cSystemController.fBeBoardInterface->DownloadFpgaConfig(pBoard, strImage, cmd.optionValue("download"));
    else
        cSystemController.fBeBoardInterface->FlashProm(pBoard, strImage, cFWFile.c_str());

    uint32_t progress;

    while(cDone == 0)
    {
        progress = cSystemController.fBeBoardInterface->GetConfiguringFpga(pBoard)->getProgressValue();

        if(progress == 100)
        {
            cDone = 1;
            LOG(INFO) << "\n 100% Done";
        }
        else
        {
            LOG(INFO) << progress << "%  " << cSystemController.fBeBoardInterface->GetConfiguringFpga(pBoard)->getProgressString() << "                 \r" << flush;
            sleep(1);
        }
    }
}
