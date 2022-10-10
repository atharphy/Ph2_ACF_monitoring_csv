/*!

        \file                   KIRA.h
        \brief                  Class to control the KIRA systems for 2S Modules
        \author                 Roland Koppenhöfer / Stefan Maier
        \version                1.0
        \date                   03/05/2022
        Support :               mail to : s.maier@kit.edu

 */
#ifndef KIRA_h__
#define KIRA_h__

#include "OTTool.h"
#include "../NetworkUtils/TCPClient.h"
#include "../NetworkUtils/TCPPublishServer.h"
#include "../Utils/ContainerRecycleBin.h"

#include <fstream>
#include <iostream>
#include <map>
#include <math.h>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/time.h>

#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramKira.h"
#endif


class Occupancy;

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

class KIRA : public OTTool
{
  public:
    KIRA();
    ~KIRA();
    // void IntensityCalibration();
    void Initialise(int kiraPort, std::string kiraId);
    void PrepareForExternal(BeBoard* pBoard);
    void determineLatency();
    void performKIRATest();
    void initializeRecycleBin() { fRecycleBin.setDetectorContainer(fDetectorContainer); }
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

  private:
    TCPClient* fKiraClient{nullptr};
    std::string fKiraId;
    ContainerRecycleBin<Occupancy> fRecycleBin;
    DQMHistogramKira fDQMHistogrammer;
    DetectorDataContainer analyseEvents(BeBoard* pBoard, const std::vector<Event*>& pEvents, uint16_t pSensor, uint16_t pLED);

};

#endif
