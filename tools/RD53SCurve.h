/*!
  \file                  RD53SCurve.h
  \brief                 Implementaion of SCurve scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53SCurve_H
#define RD53SCurve_H

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/ThresholdAndNoise.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53SCurveHistograms.h"
#include "TApplication.h"
#endif

// #####################
// # SCurve test suite #
// #####################
class SCurve : public Tool
{
  public:
    ~SCurve()
    {
        for(auto container: detectorContainerVector) theRecyclingBin.free(container);
    }

    void Start(int currentRun) override;
    void Stop() override;
    void ConfigureCalibration() override;

    void                                   sendData();
    void                                   localConfigure(const std::string fileRes_, int currentRun);
    void                                   initializeFiles(const std::string fileRes_, int currentRun);
    void                                   run();
    void                                   draw(int currentRun);
    std::shared_ptr<DetectorDataContainer> analyze();
    size_t getNumberIterations() { return RD53ChannelGroupHandler::getNumberOfGroups(doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol) * nSteps; }

#ifdef __USE_ROOT__
    SCurveHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t startValue;
    size_t stopValue;
    size_t nSteps;
    size_t offset;
    size_t nHITxCol;
    bool   doFast;

    std::vector<uint16_t> dacList;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    std::vector<DetectorDataContainer*>      detectorContainerVector;
    std::shared_ptr<DetectorDataContainer>   theThresholdAndNoiseContainer;
    ContainerRecycleBin<OccupancyAndPh>      theRecyclingBin;

    void fillHisto();
    void computeStats(const std::vector<float>& measurements, int offset, float& nHits, float& mean, float& rms);
    void chipErrorReport();
    void saveChipRegisters(int currentRun);

  protected:
    std::string fileRes;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
