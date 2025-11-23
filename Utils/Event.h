/*
        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com
 */

#ifndef __EVENT_H__
#define __EVENT_H__

// #include "ConsoleColor.h"
// #include "HWDescription/BeBoard.h"
// #include "HWDescription/Definition.h"
// #include "SLinkEvent.h"
// #include "Utils/DataContainer.h"
// #include "Utils/easylogging++.h"
#include <bitset>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>
#include <memory>
#include <vector>
#include <map>
#include <iostream>

class BoardDataContainer;
class HybridDataContainer;
class ChipDataContainer;
class ChannelGroupBase;

namespace Ph2_HwDescription
{
  class BeBoard;
}

namespace Ph2_HwInterface
{

class Event
{
  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pNbCbc
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    Event() {}
    /*!
     * \brief Copy Constructor of the Event Class
     */
    Event(const Event& pEvent) = default;
    /*!
     * \brief Copy Assignment of the Event Class
     */
    Event& operator=(const Event& pEvent) = default;
    /*!
     * \brief Destructor of the Event Class
     */
    virtual ~Event() {}

    virtual uint32_t GetNHits(uint8_t pHybridId, uint8_t pCbcId) {return 0;}

    virtual void fillDataContainer(BoardDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup);
    virtual void fillChipDataContainer(ChipDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint8_t hybridId) = 0;
};

} // namespace Ph2_HwInterface
#endif
