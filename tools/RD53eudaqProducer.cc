/*!OA
  \file                  RD53eudaqProducer.h
  \brief                 Implementaion of EUDAQ producer
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53eudaqProducer.h"

void RD53eudaqProducer::DoReset() { RD53sysCntrPhys.Stop(); }

void RD53eudaqProducer::DoInitialise()
{
    std::stringstream outp;
    RD53sysCntrPhys.InitializeHw(configFile, outp, true, false);
    RD53sysCntrPhys.InitializeSettings(configFile, outp);
    nTRIGxEvent = RD53sysCntrPhys.findValueInSettings<double>("nTRIGxEvent");
}

void RD53eudaqProducer::DoConfigure() { RD53sysCntrPhys.localConfigure(); }

void RD53eudaqProducer::DoStartRun()
{
    // #################################
    // # Reconfigure all readout chips #
    // #################################
    LOG(INFO) << GREEN << "[RD53eudaqProducer::OnStartRun] Reconfiguring all readout chips" << RESET;
    for(const auto cBoard: *RD53sysCntrPhys.fDetectorContainer)
        for(auto cOpticalGroup: *cBoard)
            for(auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid) static_cast<Ph2_HwInterface::RD53Interface*>(RD53sysCntrPhys.fReadoutChipInterface)->ConfigureChip(cChip);

    theRunNumber = GetRunNumber();
    evCounter    = 0;

    // #####################
    // # Send a BORE event #
    // #####################
    auto ev = eudaq::Event::MakeUnique(EUDAQ::EVENT);
    ev->SetBORE();
    RD53eudaqProducer::MySendEvent(std::move(ev));

    // ###################################################
    // # Get configuration directly from EUDAQ framework #
    // ###################################################
    std::string fileName("Run" + RD53Shared::fromInt2Str(theRunNumber) + "_Physics");
    RD53sysCntrPhys.initializeFiles(fileName);
    RD53sysCntrPhys.Start(theRunNumber);
}

void RD53eudaqProducer::DoStopRun()
{
    RD53sysCntrPhys.Stop();

    // #####################
    // # Send a EORE event #
    // #####################
    auto ev = eudaq::Event::MakeUnique(EUDAQ::EVENT);
    ev->SetEORE();
    RD53eudaqProducer::MySendEvent(std::move(ev));

    // ###########################
    // # Copy configuration file #
    // ###########################
    const auto configFileBasename = configFile.substr(configFile.find_last_of("/\\") + 1);
    const auto outputConfigFile   = std::string(RD53Shared::RESULTDIR) + "/Run" + RD53Shared::fromInt2Str(theRunNumber) + "_" + configFileBasename;
    system(("cp " + configFile + " " + outputConfigFile).c_str());

    // #####################
    // # Update run number #
    // #####################
    std::ofstream fileRunNumberOut;
    theRunNumber++;
    fileRunNumberOut.open(EUDAQ::FILERUNNUMBER, std::ios::out);
    if(fileRunNumberOut.is_open() == true) fileRunNumberOut << RD53Shared::fromInt2Str(theRunNumber) << std::endl;
    fileRunNumberOut.close();
}

void RD53eudaqProducer::DoTerminate()
{
    std::unique_lock<std::mutex> theGuard(theMtx);
    doExit = true;
    theGuard.unlock();
    wakeUp.notify_one();
}

void RD53eudaqProducer::RunLoop()
{
    std::unique_lock<std::mutex> theGuard(theMtx);
    wakeUp.wait(theGuard, [this]() { return doExit; });
}

void RD53eudaqProducer::Creator(Ph2_System::SystemController& RD53SysCntr, const std::string& fileName)
{
    configFile = fileName;
    doExit     = false;
    RD53sysCntrPhys.Inherit(&RD53SysCntr);
    RD53sysCntrPhys.setGenericEvtConverter(RD53eudaqProducer::RD53eudaqEvtConverter(this));
}

void RD53eudaqProducer::MainLoop()
{
    while(this->IsConnected() == true) std::this_thread::sleep_for(std::chrono::milliseconds(EUDAQ::WAIT));
}

void RD53eudaqProducer::MySendEvent(eudaq::EventSP theEvent)
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
            auto eudaqEvent = eudaq::Event::MakeUnique(EUDAQ::EVENT);

            auto                      tluTrigId = RD53EvtList[it].tlu_trigger_id;
            CMSITEventData::EventData theEvent{std::time(nullptr), eudaqProducer->nTRIGxEvent, RD53EvtList[it].l1a_counter, RD53EvtList[it].tdc, RD53EvtList[it].bx_counter, tluTrigId, {}};

            // ##################################################
            // # Collect all hits that have same TLU trigger ID #
            // ##################################################
            do
            {
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

            eudaqEvent->AddBlock(eudaqProducer->evCounter, theStream.c_str(), theStream.size());
            eudaqProducer->MySendEvent(std::move(eudaqEvent));

            eudaqProducer->evCounter++;
        }
    }
}

namespace
{
auto dummy0 = eudaq::Factory<eudaq::Producer>::Register<RD53eudaqProducer, const std::string&, const std::string&>(RD53eudaqProducer::m_id_factory);
}
