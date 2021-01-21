/*
        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com
 */

#include "../Utils/MPAEvent.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Event implementation
MPAEvent::MPAEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) { SetEvent(pBoard, pNbCbc, list); }

// MPAEvent::MPAEvent ( const Event& pEvent ) :
// fBunch ( pEvent.fBunch ),
// fOrbit ( pEvent.fOrbit ),
// fLumi ( pEvent.fLumi ),
// fEventCount ( pEvent.fEventCount ),
// fEventCountCBC ( pEvent.fEventCountCBC ),
// fTDC ( pEvent.fTDC ),
// fEventSize (pEvent.fEventSize),
// fEventDataMap ( pEvent.fEventDataMap )
//{
//}

void MPAEvent::SetEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) {}
} // namespace Ph2_HwInterface
