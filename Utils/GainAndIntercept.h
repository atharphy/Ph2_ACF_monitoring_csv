/*!
  \file                  GainAndIntercept.h
  \brief                 Generic Gain and Intercept for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef GainAndIntercept_H
#define GainAndIntercept_H

#include "Container.h"

#include <cmath>
#include <iostream>

class GainAndIntercept
{
  public:
    GainAndIntercept() : fGain(0), fGainError(0), fIntercept(0), fInterceptError(0) {}
    ~GainAndIntercept() {}

    void print(void) { std::cout << fGain << "\t" << fIntercept << std::endl; }

    template <typename T>
    void makeChannelAverage(const ChipContainer* theChipContainer, const ChannelGroupBase* chipOriginalMask, const ChannelGroupBase* cTestChannelGroup, const uint32_t numberOfEvents)
    {
    }
    void makeSummaryAverage(const std::vector<GainAndIntercept>* theGainAndInterceptVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents);
    void normalize(const uint32_t numberOfEvents) {}

    float fGain;
    float fGainError;

    float fIntercept;
    float fInterceptError;
};

template <>
inline void GainAndIntercept::makeChannelAverage<GainAndIntercept>(const ChipContainer*    theChipContainer,
                                                                   const ChannelGroupBase* chipOriginalMask,
                                                                   const ChannelGroupBase* cTestChannelGroup,
                                                                   const uint32_t          numberOfEvents)
{
    fGain           = 0;
    fGainError      = 0;
    fIntercept      = 0;
    fInterceptError = 0;

    for(auto row = 0u; row < theChipContainer->getNumberOfRows(); row++)
        for(auto col = 0u; col < theChipContainer->getNumberOfCols(); col++)
            if(chipOriginalMask->isChannelEnabled(row, col) && cTestChannelGroup->isChannelEnabled(row, col))
            {
                if(theChipContainer->getChannel<GainAndIntercept>(row, col).fGainError > 0)
                {
                    fGain += theChipContainer->getChannel<GainAndIntercept>(row, col).fGain /
                             (theChipContainer->getChannel<GainAndIntercept>(row, col).fGainError * theChipContainer->getChannel<GainAndIntercept>(row, col).fGainError);
                    fGainError += 1. / (theChipContainer->getChannel<GainAndIntercept>(row, col).fGainError * theChipContainer->getChannel<GainAndIntercept>(row, col).fGainError);
                }

                if(theChipContainer->getChannel<GainAndIntercept>(row, col).fInterceptError > 0)
                {
                    fIntercept += theChipContainer->getChannel<GainAndIntercept>(row, col).fIntercept /
                                  (theChipContainer->getChannel<GainAndIntercept>(row, col).fInterceptError * theChipContainer->getChannel<GainAndIntercept>(row, col).fInterceptError);
                    fInterceptError += 1. / (theChipContainer->getChannel<GainAndIntercept>(row, col).fInterceptError * theChipContainer->getChannel<GainAndIntercept>(row, col).fInterceptError);
                }
            }

    if(fGainError > 0)
    {
        fGain /= fGainError;
        fGainError = sqrt(1. / fGainError);
    }

    if(fInterceptError > 0)
    {
        fIntercept /= fInterceptError;
        fInterceptError = sqrt(1. / fInterceptError);
    }
}

#endif
