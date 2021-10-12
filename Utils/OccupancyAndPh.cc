/*!
  \file                  OccupancyAndPh.cc
  \brief                 Generic Occupancy and Pulse Height for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "OccupancyAndPh.h"

void OccupancyAndPh::makeSummaryAverage(const std::vector<OccupancyAndPh>* theOccupancyVector, const std::vector<uint32_t>& theNumberOfEnabledChannelsList, const uint32_t numberOfEvents)
{
    if(theOccupancyVector->size() != theNumberOfEnabledChannelsList.size())
    {
        std::cout << __PRETTY_FUNCTION__ << " theOccupancyVector size = " << theOccupancyVector->size()
                  << " does not match theNumberOfEnabledChannelsList size = " << theNumberOfEnabledChannelsList.size() << std::endl;
        abort();
    }

    fOccupancy = 0;
    fPh        = 0;
    fPhError   = 0;

    size_t totalNumberOfEnableChannels = 0;

    for(size_t iContainer = 0; iContainer < theOccupancyVector->size(); iContainer++)
    {
        fOccupancy += theOccupancyVector->at(iContainer).fOccupancy * theNumberOfEnabledChannelsList[iContainer];

        if(theOccupancyVector->at(iContainer).fPhError > 0)
        {
            fPh += theOccupancyVector->at(iContainer).fPh * theNumberOfEnabledChannelsList[iContainer] / (theOccupancyVector->at(iContainer).fPhError * theOccupancyVector->at(iContainer).fPhError);
            fPhError += theNumberOfEnabledChannelsList[iContainer] / (theOccupancyVector->at(iContainer).fPhError * theOccupancyVector->at(iContainer).fPhError);
        }

        totalNumberOfEnableChannels += theNumberOfEnabledChannelsList[iContainer];
    }

    fOccupancy /= (totalNumberOfEnableChannels > 0 ? totalNumberOfEnableChannels : 1);

    if(fPhError > 0)
    {
        fPh /= fPhError;
        fPhError /= sqrt(1. / fPhError);
    }
}

void OccupancyAndPh::normalize(const uint32_t numberOfEvents, bool doOnlyPh)
{
    fPh /= (fOccupancy > 0 ? fOccupancy : 1);
    fPhError = (fOccupancy > 1 ? sqrt((fPhError / fOccupancy - fPh * fPh) * fOccupancy / (fOccupancy - 1)) : 0);

    if(doOnlyPh == false) fOccupancy /= numberOfEvents;
}
