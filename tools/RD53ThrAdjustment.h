/*!
  \file                  RD53ThrAdjustment.h
  \brief                 Implementaion of threshold adjustment
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ThrAdjustment_H
#define RD53ThrAdjustment_H

#include "RD53PixelAlive.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53ThresholdHistograms.h"
#else
typedef bool ThresholdHistograms;
#endif

// #############
// # CONSTANTS #
// #############
#define TARGETEFF 0.50 // Target efficiency for optimization algorithm

// #####################################
// # Threshold minimization test suite #
// #####################################
class ThrAdjustment : public PixelAlive
{
  public:
    ~ThrAdjustment()
    {
#ifdef __USE_ROOT__
        this->WriteRootFile();
        this->CloseResultFile();
        delete histos;
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string& histoFileName = "", int currentRun = -1) override;
    void   run() override;
    void   draw(bool saveData = true) override;
    size_t getNumberIterations() override
    {
        uint16_t nIterationsThr = floor(log2(stopValue - startValue + 1) + 2);
        uint16_t moreIterations = 1;
        return PixelAlive::getNumberIterations() * (nIterationsThr + moreIterations);
    }

    void analyze();

    ThresholdHistograms* histos;

  private:
    void fillHisto() override;

    void bitWiseScanGlobal(const std::string& regName, float target, uint16_t startValue, uint16_t stopValue);

    DetectorDataContainer theThrContainer;

  protected:
    float  targetThreshold;
    size_t startValue;
    size_t stopValue;
    bool   doUpdateChip;
    bool   doDisplay;

    int theCurrentRun;
};

#endif
