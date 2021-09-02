/*!
\file                            BeBoardFWInterface.h
\brief                           BeBoardFWInterface base class of all type of boards
\author                          Lorenzo BIDEGAIN, Nicolas PIERRE
\version                         1.0
\date                            28/07/14
Support :                        mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com
*/

#ifndef __BEBOARDFWINTERFACE_H__
#define __BEBOARDFWINTERFACE_H__

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
/*!
 * \class BeBoardFWInterface
 * \brief Class separating board system FW interface from uHal wrapper
 */
class BeBoardFWInterface : public RegManager
{
  public:
    bool         fSaveToFile;
    FileHandler* fFileHandler;
    FpgaConfig*  fFpgaConfig;
    uint32_t     fNthAcq{0}, fNpackets{0};

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

    void setPowerSupplyClient(TCPClient* thePowerSupplyClient) { fPowerSupplyClient = thePowerSupplyClient; };
#ifdef __TCP_SERVER__
    void       setTestcardClient(TCPClient* theTestcardClient) { fTestcardClient = theTestcardClient; };
    TCPClient* getTestcardClient() { return fTestcardClient; };
#endif
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

    /*! \brief Upload a configuration in a board FPGA */
    virtual void FlashProm(const std::string& strConfig, const char* pstrFile) {}

    /*! \brief Jump to an FPGA configuration */
    virtual void JumpToFpgaConfig(const std::string& strConfig) {}

    virtual void DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest) {}

    /*! \brief Current FPGA configuration*/
    virtual const FpgaConfig* GetConfiguringFpga() { return nullptr; }
    virtual void              ProgramCdce() {}

    // this is temporary until the modified command processor block is in place
    virtual void selectLink(const uint8_t pLinkId, uint32_t pWait_ms = 100) = 0;

    /*! \brief Get the list of available FPGA configuration (or firmware images)*/
    virtual std::vector<std::string> getFpgaConfigList() { return std::vector<std::string>(); }

    /*! \brief Delete one Fpga configuration (or firmware image)*/
    virtual void DeleteFpgaConfig(const std::string& strId) {}

    /*! \brief Run Bit Error Rate test */
    virtual double RunBERtest(bool given_time, double frames_or_time, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed) = 0;

    /*!
     * \brief Encode a/several word(s) readable for a Chip
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to write
     * \param pChipId : Id of the Chip to work with
     * \param pVecReq : Vector to stack the encoded words
     */
    virtual void EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pChipId, std::vector<uint32_t>& pVecReq, bool pRead, bool pWrite)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    /*!< Encode a/several word(s) readable for a Chip*/
    /*!
     * \brief Encode a/several word(s) readable for a Chip
     * \param pRegItem : RegItem containing infos (name, adress, value...) about the register to write
     * \param pChipId : Id of the Chip to work with
     * \param pVecReq : Vector to stack the encoded words
     */
    virtual void EncodeReg(const Ph2_HwDescription::ChipRegItem& pRegItem, uint8_t pFeId, uint8_t pChipId, std::vector<uint32_t>& pVecReq, bool pRead, bool pWrite)
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
    /*!< Decode a word from a read of a register of the Chip*/

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

    /*! \brief Reboot the board */
    virtual void RebootBoard() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    /*! \brief Set or reset the start signal */
    virtual void SetForceStart(bool bStart) { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET; }

    // ############################
    // # Read/Write Optical Group #
    // ############################
    virtual void     StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus)                                                                                 = 0;
    virtual void     ResetOptoLink()                                                                                                                                             = 0;
    virtual bool     WriteOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop = false) = 0;
    virtual uint32_t ReadOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress)                                                       = 0;

    // ##########################################
    // # Read/Write new Command Processor Block #
    // ##########################################
    // functions for new Command Processor Block
    virtual void                  ResetCPB() {}
    virtual void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector, bool pVerbose = false) {}
    virtual std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords, bool pVerbose = false) { return {}; }
    // function to read/write lpGBT registers
    virtual bool    WriteLpGBTRegister(uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop = true) { return true; }
    virtual uint8_t ReadLpGBTRegister(uint16_t pRegisterAddress) { return 0; }
    // function for I2C transactions using lpGBT I2C Masters
    virtual bool    I2CWrite(uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes) { return true; }
    virtual uint8_t I2CRead(uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes) { return 0; }
    // function for front-end slow control
    virtual bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pRetry = false) { return true; }
    virtual uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress) { return 0; }

  protected:
    uint32_t   fBlockSize{0};
    uint32_t   fNPackets{0};
    uint32_t   numAcq{0};
    uint32_t   nbMaxAcq{0};
    TCPClient* fPowerSupplyClient;
#ifdef __TCP_SERVER__
    TCPClient* fTestcardClient;
#endif
    // Template to return a vector of all mismatched elements in two vectors using std::mismatch for readback value
    // comparison
    template <typename T, class BinaryPredicate>
    std::vector<typename std::iterator_traits<T>::value_type> get_mismatches(T pWriteVector_begin, T pWriteVector_end, T pReadVector_begin, BinaryPredicate p)
    {
        std::vector<typename std::iterator_traits<T>::value_type> pMismatchedWriteVector;

        for(std::pair<T, T> cPair = std::make_pair(pWriteVector_begin, pReadVector_begin); (cPair = std::mismatch(cPair.first, pWriteVector_end, cPair.second, p)).first != pWriteVector_end;
            ++cPair.first, ++cPair.second)
            pMismatchedWriteVector.push_back(*cPair.first);

        return pMismatchedWriteVector;
    }
};
} // namespace Ph2_HwInterface

#endif
