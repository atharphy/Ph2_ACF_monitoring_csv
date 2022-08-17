/*!
  \file                  RD53B.h
  \brief                 RD53B description class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53B_H
#define RD53B_H

#include "../Utils/RD53ChannelGroupHandler.h"
#include "RD53.h"
#include "RD53BCommands.h"

namespace RD53BConstants
{
const uint8_t  BROADCAST_CHIPID    = 31;   // Broadcast chip ID used to send the command to multiple chips
const uint8_t  AUTO_INCREMENT_MASK = 0x1;  // Auto-increment mask bits
const uint16_t GLOBAL_PULSE_ADDR   = 0x3D; // Global Pulse Route regiser address
} // namespace RD53BConstants

namespace Ph2_HwDescription
{
class RD53B : public RD53
{
  public:
    static constexpr size_t NROWS = 336; // Total number of rows
    static constexpr size_t NCOLS = 432; // Total number of columns

    static constexpr FrontEnd CROC = {"CROC", "DAC_GDAC_M_LIN", "DAC_KRUM_CURR_LIN", 32, 0, RD53B::NCOLS - 1};

    RD53B(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);

    const FrontEnd* getMajorityFE(size_t colStart, size_t colStop) const override { return &RD53B::CROC; }
    size_t          getNRows() const override { return RD53B::NROWS; }
    size_t          getNCols() const override { return RD53B::NCOLS; }
    void            decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent) const override;
    uint32_t        getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const override;
};

} // namespace Ph2_HwDescription

#endif
