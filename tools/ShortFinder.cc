#include "ShortFinder.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/CommonVisitors.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"
#include "../Utils/Visitor.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

// initialize the static member

ShortFinder::ShortFinder() : Tool() {}

ShortFinder::~ShortFinder() {}
void ShortFinder::Reset()
{
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                LOG(INFO) << BOLDBLUE << "Resetting all registers on readout chips connected to FEhybrid#" << (cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip) cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
    resetPointers();
}
void ShortFinder::Print()
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cShorts = fShorts.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cShortsThisOpticalGroup = cShorts->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cShortsHybrid = cShortsThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cShortsReadoutChip = cShortsHybrid->at(cChip->getIndex())->getSummary<ChannelList>();
                    if(cShortsReadoutChip.size() == 0)
                        LOG(INFO) << BOLDGREEN << "No shorts found in readout chip" << +cChip->getId() << " on FE hybrid " << +cHybrid->getId() << RESET;
                    else
                        LOG(INFO) << BOLDRED << "Found " << +cShortsReadoutChip.size() << " shorts in readout chip" << +cChip->getId() << " on FE hybrid " << +cHybrid->getId() << RESET;

                    for(auto cShort: cShortsReadoutChip)
                        LOG(DEBUG) << BOLDRED << "Possible short in channel " << +cShort << " in readout chip" << +cChip->getId() << " on FE hybrid " << +cHybrid->getId() << RESET;
                }
            }
        }
    }
}
void ShortFinder::Initialise()
{
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    fWithCBC                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    fWithSSA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA || cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA2);
    LOG(INFO) << "With SSA set to " << ((fWithSSA) ? 1 : 0) << RESET;

    if(fWithCBC)
    {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
    }
    if(fWithSSA)
    {
        SSAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA);
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA2);
    }
    // if(cWithMPA)
    // {
    //     MPAChannelGroupHandler theChannelGroupHandler;
    //     theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
    //     setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA);
    //     setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA2);
    // }

    // now read the settings from the map
    auto cSetting       = fSettingsMap.find("Nevents");
    fEventsPerPoint     = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 10;
    cSetting            = fSettingsMap.find("ShortsPulseAmplitude");
    fTestPulseAmplitude = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;

    // prepare container
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, fShortsContainer);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, fHitsContainer);
    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fShorts);
    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fInjections);

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitStructure<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitStructure<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>() = static_cast<BeBoard*>(cBoard)->getBeBoardRegMap();
        auto& cRegMapThisBoard                                                       = fRegMapContainer.at(cBoard->getIndex());
        auto& cShorts                                                                = fShorts.at(cBoard->getIndex());
        auto& cInjections                                                            = fInjections.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cShortsOpticalGroup     = cShorts->at(cOpticalGroup->getIndex());
            auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());

            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cShortsHybrid     = cShortsOpticalGroup->at(cHybrid->getIndex());
                auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    cInjectionsHybrid->at(cChip->getIndex())->getSummary<ChannelList>().clear();
                    cShortsHybrid->at(cChip->getIndex())->getSummary<ChannelList>().clear();
                    cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>() = static_cast<ReadoutChip*>(cChip)->getRegMap();
                }
            }
        }
    }
}
void ShortFinder::Stop() { this->Reset(); }
void ShortFinder::Count(BeBoard* pBoard, const std::shared_ptr<ChannelGroupBase> pGroup)
{
    auto  cBitset              = std::bitset<NCHANNELS>(std::static_pointer_cast<const ChannelGroup<NCHANNELS>>(pGroup)->getBitset());
    auto& cThisShortsContainer = fShortsContainer.at(pBoard->getIndex());
    auto& cThisHitsContainer   = fHitsContainer.at(pBoard->getIndex());
    auto& cShorts              = fShorts.at(pBoard->getIndex());
    auto& cInjections          = fInjections.at(pBoard->getIndex());

    for(auto cOpticalGroup: *pBoard)
    {
        auto& cOpticalGroupShorts     = cThisShortsContainer->at(cOpticalGroup->getIndex());
        auto& cOpticalGroupHits       = cThisHitsContainer->at(cOpticalGroup->getIndex());
        auto& cShortsOpticalGroup     = cShorts->at(cOpticalGroup->getIndex());
        auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());

        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cHybridShorts     = cOpticalGroupShorts->at(cHybrid->getIndex());
            auto& cHybridHits       = cOpticalGroupHits->at(cHybrid->getIndex());
            auto& cShortsHybrid     = cShortsOpticalGroup->at(cHybrid->getIndex());
            auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                auto& cReadoutChipShorts     = cHybridShorts->at(cChip->getIndex());
                auto& cReadoutChipHits       = cHybridHits->at(cChip->getIndex());
                auto& cShortsReadoutChip     = cShortsHybrid->at(cChip->getIndex())->getSummary<ChannelList>();
                auto& cInjectionsReadoutChip = cInjectionsHybrid->at(cChip->getIndex())->getSummary<ChannelList>();
                for(size_t cIndex = 0; cIndex < cBitset.size(); cIndex++)
                {
                    if(cBitset[cIndex] == 0 && cReadoutChipShorts->getChannelContainer<uint16_t>()->at(cIndex) > THRESHOLD_SHORT * fEventsPerPoint)
                    {
                        cShortsReadoutChip.push_back(cIndex);
                        LOG(DEBUG) << BOLDRED << "Possible short in channel " << +cIndex << RESET;
                    }
                    if(cBitset[cIndex] == 1 && cReadoutChipHits->getChannelContainer<uint16_t>()->at(cIndex) == fEventsPerPoint) { cInjectionsReadoutChip.push_back(cIndex); }
                }

                if(cInjectionsReadoutChip.size() == 0)
                {
                    LOG(INFO) << BOLDRED << "Problem injecting charge in readout chip" << +cChip->getId() << " on FE hybrid " << +cHybrid->getId() << " .. STOPPING PROCEDURE!" << RESET;
                    exit(FAILED_INJECTION);
                }
            }
        }
    }
}

// //Hacky, temporary
// void ShortFinder::Count(BeBoard* pBoard, const ChannelGroup<NSSACHANNELS>* pGroup)
// {

//     auto cBitset = std::bitset<NSSACHANNELS>( pGroup->getBitset() );
//     auto& cThisShortsContainer = fShortsContainer.at(pBoard->getIndex());
//     auto& cThisHitsContainer = fHitsContainer.at(pBoard->getIndex());
//     auto& cShorts = fShorts.at(pBoard->getIndex());
//     auto& cInjections = fInjections.at(pBoard->getIndex());

//     for(auto cOpticalGroup : *pBoard)
//     {
//         auto& cOpticalGroupShorts = cThisShortsContainer->at(cOpticalGroup->getIndex());
//         auto& cOpticalGroupHits = cThisHitsContainer->at(cOpticalGroup->getIndex());
//         auto& cShortsOpticalGroup = cShorts->at(cOpticalGroup->getIndex());
//         auto& cInjectionsOpticalGroup = cInjections->at(cOpticalGroup->getIndex());

//         for (auto cHybrid : *cOpticalGroup)
//         {
//             auto& cHybridShorts = cOpticalGroupShorts->at(cHybrid->getIndex());
//             auto& cHybridHits = cOpticalGroupHits->at(cHybrid->getIndex());
//             auto& cShortsHybrid = cShortsOpticalGroup->at(cHybrid->getIndex());
//             auto& cInjectionsHybrid = cInjectionsOpticalGroup->at(cHybrid->getIndex());
//             for (auto cChip : *cHybrid)
//             {

//                 auto& cReadoutChipShorts = cHybridShorts->at(cChip->getIndex());
//                 auto& cReadoutChipHits = cHybridHits->at(cChip->getIndex());
//                 auto& cShortsReadoutChip = cShortsHybrid->at(cChip->getIndex())->getSummary<ChannelList>();
//                 auto& cInjectionsReadoutChip = cInjectionsHybrid->at(cChip->getIndex())->getSummary<ChannelList>();

//                 for( size_t cIndex=0; cIndex < cBitset.size(); cIndex++ )
//                 {
// 		    //LOG (INFO) << BOLDRED <<"SF "<< cReadoutChipHits->getChannelContainer<uint16_t>()->at(cIndex)<<"
// "<<fEventsPerPoint<< RESET;
// 		   // LOG (INFO) << BOLDRED <<" "<< float(abs(cReadoutChipHits->getChannelContainer<uint16_t>()->at(cIndex) -
// fEventsPerPoint))/float(fEventsPerPoint)<< RESET;

//                     //LOG (INFO) << BOLDRED <<"SF "<< cBitset[cIndex]<<" "<<
//                     cReadoutChipShorts->getChannelContainer<uint16_t>()->at(cIndex) <<" "<< THRESHOLD_SHORT<<"
//                     "<<fEventsPerPoint<< RESET; if (cBitset[cIndex] == 0 &&
//                     cReadoutChipShorts->getChannelContainer<uint16_t>()->at(cIndex) > THRESHOLD_SHORT*fEventsPerPoint
//                     )
//                     {
//                         cShortsReadoutChip.push_back(cIndex);
//                         LOG (INFO) << BOLDRED << "Possible short in channel " << +cIndex << RESET;
//                     }
//                     if( cBitset[cIndex] == 1 &&
//                     float(abs(cReadoutChipHits->getChannelContainer<uint16_t>()->at(cIndex) -
//                     fEventsPerPoint))/float(fEventsPerPoint)<THRESHOLD_IN )
//                     {
//                         cInjectionsReadoutChip.push_back(cIndex);
//                     }
//                 }

//                 if( cInjectionsReadoutChip.size() == 0 )
//                 {
//                     LOG (INFO) << BOLDRED << "Problem injecting charge in readout chip"
//                         << +cChip->getId()
//                         << " on FE hybrid "
//                         << +cHybrid->getId()
//                         << " .. STOPPING PROCEDURE!"
//                         << RESET;
//                     exit(FAILED_INJECTION);
//                 }
//             }
//         }
//     }

// }

void ShortFinder::FindShortsPS(BeBoard* pBoard)
{
    // make sure that the correct trigger source is enabled
    // async injection trigger
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 12}); // Trigger source? 10 or 12?
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // make sure async mode is enabled
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "AnalogueAsync", 1);
    // first .. set injection amplitude to 0 and find pedestal
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "InjectedCharge", 0);

    // global data container is ..
    // an occupancy container
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    // find pedestal
    float cOccTarget = 0.5;
    // this->bitWiseScan("Threshold", fEventsPerPoint, cOccTarget);
    DetectorDataContainer cPedestalContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cPedestalContainer);
    float cMeanValue       = 0;
    int   cThresholdOffset = 8;
    int   cNchips          = 0;
    for(auto cBoardData: cPedestalContainer) // for on boards - begin
    {
        for(auto cOpticalGroupData: *cBoardData) // for on opticalGroup - begin
        {
            for(auto cHybridData: *cOpticalGroupData) // for on hybrid - begin
            {
                cNchips += cHybridData->size();
                for(auto cChipData: *cHybridData) // for on chip - begin
                {
                    ReadoutChip* cChip = static_cast<ReadoutChip*>(
                        fDetectorContainer->at(cBoardData->getIndex())->at(cOpticalGroupData->getIndex())->at(cHybridData->getIndex())->at(cChipData->getIndex()));
                    auto cThreshold                   = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                    cChipData->getSummary<uint16_t>() = cThreshold;
                    cMeanValue += cThreshold;
                    // set threshold a little bit lower than 90% level
                    fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold + cThresholdOffset);

                    LOG(INFO) << GREEN << "\t..Threshold at " << std::setprecision(2) << std::fixed << 100 * cOccTarget << " percent occupancy value for BeBoard " << +cBoardData->getId()
                              << " OpticalGroup " << +cOpticalGroupData->getId() << " Hybrid " << +cHybridData->getId() << " Chip " << +cChipData->getId() << " = " << cThreshold
                              << " [ setting threshold for short finding to " << +(cThreshold + cThresholdOffset) << " DAC units]" << RESET;
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on board - end
    LOG(INFO) << BOLDBLUE << "Mean Threshold at " << std::setprecision(2) << std::fixed << 100 * cOccTarget << " percent occupancy value " << cMeanValue / cNchips << RESET;

    // now configure injection amplitude to
    // whatever will be used for short finding
    // this is in the xml
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "InjectedCharge", fTestPulseAmplitude);

#ifdef __USE_ROOT__
    // create TTree for shorts: shortsTree
    auto fShortsTree = new TTree("shortsTree", "Shorted channels in the hybrid");
    // create variables for TTree branches
    TString              fShortsTreeParameter = -1;
    std::vector<uint8_t> fShortsTreeValue     = {};
    // create branches
    fShortsTree->Branch("Chip", &fShortsTreeParameter);
    fShortsTree->Branch("Value", &fShortsTreeValue);
#endif

    // container to hold information on shorts found
    DetectorDataContainer cShortsContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cShortsContainer);
    // going to inject in every Nth chnannel at a time
    int cInjectionPeriod = 4;
    // std::vector< std::vector< std::vector<uint8_t> > > cAllShorts (4, cOpticalReadout->at(0)->at(0)->size(),0);
    // std::vector< std::vector< std::vector<uint8_t> > > cAllShorts (cInjectionPeriod, std::vector< std::vector<uint8_t> >(8, std::vector<uint8_t>(0) ) );
    std::vector<std::vector<uint8_t>> cAllShorts(8, std::vector<uint8_t>(0));
    for(int cInject = 0; cInject < cInjectionPeriod; cInject++)
    {
        LOG(INFO) << BOLDBLUE << "Looking for shorts in injection group#" << +cInject << RESET;
        // configure injection
        for(auto cOpticalReadout: *pBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                // set AMUX on all SSAs to highZ
                for(auto cReadoutChip: *cHybrid)
                {
                    // add check for SSA
                    if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;

                    LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << RESET;

                    // let's say .. only enable injection in even channels first
                    for(uint8_t cChnl = 0; cChnl < cReadoutChip->size(); cChnl++)
                    {
                        char    cRegName[100];
                        uint8_t cEnable = (uint8_t)(((int)(cChnl) % cInjectionPeriod) == cInject);
                        std::sprintf(cRegName, "ENFLAGS_S%d", static_cast<int>(1 + cChnl));
                        auto    cRegValue = fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegName);
                        uint8_t cNewValue = (cRegValue & 0xF) | (cEnable << 4);
                        LOG(DEBUG) << BOLDBLUE << "\t\t..ENGLAG reg on channel#" << +cChnl << " is set to " << std::bitset<5>(cRegValue) << " want to set injection to : " << +cEnable
                                   << " so new value would be " << std::bitset<5>(cNewValue) << RESET;
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cNewValue);
                    }
                } // chip
            }     // hybrid
        }         // opticalGroup

        LOG(INFO) << "SSAs configured..." << RESET;
        bool retry = true;

        for(int i = 0; i < 2 && retry; i++) // 'Retry' to read events when the event returns with 0 on every channel of every chip.
        {
            retry = false;
            // read back events
            this->ReadNEvents(pBoard, fEventsPerPoint);
            const std::vector<Event*>& cEvents = this->GetEvents();
            // const std::vector<Event*>& cEvents = this->GetEvents(pBoard);
            // iterate over FE objects and check occupancy
            for(auto cEvent: cEvents)
            {
                for(auto cOpticalReadout: *pBoard)
                {
                    for(auto cHybrid: *cOpticalReadout)
                    {
                        int cTotalCountInjectedChnls = 0;
                        // set AMUX on all SSAs to highZ
                        for(auto cReadoutChip: *cHybrid)
                        {
                            int cChipCountInjectedChnls = 0;
                            // add check for SSA
                            if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;

                            LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << RESET;
                            auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());
                            // let's say .. only enable injection in even channels first
                            std::vector<uint8_t> cShorts(0);
                            for(uint8_t cChnl = 0; cChnl < cReadoutChip->size(); cChnl++)
                            {
                                bool cInjectionEnabled = (((int)(cChnl) % cInjectionPeriod) == cInject);
                                if(!cInjectionEnabled && cHitVector[cChnl] > THRESHOLD_SHORT * fEventsPerPoint)
                                {
                                    LOG(INFO) << BOLDRED << "\t\t\t.. Potential Short in SSA" << +cReadoutChip->getId() << " channel#" << +cChnl << " when injecting in group#" << +cInject
                                              << " ... found " << +cHitVector[cChnl] << " counts." << RESET;
                                    cShorts.push_back(cChnl);
                                    cAllShorts.at((int)cReadoutChip->getId()).push_back(cChnl);
                                }
                                if(cInjectionEnabled)
                                {
                                    cChipCountInjectedChnls += cHitVector[cChnl];
                                    cTotalCountInjectedChnls += cHitVector[cChnl];
                                    LOG(INFO) << BOLDBLUE << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                                    if(cHitVector[cChnl] == 0) LOG(INFO) << BOLDBLUE << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                                }
                                else if(cHitVector[cChnl] != 0)
                                    LOG(DEBUG) << BOLDMAGENTA << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                                else
                                    LOG(DEBUG) << BOLDGREEN << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                            } // chnl
                            auto& cShortsData = cShortsContainer.at(pBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cReadoutChip->getIndex());
                            // first time .. set to 0
                            if(cInject == 0) cShortsData->getSummary<uint16_t>() = 0;
                            cShortsData->getSummary<uint16_t>() += (uint16_t)cShorts.size();
                            LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << " found " << +cShorts.size() << " potential shorts..."
                                       << " total shorts found are " << +cShortsData->getSummary<uint16_t>() << RESET;
                        } // chip

                        if(cTotalCountInjectedChnls == 0)
                        {
                            LOG(INFO) << BOLDRED << "All injected channels on all chips have 0 hits." << RESET;
#ifdef __USE_ROOT__
                            fillSummaryTree("Empty readout (ShortFinder procedure)", 0.0);
#endif
                            retry = true;
                        }

                    } // hybrid
                }     // module
            }         // event loop
        }             // retry loop
    }

    // std::vector<uint8_t> cShorts(0)
    // for(int i = 0; i < 8; i++){
    //     for(int cInject = 0 < cInject < cInjectionPeriod; cInject++){
    //         cAllShorts.at(cInject).at((int)cReadoutChip->getId()) = cShorts;
    //     }
    // }
#ifdef __USE_ROOT__
    for(auto cReadoutChip: *pBoard->at(0)->at(0))
    {
        fShortsTreeParameter.Clear();
        fShortsTreeParameter = "Chip_" + std::to_string(cReadoutChip->getId());
        fShortsTreeValue     = cAllShorts.at(cReadoutChip->getId());
        fShortsTree->Fill();
    }
#endif

    // print summary
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA && cReadoutChip->getFrontEndType() != FrontEndType::SSA2) continue;

                auto& cShortsData = cShortsContainer.at(pBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cReadoutChip->getIndex());
                if(cShortsData->getSummary<uint16_t>() == 0)
                    LOG(INFO) << BOLDGREEN << "SSA" << +cReadoutChip->getId() << " found " << +cShortsData->getSummary<uint16_t>() << " shorts in total when injecting in every " << +cInjectionPeriod
                              << "th channel " << RESET;
                else
                    LOG(INFO) << BOLDRED << "SSA" << +cReadoutChip->getId() << " found " << +cShortsData->getSummary<uint16_t>() << " shorts in total when injecting in every " << +cInjectionPeriod
                              << "th channel " << RESET;
#ifdef __USE_ROOT__
                std::string param = Form("Shorts_%d", cReadoutChip->getId());
                fillSummaryTree(param, (double_t)cShortsData->getSummary<uint16_t>());
#endif
            } // chip
        }     // hybrid
    }         // opticalGroup
}
void ShortFinder::FindShorts2S(BeBoard* pBoard)
{
    // configure test pulse on chip
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "TestPulsePotNodeSel", 0xFF - fTestPulseAmplitude);
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "TestPulseDelay", 0);
    uint16_t cDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse") - 1;
    setSameDacBeBoard(static_cast<BeBoard*>(pBoard), "TriggerLatency", cDelay);
    fBeBoardInterface->ChipReSync(pBoard); // NEED THIS! ??
    LOG(INFO) << BOLDBLUE << "L1A latency set to " << +cDelay << RESET;

    // for (auto cBoard : this->fBoardVector)
    uint8_t cTestGroup = 0;
    LOG(INFO) << BOLDBLUE << "Starting short finding loop for 2S hybrid " << RESET;
    for(auto cGroup: *getChannelGroupHandlerContainer()->getObject(pBoard->getId())->getObject(0)->getObject(0)->getObject(0)->getSummary<std::shared_ptr<ChannelGroupHandler>>().get())
    {
        setSameGlobalDac("TestPulseGroup", cTestGroup);
        // bitset for this group
        auto cBitset = std::bitset<NCHANNELS>(std::static_pointer_cast<const ChannelGroup<NCHANNELS>>(cGroup)->getBitset());
        LOG(INFO) << BOLDBLUE << "Injecting charge into CBCs using test capacitor " << +cTestGroup << RESET;
        LOG(DEBUG) << BOLDBLUE << "Test pulse channel mask is " << cBitset << RESET;

        auto& cThisShortsContainer = fShortsContainer.at(pBoard->getIndex());
        auto& cThisHitsContainer   = fHitsContainer.at(pBoard->getIndex());

        this->ReadNEvents(pBoard, fEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents();
        for(auto cEvent: cEvents)
        {
            auto cEventCount = cEvent->GetEventCount();
            for(auto cOpticalGroup: *pBoard)
            {
                auto& cShortsContainer = cThisShortsContainer->at(cOpticalGroup->getIndex());
                auto& cHitsContainer   = cThisHitsContainer->at(cOpticalGroup->getIndex());

                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cHybridShorts = cShortsContainer->at(cHybrid->getIndex());
                    auto& cHybridHits   = cHitsContainer->at(cHybrid->getIndex());
                    for(auto cChip: *cHybrid)
                    {
                        auto& cReadoutChipShorts = cHybridShorts->at(cChip->getIndex());
                        auto& cReadoutChipHits   = cHybridHits->at(cChip->getIndex());

                        auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                        LOG(DEBUG) << BOLDBLUE << "\t\tGroup " << +cTestGroup << " FE" << +cHybrid->getId() << " .. CBC" << +cChip->getId() << ".. Event " << +cEventCount << " - " << +cHits.size()
                                   << " hits found/" << +cBitset.count() << " channels in test group" << RESET;
                        for(auto cHit: cHits)
                        {
                            if(cBitset[cHit] == 0)
                                cReadoutChipShorts->getChannelContainer<uint16_t>()->at(cHit) += 1;
                            else
                                cReadoutChipHits->getChannelContainer<uint16_t>()->at(cHit) += 1;
                        }
                    }
                }
            }
        }
        this->Count(pBoard, std::static_pointer_cast<ChannelGroup<NCHANNELS>>(cGroup));
        cTestGroup++;
    }
}
void ShortFinder::FindShorts()
{
    uint8_t cFirmwareTPdelay      = 100;
    uint8_t cFirmwareTriggerDelay = 200;

    // set-up for TP
    fAllChan                     = true;
    fMaskChannelsFromOtherGroups = !this->fAllChan;
    SetTestAllChannels(fAllChan);
    // enable TP injection
    enableTestPulse(true);

    // configure test pulse trigger
    if(fWithSSA) { static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM(fEventsPerPoint, 10000, 6, 0, 0); }
    else
    {
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cFirmwareTPdelay, cFirmwareTriggerDelay, 1000);
    }
    for(auto cBoard: *fDetectorContainer)
    {
        LOG(INFO) << BOLDBLUE << "Starting short finding procedure on BeBoard#" << +cBoard->getIndex() << RESET;
        if(fWithCBC)
            this->FindShorts2S(static_cast<BeBoard*>(cBoard));
        else if(fWithSSA)
            this->FindShortsPS(static_cast<BeBoard*>(cBoard));
        else
            LOG(INFO) << BOLDRED << "\t....Short finding for this hybrid type not yet implemented." << RESET;
    }
}
void ShortFinder::Running()
{
    Initialise();
    this->FindShorts();
    this->Print();
}
