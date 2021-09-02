/*!
  \file                  templateCMSITminiDAQ.cc
  \brief                 Template file to be used as example for a Mini DAQ to test RD53 readout chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "../DQMUtils/DQMInterface.h"
#include "../System/SystemController.h"
#include "../Utils/MiddlewareInterface.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/argvparser.h"

#include "../tools/RD53Physics.h"
#include "../tools/RD53PixelAlive.h"

#include <chrono>
#include <thread>

#include "TApplication.h"

// ##################
// # Default values #
// ##################
#define ARBITRARYDELAY 2 // [seconds]

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void readBinaryData(const std::string& binaryFile, SystemController& mySysCntr, std::vector<RD53Event>& decodedEvents)
{
    const unsigned int    wordDataSize = 32; // @CONST@
    unsigned int          errors       = 0;
    std::vector<uint32_t> data;

    RD53Event::ForkDecodingThreads();

    LOG(INFO) << BOLDMAGENTA << "@@@ Decoding binary data file @@@" << RESET;
    mySysCntr.addFileHandler(binaryFile, 'r');
    LOG(INFO) << BOLDBLUE << "\t--> Data are being readout from binary file" << RESET;
    mySysCntr.readFile(data, 0);

    uint16_t status;
    RD53Event::DecodeEventsMultiThreads(data, decodedEvents, status);
    LOG(INFO) << GREEN << "Total number of events in binary file: " << BOLDYELLOW << decodedEvents.size() << RESET;

    for(auto i = 0u; i < decodedEvents.size(); i++)
        if(RD53Event::EvtErrorHandler(decodedEvents[i].eventStatus) == false)
        {
            LOG(ERROR) << BOLDBLUE << "\t--> Corrupted event n. " << BOLDYELLOW << i << RESET;
            errors++;
            RD53Event::PrintEvents({decodedEvents[i]});
        }

    if(decodedEvents.size() != 0)
    {
        LOG(INFO) << GREEN << "Corrupted events: " << BOLDYELLOW << std::setprecision(3) << errors << " (" << 1. * errors / decodedEvents.size() * 100. << "%)" << std::setprecision(-1) << RESET;
        int avgEventSize = data.size() / decodedEvents.size();
        LOG(INFO) << GREEN << "Average event size is " << BOLDYELLOW << avgEventSize * wordDataSize << RESET << GREEN << " bits over " << BOLDYELLOW << decodedEvents.size() << RESET << GREEN
                  << " events" << RESET;
    }

    std::string fileName(binaryFile);
    RD53Event::MakeNtuple(fileName.replace(fileName.find(".raw"), 4, ".root"), decodedEvents);
    LOG(INFO) << GREEN << "Saving raw data into ROOT ntuple: " << BOLDYELLOW << fileName << RESET;

    mySysCntr.closeFileHandler();
}

int main(int argc, char** argv)
{
    const std::string configFile   = "CMSIT.xml";
    const std::string binaryFile   = "binaryFile.raw";
    const std::string fileName     = "OutputRootFileName";
    const int         runNumber    = 0;
    const bool        doReset      = false;
    const bool        doReadBinary = false;

    SystemController  mySysCntr;
    std::stringstream outp;
    mySysCntr.InitializeHw(configFile, outp, true, false);
    mySysCntr.InitializeSettings(configFile, outp);

    // ##################
    // # Reset hardware #
    // ##################
    if(doReset == true)
    {
        if(mySysCntr.fDetectorContainer->at(0)->at(0)->flpGBT == nullptr)
            static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getId()])->ResetSequence("160");
        else
            static_cast<RD53FWInterface*>(mySysCntr.fBeBoardFWMap[mySysCntr.fDetectorContainer->at(0)->getId()])->ResetSequence("320");
        return EXIT_SUCCESS;
    }

    if(doReadBinary == true)
    {
        // ######################################
        // # Read binary file and decode events #
        // ######################################
        readBinaryData(binaryFile, mySysCntr, RD53Event::decodedEvents);
        // RD53Event::PrintEvents(RD53Event::decodedEvents);
    }
    else
    {
        // #######################
        // # Initialize Hardware #
        // #######################
        mySysCntr.Configure(configFile);
    }

    // ##################
    // # Run PixelAlive #
    // ##################
    // PixelAlive pa;
    // pa.Inherit(&mySysCntr);
    // pa.localConfigure(fileName, runNumber);
    // pa.run();
    // pa.analyze();
    // pa.draw();

    // ###############
    // # Run Physics #
    // ###############
    Physics ph;
    ph.Inherit(&mySysCntr);
    if(doReadBinary == false)
    {
        ph.localConfigure(fileName, -1);
        ph.Start(runNumber);
        std::this_thread::sleep_for(std::chrono::seconds(ARBITRARYDELAY));
        ph.Stop();
    }
    else
    {
        ph.localConfigure(fileName, runNumber);
        ph.analyze(true);
        ph.draw();
    }

    // #############################
    // # Destroy System Controller #
    // #############################
    mySysCntr.Destroy();

    return EXIT_SUCCESS;
}
