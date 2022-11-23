/*!
  \file                  RD53ThrEqualization.h
  \brief                 Implementaion of threshold equalization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ThrEqualization_H
#define RD53ThrEqualization_H

#include "RD53PixelAlive.h"

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
class ThrEqualization : public PixelAlive
{
  public:
    ~ThrEqualization()
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

    void   localConfigure(const std::string& fileRes_ = "", int currentRun = -1) override;
    void   initializeFiles(const std::string& fileRes_ = "", int currentRun = -1) override;
    void   run() override;
    void   draw(bool saveData = true) override;
    size_t getNumberIterations() override
    {
        uint16_t nIterationsVCal    = floor(log2(stopValue - startValue + 1) + 2);
        uint16_t moreIterationsVCal = 1;
        uint16_t nIterationsTDAC    = (doNSteps != 0 ? doNSteps + 1 : floor(log2(frontEnd->nTDACvalues) + 2));
        uint16_t moreIterationsTDAC = 1;
        return PixelAlive::getNumberIterations() * (((TDACGainNSteps == 0 ? 1 : TDACGainNSteps + 2) * ((nIterationsVCal + moreIterationsVCal) + (nIterationsTDAC + moreIterationsTDAC))));
    }

    void analyze();
    void analyzeDuringRun();

#ifdef __USE_ROOT__
    ThrEqualizationHistograms* histos;
#endif

  private:
    void fillHisto() override;

    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer);
    void bitWiseScanGlobal(const std::string& regName, float target, uint16_t startValue, uint16_t stopValue);
    void bitWiseScanLocal(float target, bool updateDACs);
    void chipErrorReport() const;

    std::vector<uint16_t>                  dacList;
    std::shared_ptr<DetectorDataContainer> theOccContainer;
    DetectorDataContainer                  theContainer;
    DetectorDataContainer                  theTDACGainContainer;
    DetectorDataContainer                  theTDACContainer;

  protected:
    int    resetTDAC;
    size_t startValue;
    size_t stopValue;
    size_t startTDACGainValue;
    size_t stopTDACGainValue;
    size_t TDACGainNSteps;
    size_t doNSteps;
    bool   doUpdateChip;
    bool   doDisplay;

    std::string fileRes;
    int         theCurrentRun;
};

#endif
