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
constexpr RD53A::FrontEnd RD53A::SYNC;
constexpr RD53A::FrontEnd RD53A::LIN;
constexpr RD53A::FrontEnd RD53A::DIFF;
const RD53A::FrontEnd*    RD53A::frontEnds[] = {&RD53A::SYNC, &RD53A::LIN, &RD53A::DIFF};

const RD53A::FrontEnd* RD53A::getMajorityFE(size_t colStart, size_t colStop) const
{
    return *std::max_element(std::begin(frontEnds), std::end(frontEnds), [&](const FrontEnd* a, const FrontEnd* b) {
        return int(std::min(colStop, a->colStop)) - int(std::max(colStart, a->colStart)) < int(std::min(colStop, b->colStop)) - int(std::max(colStart, b->colStart));
    });
}

void RD53A::Event::DecodeQuad(uint32_t data)
{
    uint32_t core_col, side, row, col, all_tots;

    std::tie(core_col, row, side, all_tots) = bits::unpack<RD53EvtEncoder::NBIT_CCOL, RD53EvtEncoder::NBIT_ROW, RD53EvtEncoder::NBIT_SIDE, RD53EvtEncoder::NBIT_TOT>(data);
    col                                     = RD53Constants::NPIX_REGION * bits::pack<RD53EvtEncoder::NBIT_CCOL, RD53EvtEncoder::NBIT_SIDE>(core_col, side);

    uint8_t tots[RD53Constants::NPIX_REGION];
    bits::RangePacker<RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION>::unpack_reverse(all_tots, tots);

    for(int i = 0; i < RD53Constants::NPIX_REGION; i++)
        if(tots[i] != RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION)) hit_data.emplace_back(row, col + i, tots[i]);
    if((row >= RD53::nRows) || (col >= (RD53::nCols - (RD53Constants::NPIX_REGION - 1)))) eventStatus |= RD53EvtEncoder::CHIPPIX;
}

RD53A::Event::Event(const uint32_t* data, size_t n)
{
    uint32_t header;

    eventStatus = RD53EvtEncoder::CHIPGOOD;

    std::tie(header, trigger_id, trigger_tag, bc_id) = bits::unpack<RD53EvtEncoder::NBIT_HEADER, RD53EvtEncoder::NBIT_TRIGID, RD53EvtEncoder::NBIT_TRGTAG, RD53EvtEncoder::NBIT_BCID>(*data);
    if(header != RD53EvtEncoder::HEADER) eventStatus |= RD53EvtEncoder::CHIPHEAD;

    const size_t noHitToT = RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT);
    for(auto i = 1u; i < n; i++)
        if(data[i] != noHitToT) DecodeQuad(data[i]);
    // #######################################################
    // # If the number of 32bit words do not make an integer #
    // # number of 128bit words, then 0x0000FFFF words are   #
    // # added to the event                                  #
    // #######################################################
    if(n == 1) eventStatus |= RD53EvtEncoder::CHIPNOHIT;
}

} // namespace Ph2_HwDescription
