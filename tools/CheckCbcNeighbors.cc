#include "../tools/CheckCbcNeighbors.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

CheckCbcNeighbors::CheckCbcNeighbors() : Tool() { fNEvents = this->findValueInSettings<double>("Nevents", 1000); }

CheckCbcNeighbors::~CheckCbcNeighbors() {}

void CheckCbcNeighbors::Initialise(void)  {}

bool CheckCbcNeighbors::TestCbcNeighbors()
{
    bool result;
    bool allPass = true;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // first disable everything on all chips
                for(auto cChip: *cHybrid)
                {
                    CbcInterface*              theCbcInterface = static_cast<CbcInterface*>(fReadoutChipInterface);
                    ChannelGroup<NCHANNELS, 1> cChannelMask;
                    cChannelMask.disableAllChannels();
                    theCbcInterface->maskChannelGroup(cChip, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask)));
                }

                // now begin test
                for(auto cChip1: *cHybrid)
                {
                    for(auto cChip2: *cHybrid)
                    {
                        ChannelGroup<NCHANNELS, 1> cChannelMask_1;
                        ChannelGroup<NCHANNELS, 1> cChannelMask_2;
                        CbcInterface*              theCbcInterface = static_cast<CbcInterface*>(fReadoutChipInterface);

                        if(cChip2->getId() != cChip1->getId() + 1) { continue; }
                        // if(! (cChip1->getId() == 0 && cChip2->getId() == 1)) continue;

                        LOG(DEBUG) << "CheckCBCNeighbors with Hybrid " << +cHybrid->getId() << " chip1: " << +cChip1->getId() << " chip2: " << +cChip2->getId();

                        //-------------------------------------
                        // check stubs from 1->2 -- all of these will be read out as being on chip1
                        //-------------------------------------
                        LOG(INFO) << BOLDCYAN << "Checking stubs from lower chip " << +cChip1->getId() << " to higher chip " << +cChip2->getId() << RESET;
                        cChannelMask_1.disableAllChannels();
                        cChannelMask_2.disableAllChannels();
                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));

                        this->UnmaskChannels(fSharedTopHigh, cChannelMask_1);
                        this->UnmaskChannels(fSharedBottomLow, cChannelMask_2);

                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));

                        LOG(DEBUG) << "Unmasked Chip1 " << +cChannelMask_1.getNumberOfEnabledChannels() << " Unmasked Chip2 " << +cChannelMask_2.getNumberOfEnabledChannels() << RESET;
                        result = CheckStubs(cHybrid->getId(), cChip1->getId());
                        LOG(DEBUG) << "result " << +result;
                        allPass = allPass && result;
                        if(!result)
                            LOG(INFO) << RED << "FAILED hybrid " << +cHybrid->getId() << " chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << std::endl << RESET;
                        else
                            LOG(INFO) << GREEN << "PASSED hybrid " << +cHybrid->getId() << " chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << std::endl << RESET;
                        // theStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip1->getIndex())->getSummary<bool>() = result;

                        //-------------------------------------
                        // check stubs from 2->1 -- all of these will be read out as if they are on chip2
                        //-------------------------------------
                        LOG(INFO) << BOLDMAGENTA << "Checking stubs from higher chip to lower chip" << RESET;
                        cChannelMask_1.disableAllChannels();
                        cChannelMask_2.disableAllChannels();
                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));

                        this->UnmaskChannels(fSharedBottomHigh, cChannelMask_1);
                        this->UnmaskChannels(fSharedTopLow, cChannelMask_2);

                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));

                        LOG(DEBUG) << "Unmasked Chip1 " << +cChannelMask_1.getNumberOfEnabledChannels() << " Unmasked Chip2 " << +cChannelMask_2.getNumberOfEnabledChannels() << RESET;
                        result = CheckStubs(cHybrid->getId(), cChip2->getId());
                        LOG(DEBUG) << "result " << +result;
                        allPass = allPass && result;
                        if(!result)
                            LOG(INFO) << RED << "FAILED hybrid " << +cHybrid->getId() << " chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << std::endl << RESET;
                        else
                            LOG(INFO) << GREEN << "PASSED hybrid " << +cHybrid->getId() << " chip1 " << +cChip1->getId() << " chip2 " << +cChip2->getId() << std::endl << RESET;

                        // remask channels for the next test
                        cChannelMask_2.disableAllChannels();
                        cChannelMask_1.disableAllChannels();
                        theCbcInterface->maskChannelGroup(cChip1, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_1)));
                        theCbcInterface->maskChannelGroup(cChip2, std::make_shared<ChannelGroup<NCHANNELS, 1>>(std::move(cChannelMask_2)));
                    }
                }
            }
        }
    }
    LOG(INFO) << BOLDGREEN << "CheckCbcNeighbors " << (allPass ? "PASSED " : "FAILED");
    return allPass;
}

<<<<<<< HEAD


=======
>>>>>>> ba5da0a8e95dd81b0045e658793696e02d0b0899
bool CheckCbcNeighbors::CheckStubs(uint8_t hybridId, uint8_t chipId)
{
    uint32_t eventsWStubs = 0;

    for(auto cBoard: *fDetectorContainer)
    {
        ReadNEvents(cBoard, fNEvents);
        const std::vector<Event*>& cEvents = this->GetEvents();
        LOG(DEBUG) << "Got " << cEvents.size() << " events.";
        for(auto& cEvent: cEvents)
        {
            auto cStubs = cEvent->StubVector(hybridId, chipId);
            if(cStubs.size() > 0) eventsWStubs++;

            for(auto cReadoutStub: cStubs)
            {
                // bad stub
                if((cReadoutStub.getPosition() >= 2 && cReadoutStub.getPosition() <= 6) || (cReadoutStub.getPosition() >= 252 && cReadoutStub.getPosition() <= 254))
                {
                    LOG(DEBUG) << "Stub position " << +cReadoutStub.getPosition() << " bend " << +cReadoutStub.getBend() << " row " << +cReadoutStub.getRow() << " center " << +cReadoutStub.getCenter()
                               << " on hybrid " << +hybridId << " and chip " << +chipId << RESET;
                    LOG(DEBUG) << CYAN << "\t Databit string " << cEvent->DataBitString(hybridId, chipId) << RESET;
                }
                // good stub
                else
                {
                    LOG(INFO) << RED << "WRONG STUB FOUND Stub position " << +cReadoutStub.getPosition() << " bend " << +cReadoutStub.getBend() << " row " << +cReadoutStub.getRow() << " center "
                              << +cReadoutStub.getCenter() << " on hybrid " << +hybridId << " and chip " << +chipId << RESET;
                }
            }
        }
    }
    LOG(DEBUG) << eventsWStubs << " events with stubs";
    return eventsWStubs > 0;
}

void CheckCbcNeighbors::UnmaskChannels(std::vector<uint8_t> pToUnmask, ChannelGroup<NCHANNELS, 1>& pChannelMask)
{
    for(size_t cChnl: pToUnmask)
    {
        LOG(DEBUG) << GREEN << "enabling " << +cChnl << RESET;
        pChannelMask.enableChannel(cChnl);
    }
}

void CheckCbcNeighbors::ConfigureCalibration() {}

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

void CheckCbcNeighbors::Pause() {}

void CheckCbcNeighbors::Resume() {}
