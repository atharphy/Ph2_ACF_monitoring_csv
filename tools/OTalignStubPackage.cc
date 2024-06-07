#include "tools/OTalignStubPackage.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/DataContainer.h"
#include <algorithm>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignStubPackage::fCalibrationDescription = "Find stub package delay to properly decode stubs in the FC7";

OTalignStubPackage::OTalignStubPackage() : Tool() {}

OTalignStubPackage::~OTalignStubPackage() {}

void OTalignStubPackage::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9");
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11");

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignStubPackage.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignStubPackage::ConfigureCalibration() {}

void OTalignStubPackage::Running()
{
    LOG(INFO) << "Starting OTalignStubPackage measurement.";
    Initialise();
    AlignStubPackage();
    LOG(INFO) << "Done with OTalignStubPackage.";
    Reset();
}

void OTalignStubPackage::Stop(void)
{
    LOG(INFO) << "Stopping OTalignStubPackage measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTalignStubPackage.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignStubPackage stopped.";
}

void OTalignStubPackage::Pause() {}

void OTalignStubPackage::Resume() {}

void OTalignStubPackage::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTalignStubPackage::AlignStubPackage()
{
    uint16_t numberOfEvents                                   = 10;
    uint16_t triggerFrequency                                 = 400;   // kHz
    uint16_t clockFrequency                                   = 40000; // kHz
    uint16_t cMaxBxCounter                                    = 3564;
    uint16_t numberOfClockCyclesAfterInitialReset             = 100; // safety margin to avoid roll over
    uint16_t numberOfClockCyclesBetweenTwoConsecutiveTriggers = clockFrequency / triggerFrequency;

    if(numberOfClockCyclesBetweenTwoConsecutiveTriggers != float(clockFrequency / float(triggerFrequency)))
    {
        std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Error: numberOfClockCyclesBetweenTwoConsecutiveTriggers must be an integer! Aborting..." << std::endl;
        abort();
    }
    if(numberOfClockCyclesAfterInitialReset + numberOfEvents * numberOfClockCyclesBetweenTwoConsecutiveTriggers >= cMaxBxCounter)
    {
        std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Error: BxId roll over not handled by the procedure! Aborting" << std::endl;
        abort();
    }

    uint16_t numberOfEventsToSkip = 1; // Ignore first events, bug with resync in FW?

    std::vector<uint16_t> emptyBunchCrossingId(numberOfEvents, 0xFFFF);
    DetectorDataContainer theBunchCrossingIdContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, theBunchCrossingIdContainer, emptyBunchCrossingId);

    std::vector<bool>     emptyBestPackageDelay(8, false);
    DetectorDataContainer theBestPackageDelayContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<bool>>(*fDetectorContainer, theBestPackageDelayContainer, emptyBestPackageDelay);

    std::vector<uint16_t> emptyBunchCrossingIdDifference(numberOfEvents - 1 - numberOfEventsToSkip, 0x7FFF);
    DetectorDataContainer theBunchCrossingIdDifferenceContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, theBunchCrossingIdDifferenceContainer, emptyBunchCrossingIdDifference);

    for(auto theBoard: *fDetectorContainer)
    {
        // set board and get interface
        fBeBoardInterface->setBoard(theBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        // make sure you're only sending one trigger at a time here
        LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                // disable all FEs
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
            }
        }

        std::vector<std::pair<std::string, uint32_t>> initialRegisterVector;
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", triggerFrequency});
        initialRegisterVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 1}); // Ensure a fast reset is sent before reading events to avoid roll over
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
        initialRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
        fBeBoardInterface->WriteBoardMultReg(theBoard, initialRegisterVector);

        for(uint8_t thePackageDelay = 0; thePackageDelay < 8; thePackageDelay++)
        {
            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] thePackageDelay = " << +thePackageDelay << std::endl;

            uint32_t packageDelayValue = 0;
            for(size_t link = 0; link < 10; ++link) { packageDelayValue = packageDelayValue | (thePackageDelay << (3 * link)); }

            std::vector<std::pair<std::string, uint32_t>> packageDelayRegisterVector;
            packageDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9", packageDelayValue});
            packageDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11", packageDelayValue & 0x3F});
            fBeBoardInterface->WriteBoardMultReg(theBoard, packageDelayRegisterVector);

            // reset stub readout to load new setting
            cInterface->Bx0Alignment();

            // read events
            ReadNEvents(theBoard, numberOfEvents);
            const std::vector<Event*>& theEventVector = this->GetEvents();

            // retrieve bunch crossing id for all events
            for(size_t eventNumber = 0; eventNumber < numberOfEvents; ++eventNumber)
            {
                auto theEvent = theEventVector[eventNumber];
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        auto& eventBxIdVector        = theBunchCrossingIdContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<uint16_t>>();
                        eventBxIdVector[eventNumber] = theEvent->BxId(theHybrid->getId());
                        if(eventNumber > numberOfEventsToSkip)
                        {
                            auto& bxIdDifferenceVector =
                                theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<int16_t>>();
                            bxIdDifferenceVector[eventNumber - 1 - numberOfEventsToSkip] = eventBxIdVector[eventNumber] - eventBxIdVector[eventNumber - 1];
                        }
                    }
                }
            }

            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto theBunchCrossingIdDifference =
                        theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<int16_t>>();
                    // std::cout << "Hybrid id = " << theHybrid->getId() << std::endl;
                    // for(auto bxIdDifference: theBunchCrossingIdDifference) std::cout << bxIdDifference << " ";
                    // std::cout << std::endl;
                }
            }

            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto theBunchCrossingIdDifference =
                        theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<uint16_t>>();
                    // remove duplicate
                    std::sort(theBunchCrossingIdDifference.begin(), theBunchCrossingIdDifference.end());
                    theBunchCrossingIdDifference.erase(unique(theBunchCrossingIdDifference.begin(), theBunchCrossingIdDifference.end()), theBunchCrossingIdDifference.end());
                    bool isSameAndCorrectBx = theBunchCrossingIdDifference.size() == 1 && theBunchCrossingIdDifference[0] == numberOfClockCyclesBetweenTwoConsecutiveTriggers;
                    theBestPackageDelayContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<bool>>()[thePackageDelay] = isSameAndCorrectBx;
                }
            }
        }

        uint32_t bestPackageDelayLink0Link9   = 0;
        uint32_t bestPackageDelayLink10Link11 = 0;

        for(auto theOpticalGroup: *theBoard)
        {
            std::vector<uint8_t> hybridBestPackageDelay;
            for(auto theHybrid: *theOpticalGroup)
            {
                auto theBestPackageDelayVector = theBestPackageDelayContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<bool>>();
                auto numberOfBestPackageDelays = std::count(theBestPackageDelayVector.begin(), theBestPackageDelayVector.end(), true);
                if(numberOfBestPackageDelays != 1)
                {
                    LOG(ERROR) << BOLDRED << "ERROR for Board " << +theBoard->getId() << " OpticalGroup " << +theOpticalGroup->getId() << " Hybrid " << +theHybrid->getId()
                               << ": number of best package delay = " << numberOfBestPackageDelays << ", expected to be 1" << RESET;
                    continue;
                }
                hybridBestPackageDelay.push_back(std::find_if(theBestPackageDelayVector.begin(), theBestPackageDelayVector.end(), [](bool value) { return value; }) -
                                                 theBestPackageDelayVector.begin()); // find intex of the best phase
            }
            if(hybridBestPackageDelay.size() > 0)
            {
                // remove duplicate
                std::sort(hybridBestPackageDelay.begin(), hybridBestPackageDelay.end());
                hybridBestPackageDelay.erase(unique(hybridBestPackageDelay.begin(), hybridBestPackageDelay.end()), hybridBestPackageDelay.end());
                if(hybridBestPackageDelay.size() > 1)
                {
                    LOG(ERROR) << BOLDRED << "ERROR for Board " << +theBoard->getId() << " OpticalGroup " << +theOpticalGroup->getId()
                               << ": FW cannot handle different stub package delay within same OpticalGroup, setting the value found for Hybrid " << +theOpticalGroup->getFirstObject()->getId()
                               << RESET;
                }

                if(theOpticalGroup->getId() < 10)
                    bestPackageDelayLink0Link9 = (hybridBestPackageDelay[0] << theOpticalGroup->getId() % 10 * 3) | bestPackageDelayLink0Link9;
                else
                    bestPackageDelayLink10Link11 = (hybridBestPackageDelay[0] << theOpticalGroup->getId() % 10 * 3) | bestPackageDelayLink10Link11;
            }
        }

        std::vector<std::pair<std::string, uint32_t>> finalDelayRegisterVector;
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9", bestPackageDelayLink0Link9});
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11", bestPackageDelayLink10Link11});
        fBeBoardInterface->WriteBoardMultReg(theBoard, finalDelayRegisterVector);
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTalignStubPackage.fillBestStubPackageDelay(theBestPackageDelayContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theBestStubPackageDelayContainerSerialization("OTalignStubPackageBestStubPackageDelay");
        theBestStubPackageDelayContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestPackageDelayContainer);
    }
#endif
}

void OTalignStubPackage::AlignStubPackage(BeBoard* pBoard)
{
    size_t cTriggerMult  = 0;
    size_t cDelayAfterTP = 300;
    // set board and get interface
    fBeBoardInterface->setBoard(pBoard->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    // make sure you're only sending one trigger at a time here
    LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;
    // sparsification of
    bool                 cSparsified = pBoard->getSparsification();
    std::vector<uint8_t> cFeEnableRegs(0);
    // disable FEs for all hybrids
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::AlignStubPackage Sparsification off " << RESET;

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
            // disable all FEs. . not needed here
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }
    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTPdelay    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    uint16_t cOriginalResetEn    = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");

    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cTriggerSrc = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cDelayAfterTP});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 1});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // first lets figure out how many hybrids are enabled
    auto cEnableMask = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.global.hybrid_enable");
    // and select one hybrid from each link
    uint32_t cNewMask = 0x00;
    for(auto cOpticalGroup: *pBoard)
    {
        bool cFirstOnLink = true;
        for(auto cHybrid: *cOpticalGroup)
        {
            if(!cFirstOnLink) continue;
            LOG(INFO) << BOLDMAGENTA << "\t\t..Hybrid#" << +cHybrid->getId() << " on Link#" << +cOpticalGroup->getId() << RESET;
            cNewMask     = cNewMask | (1 << cHybrid->getId());
            cFirstOnLink = false;
        }
    }
    LOG(INFO) << BOLDBLUE << "LinkAlignmentOT::AlignStubPackage setting hybrid enable register to " << std::bitset<32>(cNewMask) << RESET;

    bool cSkip = false;
    // Two final delay variables according to the registers
    uint32_t cFinalDelayOGs_link0_link9   = 0;
    uint32_t cFinalDelayOGs_link10_link11 = 0;
    if(!cSkip)
    {
        // Loop over all OG, stub package delay can be different for each OG
        for(auto cOpticalGroup: *pBoard)
        {
            // Get register name dependent of OG
            std::string cRegName;
            if(cOpticalGroup->getId() < 10)
                cRegName = "fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9";
            else
                cRegName = "fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11";

            // gethybrid IDs
            std::vector<uint8_t>                    cHybridIds(0);
            std::map<uint8_t, std::vector<uint8_t>> cHybridIdsMap;
            auto                                    cIter = cHybridIdsMap.find(cOpticalGroup->getId());
            if(cIter == cHybridIdsMap.end())
            {
                std::vector<uint8_t> cDummy;
                cDummy.clear();
                cHybridIdsMap[cOpticalGroup->getId()] = cDummy;
                cIter                                 = cHybridIdsMap.find(cOpticalGroup->getId());
            }
            bool cFirstOnLink = true;
            for(auto cHybrid: *cOpticalGroup)
            {
                if(!cFirstOnLink) continue;
                cHybridIds.push_back(cHybrid->getId());
                cIter->second.push_back(cHybrid->getId());
                cFirstOnLink = false;
            }

            // unique ids for each hybrid
            bool    cCorrectDelay = false;
            uint8_t cFinalDelayOG = 0;
            // now try and find correct package delay
            uint16_t cMaxBxCounter = 3564;
            uint32_t cNevents      = 10;

            LOG(DEBUG) << cMaxBxCounter << RESET;
            size_t cAttempt = 0;
            do {
                LOG(INFO) << BOLDMAGENTA << "Package delay alignment attempt#" << +cAttempt << RESET;
                for(uint8_t cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
                {
                    if(cCorrectDelay) continue;

                    LOG(INFO) << BOLDMAGENTA << "Trying a stub package delay set to " << +cPackageDelay << ".. check BxIds in SW" << RESET;
                    // Get register value according to OG and write it to the register
                    uint32_t cRegValue;
                    if(cOpticalGroup->getId() < 10)
                        cRegValue = (cPackageDelay << cOpticalGroup->getId() % 10 * 3) + cFinalDelayOGs_link0_link9;
                    else
                        cRegValue = (cPackageDelay << cOpticalGroup->getId() % 10 * 3) + cFinalDelayOGs_link10_link11;

                    LOG(INFO) << BOLDYELLOW << "OG#" << cOpticalGroup->getId() << "\t.. Package delay of " << +cPackageDelay << " -- reg value " << std::bitset<32>(cRegValue) << RESET;
                    fBeBoardInterface->WriteBoardReg(pBoard, cRegName, cRegValue);
                    // fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay", cPackageDelay);
                    cInterface->Bx0Alignment();

                    ReadNEvents(pBoard, cNevents);
                    const std::vector<Event*>& cEvents = this->GetEvents();
                    LOG(DEBUG) << BOLDBLUE << "Read back " << +cEvents.size() << " events from the FC7 ..." << RESET;

                    // fill map of BxIds for this hybrid
                    std::map<uint8_t, std::vector<int>> cBxIds;
                    for(auto& cEvent: cEvents)
                    {
                        for(auto cId: cHybridIds)
                        {
                            auto cIter = cBxIds.find(cId);
                            if(cIter == cBxIds.end())
                            {
                                std::vector<int> cDummy;
                                cDummy.clear();
                                cBxIds[cId] = cDummy;
                                cIter       = cBxIds.find(cId);
                            }
                            cIter->second.push_back(cEvent->BxId(cId));
                            LOG(INFO) << BOLDYELLOW << "Event#" << +cEvent->GetEventCount() << "\t.. Hybrid#" << +cId << " BxId is " << cEvent->BxId(cId) << RESET;
                        }
                    }

                    // check that BxIds are synchronous across single links
                    std::vector<uint8_t> cIdsToCompare(0);
                    for(auto cIter: cHybridIdsMap)
                    {
                        LOG(INFO) << BOLDBLUE << "\t..Checking Sync for hybrids on Link#" << +cIter.first << RESET;
                        bool cSyncThisLink = true;  // if there's only one hybrid by definition you are in sync
                        if(cIter.second.size() > 1) // either 1 or 2 hybrids per link
                        {
                            // check if the two hybrids are synchronous
                            LOG(INFO) << BOLDYELLOW << "\t.. checking sync between " << +cIter.second[0] << " and " << +cIter.second[1] << RESET;
                            auto& cBxIdsFirst  = cBxIds[cIter.second[0]];
                            auto& cBxIdsSecond = cBxIds[cIter.second[1]];
                            cSyncThisLink      = (cBxIdsFirst == cBxIdsSecond);
                            if(cSyncThisLink)
                                LOG(INFO) << BOLDGREEN << "Sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
                            else
                                LOG(INFO) << BOLDRED << "No Sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
                        }
                        // if in sync.. add first hybrid id to list
                        if(cSyncThisLink) { cIdsToCompare.push_back(cIter.second[0]); }
                        else
                            LOG(INFO) << BOLDRED << "\t..FAILED sync on Link#" << +cIter.first << " between Hybrid#" << +cIter.second[0] << " and Hybrid#" << +cIter.second[1] << RESET;
                    }
                    // if all the links are synchronous then..
                    // check which package delay has to be used for which OG
                    // The condition is that the package delay read out has to be larger than 8
                    if(cIdsToCompare.size() == cHybridIdsMap.size())
                    {
                        std::vector<uint16_t> cPairsCompared;
                        std::vector<uint8_t>  cMatchesFound;
                        // compare ids from all links
                        for(auto cIdFirst: cIdsToCompare)
                        {
                            for(auto cIdSecond: cIdsToCompare)
                            {
                                if(cIdFirst == cIdSecond) continue;
                                uint16_t cPairId = (std::max(cIdFirst, cIdSecond) << 8) | std::min(cIdFirst, cIdSecond);
                                if(std::find(cPairsCompared.begin(), cPairsCompared.end(), cPairId) != cPairsCompared.end()) continue;

                                uint8_t cMatchFound = (cBxIds[cIdFirst] == cBxIds[cIdSecond]);
                                if(cMatchFound)
                                {
                                    LOG(INFO) << BOLDGREEN << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " are identical.. next will check the difference" << RESET;
                                }
                                else
                                    LOG(INFO) << BOLDRED << "\t\t..BxIds from Hybrid#" << +cIdFirst << " and " << +cIdSecond << " DO NOT match.. " << RESET;
                                cMatchesFound.push_back(cMatchFound);
                                cPairsCompared.push_back(cPairId);
                            }
                        }
                        if(cIdsToCompare.size() == 1)
                        {
                            uint16_t cPairId = 0xFF << 8 | cIdsToCompare[0];
                            cPairsCompared.push_back(cPairId);
                            cMatchesFound.push_back(1);
                        }
                        // for those that match.. check BxId difference
                        std::vector<uint8_t> cFoundDelays(0);
                        // std::vector<uint8_t> cGoodBxId(0);
                        for(size_t cIndx = 0; cIndx < cMatchesFound.size(); cIndx++)
                        {
                            if(cMatchesFound[cIndx] == 0) continue;
                            uint8_t              cFirst = cPairsCompared[cIndx] & 0xFF;
                            uint8_t              cScnd  = (cPairsCompared[cIndx] >> 8) & 0xFF;
                            std::vector<uint8_t> cIdsToCheck(0);
                            cIdsToCheck.push_back(cFirst);
                            // 0xFF marks the case where there is no second hybrid to c
                            // compare against
                            if(cScnd != 0xFF) cIdsToCheck.push_back(cScnd);
                            // std::vector<uint8_t> cIdsToCheck{ cFirst, cScnd};
                            size_t cNFound = 0;
                            for(auto cIdToCheck: cIdsToCheck)
                            {
                                std::vector<int> cBxDifferences(0);
                                size_t           cNRollOvers = 0;
                                size_t           cCounter    = 0;
                                uint8_t          cGoodBxIds  = 0;
                                for(auto cBxId: cBxIds[cIdToCheck])
                                {
                                    if(cBxId > 8) cGoodBxIds++;
                                    if(cCounter > 0)
                                    {
                                        auto cPreviousBxId = cBxIds[cIdToCheck][cCounter - 1];
                                        int  cBxDifference = (cNRollOvers)*cMaxBxCounter + (cPreviousBxId % cMaxBxCounter);
                                        cNRollOvers += ((cPreviousBxId >= 2500) && (cPreviousBxId < cMaxBxCounter)) && (cBxId < cPreviousBxId) ? 1 : 0;
                                        cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxId % cMaxBxCounter) - cBxDifference;
                                        if(cBxId > (int)cDelayAfterTP)
                                        {
                                            LOG(INFO) << BOLDGREEN << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << " [ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
                                        }
                                        else
                                            LOG(INFO) << BOLDRED << "\t\t\t\t.. Diff#" << cCounter << " : " << cBxDifference << " [ BxID = " << cBxIds[cIdToCheck][cCounter] << " ]" << RESET;
                                        cBxDifferences.push_back(cBxDifference);
                                    }
                                    cCounter++;
                                }
                                if(std::adjacent_find(cBxDifferences.begin(), cBxDifferences.end(), std::not_equal_to<int>()) == cBxDifferences.end())
                                {
                                    if(cGoodBxIds == cBxIds[cIdToCheck].size())
                                    {
                                        LOG(INFO) << BOLDGREEN << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
                                        cNFound++;
                                    }
                                    else
                                        LOG(INFO) << BOLDRED << "\t\t\t..Constant BxId difference of " << +cBxDifferences[0] << " 40 MHz clks on Hybrid#" << +cIdToCheck << RESET;
                                }
                            }
                            cFoundDelays.push_back((cNFound == cIdsToCheck.size()) ? 1 : 0);
                        }
                        auto cNFound = std::accumulate(cFoundDelays.begin(), cFoundDelays.end(), 0);
                        if((size_t)cNFound == cMatchesFound.size() && cNFound != 0)
                        {
                            LOG(INFO) << BOLDGREEN << "All hybrids match for a package delay of " << +cPackageDelay << RESET;
                            cCorrectDelay = true;
                            cFinalDelayOG = cPackageDelay;
                            if(cOpticalGroup->getId() < 10)
                                cFinalDelayOGs_link0_link9 = cRegValue;
                            else
                                cFinalDelayOGs_link10_link11 = cRegValue;
                        }
                        else
                            LOG(INFO) << BOLDRED << "For a package delay of " << +cPackageDelay << " found " << +cNFound << "/" << cMatchesFound.size()
                                      << " pairs of hybrids with a constant difference in BxIds" << RESET;
                    } // Ids are synchronous across each link
                    else
                        LOG(INFO) << BOLDRED << "For a package delay of " << +cPackageDelay << " DE-SYNC in one of the links..." << RESET;
                } // pkg delay
                cAttempt++;
            } while(cAttempt < 1 && !cCorrectDelay);
            LOG(INFO) << BOLDGREEN << "Optimal package delay of OG#" << cOpticalGroup->getId() << " is: " << +cFinalDelayOG << RESET;
            if(cOpticalGroup->getId() < 10)
                LOG(INFO) << BOLDGREEN << "Optimal package delay all OG until OG#" << cOpticalGroup->getId() << " is: " << std::bitset<32>(cFinalDelayOGs_link0_link9) << RESET;
            else
                LOG(INFO) << BOLDGREEN << "Optimal package delay all OG until OG#" << cOpticalGroup->getId() << " is: " << std::bitset<32>(cFinalDelayOGs_link10_link11) << RESET;

        } // OG
    }
    // set everything back to original values .. except for the trigger source
    // like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cOriginalTPdelay});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", cOriginalResetEn});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // reconfigure sparsification + FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    size_t cIndx = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
            fCicInterface->WriteChipReg(cCic, "FE_ENABLE", cFeEnableRegs[cIndx]);
            cIndx++;
        }
    }
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.global.hybrid_enable", cEnableMask);
    // and check
    // make sure you do this with internal triggers
    ReadNEvents(pBoard, 10);
    const std::vector<Event*>& cEvents     = this->GetEvents();
    int                        cEventCount = 0;
    for(auto& cEvent: cEvents)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cBx = (int)cEvent->BxId(cHybrid->getId());
                LOG(INFO) << BOLDGREEN << "Event#" << cEventCount << " Link#" << +cOpticalGroup->getId() << " Hybrid#" << +cHybrid->getId() << " BxId " << cBx << RESET;
            }
        }
        cEventCount++;
    }
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc);
    LOG(INFO) << BOLDMAGENTA << "Found package delay for OG#0 to OG#9 to be " << +cFinalDelayOGs_link0_link9 << " binary " << std::bitset<32>(cFinalDelayOGs_link0_link9) << RESET;
    LOG(INFO) << BOLDMAGENTA << "Found package delay for OG#10 to OG#11 to be " << +cFinalDelayOGs_link10_link11 << " binary " << std::bitset<32>(cFinalDelayOGs_link10_link11) << RESET;

    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "LinkAlignmentOT::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cOriginalTPdelay});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", cOriginalResetEn});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
}