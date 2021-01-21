/*!

        \file                                            GbtInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifndef __MUXCRATEINTERFACE_H__
#define __MUXCRATEINTERFACE_H__

#include "BeBoardInterface.h"

namespace Ph2_HwDescription
{
class BeBoard;
}
/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
/*!
 * \class Mux crate interface
 * \brief Class representing the User Interface to the multiplexing crate control
 */
class MuxCrateInterface : public BeBoardInterface
{
  public:
    /*!
     * \brief Constructor of the GbtInterface Class
     * \param pBoardMap
     */
    MuxCrateInterface(const BeBoardFWMap& pBoardMap);
    /*!
     * \brief Destructor of the GbtInterface Class
     */
    ~MuxCrateInterface();

    void ScanCrate(Ph2_HwDescription::BeBoard* pBoard);
    void DisconnectCrate(Ph2_HwDescription::BeBoard* pBoard);
    void SelectCard(Ph2_HwDescription::BeBoard* pBoard, int BackplaneNum, int CardNum);

    void                            parseAvailable(bool filterBoardsWithoutCards = true);
    void                            printAvailableCards();
    uint32_t                        getAvailableCards() { return fAvailableCards; }
    std::map<int, std::vector<int>> availableCards() { return fAvailable; }
    void                            configureWait(uint16_t pWait_ms) { fWait_ms = pWait_ms; }

  private:
    uint16_t                        fWait_ms;
    uint32_t                        fAvailableCards;
    uint16_t                        fBackplaneNum;
    uint16_t                        fCardNum;
    std::map<int, std::vector<int>> fAvailable;

  protected:
    void     DisconnectMultiplexingSetup(Ph2_HwDescription::BeBoard* pBoard);
    uint32_t ScanMultiplexingSetup(Ph2_HwDescription::BeBoard* pBoard);
    uint32_t ConfigureMultiplexingSetup(Ph2_HwDescription::BeBoard* pBoard);
};
} // namespace Ph2_HwInterface

#endif
