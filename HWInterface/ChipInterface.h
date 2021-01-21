/*!
        \file                                            ChipInterface.h
        \brief                                           User Interface to the Chip, base class for, CBC, MPA, SSA, RD53
        \author                                          Fabio RAVERA
        \version                                         1.0
        \date                        25/02/19
        Support :                    mail to : fabio.ravera@cern.ch
 */

#ifndef __CHIPINTERFACE_H__
#define __CHIPINTERFACE_H__

#include "BeBoardFWInterface.h"
#include <vector>

template <typename T>
class ChannelContainer;

/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
using BeBoardFWMap = std::map<uint16_t, BeBoardFWInterface*>; /*!< Map of Board connected */

/*!
 * \class ChipInterface
 * \brief Class representing the User Interface to the Chip on different boards
 */
class ChipInterface
{
  protected:
    BeBoardFWMap        fBoardMap;           /*!< Map of Board connected */
    BeBoardFWInterface* fBoardFW;            /*!< Board loaded */
    uint16_t            prevBoardIdentifier; /*!< Id of the previous board */

    uint16_t fRegisterCount;    /*!< Counter for the number of Registers written */
    uint16_t fTransactionCount; /*!< Counter for the number of Transactions */

    /*!
     * \brief Set the board to talk with
     * \param pBoardId
     */
    void setBoard(uint16_t pBoardIdentifier);

  public:
    /*!
     * \brief Constructor of the ChipInterface Class
     * \param pBoardMap
     */
    ChipInterface(const BeBoardFWMap& pBoardMap);
    /*!
     * \brief Destructor of the ChipInterface Class
     */
    virtual ~ChipInterface();
    /*!
     * \brief Configure the Chip with the Chip Config File
     * \param pChip: pointer to Chip object
     * \param pVerifLoop: perform a readback check
     * \param pBlockSize: the number of registers to be written at once, default is 310
     */
    virtual bool ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) = 0;

    /*!
     * \brief Write the designated register in both Chip and Chip Config File
     * \param pChip
     * \param pRegNode : Node of the register to write
     * \param pValue : Value to write
     */
    virtual bool WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop = true) = 0;

    virtual void WriteModuleBroadcastChipReg(const Ph2_HwDescription::Module* pModule, const std::string& pRegNode, uint16_t data)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    virtual void WriteBoardBroadcastChipReg(const Ph2_HwDescription::BeBoard* pBoard, const std::string& pRegNode, uint16_t data)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
    }

    /*!
     * \brief Write several registers in both Chip and Chip Config File
     * \param pChip
     * \param pVecReq : Vector of pair: Node of the register to write versus value to write
     */
    virtual bool WriteChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop = true)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "\tError: implementation of virtual member function is absent" << RESET;
        return false;
    }

    /*!
     * \brief Read the designated register in the Chip
     * \param pChip
     * \param pRegNode : Node of the register to read
     */
    virtual uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) = 0;

    void output();
};
} // namespace Ph2_HwInterface

#endif
