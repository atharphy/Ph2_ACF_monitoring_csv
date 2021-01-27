/*!
  \file                  RD53FWInterface.h
  \brief                 RD53FWInterface initialize and configure the FW
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53FWInterface_H
#define RD53FWInterface_H

#include "../HWDescription/RD53.h"
#include "../Utils/DataContainer.h"
#include "../Utils/Event.h"
#include "../Utils/GenericDataVector.h"
#include "../Utils/OccupancyAndPh.h"
#include "../Utils/RD53RunProgress.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/easylogging++.h"
#include "BeBoardFWInterface.h"
#include "D19cFpgaConfig.h"
#include <uhal/uhal.hpp>

#include <omp.h>

// #############
// # CONSTANTS #
// #############
#define DEEPSLEEP 100000     // [microseconds]
#define READOUTSLEEP 50      // [microseconds]
#define MAXATTEMPTS 20       // Maximum number of attempts
#define NWORDS_DDR3 4        // Number of IPbus words in a DDR3 word
#define NLANE_HYBRID 4       // Number of lanes per hybrid
#define HEADEAR_WRTCMD 0xFF  // Header of chip write command sequence
#define NBIT_FWVER 16        // Number of bits for the firmware version
#define IPBUS_FASTDURATION 1 // Duration of a fast command in terms of 40 MHz clk cycles

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
class RD53FWInterface : public BeBoardFWInterface
{
  public:
    RD53FWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    ~RD53FWInterface() { delete fFileHandler; }

    void      setFileHandler(FileHandler* pHandler) override;
    uint32_t  getBoardInfo() override;
    BoardType getBoardType() const override { return BoardType::RD53; }

    void ResetSequence();
    void ConfigureBoard(const Ph2_HwDescription::BeBoard* pBoard) override;

    void Start() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    void     ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait = true) override;
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait = true) override;
    void     ChipReset() override;
    void     ChipReSync() override;

    // #############################################
    // # hybridId < 0 --> broadcast to all hybrids #
    // #############################################
    void                                       WriteChipCommand(const std::vector<uint16_t>& data, int hybridId);
    void                                       ComposeAndPackChipCommands(const std::vector<uint16_t>& data, int hybridId, std::vector<uint32_t>& commandList);
    void                                       SendChipCommandsPack(const std::vector<uint32_t>& commandList);
    std::vector<std::pair<uint16_t, uint16_t>> ReadChipRegisters(Ph2_HwDescription::ReadoutChip* pChip);

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

    struct Event : public Ph2_HwInterface::Event
    {
        Event(const uint32_t* data, size_t n);

        void        fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;
        static void addBoardInfo2Events(const Ph2_HwDescription::BeBoard* pBoard, std::vector<RD53FWInterface::Event>& decodedEvents);

        uint16_t block_size;
        uint16_t tlu_trigger_id;
        uint16_t data_format_ver;
        uint16_t tdc;
        uint32_t l1a_counter;
        uint32_t bx_counter;

        std::vector<ChipFrame>                      chip_frames;
        std::vector<Ph2_HwDescription::RD53::Event> chip_events;

        uint16_t evtStatus;

      protected:
        bool       isHittedChip(uint8_t hybrid_id, uint8_t chip_id, size_t& chipIndx) const;
        static int lane2chipId(const Ph2_HwDescription::BeBoard* pBoard, uint16_t optGroup_id, uint16_t hybrid_id, uint16_t chip_lane);
    };

    static uint16_t DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events);
    static void     DecodeEventsWrapper(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events, const std::vector<size_t>& eventStart, std::atomic<uint16_t>& evtStatus);
    static uint16_t DecodeEvents(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events, const std::vector<size_t>& eventStart);
    static bool     EvtErrorHandler(uint16_t status);
    static void     PrintEvents(const std::vector<RD53FWInterface::Event>& events, const std::vector<uint32_t>& pData = {});

    enum class TriggerSource : uint32_t
    {
        IPBus = 1,
        FastCMDFSM,
        TTC,
        TLU,
        External,
        HitOr,
        UserDefined,
        Undefined = 0
    };

    // @TMP@
    enum class AutozeroSource : uint32_t
    {
        IPBus = 1,
        FastCMDFSM,
        UserDefined, // --> It needs to set IPbus register "autozero_freq"
        Disabled = 0
    };

    struct FastCmdFSMConfig
    {
        bool ecr_en        = false;
        bool first_cal_en  = false;
        bool second_cal_en = false;
        bool trigger_en    = false;

        uint32_t first_cal_data  = 0;
        uint32_t second_cal_data = 0;

        uint32_t delay_after_first_prime = 0;
        uint32_t delay_after_ecr         = 0;
        uint32_t delay_after_autozero    = 0; // @TMP@
        uint32_t delay_after_inject      = 0;
        uint32_t delay_after_trigger     = 0;
        uint32_t delay_after_prime       = 0;
    };

    struct FastCommandsConfig
    {
        TriggerSource  trigger_source  = TriggerSource::FastCMDFSM;
        AutozeroSource autozero_source = AutozeroSource::Disabled; // @TMP@

        bool initial_ecr_en  = false;
        bool backpressure_en = false;
        bool veto_en         = false;

        uint32_t n_triggers        = 0;
        uint32_t ext_trigger_delay = 0; // Used when trigger_source == TriggerSource::External
        uint32_t trigger_duration  = 0; // Number of triggers on top of the L1A (maximum value is 31)
        uint32_t enable_hitor      = 0; // Enable HitOr signals

        FastCmdFSMConfig fast_cmd_fsm;
    };

    void ConfigureFromXML(const Ph2_HwDescription::BeBoard* pBoard);
    void SetAndConfigureFastCommands(const Ph2_HwDescription::BeBoard* pBoard,
                                     const uint32_t                    nTRIGxEvent,
                                     const size_t                      injType,
                                     const uint32_t                    injLatency     = 0,
                                     const uint32_t                    nClkDelays     = 0,
                                     const bool                        enableAutozero = false);

    struct DIO5Config
    {
        bool     enable             = false;
        bool     ext_clk_en         = false;
        uint32_t ch_out_en          = 0; // chn-1 = TLU clk input, chn-2 = ext. trigger, chn-3 = TLU busy, chn-4 = TLU reset, chn-5 = ext. clk
        uint32_t fiftyohm_en        = 0;
        uint32_t ch1_thr            = 0x80; // [(thr/256*(5-1)V + 1V) * 3.3V/5V]
        uint32_t ch2_thr            = 0x80;
        uint32_t ch3_thr            = 0x80;
        uint32_t ch4_thr            = 0x80;
        uint32_t ch5_thr            = 0x80;
        bool     tlu_en             = false;
        uint32_t tlu_handshake_mode = 0; // 0 = no handshake, 1 = simple handshake, 2 = data handshake
    };

    FastCommandsConfig* getLocalCfgFastCmd() { return &localCfgFastCmd; }

    // ############################
    // # Read/Write Optical Group #
    // ############################
    void     StatusOptoLink(Ph2_HwDescription::Chip* pChip, uint32_t& isReady, uint32_t& isFIFOempty) override;
    void     ResetOptoLink(Ph2_HwDescription::Chip* pChip) override;
    bool     WriteOptoLinkRegister(Ph2_HwDescription::Chip* pChip, uint32_t pAddress, uint32_t pData, bool pVerifLoop = false) override;
    uint32_t ReadOptoLinkRegister(Ph2_HwDescription::Chip* pChip, uint32_t pAddress) override;
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB() {}
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector) override {}
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords) override { return {0}; }
    // function to read/write lpGBT registers
    bool    WriteLpGBTRegister(uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) override { return true; }
    uint8_t ReadLpGBTRegister(uint16_t pRegisterValue) override { return 0; }

    // ###########################################
    // # Member functions to handle the firmware #
    // ###########################################
    void                     FlashProm(const std::string& strConfig, const char* pstrFile);
    void                     JumpToFpgaConfig(const std::string& strConfig);
    void                     DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest);
    std::vector<std::string> getFpgaConfigList();
    void                     DeleteFpgaConfig(const std::string& strId);
    void                     CheckIfUploading();
    void                     RebootBoard();
    const FpgaConfig*        GetConfiguringFpga();

    // ########################################
    // # Vector containing the decoded events #
    // ########################################
    static std::vector<RD53FWInterface::Event> decodedEvents;

    // ################################################
    // # I2C block for programming peripheral devices #
    // ################################################
    bool I2cCmdAckWait(unsigned int trials);
    void WriteI2C(std::vector<uint32_t>& data);
    void ReadI2C(std::vector<uint32_t>& data);
    void ConfigureClockSi5324();

    // ####################################################
    // # Hybrid ADC measurements: temperature and voltage #
    // ####################################################
    float ReadHybridTemperature(int hybridId);
    float ReadHybridVoltage(int hybridId);
    float calcTemperature(uint32_t sensor1, uint32_t sensor2, int beta = 3435);
    float calcVoltage(uint32_t senseVDD, uint32_t senseGND);

    // ##############################
    // # Pseudo Random Bit Sequence #
    // ##############################
    bool RunPRBStest(bool given_time, unsigned long long frames_or_time, uint16_t hybrid_id, uint16_t chip_id);

  private:
    void                  PrintFWstatus();
    void                  TurnOffFMC();
    void                  TurnOnFMC();
    void                  ResetBoard();
    void                  ResetFastCmdBlk();
    void                  ResetSlowCmdBlk();
    void                  ResetReadoutBlk();
    void                  ConfigureFastCommands(const FastCommandsConfig* config = nullptr);
    void                  ConfigureDIO5(const DIO5Config* config);
    void                  SendBoardCommand(const std::string& cmd_reg);
    bool                  CheckChipCommunication();
    void                  InitHybridByHybrid(const Ph2_HwDescription::BeBoard* pBoard);
    std::vector<uint16_t> GetInitSequence(const unsigned int type);
    uint32_t              GetHybridEnabledChips(const Ph2_HwDescription::Hybrid* pHybrid);

    // ###################
    // # Clock generator #
    // ###################
    void InitializeClockGenerator(bool doStoreInEEPROM = false);
    void ReadClockGenerator();

    FastCommandsConfig localCfgFastCmd;
    D19cFpgaConfig*    fpgaConfig;
    size_t             ddr3Offset;
    bool               singleChip;
    unsigned int       auroraSpeed;
    uint16_t           enabledHybrids;
};

} // namespace Ph2_HwInterface

#endif
