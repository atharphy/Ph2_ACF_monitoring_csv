/*!

        \file                   ExtTriggerLatencyScan.h
        \brief                 class to do latency and threshold scans
        \author                 Stefan Maier
        \version                1.0
        \date                   25/07/24
        Support :               mail to : stefan.maier@cern.ch

 */

#ifndef EXTTRIGGERLATENCYSCAN_H__
#define EXTTRIGGERLATENCYSCAN_H__

#include "Utils/CommonVisitors.h"
#include "Utils/ContainerRecycleBin.h"
#include "Utils/Visitor.h"
#include "tools/Tool.h"
#include "tools/LatencyScan.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramLatencyScan.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TGaxis.h"
#include "TH1F.h"
#include "TH2F.h"
#endif

namespace Ph2_HwDescription
{
class BeBoard;
}

/*!
 * \class ExtTriggerLatencyScan
 * \brief Class to perform latency and threshold scans using external triggers. Inherits from Latency scan, only differs in the initialisation process
 */
class Occupancy;

class ExtTriggerLatencyScan : public LatencyScan
{
  public:
    ExtTriggerLatencyScan();
    ~ExtTriggerLatencyScan();
    //
    static std::string fCalibrationDescription;

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;


  protected:

  private:

    void InitializeExternalTriggers();

};

#endif
