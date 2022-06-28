/*!
  \file                  RD53B.cc
  \brief                 RD53B implementation class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53B.h"

namespace Ph2_HwDescription
{
// ########################################
// # Support for different FrontEnd types #
// ########################################
constexpr RD53B::FrontEnd RD53B::CROC;

RD53B::RD53B(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment)
    : RD53(pBeId, pFMCId, pOpticalGroupId, pHybridId, pRD53Id, pRD53Lane, fileName, cfgComment)
{
    fChipOriginalMask.reset(getChannelGroup());
}

} // namespace Ph2_HwDescription
