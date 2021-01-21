/*
        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com
 */

#ifndef __MPAEVENT_H__
#define __MPAEVENT_H__

#include "Event.h"

namespace Ph2_HwInterface
{
/*!
 * \class MPAEvent
 * \brief Event container to manipulate event flux from the MPA
 */
class MPAEvent : public Event
{
  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pNbCbc
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    MPAEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list);
    /*!
     * \brief Copy Constructor of the Event Class
     */
    // MPAEvent ( const Event& pEvent );
    /*!
     * \brief Destructor of the Event Class
     */
    ~MPAEvent() {}
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) override;
};
} // namespace Ph2_HwInterface

#endif
