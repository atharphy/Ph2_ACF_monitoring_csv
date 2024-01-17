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

#include "Definition.h"
#include "OpticalGroup.h"
#include "Utils/ConditionDataSet.h"
#include "Utils/Container.h"
#include "Utils/Visitor.h"
#include "Utils/easylogging++.h"
#include "pugixml.hpp"
#include <map>
#include <regex>
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

    BeBoard(const BeBoard&) = delete;

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
    uint8_t getNHybrid() const
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
    const BeBoardRegMap& getBeBoardRegMap() const { return fRegMap; }

    void setOptical(bool pOptical) { fOptical = pOptical; }

    void setCDCEconfiguration(bool pConfigure, uint32_t pClockRate = 120)
    {
        fConfigureCDCE = pConfigure;
        fClockRateCDCE = pClockRate;
    }

    bool isOptical() const { return fOptical; }

    std::pair<bool, uint32_t> configCDCE() const { return std::make_pair(fConfigureCDCE, fClockRateCDCE); }

    void setBoardType(const BoardType pBoardType) { fBoardType = pBoardType; }

    BoardType getBoardType() const { return fBoardType; }
    void      printBoardType()
    {
        if(fBoardType == BoardType::RD53) LOG(INFO) << BOLDBLUE << "\t--> Found an Inner Tracker Readout-board" << RESET;
        if(fBoardType == BoardType::D19C) LOG(INFO) << BOLDBLUE << "\t--> Found an Outer Tracker Readout-board" << RESET;
    }

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

    void    setLinkReset(uint8_t pReset) { fResetLink = pReset; }
    uint8_t getLinkReset() const { return fResetLink; }

    void    setStubOffset(uint16_t pOffset) { fStubOffset = pOffset; }
    uint8_t getStubOffset() const { return fStubOffset; }

    void    setReset(uint8_t pReset) { fReset = pReset; }
    uint8_t getReset() const { return fReset; }

    void setToConfigure(uint8_t pToConfigure) { fToConfigure = pToConfigure; }
    bool getToConfigure() const { return fToConfigure; }

    void        setConnectionId(std::string theConnectionId) { fConnectionId = theConnectionId; }
    std::string getConnectionId() const { return fConnectionId; }

    void        setConnectionUri(std::string theConnectionUri) { fConnectionUri = theConnectionUri; }
    std::string getConnectionUri() const { return fConnectionUri; }

    void        setAddressTable(std::string theAddressTable) { fAddressTable = theAddressTable; }
    std::string getAddressTable() const { return fAddressTable; }

    std::vector<FrontEndType> connectedFrontEndTypes() const;
    int                       dummyValue_ = 1989;

    void dumpRegisters()
    {
        for(auto reg: fRegMap) std::cout << reg.first << " " << reg.second << std::endl;
    }

    void saveRegMap(const std::string& fileName);

    void                                          takeSnapshot();
    void                                          clearSnapshot();
    std::vector<std::pair<std::string, uint32_t>> getSnapshot() const;
    void                                          reinitializeFreeRegisters();
    void                                          addFreeRegister(const std::regex& theRegisterName);

    void parseRegister(pugi::xml_node pRegisterNode, std::string& pAttributeString, double& pValue);

  protected:
    BoardType    fBoardType;
    EventType    fEventType;
    FrontEndType fFrontEndType;

    ConditionDataSet* fCondDataSet;
    bool              fOptical{false};
    bool              fConfigureCDCE{false};
    bool              fSparsifed{false};
    uint32_t          fClockRateCDCE{320};
    uint8_t           fResetLink{1};
    uint16_t          fStubOffset{0};
    uint8_t           fReset{0};
    bool              fToConfigure{true};

    std::string fConnectionId{""};
    std::string fConnectionUri{""};
    std::string fAddressTable{""};

  private:
    /*!
     * \brief Load RegMap from a file
     * \param filename
     */
    void                    loadConfigFile(const std::string& filename);
    BeBoardRegMap           fRegMap; /*!< Map of BeBoard Register Names vs. Register Values */
    bool                    fTrackModifiedRegistersEnabled{false};
    BeBoardRegMap           fModifiedRegisters{};
    std::vector<std::regex> fListOfFreeRegisters{};
};
} // namespace Ph2_HwDescription

#endif
