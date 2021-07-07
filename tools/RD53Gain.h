/*!
  \file                  RD53Gain.h
  \brief                 Implementaion of Gain scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Gain_H
#define RD53Gain_H

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/GainFit.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53GainHistograms.h"
#include "TApplication.h"
#endif

// #############
// # CONSTANTS #
// #############
#define NGAINPAR 4 // Number of parameters for gain data regression

// ###################
// # Gain test suite #
// ###################
class Gain : public Tool
{
  public:
    ~Gain()
    {
        for(auto container: detectorContainerVector) theRecyclingBin.free(container);
#ifdef __USE_ROOT__
        if(saveData == true) this->WriteRootFile();
        this->CloseResultFile();
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void                                   localConfigure(const std::string fileRes_ = "", int currentRun = -1);
    void                                   initializeFiles(const std::string fileRes_ = "", int currentRun = -1);
    void                                   run();
    void                                   draw(bool saveData = true);
    std::shared_ptr<DetectorDataContainer> analyze();
    size_t getNumberIterations() { return RD53ChannelGroupHandler::getNumberOfGroups(doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol) * nSteps; }
    void   saveChipRegisters(int currentRun);

    static float gainFunction(const std::vector<float>& par, float q) { return par[0] + par[1] * q; }

#ifdef __USE_ROOT__
    GainHistograms* histos;
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
    size_t nSteps;
    size_t offset;
    size_t nHITxCol;
    bool   doFast;

    std::vector<uint16_t> dacList;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    std::vector<DetectorDataContainer*>      detectorContainerVector;
    std::shared_ptr<DetectorDataContainer>   theGainContainer;
    ContainerRecycleBin<OccupancyAndPh>      theRecyclingBin;

    void fillHisto();
    void computeStats(const std::vector<float>& x,
                      const std::vector<float>& y,
                      const std::vector<float>& e,
                      const std::vector<float>& o,
                      std::vector<float>&       par,
                      std::vector<float>&       parErr,
                      float&                    chi2,
                      float&                    DoF);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
    bool        saveData;
};

#endif
