/*!
  \file                  RD53Event.cc
  \brief                 RD53Event class implementation
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Event.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
ChipFrame::ChipFrame(const uint32_t data0, const uint32_t data1)
{
    std::tie(error_code, hybrid_id, chip_lane, l1a_data_size) =
        bits::unpack<RD53FWEvtEncoder::NBIT_ERR, RD53FWEvtEncoder::NBIT_HYBRID, RD53FWEvtEncoder::NBIT_CHIPID, RD53FWEvtEncoder::NBIT_L1ASIZE>(data0);
    std::tie(chip_type, frame_delay) = bits::unpack<RD53FWEvtEncoder::NBIT_CHIPTYPE, RD53FWEvtEncoder::NBIT_DELAY>(data1);
}

RD53Event::RD53Event(const uint32_t* data, size_t n)
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
    chip_frames_events.reserve(event_sizes.size());
    index = 4;
    for(auto size: event_sizes)
    {
        chip_frames_events.emplace_back(std::pair<ChipFrame, RD53::Event>(ChipFrame(data[index], data[index + 1]), RD53::Event(&data[index + 2], size - 2)));

        if(chip_frames_events.back().first.error_code != 0)
        {
            eventStatus |= RD53FWEvtEncoder::FWERR;
            chip_frames_events.clear();
            return;
        }

        if(chip_frames_events.back().second.eventStatus != RD53EvtEncoder::CHIPGOOD) eventStatus |= chip_frames_events.back().second.eventStatus;

        index += size;
    }
}

void RD53Event::addBoardInfo2Events(const BeBoard* pBoard, std::vector<RD53Event>& decodedEvents)
{
    for(auto& evt: decodedEvents)
        for(auto& frame_event: evt.chip_frames_events)
        {
            int chip_id = RD53Event::lane2chipId(pBoard, 0, frame_event.first.hybrid_id, frame_event.first.chip_lane);
            if(chip_id != -1) frame_event.first.chip_id = chip_id;
        }
}

void RD53Event::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    bool   vectorRequired = boardContainer->at(0)->at(0)->at(0)->isSummaryContainerType<Summary<GenericDataVector, OccupancyAndPh>>();
    size_t chipIndx;

    for(const auto& cOpticalGroup: *boardContainer)
        for(const auto& cHybrid: *cOpticalGroup)
            for(const auto& cChip: *cHybrid)
                if((eventStatus == RD53FWEvtEncoder::GOOD) && (RD53Event::isHittedChip(cHybrid->getId(), cChip->getId(), chipIndx) == true))
                {
                    if(vectorRequired == true)
                    {
                        cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.push_back(chip_frames_events[chipIndx].second.bc_id);
                        cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.push_back(chip_frames_events[chipIndx].second.trigger_id);
                    }

                    for(const auto& hit: chip_frames_events[chipIndx].second.hit_data)
                    {
                        cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).fOccupancy++;
                        cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).fPh += static_cast<float>(hit.tot);
                        cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).fPhError += static_cast<float>(hit.tot * hit.tot);
                        if(cTestChannelGroup->isChannelEnabled(hit.row, hit.col) == false) cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).readoutError = true;
                    }
                }
}

bool RD53Event::isHittedChip(uint8_t hybrid_id, uint8_t chip_id, size_t& chipIndx) const
{
    auto it = std::find_if(chip_frames_events.begin(), chip_frames_events.end(), [&](const std::pair<ChipFrame, RD53::Event>& frame_event) {
        return ((frame_event.first.hybrid_id == hybrid_id) && (frame_event.first.chip_id == chip_id) && (frame_event.second.hit_data.size() != 0));
    });

    if(it == chip_frames_events.end()) return false;
    chipIndx = it - chip_frames_events.begin();
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

        for(auto& frame_event: evt.chip_frames_events)
        {
            LOG(INFO) << CYAN << "------- Chip Header -------" << RESET;
            LOG(INFO) << CYAN << "error_code      = " << frame_event.first.error_code << RESET;
            LOG(INFO) << CYAN << "hybrid_id       = " << frame_event.first.hybrid_id << RESET;
            LOG(INFO) << CYAN << "chip_lane       = " << frame_event.first.chip_lane << RESET;
            LOG(INFO) << CYAN << "l1a_data_size   = " << frame_event.first.l1a_data_size << RESET;
            LOG(INFO) << CYAN << "chip_type       = " << frame_event.first.chip_type << RESET;
            LOG(INFO) << CYAN << "frame_delay     = " << frame_event.first.frame_delay << RESET;

            LOG(INFO) << CYAN << "trigger_id      = " << frame_event.second.trigger_id << RESET;
            LOG(INFO) << CYAN << "trigger_tag     = " << frame_event.second.trigger_tag << RESET;
            LOG(INFO) << CYAN << "bc_id           = " << frame_event.second.bc_id << RESET;

            LOG(INFO) << BOLDYELLOW << "--- Hit Data (" << frame_event.second.hit_data.size() << " hits) ---" << RESET;

            for(const auto& hit: frame_event.second.hit_data)
                LOG(INFO) << BOLDYELLOW << "Column: " << std::setw(3) << hit.col << std::setw(-1) << ", Row: " << std::setw(3) << hit.row << std::setw(-1) << ", ToT: " << std::setw(3) << +hit.tot
                          << std::setw(-1) << RESET;
        }
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
    const size_t        maxL1Counter = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;
    eventStatus                      = RD53FWEvtEncoder::GOOD;

    // #####################
    // # Consistency check #
    // #####################
    if(data.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::EMPTY;
        return;
    }

    if(eventStartExt.size() == 0)
    {
        size_t i = 0u;
        while(i < data.size())
            if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
            {
                eventStartLocal.push_back(i);
                i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
            }
            else
                i++;
        if(eventStartLocal.size() == 0)
        {
            eventStatus = RD53FWEvtEncoder::NOHEADER;
            return;
        }
        eventStartLocal.push_back(data.size());
    }
    const std::vector<size_t>& refEventStart = (eventStartExt.size() == 0 ? const_cast<const std::vector<size_t>&>(eventStartLocal) : eventStartExt);

    events.reserve(events.size() + refEventStart.size() - 1);

    for(auto i = 0u; i < refEventStart.size() - 1; i++)
    {
        const auto start = refEventStart[i];
        const auto end   = refEventStart[i + 1];

        events.emplace_back(&data[start], end - start);
        if(events.back().eventStatus != RD53FWEvtEncoder::GOOD)
            eventStatus |= events.back().eventStatus;
        else
        {
            for(auto j = 0u; j < events.back().chip_frames_events.size(); j++)
                if(events.back().l1a_counter % maxL1Counter != events.back().chip_frames_events[j].second.trigger_id) eventStatus |= RD53FWEvtEncoder::L1A;
        }
    }
}

void RD53Event::ForkDecodingThreads()
{
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
    std::vector<size_t> eventStart;
    size_t              i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
        }
        else
            i++;
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
    std::vector<size_t> eventStart;
    size_t              i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
        }
        else
            i++;
    if(eventStart.size() == 0)
    {
        eventStatus = RD53FWEvtEncoder::NOHEADER;
        return;
    }
    const auto nEvents = ceil(static_cast<double>(eventStart.size()) / omp_get_max_threads());
    eventStart.push_back(data.size());

    eventStatus = 0;

    // ######################
    // # Unpack data vector #
    // ######################
#pragma omp parallel
    {
        std::vector<RD53Event> vecEvents;
        std::vector<size_t>    vecEventStart;

        if(eventStart.begin() + nEvents * omp_get_thread_num() < eventStart.end())
        {
            uint16_t status;
            auto     firstEvent = eventStart.begin() + nEvents * omp_get_thread_num();
            auto     lastEvent  = firstEvent + nEvents + 1 < eventStart.end() ? firstEvent + nEvents + 1 : eventStart.end();
            std::move(firstEvent, lastEvent, std::back_inserter(vecEventStart));

            RD53Event::DecodeEvents(data, vecEvents, vecEventStart, status);

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
        FW_nframes         = evt.chip_frames_events.size();

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

        for(auto& frame_event: evt.chip_frames_events)
        {
            FW_frame_event_error_code.push_back(frame_event.first.error_code);
            FW_frame_event_hybrid_id.push_back(frame_event.first.hybrid_id);
            FW_frame_event_chip_lane.push_back(frame_event.first.chip_lane);
            FW_frame_event_l1a_data_size.push_back(frame_event.first.l1a_data_size);
            FW_frame_event_chip_type.push_back(frame_event.first.chip_type);
            FW_frame_event_frame_delay.push_back(frame_event.first.frame_delay);

            RD53_frame_event_trigger_id.push_back(frame_event.second.trigger_id);
            RD53_frame_event_trigger_tag.push_back(frame_event.second.trigger_tag);
            RD53_frame_event_bc_id.push_back(frame_event.second.bc_id);
            RD53_frame_event_nhits.push_back(frame_event.second.hit_data.size());

            for(const auto& hit: frame_event.second.hit_data)
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
