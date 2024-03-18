#include "tools/OTinjectionDelayOptimization.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/BeBoard.h"
#include "Utils/CBCChannelGroupHandler.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTinjectionDelayOptimization::fCalibrationDescription = "Optimize delay for injecting calibration pulses";

OTinjectionDelayOptimization::OTinjectionDelayOptimization() : Tool() {}

OTinjectionDelayOptimization::~OTinjectionDelayOptimization() {}

void OTinjectionDelayOptimization::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fCbcTestPulseValue = findValueInSettings<double>("OTinjectionDelayOptimizationCbcTestPulseValue", 100);


#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTinjectionDelayOptimization.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTinjectionDelayOptimization::ConfigureCalibration()
{

}

void OTinjectionDelayOptimization::Running()
{
    LOG(INFO) << "Starting OTinjectionDelayOptimization measurement.";
    Initialise();
    optimizeInjectionDelay();
    LOG(INFO) << "Done with OTinjectionDelayOptimization.";
    Reset();
}

void OTinjectionDelayOptimization::Stop(void)
{
    LOG(INFO) << "Stopping OTinjectionDelayOptimization measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTinjectionDelayOptimization.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTinjectionDelayOptimization stopped.";
}

void OTinjectionDelayOptimization::Pause()
{

}


void OTinjectionDelayOptimization::Resume()
{

}

void OTinjectionDelayOptimization::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTinjectionDelayOptimization::optimizeInjectionDelay()
{
    this->enableTestPulse(true);
    bool is2Smodule = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S);
    if(is2Smodule) injectionDelayScan2S();
    else injectionDelayScanPS();
}

void OTinjectionDelayOptimization::injectionDelayScan2S()
{
    LOG(INFO) << BOLDBLUE << "OTinjectionDelayOptimization::injectionDelayScan2S - Scanning Delay for 2S module" << RESET;

    uint16_t initialLatency = 200;
    uint16_t totalDelay = 150;
    uint16_t delayStep = 1;
    uint16_t eventsPerPoint = 100;
    float expectedNoise = 6.5; // VCth
    float numberOfSigmas = 10.; // keeping far away from noise;
    uint16_t delayOffset = 10; // number of delays from pulse shape lower edge

    CBCChannelGroupHandler theChannelGroupHandler(std::bitset<NCHANNELS>(CBC_CHANNEL_GROUP_BITSET));
    theChannelGroupHandler.setChannelGroupParameters(16, 2);
    setChannelGroupHandler(theChannelGroupHandler);

    this->SetTestAllChannels(true);
    // Setting sparsification for simplicity
    for(auto theBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", 0);
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", initialLatency - 1);
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->SetSparsification(cCic, 0);
            }
        }
    }

    setSameDac("HitOr", 1); // using logical OR
    setSameDac("TestPulsePotNodeSel", fCbcTestPulseValue); // injected charge
    DetectorDataContainer* theOccupancyContainer = new DetectorDataContainer; //used to store occupancy while running bitWiseScan
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *theOccupancyContainer);
    fDetectorDataContainer = theOccupancyContainer;

    auto fromTotalDelayToDACs = [](uint16_t delay, uint16_t initialLatency)
    {
        uint8_t delayDAC   = 25 - (delay % 25);
        uint16_t latencyDAC = initialLatency - (delay / 25);
        if(delayDAC == 25)
        {
            delayDAC   = 0;
            latencyDAC = latencyDAC + 1;
        }
        return std::make_pair(latencyDAC, delayDAC);
    };

    std::map<uint16_t, DetectorDataContainer> theThresholdVsDelayMap;

    DetectorDataContainer thePedestalAverageContainer;
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, thePedestalAverageContainer);

    uint16_t defaultDelay = 0; // since 0 delay would not be measureable with this procedure (no pedestal) using 0 as not yet found value
    DetectorDataContainer theBestDelayContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theBestDelayContainer, defaultDelay);

    uint16_t maximumPedestalDelay = 25;
    uint16_t numberOfIterations = 0;
    bool isPedestalNormalized = false;

    for(uint16_t delay = 0; delay <= totalDelay; delay += delayStep)
    {
        float targetThreshold = 0.50;
        auto latencyAndDelay = fromTotalDelayToDACs(delay, initialLatency);
        LOG(INFO) << BOLDBLUE << "Finding threshold corresponging to " << targetThreshold << "% occupancy with latency " << +latencyAndDelay.first << " and injection delay " << +latencyAndDelay.second << RESET;
        setSameDac("TriggerLatency", latencyAndDelay.first);
        setSameDac("TestPulseDelay", latencyAndDelay.second);
        bitWiseScan("Threshold", eventsPerPoint, targetThreshold);
        DetectorDataContainer theThresholdContainer;
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theThresholdContainer);

        for(auto theBoard: *fDetectorContainer)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theChip: *theHybrid)
                    {
                        auto theThreshold = fReadoutChipInterface->ReadChipReg(theChip, "Threshold");
                        theThresholdContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint16_t>() = theThreshold;
                        if(delay < maximumPedestalDelay) // still in the plateau, add to the pedestal average
                        {
                            thePedestalAverageContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint32_t>() += theThreshold;
                        }
                        else
                        {
                            auto& theChipPedestal = thePedestalAverageContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint32_t>();
                            auto &theChipBestDelay    = theBestDelayContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint16_t>();
                            if(!isPedestalNormalized)
                            {
                                isPedestalNormalized = true;
                                theChipPedestal /= numberOfIterations;
                            }
                            if((theThreshold <= (theChipPedestal - (expectedNoise * numberOfSigmas))) && theChipBestDelay == 0)
                            {
                                theChipBestDelay = delay + delayOffset;
                                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] hybrid = " << +theHybrid->getId() << " chip = " << theChip->getId() << " best delay = " << theChipBestDelay << std::endl;
                            }
                        }
                    }
                }
            }
        }

#ifdef __USE_ROOT__
        fDQMHistogramOTinjectionDelayOptimization.fillThresholdVsDelayScan(delay, theThresholdContainer);
#else
        if(fDQMStreamerEnabled)
        {
                ContainerSerialization theContainerSerialization("OTinjectionDelayOptimizationDelayScan");
                theContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theThresholdContainer, delay);
        }
#endif

        theThresholdVsDelayMap.emplace(std::make_pair(delay, std::move(theThresholdContainer)));

        ++numberOfIterations;
    }

    delete fDetectorDataContainer;

}

void OTinjectionDelayOptimization::injectionDelayScanPS()
{

}

