/*!
  \file                  RD53GainOptimization.h
  \brief                 Implementaion of gain optimization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53GainOptimization_H
#define RD53GainOptimization_H

#include "RD53Gain.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53GainOptimizationHistograms.h"
#endif

// #############
// # CONSTANTS #
// #############
#define NSTDEV 1. // Number of standard deviations for gain tolerance

// ################################
// # Gain optimization test suite #
// ################################
class GainOptimization : public Gain
{
  public:
    void Start(int currentRun) override;
    void Stop() override;
    void ConfigureCalibration() override;

    void   sendData();
    void   localConfigure(const std::string fileRes_, int currentRun);
    void   initializeFiles(const std::string fileRes_, int currentRun);
    void   run();
    void   analyze();
    void   draw(int currentRun);
    size_t getNumberIterations()
    {
        uint16_t nBitKrumCurr   = floor(log2(KrumCurrStop - KrumCurrStart + 1) + 1);
        uint16_t moreIterations = 1;
        return Gain::getNumberIterations() * (nBitKrumCurr + moreIterations);
    }

#ifdef __USE_ROOT__
    GainOptimizationHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t startValue;
    size_t stopValue;
    float  targetCharge;
    size_t KrumCurrStart;
    size_t KrumCurrStop;
    bool   doFast;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    DetectorDataContainer theKrumCurrContainer;

    void fillHisto();
    void bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue);
    void chipErrorReport();
    void saveChipRegisters(int currentRun);

  protected:
    std::string fileRes;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
