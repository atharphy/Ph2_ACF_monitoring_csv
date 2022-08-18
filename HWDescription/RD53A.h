/*!
  \file                  RD53A.h
  \brief                 RD53A description class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53A_H
#define RD53A_H

#include "../Utils/RD53ChannelGroupHandler.h"
#include "RD53.h"
#include "RD53ACommands.h"

// ############################
// # Chip event configuration #
// ############################
namespace RD53AEvtEncoder
{
const uint8_t HEADER      = 0x1; // Data header word
const uint8_t NBIT_HEADER = 7;   // Number of data header bits
const uint8_t NBIT_TRIGID = 5;   // Number of trigger ID bits
const uint8_t NBIT_TRGTAG = 5;   // Number of trigger tag bits
const uint8_t NBIT_BCID   = 15;  // Number of bunch crossing ID bits
const uint8_t NBIT_TOT    = 16;  // Number of ToT bits
const uint8_t NBIT_SIDE   = 1;   // Number of "side" bits
const uint8_t NBIT_ROW    = 9;   // Number of row bits
const uint8_t NBIT_CCOL   = 6;   // Number of core column bits
} // namespace RD53AEvtEncoder

namespace RD53AConstants
{
const uint8_t  BROADCAST_CHIPID    = 0x08; // Broadcast chip ID used to send the command to multiple chips
const uint8_t  AUTO_INCREMENT_MASK = 0x18; // Auto-increment mask bits
const uint16_t GLOBAL_PULSE_ADDR   = 0x2C; // Global Pulse Route regiser address
} // namespace RD53AConstants

namespace Ph2_HwDescription
{
class RD53A : public RD53
{
  public:
    static constexpr size_t NROWS = 192; // Total number of rows
    static constexpr size_t NCOLS = 400; // Total number of columns

    static constexpr FrontEnd SYNC = {"SYNC", "VTH_SYNC", "IBIAS_KRUM_SYNC", 0, 0, 127};
    static constexpr FrontEnd LIN  = {"LIN", "Vthreshold_LIN", "KRUM_CURR_LIN", 16, 128, 263};
    static constexpr FrontEnd DIFF = {"DIFF", "VTH1_DIFF", "VFF_DIFF", 31, 264, 399};
    static const FrontEnd*    frontEnds[];

    RD53A(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);

    const FrontEnd* getMajorityFE(size_t colStart, size_t colStop) const override;
    size_t          getNRows() const override { return RD53A::NROWS; }
    size_t          getNCols() const override { return RD53A::NCOLS; }
    void            decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent) const override;
    uint32_t        getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const override;
};

} // namespace Ph2_HwDescription

#endif
