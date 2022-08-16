/*!
  \file                  RD53BEventDecoding.cc
  \brief                 RD53BEventDecoding description class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BEVENTDECODING_H
#define RD53BEVENTDECODING_H

#include "RD53Event.h"
#include "../HWDescription/RD53B.h"

namespace RD53BEventDecoding {

struct FormatOptions {
    bool enableChipId = false;
    bool enableToT = true;
    bool enableBCID = false;
    bool enableTriggerId = false;
};

size_t decode_events(const std::vector<uint32_t>& data, std::vector<Ph2_HwInterface::RD53Event>& events, const FormatOptions& options = {});
size_t count_events(const std::vector<uint32_t>& data);

} // namespace RD53BEventDecoding

#endif