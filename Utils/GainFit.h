/*!
  \file                  GainFit.h
  \brief                 Generic Gain for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef GainFit_H
#define GainFit_H

#include "Container.h"

#include <cmath>
#include <iostream>

class GainFit
{
  public:
    GainFit() : fSlope(0), fSlopeError(0), fIntercept(0), fInterceptError(0), fQuadratic(0), fQuadraticError(0), fLog(0), fLogError(0), fChi2(0), fDoF(0) {}
    ~GainFit() {}

    void print(void) { std::cout << fSlope << "\t" << fIntercept << "\t" << fQuadratic << "\t" << fLog << "\t" << fChi2 << "\t" << fDoF << std::endl; }

    template <typename T>
    void makeChannelAverage(const ChipContainer* theChipContainer, const ChannelGroupBase* chipOriginalMask, const ChannelGroupBase* cTestChannelGroup, const uint32_t numberOfEvents)
    {
    }
    void makeSummaryAverage(const std::vector<GainFit>* theGainVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents);
    void normalize(const uint32_t numberOfEvents) {}

    float fSlope;
    float fSlopeError;

    float fIntercept;
    float fInterceptError;

    float fQuadratic;
    float fQuadraticError;

    float fLog;
    float fLogError;

    float fChi2;
    float fDoF;
};

template <>
inline void
GainFit::makeChannelAverage<GainFit>(const ChipContainer* theChipContainer, const ChannelGroupBase* chipOriginalMask, const ChannelGroupBase* cTestChannelGroup, const uint32_t numberOfEvents)
{
    float cnt = 0;

    fSlope          = 0;
    fSlopeError     = 0;
    fIntercept      = 0;
    fInterceptError = 0;
    fQuadratic      = 0;
    fQuadraticError = 0;
    fLog            = 0;
    fLogError       = 0;
    fChi2           = 0;
    fDoF            = 0;

    for(auto row = 0u; row < theChipContainer->getNumberOfRows(); row++)
        for(auto col = 0u; col < theChipContainer->getNumberOfCols(); col++)
            if(chipOriginalMask->isChannelEnabled(row, col) && cTestChannelGroup->isChannelEnabled(row, col))
            {
                if(theChipContainer->getChannel<GainFit>(row, col).fSlopeError > 0)
                {
                    fSlope += theChipContainer->getChannel<GainFit>(row, col).fSlope /
                              (theChipContainer->getChannel<GainFit>(row, col).fSlopeError * theChipContainer->getChannel<GainFit>(row, col).fSlopeError);
                    fSlopeError += 1. / (theChipContainer->getChannel<GainFit>(row, col).fSlopeError * theChipContainer->getChannel<GainFit>(row, col).fSlopeError);
                }

                if(theChipContainer->getChannel<GainFit>(row, col).fInterceptError > 0)
                {
                    fIntercept += theChipContainer->getChannel<GainFit>(row, col).fIntercept /
                                  (theChipContainer->getChannel<GainFit>(row, col).fInterceptError * theChipContainer->getChannel<GainFit>(row, col).fInterceptError);
                    fInterceptError += 1. / (theChipContainer->getChannel<GainFit>(row, col).fInterceptError * theChipContainer->getChannel<GainFit>(row, col).fInterceptError);
                }

                if(theChipContainer->getChannel<GainFit>(row, col).fQuadraticError > 0)
                {
                    fQuadratic += theChipContainer->getChannel<GainFit>(row, col).fQuadratic /
                                  (theChipContainer->getChannel<GainFit>(row, col).fQuadraticError * theChipContainer->getChannel<GainFit>(row, col).fQuadraticError);
                    fQuadraticError += 1. / (theChipContainer->getChannel<GainFit>(row, col).fQuadraticError * theChipContainer->getChannel<GainFit>(row, col).fQuadraticError);
                }

                if(theChipContainer->getChannel<GainFit>(row, col).fLogError > 0)
                {
                    fLog +=
                        theChipContainer->getChannel<GainFit>(row, col).fLog / (theChipContainer->getChannel<GainFit>(row, col).fLogError * theChipContainer->getChannel<GainFit>(row, col).fLogError);
                    fLogError += 1. / (theChipContainer->getChannel<GainFit>(row, col).fLogError * theChipContainer->getChannel<GainFit>(row, col).fLogError);
                }

                if(theChipContainer->getChannel<GainFit>(row, col).fChi2 > 0) fChi2 += theChipContainer->getChannel<GainFit>(row, col).fChi2;
                if(theChipContainer->getChannel<GainFit>(row, col).fDoF > 0) fDoF += theChipContainer->getChannel<GainFit>(row, col).fDoF;

                cnt++;
            }

    if(fSlopeError > 0)
    {
        fSlope /= fSlopeError;
        fSlopeError = sqrt(1. / fSlopeError);
    }

    if(fInterceptError > 0)
    {
        fIntercept /= fInterceptError;
        fInterceptError = sqrt(1. / fInterceptError);
    }

    if(fQuadraticError > 0)
    {
        fQuadratic /= fQuadraticError;
        fQuadraticError = sqrt(1. / fQuadraticError);
    }

    if(fLogError > 0)
    {
        fLog /= fLogError;
        fLogError = sqrt(1. / fLogError);
    }

    if(fChi2 > 0) fChi2 /= cnt;
    if(fDoF > 0) fDoF /= cnt;
}

#endif
