/*!
  \file                  RD53DataTransmissionTest.h
  \brief                 TAP0 scan to measure the Bit Error Rate and determine data transmission quality
  \author                Marijus AMBROZAS
  \version               1.0
  \date                  26/04/20
  Support:               email to marijus.ambrozas@cern.ch
*/

#ifndef RD53DataTransmissionTest_H
#define RD53DataTransmissionTest_H

#include "RD53BERtest.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53DataTransmissionTestGraphs.h"
#include "TApplication.h"
#endif

// #########################################
// # Data Readback Optimization test suite #
// #########################################
class DataTransmissionTest : public BERtest
{
  public:
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void localConfigure(const std::string fileRes_, int currentRun);
    void initializeFiles(const std::string fileRes_, int currentRun);
    void run();
    void draw(bool saveData = true);
    void analyze();
    void analyze(const DetectorDataContainer& theTAP0scanContainer, DetectorDataContainer& theTAP0tgtContainer);

#ifdef __USE_ROOT__
    //    DataTransmissionTestHistograms* histos;
    DataTransmissionTestGraphs* histos;
#endif

  private:
    double BERtarget;
    bool   given_time;
    double frames_or_time;

    DetectorDataContainer theTAP0scanContainer;
    DetectorDataContainer theTAP0tgtContainer;

    void fillHisto();
    void binSearch(DetectorDataContainer* theTAP0scanContainer);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doDisplay;
};

#endif
