/*!
  \file                  RD53BCommands.h
  \brief                 RD53BCommands description
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BCOMMANDS_H
#define RD53BCOMMANDS_H

#include "../Utils/BitMaster/bit_packing.h"

#include <vector>

namespace RD53BCmd
{
// Map 5-bit to 8-bit fields
constexpr uint8_t map5to8bit[] = {
    0x6A, // 00: 0b01101010,
    0x6C, // 01: 0b01101100,
    0x71, // 02: 0b01110001,
    0x72, // 03: 0b01110010,
    0x74, // 04: 0b01110100,
    0x8B, // 05: 0b10001011,
    0x8D, // 06: 0b10001101,
    0x8E, // 07: 0b10001110,
    0x93, // 08: 0b10010011,
    0x95, // 09: 0b10010101,
    0x96, // 10: 0b10010110,
    0x99, // 11: 0b10011001,
    0x9A, // 12: 0b10011010,
    0x9C, // 13: 0b10011100,
    0xA3, // 14: 0b10100011,
    0xA5, // 15: 0b10100101,
    0xA6, // 16: 0b10100110,
    0xA9, // 17: 0b10101001,
    0xAA, // 18: 0b10101010,
    0xAC, // 19: 0b10101100,
    0xB1, // 20: 0b10110001,
    0xB2, // 21: 0b10110010,
    0xB4, // 22: 0b10110100,
    0xC3, // 23: 0b11000011,
    0xC5, // 24: 0b11000101,
    0xC6, // 25: 0b11000110,
    0xC9, // 26: 0b11001001,
    0xCA, // 27: 0b11001010,
    0xCC, // 28: 0b11001100,
    0xD1, // 29: 0b11010001,
    0xD2, // 30: 0b11010010,
    0xD4  // 31: 0b11010100
};

// ############
// # Commands #
// ############
namespace RD53BCmdEncoder
{
const uint16_t CAL        = 0x63;   // Calibration word
const uint16_t READ       = 0x65;   // Read command word
const uint16_t WRITE      = 0x66;   // Write command word
const uint16_t GLOB_PULSE = 0x5C;   // Global pulse word
const uint16_t CLEAR      = 0x5A;   // Clear word
const uint16_t SYNC       = 0x817E; // Synchronization word
const uint16_t PLLLOCK    = 0xAAAA; // PLL lock word
} // namespace RD53BCmdEncoder

template <class CmdType>
std::vector<uint8_t> serializeFields(const CmdType&, std::vector<uint16_t>&)
{
    return {};
}

struct PLLlock
{
    static constexpr uint16_t cmdCode() { return RD53BCmdEncoder::PLLLOCK; }
};

struct Sync
{
    static constexpr uint16_t cmdCode() { return RD53BCmdEncoder::SYNC; }
};

struct Clear
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::CLEAR; }

    uint8_t chip_id;
};

struct GlobalPulse
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::GLOB_PULSE; }

    uint8_t chip_id;
};

struct WrReg
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::WRITE; }

    uint8_t  chip_id;
    uint16_t address;
    uint16_t value;
};

std::vector<uint8_t> serializeFields(const WrReg&, std::vector<uint16_t>&);

struct WrRegLong
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::WRITE; }

    uint8_t               chip_id;
    std::vector<uint16_t> values;
};

std::vector<uint8_t> serializeFields(const WrRegLong&, std::vector<uint16_t>&);

struct RdReg
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::READ; }

    uint8_t  chip_id;
    uint16_t address;
};

std::vector<uint8_t> serializeFields(const RdReg&, std::vector<uint16_t>&);

struct Cal
{
    static constexpr uint8_t cmdCode() { return RD53BCmdEncoder::CAL; }

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

} // namespace RD53BCmd

#endif
