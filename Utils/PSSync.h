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

#include "../Utils/Container.h"
#include <iostream>
#include <math.h>
#include "../Utils/Event.h"



class PSSync //: public streammable
{
  public:

    std::vector<Ph2_HwInterface::SCluster> fSClusters;
    std::vector<Ph2_HwInterface::PCluster> fPClusters;
    std::vector<Ph2_HwInterface::Stub> fStubs;
};


#endif

