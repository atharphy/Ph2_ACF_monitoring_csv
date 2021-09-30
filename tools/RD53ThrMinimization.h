/*!
  \file                  RD53ThrMinimization.h
  \brief                 Implementaion of threshold minimization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ThrMinimization_H
#define RD53ThrMinimization_H

#include "RD53PixelAlive.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53ThresholdHistograms.h"
#endif

// #####################################
// # Threshold minimization test suite #
// #####################################
class ThrMinimization : public PixelAlive
{
  public:
    ~ThrMinimization()
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
    void   draw();
    void   analyze();
    size_t getNumberIterations()
    {
        uint16_t nIterationsThr = floor(log2(ThrStop - ThrStart + 1) + 1);
        uint16_t moreIterations = 2;
        return PixelAlive::getNumberIterations() * (nIterationsThr + moreIterations);
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    ThresholdHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    float  targetOccupancy;
    size_t ThrStart;
    size_t ThrStop;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    DetectorDataContainer theThrContainer;

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
