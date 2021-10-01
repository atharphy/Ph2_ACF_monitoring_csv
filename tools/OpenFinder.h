/*!
 *
 * \file OpenFinder.h
 * \brief OpenFinder class, Finds opens in a 2S Front-End Hybrid
 * \author Stefano Mersi and Sarah Seif El Nasr Storey
 * \date 2019-11-12
 *
 * \Support : stefano.mersi@cern.ch
 *
 */

#ifndef OpenFinder_h__
#define OpenFinder_h__

#include "PSHybridTester.h"
#ifdef __USE_ROOT__
#include "TH2F.h"
#include "TTree.h"
#endif

#ifdef __ANTENNA__
#include "Antenna.h"
#endif
#ifdef __TCUSB__
#include "USB_a.h"
#endif

#define ADC_SLAVE 4
#define CLOCK_SLAVE 3
#define POTENTIOMETER_SLAVE 2
#define SWITCH_SLAVE 0

const float THRESHOLD_OPEN = 0.1;
#ifndef ChannelList
typedef std::vector<uint8_t> ChannelList;
#endif

typedef std::pair<uint16_t, float> ScanSummary;
typedef std::vector<ScanSummary>   ScanSummaries;

// add exit codes here
const uint8_t FAILED_LATENCY = 1;

class OpenFinder : public PSHybridTester
{
  public:
    // parameters for FindOpens
    class Parameters
    {
      public:
        uint32_t    nTriggers              = 0;
        uint8_t     antennaGroup           = 0;
        uint16_t    potentiometer          = 0;
        uint16_t    latencyRange           = 0;
        uint16_t    antennaDelay           = 0;
        uint32_t    triggerRate            = 0;
        uint8_t     fExternalTriggerSource = 5;
        uint8_t     fAntennaTriggerSource  = 7;
        float       fThreshold             = 1;
        std::string UsbId                  = "";
    };

    bool FindLatency(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint16_t> pLatencies);
    void CountOpens(Ph2_HwDescription::BeBoard* pBoard);
    OpenFinder();
    ~OpenFinder();
    void Initialise(Parameters pParameters);
    void FindOpens2S();
    void FindOpensPS();
    void FindOpens();
#ifdef __USE_ROOT__
    void Print();
#endif
    void Reset();

    void SelectAntennaPosition(const std::string& cPosition, uint16_t potentiometer = 0);

  private:
    // type aliases
    using channelVector      = std::vector<int>;
    using cbcChannelsMap     = std::map<int, channelVector>;
    using antennaChannelsMap = std::map<int, cbcChannelsMap>;

    // Settings
    bool               fTestPulse;
    uint8_t            fTestPulseAmplitude;
    uint32_t           fEventsPerPoint;
    uint8_t            fAntennaPosition;
    Parameters         fParameters;
    antennaChannelsMap returnAntennaMap();

    // Containers
    // Add detector container and shorts container
    DetectorDataContainer fLatencyContainer;
    DetectorDataContainer fOpens;
    DetectorDataContainer fHitDataContainer;
    // container to hold result of latency scan
    DetectorDataContainer fInTimeOccupancy;

    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;

    // #ifdef __TCUSB__
    std::map<std::string, TC_PSFE::ant_channel> fAntennaControl = {{"EvenChannels", TC_PSFE::ant_channel::_2},
                                                                   {"OddChannels", TC_PSFE::ant_channel::_1},
                                                                   {"Enable", TC_PSFE::ant_channel::ALL},
                                                                   {"Disable", TC_PSFE::ant_channel::NONE}};
    // #endif
};

#endif
