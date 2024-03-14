/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "Utils/SSAEvent.h"
#include "HWDescription/OuterTrackerHybrid.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Event implementation
SSAEvent::SSAEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) { SetEvent(pBoard, pNbCbc, list); }

} // namespace Ph2_HwInterface
