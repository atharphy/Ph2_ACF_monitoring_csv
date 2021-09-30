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

    void                                   localConfigure(const std::string fileRes_ = "", int currentRun = -1);
    void                                   initializeFiles(const std::string fileRes_ = "", int currentRun = -1);
    void                                   run();
    void                                   draw(bool doSaveData = true);
    std::shared_ptr<DetectorDataContainer> analyze();
    size_t                                 getNumberIterations()
    {
        return RD53ChannelGroupHandler::getNumberOfGroups(injType != INJtype::None ? (doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups) : RD53GroupType::AllPixels, nHITxCol) *
               nEvents / nEvtsBurst;
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    PixelAliveHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t nEvents;
    size_t nEvtsBurst;
    size_t injType;
    size_t nHITxCol;
    float  thrOccupancy;
    bool   unstuckPixels;
    enum INJtype
    {
        None,
        Analog,
        Digital
    };

    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    std::shared_ptr<DetectorDataContainer>   theOccContainer;
    DetectorDataContainer                    theBCIDContainer;
    DetectorDataContainer                    theTrgIDContainer;

    void fillHisto();
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        doFast;
    bool        saveBinaryData;
    bool        saveData;
};

#endif
