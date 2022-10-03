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
const uint8_t NBIT_TOT    = 4;   // Number of ToT bits
const uint8_t NBIT_SIDE   = 1;   // Number of "side" bits
const uint8_t NBIT_ROW    = 9;   // Number of row bits
const uint8_t NBIT_CCOL   = 6;   // Number of core column bits
} // namespace RD53AEvtEncoder

namespace RD53AConstants
{
const uint8_t  BROADCAST_CHIPID    = 0x08; // Broadcast chip ID used to send the command to multiple chips
const uint8_t  AUTO_INCREMENT_MASK = 0x08; // Auto-increment mask bits
const uint16_t GLOBAL_PULSE_ADDR   = 0x2C; // Global Pulse Route regiser address
} // namespace RD53AConstants

// #####################################################################
// # Formula: par0/par1 * VCal / electron_charge [C] * capacitance [C] #
// #####################################################################
namespace RD53AchargeConvertion
{
const float Vref     = 0.9;    // Vref [V]
const float ADCrange = 4096.0; // VCal total range
const float cap      = 8.5;    // [fF]
const float ele      = 1.6;    // [e-19]
const float offset   = 64;     // Due to VCal_High vs VCal_Med offset difference [e-]
} // namespace RD53AchargeConvertion

namespace Ph2_HwDescription
{
class RD53A : public RD53
{
  public:
    static constexpr size_t NROWS = 192; // Total number of rows
    static constexpr size_t NCOLS = 400; // Total number of columns

    static constexpr FrontEnd SYNC = {"SYNC",
                                      "VTH_SYNC",
                                      "IBIAS_KRUM_SYNC",
                                      "LATENCY_CONFIG",
                                      0,
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_TOT) - 1,
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID),
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_TRIGID),
                                      0,
                                      127};
    static constexpr FrontEnd LIN  = {"LIN",
                                     "Vthreshold_LIN",
                                     "KRUM_CURR_LIN",
                                     "LATENCY_CONFIG",
                                     16,
                                     RD53Shared::setBits(RD53AEvtEncoder::NBIT_TOT) - 1,
                                     RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID),
                                     RD53Shared::setBits(RD53AEvtEncoder::NBIT_TRIGID),
                                     128,
                                     263};
    static constexpr FrontEnd DIFF = {"DIFF",
                                      "VTH1_DIFF",
                                      "VFF_DIFF",
                                      "LATENCY_CONFIG",
                                      31,
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_TOT) - 1,
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID),
                                      RD53Shared::setBits(RD53AEvtEncoder::NBIT_TRIGID),
                                      264,
                                      399};
    static const FrontEnd*    frontEnds[];

    static void decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent);

    RD53A(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);

    const FrontEnd* getFEtype(size_t colStart, size_t colStop) const override;
    size_t          getNRows() const override { return RD53A::NROWS; }
    size_t          getNCols() const override { return RD53A::NCOLS; }
    uint32_t        getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const override;
    float           VCal2Charge(float VCal, bool isNoise = false) const override;
    float           Charge2VCal(float Charge) const override;
};

} // namespace Ph2_HwDescription

#endif
