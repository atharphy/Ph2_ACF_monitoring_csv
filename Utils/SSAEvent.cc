/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "../Utils/SSAEvent.h"
#include "../HWDescription/OuterTrackerModule.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Event implementation
SSAEvent::SSAEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) { SetEvent(pBoard, pNbCbc, list); }

// SSAEvent::SSAEvent ( const Event& pEvent ) :
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

// void SSAEvent::SetEvent ( const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list )
// {
//     fEventSize = pNbCbc *  CBC_EVENT_SIZE_32  + EVENT_HEADER_TDC_SIZE_32;

//     //now decode FEEvents
//     uint32_t cNFe = static_cast<uint32_t> ( pBoard->getNFe() );
//     for ( uint8_t cFeId = 0; cFeId < cNFe; cFeId++ )
//     {
//         uint32_t cNSSA;
//         cNSSA = static_cast<uint32_t> ( static_cast<OuterTrackerModule*>(pBoard->getModule ( cFeId ))->getNSSA() );

//         for ( uint8_t cSSAId = 0; cSSAId < cNSSA; cSSAId++ )
//         {
//             uint16_t cKey = encodeId (cFeId, cSSAId);

//             uint32_t begin = SSA_HEADER_SIZE_32 + cFeId * SSA_EVENT_SIZE_32 * cNSSA + cSSAId * SSA_EVENT_SIZE_32;
//             uint32_t end = begin + SSA_EVENT_SIZE_32;

//             std::vector<uint32_t> cSSAData (std::next (std::begin (list), begin), std::next (std::begin (list), end)
//             ); fEventDataMap[cKey] = cSSAData;
//         }
//     }
// }
} // namespace Ph2_HwInterface
