/*!
 *
 * \file BeamTestCommissioning.h
 * \brief BeamTest commissioning class to perfrom hit and stub latencies
 * \author Younes Otarid
 * \date 04 / 05 / 22
 *
 * \Support : younes.otarid@cern.ch
 *
*/

#ifndef BeamTestCommissioning_h__
#define BeamTestCommissioning_h__

#include "../Utils/ContainerRecycleBin.h"
#include "OTTool.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/SSAChannelGroupHandler.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"

// #ifdef __USE_ROOT__
// #include "../DQMUtils/DQMHistogramBeamTestCheck.h"
// #include "TH1.h"
// #endif

class Occupancy;

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

class BeamTestCommissioning : public OTTool
{
  public:
    BeamTestCommissioning();
    ~BeamTestCommissioning();

    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void Finalise();

    void SetInternalUserTrigger();
    void SetInternalTestPulseTrigger();
    void SetExternalTestPulseTrigger();
    void SetExternalTestPulseTriggerTLU();
    void SetExternalBeamTriggerTLU();

    void ScanL1Latency();
    void ScanStubLatency();

  private: 
    void InitialiseContainers();
    void InitialiseInjection();
    void ConfigureTriggerScheme(Ph2_HwDescription::BeBoard* pBoard, bool pInternal = true, bool pEnaleTP = true, bool pEnableTLU = false);
    void ExtractHitCounts(const std::vector<Event*> pEvents, size_t pTriggerId);
    void ExtractStubCounts(const std::vector<Event*> pEvents, size_t pTriggerId);

  private:
    uint8_t fTPamplitude, fTPdelay;
    uint16_t fStripThreshold, fPixelThreshold, fStartLatency, fLatencyRange;
    DetectorDataContainer fL1LatencyContainer, fStubLatencyContainer;
    DetectorDataContainer fL1LatencyHitCountContainer, fStubLatencyStubCountContainer;
    DetectorDataContainer fHitCountContainer, fStubCountContainer;
    DetectorDataContainer fOptimalL1LatencyContainer, fOptimalStubLatencyContainer;
    DetectorDataContainer fTriggerMultiplicityContainer;
    std::vector<Ph2_HwInterface::Injection> fInjections;
// #ifdef __USE_ROOT__
//     DQMHistogramBeamTestCommissioning fQDMHistogramer;
// #endif
};

#endif