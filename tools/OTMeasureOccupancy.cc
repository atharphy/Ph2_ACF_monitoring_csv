#include "tools/OTMeasureOccupancy.h"
#include "System/RegisterHelper.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
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

    fNumberOfEvents    = findValueInSettings<double>("OTMeasureOccupancy_NumberOfEvents", 10000);
    fCBCtestPulseValue = findValueInSettings<double>("OTMeasureOccupancy_CBCtestPulseValue", 218);
    fSSAtestPulseValue = findValueInSettings<double>("OTMeasureOccupancy_SSAtestPulseValue", 90);
    fMPAtestPulseValue = findValueInSettings<double>("OTMeasureOccupancy_MPAtestPulseValue", 100);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTMeasureOccupancy.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTMeasureOccupancy::ConfigureCalibration() {}

void OTMeasureOccupancy::Running()
{
    LOG(INFO) << "Starting OTMeasureOccupancy measurement.";
    Initialise();
    measureChannelOccupancy();
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

void OTMeasureOccupancy::Pause() {}

void OTMeasureOccupancy::Resume() {}

void OTMeasureOccupancy::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTMeasureOccupancy::measureChannelOccupancy()
{
    bool is2SModule = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S;

    if(is2SModule) prepareOccupancyMeasurement2S();

    DetectorDataContainer theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, theOccupancyContainer);
    fDetectorDataContainer = &theOccupancyContainer;
    measureData(fNumberOfEvents, 65535);

#ifdef __USE_ROOT__
    fDQMHistogramOTMeasureOccupancy.fillOccupancy(theOccupancyContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theOccupancyContainerSerialization("OTMeasureOccupancyOccupancy");
        theOccupancyContainerSerialization.streamByHybridContainer(fDQMStreamer, theOccupancyContainer);
    }
#endif
}

void OTMeasureOccupancy::prepareOccupancyMeasurement2S()
{
    LOG(INFO) << BOLDBLUE << "OTMeasureOccupancy::prepareOccupancyMeasurement2S - Preparing 2S to measure occupancy with injection = " << +fCBCtestPulseValue << RESET;

    CBCChannelGroupHandler theChannelGroupHandler;
    theChannelGroupHandler.setChannelGroupParameters(16, 1, 2);
    setChannelGroupHandler(theChannelGroupHandler);

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
    bool injectPulse       = fCBCtestPulseValue != 0;
    bool injectAllChannels = !injectPulse;
    this->enableTestPulse(injectPulse);
    this->setTestAllChannels(injectAllChannels);
}