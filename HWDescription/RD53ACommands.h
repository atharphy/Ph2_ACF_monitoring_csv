/*!
  \file                  RD53ACommands.h
  \brief                 RD53ACommands description
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53ACommands_H
#define RD53ACommands_H

#include "../Utils/bit_packing.h"

#include <cstdint>
#include <vector>

namespace RD53Cmd
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
namespace RD53CmdEncoder
{
const uint16_t RESET_ECR  = 0x5A5A; // Event Counter Reset word
const uint16_t RESET_BCR  = 0x5959; // Bunch Counter Reset word
const uint16_t GLOB_PULSE = 0x5C5C; // Global pulse word
const uint16_t CAL        = 0x6363; // Calibration word
const uint16_t WRITE      = 0x6666; // Write command word
const uint16_t READ       = 0x6565; // Read command word
const uint16_t NOOP       = 0x6969; // No operation word
const uint16_t SYNC       = 0x817E; // Synchronization word
} // namespace RD53CmdEncoder

// template <uint16_t cmdCode, size_t nFields>
// class Command
// {
//     static_assert(nFields % 2 == 0, "RD53Cmd::Command: a command must have an even number of fields");

//   public:
//     static constexpr size_t cmdCode = cmdCode;
//     static constexpr size_t nFields = nFields;

//     std::array<uint8_t, nFields> serializeFields() const { return std::array<uint8_t, nFields>(); }

//   protected:
//     template <int... Sizes, class... Args>
//     uint8_t packAndEncode(Args&&... args)
//     {
//         return map5to8bit[bits::pack<Sizes...>(std::forward<Args>(args)...)];
//     }
// };

template <int... Sizes, class... Args>
uint8_t packAndEncode(Args&&... args)
{
    return map5to8bit[bits::pack<Sizes...>(std::forward<Args>(args)...)];
}

template <class T>
auto serializeFields(const T& cmd)
{
    return std::array<uint8_t, 0>();
}

template <class cmdType>
void serialize(const cmdType& cmd, std::vector<uint16_t>& frameVector)
{
    // Insert command code
    frameVector.push_back(cmdType::cmdCode);

    auto fields = cmd.serializeFields();

    // Insert: chip id, address and data
    for(auto i = 1; i < static_cast<int>(cmdType::nFields); i += 2) frameVector.push_back(bits::pack<8, 8>(fields[i - 1], fields[i]));
}

template <class cmdType>
std::vector<uint16_t> getFrames(const cmdType& cmd)
{
    std::vector<uint16_t> frameVector;

    frameVector.reserve(1 + cmdType::nFields / 2);
    serialize(cmd, frameVector);

    return frameVector;
}

struct ECR
{
    static const uint16_t cmdCode = RD53CmdEncoder::RESET_ECR;
    static const uint16_t nFields = 0;
};

struct BCR
{
    static const uint16_t cmdCode = RD53CmdEncoder::RESET_BCR;
    static const uint16_t nFields = 0;
};

struct NoOp
{
    static const uint16_t cmdCode = RD53CmdEncoder::NOOP;
    static const uint16_t nFields = 0;
};

struct Sync
{
    static const uint16_t cmdCode = RD53CmdEncoder::SYNC;
    static const uint16_t nFields = 0;
};

struct GlobalPulse
{
    static const uint16_t cmdCode = RD53CmdEncoder::SYNC;
    static const uint16_t nFields = 2;
    
    uint8_t chip_id;
    uint8_t data;
};

std::array<uint8_t, GlobalPulse::nFields> serializeFields(const GlobalPulse& cmd);

struct Cal
{
    static const uint16_t cmdCode = RD53CmdEncoder::CAL;
    static const uint16_t nFields = 4;

    uint8_t chip_id;
    bool    cal_edge_mode;
    uint8_t cal_edge_delay;
    uint8_t cal_edge_width;
    bool    cal_aux_mode;
    uint8_t cal_aux_delay;
};

std::array<uint8_t, Cal::nFields> serializeFields(const Cal& cmd);

struct WrReg
{
    static const uint16_t cmdCode = RD53CmdEncoder::WRITE;
    static const uint16_t nFields = 6;

    uint8_t  chip_id;
    uint16_t address;
    uint16_t value;
};

std::array<uint8_t, WrReg::nFields> serializeFields(const WrReg& cmd);

struct WrRegLong
{
    static const uint16_t cmdCode = RD53CmdEncoder::WRITE;
    static const uint16_t nFields = 22;

    uint8_t               chip_id;
    uint16_t              address;
    std::vector<uint16_t> values;

    std::array<uint8_t, nFields> serializeFields() const;
};

std::array<uint8_t, WrRegLong::nFields> serializeFields(const WrRegLong& cmd);

struct RdReg
{
    static const uint16_t cmdCode = RD53CmdEncoder::READ;
    static const uint16_t nFields = 4;

    uint8_t  chip_id;
    uint16_t address;
};

std::array<uint8_t, RdReg::nFields> serializeFields(const RdReg& cmd);

} // namespace RD53Cmd

#endif
