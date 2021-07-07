#include <fstream>
#include <cstring>
#include <ios>

#include "../System/SystemController.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/Timer.h"
#include "../Utils/Utilities.h"
#include "../Utils/argvparser.h"
#include "../tools/Tool.h"
#include "TApplication.h"
#include "TROOT.h"
#ifdef __ANTENNA__
#include "Antenna.h"
#endif

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;
using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

int main(int argc, char** argv)
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);
    ArgvParser cmd;
    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF d19c Testboard Firmware Test Application");
    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");
    // options
    cmd.setHelpOption("h", "help", "Print this help page");
    cmd.defineOption("file", "Hw Description File . Default value: settings/D19CHWDescription.xml", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");
    cmd.defineOption("testpulse", "Check test pulse for different groups", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("testpulse", "t");
    cmd.defineOption("rate", "Measure maximal readout rate", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("rate", "r");
    cmd.defineOption("ipb_rate", "Measure maximal IPBus readout rate", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("ipb_rate", "i");
    cmd.defineOption("occupancy", "Measure 2S Occupancy", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("occupancy", "m");
    cmd.defineOption("output", "Output Directory . Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");
    cmd.defineOption("configure", "Configure HW", ArgvParser::NoOptionAttribute);
    cmd.defineOptionAlternative("configure", "c");
    cmd.defineOption("save", "Save the data to a raw file.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("save", "s");
    cmd.defineOption("events", "Number of Events . Default value: 10000", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("events", "e");
    cmd.defineOption("pkgsize", "Avg package size (for IPBus readout speed test)", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("pkgsize", "p");
    cmd.defineOption("evtsize", "Avg event size (for IPBus readout speed test)", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("evtsize", "w");
    cmd.defineOption("dqm", "Print every i-th event.  ", ArgvParser::OptionRequiresValue);
    cmd.defineOptionAlternative("dqm", "d");
    cmd.defineOption("mask", "Hybrid mask - default all enabled", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOption("hard_reset", "Hard Reset the board", ArgvParser::NoOptionAttribute);
    cmd.defineOption("ddr3test", "Test the on-board ddr3 chip", ArgvParser::NoOptionAttribute);

    int result = cmd.parse(argc, argv);

    if(result != ArgvParser::NoParserError)
    {
        LOG(INFO) << cmd.parseErrorDescription(result);
        exit(1);
    }

    bool cHardReset    = (cmd.foundOption("hard_reset")) ? true : false;
    bool cDDR3SelfTest = (cmd.foundOption("ddr3test")) ? true : false;

    bool        cSaveToFile = false;
    std::string cOutputFile;

    if(cmd.foundOption("save")) cSaveToFile = true;

    // now query the parsing results
    std::string cHWFile = (cmd.foundOption("file")) ? cmd.optionValue("file") : "settings/D19CHWDescription.xml";

    std::stringstream outp;
    Tool              cTool;
    cTool.InitializeHw(cHWFile, outp);
    cTool.InitializeSettings(cHWFile, outp);
    LOG(INFO) << outp.str();
    if(cSaveToFile)
    {
        cOutputFile = cmd.optionValue("save");
        cTool.InitResultFile(cOutputFile);
    }
    if(!cHardReset) cTool.ConfigureHw();

// now try and look at the temperature and current on the hybrid
#ifdef __ANTENNA__
    Antenna cAntenna;
    cAntenna.initializeAntenna();
    uint16_t cIter = 0;
    do
    {
        float cTemp    = cAntenna.GetHybridTemperature(4);
        float cCurrent = cAntenna.GetHybridCurrent(4);
        LOG(INFO) << BOLDBLUE << "Temperature on the hybrid is " << cTemp << " degrees ..... current on the hybrid is " << cCurrent << " mA." << RESET;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        cIter++;
    } while(cIter < 100);
#endif

    BeBoard* pBoard = static_cast<BeBoard*>(cTool.fDetectorContainer->at(0));
    cTool.fBeBoardInterface->getBoardInfo(pBoard);

    bool cTestPulse = (cmd.foundOption("testpulse")) ? true : false;
    bool cRate      = (cmd.foundOption("rate")) ? true : false;
    bool cIPB_Rate  = (cmd.foundOption("ipb_rate")) ? true : false;
    bool cOccupancy = (cmd.foundOption("occupancy")) ? true : false;

    if(cHardReset) { cTool.fBeBoardInterface->RebootBoard(pBoard); }
    else if(cDDR3SelfTest)
    {
        cTool.fBeBoardInterface->setBoard(pBoard->getId());
        dynamic_cast<D19cFWInterface*>(cTool.fBeBoardInterface->getFirmwareInterface())->DDR3SelfTest();
        //(D19cFWInterface*)(cTool.fBeBoardInterface->fBoardFW)->DDR3SelfTest();
    }
    else
    {
        if(cTestPulse)
        {
            auto    cSetting            = cTool.fSettingsMap.find("TestPulsePotentiometer");
            uint8_t cTestPulseAmplitude = (cSetting != std::end(cTool.fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0x7F;

            uint32_t cNGroups = 8;
            uint32_t cN       = 0;
            cTool.setFWTestPulse();

            for(uint8_t i = 0; i < cNGroups; i++)
            {
                cTool.setSystemTestPulse(cTestPulseAmplitude, i, true, false);
                cTool.ReadNEvents(pBoard, 1);

                const std::vector<Event*>& events = cTool.GetEvents();
                for(auto& ev: events)
                {
                    LOG(INFO) << ">>> Event #" << cN++;
                    outp.str("");
                    outp << *ev;
                    LOG(INFO) << outp.str();
                }
            }
        }

        if(cRate)
        {
            uint32_t cN            = 0;
            uint32_t count         = 0;
            double   cAvgOccupancy = 0;

            uint32_t cNEventsToCollect = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 10000;

            // be careful works only for one hybrid
            HybridContainer* cCbcVector = pBoard->at(0)->at(0);
            /*for( auto cCbc : cCbcVector )
            {
                static_cast<CbcInterface*>(cTool.fReadoutChipInterface)->enableHipSuppression( cCbc, false, true,0);
                std::vector<uint8_t> cSeeds_ph1{ 85, 170};
                std::vector<int>     cBends_ph1( 2, static_cast<int>(1*2) );
                static_cast<CbcInterface*>(cTool.fReadoutChipInterface)->injectStubs( cCbc , cSeeds_ph1 , cBends_ph1);
            }*/
            uint32_t cNCbc = cCbcVector->size();

            Timer t;
            t.start();

            cTool.SystemController::StartBoard(pBoard);
            while(cN < cNEventsToCollect)
            {
                cTool.ReadData(pBoard);
                const std::vector<Event*>& events = cTool.GetEvents();
                for(auto& ev: events)
                {
                    count++;
                    cN++;

                    double cAvgOccupancyHyb0 = 0;
                    for(auto cCbc: *cCbcVector) cAvgOccupancyHyb0 += ev->GetNHits(0, cCbc->getId());
                    cAvgOccupancy += (cAvgOccupancyHyb0 / cNCbc);

                    if(cmd.foundOption("dqm"))
                    {
                        if(count % atoi(cmd.optionValue("dqm").c_str()) == 0)
                        {
                            LOG(INFO) << ">>> Event #" << count;
                            outp.str("");
                            outp << *ev << std::endl;
                            LOG(INFO) << outp.str();
                        }
                    }

                    if(count % 10000 == 0) LOG(INFO) << ">>> Recorded Event #" << count;
                }
            }
            cTool.StopBoard(pBoard);

            t.stop();
            LOG(INFO) << "Average Occupancy for Hybrid#0: " << (double)cAvgOccupancy / cN << " hits/(event*CBC)";
            LOG(INFO) << "Measured maximal readout rate: " << (double)(cN / t.getElapsedTime()) / 1000 << "kHz (based on " << +cN << " events)";
        }

        if(cIPB_Rate)
        {
            uint32_t cN = 0;

            uint32_t cNEventsToCollect = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 10000;
            uint32_t cPackageSize      = (cmd.foundOption("pkgsize")) ? convertAnyInt(cmd.optionValue("pkgsize").c_str()) : 100;
            uint32_t cEvtSize          = (cmd.foundOption("evtsize")) ? convertAnyInt(cmd.optionValue("evtsize").c_str()) : 94;

            Timer t;
            t.start();

            while(cN < cNEventsToCollect)
            {
                cTool.fBeBoardInterface->ReadBlockBoardReg(pBoard, "fc7_daq_ctrl.readout_block.readout_fifo", cPackageSize * cEvtSize);
                cN += cPackageSize;
            }
            cTool.StopBoard(pBoard);

            t.stop();
            LOG(INFO) << "Measured maximal IPBus readout rate: " << (double)(cN / t.getElapsedTime()) / 1000 << "kHz (based on " << +cN << " events, avg package size: " << +cPackageSize
                      << " events, avg event size: " << +cEvtSize << " words)";
        }

        // measures the 2s occupancy
        if(cOccupancy)
        {
            // init
            LOG(INFO) << "Initating occupancy meauserement";
            uint32_t cNEventsToCollect = (cmd.foundOption("events")) ? convertAnyInt(cmd.optionValue("events").c_str()) : 200;

            // get fw interface
            D19cFWInterface* d19cfw = (D19cFWInterface*)cTool.fBeBoardInterface->getFirmwareInterface();

            // init threshold visitior
            ThresholdVisitor cThresholdVisitor(cTool.fReadoutChipInterface, 0);
            cTool.accept(cThresholdVisitor);
            auto cFe0 = pBoard->at(0)->at(0);

            // hybrid mask
            uint32_t cHybridMask = (cmd.foundOption("mask")) ? convertAnyInt(cmd.optionValue("mask").c_str()) : 0xFFFFFFFF;
            ;
            d19cfw->WriteReg("fc7_daq_cnfg.calibration_2s_block.enable_hybrids", cHybridMask);

            //
            uint32_t cThresholdMin = 400;
            uint32_t cThresholdMax = 800;

            // create counters
            uint8_t*** cChannelCounters = nullptr;
            uint8_t**  cErrorCounters   = nullptr;
            // allocate memory
            d19cfw->Manage2SCountersMemory(cErrorCounters, cChannelCounters, true);

            // start time counting
            Timer t;
            t.start();

            bool doScan = true;
            if(!doScan)
            {
                // measure
                d19cfw->Measure2SOccupancy(cNEventsToCollect, cErrorCounters, cChannelCounters);

                // debug test
                // for(uint8_t ch = 0; ch < NCHANNELS; ch++) std::cout << "Ch: " << +ch << ", Counter: " <<
                // +cChannelCounters[0][0][ch] << std::endl;
            }
            else
            {
                bool useCounters = true;

                LOG(INFO) << "Mode: " << (useCounters ? "2S Counters" : "Conventional");

                // do threshokd scan
                for(uint32_t cThreshold = cThresholdMin; cThreshold < cThresholdMax; cThreshold++)
                {
                    // set threshold
                    for(auto cCbc: *cFe0)
                    {
                        cThresholdVisitor.setThreshold(cThreshold);
                        static_cast<ReadoutChip*>(cCbc)->accept(cThresholdVisitor);
                    }

                    // measure (equvuvalient tasks)
                    if(useCounters) { d19cfw->Measure2SOccupancy(cNEventsToCollect, cErrorCounters, cChannelCounters); }
                    else
                    {
                        cTool.ReadNEvents(pBoard, cNEventsToCollect);
                        const std::vector<Event*>& events = cTool.GetEvents();
                        for(auto& ev: events)
                        {
                            for(auto cOpticalGroup: *pBoard)
                            {
                                for(auto cFe: *cOpticalGroup)
                                {
                                    for(auto cCbc: *cFe)
                                    {
                                        for(uint8_t ch = 0; ch < NCHANNELS; ch++)
                                        {
                                            if(ev->DataBit(cFe->getId(), cCbc->getId(), ch)) cChannelCounters[cFe->getId()][cCbc->getId()][ch]++;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // debug output
                    std::cout << "th" << cThreshold << ":\t";
                    for(uint8_t ch = 0; ch < 16; ch++) std::cout << +cChannelCounters[0][0][ch] << "\t";
                    std::cout << std::endl;

                    // reset the counters
                    for(auto cOpticalGroup: *pBoard)
                    {
                        for(auto cFe: *cOpticalGroup)
                        {
                            for(auto cCbc: *cFe)
                            {
                                for(uint8_t ch = 0; ch < NCHANNELS; ch++) { cChannelCounters[cFe->getId()][cCbc->getId()][ch] = 0; }
                            }
                        }
                    }
                }

                t.stop();

                // print
                LOG(INFO) << "Time spent for SCurves: " << 1000 * t.getElapsedTime() / (cThresholdMax - cThresholdMin) << " mililiseconds per point (" << cThresholdMax - cThresholdMin << " points)";
            }

            // release memory
            d19cfw->Manage2SCountersMemory(cErrorCounters, cChannelCounters, false);
        }
        // if nothing else
        /*       uint32_t cNEventsToCollect = ( cmd.foundOption ( "events" ) ) ? convertAnyInt ( cmd.optionValue (
           "events" ).c_str() ) : 10000; cTool.ReadNEvents( pBoard, cNEventsToCollect ); const std::vector<Event*>&
           events = cTool.GetEvents(); int cNevents=0; for( auto& cEvent : events )
               {
                   LOG (INFO) << ">>> Event #" << cNevents ;
                   LOG (INFO) << *cEvent;
                   cNevents++;
               }*/
    }

    LOG(INFO) << "*** End of the DAQ test ***";
    //   cTool.SaveResults();
    //   cTool.CloseResultFile();
    cTool.Destroy();

    return 0;
}
