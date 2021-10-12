/*!
        \file                BeBoard.h
        \brief               BeBoard Description class, configs of the BeBoard
        \author              Lorenzo BIDEGAIN
        \date                14/07/14
        \version             1.0
        Support :            mail to : lorenzo.bidegain@gmail.com
 */

#ifndef _BeBoard_h__
#define _BeBoard_h__

#include "../Utils/ConditionDataSet.h"
#include "../Utils/Container.h"
#include "../Utils/Visitor.h"
#include "../Utils/easylogging++.h"
#include "Definition.h"
#include "OpticalGroup.h"
#include <map>
#include <stdint.h>
#include <vector>

/*!
 * \namespace Ph2_HwDescription
 * \brief Namespace regrouping all the hardware description
 */
namespace Ph2_HwDescription
{
using BeBoardRegMap = std::map<std::string, uint32_t>; /*!< Map containing the registers of a board */

/*!
 * \class BeBoard
 * \brief Read/Write BeBoard's registers on a file, handles a register map and handles a vector of Hybrid which are
 * connected to the BeBoard
 */
class BeBoard : public BoardContainer
{
  public:
    // C'tors: the BeBoard only needs to know about which BE it is
    /*!
     * \brief Default C'tor
     */
    BeBoard();

    /*!
     * \brief Standard C'tor
     * \param pBeId
     */
    BeBoard(uint8_t pBeId);

    /*!
     * \brief C'tor for a standard BeBoard reading a config file
     * \param pBeId
     * \param filename of the configuration file
     */
    BeBoard(uint8_t pBeId, const std::string& filename);

    /*!
     * \brief Destructor
     */
    ~BeBoard() {}

    // Public Methods

    /*!
     * \brief acceptor method for HwDescriptionVisitor
     * \param pVisitor
     */
    void accept(HwDescriptionVisitor& pVisitor)
    {
        pVisitor.visitBeBoard(*this);

        for(auto cOpticalGroup: *this) static_cast<OpticalGroup*>(cOpticalGroup)->accept(pVisitor);
    }

    /*!
     * \brief Get the number of hybrid connected to the BeBoard
     * \return The size of the vector
     */
    uint8_t getNFe() const
    {
        uint16_t nFe = 0;
        for(auto opticalGroup: *this) nFe += opticalGroup->size();
        return nFe;
    }

    /*!
     * \brief Get any register from the Map
     * \param pReg
     * \return The value of the register
     */
    uint32_t getReg(const std::string& pReg) const;

    /*!
     * \brief Set any register of the Map, if the register is not on the map, it adds it.
     * \param pReg
     * \param psetValue
     */
    void setReg(const std::string& pReg, uint32_t psetValue);

    // /*!
    // * \brief Get the Map of the registers
    // * \return The map of register
    // */
    BeBoardRegMap getBeBoardRegMap() const { return fRegMap; }

    void setOptical(bool pOptical) { fOptical = pOptical; }

    void setUseOpticalLink(bool pUseOpticalLink) { fUseOpticalLink = pUseOpticalLink; }

    void setUseCPB(bool pUseCPB) { fUseCPB = pUseCPB; }

    void setCDCEconfiguration(bool pConfigure, uint32_t pClockRate = 120)
    {
        fConfigureCDCE = pConfigure;
        fClockRateCDCE = pClockRate;
    }

    bool ifOptical() const { return fOptical; }

    bool ifUseOpticalLink() const { return fUseOpticalLink; }

    bool ifUseCPB() const { return fUseCPB; }

    std::pair<bool, uint32_t> configCDCE() const { return std::make_pair(fConfigureCDCE, fClockRateCDCE); }

    void setBoardType(const BoardType pBoardType) { fBoardType = pBoardType; }

    BoardType getBoardType() const { return fBoardType; }

    void setEventType(const EventType pEventType) { fEventType = pEventType; }

    EventType getEventType() const { return fEventType; }

    void setFrontEndType(const FrontEndType pFrontEndType) { fFrontEndType = pFrontEndType; }

    FrontEndType getFrontEndType() const { return fFrontEndType; }

    void addConditionDataSet(ConditionDataSet* pSet)
    {
        if(pSet != nullptr) fCondDataSet = pSet;
    }

    ConditionDataSet* getConditionDataSet() const { return fCondDataSet; }

    void updateCondData(uint32_t& pTDCVal);

    void setSparsification(bool cSparsified) { fSparsifed = cSparsified; }

    bool getSparsification() const { return fSparsifed; }

    int dummyValue_ = 1989;

  protected:
    BoardType    fBoardType;
    EventType    fEventType;
    FrontEndType fFrontEndType;

    BeBoardRegMap     fRegMap; /*!< Map of BeBoard Register Names vs. Register Values */
    ConditionDataSet* fCondDataSet;
    bool              fOptical, fUseOpticalLink, fUseCPB;
    bool              fConfigureCDCE;
    bool              fSparsifed;
    uint32_t          fClockRateCDCE;

  private:
    /*!
     * \brief Load RegMap from a file
     * \param filename
     */
    void loadConfigFile(const std::string& filename);
};
} // namespace Ph2_HwDescription

#endif
