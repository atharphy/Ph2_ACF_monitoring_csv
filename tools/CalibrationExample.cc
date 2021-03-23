#include "../tools/CalibrationExample.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include <math.h>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

CalibrationExample::CalibrationExample() : Tool(), fEventsPerPoint(0) {}

CalibrationExample::~CalibrationExample() {}

void CalibrationExample::Initialise(void)
{
    auto cSetting   = fSettingsMap.find("Nevents");
    fEventsPerPoint = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 10;

    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramCalibrationExample.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void CalibrationExample::runCalibrationExample(void)
{
    LOG(INFO) << "Taking Data with " << fEventsPerPoint << " triggers!";

    DetectorDataContainer theHitContainer;
    ContainerFactory::copyAndInitChannel<uint32_t>(*fDetectorContainer, theHitContainer);

    // getting n events and filling the container:
    for(auto board: theHitContainer) // for on boards - begin
    {
        BeBoard* theBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(board->getIndex()));
        // Send N triggers (as it was in the past)
        ReadNEvents(theBeBoard, fEventsPerPoint);
        // Get the event vector (as it was in the past)

        const std::vector<Event*>& eventVector = GetEvents();

        for(auto& event: eventVector) // for on events - begin
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup) // for on hybrid - begin
                {
                    for(auto chip: *hybrid) // for on chip - begin
                    {
                        unsigned int channelNumber = 0;
                        for(auto& channel: *chip->getChannelContainer<uint32_t>()) // for on channel - begin
                        {
                            // retreive data in the old way and add to the current number of hits of the corresponding
                            // channel
                            channel += event->DataBit(hybrid->getId(), chip->getId(), channelNumber++);
                        } // for on channel - end
                    }     // for on chip - end
                }         // for on hybrid - end
            }             // for on opticalGroup - end
        }                 // for on events - end
    }                     // for on board - end

#ifdef __USE_ROOT__
      // Calibration is not running on the SoC: plotting directly the data, no shipping is done
    fDQMHistogramCalibrationExample.fillCalibrationExamplePlots(theHitContainer);
#else
      // Calibration is running on the SoC: shipping the data!!!
    // I prepare a stream of an uint32_t container, prepareChannelContainerStreamer adds in the stream also the
    // calibration name
    // that is used when multiple calibrations are concatenated
    auto theHitStream = prepareChannelContainerStreamer<uint32_t>();
    // if the streamer was enabled (the supervisor script enable it) data are streamed
    if(fStreamerEnabled)
    {
        // Disclamer: final MW will not do a for loop on board since each instance will hanlde 1 board only
        for(auto board: theHitContainer) theHitStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif
}

void CalibrationExample::writeObjects()
{
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramCalibrationExample.process();
#endif
}

// For system on chip compatibility
void CalibrationExample::Running()
{
    LOG(INFO) << "Starting calibration example measurement.";
    Initialise();
    runCalibrationExample();
    LOG(INFO) << "Done with calibration example.";
}

// For system on chip compatibility
void CalibrationExample::Stop(void)
{
    LOG(INFO) << "Stopping calibration example measurement.";
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "Calibration example stopped.";
}
