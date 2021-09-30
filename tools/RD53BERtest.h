/*!
  \file                  RD53BERtest.h
  \brief                 Implementaion of Bit Error Rate test
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53BERtest_H
#define RD53BERtest_H

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53BERtestHistograms.h"
#include "TApplication.h"
#endif

// ##################
// # BER test suite #
// ##################
class BERtest : public Tool
{
  public:
    ~BERtest()
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

    void localConfigure(const std::string fileRes_ = "", int currentRun = -1);
    void initializeFiles(const std::string fileRes_ = "", int currentRun = -1);
    void run();
    void draw();

#ifdef __USE_ROOT__
    BERtestHistograms* histos;
#endif

  private:
    size_t chain2test;
    bool   given_time;
    double frames_or_time;

    void fillHisto();

  protected:
    DetectorDataContainer theBERtestContainer;

    std::string fileRes;
    int         theCurrentRun;
    bool        doDisplay;
};

#endif
