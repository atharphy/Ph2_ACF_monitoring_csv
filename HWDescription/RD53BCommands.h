#ifndef RD53BCOMMANDS_H
#define RD53BCOMMANDS_H

#include "../Utils/Bits/BitVector.hpp"

namespace RD53BCmd {

struct PLLlock {};

struct Sync {};

struct Clear {
    uint8_t chip_id;
};

struct GlobalPulse {
    uint8_t chip_id;
};

struct WrReg {
    uint8_t chip_id;
    uint16_t address;
    uint16_t value;
};

struct WrRegLong {
    uint8_t chip_id;
    std::vector<uint16_t> values;
};

struct RdReg {
    uint8_t chip_id;
    uint16_t address;
};

struct Cal {
    uint8_t chip_id;
    bool mode;
    uint8_t edge_delay;
    uint8_t edge_duration;
    bool aux_enable;
    uint8_t aux_delay;
};

struct Trigger {
    uint8_t pattern;
    uint8_t tag;
};

template <class Cmd>
extern void serialize(Cmd&&, std::vector<uint16_t>&);

template <class Cmd>
const bool isBroadcast = false;

template <> const bool isBroadcast<Sync> = true;
template <> const bool isBroadcast<PLLlock> = true;
template <> const bool isBroadcast<Trigger> = true;

template <class Cmd, class... Args>
void serialize(std::false_type, BitVector<uint16_t>& bits, uint8_t chip_id, Args... args) {
    serialize(Cmd{chip_id, std::forward<Args>(args)...}, bits);
}

template <class Cmd, class... Args>
void serialize(std::true_type, BitVector<uint16_t>& bits, uint8_t chip_id, Args... args) {
    serialize(Cmd{std::forward<Args>(args)...}, bits);
}

template <class Cmd, class... Args>
void serialize(BitVector<uint16_t>& bits, uint8_t chip_id, Args... args) {
    serialize<Cmd>(std::integral_constant<bool, isBroadcast<Cmd>>{} ,bits, chip_id, std::forward<Args>(args)...);
}

} // namespace RD53BCommands

#endif
