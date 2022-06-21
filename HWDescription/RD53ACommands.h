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

#include <cstdint>

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

template <uint16_t cmdCode, size_t nFields>
class Command
{
    static_assert(nFields % 2 == 0, "RD53Cmd::Command: a command must have an even number of fields");

  public:
    static constexpr size_t nFields = nFields;
    static constexpr size_t cmdCode = cmdCode;

  protected:
    template <int... Sizes, class... Args>
    uint8_t packAndEncode(Args&&... args)
    {
        return map5to8bit[bits::pack<Sizes...>(std::forward<Args>(args)...)];
    }
};

template <class cmdType>
void serialize(const cmdType& cmd, std::vector<uint16_t>& frameVector) const
{
    // Insert command code
    frameVector.push_back(cmdType::cmdCode);

    if constexpr(nFields != 0)
    {
        auto fields = cmd.serializeFields();

        // Insert: chip id, address and data
        for(auto i = 1; i < static_cast<int>(cmdType::nFields); i += 2) frameVector.push_back(bits::pack<8, 8>(fields[i - 1], fields[i]));
    }
}

template <class cmdType>
std::vector<uint16_t> getFrames(const cmdType& cmd) const
{
    std::vector<uint16_t> frameVector;

    frameVector.reserve(1 + cmdType::nFields / 2);
    Command::serialize(cmd, frameVector);

    return frameVector;
}

struct ECR : public Command<RD53CmdEncoder::RESET_ECR, 0>
{
};

struct BCR : public Command<RD53CmdEncoder::RESET_BCR, 0>
{
};

struct NoOp : public Command<RD53CmdEncoder::NOOP, 0>
{
};

struct Sync : public Command<RD53CmdEncoder::SYNC, 0>
{
};

struct GlobalPulse : public Command<RD53CmdEncoder::GLOB_PULSE, 2>
{
    uint8_t chip_id;
    uint8_t data;

    std::array<uint8_t, nFields> serializeFields() const;
};

struct Cal : public Command<RD53CmdEncoder::CAL, 4>
{
    uint8_t chip_id;
    bool    cal_edge_mode;
    uint8_t cal_edge_delay;
    uint8_t cal_edge_width;
    bool    cal_aux_mode;
    uint8_t cal_aux_delay;

    std::array<uint8_t, nFields> serializeFields() const;
};

struct WrReg : public Command<RD53CmdEncoder::WRITE, 6>
{
    uint8_t  chip_id;
    uint16_t address;
    uint16_t value;

    std::array<uint8_t, nFields> serializeFields() const;
};

struct WrRegLong : public Command<RD53CmdEncoder::WRITE, 22>
{
    uint8_t               chip_id;
    uint16_t              address;
    std::vector<uint16_t> values;

    std::array<uint8_t, nFields> serializeFields() const;
};

struct RdReg : public Command<RD53CmdEncoder::READ, 4>
{
    uint8_t  chip_id;
    uint16_t address;

    std::array<uint8_t, nFields> serializeFields() const;
};

} // namespace RD53ACmd

#endif
