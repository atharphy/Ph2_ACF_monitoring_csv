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

#include "../HWDescription/RD53.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/ThresholdAndNoise.h"
#include "RD53CalibBase.h"

#include <algorithm>

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53SCurveHistograms.h"
#endif

// #####################
// # SCurve test suite #
// #####################
class SCurve : public CalibBase
{
  public:
    ~SCurve()
    {
        for(auto container: detectorContainerVector) theRecyclingBin.free(container);
#ifdef __USE_ROOT__
        if(saveData == true) this->WriteRootFile();
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
    void   draw(bool doSaveData = true) override;
    size_t getNumberIterations() override { return theChnGroupHandler->getNumberOfGroups() * nSteps; }

    std::shared_ptr<DetectorDataContainer> analyze();

#ifdef __USE_ROOT__
    SCurveHistograms* histos;
#endif

  private:
    void fillHisto() override;

    void computeStats(std::vector<float>& measurements, int offset, float& nHits, float& mean, float& rms);

    std::vector<uint16_t>                  dacList;
    std::vector<DetectorDataContainer*>    detectorContainerVector;
    std::shared_ptr<DetectorDataContainer> theThresholdAndNoiseContainer;
    ContainerRecycleBin<OccupancyAndPh>    theRecyclingBin;

  protected:
    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    size_t      rowStart;
    size_t      rowStop;
    size_t      colStart;
    size_t      colStop;
    size_t      nEvents;
    size_t      startValue;
    size_t      stopValue;
    size_t      nSteps;
    size_t      offset;
    size_t      nHITxCol;
    size_t      doOnlyNGroups;
    bool        doDisplay;
    bool        doUpdateChip;
    bool        saveBinaryData;
    std::string dataOutputDir;

    std::string fileRes;
    int         theCurrentRun;
    bool        saveData;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
};

#endif
