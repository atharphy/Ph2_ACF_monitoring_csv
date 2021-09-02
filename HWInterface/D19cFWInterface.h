/*!

\file                           D19cFWInterface.h
\brief                          D19cFWInterface init/config of the FC7 and its Chip's
\author                         G. Auzinger, K. Uchida, M. Haranko
        \version            1.0
        \date                           24.03.2017
        Support :                       mail to : georg.auzinger@SPAMNOT.cern.ch
                                                  mykyta.haranko@SPAMNOT.cern.ch

*/

#ifndef _D19CFWINTERFACE_H__
#define _D19CFWINTERFACE_H__

#include "../Utils/DataContainer.h"
#include "../Utils/Event.h"
#include "../Utils/easylogging++.h"
#include "BeBoardFWInterface.h"
#include <limits.h>
#include <map>
#include <stdint.h>
#include <string>
#include <vector>
//#include "../Utils/OccupancyAndPh.h"
//#include "../Utils/GenericDataVector.h"
#include <uhal/uhal.hpp>

namespace D19cFWEvtEncoder
{
// ################
// # Event header #
// ################
const uint16_t EVT_HEADER = 0xFFFF;

const uint16_t IWORD_L1_HEADER = 4;
const uint16_t SBIT_L1_HEADER  = 28;
const uint16_t SBIT_L1_STATUS  = 24;
const uint16_t SBIT_HYBRID_ID  = 16;
const uint16_t SBIT_CHIP_ID    = 12;

// ################
// # Event status #
// ################
const uint16_t GOOD           = 0x0000; // Event status Good
const uint16_t EMPTY          = 0x0002; // Event status Empty event
const uint16_t BADHEADER      = 0x0004; // Bad header
const uint8_t  GOODL1HEADER   = 0x0A;
const uint8_t  GOODStubHEADER = 0x05;
const uint16_t BADL1HEADER    = 0x0006; // Bad L1 header
const uint16_t BADSTUBHEADER  = 0x0008; // Bad Stub header
/*const uint16_t INCOMPLETE = 0x0004; // Event status Incomplete event header
const uint16_t L1A        = 0x0008; // Event status L1A counter mismatch
const uint16_t FWERR      = 0x0010; // Event status Firmware error
const uint16_t FRSIZE     = 0x0020; // Event status Invalid frame size
const uint16_t MISSCHIP   = 0x0040; // Event status Chip data are missing*/
const uint16_t NODECODER = 0xFFFF; // Event decoding not implemented

const uint16_t CLUSTER_2S   = 14;
const uint16_t SCLUSTER_PS  = 14;
const uint16_t PCLUSTER_PS  = 17;
const uint16_t SCLUSTER_MPA = 0;
const uint16_t PCLUSTER_MPA = 0;
const uint16_t HITS_2S      = 274;
const uint16_t HITS_SSA     = 120;
const uint16_t HITS_CBC     = 254;

using RawFeData    = std::vector<uint32_t>;
using RawBoardData = std::vector<RawFeData>;

// ################
// # Event status #
// ################
struct D19cFWEvt
{
    std::vector<uint32_t> fEventStatus;
    RawBoardData          fBoardHitData;
    RawBoardData          fBoardStubData;
};
} // namespace D19cFWEvtEncoder

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
class D19cFpgaConfig;
class D19cSSAEvent;
/*!
 * \class Cbc3Fc7FWInterface
 *
 * \brief init/config of the Fc7 and its Chip's
 */
class D19cFWInterface : public BeBoardFWInterface
{
  private:
    D19cFWEvtEncoder::D19cFWEvt              fD19cFWEvts;
    std::vector<std::vector<uint32_t>>       fSlaveMap;
    std::map<uint8_t, std::vector<uint32_t>> fI2CSlaveMap;
    D19cFpgaConfig*                          fpgaConfig;
    FileHandler*                             fFileHandler;
    uint32_t                                 fBroadcastCbcId;
    uint32_t                                 fNReadoutChip;
    uint32_t                                 fNHybrids;
    uint32_t                                 fNCic;
    uint32_t                                 fFMCId;

    // number of chips and hybrids defined in firmware (compiled for)
    uint32_t     fFWNHybrids;
    uint32_t     fFWNChips;
    FrontEndType fFirmwareFrontEndType;
    bool         fCBC3Emulator;
    bool         fIsDDR3Readout;
    bool         fDDR3Calibrated;
    uint32_t     fDDR3Offset;
    // i2c version of master
    uint32_t fI2CVersion;
    // optical readout
    bool                       fOptical        = false;
    bool                       fUseOpticalLink = false;
    bool                       fUseCPB         = false;
    bool                       fConfigureCDCE  = false;
    std::map<uint8_t, uint8_t> fRxPolarity;
    std::map<uint8_t, uint8_t> fTxPolarity;

    uint32_t fGBTphase;

    const uint32_t SINGLE_I2C_WAIT = 200; // used for 1MHz I2C

    // some useful stuff
    int  fResetAttempts;
    void Align_out();

  public:
    /*!
     *
     * \brief Constructor of the Cbc3Fc7FWInterface class
     * \param puHalConfigFileName : path of the uHal Config File
     * \param pBoardId
     */

    D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    D19cFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler);
    /*!
     *
     * \brief Constructor of the Cbc3Fc7FWInterface class
     * \param pId : ID string
     * \param pUri: URI string
     * \param pAddressTable: address tabel string
     */

    D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    D19cFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler);
    void setFileHandler(FileHandler* pHandler);

    /*!
     *
     * \brief Destructor of the Cbc3Fc7FWInterface class
     */

    ~D19cFWInterface()
    {
        if(fFileHandler) delete fFileHandler;
    }

    ///////////////////////////////////////////////////////
    //      d19c Methods                                //
    /////////////////////////////////////////////////////

    // uint16_t ParseEvents(const std::vector<uint32_t>& pData) override;
    /*! \brief Read a block of a given size
     * \param pRegNode Param Node name
     * \param pBlocksize Number of 32-bit words to read
     * \return Vector of validated 32-bit values
     */
    std::vector<uint32_t> ReadBlockRegValue(const std::string& pRegNode, const uint32_t& pBlocksize) override;

    /*! \brief Read a block of a given size
     * \param pRegNode Param Node name
     * \param pBlocksize Number of 32-bit words to read
     * \param pBlockOffset Offset of the block
     * \return Vector of validated 32-bit values
     */
    std::vector<uint32_t> ReadBlockRegOffsetValue(const std::string& pRegNode, const uint32_t& pBlocksize, const uint32_t& pBlockOffset);

    bool WriteBlockReg(const std::string& pRegNode, const std::vector<uint32_t>& pValues) override;
    /*!
     * \brief Get the FW info
     */
    uint32_t getBoardInfo();

    BoardType getBoardType() const { return BoardType::D19C; }
    /*!
     * \brief Configure the board with its Config File
     * \param pBoard
     */
    void ConfigureBoard(const Ph2_HwDescription::BeBoard* pBoard) override;
    /*!
     * \brief Detect the right FE Id to write the right registers (not working with the latest Firmware)
     */
    void SelectFEId();
    /*!
     * \brief Start a DAQ
     */
    void Start() override;
    /*!
     * \brief Stop a DAQ
     */
    void Stop() override;
    /*!
     * \brief Pause a DAQ
     */
    void Pause() override;
    /*!
     * \brief Unpause a DAQ
     */
    void Resume() override;

    /*!
     * \brief Reset Readout
     */
    void ResetReadout();

    // print trigger config
    void TriggerConfiguration();

    /*!
     * \brief DDR3 Self-test
     */
    void DDR3SelfTest();

    /*!
     * \brief Tune the 320MHz buses phase shift
     */
    bool PhaseTuning(Ph2_HwDescription::BeBoard* pBoard, uint8_t pFeId, uint8_t pChipId, uint8_t pLineId, uint16_t pPattern, uint16_t pPatternPeriod);

    /*!
     * \brief Read data from DAQ
     * \param pBreakTrigger : if true, enable the break trigger
     * \return fNpackets: the number of packets read
     */
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait = true) override;

    void ReadASEvent(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);

    /*!
     * \brief Read data for pNEvents
     * \param pBoard : the pointer to the BeBoard
     * \param pNEvents :  the 1 indexed number of Events to read - this will set the packet size to this value -1
     */

    void ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait = true);
    // FMCs
    void InitFMCPower();
    // vector of 32 bit words for ROC#pIndex [hits]
    std::vector<uint32_t> GetHitData(uint8_t pIndex) { return fD19cFWEvts.fBoardHitData[pIndex]; }
    // vector of 32 bit words for ROC#pIndex [stubs]
    std::vector<uint32_t> GetStubData(uint8_t pIndex) { return fD19cFWEvts.fBoardStubData[pIndex]; }

  private:
    uint8_t  fFastCommandDuration = 0;
    uint16_t fWait_us             = 10000; // 10 ms
    uint8_t  fResetMinPeriod_ms   = 100;   // was 100
    // get data from FC7
    uint32_t GetData(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    // wait for events from FC7
    bool WaitForData(Ph2_HwDescription::BeBoard* pBoard);
    // split data per hybrid/chip for a given board
    uint32_t CountFwEvents(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    // read back SSA counters directly
    void ReadSSACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void ReadMPACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData, bool cFast);

    uint32_t computeEventSize(Ph2_HwDescription::BeBoard* pBoard);
    // I2C command sending implementation
    bool WriteI2C(std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pWriteRead, bool pBroadcast);
    bool ReadI2C(uint32_t pNReplies, std::vector<uint32_t>& pReplies);

    // binary predicate for comparing sent I2C commands with replies using std::mismatch
    static bool cmd_reply_comp(const uint32_t& cWord1, const uint32_t& cWord2);
    static bool cmd_reply_ack(const uint32_t& cWord1, const uint32_t& cWord2);

    // ########################################
    // # FMC powering/control/configuration  #
    // ########################################
    void powerAllFMCs(bool pEnable = false);
    // dedicated method to power on dio5
    void PowerOnDIO5(uint8_t pFMCId);
    // get fmc card name
    std::string getFMCCardName(uint32_t id);
    // convert code of the chip from firmware
    std::string  getChipName(uint32_t pChipCode);
    FrontEndType getFrontEndType(uint32_t pChipCode);

    // FMC Maps
    std::map<uint32_t, std::string> fFMCMap = {{0, "NONE"},
                                               {1, "DIO5"},
                                               {2, "2CBC2"},
                                               {3, "8CBC2"},
                                               {4, "2CBC3"},
                                               {5, "8CBC3_1"},
                                               {6, "8CBC3_2"},
                                               {7, "1CBC3"},
                                               {8, "MPA_SSA"},
                                               {9, "FERMI_TRIGGER"},
                                               {10, "CIC1_FMC1"},
                                               {11, "CIC1_FMC2"},
                                               {12, "PS_FMC1"},
                                               {13, "PS_FMC2"},
                                               {14, "2S_FMC1"},
                                               {15, "2S_FMC2"},
                                               {16, "2S"},
                                               {17, "OPTO_QUAD"},
                                               {18, "OPTO_OCTA"},
                                               {19, "FMC_FE_FOR_PS_ROH_FMC1"},
                                               {20, "FMC_FE_FOR_PS_ROH_FMC2"}};

    std::map<uint32_t, std::string> fChipNamesMap = {{0, "CBC2"}, {1, "CBC3"}, {2, "MPA"}, {3, "SSA"}, {4, "CIC"}, {5, "CIC2"}};

    std::map<uint32_t, FrontEndType> fFETypesMap = {{1, FrontEndType::CBC3}, {2, FrontEndType::MPA}, {3, FrontEndType::SSA}, {4, FrontEndType::CIC}, {5, FrontEndType::CIC2}};

    // template to copy every nth element out of a vector to another vector
    template <class in_it, class out_it>
    out_it copy_every_n(in_it b, in_it e, out_it r, size_t n)
    {
        for(size_t i = std::distance(b, e) / n; i--; std::advance(b, n)) *r++ = *b;

        return r;
    }

    // method to split a vector in vectors that contain elements from even and odd indices
    void splitVectorEvenOdd(std::vector<uint32_t> pInputVector, std::vector<uint32_t>& pEvenVector, std::vector<uint32_t>& pOddVector)
    {
        bool ctoggle = false;
        std::partition_copy(pInputVector.begin(), pInputVector.end(), std::back_inserter(pEvenVector), std::back_inserter(pOddVector), [&ctoggle](int) { return ctoggle = !ctoggle; });
    }

    void getOddElements(std::vector<uint32_t> pInputVector, std::vector<uint32_t>& pOddVector)
    {
        bool ctoggle = true;
        std::copy_if(pInputVector.begin(), pInputVector.end(), std::back_inserter(pOddVector), [&ctoggle](int) { return ctoggle = !ctoggle; });
    }

    void ReadErrors();
    void ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig);

  public:
    ///////////////////////////////////////////////////////
    //      CBC Methods                                 //
    /////////////////////////////////////////////////////

    // Encode/Decode Chip values
    /*!
     * \brief Encode a/several word(s) readable for a Chip
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to write
     * \param pCbcId : Id of the Chip to work with
     * \param pVecReq : Vector to stack the encoded words
     */
    void
         EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pCbcId, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override; /*!< Encode a/several word(s) readable for a Chip*/
    void EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pFeId, uint8_t pCbcId, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
        override; /*!< Encode a/several word(s) readable for a Chip*/

    void BCEncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pNCbc, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override;
    void DecodeReg(Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t& pCbcId, uint32_t pWord, bool& pRead, bool& pFailed) override;

    bool WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback) override;
    bool BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback) override;
    void ReadChipBlockReg(std::vector<uint32_t>& pVecReg);

    void ChipReSync() override;

    void ChipReset() override;

    void ChipI2CRefresh();

    void ChipTestPulse();

    void ChipTrigger();
    void Trigger(uint8_t pDuration = 1);
    // Readout chip specific stuff
    void Send_pulses(uint32_t pNtriggers);

    void ReadoutChipReset();
    // CIC BE stuff
    bool Bx0Alignment();
    // TP FSM
    void ConfigureTestPulseFSM(uint16_t pDelayAfterFastReset = 1,
                               uint16_t pDelayAfterTP        = 200,
                               uint16_t pDelayBeforeNextTP   = 400,
                               uint8_t  pEnableFastReset     = 1,
                               uint8_t  pEnableTP            = 1,
                               uint8_t  pEnableL1A           = 1);
    // trigger FSM
    void ConfigureTriggerFSM(uint16_t pNtriggers = 100, uint16_t pTriggerRate = 100, uint8_t pSource = 3, uint8_t pStubsMask = 0, uint8_t pStubLatency = 50);
    // consecutive triggers FSM
    void ConfigureConsecutiveTriggerFSM(uint16_t pNtriggers = 32, uint16_t pDelayBetweenTriggers = 1, uint16_t pDelayToNext = 1);
    // back-end tuning for CIC data
    void ConfigureFastCommandBlock(const Ph2_HwDescription::BeBoard* pBoard);
    // consecutive triggers FSM
    void ConfigureAntennaFSM(uint16_t pNtriggers = 1, uint16_t pTriggerRate = 1, uint16_t pL1Delay = 100);

    void L1ADebug(uint8_t pWait_ms = 1);
    void StubDebug(bool pWithTestPulse = true, uint8_t pNlines = 5);
    bool L1PhaseTuning(const Ph2_HwDescription::BeBoard* pBoard, bool pScope = false);
    bool L1WordAlignment(const Ph2_HwDescription::BeBoard* pBoard, bool pScope = false);
    bool L1Tuning(const Ph2_HwDescription::BeBoard* pBoard, bool pScope = false);
    bool StubTuning(const Ph2_HwDescription::BeBoard* pBoard, bool pScope = false);
    // bool BackEndTuning(const BeBoard* pBoard, bool pDoL1A=true);

    // Optical readout specific functions - d19c [temporary]
    void                       setGBTxPhase(uint32_t pPhase) { fGBTphase = pPhase; }
    void                       configureLink(const Ph2_HwDescription::BeBoard* pBoard);
    bool                       LinkLock(const Ph2_HwDescription::BeBoard* pBoard);
    bool                       GBTLock(const Ph2_HwDescription::BeBoard* pBoard);
    std::pair<uint16_t, float> readADC(std::string pValueToRead = "AMUX_L", bool pApplyCorrection = false);
    void                       setRxPolarity(uint8_t pLinkId, uint8_t pPolarity = 1) { fRxPolarity.insert({pLinkId, pPolarity}); };
    void                       setTxPolarity(uint8_t pLinkId, uint8_t pPolarity = 1) { fTxPolarity.insert({pLinkId, pPolarity}); };

    // CDCE
    void configureCDCE_old(uint16_t pClockRate = 120);
    void configureCDCE(uint16_t pClockRate = 120, std::pair<std::string, float> pCDCEselect = std::make_pair("sec", 40));
    void syncCDCE();
    void epromCDCE();

    // phase tuning commands - d19c
    struct PhaseTuner
    {
        uint8_t fWait_ms = 10;
        uint8_t fType;
        uint8_t fMode;
        uint8_t fDelay;
        uint8_t fBitslip;
        uint8_t fDone;
        uint8_t fWordAlignmentFSMstate;
        uint8_t fPhaseAlignmentFSMstate;
        uint8_t fFSMstate;

        void ParseResult(uint32_t pReply)
        {
            fType = (pReply >> 24) & 0xF;
            if(fType == 0)
            {
                fMode    = (pReply & 0x00003000) >> 12;
                fDelay   = (pReply & 0x000000F8) >> 3;
                fBitslip = (pReply & 0x00000007) >> 0;
            }
            else if(fType == 1)
            {
                fDelay                  = (pReply & 0x00F80000) >> 19;
                fBitslip                = (pReply & 0x00070000) >> 16;
                fDone                   = (pReply & 0x00008000) >> 15;
                fWordAlignmentFSMstate  = (pReply & 0x00000F00) >> 8;
                fPhaseAlignmentFSMstate = (pReply & 0x0000000F) >> 0;
            }
            else if(fType == 6)
            {
                fFSMstate = (pReply & 0x000000FF) >> 0;
            }
        };
        uint8_t ParseStatus(BeBoardFWInterface* pInterface)
        {
            uint8_t cStatus = 0;
            // read status
            uint32_t cReply = pInterface->ReadReg("fc7_daq_stat.physical_interface_block.phase_tuning_reply");
            ParseResult(cReply);

            if(fType == 0)
            {
                LOG(INFO) << "\t\t Mode: " << +fMode;
                LOG(INFO) << "\t\t Manual Delay: " << +fDelay << ", Manual Bitslip: " << +fBitslip;
                cStatus = 1;
            }
            else if(fType == 1)
            {
                LOG(INFO) << "\t\t Done: " << +fDone << ", PA FSM: " << BOLDGREEN << fPhaseFSMStateMap[fPhaseAlignmentFSMstate] << RESET << ", WA FSM: " << BOLDGREEN
                          << fWordFSMStateMap[fWordAlignmentFSMstate] << RESET;
                LOG(INFO) << "\t\t Delay: " << +fDelay << ", Bitslip: " << +fBitslip;
                cStatus = 1;
            }
            else if(fType == 6)
            {
                LOG(INFO) << "\t\t Default FSM State: " << +fFSMstate;
                cStatus = 1;
            }
            else
                cStatus = 0;
            return cStatus;
        };
        uint32_t fHybrid;
        uint32_t fChip;
        uint32_t fLine;
        void     ConfigureInput(uint8_t pHybrid, uint8_t pChip, uint8_t pLine)
        {
            fHybrid = (pHybrid & 0xF) << 28;
            fChip   = (pChip & 0xF) << 24;
            fLine   = (pLine & 0xF) << 20;
        };
        uint32_t fCommand;
        void     ConfigureCommandType(uint8_t pType) { fCommand = (pType & 0xF) << 16; };
        void     SetLineMode(BeBoardFWInterface* pInterface,
                             uint8_t             pHybrid,
                             uint8_t             pChip,
                             uint8_t             pLine,
                             uint8_t             pMode       = 0,
                             uint8_t             pDelay      = 0,
                             uint8_t             pBitSlip    = 0,
                             uint8_t             pEnableL1   = 0,
                             uint8_t             pMasterLine = 0)
        {
            // select FE
            ConfigureInput(pHybrid, pChip, pLine);
            // command
            uint32_t command_type = 2;
            ConfigureCommandType(command_type);
            // shift payload
            uint32_t mode_raw = (pMode & 0x3) << 12;
            // set defaults
            uint32_t l1a_en_raw         = (pMode == 0) ? ((pEnableL1 & 0x1) << 11) : 0;
            uint32_t master_line_id_raw = (pMode == 1) ? ((pMasterLine & 0xF) << 8) : 0;
            uint32_t delay_raw          = (pMode == 2) ? ((pDelay & 0x1F) << 3) : 0;
            uint32_t bitslip_raw        = (pMode == 2) ? ((pBitSlip & 0x7) << 0) : 0;
            // form command
            uint32_t command_final = fHybrid + fChip + fLine + fCommand + mode_raw + l1a_en_raw + master_line_id_raw + delay_raw + bitslip_raw;
            LOG(DEBUG) << BOLDBLUE << "Line " << +pLine << " setting line mode to " << std::hex << command_final << std::dec << RESET;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_ms * 1000));
        };
        void SetLinePattern(BeBoardFWInterface* pInterface, uint8_t pHybrid, uint8_t pChip, uint8_t pLine, uint16_t pPattern, uint16_t pPatternPeriod)
        {
            // select FE
            ConfigureInput(pHybrid, pChip, pLine);
            // set the pattern size
            uint8_t command_type = 3;
            ConfigureCommandType(command_type);
            uint32_t len_raw       = (0xFF & pPatternPeriod) << 0;
            uint32_t command_final = fHybrid + fChip + fLine + fCommand + len_raw;
            LOG(DEBUG) << BOLDBLUE << "Setting line pattern size to " << std::hex << command_final << std::dec << RESET;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            // set the pattern
            command_type = 4;
            ConfigureCommandType(command_type);
            uint8_t byte_id_raw = (0xFF & 0) << 8;
            uint8_t pattern_raw = (0xFF & pPattern) << 0;
            command_final       = fHybrid + fChip + fLine + fCommand + byte_id_raw + pattern_raw;
            LOG(DEBUG) << BOLDBLUE << "Setting line pattern  to " << std::hex << command_final << std::dec << RESET;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_ms * 1000));
        };
        void SendControl(BeBoardFWInterface* pInterface, uint8_t pHybrid, uint8_t pChip, uint8_t pLine, std::string pCommand)
        {
            // select FE
            ConfigureInput(pHybrid, pChip, pLine);
            // set the pattern size
            uint8_t command_type = 5;
            ConfigureCommandType(command_type);
            uint32_t command_final = fHybrid + fChip + fLine + fCommand;
            if(pCommand == "Apply")
                command_final += 4;
            else if(pCommand == "WordAlignment")
                command_final += 2;
            else if(pCommand == "PhaseAlignment")
                command_final += 1;
            LOG(DEBUG) << BOLDBLUE << pCommand << ": sending " << std::hex << command_final << std::dec << RESET;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_ms * 1000));
        };
        uint8_t GetLineStatus(BeBoardFWInterface* pInterface, uint8_t pHybrid, uint8_t pChip, uint8_t pLine)
        {
            // select FE
            ConfigureInput(pHybrid, pChip, pLine);
            // print header
            LOG(INFO) << BOLDBLUE << "\t Hybrid: " << RESET << +pHybrid << BOLDBLUE << ", Chip: " << RESET << +pChip << BOLDBLUE << ", Line: " << RESET << +pLine;
            uint8_t command_type = 0;
            ConfigureCommandType(command_type);
            uint32_t command_final = fHybrid + fChip + fLine + fCommand;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_ms * 1000));
            uint8_t cStatus = ParseStatus(pInterface);
            //
            command_type = 1;
            ConfigureCommandType(command_type);
            command_final = fHybrid + fChip + fLine + fCommand;
            pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_ms * 1000));
            cStatus = ParseStatus(pInterface);
            return cStatus;
        };
        bool TuneLine(BeBoardFWInterface* pInterface, uint8_t pHybrid, uint8_t pChip, uint8_t pLine, uint8_t pPattern, uint8_t pPatternPeriod, bool pChangePattern)
        {
            LOG(INFO) << BOLDBLUE << "Tuning line " << +pLine << RESET;
            if(pChangePattern)
            {
                SetLineMode(pInterface, pHybrid, pChip, pLine);
                SetLinePattern(pInterface, pHybrid, pChip, pLine, pPattern, pPatternPeriod);
            }
            // perform phase alignment
            // LOG (INFO) << BOLDBLUE << "\t..... running phase alignment...." << RESET;
            SendControl(pInterface, pHybrid, pChip, pLine, "PhaseAlignment");
            // perform word alignment
            // LOG (INFO) << BOLDBLUE << "\t..... running word alignment...." << RESET;
            SendControl(pInterface, pHybrid, pChip, pLine, "WordAlignment");
            uint8_t cLineStatus = GetLineStatus(pInterface, pHybrid, pChip, pLine);
            return (cLineStatus == 1);
        };

        // maps to decode status of word and phase alignment FSM
        std::map<int, std::string> fPhaseFSMStateMap = {{0, "IdlePHASE"},
                                                        {1, "ResetIDELAYE"},
                                                        {2, "WaitResetIDELAYE"},
                                                        {3, "ApplyInitialDelay"},
                                                        {4, "CheckInitialDelay"},
                                                        {5, "InitialSampling"},
                                                        {6, "ProcessInitialSampling"},
                                                        {7, "ApplyDelay"},
                                                        {8, "CheckDelay"},
                                                        {9, "Sampling"},
                                                        {10, "ProcessSampling"},
                                                        {11, "WaitGoodDelay"},
                                                        {12, "FailedInitial"},
                                                        {13, "FailedToApplyDelay"},
                                                        {14, "TunedPHASE"},
                                                        {15, "Unknown"}};
        std::map<int, std::string> fWordFSMStateMap  = {{0, "IdleWORD or WaitIserdese"},
                                                       {1, "WaitFrame"},
                                                       {2, "ApplyBitslip"},
                                                       {3, "WaitBitslip"},
                                                       {4, "PatternVerification"},
                                                       {5, "Not Defined"},
                                                       {6, "Not Defined"},
                                                       {7, "Not Defined"},
                                                       {8, "Not Defined"},
                                                       {9, "Not Defined"},
                                                       {10, "Not Defined"},
                                                       {11, "Not Defined"},
                                                       {12, "FailedFrame"},
                                                       {13, "FailedVerification"},
                                                       {14, "TunedWORD"},
                                                       {15, "Unknown"}};
    };

    // measures the occupancy of the 2S chips
    bool Measure2SOccupancy(uint32_t pNEvents, uint8_t**& pErrorCounters, uint8_t***& pChannelCounters);
    void Manage2SCountersMemory(uint8_t**& pErrorCounters, uint8_t***& pChannelCounters, bool pAllocate);

    ///////////////////////////////////////////////////////
    //      MPA/SSA Methods                             //
    /////////////////////////////////////////////////////

    // Coms
    void     PSInterfaceBoard_SetSlaveMap();
    void     PSInterfaceBoard_ConfigureI2CMaster(uint32_t pEnabled, uint32_t pFrequency);
    void     PSInterfaceBoard_SendI2CCommand(uint32_t slave_id, uint32_t board_id, uint32_t read, uint32_t register_address, uint32_t data);
    uint32_t PSInterfaceBoard_SendI2CCommand_READ(uint32_t slave_id, uint32_t board_id, uint32_t read, uint32_t register_address, uint32_t data);

    // Main Power:
    void PSInterfaceBoard_PowerOn(uint8_t mpaid = 0, uint8_t ssaid = 0);
    void PSInterfaceBoard_PowerOff();

    void PSInterfaceBoard_PowerOn_MPASSA(float VDDPST = 1.25, float DVDD = 1.2, float AVDD = 1.25, float VBG = 0.3, float VBF = 0.3, uint8_t mpaid = 0, uint8_t ssaid = 0);
    // MPA power on
    void PSInterfaceBoard_PowerOn_MPA(float VDDPST = 1.25, float DVDD = 1.2, float AVDD = 1.25, float VBG = 0.3, uint8_t mpaid = 0, uint8_t ssaid = 0);
    void PSInterfaceBoard_PowerOff_MPA(uint8_t mpaid = 0, uint8_t ssaid = 0);
    /// SSA power on
    void PSInterfaceBoard_PowerOn_SSA(float VDDPST = 1.25, float DVDD = 1.25, float AVDD = 1.25, float VBF = 0.3, float BG = 0.0, uint8_t ENABLE = 0);
    void PSInterfaceBoard_PowerOff_SSA(uint8_t mpaid = 0, uint8_t ssaid = 0);
    void ReadPower_SSA(uint8_t mpaid = 0, uint8_t ssaid = 0);
    void SSAEqualizeDACs(uint8_t pChipId);
    void KillI2C();
    ///

    void     Pix_write_MPA(Ph2_HwDescription::Chip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data);
    uint32_t Pix_read_MPA(Ph2_HwDescription::Chip* cMPA, Ph2_HwDescription::ChipRegItem cRegItem, uint32_t row, uint32_t pixel);

    void Compose_fast_command(uint32_t duration = 0, uint32_t resync_en = 0, uint32_t l1a_en = 0, uint32_t cal_pulse_en = 0, uint32_t bc0_en = 0);
    void PS_Open_shutter(uint32_t duration = 0);
    void PS_Close_shutter(uint32_t duration = 0);
    void PS_Clear_counters(uint32_t duration = 0);
    void PS_Start_counters_read(uint32_t duration = 0);

    ///////////////////////////////////////////////////////
    //      FPGA CONFIG                                 //
    /////////////////////////////////////////////////////

    void checkIfUploading();
    /*! \brief Upload a firmware (FPGA configuration) from a file in MCS format into a given configuration
     * \param strConfig FPGA configuration name
     * \param pstrFile path to MCS file
     */
    void FlashProm(const std::string& strConfig, const char* pstrFile);
    /*! \brief Jump to an FPGA configuration */
    void JumpToFpgaConfig(const std::string& strConfig);

    void DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest);
    /*! \brief Is the FPGA being configured ?
     * \return FPGA configuring process or NULL if configuration occurs */
    const FpgaConfig* GetConfiguringFpga() { return (const FpgaConfig*)fpgaConfig; }
    /*! \brief Get the list of available FPGA configuration (or firmware images)*/
    std::vector<std::string> getFpgaConfigList();
    /*! \brief Delete one Fpga configuration (or firmware image)*/
    void DeleteFpgaConfig(const std::string& strId);
    /*! \brief Reboot the board */
    void RebootBoard();
    /*! \brief Set or reset the start signal */
    void SetForceStart(bool bStart) {}

    ///////////////////////////////////////////////////////
    //      Optical readout                                 //
    /////////////////////////////////////////////////////
    void selectLink(const uint8_t pLinkId = 0, uint32_t cWait_ms = 100) override;

    ///////////////////////////////////////////////////////
    //      Multiplexing crate                          //
    /////////////////////////////////////////////////////
    /*!
     * \breif Disconnect Setup with Multiplexing Backplane
     */
    void DisconnectMultiplexingSetup(uint8_t pWait_ms = 100);
    void DisconnectMultiplexingSetup_old(uint8_t pWait_ms = 100);

    /*!
     * \breif Scan Setup with Multiplexing Backplane
     */
    uint32_t ScanMultiplexingSetup(uint8_t pWait_ms = 100);
    uint32_t ScanMultiplexingSetup_old(uint8_t pWait_ms = 100);

    /*!
     * \breif Configure Setup with Multiplexing Backplane
     * \param BackplaneNum
     * \param CardNum
     */
    void ConfigureMultiplexingSetup(int BackplaneNum, int CardNum, uint8_t pWait_ms = 100);

    // ##############################
    // # Pseudo Random Bit Sequence #
    // ##############################
    double RunBERtest(bool given_time, double frames_or_time, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed) override { return 0; };

    // ############################
    // # Read/Write Optical Group #
    // ############################
    const uint8_t                   fI2CFrequency = 3; // 1 MHz
    std::map<FrontEndType, uint8_t> fFEAddressMap = {{FrontEndType::CIC, 0x60}, {FrontEndType::CIC2, 0x60}, {FrontEndType::SSA, 0x20}, {FrontEndType::MPA, 0x40}};
    // Functions for standard uDTC
    void     StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus) override {}
    void     ResetOptoLink() override;
    bool     WriteOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop = false) override;
    uint32_t ReadOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress) override;
    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB() override;
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector, bool pVerbose = false) override;
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords, bool pVerbose = false) override;
    // function to read/write lpGBT registers
    bool    WriteLpGBTRegister(uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) override;
    uint8_t ReadLpGBTRegister(uint16_t pRegisterValue) override;
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite(uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes) override;
    uint8_t I2CRead(uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes) override;
    // function for front-end slow control
    bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false) override;
    uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress) override;
};
} // namespace Ph2_HwInterface

#endif
