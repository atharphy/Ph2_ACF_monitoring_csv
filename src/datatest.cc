#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Module.h"
#include "../HWInterface/BeBoardInterface.h"
#include "../HWInterface/ChipInterface.h"
#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/SLinkEvent.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"

#include <cstring>
#include <fstream>
#include <inttypes.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;

using namespace std;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf("settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    uint32_t pEventsperVcth;
    int      cVcth;

    SystemController cSystemController;
    ArgvParser       cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF  Data acquisition test and Data dump");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("ignoreI2c", "Ignore I2C configuration of CBCs. Allows to run acquisition on a bare board without CBC.");
    cmd.defineOptionAlternative("ignoreI2c", "i");

    cmd.defineOption("file", "Hw Description File . Default value: settings/HWDescription_2CBC.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption(
        "vcth", "Threshold in VCth units (hex (including 0x) or decimal) . Default values from HW description .XML file", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("vcth", "v");

    cmd.defineOption("events", "Number of Events . Default value: 10", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("events", "e");

    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("save", "s");

    cmd.defineOption("daq", "Save the data into a .daq file using the phase-2 Tracker data format.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("daq", "d");

    cmd.defineOption("read", "Read the data from a raw file instead of the board.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("read", "r");

    // cmd.defineOption( "option", "Define file access mode: w : write , a : append, w+ : write/update",
    // ArgvParser::OptionRequiresValue ); cmd.defineOptionAlternative( "option", "o" );

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    bool        cSaveToFile   = false;
    bool        cReadFromFile = false;
    std::string cOutputFile;
    std::string cInputFile;

    // file handler for reading
    uint32_t fPlaybackEventSize32 = 0;

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/HWDescription_2CBC.xml";

    if(cmd.foundOption("save")) cSaveToFile = true;

    if(cmd.foundOption("read")) cReadFromFile = true;

    if(cSaveToFile)
    {
        cOutputFile = cmd.optionValue("save");
        cSystemController.addFileHandler(cOutputFile, 'w');
        LOG(INFO) << "Writing Binary Rawdata to:   " << cOutputFile;
    }
    else if(cReadFromFile)
    {
        cInputFile = cmd.optionValue("read");
        cSystemController.addFileHandler(cInputFile, 'r');
        LOG(INFO) << "Reading Binary Rawdata file from:   " << cInputFile;
    }

    std::string  cDAQFileName;
    FileHandler* cDAQFileHandler = nullptr;
    bool         cDAQFile        = cmd.foundOption("daq");

    if(cDAQFile)
    {
        cDAQFileName    = cmd.optionValue("daq");
        cDAQFileHandler = new FileHandler(cDAQFileName, 'w');
        LOG(INFO) << "Writing DAQ File to:   " << cDAQFileName << " - ConditionData, if present, parsed from " << cHWFile;
    }

    cVcth          = (cmd.foundOption("vcth")) ? convertAnyInt(cmd.optionValue("vcth").c_str()) : 0;
    pEventsperVcth = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 10;

    Timer t;
    t.start();

    std::stringstream outp;
    cSystemController.InitializeHw(cHWFile, outp);
    LOG(INFO) << outp.str();
    outp.str("");

    if(!cmd.foundOption("read")) cSystemController.ConfigureHw(cmd.foundOption("ignoreI2c"));

    t.stop();
    t.show("Time to Initialize/configure the system: ");

    if(cVcth != 0)
    {
        t.start();
        ThresholdVisitor cVisitor(cSystemController.fReadoutChipInterface, 0);
        cVisitor.setThreshold(cVcth);
        cSystemController.accept(cVisitor);

        t.stop();
        t.show("Time for changing VCth on all CBCs:");
    }

    BeBoard* pBoard = static_cast<BeBoard*>(cSystemController.fDetectorContainer->at(0));
    t.start();
    // make event counter start at 1 as does the L1A counter
    uint32_t cN      = 1;
    uint32_t cNthAcq = 0;

    Counter cCbcCounter;
    pBoard->accept(cCbcCounter);

    if(!cmd.foundOption("read"))
        cSystemController.fBeBoardInterface->Start(pBoard);
    else
    {
        FileHeader cHeader;
        cSystemController.fFileHandler->getHeader(cHeader);
        pBoard->setBoardType(cHeader.getBoardType());
        pBoard->setEventType(cHeader.fEventType);
        fPlaybackEventSize32 = cHeader.fEventSize;

        if(cHeader.fNchip != cCbcCounter.getNChip())
        {
            LOG(ERROR) << "Error, wrong number of CBCs in config file w.r.t. File Header; config file: " << +cCbcCounter.getNChip() << " - header: " << cHeader.fNchip << " - aborting!";
            exit(1);
        }
    }

    const std::vector<Event*>* pEvents;
    int                        itCounter = 0;

    while(cN <= pEventsperVcth)
    {
        if(cmd.foundOption("read"))
        {
            std::vector<uint32_t> cReadVec;
            cSystemController.readFile(cReadVec, 10 * fPlaybackEventSize32);
            size_t cCalcEvents = cReadVec.size() / fPlaybackEventSize32;
            cSystemController.DecodeData(pBoard, cReadVec, cCalcEvents, pBoard->getBoardType());
            pEvents = &cSystemController.GetEvents(pBoard);
        }
        else
        {
            uint32_t cPacketSize = cSystemController.ReadData(pBoard);

            if(cN + cPacketSize > pEventsperVcth) cSystemController.fBeBoardInterface->Stop(pBoard);

            pEvents = &cSystemController.GetEvents(pBoard);
        }

        for(auto& ev: *pEvents)
        {
            if(cN < 10)
            {
                LOG(INFO) << ">>> Event #" << cN;
                LOG(INFO) << *ev;
            }
            cN++;
            if(cDAQFile)
            {
                SLinkEvent cSLev = ev->GetSLinkEvent(pBoard);
                auto       data  = cSLev.getData<uint32_t>();
                cDAQFileHandler->setData(data);
                cSLev.print(std::cout);
            }
        }

        cNthAcq++;
        cN += pEvents->size();
        itCounter++;
    }

    t.stop();
    t.show("Time to take data:");

    if(cDAQFile) delete cDAQFileHandler;

    cSystemController.Destroy();
}
