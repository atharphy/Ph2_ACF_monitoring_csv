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
        uint16_t nIterationsVCal    = floor(log2(stopValue - startValue + 1) + 2);
        uint16_t moreIterationsVCal = 1;
        uint16_t nIterationsTDAC    = floor(log2(frontEnd->nTDACvalues) + 2);
        uint16_t moreIterationsTDAC = 1;
        return PixelAlive::getNumberIterations() * ((nIterationsVCal + moreIterationsVCal) + (nIterationsTDAC + moreIterationsTDAC));
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    ThrEqualizationHistograms* histos;
#endif

  private:
    size_t startValue;
    size_t stopValue;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    std::shared_ptr<DetectorDataContainer>   theOccContainer;
    DetectorDataContainer                    theTDACcontainer;

    void fillHisto();
    void bitWiseScanGlobal(const std::string& regName, const float& target, uint16_t startValue, uint16_t stopValue);
    void bitWiseScanLocal(const std::string& regName, const float& target);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
};

#endif
