#include "../tools/CBCPulseShape.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/Exception.h"
#include "../Utils/Occupancy.h"
#include "../Utils/ThresholdAndNoise.h"
#include "../Utils/Utilities.h"

#include <math.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

CBCPulseShape::CBCPulseShape() : PedeNoise() {}

CBCPulseShape::~CBCPulseShape() { delete fChannelGroupHandler; }

void CBCPulseShape::Initialise(void)
{
    fEventsPerPoint        = findValueInSettings<double>("PulseShapeNevents", 10);
    fInitialLatency        = findValueInSettings<double>("PulseShapeInitialLatency", 200);
    fInitialDelay          = findValueInSettings<double>("PulseShapeInitialDelay", 0);
    fFinalDelay            = findValueInSettings<double>("PulseShapeFinalDelay", 25);
    fDelayStep             = findValueInSettings<double>("PulseShapeDelayStep", 1);
    fPulseAmplitude        = findValueInSettings<double>("PulseShapePulseAmplitude", 150);
    fChannelGroup          = findValueInSettings<double>("PulseShapeChannelGroup", -1);
    fPlotPulseShapeSCurves = findValueInSettings<double>("PlotPulseShapeSCurves", 0);

    fLimit = 0.02; // larger tollerance for SCurve limits

    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;

    if(fChannelGroup >= 8) throw Exception(std::string(__PRETTY_FUNCTION__) + " fChannelGroup cannot be grater than 7");
    if(fChannelGroup < 0)
        fChannelGroupHandler = new CBCChannelGroupHandler();
    else
        fChannelGroupHandler = new CBCChannelGroupHandler(std::bitset<NCHANNELS>(CBC_CHANNEL_GROUP_BITSET) << (fChannelGroup * 2));

    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    initializeRecycleBin();

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fCBCHistogramPulseShape.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void CBCPulseShape::runCBCPulseShape(void)
{
    LOG(INFO) << "Taking Data with " << fEventsPerPoint << " triggers!";

    this->enableTestPulse(true);
    setFWTestPulse();
    disableStubLogic();

    setSameDac("TestPulsePotNodeSel", fPulseAmplitude);
    setSameDac("TriggerLatency", fInitialLatency);

    // setSameGlobalDac("TestPulsePotNodeSel",  pTPAmplitude);
    LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;

    for(uint16_t delay = fInitialDelay; delay <= fFinalDelay; delay += fDelayStep)
    {
        uint16_t delayDAC   = 25 - (delay % 25);
        uint16_t latencyDAC = fInitialLatency - (delay / 25);
        if(delayDAC == 25)
        {
            delayDAC   = 0;
            latencyDAC = latencyDAC + 1;
        }
        LOG(INFO) << BOLDBLUE << "Scanning VcThr for delay = " << +delayDAC << " and latency = " << +latencyDAC << RESET;

        setSameDac("TestPulseDelay", delayDAC);
        setSameDac("TriggerLatency", latencyDAC);

        measureSCurves(findPedestal());
        extractPedeNoise();

#ifdef __USE_ROOT__
        LOG(INFO) << BOLDGREEN << "Plotting delay for " << +delay << RESET;
        fCBCHistogramPulseShape.fillCBCPulseShapePlots(delay, *fThresholdAndNoiseContainer);
        if(fPlotPulseShapeSCurves)
            for(auto& scurveOccupancy: fSCurveOccupancyMap) { fCBCHistogramPulseShape.fillSCurvePlots(scurveOccupancy.first, latencyDAC, delayDAC, *scurveOccupancy.second); }
#else
        if(fStreamerEnabled)
        {
            auto theThresholdAndNoiseStream = prepareChipContainerStreamer<ThresholdAndNoise, ThresholdAndNoise, uint16_t>();
            theThresholdAndNoiseStream.setHeaderElement<0>(delay);

            for(auto board: *fThresholdAndNoiseContainer) { theThresholdAndNoiseStream.streamAndSendBoard(board, fNetworkStreamer); }

            for(auto& scurveOccupancy: fSCurveOccupancyMap)
            {
                auto theScurveOccupancyStream = prepareChannelContainerStreamer<Occupancy, uint16_t, uint16_t, uint16_t>("SCurve");
                theScurveOccupancyStream.setHeaderElement<0>(scurveOccupancy.first);
                theScurveOccupancyStream.setHeaderElement<1>(latencyDAC);
                theScurveOccupancyStream.setHeaderElement<2>(delayDAC);
                for(auto board: *scurveOccupancy.second) { theScurveOccupancyStream.streamAndSendBoard(board, fNetworkStreamer); }
            }
        }
#endif
        fThresholdAndNoiseContainer->reset();
        cleanContainerMap();
    }

    reloadStubLogic();
    this->enableTestPulse(false);
    setSameGlobalDac("TestPulsePotNodeSel", 0);
    LOG(INFO) << BLUE << "Disabled test pulse. " << RESET;
}

void CBCPulseShape::writeObjects()
{
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fCBCHistogramPulseShape.process();
#endif
}

// For system on chip compatibility
void CBCPulseShape::Running()
{
    LOG(INFO) << "Starting calibration example measurement.";
    Initialise();
    runCBCPulseShape();
    LOG(INFO) << "Done with calibration example.";
}

// For system on chip compatibility
void CBCPulseShape::Stop(void)
{
    LOG(INFO) << "Stopping calibration example measurement.";
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "Calibration example stopped.";
}
