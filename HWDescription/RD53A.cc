/*!
  \file                  RD53A.cc
  \brief                 RD53A implementation class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53A.h"

namespace Ph2_HwDescription
{
// ########################################
// # Support for different FrontEnd types #
// ########################################
constexpr RD53::FrontEnd RD53A::SYNC;
constexpr RD53::FrontEnd RD53A::LIN;
constexpr RD53::FrontEnd RD53A::DIFF;
const RD53::FrontEnd*    RD53A::frontEnds[] = {&RD53A::SYNC, &RD53A::LIN, &RD53A::DIFF};

RD53A::RD53A(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment)
    : RD53(pBeId, pFMCId, pOpticalGroupId, pHybridId, pRD53Id, pRD53Lane, fileName, cfgComment)
{
    fChipOriginalMask.reset(getChannelGroup());
}

const RD53A::FrontEnd* RD53A::getMajorityFE(size_t colStart, size_t colStop) const
{
    return *std::max_element(std::begin(frontEnds), std::end(frontEnds), [&](const FrontEnd* a, const FrontEnd* b) {
        return int(std::min(colStop, a->colStop)) - int(std::max(colStart, a->colStart)) < int(std::min(colStop, b->colStop)) - int(std::max(colStart, b->colStart));
    });
}

} // namespace Ph2_HwDescription
