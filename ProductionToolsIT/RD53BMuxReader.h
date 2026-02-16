/*!
  \file                  RD53BMuxReader.h
  \brief
  \author
  \version               1.0
  \date
  Support:               none
*/

#ifndef RD53BMuxReader_H
#define RD53BMuxReader_H

#include "../tools/RD53CalibBase.h"
#include "../HWInterface/RD53Interface.h"
#include "HWDescription/RD53.h"

#ifdef __USE_ROOT__
#include "DQMUtils/RD53VoltageTuningHistograms.h"
#endif

// #############################
// # #
// #############################
class RD53BMuxReader : public CalibBase
{
  public:
    ~RD53BMuxReader()
    {
        // this->WriteRootFile();
        //  this->CloseResultFile();
        //   delete histos;
    }

  
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void localConfigure(const std::string& histoFileName = "", int currentRun = -1) override;
    void run() override;
    void draw(bool saveData = true) override;

    void analyze();
    void configure(const std::string & args);

    struct adc_result{
      float value;
      float error;
      std::string line;
    };
  
    adc_result get_adc(Ph2_HwInterface::RD53Interface* chipInterface, Ph2_HwDescription::ReadoutChip* chip, const std::string & name, unsigned int nsample);
  
  private:
    void fillHisto() override;
  protected:
    int theCurrentRun;
    std::vector<std::string> muxlist;
    bool use_wlt_calibration;
    bool read_temperatures;
};

#endif
