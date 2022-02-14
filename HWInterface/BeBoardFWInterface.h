/*!
\file                            BeBoardFWInterface.h
\brief                           BeBoardFWInterface base class of all type of boards
\author                          Lorenzo BIDEGAIN, Nicolas PIERRE
\version                         1.0
\date                            28/07/14
Support :                        mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com
*/

#ifndef BEBOARDFWINTERFACE_H
#define BEBOARDFWINTERFACE_H

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Chip.h"
#include "../HWDescription/ChipRegItem.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/Hybrid.h"
#include "../HWDescription/MPA.h"
#include "../HWDescription/ReadoutChip.h"
#include "../HWDescription/SSA.h"
#include "../NetworkUtils/TCPClient.h"
#include "../Utils/Exception.h"
#include "../Utils/FileHandler.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include "D19cFpgaConfig.h"
#include "RegManager.h"

#include <uhal/uhal.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>

namespace Ph2_HwInterface
{
struct CPBconfig
{
    uint8_t  fEnable      = 0;
    uint8_t  fReTry       = 0;
    uint8_t  fVerbose     = 0;
    uint32_t fWait_us     = 50;
    uint16_t fMaxAttempts = 500;
    uint8_t  fResetEn     = 1;
};

/*!
 * \class BeBoardFWInterface
 * \brief Class separating board system FW interface from uHal wrapper
 */
class BeBoardFWInterface : public RegManager
{
  public:
    bool         fSaveToFile;
    FileHandler* fFileHandler;
    uint32_t     fNthAcq{0}, fNpackets{0};
    uint8_t      fCurrentPage = 0; // For slow control

    static const uint32_t cMask1 = 0xff;
    static const uint32_t cMask2 = 0xff00;
    static const uint32_t cMask3 = 0xff0000;
    static const uint32_t cMask4 = 0xff000000;
    static const uint32_t cMask5 = 0x1e0000;
    static const uint32_t cMask6 = 0x10000;
    static const uint32_t cMask7 = 0x200000;

  public:
    /*!
     * \brief Constructor of the BeBoardFWInterface class
     * \param puHalConfigFileName : path of the uHal Config File*/
    BeBoardFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    BeBoardFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    /*!
     * \brief set a FileHandler Object and enable saving to file!
     * \param pFileHandler : pointer to file handler for saving Raw Data*/
    // void setFileHandler (FileHandler* pHandler);
    virtual void setFileHandler(FileHandler* pHandler) = 0;

    /*!
     * \brief Destructor of the BeBoardFWInterface class
     */
    virtual ~BeBoardFWInterface() {}

    /*!
     * \brief enable the file handler temporarily
     */
    void enableFileHandler() { fSaveToFile = true; }
    /*!
     * \brief disable the file handler temporarily
     */
    void disableFileHandler() { fSaveToFile = false; }

    /*!
     * \brief Get the board type
     */
    virtual std::string readBoardType();

    /*!
     * \brief Get the board infos
     */
    virtual uint32_t getBoardInfo() = 0;

    virtual void ProgramCdce() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    virtual void selectLink(const uint8_t pLinkId, uint32_t pWait_ms = 100) = 0;

    /*! \brief Run Bit Error Rate test */
    virtual double RunBERtest(bool given_time, double frames_or_time, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed) = 0;

    /*!
     * \brief Encode a/several word(s) readable for a Chip
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to write
     * \param pChip : Chip object
     */
    virtual void EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, Ph2_HwDescription::Chip* pChip, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    /*!< Encode a/several word(s) readable for a Chip*/
    /*!
     * \brief Encode a/several word(s) for Broadcast write to Chips
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to write
     * \param pNChip : number of Chips to write to
     * \param pVecReq : Vector to stack the encoded words
     */
    virtual void BCEncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pNChip, std::vector<uint32_t>& pVecReq, bool pRead = false, bool pWrite = false)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    /*!< Encode a/several word(s) readable for a Chip*/
    /*!
     * \brief Decode a word from a read of a register of the Chip
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to read
     * \param pChipId : Id of the Chip to work with
     * \param pWord : variable to put the decoded word
     */
    virtual void DecodeReg(Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t& pChipId, uint32_t pWord, bool& pRead, bool& pFailed)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    /*!
     * \brief Configure the board with its Config File
     * \param pBoard
     */
    virtual void ConfigureBoard(const Ph2_HwDescription::BeBoard* pBoard) = 0;

    /*!
     * \brief Send a Chip reset
     */
    virtual void ChipReset() = 0;

    /*!
     * \brief Send a Chip re-sync
     */
    virtual void ChipReSync() = 0;

    /*!
     * \brief Send a Chip trigger
     */
    virtual void ChipTrigger() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    /*!
     * \brief Send a Chip trigger
     */
    virtual void ChipTestPulse() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    /*!
     * \brief Start a DAQ
     */
    virtual void Start() = 0;

    /*!
     * \brief Stop a DAQ
     */
    virtual void Stop() = 0;

    /*!
     * \brief Pause a DAQ
     */
    virtual void Pause() = 0;

    /*!
     * \brief Resume a DAQ
     */
    virtual void Resume() = 0;
    
    /*!
     * \brief Read data from DAQ
     * \param pBoard
     * \param pBreakTrigger : if true, enable the break trigger
     * \return fNpackets: the number of packets read
     */
    virtual uint32_t ReadData(Ph2_HwDescription::BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait = true) = 0;

    /*!
     * \brief Read data for pNEvents
     * \param pBoard : the pointer to the BeBoard
     * \param pNEvents :  the 1 indexed number of Events to read - this will set the packet size to this value -1
     */
    virtual void ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait = true) = 0;

    virtual std::vector<uint32_t> ReadBlockRegValue(const std::string& pRegNode, const uint32_t& pBlocksize)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return {};
    }

    virtual bool WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    virtual bool BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    virtual void ReadChipBlockReg(std::vector<uint32_t>& pVecReq) { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    /*!
     * Activate power on and off sequence
     */
    virtual void PowerOn();
    virtual void PowerOff();

    /*!
     * Read the firmware version
     */
    virtual void ReadVer();

    virtual BoardType getBoardType() const = 0;

    /*! \brief Set or reset the start signal */
    virtual void SetForceStart(bool bStart) { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    // ############################
    // # Read/Write Optical Group #
    // ############################
    virtual void     StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus)                                                               = 0;
    virtual void     ResetOptoLink()                                                                                                                           = 0;
    virtual bool     WriteOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop = false) = 0;
    virtual uint32_t ReadOptoLinkRegister(const Ph2_HwDescription::Chip* pChip, const uint32_t pAddress)                                                       = 0;

    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    virtual void                  ResetCPB() {}
    virtual void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector) {}
    virtual std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords) { return {}; }
    // Function to read/write lpGBT registers
    virtual bool    WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) { return true; }
    virtual uint8_t ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress) { return 0; }
    // function for I2C transactions using lpGBT I2C Masters
    virtual bool    I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, uint8_t pFrequency, uint32_t& theI2CWriteCount) { return true; }
    virtual uint8_t I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFrequency, uint32_t& theI2CReadCount) { return 0; }
    // function for front-end slow control
    virtual bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false) { return true; }
    virtual uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress) { return 0; }

    // ##########################################
    // # Configuration FE Read/Write #
    // ##########################################
    // Register write
    virtual bool    SingleRegisterWrite(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem, bool pVerify = true) { return true; }
    virtual bool    MultiRegisterWrite(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem, bool pVerify = true) { return true; }
    // Register write + read-back 
    virtual bool    MultiRegisterWriteRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem) { return true; }
    virtual bool    SingleRegisterWriteRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem){ return true; }
    // Register read 
    virtual uint8_t SingleRegisterRead(Ph2_HwDescription::Chip* pChip, Ph2_HwDescription::ChipRegItem& pItem ) { return 0; }
    virtual std::vector<uint8_t> MultiRegisterRead(Ph2_HwDescription::Chip* pChip, std::vector<Ph2_HwDescription::ChipRegItem>& pItem ) { std::vector<uint8_t> cData(0); return cData; }
     
    void ConfigureCPB(CPBconfig pConfig)
    {
        fCPBConfig.fEnable      = pConfig.fEnable;
        fCPBConfig.fVerbose     = pConfig.fVerbose;
        fCPBConfig.fWait_us     = pConfig.fWait_us;
        fCPBConfig.fReTry       = pConfig.fReTry;
        fCPBConfig.fMaxAttempts = pConfig.fMaxAttempts;
    }

  protected:
    uint32_t  fBlockSize{0};
    uint32_t  fNPackets{0};
    uint32_t  numAcq{0};
    uint32_t  nbMaxAcq{0};
    CPBconfig fCPBConfig;
};

} // namespace Ph2_HwInterface

#endif
