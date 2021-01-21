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
    void Start(int currentRun) override;
    void Stop() override;
    void ConfigureCalibration() override;

    void   sendData();
    void   localConfigure(const std::string fileRes_, int currentRun);
    void   initializeFiles(const std::string fileRes_, int currentRun);
    void   run();
    void   draw(int currentRun);
    void   analyze();
    size_t getNumberIterations()
    {
        uint16_t nBitThr        = floor(log2(ThrStop - ThrStart + 1) + 1);
        uint16_t nBitVCal       = floor(log2(VCalStop - VCalStart + 1) + 1);
        uint16_t moreIterations = 1;
        return PixelAlive::getNumberIterations() * (nBitThr + moreIterations) * (nBitVCal + moreIterations);
    }

#ifdef __USE_ROOT__
    ThresholdHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t VCalStart;
    size_t VCalStop;
    size_t targetThreshold;
    size_t ThrStart;
    size_t ThrStop;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    DetectorDataContainer theThrContainer;

    void                                   fillHisto();
    void                                   bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue);
    std::shared_ptr<DetectorDataContainer> bitWiseScanGlobal_MeasureThr(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue);
    void                                   chipErrorReport();
    void                                   saveChipRegisters(int currentRun);

  protected:
    std::string fileRes;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
