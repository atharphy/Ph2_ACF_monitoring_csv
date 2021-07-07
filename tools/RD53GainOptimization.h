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
#define NSTDEV 4. // Number of standard deviations for gain tolerance

// ################################
// # Gain optimization test suite #
// ################################
class GainOptimization : public Gain
{
  public:
    ~GainOptimization()
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

    void   localConfigure(const std::string fileRes_ = "", int currentRun = -1);
    void   initializeFiles(const std::string fileRes_ = "", int currentRun = -1);
    void   run();
    void   analyze();
    void   draw();
    size_t getNumberIterations()
    {
        uint16_t nIterationsKrumCurr = floor(log2(KrumCurrStop - KrumCurrStart + 1) + 1);
        uint16_t moreIterations      = 2;
        return Gain::getNumberIterations() * (nIterationsKrumCurr + moreIterations);
    }
    void saveChipRegisters(int currentRun);

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
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
