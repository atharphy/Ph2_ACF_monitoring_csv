#include "../tools/CheckCbcNeighbors.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

CheckCbcNeighbors::CheckCbcNeighbors() : Tool() {
    fNEvents    = this->findValueInSettings<double>("Nevents", 1000);
}

CheckCbcNeighbors::~CheckCbcNeighbors() {}

void CheckCbcNeighbors::Initialise(void)
{
   

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramCheckCbcNeighbors.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void CheckCbcNeighbors::ConfigureSharedChannels()
{
    ContainerFactory::copyAndInitChip<bool>(*fDetectorContainer, theStubContainer);
    bool result;
    bool allPass = true;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {

            for(auto cHybrid: *cOpticalGroup)
            {
                //turn off sparsification
               // auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
               // fCicInterface->SetSparsification(cCic, false);
                
                for(auto cChip: *cHybrid)
                {
                    CbcInterface* theCbcInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                    //first disable everything
                    ChannelGroup<NCHANNELS, 1> cChannelMask;
                    cChannelMask.disableAllChannels();
                    theCbcInterface->maskChannelGroup(cChip, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask)));
                }

                //now begin test 
                for(auto cChip1: *cHybrid)
                {
                    CbcInterface* theCbcInterface_1 = static_cast<CbcInterface*>(fReadoutChipInterface);
                    // //disable everything
                    // ChannelGroup<NCHANNELS, 1> cChannelMask_1;
                    // cChannelMask_1.disableAllChannels();
                    // theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                    
                    for(auto cChip2: *cHybrid)
                    {
                        ChannelGroup<NCHANNELS, 1> cChannelMask_1;
                        ChannelGroup<NCHANNELS, 1> cChannelMask_2;
                        CbcInterface* theCbcInterface_2 = static_cast<CbcInterface*>(fReadoutChipInterface);

                        if( cChip2->getId() !=  cChip1->getId()+1){
                            continue;
                        }
                        //if(! (cChip1->getId() == 0 && cChip2->getId() == 1)) continue;

                        LOG(INFO) << "CheckCBCNeighbors with Hybrid " << +cHybrid->getId() << " chip1: " << +cChip1->getId() << " chip2: " << +cChip2->getId();

                        //-------------------------------------
                        //check stubs from 1->2 -- all of these will be read out as being on chip1 
                        //-------------------------------------
                        LOG(INFO) << BOLDCYAN << "Checking stubs from lower chip to higher chip" << RESET;
                        cChannelMask_1.disableAllChannels();
                        cChannelMask_2.disableAllChannels();
                        theCbcInterface_1->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface_2->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));
                        
                        theCbcInterface_1->unmaskChannels({250, 252}, cChannelMask_1);
                        theCbcInterface_2->unmaskChannels({1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25}, cChannelMask_2);
                        

                        theCbcInterface_1->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface_2->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));

                        LOG(INFO) << "Unmasked Chip1 " << +cChannelMask_1.getNumberOfEnabledChannels() << " Unmasked Chip2 " << +cChannelMask_2.getNumberOfEnabledChannels() << RESET;
                        result = CheckStubs(cHybrid->getId(), cChip1->getId());
                        LOG(INFO) << "result " << +result;
                        allPass = allPass && result;
                        if(!result) LOG(INFO) << BOLDRED << "FAILED chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << RESET; 
                        else LOG(INFO) << BOLDGREEN << "PASSED chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << RESET; 
                        //theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip1->getIndex())->getSummary<bool>() = result;


                        //-------------------------------------
                        //check stubs from 2->1 -- all of these will be read out as if they are on chip2
                        //-------------------------------------
                        /* LOG(INFO) << BOLDMAGENTA << "Checking stubs from higher chip to lower chip" << RESET;
                        cChannelMask_1.disableAllChannels();
                        cChannelMask_2.disableAllChannels();

                        theCbcInterface->unmaskChannels({233, 235, 235, 237, 239, 241, 243, 245, 247, 249, 251, 253}, cChannelMask_1);
                        theCbcInterface->unmaskChannels({0, 2, 4}, cChannelMask_2);
                        
                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));
                        LOG(INFO) << "Unmasked Chip1 " << +cChannelMask_1.getNumberOfEnabledChannels() << " Unmasked Chip2 " << +cChannelMask_2.getNumberOfEnabledChannels() << RESET;
                        result = CheckStubs(cHybrid->getId(), cChip2->getId());
                        theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip2->getIndex())->getSummary<bool>() = result;
                        */

                        //remask channels for the next test
                        cChannelMask_2.disableAllChannels();
                        cChannelMask_1.disableAllChannels();
                        theCbcInterface_1->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface_2->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));
                    }
                }
            }
        }
    }
    LOG(INFO) << "CheckCbcNeighbors " << (allPass ? "PASSED " : "FAILED");
}

bool CheckCbcNeighbors::CheckStubs(uint8_t hybridId, uint8_t chipId)
{
    ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, theStubContainer);
    uint32_t eventsWStubs = 0;

    for(auto cBoard: *fDetectorContainer)
    {
        ReadNEvents(cBoard, fNEvents);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(INFO) << "Got " << cEvents.size() << " events.";
        for(auto& cEvent: cEvents)
        {
            auto cStubs = cEvent->StubVector(hybridId, chipId);
            if(cStubs.size() > 0) eventsWStubs ++;
            
            for(auto cReadoutStub: cStubs)
            {
                std::vector<uint8_t> channelList;
                LOG(DEBUG) << MAGENTA <<  "Stub position " << +cReadoutStub.getPosition() << " bend " << +cReadoutStub.getBend() << " row " << +cReadoutStub.getRow() << " center " << +cReadoutStub.getCenter() << " on hybrid " << +hybridId << " and chip " << +chipId << RESET;
                LOG(DEBUG) << CYAN << "\t Databit string " << cEvent->DataBitString(hybridId, chipId) << RESET;
            }
        }
    }
    LOG(INFO) << eventsWStubs << " events with stubs";
    return eventsWStubs > 0;
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
