#include <boost/math/distributions/normal.hpp>
#include "tools/OTPScommonNoise.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPScommonNoise::fCalibrationDescription = "Measure common noise in PS modules";

OTPScommonNoise::OTPScommonNoise() : Tool() {}

OTPScommonNoise::~OTPScommonNoise() {}

void OTPScommonNoise::Initialise(void)
{
    fNumberOfEvents = findValueInSettings<double>("OTPScommonNoise_NumberOfEvents", 100);
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTPScommonNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTPScommonNoise::ConfigureCalibration()
{

}

void OTPScommonNoise::SetThresholds()
{
    // For PS modules the CIC is in sparsified mode. Therefore we cannot have more 128 channel per hybrid on the strips and pixels sensor.
    // Therefore we must set a threshold that allows around 64 channels per pixels and strips
    uint8_t theMaximumChannelNumber = 64;
    float theStripAllowedOccupancy  = float(theMaximumChannelNumber)/(NSSACHANNELS*NCHIPS_OT);
    float thePixelAllowedOccupancy  = float(theMaximumChannelNumber)/(NSSACHANNELS*NMPAROWS*NCHIPS_OT);

    LOG(INFO) << BOLDYELLOW << "theStripAllowedOccupancy: " << theStripAllowedOccupancy << " thePixelAllowedOccupancy: "<< thePixelAllowedOccupancy << RESET;

    // Now we calculate how many sigmas away from the pedestal we should be to have that occupancy
    boost::math::normal gaus(0, 1); // we consider a standard gaussian
    float theStripSigma = quantile(complement(gaus, theStripAllowedOccupancy));
    float thePixelSigma = quantile(complement(gaus, thePixelAllowedOccupancy));

    LOG(INFO) << BOLDYELLOW << "theStripSigma: " << theStripSigma << " thePixelSigma: "<< thePixelSigma << RESET;

    // now we set the thresold  to pedestal + noise*sigma
    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    float theSigma = (cChip->getFrontEndType() == FrontEndType::SSA2) ? theStripSigma : thePixelSigma;
                    LOG(INFO) << BOLDYELLOW << " chip " << +cChip->getId() << " cChip->getAveragePedestal(): " << cChip->getAveragePedestal() << " cChip->getAverageNoise: " << cChip->getAverageNoise() << RESET;
                    float theThreshold = cChip->getAveragePedestal() + cChip->getAverageNoise() + theSigma;
                    LOG(INFO) << BOLDYELLOW << " chip " << +cChip->getId() << " theThreshold: " << theThreshold << " rounded " << std::round(theThreshold) << RESET;
                    fReadoutChipInterface->WriteChipReg(cChip,"Threshold",std::round(theThreshold));
                }
            }
        }
    }
}
void OTPScommonNoise::TakeData()
{

    DetectorDataContainer theStripHitContainer;
    DetectorDataContainer thePixelHitContainer;
    DetectorDataContainer theStripHybridHitContainer;
    DetectorDataContainer thePixelHybridHitContainer;
    DetectorDataContainer theStripModuleHitContainer;
    DetectorDataContainer thePixelModuleHitContainer;

    // DetectorDataContainer the2DHitContainer;

    ContainerFactory::copyAndInitChip<GenericDataArray<uint32_t, (NSSACHANNELS + 1)>>(*fDetectorContainer, theStripHitContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<uint32_t, (NSSACHANNELS * NMPAROWS + 1)>>(*fDetectorContainer, thePixelHitContainer);

    ContainerFactory::copyAndInitHybrid<GenericDataArray<uint32_t, (NSSACHANNELS * NCHIPS_OT + 1)>>( *fDetectorContainer, theStripHybridHitContainer);
    ContainerFactory::copyAndInitHybrid<GenericDataArray<uint32_t, (NSSACHANNELS * NMPAROWS * NCHIPS_OT + 1)>>( *fDetectorContainer, thePixelHybridHitContainer);

    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint32_t, (NSSACHANNELS * NCHIPS_OT * 2 + 1)>>( *fDetectorContainer, theStripModuleHitContainer);
    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint32_t, (NSSACHANNELS * NMPAROWS * NCHIPS_OT * 2 + 1)>>( *fDetectorContainer, thePixelModuleHitContainer);

    /*
    // 2D arrays for module-level and hybrid-level correlation
    if(f2DHistograms)
        ContainerFactory::copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>, EmptyContainer, EmptyContainer>(
            *fDetectorContainer, the2DHitContainer);

    // Creating the correlation plots... Maybe a lot of RAM being used?
    DetectorDataContainer the2DSensorModuleCorrelationContainer;
    DetectorDataContainer the2DSensorHybridCorrelationContainer;
    DetectorDataContainer the2DSensorChipCorrelationContainer;
    DetectorDataContainer the2DHybridCorrelationContainer;

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<uint32_t, NCHANNELS + 1, HYBRID_CHANNELS_OT + 1>,
                                           EmptyContainer,
                                           GenericDataArray<uint32_t, HYBRID_CHANNELS_OT + 1, HYBRID_CHANNELS_OT + 1>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DHybridCorrelationContainer);

    ContainerFactory::
        copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, TOTAL_CHANNELS_OT / 2 + 1, TOTAL_CHANNELS_OT / 2 + 1>, EmptyContainer, EmptyContainer>(
            *fDetectorContainer, the2DSensorModuleCorrelationContainer);

    ContainerFactory::
        copyAndInitStructure<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, (HYBRID_CHANNELS_OT / 2 + 1), (HYBRID_CHANNELS_OT / 2 + 1)>, EmptyContainer, EmptyContainer, EmptyContainer>(
            *fDetectorContainer, the2DSensorHybridCorrelationContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer, GenericDataArray<uint32_t, (NCHANNELS / 2 + 1), (NCHANNELS / 2 + 1)>, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer>(
        *fDetectorContainer, the2DSensorChipCorrelationContainer);
    */
    
    /*
    for(auto theBoard: *fDetectorContainer)
    {
        // FIXME ! This part needs to be checked and fixed for PS modules
        // See what is inside the Start function etc. and how Fabio prepares the chips in the occupancy measurement without injection
        // because now I get: D19cL1ReadoutInterface::WaitForReadout no words in the readout ..[ReadoutAttempt#0]
        fBeBoardInterface->Start(theBoard);
        uint32_t theEventCounter = fNumberOfEvents;
        while(theEventCounter != 0)
        {
            uint32_t cNEventToRead = theEventCounter;
            theEventCounter -= cNEventToRead;
            ReadNEvents(theBoard, cNEventToRead);
            const std::vector<Event*>& events = GetEvents();
            setNReadbackEvents(events.size());
            LOG(INFO) << "Reading out " << events.size() << "events, " << theEventCounter << " events remaining.";
 

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto& cEvent: events)
                {
                    if(theEventCounter > fNumberOfEvents) continue;

                    uint32_t cModuleHits     = 0;
                    uint32_t cModuleHitsEven = 0;
                    uint32_t cModuleHitsOdd  = 0;

                    std::vector<uint32_t>             hit_channels;
                    std::map<int, std::map<int, int>> cChipCorrelationMap;
                    std::map<int, int>                cHybridCorrelationMap;
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        uint32_t cHybridHits     = 0;
                        uint32_t cHybridHitsEven = 0;
                        uint32_t cHybridHitsOdd  = 0;
                        for(auto cChip: *cHybrid)
                        {
                            uint32_t chipOffset_module = (cHybrid->getId() * HYBRID_CHANNELS_OT) + (cChip->getId() * NCHANNELS);
                            auto     hit_vec           = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                            uint32_t cEventHitsEven    = 0;
                            uint32_t cEventHitsOdd     = 0;
                            for(auto hit: hit_vec)
                            {
                                if(hit.second % 2)
                                    cEventHitsEven++;
                                else
                                    cEventHitsOdd++;
                            }
                            uint32_t cEventHits                                   = cEventHitsEven + cEventHitsOdd;
                            cChipCorrelationMap[cHybrid->getId()][cChip->getId()] = cEventHits;

                            auto theChipHitContainerValues = &(theChipHitContainer.getObject(cBoard->getId())
                                                                   ->getObject(cOpticalGroup->getId())
                                                                   ->getObject(cHybrid->getId())
                                                                   ->getObject(cChip->getId())
                                                                   ->getSummary<GenericDataArray<uint32_t, 3 * (NCHANNELS + 1)>>());

                            (*theChipHitContainerValues)[cEventHitsEven]++;
                            (*theChipHitContainerValues)[(NCHANNELS + 1) + cEventHitsOdd]++;
                            (*theChipHitContainerValues)[2 * (NCHANNELS + 1) + cEventHits]++;

                            cHybridHits += cEventHits;
                            cHybridHitsEven += cEventHitsEven;
                            cHybridHitsOdd += cEventHitsOdd;
                            cHybridCorrelationMap[cHybrid->getId()] = cHybridHits;

                            the2DSensorChipCorrelationContainer.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<GenericDataArray<uint32_t, (NCHANNELS / 2 + 1), (NCHANNELS / 2 + 1)>>()[cEventHitsEven][cEventHitsOdd] += 1;

                            // for 2d correlation, save channels with hits per chip
                            if(f2DHistograms)
                            {
                                for(auto hit: hit_vec) { hit_channels.push_back(hit.second + chipOffset_module); }
                            }
                        }

                        // save per hybrid
                        cModuleHits += cHybridHits;
                        cModuleHitsEven += cHybridHitsEven;
                        cModuleHitsOdd += cHybridHitsOdd;

                        // Re-looping on chips...
                        for(auto cChip: *cHybrid)
                        {
                            the2DHybridCorrelationContainer.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<GenericDataArray<uint32_t, NCHANNELS + 1, HYBRID_CHANNELS_OT + 1>>()[cChipCorrelationMap[cHybrid->getId()][cChip->getId()]][cHybridHits] += 1;
                        }

                        the2DSensorHybridCorrelationContainer.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<GenericDataArray<uint32_t, (HYBRID_CHANNELS_OT / 2 + 1), (HYBRID_CHANNELS_OT / 2 + 1)>>()[cHybridHitsEven][cHybridHitsOdd] += 1;

                        auto theHybridContainerValues = &(theHybridHitContainer.getObject(cBoard->getId())
                                                              ->getObject(cOpticalGroup->getId())
                                                              ->getObject(cHybrid->getId())
                                                              ->getSummary<GenericDataArray<uint32_t, 3 * (HYBRID_CHANNELS_OT + 1)>>());
                        (*theHybridContainerValues)[cHybridHitsEven]++;
                        (*theHybridContainerValues)[(HYBRID_CHANNELS_OT + 1) + cHybridHitsOdd]++;
                        (*theHybridContainerValues)[2 * (HYBRID_CHANNELS_OT + 1) + cHybridHits]++;
                    }

                    auto theModuleContainerValues =
                        &(theModuleHitContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<GenericDataArray<uint32_t, 3 * (TOTAL_CHANNELS_OT + 1)>>());

                    (*theModuleContainerValues)[cModuleHitsEven]++;
                    (*theModuleContainerValues)[(TOTAL_CHANNELS_OT + 1) + cModuleHitsOdd]++;
                    (*theModuleContainerValues)[2 * (TOTAL_CHANNELS_OT + 1) + cModuleHits]++;

                    the2DSensorModuleCorrelationContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getSummary<GenericDataArray<uint32_t, (TOTAL_CHANNELS_OT / 2 + 1), (TOTAL_CHANNELS_OT / 2 + 1)>>()[cModuleHitsEven][cModuleHitsOdd] += 1;

                    the2DHybridCorrelationContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getSummary<GenericDataArray<uint32_t, HYBRID_CHANNELS_OT + 1, HYBRID_CHANNELS_OT + 1>>()[cHybridCorrelationMap.begin()->second][cHybridCorrelationMap.rbegin()->second] += 1;

                    if(f2DHistograms)
                    {
                        // per module correlation also tells us per hybrid correlation
                        for(size_t iCh1 = 0; iCh1 < hit_channels.size(); iCh1++)
                        {
                            for(size_t iCh2 = 0; iCh2 < hit_channels.size(); iCh2++)
                            {
                                the2DHitContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getSummary<GenericDataArray<uint32_t, TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT>>()[hit_channels[iCh1]][hit_channels[iCh2]] += 1;
                            }
                        }
                    }

                } // end events loop

            } // end module loop
        }     // end acquisition loop
    }
#ifdef __USE_ROOT__
    fDQMHistogramOTCMNoise.fillChipHitPlots(theChipHitContainer, true);
    fDQMHistogramOTCMNoise.fillHybridHitPlots(theHybridHitContainer);
    fDQMHistogramOTCMNoise.fillModuleHitPlots(theModuleHitContainer);

    fDQMHistogramOTCMNoise.fillHybridCorrelationPlots(the2DHybridCorrelationContainer);
    fDQMHistogramOTCMNoise.fillSensorChipCorrelationPlots(the2DSensorChipCorrelationContainer);
    fDQMHistogramOTCMNoise.fillSensorHybridCorrelationPlots(the2DSensorHybridCorrelationContainer);
    fDQMHistogramOTCMNoise.fillSensorModuleCorrelationPlots(the2DSensorModuleCorrelationContainer);
    if(f2DHistograms) fDQMHistogramOTCMNoise.fill2DHitPlots(the2DHitContainer);

#else
    if(fDQMStreamerEnabled)
    {
        std::map<std::string, DetectorDataContainer*> cStreamableMap;
        cStreamableMap["OTCMNoiseChipHitStream"]                   = &theChipHitContainer;
        cStreamableMap["OTCMNoiseHybridHitStream"]                 = &theHybridHitContainer;
        cStreamableMap["OTCMNoiseModuleHitStream"]                 = &theModuleHitContainer;
        cStreamableMap["OTCMNoise2DHybridCorrelationStream"]       = &the2DHybridCorrelationContainer;
        cStreamableMap["OTCMNoise2DSensorModuleCorrelationStream"] = &the2DSensorModuleCorrelationContainer;
        // cStreamableMap["OTCMNoise2DSensorHybridCorrelationStream"]     = &the2DSensorHybridCorrelationContainer; //Ignoring, causes a crash
        cStreamableMap["OTCMNoise2DSensorChipCorrelationStream"] = &the2DSensorChipCorrelationContainer;

        for(auto cStreamable: cStreamableMap)
        {
            try
            {
                LOG(DEBUG) << "Streaming " << cStreamable.first << RESET;
                ContainerSerialization theHitSerializationSum(cStreamable.first);
                theHitSerializationSum.streamByOpticalGroupContainer(fDQMStreamer, *(cStreamable.second));
            }
            catch(const std::exception& e) // reference to the base of a polymorphic object
            {
                LOG(INFO) << BOLDRED << " Unable to serialize " << cStreamable.first << RESET;
                LOG(INFO) << e.what() << RESET;
            }
        }

        if(f2DHistograms)
        {
            LOG(DEBUG) << "Streaming OTCMNoise2DHitStream" << RESET;
            ContainerSerialization the2DHitSerialization("OTCMNoise2DHitStream");
            the2DHitSerialization.streamByOpticalGroupContainer(fDQMStreamer, the2DHitContainer);
        }
    }
#endif
*/
}

void OTPScommonNoise::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S)
    { 
        LOG(ERROR) << ERROR_FORMAT << " Running a PS calibration on a 2S module! " << RESET;
        return;
    }
    LOG(INFO) << BOLDMAGENTA << "Starting OTPScommonNoise measurement." << RESET;
    Initialise();
    SetThresholds();
    TakeData();
    LOG(INFO) << BOLDMAGENTA << "Done with OTPScommonNoise." << RESET;
    Reset();
}

void OTPScommonNoise::Stop(void)
{
    LOG(INFO) << "Stopping OTPScommonNoise measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTPScommonNoise.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTPScommonNoise stopped.";
}

void OTPScommonNoise::Pause()
{

}


void OTPScommonNoise::Resume()
{

}


void OTPScommonNoise::Reset()
{
    fRegisterHelper->restoreSnapshot();
}
