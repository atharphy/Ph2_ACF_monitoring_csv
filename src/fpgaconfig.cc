#include <cstdlib>
#include <inttypes.h>
#include <string>
#include <vector>

#include "../System/SystemController.h"
#include "../Utils/argvparser.h"
#include "FC7FpgaConfig.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

using namespace CommandLineProcessing;

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

void verifyImageName(const std::string& strImage, const std::vector<std::string>& lstNames)
{
    if(lstNames.empty())
    {
        if(strImage.compare("1") != 0 && strImage.compare("2") != 0)
        {
            LOG(ERROR) << "Error, invalid image name, should be 1 (golden) or 2 (user)";
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }
    }
}

int main(int argc, char* argv[])
{
    auto* baseDirChar_p = std::getenv("PH2ACF_BASE_DIR");
    if(baseDirChar_p == nullptr)
    {
        LOG(ERROR) << "Error, the environment variable PH2ACF_BASE_DIR is not initialized (hint: source setup.sh)";
        exit(EXIT_FAILURE);
    }

    el::Configurations conf(std::string(baseDirChar_p) + "/settings/logger.conf");
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

    cmd.defineOption("image", "Without -f: load image from SD card to FPGA\nWith    -f: name of image written to SD card\n-f specifies the source filename", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("image", "i");

    cmd.defineOption("board", "In case of multiple boards in the same file, specify board Id", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("board", "b");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(EXIT_FAILURE);
    }

    std::string        cHWFile = (cmd.foundOption("config")) ? cmd.optionValue("config") : "settings/HWDescription_2CBC.xml";
    std::ostringstream cStr;
    cSystemController.setInterfaceInitialization(0);
    cSystemController.InitializeHw(cHWFile, cStr);
    BeBoard* pBoard = cSystemController.fDetectorContainer->at((cmd.foundOption("board")) ? convertAnyInt(cmd.optionValue("board").c_str()) : 0);
    cSystemController.fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = FC7FpgaConfig(cSystemController.fBeBoardInterface->getFirmwareInterface());

    std::vector<std::string> lstNames = cInterface.getFpgaConfigList();
    std::string              cFWFile;
    std::string              strImage("1");

    if(cmd.foundOption("list"))
    {
        LOG(INFO) << lstNames.size() << " firmware images on SD card:";

        for(auto& name: lstNames) LOG(INFO) << " - " << name;

        exit(EXIT_SUCCESS);
    }
    else if(cmd.foundOption("file"))
    {
        cFWFile = cmd.optionValue("file");

        if(lstNames.size() == 0 && cFWFile.find(".mcs") == std::string::npos)
        {
            LOG(ERROR) << "Error, the specified file is not a .mcs file";
            exit(EXIT_FAILURE);
        }
        else if(lstNames.size() > 0 && cFWFile.compare(cFWFile.length() - 4, 4, ".bit") && cFWFile.compare(cFWFile.length() - 4, 4, ".bin"))
        {
            LOG(ERROR) << "Error, the specified file is neither a .bit nor a .bin file";
            exit(EXIT_FAILURE);
        }
    }
    else if(cmd.foundOption("delete") && !lstNames.empty())
    {
        strImage = cmd.optionValue("delete");
        verifyImageName(strImage, lstNames);
        cInterface.deleteFpgaConfig(strImage);
        LOG(INFO) << "Firmware image: " << strImage << " deleted from SD card";
        exit(EXIT_SUCCESS);
    }
    else if(!cmd.foundOption("image"))
    {
        cFWFile = "";
        LOG(ERROR) << "Error, no FW image specified";
        exit(EXIT_FAILURE);
    }

    if(cmd.foundOption("image"))
    {
        strImage = cmd.optionValue("image");

        if(!cmd.foundOption("file"))
        {
            verifyImageName(strImage, lstNames);
            LOG(INFO) << BOLDBLUE << ">>> Done <<<" << RESET;
        }
    }
    else if(!lstNames.empty())
        strImage = "GoldenImage.bin";

    if(!cmd.foundOption("file") && !cmd.foundOption("download"))
    {
        cInterface.jumpToFpgaConfig(strImage);
        exit(EXIT_SUCCESS);
    }

    bool cDone = 0;

    if(cmd.foundOption("download"))
        cInterface.downloadFpgaConfig(strImage, cmd.optionValue("download"));
    else
        cInterface.flashProm(strImage, cFWFile.c_str());

    uint32_t progress;

    while(cDone == 0)
    {
        progress = cInterface.getProgressValue();

        if(progress == 100)
        {
            cDone = 1;
            LOG(INFO) << BOLDBLUE << ">>> 100% Done <<<" << RESET;
        }
        else
        {
            LOG(INFO) << progress << "%  " << cInterface.getProgressString() << "                 \r" << std::flush;
            sleep(1);
        }
    }
}
