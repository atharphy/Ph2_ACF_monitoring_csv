/*!
  \file                           FrontEndDescription.cc
  \brief                          FrontEndDescription base class to describe all parameters common to all FE Components
  in the DAQ chain \author                         Lorenzo BIDEGAIN \version                        1.0 \date 25/06/14
  Support :                       mail to : lorenzo.bidegain@gmail.com
*/

#include "FrontEndDescription.h"

namespace Ph2_HwDescription
{
FrontEndDescription::FrontEndDescription(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, bool pStatus, FrontEndType pType) : fBeId(pBeId), fFMCId(pFMCId), fFeId(pFeId), fStatus(pStatus), fType(pType) {}

FrontEndDescription::FrontEndDescription() : fBeId(0), fFMCId(0), fFeId(0), fStatus(true), fType(FrontEndType::UNDEFINED) {}

FrontEndDescription::FrontEndDescription(const FrontEndDescription& pFeDesc) : fBeId(pFeDesc.fBeId), fFMCId(pFeDesc.fFMCId), fFeId(pFeDesc.fFeId), fStatus(pFeDesc.fStatus), fType(pFeDesc.fType) {}

FrontEndDescription::~FrontEndDescription() {}
} // namespace Ph2_HwDescription
