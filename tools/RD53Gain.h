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

#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/GainFit.h"
#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53GainHistograms.h"
#endif

// #############
// # CONSTANTS #
// #############
#define NGAINPAR 4 // Number of parameters for gain data regression

// ###################
// # Gain test suite #
// ###################
class Gain : public CalibBase
{
  public:
    ~Gain()
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
    void   draw(bool saveData = true) override;
    size_t getNumberIterations() override { return theChnGroupHandler->getNumberOfGroups() * nSteps; }

    std::shared_ptr<DetectorDataContainer> analyze();
    static float                           gainFunction(const std::vector<float>& par, float q, const Ph2_HwDescription::RD53::FrontEnd* frontEnd);
    static float                           gainInverseFunction(const std::vector<float>& par, float ToT, const Ph2_HwDescription::RD53::FrontEnd* frontEnd);

#ifdef __USE_ROOT__
    GainHistograms* histos;
#endif

  private:
    void fillHisto() override;
    void computeStats(const std::vector<float>& x,
                      const std::vector<float>& y,
                      const std::vector<float>& e,
                      const std::vector<float>& o,
                      std::vector<float>&       par,
                      std::vector<float>&       parErr,
                      float&                    chi2,
                      float&                    DoF);

    std::vector<uint16_t>                  dacList;
    std::vector<DetectorDataContainer*>    detectorContainerVector;
    std::shared_ptr<DetectorDataContainer> theGainContainer;
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
    float       targetCharge;
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
