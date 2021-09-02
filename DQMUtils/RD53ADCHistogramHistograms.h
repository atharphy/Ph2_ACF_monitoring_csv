/*!
  \file                  RD53ADCHistogramHistograms.h
  \brief                 Header file of ADCHistogram histograms
  \author                Umberto MOLINATTI
  \version               1.0
  \date                  19/04/21
  Support:               email to umberto.molinatti@cern.ch
*/

#ifndef RD53ADCHistogramHistograms_H
#define RD53ADCHistogramHistograms_H

#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GenericDataArray.h"
#include "DQMHistogramBase.h"

#include "TFitResult.h"
#include "TGraph.h"
#include <TStyle.h>
#include <fstream>

#include "TGraph.h"
#include "TH1.h"
#include <TStyle.h>

class ADCHistogramHistograms : public DQMHistogramBase
{
  public:
    void fillHH(std::vector<double> ADCcode, int max_counts);

    double* codes    = new double[4094]();
    double* DNLvalue = new double[4094]();
    double* INLvalue = new double[4094]();

  private:
    DetectorDataContainer DetectorData;
};

#endif
