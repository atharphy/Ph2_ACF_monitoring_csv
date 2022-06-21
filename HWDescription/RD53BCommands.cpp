#include "../Utils/BitSerialization/Types/All.hpp"

#include "RD53BCommands.h"

#include <sstream>


namespace RD53BCmd {

using namespace BitSerialization;


struct ByteTransform {
    constexpr ByteTransform(std::initializer_list<uint8_t> encoding) 
        : size(encoding.size())
        , encoding_array{{0}}
        , decoding_array{{0}}
    {
        std::copy(encoding.begin(), encoding.end(), std::begin(encoding_array));
        for (size_t i = 0; i < size; ++i)
            decoding_array[encoding_array[i]] = i;
        for (size_t i = size; i < 256; ++i)
            decoding_array[encoding_array[i]] = size;
    }

    uint8_t encode(uint8_t value) const { return encoding_array[value]; }
    uint8_t decode(uint8_t value) const { return decoding_array[value]; }

    size_t size;
    uint8_t encoding_array[256];
    uint8_t decoding_array[256];
};

constexpr ByteTransform FieldEncodingTransform({
    0b01101010,
    0b01101100,
    0b01110001,
    0b01110010,
    0b01110100,
    0b10001011,
    0b10001101,
    0b10001110,
    0b10010011,
    0b10010101,
    0b10010110,
    0b10011001,
    0b10011010,
    0b10011100,
    0b10100011,
    0b10100101,
    0b10100110,
    0b10101001,
    0b01011001,
    0b10101100,
    0b10110001,
    0b10110010,
    0b10110100,
    0b11000011,
    0b11000101,
    0b11000110,
    0b11001001,
    0b11001010,
    0b11001100,
    0b11010001,
    0b11010010,
    0b11010100
});

constexpr ByteTransform TriggerPatternTransform({
    0b00101011,
    0b00101101,
    0b00101110,
    0b00110011,
    0b00110101,
    0b00110110,
    0b00111001,
    0b00111010,
    0b00111100,
    0b01001011,
    0b01001101,
    0b01001110,
    0b01010011,
    0b01010101,
    0b01010110
});

constexpr ByteTransform TriggerTagTransform({
    0b01101010,
    0b01101100,
    0b01110001,
    0b01110010,
    0b01110100,
    0b10001011,
    0b10001101,
    0b10001110,
    0b10010011,
    0b10010101,
    0b10010110,
    0b10011001,
    0b10011010,
    0b10011100,
    0b10100011,
    0b10100101,
    0b10100110,
    0b10101001,
    0b01011001,
    0b10101100,
    0b10110001,
    0b10110010,
    0b10110100,
    0b11000011,
    0b11000101,
    0b11000110,
    0b11001001,
    0b11001010,
    0b11001100,
    0b11010001,
    0b11010010,
    0b11010100,
    0b01100011,
    0b01011010,
    0b01011100,
    0b10101010,
    0b01100101,
    0b01101001,
    0b00101011,
    0b00101101,
    0b00101110,
    0b00110011,
    0b00110101,
    0b00110110,
    0b00111001,
    0b00111010,
    0b00111100,
    0b01001011,
    0b01001101,
    0b01001110,
    0b01010011,
    0b01010101,
    0b01010110,
    0b01100110
});


template <class Cmd>
struct command_encoding;

template <class Cmd>
using command_encoding_t = command_encoding<Cmd>::type;

template <size_t OpCode, class... Fields>
using LongCommand = NamedTuple<
    NamedField<"opCode", Constant<Uint<8>, OpCode>>,
    NamedField<"fields", Compose3<
        ClassObject<Fields...>,
        Many<Uint<5>>,
        Many<
            Validated<
                Transformed<Uint<8>, FieldEncodingTransform>,
                [] (const auto&, const auto& value) { 
                    return value < FieldEncodingTransform.size;
                }
            >
        >
    >>
>;

template <>
struct command_encoding<Clear> {
    using type = LongCommand<0b001011010,
        Field<&Clear::chip_id, Uint<5>>
    >;
};

template <>
struct command_encoding<GlobalPulse> {
    using type = LongCommand<0b01011100,
        Field<&GlobalPulse::chip_id, Uint<5>>
    >;
};

template <>
struct command_encoding<WrReg> {
    using type = LongCommand<0b01100110,
        Field<&WrReg::chip_id, Uint<5>>,
        Field<Discarded{}, Constant<Bool, 0>>,
        Field<&WrReg::address, Uint<9>>,
        Field<&WrReg::value, Uint<16>>,
        Field<Discarded{}, Constant<Uint<4>, 0>>
    >;
};

template <>
struct command_encoding<WrRegLong> {
    using type = LongCommand<0b01100110, 
        Field<&WrRegLong::chip_id, Uint<5>>,
        Field<Discarded{}, Constant<Uint<10>, 0b1000000000>>,
        Field<&WrRegLong::values, Many<Uint<10>>>
    >;
};

template <>
struct command_encoding<RdReg> {
    using type = LongCommand<0b01100101, 
        Field<&RdReg::chip_id, Uint<5>>,
        Field<Discarded{}, Constant<Bool, 0>>,
        Field<&RdReg::address, Uint<9>>
    >;
};

template <>
struct command_encoding<Cal> {
    using type = LongCommand<0b01100011,
        Field<&Cal::chip_id, Uint<5>>,
        Field<&Cal::mode, Bool>,
        Field<&Cal::edge_delay, Uint<5>>,
        Field<&Cal::edge_duration, Uint<8>>,
        Field<&Cal::aux_enable, Bool>,
        Field<&Cal::aux_delay, Uint<5>>
    >;
};

template <>
struct command_encoding<Trigger> {
    using type = ClassObject<
        Field<&Trigger::pattern, Transformed<Uint<8>, TriggerPatternTransform>>,
        Field<&Trigger::tag, Transformed<Uint<8>, TriggerTagTransform>>
    >;
};

template <>
struct command_encoding<Sync> {
    using type = Constant<Uint<16>, 0b1000000101111110>;
};

template <>
struct command_encoding<PLLlock> { 
    using type = Constant<Uint<16>, 0b1010101010101010>;
};

template <class Cmd>
void serialize(Cmd&& cmd, std::vector<uint16_t>& bits) {
    using encoding = command_encoding_t<std::remove_reference_t<Cmd>>;
    auto serialize = [&] (auto&& value) {
        auto result = encoding::serialize(value, bits);
        if (!result) {
            std::stringstream ss;
            ss << "Command serializer error: " << result.error();
            throw std::runtime_error(ss.str());
        }
    };
    if constexpr (ignores_input_value_v<encoding>)
        serialize(value_type_t<encoding>{});
    else
        serialize(value_type_t<encoding>{std::forward<Cmd>(cmd)});
}

#define INSTANTIATE_SERIALIZE_FUNC(Cmd) \
template void serialize<Cmd>(Cmd&&, BitVector<uint16_t>&); \
template void serialize<Cmd&>(Cmd&, BitVector<uint16_t>&);

INSTANTIATE_SERIALIZE_FUNC(Sync)
INSTANTIATE_SERIALIZE_FUNC(PLLlock)
INSTANTIATE_SERIALIZE_FUNC(Clear)
INSTANTIATE_SERIALIZE_FUNC(GlobalPulse)
INSTANTIATE_SERIALIZE_FUNC(WrReg)
INSTANTIATE_SERIALIZE_FUNC(WrRegLong)
INSTANTIATE_SERIALIZE_FUNC(RdReg)
INSTANTIATE_SERIALIZE_FUNC(Cal)
INSTANTIATE_SERIALIZE_FUNC(Trigger)

}
