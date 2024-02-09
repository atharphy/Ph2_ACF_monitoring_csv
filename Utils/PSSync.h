/*

        \file                          Occupancy.h
        \brief                         Generic Occupancy for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          08/04/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __PSSYNC_H__
#define __PSSYNC_H__

#include "Utils/Container.h"
#include "Utils/Event.h"
#include "Utils/GenericDataArray.h"
#include <iostream>
#include <math.h>

template <size_t StripSize, size_t PixelSize, size_t StubSize>
class PSSync //: public streammable
{
  public:
    GenericDataArray<Ph2_HwInterface::SCluster, StripSize> fSClusters;
    GenericDataArray<Ph2_HwInterface::PCluster, PixelSize> fPClusters;
    GenericDataArray<Ph2_HwInterface::Stub, StubSize>      fStubs;
};

#endif
