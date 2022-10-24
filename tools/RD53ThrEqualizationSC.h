/*!
  \file                  RD53ThrEqualizationSC.h
  \brief                 Implementaion of threshold equalization with SCurves
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ThrEqualizationSC_H
#define RD53ThrEqualizationSC_H

#include "RD53SCurve.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53ThrEqualizationHistograms.h"
#endif

// #####################################
// # Threshold equalization test suite #
// #####################################
class ThrEqualizationSC : public SCurve
{
  public:
    ~ThrEqualizationSC()
    {
#ifdef __USE_ROOT__
        this->WriteRootFile();
        this->CloseResultFile();
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string& fileRes_ = "", int currentRun = -1);
    void   initializeFiles(const std::string& fileRes_ = "", int currentRun = -1);
    void   run();
    void   draw();
    void   analyze();
    size_t getNumberIterations()
    {
        uint16_t nIterationsVCal    = 1;
        uint16_t nIterationsTDAC    = floor(log2(frontEnd->nTDACvalues) + 2);
        uint16_t moreIterationsTDAC = 1;
        return SCurve::getNumberIterations() * (nIterationsVCal + nIterationsTDAC + moreIterationsTDAC);
    }

#ifdef __USE_ROOT__
    ThrEqualizationHistograms* histos;
#endif

  private:
    DetectorDataContainer theTDACcontainer;

    void fillHisto();
    void bitWiseScanLocal(const std::string& regName, std::shared_ptr<DetectorDataContainer> target);

  protected:
    size_t doNthrequSteps;
    bool   doDisplay;
    bool   doUpdateChip;

    std::string fileRes;
    int         theCurrentRun;
};

#endif
