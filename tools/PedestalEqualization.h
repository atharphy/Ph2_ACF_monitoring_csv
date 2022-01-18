/*!
 *
 * \file PedestalEqualization.h
 * \brief PedestalEqualization class, PedestalEqualization of the hardware
 * \author Georg AUZINGER
 * \date 13 / 11 / 15
 *
 * \Support : georg.auzinger@cern.ch
 *
 */

#ifndef PedestalEqualization_h__
#define PedestalEqualization_h__

#include "../Utils/CommonVisitors.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/Visitor.h"
#include "Tool.h"
#include <map>

#ifdef __USE_ROOT__
#include "../DQMUtils/DQMHistogramPedestalEqualization.h"
#include "TString.h"
#endif

class PedestalEqualization : public Tool
{
  public:
    PedestalEqualization();
    ~PedestalEqualization();

    void Initialise(bool pAllChan = false, bool pDisableStubLogic = true);
    void FindVplus();
    // offsets are found by taking pMultiple*fEvents triggers
    void FindOffsets();
    void writeObjects();

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

  private:
    std::vector<EventType> fEventTypes;
    // Settings
    bool     fTestPulse{false};
    uint8_t  fTestPulseAmplitude{0};
    uint32_t fEventsPerPoint{10};
    uint16_t fTargetVcth{0x0};
    uint8_t  fTargetOffset{0x80};
    bool     fCheckLoop{true};
    bool     fAllChan{true};
    bool     fDisableStubLogic{true};
    uint32_t fMaxNevents{65535};
    int      fNEventsPerBurst{-1};
    float    fOccupancyAtPedestal{0.56};
    uint8_t  fUseMean{1};

    // to hold the original register values
    // DetectorDataContainer fStubLogicCointainer;
    // DetectorDataContainer fHIPCountCointainer;
    DetectorDataContainer fBoardRegContainer;
    bool                  cWithCBC = true;
    bool                  cWithSSA = false;
    bool                  cWithMPA = false;

#ifdef __USE_ROOT__
    DQMHistogramPedestalEqualization fDQMHistogramPedestalEqualization;
#endif
};

#endif
