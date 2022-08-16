/*!
  \file                  RD53BEventDecoding.cc
  \brief                 RD53BEventDecoding class implementation
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53BEventDecoding.h"
#include "BitMaster/BitVector.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace RD53BEventDecoding
{
template <class T>
size_t decode_compressed_bitpair(BitView<T>& bits)
{
    if(bits.pop(1) == 0) return 1;
    return 2 | bits.pop(1);
}

template <class T>
auto decode_compressed_hitmap(BitView<T>& bits)
{
    std::array<std::array<bool, 8>, 2> hits{{0}};

    auto row_mask = decode_compressed_bitpair(bits);

    for(size_t row = 0; row < 2; row++)
    {
        if(row_mask & (2 >> row))
        {
            auto quad_mask = decode_compressed_bitpair(bits);

            std::vector<size_t> pair_masks;
            for(size_t i = 0; i < __builtin_popcount(quad_mask); ++i)
            {
                auto pair_mask = decode_compressed_bitpair(bits);
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
                            size_t pixel_mask                              = decode_compressed_bitpair(bits);
                            hits[row][pixel_quad * 4 + pixel_pair * 2]     = pixel_mask & 2;
                            hits[row][pixel_quad * 4 + pixel_pair * 2 + 1] = pixel_mask & 1;
                        }
                    }
                    ++current_quad;
                }
            }
        }
    }

    return hits;
}

void decode_stream_header(BitView<const uint32_t>& bits, RD53ChipEvent& e, const FormatOptions& options)
{
    if(options.enableBCID) e.bc_id = bits.pop(11);
    if(options.enableTriggerId) e.trigger_id = bits.pop(8);
}

void decode_chip_id(uint8_t chipId, size_t i, RD53ChipEvent& e, size_t n_words)
{
    if(i == 0)
        e.chip_id_mod4 = chipId;
    else if(e.chip_id_mod4 != chipId)
        throw std::runtime_error("Found conflicting chip ID: " + std::to_string(chipId) + " (previously " + std::to_string(e.chip_id_mod4) + ") @ word # " + std::to_string(i) + " / " +
                                 std::to_string(n_words));
}

BitVector<uint32_t> decode_event_stream(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    BitVector<uint32_t> payload_data;
    size_t              n_words = bits.size() / 64;
    bool                isLast  = false;

    for(size_t i = 0; i < n_words && !isLast; i++)
    {
        isLast = bits.pop(1);

        if(isLast == true)
        {
            if(i + 2 < n_words) { throw std::runtime_error("The end-of-stream bit was 1 before the last word of the event stream"); }
        }
        else if(i == n_words)
            throw std::runtime_error("The end-of-stream bit was 0 in the last word of the event stream");

        if(options.enableChipId) decode_chip_id(bits.pop(2), i, e, n_words);

        payload_data.append(bits.pop_slice(63 - 2 * options.enableChipId));
    }
    return payload_data;
}

void decode_chip_event(BitView<const uint32_t> bits, RD53ChipEvent& e, const FormatOptions& options)
{
    const auto event_stream      = decode_event_stream(bits, e, options);
    auto       event_stream_view = bit_view(event_stream);

    decode_stream_header(event_stream_view, e, options);

    e.trigger_tag = event_stream_view.pop(8);

    std::array<int, RD53B::NCOLS / 8> last_qrow;

    while(true)
    {
        if(event_stream_view.size() < 6) return;
        size_t ccol = event_stream_view.pop(6);
        if(ccol == 0) return;

        if(8 * (ccol - 1) >= RD53B::NCOLS) throw std::runtime_error("Invalid column: " + std::to_string(8 * (ccol - 1)));

        bool isLast = false;
        while(!isLast)
        {
            isLast      = event_stream_view.pop(1);
            size_t qrow = event_stream_view.pop(1) ? last_qrow[ccol - 1] + 1 : event_stream_view.pop(8);

            if(2 * qrow >= RD53B::NROWS) throw std::runtime_error("Invalid row: " + std::to_string(2 * qrow));

            last_qrow[ccol - 1] = qrow;

            auto hitmap = decode_compressed_hitmap(event_stream_view);
            for(size_t row = 0; row < 2; row++)
                for(size_t col = 0; col < 8; col++)
                    if(hitmap[row][col])
                    {
                        uint8_t tot = 0;
                        if(options.enableToT == true)
                        {
                            tot = event_stream_view.pop(4);
                            if(tot == 15) throw std::runtime_error("Invalid tot value: 15");
                        }
                        e.hit_data.emplace_back(qrow * 2 + row, (ccol - 1) * 8 + col, tot);
                    }
        }
    }
}

size_t decode_events(const std::vector<uint32_t>& data, std::vector<RD53Event>& events, const FormatOptions& options)
{
    auto   bits    = bit_view(data);
    size_t nEvents = 0;

    while(bits.size())
    {
        if(bits.pop(16) != 0xFFFF) throw std::runtime_error("Invalid event container header");

        RD53Event evt;

        size_t block_size  = bits.pop(16);
        evt.tlu_trigger_id = bits.pop(16);
        bits.skip(8); // trigger_tag
        size_t dummy_size = bits.pop(8);
        evt.tdc           = bits.pop(8);
        evt.l1a_counter   = bits.pop(24);
        evt.bx_counter    = bits.pop(32);

        auto event_bits = bits.pop_slice(128 * (block_size - 1 - dummy_size));

        while(event_bits.size())
        {
            if(event_bits.pop(4) != 0xA) throw std::runtime_error("Invalid event header");

            RD53ChipEvent chipEvt;

            event_bits.skip(4); // error_code
            chipEvt.hybrid_id = event_bits.pop(8);
            chipEvt.chip_lane = event_bits.pop(4);
            size_t l1a_size   = event_bits.pop(12);
            event_bits.skip(16); // padding
            event_bits.skip(4);  // chip_type
            event_bits.skip(12); // frame_delay

            decode_chip_event(event_bits.pop_slice(l1a_size * 128 - 64), chipEvt, options);
            evt.chip_events.push_back(std::move(chipEvt));
        }

        bits.skip(128 * dummy_size);
        events.push_back(std::move(evt));
        ++nEvents;
    }

    return nEvents;
}

size_t count_events(const std::vector<uint32_t>& data)
{
    auto   bits = bit_view(data);
    size_t n    = 0;

    while(bits.size())
    {
        if(bits.pop(16) != 0xFFFF) break;
        size_t block_size = bits.pop(16);
        bits.skip(128 * block_size - 32);
        ++n;
    }

    return n;
}

} // namespace RD53BEventDecoding
