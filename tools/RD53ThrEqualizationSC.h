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

#include "RD53PixelAlive.h"
#include "RD53SCurve.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53ThrEqualizationHistograms.h"
#endif

// #############
// # CONSTANTS #
// #############
#define TARGETEFF 0.50 // Target efficiency for optimization algorithm

// #####################################
// # Threshold equalization test suite #
// #####################################
class ThrEqualizationSC : public PixelAlive
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

    void   localConfigure(const std::string fileRes_ = "", int currentRun = -1);
    void   initializeFiles(const std::string fileRes_ = "", int currentRun = -1);
    void   run();
    void   draw();
    void   analyze();
    size_t getNumberIterations()
    {
        uint16_t nIterationsVCal    = floor(log2(stopValue - startValue + 1) + 2);
        uint16_t moreIterationsVCal = 1;
        uint16_t nIterationsTDAC    = floor(log2(frontEnd->nTDACvalues) + 2);
        uint16_t moreIterationsTDAC = 1;
        return PixelAlive::getNumberIterations() * (nIterationsVCal + moreIterationsVCal) +
               ((sc.getNumberIterations() * (nIterationsTDAC + moreIterationsTDAC)) + nIterationsTDAC) * nEvents / nEvtsBurst;
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    ThrEqualizationHistograms* histos;
#endif

  private:
    SCurve sc;
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t nEvtsBurst;
    size_t startValue;
    size_t stopValue;
    size_t offset;
    size_t nHITxCol;
    bool   doFast;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    DetectorDataContainer                    theOccContainer;
    DetectorDataContainer                    theTDACcontainer;

    void                                   fillHisto();
    std::shared_ptr<DetectorDataContainer> bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue);
    void                                   bitWiseScanLocal(const std::string& regName, uint32_t nEvents, std::shared_ptr<DetectorDataContainer> target, uint32_t nEvtsBurst);
    void                                   chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
