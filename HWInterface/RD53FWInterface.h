/*!
  \file                  RD53FWInterface.h
  \brief                 RD53FWInterface to initialize and configure the FW
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53FWInterface_H
#define RD53FWInterface_H

#include "../HWDescription/RD53.h"
#include "../Utils/RD53Event.h"
#include "../Utils/RD53RunProgress.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/easylogging++.h"
#include "BeBoardFWInterface.h"
#include "D19cFpgaConfig.h"
#include "RD53lpGBTInterface.h"

#include <uhal/uhal.hpp>

// #######################
// # FW useful constants #
// #######################
namespace RD53FWconstants
{
const uint8_t MAXATTEMPTS        = 20;   // Maximum number of attempts
const uint8_t READOUTSLEEP       = 50;   // [microseconds]
const uint8_t NLANE_HYBRID       = 4;    // Number of lanes per hybrid
const uint8_t HEADEAR_WRTCMD     = 0xFF; // Header of chip write command sequence
const uint8_t NBIT_FWVER         = 16;   // Number of bits for the firmware version
const uint8_t IPBUS_FASTDURATION = 1;    // Duration of a fast command in terms of 40 MHz clk cycles
} // namespace RD53FWconstants

namespace Ph2_HwInterface
{
class RD53FWInterface : public BeBoardFWInterface
{
  public:
    RD53FWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    ~RD53FWInterface() { delete fFileHandler; }

    // #############################
    // # Override member functions #
    // #############################
    void      setFileHandler(FileHandler* pHandler) override;
    uint32_t  getBoardInfo() override;
    BoardType getBoardType() const override { return BoardType::RD53; }

    void ResetSequence(const std::string& refClockRate);
    void ConfigureBoard(const Ph2_HwDescription::BeBoard* pBoard) override;

    void Start() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    bool     RunBERtest(bool given_time, double frames_or_time, uint16_t optGroup_id, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed) override;
    void     ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait = true) override;
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait = true) override;
    void     ChipReset() override;
    void     ChipReSync() override;
    // #############################

    // ####################################
    // # Check AURORA lock on data stream #
    // ####################################
    bool     CheckChipCommunication(const Ph2_HwDescription::BeBoard* pBoard);
    uint32_t ReadoutSpeed();
    bool     DidIwriteChipReg(uint16_t optGroup_id) // @TMP@
    {
        RegManager::WriteReg("user.ctrl_regs.PRBS_checker.upgroup_addr", optGroup_id);

        RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, 0x44, RD53Constants::PATTERN_CLOCK);
        usleep(1000);
        uint32_t readPattern = RegManager::ReadReg("user.stat_regs.rate_measurement_bx_counter");
        std::cout << "AAAAA " << std::hex << readPattern << std::dec << std::endl;
        if(readPattern == 0x5555) return true;
        return false;
    }

    // #############################################
    // # hybridId < 0 --> broadcast to all hybrids #
    // #############################################
    void                                       WriteChipCommand(const std::vector<uint16_t>& data, int hybridId);
    void                                       ComposeAndPackChipCommands(const std::vector<uint16_t>& data, int hybridId, std::vector<uint32_t>& commandList);
    void                                       SendChipCommandsPack(const std::vector<uint32_t>& commandList);
    std::vector<std::pair<uint16_t, uint16_t>> ReadChipRegisters(Ph2_HwDescription::ReadoutChip* pChip);

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

    // ###################################
    // # Read/Write Status Optical Group #
    // ###################################
    void     ResetOptoLinkSlowControl();
    void     StatusOptoLinkSlowControl(uint32_t& txIsReady, uint32_t& rxIsReady);
    void     ResetOptoLink() override;
    void     StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus) override;
    bool     WriteOptoLinkRegister(uint32_t pAddress, uint32_t pData, bool pVerifLoop = false) override;
    uint32_t ReadOptoLinkRegister(uint32_t pAddress) override;

    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB() {}
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector, bool pVerbose = false) override {}
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords, bool pVerbose = false) override { return {0}; }
    // function to read/write lpGBT registers
    bool    WriteLpGBTRegister(uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) override { return true; }
    uint8_t ReadLpGBTRegister(uint16_t pRegisterValue) override { return 0; }
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite(uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes) override { return true; };
    uint8_t I2CRead(uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes) override { return 0; };
    // function for front-end slow control
    bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = true) override { return true; };
    uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress) override { return 0; };

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

    // ################################################
    // # I2C block for programming peripheral devices #
    // ################################################
    bool I2cCmdAckWait(int nAttempts);
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
    void                  InitHybridByHybrid(const Ph2_HwDescription::BeBoard* pBoard);
    std::vector<uint16_t> GetInitSequence(const unsigned int type);
    uint32_t              GetHybridEnabledChips(const Ph2_HwDescription::Hybrid* pHybrid);

    // ###################
    // # Clock generator #
    // ###################
    void InitializeClockGenerator(const std::string& refClockRate = "160", bool doStoreInEEPROM = false);
    void ReadClockGenerator();

    FastCommandsConfig localCfgFastCmd;
    D19cFpgaConfig*    fpgaConfig;
    size_t             ddr3Offset;
    bool               singleChip;
    uint16_t           enabledHybrids;
};

} // namespace Ph2_HwInterface

#endif
