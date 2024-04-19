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
    this->setNormalization(true);

    if(is2SModule) prepareOccupancyMeasurement2S();
    else prepareOccupancyMeasurementPS();

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

void OTMeasureOccupancy::prepareOccupancyMeasurementPS()
{
    LOG(INFO) << BOLDBLUE << "OTMeasureOccupancy::prepareOccupancyMeasurementPS - Preparing 2S to measure occupancy with pixel injection = " << +fMPAtestPulseValue << " and strip injection = " << +fSSAtestPulseValue << RESET;

    // for(auto theBoard: *fDetectorContainer)
    // {
    //     fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0);
    // }

    SSAChannelGroupHandler theSSAChannelGroupHandler;
    theSSAChannelGroupHandler.setChannelGroupParameters(15, 1, 1);
    setChannelGroupHandler(theSSAChannelGroupHandler, FrontEndType::SSA2);

    MPAChannelGroupHandler theMPAChannelGroupHandler;
    theMPAChannelGroupHandler.setChannelGroupParameters(15, 1, 1);
    setChannelGroupHandler(theMPAChannelGroupHandler, FrontEndType::MPA2);

    bool injectSSApulse       = fSSAtestPulseValue != 0;
    bool injectMPApulse       = fMPAtestPulseValue != 0;

    auto        MPAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string theMPAqueryFunctionString = "MPAqueryFunction";
    // settings for MPAs
    fDetectorContainer->addReadoutChipQueryFunction(MPAqueryFunction, theMPAqueryFunctionString);
    setSameDac("Control_1", 0x0);                     // set Readout mode to normal
    setSameDac("InjectedCharge", fMPAtestPulseValue); // injected charge
    fDetectorContainer->removeReadoutChipQueryFunction(theMPAqueryFunctionString);

    auto        SSAqueryFunction          = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string theSSAqueryFunctionString = "SSAqueryFunction";
    // settings for SSAs
    fDetectorContainer->addReadoutChipQueryFunction(SSAqueryFunction, theSSAqueryFunctionString);
    setSameDac("InjectedCharge", fSSAtestPulseValue); // injected charge
    setSameDac("ReadoutMode", 0x0);                   // normal readout mode
    setSameDac("CalPulse_duration", 1);               // set calpulse duration to 1 40MHz clock cycle
    fDetectorContainer->removeReadoutChipQueryFunction(theSSAqueryFunctionString);

    bool injectPulse       = injectSSApulse || injectMPApulse;
    bool injectAllChannels = !injectPulse;
    setFWTestPulse(injectPulse);
    this->setTestAllChannels(injectAllChannels);

    auto firstSSA = fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(0);
    auto firstMPA = fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8);

    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] SSA TriggerLatency = " << fReadoutChipInterface->ReadChipReg(firstSSA, "TriggerLatency") << std::endl;
    std::cout<< std::endl;
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] MPA TriggerLatency = " << fReadoutChipInterface->ReadChipReg(firstMPA, "TriggerLatency") << std::endl;

}