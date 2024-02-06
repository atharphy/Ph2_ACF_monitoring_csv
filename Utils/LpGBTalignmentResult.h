#ifndef __LPGBT_ALIGNMENT_RESULT__
#define __LPGBT_ALIGNMENT_RESULT__

#include <map>
#include <tuple>
#include "Utils/GenericDataArray.h"

struct LpGBTalignmentResult
{
    LpGBTalignmentResult() {};

    void setGroupAndChannelResult(const uint8_t groupNumber, const uint8_t channelNumber, const float alignmentEfficiency, const uint8_t bestPhase, const GenericDataArray<16, float>& phaseHistogram)
    {
        fResultContainer[groupNumber][channelNumber] = std::make_tuple(alignmentEfficiency, bestPhase, phaseHistogram);
    }

    float getGroupAndChannelAlignmentEfficiency(const uint8_t groupNumber, const uint8_t channelNumber) const
    {
        return std::get<0>(fResultContainer.at(groupNumber).at(channelNumber));
    }

    uint8_t getGroupAndChannelBestPhase(const uint8_t groupNumber, const uint8_t channelNumber) const
    {
        return std::get<1>(fResultContainer.at(groupNumber).at(channelNumber));
    }

    GenericDataArray<16, float> getGroupAndChannelPhaseHystogram(const uint8_t groupNumber, const uint8_t channelNumber) const
    {
        return std::get<2>(fResultContainer.at(groupNumber).at(channelNumber));
    }

    std::map<uint8_t, std::map<uint8_t, std::tuple<float, uint8_t, GenericDataArray<16, float>>>> fResultContainer;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& theArchive, const unsigned int version)
    {
        theArchive& fResultContainer;
    }

};


#endif