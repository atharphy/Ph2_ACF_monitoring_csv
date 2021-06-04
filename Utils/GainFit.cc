/*!
  \file                  GainFit.cc
  \brief                 Generic Gain for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "GainFit.h"

void GainFit::makeSummaryAverage(const std::vector<GainFit>* theGainVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents)
{
    if(theGainVector->size() != theNumberOfEnabledChannelsList.size())
    {
        std::cout << __PRETTY_FUNCTION__ << " theGainVector size = " << theGainVector->size() << " does not match theNumberOfEnabledChannelsList size = " << theNumberOfEnabledChannelsList.size()
                  << std::endl;
        abort();
    }

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

    for(size_t iContainer = 0; iContainer < theGainVector->size(); iContainer++)
    {
        if(theGainVector->at(iContainer).fSlopeError > 0)
        {
            fSlope += theGainVector->at(iContainer).fSlope * theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fSlopeError * theGainVector->at(iContainer).fSlopeError);
            fSlopeError += theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fSlopeError * theGainVector->at(iContainer).fSlopeError);
        }

        if(theGainVector->at(iContainer).fInterceptError > 0)
        {
            fIntercept +=
                theGainVector->at(iContainer).fIntercept * theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fInterceptError * theGainVector->at(iContainer).fInterceptError);
            fInterceptError += theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fInterceptError * theGainVector->at(iContainer).fInterceptError);
        }

        if(theGainVector->at(iContainer).fQuadraticError > 0)
        {
            fQuadratic +=
                theGainVector->at(iContainer).fQuadratic * theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fQuadraticError * theGainVector->at(iContainer).fQuadraticError);
            fQuadraticError += theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fQuadraticError * theGainVector->at(iContainer).fQuadraticError);
        }

        if(theGainVector->at(iContainer).fLogError > 0)
        {
            fLog += theGainVector->at(iContainer).fLog * theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fLogError * theGainVector->at(iContainer).fLogError);
            fLogError += theNumberOfEnabledChannelsList[iContainer] / (theGainVector->at(iContainer).fLogError * theGainVector->at(iContainer).fLogError);
        }

        if(theGainVector->at(iContainer).fChi2 > 0) fChi2 += theGainVector->at(iContainer).fChi2;
        if(theGainVector->at(iContainer).fDoF > 0) fDoF += theGainVector->at(iContainer).fDoF;

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
