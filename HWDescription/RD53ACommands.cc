/*!
  \file                  RD53ACommands.cc
  \brief                 RD53ACommands implementation
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53ACommands.h"

namespace RD53Cmd
{
std::array<uint8_t, nFields> GlobalPulse::serializeFields() const
{
    std::array<uint8_t, nFields> fields;

    fields[0] = packAndEncode<4, 1>(chip_id, 0);
    fields[1] = packAndEncode<4, 1>(data, 0);

    return fields;
}

std::array<uint8_t, nFields> Cal::serializeFields() const
{
    std::array<uint8_t, nFields> fields;

    fields[0] = packAndEncode<4, 1>(chip_id, cal_edge_mode);
    fields[1] = packAndEncode<3, 2>(cal_edge_delay, cal_edge_width >> 4);
    fields[2] = packAndEncode<4, 1>(cal_edge_width, cal_aux_mode);
    fields[3] = packAndEncode<5>(cal_aux_delay);
}

std::array<uint8_t, nFields> WrReg::serializeFields() const
{
    std::array<uint8_t, nFields> fields;

    fields[0] = packAndEncode<4, 1>(chip_id, 0);
    fields[1] = packAndEncode<5>(address >> 4);
    fields[2] = packAndEncode<4, 1>(address, value >> 15);
    fields[3] = packAndEncode<5>(value >> 10);
    fields[4] = packAndEncode<5>(value >> 5);
    fields[5] = packAndEncode<5>(value);

    return fields;
}

std::array<uint8_t, nFields> WrRegLong::serializeFields() const
{
    std::array<uint8_t, nFields> fields;

    fields[0] = packAndEncode<4, 1>(chip_id, 1);
    fields[1] = packAndEncode<5>(address >> 4);
    fields[2] = packAndEncode<4, 1>(address, values[0] >> 15);
    fields[3] = packAndEncode<5>(values[0] >> 10);
    fields[4] = packAndEncode<5>(values[0] >> 5);
    fields[5] = packAndEncode<5>(values[0]);

    bits::unpack_range<5>(values.begin() + 1, values.end(), fields.begin() + 6);
    for(auto i = 6u; i < fields.size(); i++) fields[i] = map5to8bit[fields[i]];

    return fields;
}

std::array<uint8_t, nFields> RdReg::serializeFields() const
{
    std::array<uint8_t, nFields> fields;

    fields[0] = packAndEncode<4, 1>(chip_id, 0);
    fields[1] = packAndEncode<5>(address >> 4);
    fields[2] = packAndEncode<4, 1>(address, 0);
    fields[3] = packAndEncode<5>(0);

    return fields;
}

} // namespace RD53Cmd
