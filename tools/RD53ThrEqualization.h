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
        uint16_t nBitVCal         = floor(log2(stopValue - startValue + 1) + 1);
        uint16_t moreIterationsPA = 1;
        uint16_t nBitTDAC         = 4;
        uint16_t moreIterations   = 2;
        return PixelAlive::getNumberIterations() * (nBitVCal + moreIterationsPA) +
               RD53ChannelGroupHandler::getNumberOfGroups(doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol) * (nBitTDAC + moreIterations) * nEvents / nEvtsBurst;
    }

#ifdef __USE_ROOT__
    ThrEqualizationHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t nEvtsBurst;
    size_t startValue;
    size_t stopValue;
    size_t nHITxCol;
    bool   doFast;

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    DetectorDataContainer                    theOccContainer;
    DetectorDataContainer                    theTDACcontainer;

    void fillHisto();
    void bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue);
    void bitWiseScanLocal(const std::string& regName, uint32_t nEvents, const float& target, uint32_t nEvtsBurst);
    void chipErrorReport();
    void saveChipRegisters(int currentRun);

  protected:
    std::string fileRes;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
