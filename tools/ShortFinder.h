/*!
 *
 * \file ShortFinder.h
 * \brief ShortFinder class, Finds shorts in a 2S Front-End Hybrid
 * \author Stefano Mersi and Sarah Seif El Nasr Storey
 * \date 2019-11-12
 *
 * \Support : stefano.mersi@cern.ch
 *
 */

#ifndef ShortFinder_h__
#define ShortFinder_h__

#include "Tool.h"

const float THRESHOLD_SHORT = 0.1;

typedef std::vector<uint8_t> ChannelList;

// add exit codes here
const uint8_t FAILED_INJECTION = 1;

class ShortFinder : public Tool
{
  public:
    ShortFinder();
    ~ShortFinder();
    void Initialise();

    void Count(Ph2_HwDescription::BeBoard* pBoard, const ChannelGroup<NCHANNELS>* pGroup);
    // void Count(Ph2_HwDescription::BeBoard* pBoard, const ChannelGroup<NSSACHANNELS>* pGroup);

    void FindShorts2S(Ph2_HwDescription::BeBoard* pBoard);
    void FindShortsPS(Ph2_HwDescription::BeBoard* pBoard);
    void FindShorts();
    void Print();
    void Reset();
    void Running() override;
    void Stop() override;

  private:
    // Settings
    bool     fTestPulse;
    uint8_t  fTestPulseAmplitude;
    float    fThreshold;
    uint32_t fEventsPerPoint;

    // Containers
    // Add detector container and shorts container
    DetectorDataContainer fShorts;
    DetectorDataContainer fInjections;
    DetectorDataContainer fShortsContainer;
    DetectorDataContainer fHitsContainer;

    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;

    float THRESHOLD_IN = 0.0;

    bool fWithCBC = false;
    bool fWithSSA = false;
};

#endif
