/*!
  \file                  RD53VoltageTuning.h
  \brief                 Implementaion of Voltage Tuning
  \author                Yuta TAKAHASHI
  \version               1.0
  \date                  03/05/21
  Support:               email to Yuta.Takahashi@cern.ch
*/

#ifndef RD53VoltageTuning_H
#define RD53VoltageTuning_H

#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53VoltageTuningHistograms.h"
#endif

// #############################
// # Voltage tuning test suite #
// #############################
class VoltageTuning : public CalibBase
{
  public:
    ~VoltageTuning()
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

    void localConfigure(const std::string& fileRes_ = "", int currentRun = -1);
    void initializeFiles(const std::string& fileRes_ = "", int currentRun = -1);
    void run();
    void draw();
    void analyze();

#ifdef __USE_ROOT__
    VoltageTuningHistograms* histos;
#endif

  private:
    DetectorDataContainer theAnaContainer;
    DetectorDataContainer theDigContainer;

    void             fillHisto();
    std::vector<int> createScanRange(Ph2_HwDescription::Chip* pChip, const std::string regName, float target, float initial);

  protected:
    float targetDig;
    float targetAna;
    float toleranceDig;
    float toleranceAna;
    bool  doDisplay;

    std::string fileRes;
    int         theCurrentRun;
};

#endif
