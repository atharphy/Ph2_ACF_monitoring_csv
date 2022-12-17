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

#include "../Utils/BitMaster/BitVector.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "../Utils/RD53Event.h"
#include "RD53.h"
#include "RD53BCommands.h"

// ############################
// # Chip event configuration #
// ############################
namespace RD53BEvtEncoder
{
const uint8_t NBIT_CHIPID = 2;  // Number of chip ID bits
const uint8_t NBIT_TRIGID = 8;  // Number of trigger ID bits
const uint8_t NBIT_TRGTAG = 8;  // Number of trigger tag bits
const uint8_t NBIT_BCID   = 11; // Number of bunch crossing ID bits
const uint8_t NBIT_TOT    = 4;  // Number of ToT bits
const uint8_t NBIT_CCOL   = 6;  // Number of core column bits
} // namespace RD53BEvtEncoder

namespace RD53BConstants
{
const uint8_t  BROADCAST_CHIPID  = 31;   // Broadcast chip ID used to send the command to multiple chips
const uint16_t GLOBAL_PULSE_ADDR = 0x3D; // Global Pulse Route regiser address
} // namespace RD53BConstants

// ####################################################################################
// # Formula: Vref / ADCrange * VCal / electron_charge [C] * capacitance [F] + offset #
// ####################################################################################
namespace RD53BchargeConvertion
{
const float Vref     = 0.8;    // Vref [V]
const float ADCrange = 4096.0; // VCal total range
const float cap      = 8.0;    // [fF]
const float ele      = 1.6;    // [e-19]
const float offset   = 64;     // Due to VCal_High vs VCal_Med offset difference [e-]
} // namespace RD53BchargeConvertion

namespace Ph2_HwDescription
{
class RD53B : public RD53
{
  public:
    static constexpr size_t NROWS = 336; // Total number of rows
    static constexpr size_t NCOLS = 432; // Total number of columns

    static constexpr FrontEnd CROC = {"CROC",
                                      "DAC_GDAC_M_LIN",
                                      "DAC_KRUM_CURR_LIN",
                                      "TriggerConfig",
                                      "DAC_LDAC_LIN",
                                      "VDDD",
                                      "VDDA",
                                      1,
                                      32,
                                      RD53Shared::setBits(RD53BEvtEncoder::NBIT_TOT) - 1,
                                      RD53Shared::setBits(RD53BEvtEncoder::NBIT_BCID),
                                      RD53Shared::setBits(RD53BEvtEncoder::NBIT_TRIGID),
                                      4,
                                      4,
                                      0,
                                      RD53B::NCOLS - 1,
                                      0,
                                      0x01};

    static void decodeChipData(BitView<const uint32_t> bits, Ph2_HwInterface::RD53ChipEvent& e, const Ph2_HwInterface::FormatOptions& options = {});

    RD53B() {}
    RD53B(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);

    const FrontEnd*       getFEtype(size_t colStart, size_t colStop) const override { return &RD53B::CROC; }
    size_t                getNRows() const override { return RD53B::NROWS; }
    size_t                getNCols() const override { return RD53B::NCOLS; }
    std::vector<uint16_t> getLaneUpInitSequence() const override { return {}; }
    uint32_t              getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const override;
    float                 VCal2Charge(float VCal, bool isNoise = false) const override;
    float                 Charge2VCal(float Charge) const override;
};

} // namespace Ph2_HwDescription

#endif
