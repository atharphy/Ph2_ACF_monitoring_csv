/*!
  \file                  RD53PixelAlive.h
  \brief                 Implementaion of PixelAlive scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53PixelAlive_H
#define RD53PixelAlive_H

#include "../Utils/GenericDataArray.h"
#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53PixelAliveHistograms.h"
#endif

// #########################
// # PixelAlive test suite #
// #########################
class PixelAlive : public CalibBase
{
  public:
    ~PixelAlive()
    {
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
    size_t getNumberIterations() override { return theChnGroupHandler->getNumberOfGroups() * nEvents / nEvtsBurst; }

    std::shared_ptr<DetectorDataContainer> analyze();

#ifdef __USE_ROOT__
    PixelAliveHistograms* histos;
#endif

  private:
    void fillHisto() override;

    std::shared_ptr<DetectorDataContainer> theOccContainer;
    DetectorDataContainer                  theBCIDContainer;
    DetectorDataContainer                  theTrgIDContainer;

  protected:
    size_t injType;
    enum INJtype
    {
        None,
        Analog,
        Digital
    };

    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;

    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t nEvtsBurst;
    size_t nTRIGxEvent;
    size_t nHITxCol;
    float  occPerPixel;
    bool   unstuckPixels;
    size_t doOnlyNGroups;
    bool   doDisplay;
    bool   doUpdateChip;
    bool   saveBinaryData;

    std::string fileRes;
    int         theCurrentRun;
    bool        saveData;

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
};

#endif
