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
    ReadoutChip::fChipOriginalMask = std::make_shared<RD53ChannelGroup>(RD53A::NROWS, RD53A::NCOLS, true);
    RD53::loadfRegMap(fileName);
    this->setFrontEndType(FrontEndType::RD53A);
}

const RD53A::FrontEnd* RD53A::getFEtype(size_t colStart, size_t colStop) const
{
    return *std::max_element(std::begin(frontEnds),
                             std::end(frontEnds),
                             [&](const FrontEnd* a, const FrontEnd* b)
                             { return int(std::min(colStop, a->colStop)) - int(std::max(colStart, a->colStart)) < int(std::min(colStop, b->colStop)) - int(std::max(colStart, b->colStart)); });
}

void RD53A::decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent)
{
    uint32_t header;

    std::tie(header, chipEvent.trigger_id, chipEvent.trigger_tag, chipEvent.bc_id) =
        bits::unpack<RD53AEvtEncoder::NBIT_HEADER, RD53AEvtEncoder::NBIT_TRIGID, RD53AEvtEncoder::NBIT_TRGTAG, RD53AEvtEncoder::NBIT_BCID>(*data);
    if(header != RD53AEvtEncoder::HEADER) chipEvent.eventStatus |= RD53EvtEncoder::CHIPHEAD;

    const size_t noHitToT = RD53Shared::setBits(RD53AEvtEncoder::NBIT_TOT * RD53Constants::NPIX_REGION);

    for(auto i = 1u; i < size; i++)
    {
        if(data[i] != noHitToT)
        {
            uint32_t core_col, side, row, col, all_tots;

            std::tie(core_col, row, side, all_tots) =
                bits::unpack<RD53AEvtEncoder::NBIT_CCOL, RD53AEvtEncoder::NBIT_ROW, RD53AEvtEncoder::NBIT_SIDE, RD53AEvtEncoder::NBIT_TOT * RD53Constants::NPIX_REGION>(data[i]);
            col = RD53Constants::NPIX_REGION * bits::pack<RD53AEvtEncoder::NBIT_CCOL, RD53AEvtEncoder::NBIT_SIDE>(core_col, side);

            uint8_t tots[RD53Constants::NPIX_REGION];
            bits::RangePacker<RD53AEvtEncoder::NBIT_TOT>::unpack_reverse(all_tots, tots);

            for(int j = 0; j < RD53Constants::NPIX_REGION; j++)
                if(tots[j] != RD53Shared::setBits(RD53AEvtEncoder::NBIT_TOT)) chipEvent.hit_data.push_back({(uint16_t)row, (uint16_t)(col + j), tots[j]});
            if((row >= RD53A::NROWS) || (col >= (RD53A::NCOLS - (RD53Constants::NPIX_REGION - 1)))) chipEvent.eventStatus |= RD53EvtEncoder::CHIPPIX;
        }
    }

    // ########################################################
    // # If the number of 32bit words do not make an integer  #
    // # number of 128bit words, then special words are added #
    // # to the event                                         #
    // ########################################################
    if(size == 1) chipEvent.eventStatus |= RD53EvtEncoder::CHIPNOHIT;
}

uint32_t RD53A::getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const
{
    return bits::pack<4, 1, 3, 6, 1, 5>(RD53AConstants::BROADCAST_CHIPID, cal_edge_mode, cal_edge_delay, cal_edge_width, cal_aux_mode, cal_aux_delay);
}

float RD53A::VCal2Charge(float VCal, bool isNoise) const
{
    return (RD53AchargeConvertion::Vref / RD53AchargeConvertion::ADCrange) * VCal / RD53AchargeConvertion::ele * RD53AchargeConvertion::cap * 1e4 +
           (isNoise == false ? RD53AchargeConvertion::offset : 0);
}

float RD53A::Charge2VCal(float Charge) const
{
    return (Charge - RD53AchargeConvertion::offset) / (RD53AchargeConvertion::cap * 1e4) * RD53AchargeConvertion::ele / (RD53AchargeConvertion::Vref / RD53AchargeConvertion::ADCrange);
}

} // namespace Ph2_HwDescription
