/*!
  \file                         RegManager.h
  \brief                        RegManager class, permit connection & r/w registers
  \author                       Nicolas PIERRE
  \version                      1.0
  \date                         06/06/14
  Support :                     mail to : nico.pierre@icloud.com
*/

#ifndef REGMANAGER_H
#define REGMANAGER_H

#include "../Utils/easylogging++.h"
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <uhal/uhal.hpp>
#include <utility>
#include <vector>

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
/*!
 * \class RegManager
 * \brief Permit connection to given boards and r/w given registers
 */
class RegManager
{
  protected:
    uhal::HwInterface*                            fBoard;              /*!< Board in use*/
    const char*                                   fUHalConfigFileName; /*!< path of the uHal Config File*/
    std::vector<std::pair<std::string, uint32_t>> fStackReg;           /*!< Stack of registers*/
    const char*                                   fUri;
    const char*                                   fAddressTable;
    const char*                                   fId;

  public:
    // Connection w uHal
    /*!
     * \brief Constructor of the RegManager class
     * \param puHalConfigFileName : path of the uHal Config File
     * \param pBoardId Board Id in the XML configuration file. The uHAL connection name will be boardX where X is the
     * number Id.
     */
    RegManager(const char* puHalConfigFileName, uint32_t pBoardId);

    /*!
     * \brief Constructor of the RegManager class
     * \param pId : id in the connections node
     * \param pUri: URI string for uHAL
     * \param pAddressTable: address table path
     */
    RegManager(const char* pId, const char* pUri, const char* pAddressTable);

    /*!
     * \brief Destructor of the RegManager class
     */
    virtual ~RegManager();

    /*!
     * \brief Write a register
     * \param pRegNode : Node of the register to write
     * \param pVal : Value to write
     * \return boolean confirming the writing
     */
    virtual bool WriteReg(const std::string& pRegNode, const uint32_t& pVal);

    /*!
     * \brief Write a stack of registers
     * \param pVecReg : vector containing the registers and the associated values to write
     * \return boolean confirming the writing
     */
    virtual bool WriteStackReg(const std::vector<std::pair<std::string, uint32_t>>& pVecReg);

    /*!
     * \brief Write a block of values in a register
     * \param pRegNode : Node of the register to write
     * \param pValues : Block of values to write
     * \return boolean confirming the writing
     */
    virtual bool WriteBlockReg(const std::string& pRegNode, const std::vector<uint32_t>& pValues);

    /** \brief Write a block of values at a given address
     * \param uAddr 32-bit address
     * \param pValues : Block of values to write
     * \param bNonInc true if Write mode is non-incremental
     * \return boolean confirming the writing
     */
    virtual bool WriteBlockAtAddress(uint32_t uAddr, const std::vector<uint32_t>& pValues, bool bNonInc = false);

    /*!
     * \brief Read a value in a register
     * \param pRegNode : Node of the register to read
     * \return ValWord value of the register
     */
    virtual uint32_t ReadReg(const std::string& pRegNode);

    /*!
     * \brief Read a value at a given address
     * \param uAddr 32-bit address
     * \param uMask 32-bit mask
     * \return ValWord value of the register
     */
    virtual uint32_t ReadAtAddress(uint32_t uAddr, uint32_t uMask = 0xFFFFFFFF);

    /*!
     * \brief Read a block of values in a register
     * \param pRegNode : Node of the register to read
     * \param pBlocksize : Size of the block to read
     * \return ValVector block values of the register
     */
    virtual std::vector<uint32_t> ReadBlockReg(const std::string& pRegNode, const uint32_t& pBlocksize);

    /*!
     * \brief Read a block of values in a register
     * \param pRegNode : Node of the register to read
     * \param pBlocksize : Size of the block to read
     * \param pBlockOffset : Offset of the block
     * \return ValVector block values of the register
     */
    virtual std::vector<uint32_t> ReadBlockRegOffset(const std::string& pRegNode, const uint32_t& pBlocksize, const uint32_t& pBlockOffset);

    /*!
     * \brief Reset the HW Interface with different Id, Uri and Address Table
     */
    virtual void ResetRegManager(const char* pId, const char* pUri, const char* pAddressTable)
    {
        if(fBoard)
        {
            delete fBoard;
            fBoard = new uhal::HwInterface(uhal::ConnectionManager::getDevice(pId, pUri, pAddressTable));
        }
    }

    /*!
     * \brief Stack the commands, deliver when full or timeout
     * \param pRegNode : Register to write
     * \param pVal : Value to write
     * \param pSend : Send the stack to write or nor (1/0)
     */
    virtual void StackReg(const std::string& pRegNode, const uint32_t& pVal, bool pSend = false);

    /*!
     * \brief get the uHAL HW Interface
     */
    uhal::HwInterface* getHardwareInterface() const { return fBoard; }

    /*!
     * \brief get the uHAL HW Id
     */
    const char* getId() { return fId; }

    /*!
     * \brief get the uHAL HW Uri
     */
    const char* getUri() { return fUri; }

    /*!
     * \brief get the uHAL HW AddressTable
     */
    const char* getAddressTable() { return fAddressTable; }

    /*!
     * \brief get the uHAL node
     */
    const uhal::Node& getUhalNode(const std::string& pStrPath);

    // ##############################################
    // # Capure and replay data stream to/from FPGA #
    // ##############################################
    static void enableCapture(const std::string filename);
    static void enableReplay(const std::string filename);

  private:
    enum class Mode
    {
        Default,
        Capture,
        Replay
    };

    static Mode           mode;
    uint32_t              replayRead();
    std::vector<uint32_t> replayBlockRead(size_t size);
    void                  captureRead(uint32_t value);
    void                  captureBlockRead(std::vector<uint32_t> data);
};
} // namespace Ph2_HwInterface

#endif
