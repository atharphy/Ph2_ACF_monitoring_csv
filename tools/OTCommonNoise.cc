#include "../tools/OTCommonNoise.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTCommonNoise::OTCommonNoise() : Tool() {}

OTCommonNoise::~OTCommonNoise() {}

void OTCommonNoise::Initialise(void)
{
   

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCommonNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCommonNoise::ConfigureCalibration()
{

}

void OTCommonNoise::Running()
{
    LOG(INFO) << "Starting OTCommonNoise measurement.";
    Initialise();
    LOG(INFO) << "Done with OTCommonNoise.";
}

void OTCommonNoise::Stop(void)
{
    LOG(INFO) << "Stopping OTCommonNoise measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTCommonNoise.process();
    #endif
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCommonNoise stopped.";
}

void OTCommonNoise::Pause()
{

}


void OTCommonNoise::Resume()
{

}
