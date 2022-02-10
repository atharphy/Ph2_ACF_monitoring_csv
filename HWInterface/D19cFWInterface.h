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
// #include "FEConfigurationInterface.h"

#include <limits.h>
#include <map>
#include <mutex>
#include <numeric>
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
  class L1ReadoutInterface;   
  class FEConfigurationInterface;

/*!
 * \class Cbc3Fc7FWInterface
 *
 * \brief init/config of the Fc7 and its Chip's
 */
class D19cFWInterface : public BeBoardFWInterface
{
  private:
    // std::recursive_mutex                     fMutex;
    FEConfigurationInterface*                 fFEConfigurationInterface; 
    L1ReadoutInterface*                       fL1ReadoutInterface; 

    D19cFWEvtEncoder::D19cFWEvt              fD19cFWEvts;
    std::vector<std::vector<uint32_t>>       fSlaveMap;
    std::map<uint8_t, std::vector<uint32_t>> fI2CSlaveMap;
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
    bool                       fConfigureCDCE  = false;
    std::map<uint8_t, uint8_t> fRxPolarity;
    std::map<uint8_t, uint8_t> fTxPolarity;
    // 2S or PS readout
    bool           fIs2S           = true;
    const uint32_t SINGLE_I2C_WAIT = 200; // used for 1MHz I2C
    // I'm going to add a variable to hold the stub offset
    uint32_t fStubOffset = 0xFFFF;
    // event counter
    uint32_t fEventCounter = 0;

    // some useful stuff
    int fResetAttempts;

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

    void     ResetEventCounter() { fEventCounter = 0; }
    uint32_t GetEventCounter() { return fEventCounter; }
    /*!
     * \brief Status of triggers
     */
    uint32_t GetTriggerState();
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

    // send N triggers
    void SendNTriggers(uint16_t pNtriggers) override;

    void ResetTriggerFSM();
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
     * \brief Read data from DAQ
     * \param pBreakTrigger : if true, enable the break trigger
     * \return fNpackets: the number of packets read
     */
    uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait = true) override;

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
    // set stub offset
    void     SetStubOffset(uint32_t pOffset) { fStubOffset = pOffset; };
    uint32_t getStubOffset() { return fStubOffset; };
    uint8_t  getI2Cstatus() { return fI2Cstatus; }

  private:
    uint8_t  fFastCommandDuration = 0;
    uint32_t fReadoutAttempts     = 0;
    uint16_t fWait_us             = 10000; // 10 ms
    uint8_t  fResetMinPeriod_ms   = 100;   // was 100

    // get data from FC7

    // wait for events from FC7
    bool WaitForData(Ph2_HwDescription::BeBoard* pBoard);
    // split data per hybrid/chip for a given board
    uint32_t CountFwEvents(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
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
    void EnableFrontEnds(const Ph2_HwDescription::BeBoard* pBoard);

  public:
    void ReconfigureTriggerFSM(std::vector<std::pair<std::string, uint32_t>> pTriggerConfig);
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
    // for testing, move back
    uint32_t GetData(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void     EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, Ph2_HwDescription::Chip* pChip, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override;
    void     BCEncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pNCbc, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite) override;
    void     DecodeReg(Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t& pCbcId, uint32_t pWord, bool& pRead, bool& pFailed) override;

    bool WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback) override;
    bool BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback) override;
    void ReadChipBlockReg(std::vector<uint32_t>& pVecReg);

    void ChipReSync() override;

    void ChipReset() override;

    void ChipI2CRefresh();

    void ChipTestPulse();

    void ChipTrigger();
    void Trigger(uint8_t pDuration = 1);

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

    // Optical readout specific functions - d19c [temporary]
    void ResetLink(uint8_t pLinkId);
    bool GetLinkStatus(uint8_t pLinkId);

    bool LinkLock(const Ph2_HwDescription::BeBoard* pBoard);
    void setRxPolarity(uint8_t pLinkId, uint8_t pPolarity = 1) { fRxPolarity.insert({pLinkId, pPolarity}); };
    void setTxPolarity(uint8_t pLinkId, uint8_t pPolarity = 1) { fTxPolarity.insert({pLinkId, pPolarity}); };

    // CDCE
    void configureCDCE_old(uint16_t pClockRate = 120);
    void configureCDCE(uint16_t pClockRate = 120, std::pair<std::string, float> pCDCEselect = std::make_pair("sec", 40));
    void syncCDCE();
    void epromCDCE();

    // measures the occupancy of the 2S chips
    bool Measure2SOccupancy(uint32_t pNEvents, uint8_t**& pErrorCounters, uint8_t***& pChannelCounters);
    void Manage2SCountersMemory(uint8_t**& pErrorCounters, uint8_t***& pChannelCounters, bool pAllocate);

    void Compose_fast_command(uint32_t duration = 0, uint32_t resync_en = 0, uint32_t l1a_en = 0, uint32_t cal_pulse_en = 0, uint32_t bc0_en = 0);
    void SetForceStart(bool bStart) {}

    ///////////////////////////////////////////////////////
    //      Optical readout                                 //
    /////////////////////////////////////////////////////
    void selectLink(const uint8_t pLinkId = 0, uint32_t cWait_ms = 100) override;

    // ##############################
    // # Pseudo Random Bit Sequence #
    // ##############################
    double RunBERtest(bool given_time, double frames_or_time, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed) override { return 0; };

    // ############################
    // # Read/Write Optical Group #
    // ############################
    uint8_t       fI2Cstatus    = 0;
    const uint8_t flpGBTAddress = 0x70;

    // OT implementation of write and read
    bool     WriteOptoLpGBTRegister(const uint32_t linkNumber, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop);
    uint32_t ReadOptoLpGBTRegister(const uint32_t linkNumber, const uint32_t pAddress);

    // Functions for standard uDTC
    void     StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus) override {}
    void     ResetOptoLink() override;
    bool     WriteOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop = false) override;
    uint32_t ReadOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress) override;
    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    void                  ResetCPB() override;
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector) override;
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords) override;
    std::vector<uint32_t> WriteCommandCPBandReadReply(const std::vector<uint32_t>& pCommandVector, uint8_t pNWords);

    // function to read/write lpGBT registers
    bool    WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) override;
    uint8_t ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterValue) override;
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, uint8_t pFrequency, uint32_t& pNWrites) override;
    uint8_t I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFrequency, uint32_t& pNReads) override;
    // function for front-end slow control
    bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false) override;
    bool    localWriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry, uint32_t& theI2CWriteCount, uint32_t& theI2CReadMismatches);
    uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress) override;
    
    // Generic FE configuration functions
    // single register functions 
    // Register write
    bool    SingleRegisterWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify = true) override;
    bool    MultiRegisterWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem , bool pVerify = true) override;
    // Register write + read-back 
    bool    MultiRegisterWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem) override;
    bool    SingleRegisterWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem)override;
    // Registe read 
    uint8_t SingleRegisterRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem ) override;
    std::vector<uint8_t> MultiRegisterRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem ) override;
   
    // fast command generic block
    void ResetFCMDBram();
    void ConfigureFCMDBram(std::vector<uint8_t> pFastCommands);
};
} // namespace Ph2_HwInterface

#endif
