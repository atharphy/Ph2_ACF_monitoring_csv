/*!
  \file                  RD53eudaqProducer.h
  \brief                 Implementaion of EUDAQ producer
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53eudaqProducer.h"

RD53eudaqProducer::RD53eudaqProducer(Ph2_System::SystemController& RD53SysCntr, const std::string configFile, const std::string producerName, const std::string runControl)
    : eudaq::Producer(producerName, runControl), configFile(configFile)
{
    RD53sysCntrPhys.Inherit(&RD53SysCntr);
    RD53sysCntrPhys.setGenericEvtConverter(RD53eudaqProducer::RD53eudaqEvtConverter(this));

    this->SetStatus(eudaq::Status::STATE_UNINIT, "RD53eudaqProducer::Uninitialized");
    this->SetStatus(eudaq::Status::STATE_UNCONF, "RD53eudaqProducer::Unconfigured");
}

void RD53eudaqProducer::DoReset()
{
    RD53sysCntrPhys.Stop();

    this->SetStatus(eudaq::Status::STATE_UNINIT, "RD53eudaqProducer::Uninitialized");
    this->SetStatus(eudaq::Status::STATE_UNCONF, "RD53eudaqProducer::Unconfigured");
}

void RD53eudaqProducer::DoInitialise()
{
    std::stringstream outp;
    RD53sysCntrPhys.InitializeHw(configFile, outp, true, false);
    RD53sysCntrPhys.InitializeSettings(configFile, outp);

    this->SetStatus(eudaq::Status::STATE_UNCONF, "RD53eudaqProducer::Unconfigured");
}

void RD53eudaqProducer::DoConfigure()
{
    RD53sysCntrPhys.localConfigure("", -1);

    this->SetStatus(eudaq::Status::STATE_CONF, "RD53eudaqProducer::Configured");
}

void RD53eudaqProducer::DoStartRun()
{
    fRunNumber = this->GetRunNumber();

    // ###################################################
    // # Get configuration directly from EUDAQ framework #
    // ###################################################
    // auto eudaqConf = this->GetConfiguration();
    // std::string fileName(eudaqConf->Get("Results", "Run" + RD53Shared::fromInt2Str(fRunNumber) + "_Physics"));
    std::string fileName("Run" + RD53Shared::fromInt2Str(fRunNumber) + "_Physics");
    RD53sysCntrPhys.initializeFiles(fileName, -1);
    RD53sysCntrPhys.Running();

    this->SetStatus(eudaq::Status::STATE_RUNNING, "RD53eudaqProducer::Running");
}

void RD53eudaqProducer::DoStopRun()
{
    RD53sysCntrPhys.Stop();
    RD53sysCntrPhys.draw();

    // ###########################
    // # Copy configuration file #
    // ###########################
    const auto configFileBasename = configFile.substr(configFile.find_last_of("/\\") + 1);
    const auto outputConfigFile   = std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(runNumber) + "_" + configFileBasename;
    system(("cp " + configFile + " " + outputConfigFile).c_str());

    this->SetStatus(eudaq::Status::STATE_STOPPED, "RD53eudaqProducer::Stopped");
    this->SetStatus(eudaq::Status::STATE_CONF, "RD53eudaqProducer::Configured");
}

void RD53eudaqProducer::DoTerminate() { RD53eudaqProducer::DoStopRun(); }

void RD53eudaqProducer::RD53eudaqEvtConverter::operator()(const std::vector<Ph2_HwInterface::RD53FWInterface::Event>& RD53EvtList)
{
    std::time_t timeStamp = std::time(nullptr);

    if(RD53EvtList.size() != 0)
    {
        eudaq::EventSP eudaqEvent = eudaq::Event::MakeShared("RD53eudaqEventHeadaer");
        eudaqEvent->SetTimestamp(timeStamp, timeStamp);

        for(const auto& evt: RD53EvtList)
        {
            eudaq::EventSP eudaqSubEvent = eudaq::Event::MakeShared("RD53eudaqEvent");

            eudaqSubEvent->SetTag("L1A_COUNTER", evt.l1a_counter);
            eudaqSubEvent->SetTag("TDC", evt.tdc);
            eudaqSubEvent->SetTag("BX_COUNTER", evt.bx_counter);
            eudaqSubEvent->SetTag("TLU_TRIGGER_ID", evt.tlu_trigger_id);
            eudaqSubEvent->SetTriggerN(evt.tlu_trigger_id);

            for(auto i = 0u; i < evt.chip_events.size(); i++)
            {
                std::vector<uint8_t> eudaq_hits;
                eudaq_hits.push_back((Ph2_HwDescription::RD53::nRows >> 0) & 0xFF);
                eudaq_hits.push_back((Ph2_HwDescription::RD53::nRows >> 8) & 0xFF);
                eudaq_hits.push_back((Ph2_HwDescription::RD53::nCols >> 0) & 0xFF);
                eudaq_hits.push_back((Ph2_HwDescription::RD53::nCols >> 8) & 0xFF);
                eudaq_hits.push_back((evt.chip_events[i].hit_data.size() >> 0) & 0xFF);
                eudaq_hits.push_back((evt.chip_events[i].hit_data.size() >> 8) & 0xFF);
                for(const auto& hit: evt.chip_events[i].hit_data)
                {
                    // #######
                    // # ROW #
                    // #######
                    eudaq_hits.push_back((hit.row >> 0) & 0xFF);
                    eudaq_hits.push_back((hit.row >> 8) & 0xFF);
                    // #######
                    // # COL #
                    // #######
                    eudaq_hits.push_back((hit.col >> 0) & 0xFF);
                    eudaq_hits.push_back((hit.col >> 8) & 0xFF);
                    // #######
                    // # TOT #
                    // #######
                    eudaq_hits.push_back(hit.tot);
                    eudaq_hits.push_back(0);
                }
                // ###########
                // # Chip ID #
                // ###########
                eudaqSubEvent->AddBlock(evt.chip_frames[i].chip_id, eudaq_hits);
            }

            eudaqSubEvent->SetTimestamp(timeStamp, timeStamp);
            eudaqEvent->AddSubEvent(eudaqSubEvent);
        }

        eudaqProducer->SendEvent(eudaqEvent);
    }
}
