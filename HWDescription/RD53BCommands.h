/*!
  \file                  RD53BCommands.h
  \brief                 RD53BCommands description
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BCOMMANDS_H
#define RD53BCOMMANDS_H

#include "../Utils/bit_packing.h"
#include <sstream>
#include <vector>

namespace RD53BCmd
{

constexpr uint8_t map5to8bit[] = {
    0b01101010, 0b01101100, 0b01110001, 0b01110010, 0b01110100, 0b10001011, 0b10001101, 0b10001110, 0b10010011, 0b10010101, 0b10010110,
    0b10011001, 0b10011010, 0b10011100, 0b10100011, 0b10100101, 0b10100110, 0b10101001, 0b01011001, 0b10101100, 0b10110001, 0b10110010,
    0b10110100, 0b11000011, 0b11000101, 0b11000110, 0b11001001, 0b11001010, 0b11001100, 0b11010001, 0b11010010, 0b11010100
};

template <class CmdType>
std::vector<uint8_t> serializeFields(const CmdType&, std::vector<uint16_t>&) {
    return {};
}

struct PLLlock
{
    static constexpr uint16_t cmdCode = 0b1010101010101010;
};

struct Sync
{
    static constexpr uint16_t cmdCode = 0b1000000101111110;
};

struct Clear
{
    static constexpr uint8_t cmdCode = 0b001011010;

    uint8_t chip_id;
};

struct GlobalPulse
{
    static constexpr uint8_t cmdCode = 0b01011100;

    uint8_t chip_id;
};

struct WrReg
{
    static constexpr uint8_t cmdCode = 0b01100110;

    uint8_t  chip_id;
    uint16_t address;
    uint16_t value;
};

std::vector<uint8_t> serializeFields(const WrReg&, std::vector<uint16_t>&);

struct WrRegLong
{
    static constexpr uint8_t cmdCode = 0b01100110;

    uint8_t               chip_id;
    std::vector<uint16_t> values;
};

std::vector<uint8_t> serializeFields(const WrRegLong&, std::vector<uint16_t>&);

struct RdReg
{
    static constexpr uint8_t cmdCode = 0b01100101;

    uint8_t  chip_id;
    uint16_t address;
};

std::vector<uint8_t> serializeFields(const RdReg&, std::vector<uint16_t>&);

struct Cal
{
    static constexpr uint8_t cmdCode = 0b01100011;

    uint8_t chip_id;
    bool    mode;
    uint8_t edge_delay;
    uint8_t edge_duration;
    bool    aux_enable;
    uint8_t aux_delay;
};

std::vector<uint8_t> serializeFields(const Cal&, std::vector<uint16_t>&);

// struct Trigger
// {
//     uint8_t pattern;
//     uint8_t tag;
// };

template <int... Sizes, class... Args>
uint8_t packAndEncode(Args&&... args)
{
    return map5to8bit[bits::pack<Sizes...>(std::forward<Args>(args)...)];
}

template <class CmdType, std::enable_if_t<(CmdType::cmdCode > 0xFF), int> = 0>
void serialize(const CmdType& cmd, std::vector<uint16_t>& cmdStream)
{
    // Insert command code
    cmdStream.push_back(CmdType::cmdCode);
}

template <class CmdType, std::enable_if_t<(CmdType::cmdCode <= 0xFF), int> = 0>
void serialize(const CmdType& cmd, std::vector<uint16_t>& cmdStream)
{
    // Insert command code
    cmdStream.push_back(bits::pack<8, 8>(CmdType::cmdCode, packAndEncode<5>(cmd.chip_id)));

    auto fields = cmd.serializeFields();

    // Insert: chip id, address and data
    for(auto i = 1; i < static_cast<int>(CmdType::nFields); i += 2) cmdStream.push_back(bits::pack<8, 8>(fields[i - 1], fields[i]));
}

// void serialize(const Trigger&, std::vector<uint16_t>&);

} // namespace RD53BCmd

#endif
