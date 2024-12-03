#include "OTCMNoise.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include <math.h>

std::string OTCMNoise::fCalibrationDescription = "Measure common noise in 2S modules";
// PUBLIC METHODS
OTCMNoise::OTCMNoise() : Tool() {}

OTCMNoise::~OTCMNoise() {}

void OTCMNoise::Initialize()
{
    fRegisterHelper->takeSnapshot();

    parseSettings();

#ifdef __USE_ROOT__
    fDQMHistogramOTCMNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    LOG(INFO) << "Histograms and Settings initialised.";
}

void OTCMNoise::SetThresholds()
{
    // Set Vcth to pedestal, or overload with manual setting
    ThresholdVisitor cVisitor(fReadoutChipInterface, 0);

    LOG(INFO) << "OT_MODULE_TEST:: Setting threshold on each chip" << RESET;
    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(INFO) << BOLDGREEN << "Setting Manual Vcth to " << fManualVcth << RESET;
                if(fManualVcth != 0)
                {
                    cVisitor.setThreshold(fManualVcth);
                    static_cast<OuterTrackerHybrid*>(cHybrid)->accept(cVisitor);
                }
                else
                {
                    LOG(INFO) << BOLDCYAN << "Not setting manual threshold! Running with threshold at the pedestal." << RESET;
                    for(auto theChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(theChip, "Threshold", round(theChip->getAveragePedestal())); }
                }

                for(auto cChip: *cHybrid)
                {
                    LOG(INFO) << BOLDGREEN << "Disabling stub reconstruction" << RESET;
                    static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(cChip, false, true, 0);

                    // if (cChip->getId()!=4){
                    //     LOG(INFO)<<BOLDRED<<"Setting threshold to 0 on CBC# "<<cChip->getId()<<RESET;
                    //     fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 0);
                    // }
                }
            }
        }
    }
}

void OTCMNoise::TakeData()
{
    ThresholdVisitor cVisitor(fReadoutChipInterface);
    this->accept(cVisitor);
    fVcth = cVisitor.getThreshold();
    LOG(INFO) << "Checking threshold on latest CBC that was touched...: " << fVcth;
    // Data is split between odd and even strips...
    DetectorDataContainer theChipHitContainer;
    DetectorDataContainer theHybridHitContainer;
    DetectorDataContainer theModuleHitContainer;

    DetectorDataContainer the2DHitContainer;

    // channel, chip, hybrid, optical group, board, detector
    // can have 0 or 255 hits, need NCHANNELS+1 (inclusive)
    ContainerFactory::copyAndInitStructure<EmptyContainer, GenericDataArray<uint32_t, 3 * (NCHANNELS + 1)>, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer>(*fDetectorContainer,
                                                                                                                                                                            theChipHitContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, 3 * (NCHANNELS * NCHIPS_OT + 1)>, EmptyContainer, EmptyContainer, EmptyContainer>(
        *fDetectorContainer, theHybridHitContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, 3 * (NCHANNELS * NCHIPS_OT * 2 + 1)>, EmptyContainer, EmptyContainer>(
        *fDetectorContainer, theModuleHitContainer);

    // 2D arrays for module-level and hybrid-level correlation
    if(f2DHistograms)
        ContainerFactory::
            copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray<uint32_t, NCHANNELS * NCHIPS_OT * 2, NCHANNELS * NCHIPS_OT * 2>, EmptyContainer, EmptyContainer>(
                *fDetectorContainer, the2DHitContainer);

    // Creating the correlation plots... Maybe a lot of RAM being used?
    DetectorDataContainer the2DSensorModuleCorrelationContainer;
    DetectorDataContainer the2DSensorHybridCorrelationContainer;
    DetectorDataContainer the2DSensorChipCorrelationContainer;
    DetectorDataContainer the2DHybridCorrelationContainer;

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<uint32_t, NCHANNELS + 1, NCHANNELS * NCHIPS_OT + 1>,
                                           EmptyContainer,
                                           GenericDataArray<uint32_t, NCHANNELS * NCHIPS_OT + 1, NCHANNELS * NCHIPS_OT + 1>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DHybridCorrelationContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           EmptyContainer,
                                           EmptyContainer,
                                           GenericDataArray<uint32_t, (NCHANNELS * NCHIPS_OT * 2) / 2 + 1, (NCHANNELS * NCHIPS_OT * 2) / 2 + 1>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DSensorModuleCorrelationContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           EmptyContainer,
                                           GenericDataArray<uint32_t, (NCHANNELS * NCHIPS_OT / 2 + 1), (NCHANNELS * NCHIPS_OT / 2 + 1)>,
                                           EmptyContainer,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DSensorHybridCorrelationContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer, GenericDataArray<uint32_t, (NCHANNELS / 2 + 1), (NCHANNELS / 2 + 1)>, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer>(
        *fDetectorContainer, the2DSensorChipCorrelationContainer);
    for(auto cBoard: theChipHitContainer)
    {
        // BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->getObject(cBoard->getId()));

        fBeBoardInterface->Start(theBoard);
        uint32_t cN = fNevents;
        while(cN != 0)
        {
            uint32_t cNEventToRead = cN;
            if(cNEventToRead > 1000) cNEventToRead = 1000;
            cN -= cNEventToRead;
            ReadNEvents(theBoard, cNEventToRead);
            const std::vector<Event*>& events = GetEvents();
            setNReadbackEvents(events.size());
            LOG(INFO) << "Reading out " << events.size() << "events, " << cN << " events remaining.";

            for(auto cOpticalGroup: *cBoard)
            {
                for(auto& cEvent: events)
                {
                    if(cN > fNevents) continue;

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
                            uint32_t chipOffset_module = (cHybrid->getId() * NCHANNELS * NCHIPS_OT) + (cChip->getId() * NCHANNELS);
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
                                ->getSummary<GenericDataArray<uint32_t, NCHANNELS + 1, NCHANNELS * NCHIPS_OT + 1>>()[cChipCorrelationMap[cHybrid->getId()][cChip->getId()]][cHybridHits] += 1;
                        }

                        the2DSensorHybridCorrelationContainer.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<GenericDataArray<uint32_t, (NCHANNELS * NCHIPS_OT / 2 + 1), (NCHANNELS * NCHIPS_OT / 2 + 1)>>()[cHybridHitsEven][cHybridHitsOdd] += 1;

                        auto theHybridContainerValues = &(theHybridHitContainer.getObject(cBoard->getId())
                                                              ->getObject(cOpticalGroup->getId())
                                                              ->getObject(cHybrid->getId())
                                                              ->getSummary<GenericDataArray<uint32_t, 3 * (NCHANNELS * NCHIPS_OT + 1)>>());
                        (*theHybridContainerValues)[cHybridHitsEven]++;
                        (*theHybridContainerValues)[(NCHANNELS * NCHIPS_OT + 1) + cHybridHitsOdd]++;
                        (*theHybridContainerValues)[2 * (NCHANNELS * NCHIPS_OT + 1) + cHybridHits]++;
                    }

                    auto theModuleContainerValues =
                        &(theModuleHitContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<GenericDataArray<uint32_t, 3 * (NCHANNELS * NCHIPS_OT * 2 + 1)>>());

                    (*theModuleContainerValues)[cModuleHitsEven]++;
                    (*theModuleContainerValues)[(NCHANNELS * NCHIPS_OT * 2 + 1) + cModuleHitsOdd]++;
                    (*theModuleContainerValues)[2 * (NCHANNELS * NCHIPS_OT * 2 + 1) + cModuleHits]++;

                    the2DSensorModuleCorrelationContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getSummary<GenericDataArray<uint32_t, ((NCHANNELS * NCHIPS_OT * 2) / 2 + 1), ((NCHANNELS * NCHIPS_OT * 2) / 2 + 1)>>()[cModuleHitsEven][cModuleHitsOdd] += 1;

                    the2DHybridCorrelationContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getSummary<GenericDataArray<uint32_t, NCHANNELS * NCHIPS_OT + 1, NCHANNELS * NCHIPS_OT + 1>>()[cHybridCorrelationMap.begin()->second]
                                                                                                                        [cHybridCorrelationMap.rbegin()->second] += 1;

                    if(f2DHistograms)
                    {
                        // per module correlation also tells us per hybrid correlation
                        for(size_t iCh1 = 0; iCh1 < hit_channels.size(); iCh1++)
                        {
                            for(size_t iCh2 = 0; iCh2 < hit_channels.size(); iCh2++)
                            {
                                the2DHitContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getSummary<GenericDataArray<uint32_t, NCHANNELS * NCHIPS_OT * 2, NCHANNELS * NCHIPS_OT * 2>>()[hit_channels[iCh1]][hit_channels[iCh2]] += 1;
                            }
                        }
                    }

                } // end events loop

            } // end module loop
        } // end acquisition loop
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
        cStreamableMap["OTCMNoise2DSensorHybridCorrelationStream"] = &the2DSensorHybridCorrelationContainer;
        cStreamableMap["OTCMNoise2DSensorChipCorrelationStream"]   = &the2DSensorChipCorrelationContainer;

        for(auto cStreamable: cStreamableMap)
        {
            LOG(INFO) << "Streaming " << cStreamable.first << RESET;
            ContainerSerialization theHitSerializationSum(cStreamable.first);
            theHitSerializationSum.streamByOpticalGroupContainer(fDQMStreamer, *(cStreamable.second));
        }

        if(f2DHistograms)
        {
            LOG(DEBUG) << "Streaming OTCMNoise2DHitStream" << RESET;
            ContainerSerialization the2DHitSerialization("OTCMNoise2DHitStream");
            the2DHitSerialization.streamByOpticalGroupContainer(fDQMStreamer, the2DHitContainer);
        }
    }
#endif
}

void OTCMNoise::parseSettings()
{
    // now read the settings from the map
    fNevents      = findValueInSettings<double>("CMNoise_Nevents", 100);
    f2DHistograms = findValueInSettings<double>("CMNoise_2DHistograms", 0);
    fManualVcth   = findValueInSettings<double>("CMNoise_manualVcth", 0);

    LOG(INFO) << "Parsed the following settings:";
    LOG(INFO) << "	Running " << fNevents;
    LOG(INFO) << "	2D Histograms? " << f2DHistograms;
    LOG(INFO) << "	Manual Vcth " << fManualVcth;
}

void OTCMNoise::writeObjects() {}

void OTCMNoise::ConfigureCalibration() {}

void OTCMNoise::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS)
    {
        LOG(ERROR) << ERROR_FORMAT << " Running a PS calibration on a PS module! " << RESET;
        return;
    }
    LOG(INFO) << "Starting CM noise measurement";
    Initialize();
    SetThresholds();
    TakeData();
    Reset();
    LOG(INFO) << "Done with CM noise";
}

void OTCMNoise::Stop()
{
    LOG(INFO) << "Stopping CM noise measurement";
    writeObjects();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "CM Noise measurement stopped.";
}

void OTCMNoise::Pause() {}

void OTCMNoise::Resume() {}

void OTCMNoise::Reset() { fRegisterHelper->restoreSnapshot(); }
