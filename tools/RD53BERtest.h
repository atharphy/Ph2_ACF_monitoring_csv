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

#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53BERtestHistograms.h"
#endif

// ##################
// # BER test suite #
// ##################
class BERtest : public CalibBase
{
  public:
    ~BERtest()
    {
#ifdef __USE_ROOT__
        this->WriteRootFile();
        this->CloseResultFile();
        delete histos;
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void localConfigure(const std::string& fileRes_ = "", int currentRun = -1) override;
    void initializeFiles(const std::string& fileRes_ = "", int currentRun = -1) override;
    void run() override;
    void draw(bool saveData = true) override;

#ifdef __USE_ROOT__
    BERtestHistograms* histos;
#endif

  private:
    void fillHisto() override;

  protected:
    size_t      chain2test;
    bool        given_time;
    double      frames_or_time;
    bool        doDisplay;
    std::string dataOutputDir;

    std::string fileRes;
    int         theCurrentRun;

    DetectorDataContainer theBERtestContainer;
};

#endif
