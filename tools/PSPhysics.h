/*!
  \file                  PSPhysics.h
  \brief                 Implementaion of PSPhysics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef PSPhysics_H
#define PSPhysics_H

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Event.h"
#include "Tool.h"
// #include "../Utils/PSSharedConstants.h"
// #include "../HWInterface/PSFWInterface.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/PSPhysicsHistograms.h"
#include "TApplication.h"
#endif

// #############
// # CONSTANTS #
// #############
#define RESULTDIR "Results" // Directory containing the results

// #######################
// # PSPhysics data taking #
// #######################
class PSPhysics : public Tool
{
  public:
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;

    void sendBoardData(BoardContainer* const& cBoard);
    void initialize(const std::string fileRes_, const std::string fileReg_);
    void run();
    void draw();
    void fillDataContainer(BoardContainer* const& cBoard, Ph2_HwInterface::Event* event);

  private:
    DetectorDataContainer fPSSyncContainer;

    void initHisto();
    void fillHisto();
    void display();
    void chipErrorReport();

    // ########
    // # ROOT #
    // ########
#ifdef __USE_ROOT__
    PSPhysicsHistograms histos;
    TApplication*        myApp;
#endif

  protected:
    std::string fileRes;
    std::string fileReg;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveRawData;
    bool        doLocal;
};

#endif
