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

    ChannelGroupBase* getChannelGroup() const override { return new ChannelGroup<RD53A::NROWS, RD53A::NCOLS>; }
    ChannelGroupBase* getChannelGroupAll() const override { return new RD53ChannelGroupHandler::RD53ChannelGroupAll<RD53A::NROWS, RD53A::NCOLS>; }
    ChannelGroupBase* getChannelGroupPattern(uint8_t hitPerCol) const override { return new RD53ChannelGroupHandler::RD53ChannelGroupPattern<RD53A::NROWS, RD53A::NCOLS>(hitPerCol); }
    size_t            getNRows() const override { return RD53A::NROWS; }
    size_t            getNCols() const override { return RD53A::NCOLS; }
    const FrontEnd*   getMajorityFE(size_t colStart, size_t colStop) const override;
    void decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent) const override
    {
    //       uint32_t header;

    // eventStatus = RD53EvtEncoder::CHIPGOOD;

    // std::tie(header, trigger_id, trigger_tag, bc_id) = bits::unpack<RD53EvtEncoder::NBIT_HEADER, RD53EvtEncoder::NBIT_TRIGID, RD53EvtEncoder::NBIT_TRGTAG, RD53EvtEncoder::NBIT_BCID>(*data);
    // if(header != RD53EvtEncoder::HEADER) eventStatus |= RD53EvtEncoder::CHIPHEAD;

    // const size_t noHitToT = RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT);
    // for(auto i = 1u; i < n; i++)
    //     if(data[i] != noHitToT)
    //     {
    //         uint32_t core_col, side, row, col, all_tots;

    // std::tie(core_col, row, side, all_tots) = bits::unpack<RD53EvtEncoder::NBIT_CCOL, RD53EvtEncoder::NBIT_ROW, RD53EvtEncoder::NBIT_SIDE, RD53EvtEncoder::NBIT_TOT>(data);
    // col                                     = RD53Constants::NPIX_REGION * bits::pack<RD53EvtEncoder::NBIT_CCOL, RD53EvtEncoder::NBIT_SIDE>(core_col, side);

    // uint8_t tots[RD53Constants::NPIX_REGION];
    // bits::RangePacker<RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION>::unpack_reverse(all_tots, tots);

    // for(int i = 0; i < RD53Constants::NPIX_REGION; i++)
    //     if(tots[i] != RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION)) hit_data.emplace_back(row, col + i, tots[i]);
    // if((row >= RD53A::NROWS) || (col >= (RD53A::NCOLS - (RD53Constants::NPIX_REGION - 1)))) eventStatus |= RD53EvtEncoder::CHIPPIX;
    // }
    // #######################################################
    // # If the number of 32bit words do not make an integer #
    // # number of 128bit words, then 0x0000FFFF words are   #
    // # added to the event                                  #
    // #######################################################
    // if(n == 1) eventStatus |= RD53EvtEncoder::CHIPNOHIT;
    }
};

} // namespace Ph2_HwDescription

#endif
