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

#include "../HWDescription/RD53.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53PixelAliveHistograms.h"
#include "TApplication.h"
#endif

// #########################
// # PixelAlive test suite #
// #########################
class PixelAlive : public Tool
{
  public:
    ~PixelAlive()
    {
#ifdef __USE_ROOT__
        if(saveData == true) this->WriteRootFile();
        this->CloseResultFile();
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void                                   localConfigure(const std::string& fileRes_ = "", int currentRun = -1);
    void                                   initializeFiles(const std::string& fileRes_ = "", int currentRun = -1);
    void                                   run();
    void                                   draw(bool doSaveData = true);
    std::shared_ptr<DetectorDataContainer> analyze();
    size_t                                 getNumberIterations() { return theChnGroupHandler->getNumberOfGroups() * nEvents / nEvtsBurst; }
    void                                   saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    PixelAliveHistograms* histos;
#endif

  private:
    bool unstuckPixels;

    std::shared_ptr<DetectorDataContainer> theOccContainer;
    DetectorDataContainer                  theBCIDContainer;
    DetectorDataContainer                  theTrgIDContainer;

    void fillHisto();
    void chipErrorReport() const;

  protected:
    size_t injType;
    enum INJtype
    {
        None,
        Analog,
        Digital
    };

    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t nEvtsBurst;
    size_t nTRIGxEvent;
    size_t nHITxCol;
    float  thrOccupancy;
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
