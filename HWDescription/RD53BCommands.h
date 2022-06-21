#ifndef RD53BCOMMANDS_H
#define RD53BCOMMANDS_H

#include "../Utils/BitSerialization/BitVector.h"

namespace RD53BCmd
{
struct PLLlock
{
};

struct Sync
{
};

struct Clear
{
    uint8_t chip_id;
};

struct GlobalPulse
{
    uint8_t chip_id;
};

struct WrReg
{
    uint8_t  chip_id;
    uint16_t address;
    uint16_t value;
};

struct WrRegLong
{
    uint8_t               chip_id;
    std::vector<uint16_t> values;
};

struct RdReg
{
    uint8_t  chip_id;
    uint16_t address;
};

struct Cal
{
    uint8_t chip_id;
    bool    mode;
    uint8_t edge_delay;
    uint8_t edge_duration;
    bool    aux_enable;
    uint8_t aux_delay;
};

struct Trigger
{
    uint8_t pattern;
    uint8_t tag;
};

template <class Cmd>
extern void serialize(Cmd&&, std::vector<uint16_t>&);

} // namespace RD53BCmd

#endif
