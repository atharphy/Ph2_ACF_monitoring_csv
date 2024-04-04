#include "tools/OTMeasureOccupancy.h"
#include "System/RegisterHelper.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/SSAChannelGroupHandler.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTMeasureOccupancy::fCalibrationDescription = "Measure channel occupancy";

OTMeasureOccupancy::OTMeasureOccupancy() : Tool() {}

OTMeasureOccupancy::~OTMeasureOccupancy() {}

void OTMeasureOccupancy::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfEvents    = findValueInSettings<double>("OTMeasureOccupancyNumberOfEvents", 10000);
    fCBCtestPulseValue = findValueInSettings<double>("OTMeasureOccupancyCBCtestPulseValue", 218);

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTMeasureOccupancy.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTMeasureOccupancy::ConfigureCalibration()
{

}

void OTMeasureOccupancy::Running()
{
    LOG(INFO) << "Starting OTMeasureOccupancy measurement.";
    Initialise();
    LOG(INFO) << "Done with OTMeasureOccupancy.";
    Reset();
}

void OTMeasureOccupancy::Stop(void)
{
    LOG(INFO) << "Stopping OTMeasureOccupancy measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTMeasureOccupancy.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTMeasureOccupancy stopped.";
}

void OTMeasureOccupancy::Pause()
{

}


void OTMeasureOccupancy::Resume()
{

}


void OTMeasureOccupancy::Reset()
{
    fRegisterHelper->restoreSnapshot();
}


void OTMeasureOccupancy::prepareOccupancyMeasurement2S()
{
    this->enableTestPulse(true);
    LOG(INFO) << BOLDBLUE << "OTMeasureOccupancy::injectionDelayScan2S - Scanning Delay for 2S module" << RESET;

    CBCChannelGroupHandler theChannelGroupHandler(std::bitset<NCHANNELS>(CBC_CHANNEL_GROUP_BITSET));
    theChannelGroupHandler.setChannelGroupParameters(16, 2);
    setChannelGroupHandler(theChannelGroupHandler);

    this->SetTestAllChannels(false);
    // Setting sparsification for simplicity
    for(auto theBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 0);
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, 0);
            }
        }
    }

    setSameDac("TestPulsePotNodeSel", fCBCtestPulseValue); // injected charge
}
