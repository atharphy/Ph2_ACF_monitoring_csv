#include <cstdlib>
#include <inttypes.h>
#include <string>
#include <vector>

#define __NAMEDPIPE__
#ifdef __NAMEDPIPE__
#include "gui_logger.h"
#endif

#include "D19cFpgaConfig.h"
#include "System/SystemController.h"
#include "Utils/argvparser.h"

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
    auto* baseDirChar_p = std::getenv("PH2ACF_BASE_DIR");
    if(baseDirChar_p == nullptr)
    {
        LOG(ERROR) << "Error, the environment variable PH2ACF_BASE_DIR is not initialized (hint: source setup.sh)";
        exit(1);
    }

    std::string loggerConfigFile = std::getenv("PH2ACF_BASE_DIR");
    loggerConfigFile += "/settings/logger.conf";
    el::Configurations conf(loggerConfigFile);

    el::Loggers::reconfigureAllLoggers(conf);

    el::Helpers::installLogDispatchCallback<gui::LogDispatcher>("GUILogDispatcher");
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

    cmd.defineOption("gui", "fpgaconfig is called from the gui, hand over named pipe", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("gui", "g");

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    // Check if there is a gui involved, if not dump information in a dummy pipe
    std::string guiPipe = (cmd.foundOption("gui")) ? cmd.optionValue("gui") : "/tmp/guiDummyPipe";
    gui::init(guiPipe.c_str());
    gui::progress(0);
    bool skipUpload = false; // Set true if the last while loop should be skipped to update FW information output

    std::string        cHWFile = (cmd.foundOption("config")) ? cmd.optionValue("config") : "settings/HWDescription_2CBC.xml";
    std::ostringstream cStr;
    cSystemController.InitializeHw(cHWFile, cStr);
    BeBoard* pBoard = cSystemController.fDetectorContainer->at((cmd.foundOption("board")) ? convertAnyInt(cmd.optionValue("board").c_str()) : 0);
    cSystemController.fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = D19cFpgaConfig(cSystemController.fBeBoardInterface->getFirmwareInterface());

    // std::vector<std::string> lstNames = cInterface->getFpgaConfigList(); // cSystemController.fBeBoardInterface->getFpgaConfigList(pBoard);

    // First get list of FW files
    std::vector<std::string> lstNames;
    uint32_t                 timeStamp = 0;
    uint32_t                 nHybrids  = 0;
    uint32_t                 nChips    = 0;
    uint32_t                 chipCode  = 0;
    // getFpgaConfigList will fail when Board is busy
    try
    {
        lstNames  = cInterface.getFpgaConfigList();
        timeStamp = cSystemController.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.general.firmware_timestamp");
        nHybrids  = cSystemController.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.general.info.num_hybrids");
        nChips    = cSystemController.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.general.info.num_chips");
        chipCode  = cSystemController.fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.general.info.chip_type");
    }
    catch(...)
    {
        LOG(ERROR) << "FC7 Board not responsive or no Ph2_ACF firmware loaded";
        exit(1);
    }

    std::string cFWFile;
    std::string strImage("1");

    // Provide information for the gui
    if(cmd.foundOption("gui"))
    {
        LOG(INFO) << "Provide information from current firmware and available firmwares on SD card to the GUI";
        gui::status("Provide FW Info");
        gui::progress(0.5);

        // Get Chip Type
        std::string chipType = "UNKNOWN";
        switch(chipCode)
        {
        case 0x0: chipType = "CBC2"; break;
        case 0x1: chipType = "CBC3"; break;
        case 0x2: chipType = "MPA"; break;
        case 0x3: chipType = "SSA"; break;
        case 0x4: chipType = "CIC"; break;
        case 0x5: chipType = "CIC2"; break;
        }
        // Process FW Date
        std::stringstream ss;
        ss << ((timeStamp >> 27) & 0x1F) << "." << ((timeStamp >> 23) & 0xF) << "." << ((timeStamp >> 17) & 0x3F);
        std::string fWStringTimeStamp = ss.str();

        // Build message pairs to be send to the pipe
        gui::data("timeStamp", fWStringTimeStamp.c_str());
        gui::data("nHybrids", std::to_string(nHybrids).c_str());
        gui::data("nChips", std::to_string(nChips).c_str());
        gui::data("chipType", chipType.c_str());

        for(auto& fwName: lstNames) { gui::data("Firmware", fwName.c_str()); }
        gui::progress(1);
        LOG(INFO) << "Firmware information loaded";
        gui::status("FW info provided");
        gui::status("Readout successful");
    }

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
        cInterface.deleteFpgaConfig(strImage);
        // cSystemController.fBeBoardInterface->DeleteFpgaConfig(pBoard, strImage);
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
            LOG(INFO) << BOLDBLUE << ">>> Done <<<" << RESET;
        }
    }
    else if(!lstNames.empty())
    {
        strImage = "GoldenImage.bin";
    }
    if(!cmd.foundOption("file") && !cmd.foundOption("download"))
    {
        std::string ss = "Switch FW: ";
        ss.append(strImage);
        LOG(INFO) << ss;
        gui::status(ss.c_str());
        gui::progress(0);
        cInterface.jumpToFpgaConfig(strImage);
        gui::progress(1);
        skipUpload = true;
        gui::data("NewFirmware", strImage.c_str());
    }

    bool cDone = 0;

    if(cmd.foundOption("download")) { cInterface.downloadFpgaConfig(strImage, cmd.optionValue("download")); }
    else if(!skipUpload)
    {
        if(std::find(std::begin(lstNames), std::end(lstNames), strImage) != std::end(lstNames))
        {
            LOG(ERROR) << "Abort Uploed FC7 Board already contains a firwmware named " << strImage;
            exit(1);
        }
        else
        {
            std::stringstream ss;
            ss << "Upload FW " << cFWFile.c_str();
            LOG(INFO) << ss.str();
            gui::status(ss.str().c_str());
            gui::progress(0);
            cInterface.flashProm(strImage, cFWFile.c_str());
        }
    }

    uint32_t progress;

    while(cDone == 0 && !skipUpload)
    {
        progress = cInterface.getProgressValue();
        // progress = cSystemController.fBeBoardInterface->GetConfiguringFpga(pBoard)->getProgressValue();

        if(progress == 100)
        {
            cDone = 1;
            LOG(INFO) << BOLDBLUE << ">>> 100% Done <<<" << RESET;
            gui::progress(1);
        }
        else
        {
            gui::progress(((float)((int)progress)) / 100);
            LOG(INFO) << progress << "%  " << cInterface.getProgressString() << "                 \r" << std::flush;
            sleep(1);
        }
    }
    // Provide information for the gui
    if(cmd.foundOption("gui"))
    {
        LOG(INFO) << "Provide information from current firmware and available firmwares on SD card to the GUI";
        gui::status("Provide FW Info");
        gui::progress(0.5);

        // Get Chip Type
        std::string chipType = "UNKNOWN";
        switch(chipCode)
        {
        case 0x0: chipType = "CBC2"; break;
        case 0x1: chipType = "CBC3"; break;
        case 0x2: chipType = "MPA"; break;
        case 0x3: chipType = "SSA"; break;
        case 0x4: chipType = "CIC"; break;
        case 0x5: chipType = "CIC2"; break;
        }
        // Process FW Date
        std::stringstream ss;
        ss << ((timeStamp >> 27) & 0x1F) << "." << ((timeStamp >> 23) & 0xF) << "." << ((timeStamp >> 17) & 0x3F);
        std::string fWStringTimeStamp = ss.str();

        // Build message pairs to be send to the pipe
        gui::data("timeStamp", fWStringTimeStamp.c_str());
        gui::data("nHybrids", std::to_string(nHybrids).c_str());
        gui::data("nChips", std::to_string(nChips).c_str());
        gui::data("chipType", chipType.c_str());

        for(auto& fwName: lstNames) { gui::data("Firmware", fwName.c_str()); }
        gui::progress(1);
        LOG(INFO) << "Firmware information loaded";
        gui::status("FW info provided");
        gui::status("Readout successful");
    }
}
