/*!
  \file                  RD53BEventDecoding.cc
  \brief                 RD53BEventDecoding description class
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/22
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BEventDecoding_H
#define RD53BEventDecoding_H

#include "../HWDescription/RD53B.h"
#include "RD53Event.h"

namespace RD53BEventDecoding
{
struct FormatOptions
{
    bool enableChipId    = true;
    bool enableToT       = true;
    bool enableBCID      = false;
    bool enableTriggerId = false;
};

size_t decode_events(const std::vector<uint32_t>& data, std::vector<Ph2_HwInterface::RD53Event>& events, const FormatOptions& options = {});
size_t decode_events(const std::vector<uint32_t>& data, std::vector<Ph2_HwInterface::RD53Event>& events, const std::vector<size_t>& refEventStart, const FormatOptions& options = {});
size_t decode_events(const uint32_t* data, std::vector<Ph2_HwInterface::RD53Event>& events, const size_t howMany, const FormatOptions& options = {});
size_t count_events(const std::vector<uint32_t>& data);

} // namespace RD53BEventDecoding

#endif
