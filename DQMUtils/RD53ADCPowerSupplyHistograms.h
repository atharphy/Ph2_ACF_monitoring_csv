/*!
  \file                  RD53ADCPowerSupplyHistograms.h
  \brief                 Header file of ADCPowerSupply histograms
  \author                Umberto MOLINATTI
  \version               1.0
  \date                  19/04/21
  Support:               email to umberto.molinatti@cern.ch
*/

#ifndef RD53ADCPowerSupplyHistograms_H
#define RD53ADCPowerSupplyHistograms_H

#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GenericDataArray.h"
#include "DQMHistogramBase.h"

#include "TFitResult.h"
#include "TGraph.h"
#include <TStyle.h>
#include <fstream>

class ADCPowerSupplyHistograms : public DQMHistogramBase
{
  public:
    void fillPS(DetectorContainer& DataContainer, double fitStart, double fitEnd, double* VMUXvolt, double* ADCcode);

  private:
    DetectorDataContainer DetectorData;
};

#endif
