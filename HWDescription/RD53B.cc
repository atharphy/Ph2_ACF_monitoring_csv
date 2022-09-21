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

using namespace Ph2_HwInterface;

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

uint32_t RD53B::getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const
{
    return bits::pack<1, 5, 8, 1, 5>(cal_edge_mode, cal_edge_delay, cal_edge_width, cal_aux_mode, cal_aux_delay);
}

template <class T>
size_t decodeCompressedBitpair(BitView<T>& bits)
{
    if(bits.pop(1) == 0) return 1;
    return 2 | bits.pop(1);
}

template <class T>
auto decodeCompressedHitmap(BitView<T>& bits)
{
    std::array<std::array<bool, 8>, 2> hits{{0}};

    auto row_mask = decodeCompressedBitpair(bits);

    for(size_t row = 0; row < 2; row++)
    {
        if(row_mask & (2 >> row))
        {
            auto quad_mask = decodeCompressedBitpair(bits);

            std::vector<size_t> pair_masks;
            for(size_t i = 0; i < __builtin_popcount(quad_mask); i++)
            {
                auto pair_mask = decodeCompressedBitpair(bits);
                pair_masks.push_back(pair_mask);
            }

            int current_quad = 0;
            for(int pixel_quad = 0; pixel_quad < 2; pixel_quad++)
            {
                if(quad_mask & (2 >> pixel_quad))
                {
                    for(int pixel_pair = 0; pixel_pair < 2; pixel_pair++)
                    {
                        if(pair_masks[current_quad] & (2 >> pixel_pair))
                        {
                            size_t pixel_mask                              = decodeCompressedBitpair(bits);
                            hits[row][pixel_quad * 4 + pixel_pair * 2]     = pixel_mask & 2;
                            hits[row][pixel_quad * 4 + pixel_pair * 2 + 1] = pixel_mask & 1;
                        }
                    }

                    current_quad++;
                }
            }
        }
    }

    return hits;
}

// ###########################################
// # Functions needed for decoding chip data #
// ###########################################

void decodeStreamHeader(BitView<const uint32_t>& bits, RD53ChipEvent& e, const FormatOptions& options)
{
    if(options.enableBCID) e.bc_id = bits.pop(RD53BEvtEncoder::NBIT_BCID);
    if(options.enableTriggerId) e.trigger_id = bits.pop(RD53BEvtEncoder::NBIT_TRIGID);
}

void decodeChipId(uint8_t chipId, size_t i, RD53ChipEvent& e, size_t nWords)
{
    if(i == 0)
        e.chip_id_mod4 = chipId;
    else if(e.chip_id_mod4 != chipId)
        e.eventStatus |= RD53EvtEncoder::CHIPID;
}

auto decodeEventStream(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    BitVector<uint32_t> payloadData;
    size_t              nWords = bits.size() / 64;
    bool                isLast = false;

    for(size_t i = 0; i < nWords && !isLast; i++)
    {
        isLast = bits.pop(1);

        if(isLast == true)
        {
            if(i + 2 < nWords)
            {
                e.eventStatus |= RD53EvtEncoder::CHIPEOS;
                break;
            }
        }
        else if(i == nWords)
        {
            e.eventStatus |= RD53EvtEncoder::CHIPEOS;
            break;
        }

        if(options.enableChipId) decodeChipId(bits.pop(RD53BEvtEncoder::NBIT_CHIPID), i, e, nWords);

        payloadData.append(bits.pop_slice(63 - 2 * options.enableChipId));
    }

    return payloadData;
}

void RD53B::decodeChipData(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    std::array<int, RD53B::NCOLS / RD53Constants::NROW_CORE> last_qrow;
    const auto                                               eventStream     = decodeEventStream(bits, e, options);
    auto                                                     eventStreamView = bit_view(eventStream);

    if(eventStreamView.size() == 0)
    {
        e.eventStatus = RD53FWEvtEncoder::MISSCHIP;
        return;
    }
    if((e.eventStatus & RD53EvtEncoder::CHIPEOS) != 0) return;

    decodeStreamHeader(eventStreamView, e, options);
    e.trigger_tag = eventStreamView.pop(RD53BEvtEncoder::NBIT_TRGTAG);

    // ##################
    // # Decode hit map #
    // ##################
    while(true)
    {
        // ##########################
        // # End-of-data conditions #
        // ##########################
        if(eventStreamView.size() < 6) return; // Good end of chip data
        size_t ccol = eventStreamView.pop(RD53BEvtEncoder::NBIT_CCOL);
        if(ccol == 0) return; // Good end of chip data

        if(RD53Constants::NROW_CORE * (ccol - 1) >= RD53B::NCOLS) e.eventStatus |= RD53EvtEncoder::CHIPPIX;

        bool isLast = false;
        while(isLast == false)
        {
            isLast              = eventStreamView.pop(1);
            size_t qrow         = eventStreamView.pop(1) ? last_qrow[ccol - 1] + 1 : eventStreamView.pop(8);
            last_qrow[ccol - 1] = qrow;

            if(2 * qrow >= RD53B::NROWS) e.eventStatus |= RD53EvtEncoder::CHIPPIX;

            auto hitmap = decodeCompressedHitmap(eventStreamView);
            for(size_t row = 0; row < 2; row++)
                for(size_t col = 0; col < RD53Constants::NROW_CORE; col++)
                    if(hitmap[row][col])
                    {
                        uint8_t tot = 0;
                        if(options.enableToT == true)
                        {
                            tot = eventStreamView.pop(RD53BEvtEncoder::NBIT_TOT);
                            if(tot == RD53Shared::setBits(RD53BEvtEncoder::NBIT_TOT)) e.eventStatus |= RD53EvtEncoder::CHIPTOT;
                        }

                        e.hit_data.emplace_back(qrow * 2 + row, (ccol - 1) * RD53Constants::NROW_CORE + col, tot);
                    }
        }
    }
}

} // namespace Ph2_HwDescription
