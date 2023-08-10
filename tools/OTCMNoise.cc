#include "OTCMNoise.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

// PUBLIC METHODS
OTCMNoise::OTCMNoise() : Tool() {}

OTCMNoise::~OTCMNoise() {}

void OTCMNoise::Initialize()
{
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
                    LOG(INFO) << BOLDCYAN << "Not resetting threshold! Running with values in config files." << RESET;
                }

                for (auto cChip: *cHybrid){
                    LOG(INFO) << BOLDGREEN << "Disabling stub reconstruction"<< RESET;
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

    //Data is split between odd and even strips...
    DetectorDataContainer theHitContainerEven;
    DetectorDataContainer theHitContainerOdd;
    DetectorDataContainer theHitContainerSum;
    DetectorDataContainer theHitProfileContainer;

    DetectorDataContainer the2DHitContainer;

    // channel, chip, hybrid, optical group, board, detector
    // can have 0 or 255 hits, need NCHANNELS+1 (inclusive)
    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<(NCHANNELS + 1), uint32_t>,
                                           GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>,
                                           GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, theHitContainerEven);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<(NCHANNELS + 1), uint32_t>,
                                           GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>,
                                           GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, theHitContainerOdd);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<(NCHANNELS + 1), uint32_t>,
                                           GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>,
                                           GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, theHitContainerSum);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray<(NCHANNELS), uint32_t>,
                                           GenericDataArray<(HYBRID_CHANNELS_OT), uint32_t>,
                                           GenericDataArray<TOTAL_CHANNELS_OT, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, theHitProfileContainer);


    // 2D arrays for module-level and hybrid-level correlation
    if(f2DHistograms)
        ContainerFactory::copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>, EmptyContainer, EmptyContainer>(
            *fDetectorContainer, the2DHitContainer);

    //Creating the correlation plots... Maybe a lot of RAM being used?
    DetectorDataContainer     the2DSensorCorrelationContainer;
    DetectorDataContainer     the2DHybridCorrelationContainer;

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray_2D<NCHANNELS + 1,HYBRID_CHANNELS_OT + 1, uint32_t>,
                                           EmptyContainer,
                                           GenericDataArray_2D<HYBRID_CHANNELS_OT + 1,HYBRID_CHANNELS_OT + 1, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DHybridCorrelationContainer);

    ContainerFactory::copyAndInitStructure<EmptyContainer,
                                           GenericDataArray_2D<(NCHANNELS/2 + 1),(NCHANNELS/2 + 1), uint32_t>,
                                           GenericDataArray_2D<(HYBRID_CHANNELS_OT/2 + 1),(HYBRID_CHANNELS_OT/2 + 1), uint32_t>,
                                           GenericDataArray_2D<TOTAL_CHANNELS_OT/2 + 1,TOTAL_CHANNELS_OT/2 + 1, uint32_t>,
                                           EmptyContainer,
                                           EmptyContainer>(*fDetectorContainer, the2DSensorCorrelationContainer);



    for(auto cBoard: theHitContainerSum)
    {
        // BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->getObject(cBoard->getId()));


        fBeBoardInterface->Start(theBoard);
        uint32_t cN = fNevents;
        while (cN != 0){
            uint32_t cNEventToRead = cN;
            if (cNEventToRead > 1000)
                cNEventToRead = 1000;
            cN-= cNEventToRead;
            ReadNEvents(theBoard, cNEventToRead);
            const std::vector<Event*>& events = GetEvents();
            setNReadbackEvents(events.size());
            LOG(INFO)<<"Reading out "<<events.size()<<"events, "<<cN<<" events remaining.";



            for(auto cOpticalGroup: *cBoard)
            {
                for(auto& cEvent: events)
                {   
                    
                    if(cN > fNevents) continue;

                    uint32_t              cModuleHits = 0;
                    uint32_t              cModuleHitsEven = 0;
                    uint32_t              cModuleHitsOdd  = 0;

                    std::vector<uint32_t> hit_channels;
                    std::map < int ,std::map <int, int> > cChipCorrelationMap;
                    std::map < int ,int > cHybridCorrelationMap;
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        uint32_t cHybridHits     = 0;
                        uint32_t cHybridHitsEven = 0;
                        uint32_t cHybridHitsOdd  = 0;
                        for(auto cChip: *cHybrid)
                        {
                            uint32_t chipOffset_module = (cHybrid->getId() * HYBRID_CHANNELS_OT) + (cChip->getId() * NCHANNELS);
                            auto hit_vec = cEvent->GetHits(cHybrid->getId(),cChip->getId());
                            uint32_t cEventHitsEven = 0;
                            uint32_t cEventHitsOdd  = 0;
                            for (auto hit: hit_vec){                                
                                if (hit%2)
                                    cEventHitsEven++;
                                else
                                    cEventHitsOdd++;

                            }
                            uint32_t cEventHits = cEventHitsEven+cEventHitsOdd;
                            cChipCorrelationMap[cHybrid->getId()][cChip->getId()] = cEventHits;

                            theHitContainerEven.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getObject(cChip->getId())
                                    ->getSummary<GenericDataArray<(NCHANNELS + 1), uint32_t>>()[cEventHitsEven] += 1;

                            cChip->getSummary<GenericDataArray<(NCHANNELS + 1), uint32_t>>()[cEventHits] += 1;

                            theHitContainerOdd.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getObject(cChip->getId())
                                    ->getSummary<GenericDataArray<(NCHANNELS + 1), uint32_t>>()[cEventHitsOdd] += 1;

                            cHybridHits += cEventHits;
                            cHybridHitsEven += cEventHitsEven;
                            cHybridHitsOdd  += cEventHitsOdd;
                            cHybridCorrelationMap[cHybrid->getId()] = cHybridHits;

                            the2DSensorCorrelationContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getObject(cChip->getId())
                                    ->getSummary<GenericDataArray_2D<(NCHANNELS/2 + 1), (NCHANNELS/2 + 1), uint32_t>>()(cEventHitsEven,cEventHitsOdd) += 1;


                            // for 2d correlation, save channels with hits per chip
                            if(f2DHistograms)
                            {
                                for (auto hit: hit_vec){
                                    hit_channels.push_back(hit + chipOffset_module);
                                    
                                }
                            }

                        }

                        // save per hybrid
                        cModuleHits     += cHybridHits;
                        cModuleHitsEven += cHybridHitsEven;
                        cModuleHitsOdd  += cHybridHitsOdd;
                        
                        //Re-looping on chips...
                        for (auto cChip: *cHybrid){
                                the2DHybridCorrelationContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getObject(cChip->getId())
                                    ->getSummary<GenericDataArray_2D<NCHANNELS + 1,HYBRID_CHANNELS_OT + 1, uint32_t>>()(cChipCorrelationMap[cHybrid->getId()][cChip->getId()],cHybridHits) += 1;
                        }

                        the2DSensorCorrelationContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getSummary<GenericDataArray_2D<(HYBRID_CHANNELS_OT/2 + 1), (HYBRID_CHANNELS_OT/2 + 1), uint32_t>>()(cHybridHitsEven,cHybridHitsOdd) += 1;

                        cHybrid->getSummary<GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>>()[cHybridHits] += 1;
                        theHitContainerEven.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getSummary<GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>>()[cHybridHitsEven] += 1;

                        theHitContainerOdd.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getSummary<GenericDataArray<(HYBRID_CHANNELS_OT + 1), uint32_t>>()[cHybridHitsOdd] += 1;


                    }
                    // std::cout<<std::endl;
                    cOpticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>>()[cModuleHits] += 1;
                    the2DSensorCorrelationContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getSummary<GenericDataArray_2D<(TOTAL_CHANNELS_OT/2 + 1), (TOTAL_CHANNELS_OT/2 + 1), uint32_t>>()(cModuleHitsEven,cModuleHitsOdd) += 1;

                    theHitContainerOdd.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getSummary<GenericDataArray<(TOTAL_CHANNELS_OT + 1), uint32_t>>()[cModuleHitsOdd] += 1;
                    theHitContainerEven.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getSummary<GenericDataArray<(TOTAL_CHANNELS_OT + 1), uint32_t>>()[cModuleHitsEven] += 1;
                    the2DHybridCorrelationContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getSummary<GenericDataArray_2D<HYBRID_CHANNELS_OT + 1,HYBRID_CHANNELS_OT + 1, uint32_t>>()(cHybridCorrelationMap.begin()->second, cHybridCorrelationMap.rbegin()->second) += 1;

                    
                    if(f2DHistograms)
                    {
                        // per module correlation also tells us per hybrid correlation
                        for(size_t iCh1 = 0; iCh1 < hit_channels.size(); iCh1++)
                        {
                            for(size_t iCh2 = 0; iCh2 < hit_channels.size(); iCh2++)
                            {
                                the2DHitContainer.getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(hit_channels[iCh1], hit_channels[iCh2]) += 1;
                            }
                        }
                    }

                } // end events loop

            } // end module loop
        } //end acquisition loop
    }
#ifdef __USE_ROOT__
    fDQMHistogramOTCMNoise.fillHitPlots(theHitContainerSum,theHitContainerOdd,theHitContainerEven);
    fDQMHistogramOTCMNoise.fillCorrelationPlots(the2DHybridCorrelationContainer,the2DSensorCorrelationContainer);
    if(f2DHistograms) fDQMHistogramOTCMNoise.fill2DHitPlots(the2DHitContainer);
    
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theHitSerialization("OTCMNoiseHitStream");
        theHitSerialization.streamByOpticalGroupContainer(fDQMStreamer, theHitContainer);
        if(f2DHistograms)
        {
            ContainerSerialization the2DHitSerialization("OTCMNoise2DHitStream");
            the2DHitSerialization.streamByOpticalGroupContainer(fDQMStreamer, theHitContainer);
        }
    }
#endif
}

void OTCMNoise::parseSettings()
{
    // now read the settings from the map
    fNevents      = findValueInSettings<double>("Nevents", 100);
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
    LOG(INFO) << "Starting CM noise measurement";
    Initialize();
    SetThresholds();
    TakeData();
    LOG(INFO) << "Done with CM noise";
}

void OTCMNoise::Stop()
{
    LOG(INFO) << "Stopping CM noise measurement";
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "CM Noise measurement stopped.";
}

void OTCMNoise::Pause() {}

void OTCMNoise::Resume() {}
