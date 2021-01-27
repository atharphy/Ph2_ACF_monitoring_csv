#include <cstring>
#include <fstream>
#include <inttypes.h>
#include <iostream>
#include <map>
#include <sstream>
#include <stdint.h>
#include <string>

#include "../Utils/ConsoleColor.h"
#include "../Utils/SLinkEvent.h"
#include "../Utils/Timer.h"
#include "../Utils/argvparser.h"
#include "../Utils/easylogging++.h"

#include "../RootUtils/publisher.h"
#include "TROOT.h"
#include <boost/filesystem.hpp>

#include "../DQMUtils/DQMEvent.h"
#include "../DQMUtils/SLinkDQMHistogrammer.h"

using namespace CommandLineProcessing;
INITIALIZE_EASYLOGGINGPP

void dumpEvents(const std::vector<DQMEvent*>& elist, size_t evt_limit, std::ostream& os)
{
    for(size_t i = 0; i < std::min(elist.size(), evt_limit); ++i)
    {
        os << "==> Event index: " << i + 1 << std::endl;
        const DQMEvent* ev = elist.at(i);
        os << *ev << std::endl;
    }
}
void readSLinkFromFile(std::ifstream& fh, int maxevt, bool skipHeader, std::vector<DQMEvent*>& evList)
{
    if(skipHeader)
    {
        size_t nlskip = 0;
        while(!fh.eof())
        {
            char buffer[8];
            fh.read(buffer, sizeof(uint64_t));
            if(!fh) std::cerr << "Error reading data from the SLink file!" << std::endl;
            if(++nlskip == 6) break;
        }
#if 0
           std::cout << "After reading the SLink Header words tellg() returns " 
    		 << fh.tellg() 
    		 << std::endl;
#endif
    }

    // now read the relevant SLink data
    // read line-by-line
    while(1)
    {
        std::vector<uint64_t> cData;
        bool                  trailerFound = false;

        while(!fh.eof())
        {
            char buffer[8];
            fh.read(buffer, sizeof(uint64_t));

            if(!fh)
            {
                std::cerr << "Error reading data from the SLink file!" << std::endl;
                // continue;
            }

            uint64_t word;
            std::memcpy(&word, buffer, sizeof(uint64_t));
            uint64_t correctedWord = ((word & 0xFFFFFFFF) << 32) | ((word >> 32) & 0xFFFFFFFF);
            cData.push_back(correctedWord);
#if 1
            std::cout << setw(4) << cData.size() << "\t" << std::bitset<64>(correctedWord) << "\t0x" << std::hex << std::noshowbase << std::setw(16) << std::setfill('0') << correctedWord
                      << std::setfill(' ') << std::dec << "\t" << ((correctedWord & 0xFF00000000000000) >> 56 == 0xa0) << "\t" << ((correctedWord & 0x00000000000000F0) >> 4 == 0x7) << std::endl;
#endif
            // Now find the last word of the event
            if((correctedWord & 0xFF00000000000000) >> 56 == 0xA0 && (correctedWord & 0x00000000000000F0) >> 4 == 0x7) // SLink Trailer
            {
                trailerFound = true;
                break;
            }
        }

        // 3 is the minimum size for an empty SLinkEvent (2 words header and 1 word trailer)
        if(trailerFound && cData.size() > 3)
        {
            std::cout << "-> Build event w/ the above " << cData.size() << " words." << std::endl;
            try
            {
                DQMEvent* ev = new DQMEvent(new SLinkEvent(cData));
                if(ev->trkPayload().feReadoutMapping().size() > 0) evList.push_back(std::move(ev));
                if(static_cast<int>(evList.size()) == maxevt) return;
            }
            catch(...)
            {
                std::cerr << "-> Corrupted event! nwords: " << cData.size() << std::endl;
#if 0
                for (size_t i = 0; i < cData.size(); ++i)
		  std::cout << i << "\t" << std::bitset<64> (cData[i]) << std::endl;
#endif
            }
        }

        if(fh.eof()) break;
    }
}
int main(int argc, char* argv[])
{
    // configure the logger
    el::Configurations conf(std::string(std::getenv("PH2ACF_BASE_DIR")) + "/settings/logger.conf");
    el::Loggers::reconfigureAllLoggers(conf);

    ArgvParser cmd;

    // init
    cmd.setIntroductoryDescription("CMS Ph2_ACF SLinkEvent based miniDQM application");

    // error codes
    cmd.addErrorCode(0, "Success");
    cmd.addErrorCode(1, "Error");

    // options
    cmd.setHelpOption("h", "help", "Print this help page");

    cmd.defineOption("file", "Binary .daq Data File", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("file", "f");

    cmd.defineOption("output", "Output Directory for DQM plots & page. Default value: Results", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("output", "o");

    cmd.defineOption("dqm", "Build DQM webpage. Default = false", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("dqm", "d");

    cmd.defineOption("tree", "Create a ROOT tree also. Default = false", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);

    cmd.defineOption("dumpevt", "Specify number of events to be dumped in raw format", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("dumpevt", "p");

    cmd.defineOption("readevt", "Specify number of events to be read from file at a time", ArgvParser::OptionRequiresValue /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("readevt", "r");

    cmd.defineOption("skipDebugHist", "Switch off debug histograms. Default = false", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("skipDebugHist", "g");

    cmd.defineOption("skipHeader", "Skip SLink header (6 64-bit words). Default = false", ArgvParser::NoOptionAttribute /*| ArgvParser::OptionRequired*/);
    cmd.defineOptionAlternative("skipHeader", "s");

    int result = cmd.parse(argc, argv);
    if(result != ArgvParser::NoParserError)
    {
        std::cerr << cmd.parseErrorDescription(result) << std::endl;
        exit(1);
    }

    // now query the parsing results
    std::string daqFilename = (cmd.foundOption("file")) ? cmd.optionValue("file") : "";
    if(daqFilename.empty())
    {
        std::cerr << "Error, no binary SLink64 .daq format file provided. exiting" << std::endl;
        exit(2);
    }

    // Check if the file can be found
    if(!boost::filesystem::exists(daqFilename))
    {
        std::cerr << "Error!! binary SLink64 .daq file " << daqFilename << " not found, exiting!" << std::endl;
        exit(3);
    }

    bool cDQMPage   = (cmd.foundOption("dqm")) ? true : false;
    int  dumpevt    = (cmd.foundOption("dumpevt")) ? stoi(cmd.optionValue("dumpevt")) : 10;
    int  readevt    = (cmd.foundOption("readevt")) ? stoi(cmd.optionValue("readevt")) : 100000;
    bool addTree    = (cmd.foundOption("tree")) ? true : false;
    bool skipHist   = (cmd.foundOption("skipDebugHist")) ? true : false;
    bool skipHeader = (cmd.foundOption("skipHeader")) ? true : false;

    // open the binary file
    std::ifstream fh(daqFilename, std::fstream::in | std::fstream::binary);
    if(!fh)
    {
        std::cerr << "Error opening the SLink file: " << daqFilename << "!" << std::endl;
        exit(4);
    }

    // Create the SLinkDQMHistogrammer object
    std::unique_ptr<SLinkDQMHistogrammer> dqmH = std::unique_ptr<SLinkDQMHistogrammer>(new SLinkDQMHistogrammer(0, addTree, skipHist));

    // read the SLink64 .daq file and build DQMEvents
    std::vector<DQMEvent*> evList;
    readSLinkFromFile(fh, dumpevt, skipHeader, evList); // read the first few events
#if 0 
    std::cout << "After reading "
	      << dumpevt 
	      << " SLink events, tellg() returns " 
	      << fh.tellg() 
	      << std::endl;
#endif
    // either prepare and public DQM histograms or simply dump (a few) events in raw and human readable formats
    if(cDQMPage)
    {
        // book and fill histograms
        gROOT->SetBatch(true);
        dqmH->bookHistograms(evList[0]->trkPayload().feReadoutMapping());
        dqmH->fillHistograms(evList);

        // now read the whole file in chunks of maxevt
        long ntotevt = evList.size();
        while(1)
        {
            for(DQMEvent* p: evList) delete p;
            evList.clear();

            readSLinkFromFile(fh, readevt, false, evList);
#if 0
	  std::cout << "After reading " 
		    << readevt 
		    << " SLink events, tellg() returns " 
		    << fh.tellg() 
		    << std::endl;
#endif
            dqmH->fillHistograms(evList);

            ntotevt += evList.size();
            LOG(INFO) << "-> eventsRead = " << evList.size() << ", totalEventsRead = " << ntotevt;

            if(fh.eof()) break;
            if(!fh.is_open()) break;
        }
        // save and publish
        // Create the DQM plots and generate the root file
        // first of all, strip the folder name
        std::vector<std::string> tokens;
        tokenize(daqFilename, tokens, "/");
        std::string fname = tokens.back();

        // now form the output Root filename
        tokens.clear();
        tokenize(fname, tokens, ".");
        std::string runLabel         = tokens[0];
        std::string dqmFilename      = runLabel + "_dqm.root";
        std::string flatTreeFilename = runLabel + "_tree.root";
        dqmH->saveHistograms(dqmFilename, flatTreeFilename);

        // find the folder (i.e DQM page) where the histograms will be published
        std::string cDirBasePath;
        if(cmd.foundOption("output"))
        {
            cDirBasePath = cmd.optionValue("output");
            cDirBasePath += "/";
        }
        else
            cDirBasePath = "Results/";

        // now read back the Root file and publish the histograms on the DQM page
        RootWeb::makeDQMmonitor(dqmFilename, cDirBasePath, runLabel);
        LOG(INFO) << "Saving root file to " << dqmFilename << " and webpage to " << cDirBasePath;
    }
    else
    {
        // std::stringstream outp;
        // dumpEvents ( evList, maxevt, outp );
        dumpEvents(evList, dumpevt, std::cout);
        // outp << std::flush;
        // LOG (INFO) << outp.str();
    }

    // time to close the file
    if(fh.is_open()) fh.close();

    return 0;
}
