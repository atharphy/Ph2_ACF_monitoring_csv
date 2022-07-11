#include "../tools/CheckCbcNeighbors.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

CheckCbcNeighbors::CheckCbcNeighbors() : Tool() {}

CheckCbcNeighbors::~CheckCbcNeighbors() {}

void CheckCbcNeighbors::Initialise(void)
{
   

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramCheckCbcNeighbors.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void CheckCbcNeighbors::ConfigureCalibration()
{

}

void CheckCbcNeighbors::Running()
{
    LOG(INFO) << "Starting CheckCbcNeighbors measurement.";
    Initialise();
    LOG(INFO) << "Done with CheckCbcNeighbors.";
}

void CheckCbcNeighbors::Stop(void)
{
    LOG(INFO) << "Stopping CheckCbcNeighbors measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramCheckCbcNeighbors.process();
    #endif
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "CheckCbcNeighbors stopped.";
}

void CheckCbcNeighbors::Pause()
{

}


void CheckCbcNeighbors::Resume()
{

}
