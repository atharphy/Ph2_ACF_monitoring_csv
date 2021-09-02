/*!
  \file                  RD53Event.h
  \brief                 RD53Event class implementation
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Event_H
#define RD53Event_H

#include "../HWDescription/RD53.h"
#include "DataContainer.h"
#include "Event.h"
#include "GenericDataVector.h"

#include <atomic>
#include <condition_variable>
#include <omp.h>

#ifdef __USE_ROOT__
#include "TFile.h"
#include "TTree.h"
#endif

// #############
// # CONSTANTS #
// #############
#define NWORDS_DDR3 4 // Number of IPbus words in a DDR3 word

// ##############################
// # RD53Event useful constants #
// ##############################
namespace RD53FWEvtEncoder
{
// ################
// # Event header #
// ################
const uint16_t EVT_HEADER      = 0xFFFF;
const uint16_t EVT_HEADER_SIZE = 4;  // Number of words in event header
const uint8_t  NBIT_EVTHEAD    = 16; // Number of bits for the Error Code
const uint8_t  NBIT_BLOCKSIZE  = 16; // Number of bits for the Block Size
const uint8_t  NBIT_TRIGID     = 16; // Number of bits for the TLU Trigger ID
const uint8_t  NBIT_FMTVER     = 8;  // Number of bits for the Format Version
const uint8_t  NBIT_DUMMY      = 8;  // Number of bits for the Dummy Size
const uint8_t  NBIT_TDC        = 8;  // Number of bits for the TDC
const uint8_t  NBIT_L1ACNT     = 24; // Number of bits for the L1A Counter (Event number)
const uint8_t  NBIT_BXCNT      = 32; // Number of bits for the BX Counter

// ###############
// # Chip header #
// ###############
const uint8_t FRAME_HEADER   = 0xA;
const uint8_t NBIT_FRAMEHEAD = 4;  // Number of bits for the Frame Header
const uint8_t NBIT_ERR       = 4;  // Number of bits for the Error Code
const uint8_t NBIT_HYBRID    = 8;  // Number of bits for the Hybrid ID
const uint8_t NBIT_CHIPID    = 4;  // Number of bits for the Chip ID
const uint8_t NBIT_L1ASIZE   = 12; // Number of bits for the L1A Data Size
const uint8_t NBIT_CHIPTYPE  = 4;  // Number of bits for the Chip Type
const uint8_t NBIT_DELAY     = 12; // Number of bits for the Frame Delay

// ################
// # Event status #
// ################
const uint16_t GOOD       = 0x0000; // Event status Good
const uint16_t EVSIZE     = 0x0001; // Event status Invalid event size
const uint16_t EMPTY      = 0x0002; // Event status Empty event
const uint16_t NOHEADER   = 0x0004; // Event status No event headear found in data
const uint16_t INCOMPLETE = 0x0008; // Event status Incomplete event header
const uint16_t L1A        = 0x0010; // Event status L1A counter mismatch
const uint16_t FWERR      = 0x0020; // Event status Firmware error
const uint16_t FRSIZE     = 0x0040; // Event status Invalid frame size
const uint16_t MISSCHIP   = 0x0080; // Event status Chip data are missing
} // namespace RD53FWEvtEncoder

namespace Ph2_HwInterface
{
struct ChipFrame
{
    ChipFrame(const uint32_t data0, const uint32_t data1);

    uint16_t error_code;
    uint16_t hybrid_id;
    uint16_t chip_id;
    uint16_t chip_lane;
    uint16_t l1a_data_size;
    uint16_t chip_type;
    uint16_t frame_delay;
};

class RD53Event : public Ph2_HwInterface::Event
{
  public:
    RD53Event(const uint32_t* data, size_t n);

    void fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;

    static void addBoardInfo2Events(const Ph2_HwDescription::BeBoard* pBoard, std::vector<RD53Event>& decodedEvents);
    static void ForkDecodingThreads();
    static void JoinDecodingThreads();
    static void DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, uint16_t& eventStatus);
    static void DecodeEvents(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const std::vector<size_t>& eventStart, uint16_t& eventStatus);
    static bool EvtErrorHandler(uint16_t status);
    static void PrintEvents(const std::vector<RD53Event>& events, const std::vector<uint32_t>& pData = {});
    static void MakeNtuple(const std::string& fileName, const std::vector<RD53Event>& events);

    uint16_t block_size;
    uint16_t tlu_trigger_id;
    uint16_t data_format_ver;
    uint16_t tdc;
    uint32_t l1a_counter;
    uint32_t bx_counter;

    std::vector<std::pair<ChipFrame, Ph2_HwDescription::RD53::Event>> chip_frames_events;

    uint16_t eventStatus;

    // ########################################
    // # Vector containing the decoded events #
    // ########################################
    static std::vector<RD53Event> decodedEvents;

  private:
    bool        isHittedChip(uint8_t hybrid_id, uint8_t chip_id, size_t& chipIndx) const;
    static int  lane2chipId(const Ph2_HwDescription::BeBoard* pBoard, uint16_t optGroup_id, uint16_t hybrid_id, uint16_t chip_lane);
    static void decoderThread(std::vector<uint32_t>*& data, std::vector<RD53Event>& events, const std::vector<size_t>& eventStart, uint16_t& eventStatus, std::atomic<bool>& workDone);

    static std::vector<std::thread>            decodingThreads;
    static std::vector<std::vector<RD53Event>> vecEvents;
    static std::vector<std::vector<size_t>>    vecEventStart;
    static std::vector<uint16_t>               vecEventStatus;
    static std::vector<std::atomic<bool>>      vecWorkDone;
    static std::vector<uint32_t>*              theData;

    static std::condition_variable thereIsWork2Do;
    static std::atomic<bool>       keepDecodersRunning;
    static std::mutex              theMtx;
};

} // namespace Ph2_HwInterface

#endif
