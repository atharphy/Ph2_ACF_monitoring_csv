/*!OA
  \file                  RD53eudaqProducer.h
  \brief                 Implementaion of EUDAQ producer
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53eudaqProducer.h"

RD53eudaqProducer::RD53eudaqProducer(Ph2_System::SystemController& RD53SysCntr, const std::string& configFile, const std::string& producerName, const std::string& runControl)
    : eudaq::Producer(producerName, runControl), configFile(configFile)
{
    try
    {
        doExit = false;
        RD53sysCntrPhys.Inherit(&RD53SysCntr);
        RD53sysCntrPhys.setGenericEvtConverter(RD53eudaqProducer::RD53eudaqEvtConverter(this));

        this->SetConnectionState(eudaq::ConnectionState::STATE_UNINIT, "RD53eudaqProducer::Uninitialized");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Constructor Error");
    }
}

void RD53eudaqProducer::OnReset()
{
    try
    {
        RD53sysCntrPhys.Stop();

        this->SetConnectionState(eudaq::ConnectionState::STATE_UNINIT, "RD53eudaqProducer::Uninitialized");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Resetting Error");
    }
}

void RD53eudaqProducer::OnInitialise(const eudaq::Configuration& param)
{
    try
    {
        std::stringstream outp;
        RD53sysCntrPhys.InitializeHw(configFile, outp, true, false);
        RD53sysCntrPhys.InitializeSettings(configFile, outp);

        this->SetConnectionState(eudaq::ConnectionState::STATE_UNCONF, "RD53eudaqProducer::Unconfigured");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Initialisation Error");
    }
}

void RD53eudaqProducer::OnConfigure(const eudaq::Configuration& param)
{
    try
    {
        RD53sysCntrPhys.localConfigure();

        this->SetConnectionState(eudaq::ConnectionState::STATE_CONF, "RD53eudaqProducer::Configured");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Configuration Error");
    }
}

void RD53eudaqProducer::OnStartRun(unsigned runNumber)
{
    try
    {
        theRunNumber = runNumber;
        evCounter    = 0;

        // #####################
        // # Send a BORE event #
        // #####################
        eudaq::RawDataEvent evBORE(eudaq::RawDataEvent::BORE(EUDAQ::EVENT, theRunNumber));
        RD53eudaqProducer::MySendEvent(evBORE);

        // ###################################################
        // # Get configuration directly from EUDAQ framework #
        // ###################################################
        // auto eudaqConf = this->GetConfiguration();
        // std::string fileName(eudaqConf->Get("Results", "Run" + RD53Shared::fromInt2Str(runNumber) + "_Physics"));
        std::string fileName("Run" + RD53Shared::fromInt2Str(theRunNumber) + "_Physics");
        RD53sysCntrPhys.initializeFiles(fileName);
        RD53sysCntrPhys.Start(theRunNumber);

        this->SetConnectionState(eudaq::ConnectionState::STATE_RUNNING, "RD53eudaqProducer::Running");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Running Error");
    }
}

void RD53eudaqProducer::OnStopRun()
{
    try
    {
        RD53sysCntrPhys.Stop();

        // #####################
        // # Send a EORE event #
        // #####################
        eudaq::RawDataEvent evEORE(eudaq::RawDataEvent::EORE(EUDAQ::EVENT, theRunNumber, evCounter));
        RD53eudaqProducer::MySendEvent(evEORE);

        // ###########################
        // # Copy configuration file #
        // ###########################
        const auto configFileBasename = configFile.substr(configFile.find_last_of("/\\") + 1);
        const auto outputConfigFile   = std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(theRunNumber) + "_" + configFileBasename;
        system(("cp " + configFile + " " + outputConfigFile).c_str());

        this->SetConnectionState(eudaq::ConnectionState::STATE_CONF, "RD53eudaqProducer::Configured");
    }
    catch(...)
    {
        this->SetConnectionState(eudaq::ConnectionState::STATE_ERROR, "RD53eudaqProducer::Stopping Error");
    }
}

void RD53eudaqProducer::OnTerminate()
{
    std::unique_lock<std::mutex> theGuard(theMtx);
    doExit = true;
    theGuard.unlock();
    wakeUp.notify_one();
}

void RD53eudaqProducer::MainLoop()
{
    std::unique_lock<std::mutex> theGuard(theMtx);
    wakeUp.wait(theGuard, [this]() { return doExit; });
}

void RD53eudaqProducer::MySendEvent(eudaq::Event& theEvent)
{
    while(true)
    {
        try
        {
            this->SendEvent(theEvent);
            break;
        }
        catch(...)
        {
            std::cout << "Resource unavailable" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(EUDAQ::WAIT));
    }
}

void RD53eudaqProducer::RD53eudaqEvtConverter::operator()(const std::vector<Ph2_HwInterface::RD53Event>& RD53EvtList)
{
    if(RD53EvtList.size() != 0)
    {
        size_t it = 0;
        while(it < RD53EvtList.size())
        {
            eudaq::RawDataEvent eudaqEvent(EUDAQ::EVENT, eudaqProducer->theRunNumber, eudaqProducer->evCounter);

            auto                      tluTrigId = RD53EvtList[it].tlu_trigger_id;
            CMSITEventData::EventData theEvent{std::time(nullptr), RD53EvtList[it].l1a_counter, RD53EvtList[it].tdc, RD53EvtList[it].bx_counter, tluTrigId, {}};

            // ##################################################
            // # Collect all hits that have same TLU trigger ID #
            // ##################################################
            do {
                for(const auto& frame: RD53EvtList[it].chip_frames_events)
                {
                    std::string chipType = "unknown";
                    for(const auto& cHybrid: *(eudaqProducer->RD53sysCntrPhys.fDetectorContainer->at(0)->at(0)))
                        for(const auto& cChip: *cHybrid)
                            if((cHybrid->getId() == frame.first.hybrid_id) && (cChip->getId() == frame.first.chip_id)) chipType = static_cast<Ph2_HwDescription::RD53*>(cChip)->getComment();

                    theEvent.chipData.push_back(
                        {chipType, frame.first.chip_id, frame.first.chip_lane, frame.first.hybrid_id, frame.second.trigger_id, frame.second.trigger_tag, frame.second.bc_id, {}});
                    for(const auto& hit: frame.second.hit_data) theEvent.chipData.back().hits.push_back({hit.row, hit.col, hit.tot});
                }

                it++;
            } while((it < RD53EvtList.size()) && (RD53EvtList[it].tlu_trigger_id == tluTrigId));

            // #################
            // # Serialization #
            // #################
            std::ostringstream              theSerialized;
            boost::archive::binary_oarchive theArchive(theSerialized);
            theArchive << theEvent;
            const std::string& theStream = theSerialized.str();

            eudaqEvent.AddBlock(eudaqProducer->evCounter, theStream.c_str(), theStream.size());
            eudaqProducer->MySendEvent(eudaqEvent);

            eudaqProducer->evCounter++;
        }
    }
}
