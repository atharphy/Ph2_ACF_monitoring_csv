/*!
  \file                  GainAndIntercept.cc
  \brief                 Generic Gain and Intercept for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "GainAndIntercept.h"

void GainAndIntercept::makeSummaryAverage(const std::vector<GainAndIntercept>* theGainAndInterceptVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents)
{
    if(theGainAndInterceptVector->size() != theNumberOfEnabledChannelsList.size())
    {
        std::cout << __PRETTY_FUNCTION__ << " theGainAndInterceptVector size = " << theGainAndInterceptVector->size()
                  << " does not match theNumberOfEnabledChannelsList size = " << theNumberOfEnabledChannelsList.size() << std::endl;
        abort();
    }

    fGain           = 0;
    fGainError      = 0;
    fIntercept      = 0;
    fInterceptError = 0;

    for(size_t iContainer = 0; iContainer < theGainAndInterceptVector->size(); iContainer++)
    {
        if(theGainAndInterceptVector->at(iContainer).fGainError > 0)
        {
            fGain += theGainAndInterceptVector->at(iContainer).fGain * theNumberOfEnabledChannelsList[iContainer] /
                     (theGainAndInterceptVector->at(iContainer).fGainError * theGainAndInterceptVector->at(iContainer).fGainError);
            fGainError += theNumberOfEnabledChannelsList[iContainer] / (theGainAndInterceptVector->at(iContainer).fGainError * theGainAndInterceptVector->at(iContainer).fGainError);
        }

        if(theGainAndInterceptVector->at(iContainer).fInterceptError > 0)
        {
            fIntercept += theGainAndInterceptVector->at(iContainer).fIntercept * theNumberOfEnabledChannelsList[iContainer] /
                          (theGainAndInterceptVector->at(iContainer).fInterceptError * theGainAndInterceptVector->at(iContainer).fInterceptError);
            fInterceptError += theNumberOfEnabledChannelsList[iContainer] / (theGainAndInterceptVector->at(iContainer).fInterceptError * theGainAndInterceptVector->at(iContainer).fInterceptError);
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
