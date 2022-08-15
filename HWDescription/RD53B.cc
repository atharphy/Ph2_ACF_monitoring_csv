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
    ReadoutChip::fChipOriginalMask = std::make_shared<RD53ChannelGroup>(RD53B::NROWS, RD53B::NCOLS, true);
    RD53::loadfRegMap(fileName);
    this->setFrontEndType(FrontEndType::RD53B);
}

void RD53B::decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent) const {}

uint32_t RD53B::getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const {
    return bits::pack<1, 5, 8, 1, 5>(cal_edge_mode, cal_edge_delay, cal_edge_width, cal_aux_mode, cal_aux_delay);
}

} // namespace Ph2_HwDescription
