/*!
 *
 * \file PSCounterTest.h
 * \brief PSCounterTest class
 * \author Adam Kobert
 * \date 25/07/24
 *
 */

#ifndef PSCounterTest_h__
#define PSCounterTest_h__

#include "Utils/CommonVisitors.h"
#include "Utils/ContainerRecycleBin.h"
#include "Utils/Visitor.h"
#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramPSCounterTest.h"
#endif

namespace Ph2_HwInterface
{
  class D19cFWInterface;
}

class PSCounterTest : public Tool
{
  public:
    PSCounterTest();
    ~PSCounterTest();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void RunFast(uint16_t stripThreshold, uint16_t pixelThreshold, uint16_t delay = 0x3bf2);
    void Pause() override;
    void Resume() override;
    void Reset();

    static std::string fCalibrationDescription;

  private:
    std::vector<EventType> fEventTypes;
    DetectorDataContainer* fStubLogicValue;
    DetectorDataContainer* fHIPCountValue;
    DetectorDataContainer  fBoardRegContainer;
    bool fWithCBC = false;
    bool fWithSSA = false;
    bool fWithMPA = false;

    bool GetCounterData(Ph2_HwInterface::D19cFWInterface *theFWinterface, const std::string& theOutputFileName, int eventsPerPoint);
    // Settings
    bool fDisableStubLogic{true};
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramPSCounterTest fDQMHistogramPSCounterTest;
#endif
};

#endif
