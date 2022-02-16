#include "CMTester.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"

// PUBLIC METHODS
CMTester::CMTester() : Tool() {}

CMTester::~CMTester() {}

void CMTester::Initialize()
{    
    parseSettings();
    
#ifdef __USE_ROOT__
    fDQMHistogramOTCommonNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    LOG(INFO) << "Histograms and Settings initialised.";
}

void CMTester::SetThresholds(uint32_t pManualVcth)
{
    // Set Vcth to pedestal, or overload with manual setting
    ThresholdVisitor cVisitor(fReadoutChipInterface, 0);
  
    LOG(INFO) << "OT_MODULE_TEST:: Setting threshold on each chip" << RESET;
    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                LOG(INFO) << BOLDGREEN << "Setting Manual Vcth to " << pManualVcth << RESET;
                if(pManualVcth != 0){
                    cVisitor.setThreshold(pManualVcth);
                    static_cast<OuterTrackerHybrid*>(cFe)->accept(cVisitor);
                }
                else{
                    LOG(INFO) << BOLDCYAN << "Not resetting threshold! Running with values in config files." << RESET;
                }
            }
        }
    }
}


void CMTester::TakeData()
{
    ThresholdVisitor cVisitor(fReadoutChipInterface);
    this->accept(cVisitor);
    fVcth = cVisitor.getThreshold();
    LOG(INFO) << "Checking threshold on latest CBC that was touched...: " << fVcth;

    DetectorDataContainer theHitContainer;
    DetectorDataContainer the2DHitContainer;
    //channel, chip, hybrid, optical group, board, detector
    //can have 0 or 255 hits, need NCHANNELS+1 (inclusive)
    ContainerFactory::copyAndInitStructure<EmptyContainer, GenericDataArray<(NCHANNELS+1), uint32_t>, GenericDataArray<(HYBRID_CHANNELS_OT+1), uint32_t>, GenericDataArray<TOTAL_CHANNELS_OT+1, uint32_t>, EmptyContainer, EmptyContainer>(*fDetectorContainer, theHitContainer);
    //3D arrays for module-level and hybrid-level correlation
    ContainerFactory::copyAndInitStructure<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>, EmptyContainer, EmptyContainer>(*fDetectorContainer, the2DHitContainer);


    //TODO LESYA -- currently missing the channel by channel data per event, need to think about how to implement this to make the 2D plot.

    for(auto cBoard: theHitContainer)
    {
        //BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        BeBoard* theBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));

        uint32_t cN      = 0;

        fBeBoardInterface->Start(theBoard);
        ReadNEvents(theBoard, fNevents);
        const std::vector<Event*>& events = GetEvents();
        

        for(auto cOpticalGroup: *cBoard)
        {

            for(auto& cEvent: events)
            {

                if(cN > fNevents) continue;
                
                uint32_t cModuleHits = 0;
                std::vector<uint32_t> hit_channels;
                    
                for(auto cHybrid: *cOpticalGroup)
                {
                    uint32_t cHybridHits = 0;


                    for(auto cChip: *cHybrid)
                    {
                        uint32_t cEventHits = cEvent->GetNHits(cHybrid->getId(), cChip->getId());
                        //basically filling the histogram, then we will set bin content later
                        cChip->getSummary<GenericDataArray<(NCHANNELS+1), uint32_t>>()[cEventHits] += 1;
                        cHybridHits += cEventHits;
 

                        //for 2d correlation, save channels with hits per chip 
                        //TODO add checking for masked channels
                        uint32_t chipOffset_module = (cHybrid->getIndex() * HYBRID_CHANNELS_OT) + (cChip->getIndex() * NCHANNELS );
                        for(uint32_t iCh = 0; iCh < NCHANNELS+1; iCh++){
                            if(cEvent->DataBit(cHybrid->getId(), cChip->getId(), iCh)){
                                hit_channels.push_back(iCh + chipOffset_module);
                            }
                        }
                    }

                    //save per hybrid
                    cModuleHits += cHybridHits;
                    cHybrid->getSummary<GenericDataArray<(HYBRID_CHANNELS_OT+1), uint32_t>>()[cHybridHits] += 1; 

                }
                cOpticalGroup->getSummary<GenericDataArray<TOTAL_CHANNELS_OT+1, uint32_t>>()[cModuleHits] += 1;

                //per module correlation also tells us per hybrid correlation
                for(size_t iCh1 = 0; iCh1 < hit_channels.size(); iCh1 ++ ){
                    for(size_t iCh2 = 0; iCh2 < hit_channels.size(); iCh2 ++){
                        the2DHitContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->getSummary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>()(hit_channels[iCh1], hit_channels[iCh2]) += 1;
                    }
                }

                //print out event counter
                if(cN % 100 == 0)
                {
                    LOG(INFO) << cN << " Events recorded!";
                }
                cN++;
            } //end events loop

        } //end module loop
    }
#ifdef __USE_ROOT__
    fDQMHistogramOTCommonNoise.fillHitPlots(theHitContainer);
    fDQMHistogramOTCommonNoise.fill2DHitPlots(the2DHitContainer);
#else
    //TODO optical group contianer streamer doesnt exist
    /* 
    auto theHitStream = prepareOpticalGroupContainerStream<EmptyContainer, GenericDataArray<NCHANNELS+1, uint32_t>, GenericDataArray<HYBRID_CHANNELS_OT+1, uint32_t>, GenericDataArray<TOTAL_CHANNELS_OT+1, uint32_t>>("CMNoise_HitStream");
    for(auto board: theHitContainer)
    {
        if(fDQMStreamerEnabled) theHitStream.streamAndSendBoard(board, fDQMStreamer);
    }
    auto the2DHitStream = prepareOpticalGroupContainerStream<EmptyContainer, EmptyContainer, EmptyContainer, GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>>("CMNoise_2DHitStream");
    for(auto board: the2DHitContainer)
    {
        if(fDQMStreamerEnabled) the2DHitStream.streamAndSendBoard(board, fDQMStreamer);
    }
    */
#endif

}


void CMTester::parseSettings()
{
    // now read the settings from the map
    auto cSetting = fSettingsMap.find("Nevents");

    if(cSetting != std::end(fSettingsMap))
        fNevents = boost::any_cast<double>(cSetting->second);
    else
        fNevents = 2000;

    LOG(INFO) << "Parsed the following settings:";
    LOG(INFO) << "	Running " << fNevents;
}

