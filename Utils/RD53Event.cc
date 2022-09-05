/*!
  \file                  RD53Event.cc
  \brief                 RD53Event class implementation
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Event.h"
#include "../HWDescription/RD53.h"
#include "../HWDescription/RD53A.h"
#include "../HWDescription/RD53B.h"
#include "BitMaster/BitVector.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#include "TTree.h"
#endif

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
void RD53Event::addBoardInfo2Events(const BeBoard* pBoard, std::vector<RD53Event>& decodedEvents)
{
    for(auto& evt: decodedEvents)
        for(auto& chip_event: evt.chip_events)
        {
            int chip_id = RD53Event::lane2chipId(pBoard, 0, chip_event.hybrid_id, chip_event.chip_lane);
            if(chip_id != -1) chip_event.chip_id = chip_id;
        }
}

void RD53Event::fillDataContainer(BoardDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup)
{
    for(const auto& cOpticalGroup: *boardContainer)
        for(const auto& cHybrid: *cOpticalGroup)
            for(const auto& cChip: *cHybrid) RD53Event::fillChipDataContainer(cChip, testChannelGroup, cHybrid->getId());
}

void RD53Event::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId)
{
    bool   vectorRequired = chipContainer->isSummaryContainerType<Summary<GenericDataVector, OccupancyAndPh>>();
    size_t chipIndx;

    if((eventStatus == RD53FWEvtEncoder::GOOD) && (RD53Event::isHittedChip(hybridId, chipContainer->getId(), chipIndx) == true))
    {
        if(vectorRequired == true)
        {
            chipContainer->getSummary<GenericDataVector, OccupancyAndPh>().data1.push_back(chip_events[chipIndx].bc_id);
            chipContainer->getSummary<GenericDataVector, OccupancyAndPh>().data2.push_back(chip_events[chipIndx].trigger_id);
        }

        for(const auto& hit: chip_events[chipIndx].hit_data)
        {
            chipContainer->getChannel<OccupancyAndPh>(hit.row, hit.col).fOccupancy++;
            chipContainer->getChannel<OccupancyAndPh>(hit.row, hit.col).fPh += static_cast<float>(hit.tot);
            chipContainer->getChannel<OccupancyAndPh>(hit.row, hit.col).fPhError += static_cast<float>(hit.tot * hit.tot);
            if(testChannelGroup->isChannelEnabled(hit.row, hit.col) == false) chipContainer->getChannel<OccupancyAndPh>(hit.row, hit.col).readoutError = true;
        }
    }
}

bool RD53Event::isHittedChip(uint8_t hybrid_id, uint8_t chip_id, size_t& chipIndx) const
{
    auto it = std::find_if(
        chip_events.begin(), chip_events.end(), [&](const RD53ChipEvent& event) { return ((event.hybrid_id == hybrid_id) && (event.chip_id == chip_id) && (event.hit_data.size() != 0)); });

    if(it == chip_events.end()) return false;
    chipIndx = it - chip_events.begin();

    return true;
}

int RD53Event::lane2chipId(const BeBoard* pBoard, uint16_t optGroup_id, uint16_t hybrid_id, uint16_t chip_lane)
{
    // #############################
    // # Translate lane to chip ID #
    // #############################
    if(pBoard != nullptr)
    {
        auto opticalGroup = std::find_if(pBoard->begin(), pBoard->end(), [&](OpticalGroupContainer* cOpticalGroup) { return cOpticalGroup->getId() == optGroup_id; });
        if(opticalGroup != pBoard->end())
        {
            auto hybrid = std::find_if((*opticalGroup)->begin(), (*opticalGroup)->end(), [&](HybridContainer* cHybrid) { return cHybrid->getId() == hybrid_id; });
            if(hybrid != (*opticalGroup)->end())
            {
                auto it = std::find_if((*hybrid)->begin(), (*hybrid)->end(), [&](ChipContainer* pChip) { return static_cast<RD53*>(pChip)->getChipLane() == chip_lane; });

                if(it != (*hybrid)->end()) return (*it)->getId();
            }
        }
    }

    return -1; // Chip not found
}

void RD53Event::clearEventContainer(BeBoard& theBoard, DetectorDataContainer& theContainer)
{
    for(const auto cOpticalGroup: *theContainer.at(theBoard.getIndex()))
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
            {
                for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                    for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                    {
                        cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy   = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).fPh          = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).fPhError     = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).readoutError = false;
                    }

                cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.clear();
                cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.clear();
            }
}

// ##########################################
// # Event static data member instantiation #
// ##########################################
std::vector<RD53Event> RD53Event::decodedEvents;

std::vector<std::thread>            RD53Event::decodingThreads;
std::vector<std::vector<RD53Event>> RD53Event::vecEvents(RD53Shared::NTHREADS);
std::vector<std::vector<size_t>>    RD53Event::vecEventStart(RD53Shared::NTHREADS);
std::vector<uint16_t>               RD53Event::vecEventStatus(RD53Shared::NTHREADS);
std::vector<std::atomic<bool>>      RD53Event::vecWorkDone(RD53Shared::NTHREADS);
std::vector<uint32_t>*              RD53Event::theData;

std::condition_variable RD53Event::thereIsWork2Do;
std::atomic<bool>       RD53Event::keepDecodersRunning(false);
std::mutex              RD53Event::theMtx;

void RD53Event::PrintEvents(const std::vector<RD53Event>& events, const std::vector<uint32_t>& pData)
{
    // ##################
    // # Print raw data #
    // ##################
    if(pData.size() != 0)
        for(auto j = 0u; j < pData.size(); j++)
        {
            if(j % NWORDS_DDR3 == 0) std::cout << std::dec << j << ":\t";
            std::cout << std::hex << std::setfill('0') << std::setw(8) << pData[j] << "\t";
            if(j % NWORDS_DDR3 == NWORDS_DDR3 - 1) std::cout << std::endl;
        }

    // ######################
    // # Print decoded data #
    // ######################
    for(auto i = 0u; i < events.size(); i++)
    {
        auto& evt = events[i];
        LOG(INFO) << BOLDGREEN << "===========================" << RESET;
        LOG(INFO) << BOLDGREEN << "EVENT           = " << i << RESET;
        LOG(INFO) << BOLDGREEN << "block_size      = " << evt.block_size << RESET;
        LOG(INFO) << BOLDGREEN << "tlu_trigger_id  = " << evt.tlu_trigger_id << RESET;
        LOG(INFO) << BOLDGREEN << "data_format_ver = " << evt.data_format_ver << RESET;
        LOG(INFO) << BOLDGREEN << "tdc             = " << evt.tdc << RESET;
        LOG(INFO) << BOLDGREEN << "l1a_counter     = " << evt.l1a_counter << RESET;
        LOG(INFO) << BOLDGREEN << "bx_counter      = " << evt.bx_counter << RESET;

        for(auto& event: evt.chip_events)
        {
            LOG(INFO) << CYAN << "------- Chip Header -------" << RESET;
            LOG(INFO) << CYAN << "error_code      = " << event.error_code << RESET;
            LOG(INFO) << CYAN << "hybrid_id       = " << event.hybrid_id << RESET;
            LOG(INFO) << CYAN << "chip_lane       = " << event.chip_lane << RESET;
            LOG(INFO) << CYAN << "l1a_data_size   = " << event.l1a_data_size << RESET;
            LOG(INFO) << CYAN << "chip_type       = " << event.chip_type << RESET;
            LOG(INFO) << CYAN << "frame_delay     = " << event.frame_delay << RESET;

            LOG(INFO) << CYAN << "trigger_id      = " << event.trigger_id << RESET;
            LOG(INFO) << CYAN << "trigger_tag     = " << event.trigger_tag << RESET;
            LOG(INFO) << CYAN << "bc_id           = " << event.bc_id << RESET;

            LOG(INFO) << BOLDYELLOW << "--- Hit Data (" << event.hit_data.size() << " hits) ---" << RESET;

            for(const auto& hit: event.hit_data)
                LOG(INFO) << BOLDYELLOW << "Column: " << std::setw(3) << hit.col << std::setw(-1) << ", Row: " << std::setw(3) << hit.row << std::setw(-1) << ", ToT: " << std::setw(3) << +hit.tot
                          << std::setw(-1) << RESET;
        }
        LOG(INFO) << BOLDGREEN << "===========================" << RESET;
        LOG(INFO) << BOLDGREEN << "EVENT STATUS    = " << evt.eventStatus << RESET;
        RD53Event::EvtErrorHandler(evt.eventStatus);
    }
}

bool RD53Event::EvtErrorHandler(uint16_t status)
{
    bool isGood = true;

    if(status & RD53FWEvtEncoder::EVSIZE)
    {
        LOG(ERROR) << BOLDRED << "Invalid event size " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::EMPTY)
    {
        LOG(ERROR) << BOLDRED << "No data collected " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::NOHEADER)
    {
        LOG(ERROR) << BOLDRED << "No event headear found in data " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::INCOMPLETE)
    {
        LOG(ERROR) << BOLDRED << "Incomplete event header " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::L1A)
    {
        LOG(ERROR) << BOLDRED << "L1A counter mismatch " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::FWERR)
    {
        LOG(ERROR) << BOLDRED << "Firmware error " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::FRSIZE)
    {
        LOG(ERROR) << BOLDRED << "Invalid frame size " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::MISSCHIP)
    {
        LOG(ERROR) << BOLDRED << "Chip data are missing " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPHEAD)
    {
        LOG(ERROR) << BOLDRED << "Invalid chip header " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPPIX)
    {
        LOG(ERROR) << BOLDRED << "Invalid pixel row or column " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPNOHIT)
    {
        LOG(ERROR) << BOLDRED << " Hit data are missing " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    return isGood;
}

void RD53Event::DecodeEvents(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const std::vector<size_t>& eventStartExt, uint16_t& eventStatus)
{
    std::vector<size_t> eventStartLocal;
    eventStatus = RD53FWEvtEncoder::GOOD;

    // #####################
    // # Consistency check #
    // #####################
    if(data.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::EMPTY;
        return;
    }

    // ##########################
    // # Search for event start #
    // ##########################
    if(eventStartExt.size() == 0)
    {
        size_t i = 0u;

        while(i < data.size())
            if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
            {
                eventStartLocal.push_back(i);
                size_t block_size;
                std::tie(block_size) = bits::unpack<RD53FWEvtEncoder::NBIT_BLOCKSIZE>(data[i]);
                i += 4 * block_size;
            }
            else
            {
                eventStatus = RD53FWEvtEncoder::EVSIZE;
                return;
            }

        if(eventStartLocal.size() == 0)
        {
            eventStatus = RD53FWEvtEncoder::NOHEADER;
            return;
        }
        eventStartLocal.push_back(data.size());
    }
    const std::vector<size_t>& refEventStart = (eventStartExt.size() == 0 ? const_cast<const std::vector<size_t>&>(eventStartLocal) : eventStartExt);

    // #################################
    // # Branching for RD53A and RD53B #
    // #################################
    events.reserve(events.size() + refEventStart.size() - 1);
    if(RD53Shared::firstChip->getFrontEndType() == FrontEndType::RD53A) { RD53Event::DecodeRD53AEvents(data, events, refEventStart, eventStatus); }
    else
        try
        {
            RD53Event::DecodeRD53BEvents(data, events, refEventStart);
        }
        catch(std::runtime_error& e)
        {
            LOG(WARNING) << BOLDRED << e.what() << RESET;
            eventStatus = RD53FWEvtEncoder::EMPTY; // @TMP@
        }
}

void RD53Event::ForkDecodingThreads()
{
    if(RD53Event::decodingThreads.size() != 0) RD53Event::JoinDecodingThreads();

    RD53Event::keepDecodersRunning = true;

    RD53Event::decodingThreads.clear();
    RD53Event::decodingThreads.reserve(RD53Shared::NTHREADS);

    for(auto& events: RD53Event::vecEvents) events.clear();
    for(auto& eventStart: RD53Event::vecEventStart) eventStart.clear();
    for(auto& eventStatus: RD53Event::vecEventStatus) eventStatus = 0;
    for(auto& workDone: RD53Event::vecWorkDone) workDone = true;

    for(auto i = 0u; i < RD53Shared::NTHREADS; i++)
        RD53Event::decodingThreads.emplace_back(std::thread(&RD53Event::decoderThread,
                                                            std::ref(RD53Event::theData),
                                                            std::ref(RD53Event::vecEvents[i]),
                                                            std::ref(RD53Event::vecEventStart[i]),
                                                            std::ref(RD53Event::vecEventStatus[i]),
                                                            std::ref(RD53Event::vecWorkDone[i])));
}

void RD53Event::JoinDecodingThreads()
{
    RD53Event::keepDecodersRunning = false;
    for(auto& workDone: RD53Event::vecWorkDone) workDone = false;
    RD53Event::thereIsWork2Do.notify_all();

    for(auto& thr: RD53Event::decodingThreads)
        if(thr.joinable() == true) thr.join();
}

void RD53Event::decoderThread(std::vector<uint32_t>*& data, std::vector<RD53Event>& events, const std::vector<size_t>& eventStart, uint16_t& eventStatus, std::atomic<bool>& workDone)
{
    while(RD53Event::keepDecodersRunning == true)
    {
        std::unique_lock<std::mutex> theGuard(RD53Event::theMtx);
        RD53Event::thereIsWork2Do.wait(theGuard, [&workDone]() { return !workDone; });
        theGuard.unlock();
        if(eventStart.size() != 0) RD53Event::DecodeEvents(*data, events, eventStart, eventStatus);
        workDone = true;
    }
}

void RD53Event::DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, uint16_t& eventStatus)
{
    std::vector<size_t> eventStart;

    // #####################
    // # Consistency check #
    // #####################
    if(RD53Event::decodingThreads.size() == 0)
    {
        LOG(ERROR) << BOLDRED << "Threads for data decoding haven't been forked: use " << BOLDYELLOW << "RD53Event::ForkDecodingThreads()" << BOLDRED << " first" << RESET;
        eventStatus = RD53FWEvtEncoder::EMPTY;
        return;
    }
    if(data.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::EMPTY;
        return;
    }

    // #####################
    // # Find events start #
    // #####################
    size_t i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            size_t block_size;
            std::tie(block_size) = bits::unpack<RD53FWEvtEncoder::NBIT_BLOCKSIZE>(data[i]);
            i += 4 * block_size;
        }
        else
        {
            eventStatus = RD53FWEvtEncoder::EVSIZE;
            return;
        }

    if(eventStart.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::NOHEADER;
        return;
    }
    const auto nEvents = ceil(static_cast<double>(eventStart.size()) / RD53Shared::NTHREADS);
    eventStart.push_back(data.size());

    // ######################
    // # Unpack data vector #
    // ######################
    for(i = 0u; RD53Shared::NTHREADS > 1 && i < RD53Shared::NTHREADS - 1; i++)
    {
        auto firstEvent = eventStart.begin() + nEvents * i;
        if(firstEvent + nEvents + 1 > eventStart.end() - 1) break;
        auto lastEvent = firstEvent + nEvents + 1;
        std::move(firstEvent, lastEvent, std::back_inserter(RD53Event::vecEventStart[i]));
    }
    auto firstEvent = eventStart.begin() + nEvents * i;
    auto lastEvent  = eventStart.end();
    std::move(firstEvent, lastEvent, std::back_inserter(RD53Event::vecEventStart[i]));

    // #######################
    // # Start data decoding #
    // #######################
    RD53Event::theData = const_cast<std::vector<uint32_t>*>(&data);
    for(auto& workDone: RD53Event::vecWorkDone) workDone = false;
    RD53Event::thereIsWork2Do.notify_all();
    while(true)
    {
        bool allWorkDone = true;
        for(auto& workDone: RD53Event::vecWorkDone) allWorkDone &= workDone;
        if(allWorkDone == true) break;
    }

    // #####################
    // # Pack event vector #
    // #####################
    eventStatus = 0;
    for(auto i = 0u; i < RD53Shared::NTHREADS; i++)
    {
        RD53Shared::myMove(std::move(RD53Event::vecEvents[i]), events);
        eventStatus |= RD53Event::vecEventStatus[i];
        RD53Event::vecEventStatus[i] = 0;
        RD53Event::vecEventStart[i].clear();
    }
}

// ##########################################
// # Use of OpenMP (compiler flag -fopenmp) #
// ##########################################
/*
void RD53Event::DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, uint16_t& eventStatus)
{
    std::vector<size_t> eventStart;
    eventStatus = RD53FWEvtEncoder::GOOD;

    // #####################
    // # Consistency check #
    // #####################
    if(data.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::EMPTY;
        return;
    }

    // #####################
    // # Find events start #
    // #####################
    size_t i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            size_t block_size;
            std::tie(block_size) = bits::unpack<RD53FWEvtEncoder::NBIT_BLOCKSIZE>(data[i]);
            i += 4 * block_size;
        }
        else
        {
            eventStatus = RD53FWEvtEncoder::EVSIZE;
            return;
        }

    if(eventStart.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::NOHEADER;
        return;
    }
    const auto nEvents = ceil(static_cast<double>(eventStart.size()) / omp_get_max_threads());
    eventStart.push_back(data.size());

    // ######################
    // # Unpack data vector #
    // ######################
#pragma omp parallel
    {
        std::vector<RD53Event> vecEvents;
        std::vector<size_t>    vecEventStart;

        if(eventStart.begin() + nEvents * omp_get_thread_num() < eventStart.end())
        {
            auto     status     = eventStatus;
            auto     firstEvent = eventStart.begin() + nEvents * omp_get_thread_num();
            auto     lastEvent  = firstEvent + nEvents + 1 < eventStart.end() ? firstEvent + nEvents + 1 : eventStart.end();
            std::move(firstEvent, lastEvent, std::back_inserter(vecEventStart));

            // #################################
            // # Branching for RD53A and RD53B #
            // #################################
            if(RD53Shared::firstChip->getFrontEndType() == FrontEndType::RD53A) { RD53Event::DecodeRD53AEvents(data, vecEvents, vecEventStart, status); }
            else
                try
                {
                    RD53Event::DecodeRD53BEvents(data, vecEvents, vecEventStart);
                }
                catch(std::runtime_error& e)
                {
                    LOG(WARNING) << BOLDRED << e.what() << RESET;
                    status = RD53FWEvtEncoder::EMPTY; // @TMP@
                }

                // #####################
                // # Pack event vector #
                // #####################
#pragma omp critical
            RD53Shared::myMove(std::move(vecEvents), events);
            eventStatus |= status;
        }
    }
}
*/

// ######################
// # Specific for RD53A #
// ######################

void RD53ChipEvent::decodeChipFrame(const uint32_t data0, const uint32_t data1, RD53ChipEvent& event)
{
    std::tie(event.error_code, event.hybrid_id, event.chip_lane, event.l1a_data_size) =
        bits::unpack<RD53FWEvtEncoder::NBIT_ERR, RD53FWEvtEncoder::NBIT_HYBRID, RD53FWEvtEncoder::NBIT_CHIPID, RD53FWEvtEncoder::NBIT_L1ASIZE>(data0);
    std::tie(event.chip_type, event.frame_delay) = bits::unpack<RD53FWEvtEncoder::NBIT_CHIPTYPE, RD53FWEvtEncoder::NBIT_DELAY>(data1);
}

void RD53Event::DecodeRD53AEvent(const uint32_t* data, size_t n)
{
    eventStatus = RD53FWEvtEncoder::GOOD;

    // ######################
    // # Consistency checks #
    // ######################
    if(n < RD53FWEvtEncoder::EVT_HEADER_SIZE)
    {
        eventStatus = RD53FWEvtEncoder::INCOMPLETE;
        return;
    }

    std::tie(block_size) = bits::unpack<RD53FWEvtEncoder::NBIT_BLOCKSIZE>(data[0]);
    if(block_size * NWORDS_DDR3 != n)
    {
        eventStatus = RD53FWEvtEncoder::EVSIZE;
        return;
    }

    // #######################
    // # Decode event header #
    // #######################
    bool dummy_size;
    std::tie(tlu_trigger_id, data_format_ver, dummy_size) = bits::unpack<RD53FWEvtEncoder::NBIT_TRIGID, RD53FWEvtEncoder::NBIT_FMTVER, RD53FWEvtEncoder::NBIT_DUMMY>(data[1]);
    std::tie(tdc, l1a_counter)                            = bits::unpack<RD53FWEvtEncoder::NBIT_TDC, RD53FWEvtEncoder::NBIT_L1ACNT>(data[2]);
    bx_counter                                            = data[3];

    // ############################
    // # Search for frame lengths #
    // ############################
    std::vector<size_t> event_sizes;
    size_t              index = 4;
    while(index < n - dummy_size * NWORDS_DDR3)
    {
        if(data[index] >> (RD53FWEvtEncoder::NBIT_ERR + RD53FWEvtEncoder::NBIT_HYBRID + RD53FWEvtEncoder::NBIT_CHIPID + RD53FWEvtEncoder::NBIT_L1ASIZE) != RD53FWEvtEncoder::FRAME_HEADER)
        {
            eventStatus |= RD53FWEvtEncoder::FRSIZE;
            return;
        }

        size_t size = (data[index] & ((1 << RD53FWEvtEncoder::NBIT_L1ASIZE) - 1)) * NWORDS_DDR3;
        event_sizes.push_back(size);
        index += size;
    }

    if(index != n - dummy_size * NWORDS_DDR3)
    {
        eventStatus |= RD53FWEvtEncoder::MISSCHIP;
        return;
    }

    // ##############################
    // # Decode frame and chip data #
    // ##############################
    chip_events.reserve(event_sizes.size());
    index = 4;
    for(auto size: event_sizes)
    {
        RD53ChipEvent event;
        RD53ChipEvent::decodeChipFrame(data[index], data[index + 1], event);
        RD53Shared::firstChip->decodeChipData(&data[index + 2], size - 2, event);

        if(event.error_code != 0)
        {
            eventStatus |= RD53FWEvtEncoder::FWERR;
            chip_events.clear();
            return;
        }

        if(event.eventStatus != RD53EvtEncoder::CHIPGOOD) eventStatus |= event.eventStatus;

        chip_events.push_back(std::move(event));

        index += size;
    }
}

void RD53Event::DecodeRD53AEvents(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const std::vector<size_t>& refEventStart, uint16_t& eventStatus)
{
    const size_t maxL1Counter = RD53Shared::setBits(RD53AEvtEncoder::NBIT_TRIGID) + 1;

    for(auto i = 0u; i < refEventStart.size() - 1; i++)
    {
        const auto start = refEventStart[i];
        const auto end   = refEventStart[i + 1];

        // #######################
        // # Decode single event #
        // #######################
        RD53Event evt;
        evt.DecodeRD53AEvent(&data[start], end - start);
        events.push_back(evt);

        if(events.back().eventStatus != RD53FWEvtEncoder::GOOD)
            eventStatus |= events.back().eventStatus;
        else
        {
            for(auto j = 0u; j < events.back().chip_events.size(); j++)
                if(events.back().l1a_counter % maxL1Counter != events.back().chip_events[j].trigger_id) eventStatus |= RD53FWEvtEncoder::L1A;
        }
    }
}

// ######################
// # Specific for RD53B #
// ######################

template <class T>
size_t decodeCompressedBitpair(BitView<T>& bits)
{
    if(bits.pop(1) == 0) return 1;
    return 2 | bits.pop(1);
}

template <class T>
auto decodeCompressedHitmap(BitView<T>& bits)
{
    std::array<std::array<bool, 8>, 2> hits{{0}};

    auto row_mask = decodeCompressedBitpair(bits);

    for(size_t row = 0; row < 2; row++)
    {
        if(row_mask & (2 >> row))
        {
            auto quad_mask = decodeCompressedBitpair(bits);

            std::vector<size_t> pair_masks;
            for(size_t i = 0; i < __builtin_popcount(quad_mask); ++i)
            {
                auto pair_mask = decodeCompressedBitpair(bits);
                pair_masks.push_back(pair_mask);
            }

            int current_quad = 0;
            for(int pixel_quad = 0; pixel_quad < 2; pixel_quad++)
            {
                if(quad_mask & (2 >> pixel_quad))
                {
                    for(int pixel_pair = 0; pixel_pair < 2; pixel_pair++)
                    {
                        if(pair_masks[current_quad] & (2 >> pixel_pair))
                        {
                            size_t pixel_mask                              = decodeCompressedBitpair(bits);
                            hits[row][pixel_quad * 4 + pixel_pair * 2]     = pixel_mask & 2;
                            hits[row][pixel_quad * 4 + pixel_pair * 2 + 1] = pixel_mask & 1;
                        }
                    }
                    current_quad++;
                }
            }
        }
    }

    return hits;
}

void decodeStreamHeader(BitView<const uint32_t>& bits, RD53ChipEvent& e, const FormatOptions& options)
{
    if(options.enableBCID) e.bc_id = bits.pop(11);
    if(options.enableTriggerId) e.trigger_id = bits.pop(8);
}

void decodeChipId(uint8_t chipId, size_t i, RD53ChipEvent& e, size_t n_words)
{
    if(i == 0)
        e.chip_id_mod4 = chipId;
    else if(e.chip_id_mod4 != chipId)
        throw std::runtime_error("Found conflicting chip ID: " + std::to_string(chipId) + " (previously " + std::to_string(e.chip_id_mod4) + ") @ word # " + std::to_string(i) + " / " +
                                 std::to_string(n_words));
}

BitVector<uint32_t> decodeEventStream(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    BitVector<uint32_t> payload_data;
    size_t              n_words = bits.size() / 64;
    bool                isLast  = false;

    for(size_t i = 0; i < n_words && !isLast; i++)
    {
        isLast = bits.pop(1);

        if(isLast == true)
        {
            if(i + 2 < n_words) { throw std::runtime_error("The end-of-stream bit was 1 before the last word of the event stream"); }
        }
        else if(i == n_words)
            throw std::runtime_error("The end-of-stream bit was 0 in the last word of the event stream");

        if(options.enableChipId) decodeChipId(bits.pop(2), i, e, n_words);

        payload_data.append(bits.pop_slice(63 - 2 * options.enableChipId));
    }
    return payload_data;
}

void decodeChipEvent(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    const auto event_stream      = decodeEventStream(bits, e, options);
    auto       event_stream_view = bit_view(event_stream);

    decodeStreamHeader(event_stream_view, e, options);

    e.trigger_tag = event_stream_view.pop(8);

    std::array<int, RD53B::NCOLS / 8> last_qrow;

    while(true)
    {
        if(event_stream_view.size() < 6) return;
        size_t ccol = event_stream_view.pop(6);
        if(ccol == 0) return;

        if(8 * (ccol - 1) >= RD53B::NCOLS) throw std::runtime_error("Invalid column: " + std::to_string(8 * (ccol - 1)));

        bool isLast = false;
        while(!isLast)
        {
            isLast      = event_stream_view.pop(1);
            size_t qrow = event_stream_view.pop(1) ? last_qrow[ccol - 1] + 1 : event_stream_view.pop(8);

            if(2 * qrow >= RD53B::NROWS) throw std::runtime_error("Invalid row: " + std::to_string(2 * qrow));

            last_qrow[ccol - 1] = qrow;

            auto hitmap = decodeCompressedHitmap(event_stream_view);
            for(size_t row = 0; row < 2; row++)
                for(size_t col = 0; col < 8; col++)
                    if(hitmap[row][col])
                    {
                        uint8_t tot = 0;
                        if(options.enableToT == true)
                        {
                            tot = event_stream_view.pop(4);
                            if(tot == 15) throw std::runtime_error("Invalid tot value: 15");
                        }
                        e.hit_data.emplace_back(qrow * 2 + row, (ccol - 1) * 8 + col, tot);
                    }
        }
    }
}

size_t RD53Event::DecodeRD53BEvents(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const FormatOptions& options)
{
    return RD53Event::DecodeRD53BEvents(&data[0], events, 32 * data.size(), options);
}

size_t RD53Event::DecodeRD53BEvents(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const std::vector<size_t>& refEventStart, const FormatOptions& options)
{
    return RD53Event::DecodeRD53BEvents(&data[refEventStart[0]], events, 32 * (refEventStart.back() - refEventStart[0]), options);
}

size_t RD53Event::DecodeRD53BEvents(const uint32_t* data, std::vector<RD53Event>& events, const size_t howMany, const FormatOptions& options)
{
    auto   bits    = bit_view(data, 0, howMany);
    size_t nEvents = 0;

    while(bits.size())
    {
        if(bits.pop(RD53FWEvtEncoder::NBIT_EVTHEAD) != RD53FWEvtEncoder::EVT_HEADER) throw std::runtime_error("Invalid event container header");

        RD53Event evt;

        size_t block_size  = bits.pop(RD53FWEvtEncoder::NBIT_BLOCKSIZE);
        evt.tlu_trigger_id = bits.pop(RD53FWEvtEncoder::NBIT_TRIGID);
        bits.skip(RD53FWEvtEncoder::NBIT_FMTVER);
        size_t dummy_size = bits.pop(RD53FWEvtEncoder::NBIT_DUMMY);
        evt.tdc           = bits.pop(RD53FWEvtEncoder::NBIT_TDC);
        evt.l1a_counter   = bits.pop(RD53FWEvtEncoder::NBIT_L1ACNT);
        evt.bx_counter    = bits.pop(RD53FWEvtEncoder::NBIT_BXCNT);

        auto event_bits = bits.pop_slice(128 * (block_size - 1 - dummy_size));

        while(event_bits.size())
        {
            if(event_bits.pop(RD53FWEvtEncoder::NBIT_FRAMEHEAD) != RD53FWEvtEncoder::FRAME_HEADER) throw std::runtime_error("Invalid frame event header");

            RD53ChipEvent chipEvt;

            event_bits.skip(RD53FWEvtEncoder::NBIT_ERR);
            chipEvt.hybrid_id = event_bits.pop(RD53FWEvtEncoder::NBIT_HYBRID);
            chipEvt.chip_lane = event_bits.pop(RD53FWEvtEncoder::NBIT_CHIPID);
            size_t l1a_size   = event_bits.pop(RD53FWEvtEncoder::NBIT_L1ASIZE);
            event_bits.skip(RD53FWEvtEncoder::NBIT_PADDING);
            event_bits.skip(RD53FWEvtEncoder::NBIT_CHIPTYPE);
            event_bits.skip(RD53FWEvtEncoder::NBIT_DELAY);

            decodeChipEvent(event_bits.pop_slice(l1a_size * 128 - 64), chipEvt, options);
            evt.chip_events.push_back(std::move(chipEvt));
        }

        bits.skip(128 * dummy_size);
        events.push_back(std::move(evt));
        nEvents++;
    }

    return nEvents;
}

void RD53Event::MakeNtuple(const std::string& fileName, const std::vector<RD53Event>& events)
{
#ifdef __USE_ROOT__
    TFile theFile(fileName.c_str(), "RECREATE");
    TTree theTree("theTree", "Ntuple with event data");

    uint32_t event, FW_block_size, FW_tlu_trigger_id, FW_data_format_ver, FW_tdc, FW_l1a_counter, FW_bx_counter, FW_nframes;
    theTree.Branch("event", &event, "event/i");
    theTree.Branch("FW_block_size", &FW_block_size, "FW_block_size/i");
    theTree.Branch("FW_tlu_trigger_id", &FW_tlu_trigger_id, "FW_tlu_trigger_id/i");
    theTree.Branch("FW_data_format_ver", &FW_data_format_ver, "FW_data_format_ver/i");
    theTree.Branch("FW_tdc", &FW_tdc, "FW_tdc/i");
    theTree.Branch("FW_l1a_counter", &FW_l1a_counter, "FW_l1a_counter/i");
    theTree.Branch("FW_bx_counter", &FW_bx_counter, "FW_bx_counter/i");
    theTree.Branch("FW_nframes", &FW_nframes, "FW_nframes/i");

    std::vector<uint32_t> FW_frame_event_error_code;
    std::vector<uint32_t> FW_frame_event_hybrid_id;
    std::vector<uint32_t> FW_frame_event_chip_lane;
    std::vector<uint32_t> FW_frame_event_l1a_data_size;
    std::vector<uint32_t> FW_frame_event_chip_type;
    std::vector<uint32_t> FW_frame_event_frame_delay;

    std::vector<uint32_t> RD53_frame_event_trigger_id;
    std::vector<uint32_t> RD53_frame_event_trigger_tag;
    std::vector<uint32_t> RD53_frame_event_bc_id;
    std::vector<uint32_t> RD53_frame_event_nhits;

    theTree.Branch("FW_frame_event_error_code", &FW_frame_event_error_code);
    theTree.Branch("FW_frame_event_hybrid_id", &FW_frame_event_hybrid_id);
    theTree.Branch("FW_frame_event_chip_lane", &FW_frame_event_chip_lane);
    theTree.Branch("FW_frame_event_l1a_data_size", &FW_frame_event_l1a_data_size);
    theTree.Branch("FW_frame_event_chip_type", &FW_frame_event_chip_type);
    theTree.Branch("FW_frame_event_frame_delay", &FW_frame_event_frame_delay);

    theTree.Branch("RD53_frame_event_trigger_id", &RD53_frame_event_trigger_id);
    theTree.Branch("RD53_frame_event_trigger_tag", &RD53_frame_event_trigger_tag);
    theTree.Branch("RD53_frame_event_bc_id", &RD53_frame_event_bc_id);
    theTree.Branch("RD53_frame_event_nhits", &RD53_frame_event_nhits);

    std::vector<uint8_t> RD53_hit_row;
    std::vector<uint8_t> RD53_hit_col;
    std::vector<uint8_t> RD53_hit_tot;

    theTree.Branch("RD53_hit_row", &RD53_hit_row);
    theTree.Branch("RD53_hit_col", &RD53_hit_col);
    theTree.Branch("RD53_hit_tot", &RD53_hit_tot);

    for(auto i = 0u; i < events.size(); i++)
    {
        auto& evt = events[i];

        event              = i;
        FW_block_size      = evt.block_size;
        FW_tlu_trigger_id  = evt.tlu_trigger_id;
        FW_data_format_ver = evt.data_format_ver;
        FW_tdc             = evt.tdc;
        FW_l1a_counter     = evt.l1a_counter;
        FW_bx_counter      = evt.bx_counter;
        FW_nframes         = evt.chip_events.size();

        FW_frame_event_error_code.clear();
        FW_frame_event_hybrid_id.clear();
        FW_frame_event_chip_lane.clear();
        FW_frame_event_l1a_data_size.clear();
        FW_frame_event_chip_type.clear();
        FW_frame_event_frame_delay.clear();

        RD53_frame_event_trigger_id.clear();
        RD53_frame_event_trigger_tag.clear();
        RD53_frame_event_bc_id.clear();
        RD53_frame_event_nhits.clear();

        RD53_hit_row.clear();
        RD53_hit_col.clear();
        RD53_hit_tot.clear();

        for(auto& event: evt.chip_events)
        {
            FW_frame_event_error_code.push_back(event.error_code);
            FW_frame_event_hybrid_id.push_back(event.hybrid_id);
            FW_frame_event_chip_lane.push_back(event.chip_lane);
            FW_frame_event_l1a_data_size.push_back(event.l1a_data_size);
            FW_frame_event_chip_type.push_back(event.chip_type);
            FW_frame_event_frame_delay.push_back(event.frame_delay);

            RD53_frame_event_trigger_id.push_back(event.trigger_id);
            RD53_frame_event_trigger_tag.push_back(event.trigger_tag);
            RD53_frame_event_bc_id.push_back(event.bc_id);
            RD53_frame_event_nhits.push_back(event.hit_data.size());

            for(const auto& hit: event.hit_data)
            {
                RD53_hit_row.push_back(hit.row);
                RD53_hit_col.push_back(hit.col);
                RD53_hit_tot.push_back(hit.tot);
            }
        }

        theTree.Fill();
    }

    theTree.Write();
    theFile.Close();
#else
    LOG(WARNING) << BOLDBLUE << "[RD53Event::MakeNtuple] Function to translate raw data into ROOT ntuple was not compilded" << RESET;
#endif
}

} // namespace Ph2_HwInterface
