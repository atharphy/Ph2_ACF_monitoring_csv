#include "tools/OTinjectionDelayOptimization.h"
#include "HWDescription/BeBoard.h"
#include "System/RegisterHelper.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/SSAChannelGroupHandler.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTinjectionDelayOptimization::fCalibrationDescription = "Optimize delay for injecting calibration pulses";

OTinjectionDelayOptimization::OTinjectionDelayOptimization() : Tool() {}

OTinjectionDelayOptimization::~OTinjectionDelayOptimization() {}

void OTinjectionDelayOptimization::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CBC3, "TestPulseDel&ChanGroup");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CBC3, "TriggerLatency1");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CBC3, "FeCtrl&TrgLat2");

    fNumberOfEvents                        = findValueInSettings<double>("OTinjectionDelayOptimizationNumberOfEvents", 100);
    fCbcTestPulseValue                     = findValueInSettings<double>("OTinjectionDelayOptimizationCbcTestPulseValue", 150);
    fCbcNumberOfSigmaNoiseAwayFromPedestal = findValueInSettings<double>("OTinjectionDelayOptimizationCbcNumberOfSigmaNoiseAwayFromPedestal", 10.);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTinjectionDelayOptimization.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTinjectionDelayOptimization::ConfigureCalibration() {}

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

void OTinjectionDelayOptimization::Pause() {}

void OTinjectionDelayOptimization::Resume() {}

void OTinjectionDelayOptimization::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTinjectionDelayOptimization::optimizeInjectionDelay()
{
    this->enableTestPulse(true);
    bool is2Smodule = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S);
    if(is2Smodule)
        injectionDelayScan2S();
    else
        injectionDelayScanPS();
}

void OTinjectionDelayOptimization::injectionDelayScan2S()
{
    LOG(INFO) << BOLDBLUE << "OTinjectionDelayOptimization::injectionDelayScan2S - Scanning Delay for 2S module" << RESET;

    uint16_t initialLatency = 200;
    uint16_t totalDelay     = 150;
    uint16_t delayStep      = 1;
    float    expectedNoise  = 6.5; // VCth units
    uint16_t delayOffset    = 12;  // number of delays from pulse shape lower edge

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

    setSameDac("HitOr", 1);                                                   // using logical OR
    setSameDac("TestPulsePotNodeSel", fCbcTestPulseValue);                    // injected charge
    DetectorDataContainer* theOccupancyContainer = new DetectorDataContainer; // used to store occupancy while running bitWiseScan
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *theOccupancyContainer);
    fDetectorDataContainer = theOccupancyContainer;

    auto fromTotalDelayToDACs = [](uint16_t delay, uint16_t initialLatency)
    {
        uint8_t  delayDAC   = 25 - (delay % 25);
        uint16_t latencyDAC = initialLatency - (delay / 25);
        if(delayDAC == 25)
        {
            delayDAC   = 0;
            latencyDAC = latencyDAC + 1;
        }
        return std::make_pair(latencyDAC, delayDAC);
    };

    std::map<uint16_t, DetectorDataContainer> theThresholdVsDelayMap;

    std::pair<float, uint16_t> defaultThresholdAndDelay{0, 0}; // since 0 delay would not be measureable with this procedure (no pedestal) using 0 as not yet found value
    DetectorDataContainer      theBestThresholdAndDelayContainer;
    ContainerFactory::copyAndInitChip<std::pair<float, uint16_t>>(*fDetectorContainer, theBestThresholdAndDelayContainer, defaultThresholdAndDelay);

    uint16_t maximumPedestalDelay = 25;
    uint16_t numberOfIterations   = 0;
    bool     isPedestalAveraged   = false;

    for(uint16_t delay = 0; delay <= totalDelay; delay += delayStep)
    {
        float targetThreshold = 0.50;
        auto  latencyAndDelay = fromTotalDelayToDACs(delay, initialLatency);
        LOG(INFO) << BOLDBLUE << "Finding threshold corresponging to " << targetThreshold << "% occupancy with latency " << +latencyAndDelay.first << " and injection delay " << +latencyAndDelay.second
                  << RESET;
        setSameDac("TriggerLatency", latencyAndDelay.first);
        setSameDac("TestPulseDelay", latencyAndDelay.second);
        bitWiseScan("Threshold", fNumberOfEvents, targetThreshold);
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
                        auto  theThreshold = fReadoutChipInterface->ReadChipReg(theChip, "Threshold");
                        auto& theChipBestThresholdAndDelay =
                            theBestThresholdAndDelayContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::pair<float, uint16_t>>();
                        theThresholdContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint16_t>() = theThreshold;
                        if(delay < maximumPedestalDelay) // still in the plateau, add to the pedestal average
                        {
                            theChipBestThresholdAndDelay.first += theThreshold;
                        }
                        else
                        {
                            // auto &theChipBestDelay    = theBestDelayContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<uint16_t>();
                            if(!isPedestalAveraged)
                            {
                                theChipBestThresholdAndDelay.first /= numberOfIterations;                                       // average pedestal
                                theChipBestThresholdAndDelay.first -= (expectedNoise * fCbcNumberOfSigmaNoiseAwayFromPedestal); // move away from pedestal by n times the noise
                            }
                            if((theThreshold <= theChipBestThresholdAndDelay.first) && theChipBestThresholdAndDelay.second == 0) { theChipBestThresholdAndDelay.second = delay + delayOffset; }
                        }
                    }
                }
            }
        }

        if(delay >= maximumPedestalDelay && !isPedestalAveraged) // still in the plateau, add to the pedestal average
        {
            isPedestalAveraged = true;
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

    // set best delay and latency
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    auto theChipAveragePedestalAndBestDelay =
                        theBestThresholdAndDelayContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::pair<float, uint16_t>>();
                    auto latencyAndDelay = fromTotalDelayToDACs(theChipAveragePedestalAndBestDelay.second, initialLatency);
                    fReadoutChipInterface->WriteChipReg(theChip, "TriggerLatency", latencyAndDelay.first);
                    fReadoutChipInterface->WriteChipReg(theChip, "TestPulseDelay", latencyAndDelay.second);
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTinjectionDelayOptimization.fillBestThresholdAndDelay(theBestThresholdAndDelayContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theBestValuesSerialization("OTinjectionDelayOptimizationBestValues");
        theBestValuesSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestThresholdAndDelayContainer);
    }
#endif

    delete fDetectorDataContainer;
}

void OTinjectionDelayOptimization::injectionDelayScanPS()
{
    // TODO: working in progess by Fabio

    // LOG(INFO) << BOLDBLUE << "OTinjectionDelayOptimization::injectionDelayScanPS - Scanning Delay for PS module" << RESET;

    //     uint16_t initialLatency = 200;
    //     uint16_t totalDelay = 150;
    //     uint16_t delayStep = 1;
    //     float expectedNoise = 6.5; // VCth units
    //     uint16_t delayOffset = 12; // number of delays from pulse shape lower edge

    //     // Enabling 1 every N columns and corresponding rows in a diagonal pattern
    //     ChannelGroup<NMPAROWS, NSSACHANNELS> theMPAChannelGroup;
    //     theMPAChannelGroup.disableAllChannels();
    //     uint16_t initialCol = 2;
    //     uint16_t colsToSkip = 10;
    //     uint16_t currentRow = 1;
    //     uint16_t rowsToSkip = 1;
    //     for(uint16_t col = initialCol; col < NSSACHANNELS; col+=initialCol)
    //     {
    //         theMPAChannelGroup.enableChannel(currentRow % NMPAROWS, col);
    //         currentRow += rowsToSkip;
    //     }

    //     MPAChannelGroupHandler theChannelGroupHandlerMPA;
    //     theChannelGroupHandlerMPA.setChannelGroupParameters(NMPAROWS, NSSACHANNELS);
    //     setChannelGroupHandler(theChannelGroupHandlerMPA, FrontEndType::MPA2);

    //     // Enabling 1 every N columns
    //     ChannelGroup<1, NSSACHANNELS> theSSAChannelGroup;
    //     theSSAChannelGroup.disableAllChannels();
    //     uint16_t initialStrip = 3;
    //     uint16_t stripsToSkip = 10;
    //     for(uint16_t col = initialStrip; col < NSSACHANNELS; col+=stripsToSkip) theSSAChannelGroup.enableChannel(0, col);
    //     SSAChannelGroupHandler theChannelGroupHandlerSSA;
    //     theChannelGroupHandlerSSA.setCustomChannelGroup(theSSAChannelGroup);
    //     theChannelGroupHandlerSSA.setChannelGroupParameters(1, NSSACHANNELS);
    //     setChannelGroupHandler(theChannelGroupHandlerSSA, FrontEndType::SSA2);

    //     this->SetTestAllChannels(true);

    //     auto fromTotalDelayToDACs = [](uint16_t delay, uint16_t initialLatency)
    //     {
    //         uint8_t delayDAC   = 25 - (delay % 25);
    //         uint16_t latencyDAC = initialLatency - (delay / 25);
    //         if(delayDAC == 25)
    //         {
    //             delayDAC   = 0;
    //             latencyDAC = latencyDAC + 1;
    //         }
    //         delayDAC = delayDAC + (1 << 7); // in the SSA the MSB of the delay register need to be set to 1 to enable the delay
    //         return std::make_pair(latencyDAC, delayDAC);
    //     };

    //     // "Delay_line"
    //     // "DL_ctrl0" -> "DL_ctrl6"
}
