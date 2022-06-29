/*!
  \file                  OccupancyAndPh.h
  \brief                 Generic Occupancy and Pulse Height for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef OccupancyAndPh_H
#define OccupancyAndPh_H

#include "Container.h"
#include "EmptyContainer.h"

#include <algorithm>
#include <cmath>
#include <iostream>

class OccupancyAndPh
{
  public:
    OccupancyAndPh() : fOccupancy(0), fOccupancyMedian(0), fPh(0), fPhError(0), readoutError(false) {}

    void print(void) { std::cout << fOccupancy << "\t" << fPh << std::endl; }

    template <typename T>
    void
    makeChannelAverage(const ChipContainer* theChipContainer, std::shared_ptr<ChannelGroupBase> chipOriginalMask, std::shared_ptr<ChannelGroupBase> cTestChannelGroup, const uint32_t numberOfEvents)
    {
    }
    void makeSummaryAverage(const std::vector<OccupancyAndPh>* theOccupancyVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents);
    void makeSummaryAverage(const std::vector<EmptyContainer>* theOccupancyVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents) {}
    void normalize(const uint32_t numberOfEvents, bool doOnlyPh = false);

    float fOccupancy;
    float fOccupancyMedian;

    float fPh;
    float fPhError;

    bool readoutError;
};

template <>
inline void OccupancyAndPh::makeChannelAverage<OccupancyAndPh>(const ChipContainer*              theChipContainer,
                                                               std::shared_ptr<ChannelGroupBase> chipOriginalMask,
                                                               std::shared_ptr<ChannelGroupBase> cTestChannelGroup,
                                                               const uint32_t                    numberOfEvents)
{
    fOccupancy       = 0;
    fOccupancyMedian = 0;
    fPh              = 0;
    fPhError         = 0;

    std::vector<float> sortedOcc;
    size_t             numberOfEnabledChannels = 0;

    for(auto row = 0u; row < theChipContainer->getNumberOfRows(); row++)
        for(auto col = 0u; col < theChipContainer->getNumberOfCols(); col++)
            if(chipOriginalMask->isChannelEnabled(row, col) && cTestChannelGroup->isChannelEnabled(row, col))
            {
                fOccupancy += theChipContainer->getChannel<OccupancyAndPh>(row, col).fOccupancy;

                sortedOcc.insert(std::upper_bound(sortedOcc.begin(), sortedOcc.end(), theChipContainer->getChannel<OccupancyAndPh>(row, col).fOccupancy),
                                 theChipContainer->getChannel<OccupancyAndPh>(row, col).fOccupancy);

                if(theChipContainer->getChannel<OccupancyAndPh>(row, col).fPhError > 0)
                {
                    fPh += theChipContainer->getChannel<OccupancyAndPh>(row, col).fPh /
                           (theChipContainer->getChannel<OccupancyAndPh>(row, col).fPhError * theChipContainer->getChannel<OccupancyAndPh>(row, col).fPhError);
                    fPhError += 1. / (theChipContainer->getChannel<OccupancyAndPh>(row, col).fPhError * theChipContainer->getChannel<OccupancyAndPh>(row, col).fPhError);
                }

                numberOfEnabledChannels++;
            }

    fOccupancy /= (numberOfEnabledChannels > 0 ? numberOfEnabledChannels : 1);

    if(numberOfEnabledChannels != 0)
    {
        if(numberOfEnabledChannels % 2 == 0)
            fOccupancyMedian = (sortedOcc[sortedOcc.size() / 2 - 1] + sortedOcc[sortedOcc.size() / 2]) / 2;
        else
            fOccupancyMedian = sortedOcc[sortedOcc.size() / 2];
    }

    if(fPhError > 0)
    {
        fPh /= fPhError;
        fPhError /= sqrt(1. / fPhError);
    }
}

#endif
