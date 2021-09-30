#include "ShortFinder.h"
#include "CBCChannelGroupHandler.h"
#include "CommonVisitors.h"
#include "ContainerFactory.h"
#include "DataContainer.h"
#include "Occupancy.h"
#include "SSAChannelGroupHandler.h"
#include "Visitor.h"

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
    fWithSSA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    LOG(INFO) << "With SSA set to " << ((fWithSSA) ? 1 : 0) << RESET;

    if(ShortFinder::fWithCBC)
    {
        fChannelGroupHandler = new CBCChannelGroupHandler();
        fChannelGroupHandler->setChannelGroupParameters(16, 2);
        fSkipMaskedChannels = findValueInSettings<double>("SkipMaskedChannels", 0);
        this->SetSkipMaskedChannels(fSkipMaskedChannels);
    }
    if(ShortFinder::fWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    // THRESHOLD_IN=0.01;

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
        auto& cRegMapThisBoard                                                 = fRegMapContainer.at(cBoard->getIndex());
        auto& cShorts                                                          = fShorts.at(cBoard->getIndex());
        auto& cInjections                                                      = fInjections.at(cBoard->getIndex());
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
void ShortFinder::Count(BeBoard* pBoard, const ChannelGroup<NCHANNELS>* pGroup)
{
    auto  cBitset              = std::bitset<NCHANNELS>(pGroup->getBitset());
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
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // make sure async mode is enabled
    setSameDacBeBoard(pBoard, "AnalogueAsync", 1);
    // first .. set injection amplitude to 0 and find pedestal
    setSameDacBeBoard(pBoard, "InjectedCharge", 0);

    // global data container is ..
    // an occupancy container
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    // find pedestal
    float cOccTarget = 0.5;
    this->bitWiseScan("Threshold", fEventsPerPoint, cOccTarget);
    DetectorDataContainer cPedestalContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cPedestalContainer);
    float cMeanValue       = 0;
    int   cThresholdOffset = 5;
    int   cNchips          = 0;
    for(auto cBoardData: cPedestalContainer) // for on boards - begin
    {
        for(auto cOpticalGroupData: *cBoardData) // for on opticalGroup - begin
        {
            for(auto cHybridData: *cOpticalGroupData) // for on hybrid - begin
            {
                cNchips += cHybridData->size();
                for(auto cROCData: *cHybridData) // for on chip - begin
                {
                    ReadoutChip* cChip =
                        static_cast<ReadoutChip*>(fDetectorContainer->at(cBoardData->getIndex())->at(cOpticalGroupData->getIndex())->at(cHybridData->getIndex())->at(cROCData->getIndex()));
                    auto cThreshold                  = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                    cROCData->getSummary<uint16_t>() = cThreshold;
                    cMeanValue += cThreshold;
                    // set threshold a little bit lower than 90% level
                    fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold + cThresholdOffset);

                    LOG(INFO) << GREEN << "\t..Threshold at " << std::setprecision(2) << std::fixed << 100 * cOccTarget << " percent occupancy value for BeBoard " << +cBoardData->getId()
                              << " OpticalGroup " << +cOpticalGroupData->getId() << " Hybrid " << +cHybridData->getId() << " ROC " << +cROCData->getId() << " = " << cThreshold
                              << " [ setting threshold for short finding to " << +(cThreshold + cThresholdOffset) << " DAC units]" << RESET;
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on board - end
    LOG(INFO) << BOLDBLUE << "Mean Threshold at " << std::setprecision(2) << std::fixed << 100 * cOccTarget << " percent occupancy value " << cMeanValue / cNchips << RESET;

    // now configure injection amplitude to
    // whatever will be used for short finding
    // this is in the xml
    setSameDacBeBoard(pBoard, "InjectedCharge", fTestPulseAmplitude);

    // container to hold information on shorts found
    DetectorDataContainer cShortsContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cShortsContainer);
    // going to inject in every Nth chnannel at a time
    int cInjectionPeriod = 4;
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
                    if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

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

        // read back events
        this->ReadNEvents(pBoard, fEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents();
        // iterate over FE objects and check occupancy
        for(auto cEvent: cEvents)
        {
            for(auto cOpticalReadout: *pBoard)
            {
                for(auto cHybrid: *cOpticalReadout)
                {
                    // set AMUX on all SSAs to highZ
                    for(auto cReadoutChip: *cHybrid)
                    {
                        // add check for SSA
                        if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                        LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << RESET;
                        auto cHitVector = cEvent->GetHits(cHybrid->getId(), cReadoutChip->getId());
                        // let's say .. only enable injection in even channels first
                        std::vector<uint8_t> cShorts(0);
                        for(uint8_t cChnl = 0; cChnl < cReadoutChip->size(); cChnl++)
                        {
                            bool cInjectionEnabled = (((int)(cChnl) % cInjectionPeriod) == cInject);
                            if(!cInjectionEnabled && cHitVector[cChnl] > THRESHOLD_SHORT * fEventsPerPoint)
                            {
                                LOG(INFO) << BOLDRED << "\t\t\t.. Potential Short in SSA" << +cReadoutChip->getId() << " channel#" << +cChnl << " when injecting in group#" << +cInject << " ... found "
                                          << +cHitVector[cChnl] << " counts." << RESET;
                                cShorts.push_back(cChnl);
                            }
                            if(cInjectionEnabled)
                                LOG(DEBUG) << BOLDBLUE << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                            else
                                LOG(DEBUG) << BOLDMAGENTA << "\t\t..Chnl#" << +cChnl << " counts : " << +cHitVector[cChnl] << RESET;
                        } // chnl
                        auto& cShortsData = cShortsContainer.at(pBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cReadoutChip->getIndex());
                        // first time .. set to 0
                        if(cInject == 0) cShortsData->getSummary<uint16_t>() = 0;
                        cShortsData->getSummary<uint16_t>() += (uint16_t)cShorts.size();
                        LOG(DEBUG) << BOLDBLUE << "\t...SSA" << +cReadoutChip->getId() << " found " << +cShorts.size() << " potential shorts..."
                                   << " total shorts found are " << +cShortsData->getSummary<uint16_t>() << RESET;

                    } // chip
                }     // hybrid
            }         // opticalGroup
        }             // event loop
    }

    // print summary
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                auto& cShortsData = cShortsContainer.at(pBoard->getIndex())->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex())->at(cReadoutChip->getIndex());
                if(cShortsData->getSummary<uint16_t>() == 0)
                    LOG(INFO) << BOLDGREEN << "SSA" << +cReadoutChip->getId() << " found " << +cShortsData->getSummary<uint16_t>() << " shorts in total when injecting in every " << +cInjectionPeriod
                              << "th channel " << RESET;
                else
                    LOG(INFO) << BOLDRED << "SSA" << +cReadoutChip->getId() << " found " << +cShortsData->getSummary<uint16_t>() << " shorts in total when injecting in every " << +cInjectionPeriod
                              << "th channel " << RESET;
            } // chip
        }     // hybrid
    }         // opticalGroup
}
void ShortFinder::FindShorts2S(BeBoard* pBoard)
{
    // configure test pulse on chip
    setSameDacBeBoard(pBoard, "TestPulsePotNodeSel", 0xFF - fTestPulseAmplitude);
    setSameDacBeBoard(pBoard, "TestPulseDelay", 0);
    uint16_t cDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse") - 1;
    setSameDacBeBoard(pBoard, "TriggerLatency", cDelay);
    fBeBoardInterface->ChipReSync(pBoard); // NEED THIS! ??
    LOG(INFO) << BOLDBLUE << "L1A latency set to " << +cDelay << RESET;

    // for (auto cBoard : this->fBoardVector)
    uint8_t cTestGroup = 0;
    LOG(INFO) << BOLDBLUE << "Starting short finding loop for 2S hybrid " << RESET;
    for(auto cGroup: *fChannelGroupHandler)
    {
        setSameGlobalDac("TestPulseGroup", cTestGroup);
        // bitset for this group
        auto cBitset = std::bitset<NCHANNELS>(static_cast<const ChannelGroup<NCHANNELS>*>(cGroup)->getBitset());
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
        this->Count(pBoard, static_cast<const ChannelGroup<NCHANNELS>*>(cGroup));
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
