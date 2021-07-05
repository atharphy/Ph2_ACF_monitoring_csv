#include "BackEndAlignment.h"

#include "../HWInterface/BackendAlignmentInterface.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "boost/format.hpp"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

BackEndAlignment::BackEndAlignment() : Tool() { fRegMapContainer.reset(); }

BackEndAlignment::~BackEndAlignment() {}
void BackEndAlignment::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by BackEndAlignment" << RESET;
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
            if(cReg.first.find("stub_package_delay") != std::string::npos) continue;
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        for(auto cOpticalGroup: *cBoard)
        {
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                bool cIsPS    = (cWithSSA && cWithMPA) && cWithLpGBT;
                cWithPS       = cWithPS || cIsPS;
                LOG(INFO) << BOLDBLUE << "BackEndAlignment::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(INFO) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(DEBUG) << BOLDBLUE << "BackEndAlignment::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                    }
                }
            }
        }
    }
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        if(cWithPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }
    resetPointers();
}
void BackEndAlignment::Initialise()
{
    fSuccess = false;
    // this is needed if you're going to use groups anywhere
    fChannelGroupHandler = new CBCChannelGroupHandler(); // This will be erased in tool.resetPointers()
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    ContainerFactory::copyAndInitHybrid<uint8_t>(*fDetectorContainer, fEnabledFEs);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cEnabledFEs = fEnabledFEs.at(cBoard->getIndex());
        //
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cEnabledFEsOG = cEnabledFEs->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cEnabledFEsHybrid = cEnabledFEsOG->at(cHybrid->getIndex());
                auto& cEnabled          = cEnabledFEsHybrid->getSummary<uint8_t>();
                cEnabled                = 0;
                for(auto cChip: *cHybrid)
                {
                    ChipRegMap&       theChipMap     = fRegMapContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    const ChipRegMap& theOriginalMap = static_cast<ReadoutChip*>(cChip)->getRegMap();
                    theChipMap.insert(theOriginalMap.begin(), theOriginalMap.end());
                    if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::CBC3) cEnabled = cEnabled | (1 << cChip->getId());
                }
            }
        }
    }

    // read back original masks
    ContainerFactory::copyAndInitChip<const ChannelGroup<NCHANNELS>*>(*fDetectorContainer, fChipMasks);
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cMasksThisBrd = fChipMasks.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>() = static_cast<const ChannelGroup<NCHANNELS>*>(cChip->getChipOriginalMask());
                    }
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                    {
                        cMasksThisChip->getSummary<const ChannelGroup<NSSACHANNELS>*>() = static_cast<const ChannelGroup<NSSACHANNELS>*>(cChip->getChipOriginalMask());
                    }
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        cMasksThisChip->getSummary<const ChannelGroup<NSSACHANNELS, NMPACOLS>*>() = static_cast<const ChannelGroup<NSSACHANNELS, NMPACOLS>*>(cChip->getChipOriginalMask());
                    }
                }
            } // hybrids
        }     // OG
    }

    // clear map of modified registers
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        bool cIsPS = false;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cIsPS) continue;

                    auto cType    = FrontEndType::SSA;
                    bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cType         = FrontEndType::MPA;
                    bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cIsPS         = (cWithSSA && cWithMPA) && cWithLpGBT;
                }
            }
        }
        if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }
}
void BackEndAlignment::Reconfigure(BeBoard* pBoard)
{
    // reconfigure ROC registers
    // only those that I've touched
    LOG(INFO) << BOLDMAGENTA << "\t... [BackEndAlignment] Resetting ROC regs back to their original values" << RESET;
    auto& cRegMapThisBoard = fRegMapContainer.at(pBoard->getIndex());
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                const ChipRegMap&                             cCurrentMap     = static_cast<ReadoutChip*>(cChip)->getRegMap();
                auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                cVecRegisters.clear();
                // LOG (INFO) << BOLDMAGENTA << "\t.. CBC#" << +cChip->getId() << RESET;
                for(auto cReg: cRegMapThisChip)
                {
                    auto cIter = cCurrentMap.find(cReg.first);
                    if(cIter->second.fValue != cReg.second.fValue)
                    {
                        // mask registers I'll do separately later
                        if(cIter->first.find("MaskChannel") == std::string::npos)
                        {
                            // LOG (INFO) << BOLDMAGENTA << "\t\t\t.. Register " << cReg.first
                            //     << " was changed .. from 0x"
                            //     << std::hex << +cIter->second.fValue << std::dec
                            //     << " to "
                            //     << std::hex << +cReg.second.fValue << std::dec
                            //     << " re-writing"
                            //     << RESET;
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                }
                // for now only reconfigure CBCs
                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                if(cVecRegisters.size() == 0) continue;
                fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
            }
        }
    }

    // // reconfigure masks
    auto& cMasksThisBrd = fChipMasks.at(pBoard->getIndex());
    LOG(INFO) << BOLDMAGENTA << "\t... [BackEndAlignment] Resetting ROC masks back to their original values" << RESET;
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cMasksThisOG = cMasksThisBrd->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cMasksThisHybrid = cMasksThisOG->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                auto& cMasksThisChip = cMasksThisHybrid->at(cChip->getIndex());
                auto& cOriginalMask  = cMasksThisChip->getSummary<const ChannelGroup<NCHANNELS>*>();
                fReadoutChipInterface->maskChannelsGroup(cChip, cOriginalMask);
            }
        } // hybrids
    }     // OG

    // last thing to do
    // is to reconfigure registers on page 1
    // of the CBCs
    LOG(INFO) << BOLDMAGENTA << "\t... [BackEndAlignment] Resetting all registers on Page1 of CBCs" << RESET;
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;

                auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                cVecRegisters.clear();
                for(auto cReg: cRegMapThisChip)
                {
                    // mask registers I'll do separately later
                    if(cReg.second.fPage == 1) { cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue)); }
                }
                // for now only reconfigure CBCs
                fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
            }
        }
    }
}
bool BackEndAlignment::FindPackageDelay(BeBoard* pBoard)
{
    LOG(INFO) << GREEN << "Trying CIC un-packer alignment in the back-end" << RESET;
    uint32_t cNevents      = 10;
    uint16_t cMaxBxCounter = 3564;

    // sparsification of
    bool cSparsified = pBoard->getSparsification();
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Sparsification off " << RESET;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // disable all FEs. . not needed here 
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }

    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
    cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // now try and find correct package delay
    auto cOriginalDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
    LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
    bool    cCorrectDelay = false;
    uint8_t cPackageDelay = 7;
    uint8_t cFinalDelay   = cPackageDelay;

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic == NULL) continue;
            // I don't actually need any FEs enabled to do this step
            // if I do this I am sure they are all off
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false); // make sure all FEs are disabled by default
        }
    }
    for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
    {
        if(cCorrectDelay) continue;

        LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << RESET;
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();

        // // check stubs
        // // 2 events should be enough
        // // LOG(DEBUG) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
        ReadNEvents(pBoard, cNevents);
        const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
        LOG(INFO) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;

        // now ... check for incrementing BxIds
        int              cNRollOvers = 0;
        std::vector<int> cBxIds(0);
        std::vector<int> cBxDifferences(0); // I think by injecting this way this number should always be the same ..
        for(auto& cEvent: cEventsWithStubs)
        {
            for(auto cOpticalGroup: *pBoard)
            {
                // only checked for first link
                if(cOpticalGroup->getIndex() > 0) continue;

                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getIndex() > 0) continue;

                    auto cBx = (int)cEvent->BxId(cHybrid->getId());
                    if(cBxIds.size() > 0)
                    {
                        int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxIds[cBxIds.size() - 1] % cMaxBxCounter);
                        cNRollOvers += ((cBxIds[cBxIds.size() - 1] >= 2500) && (cBxIds[cBxIds.size() - 1] < cMaxBxCounter)) && (cBx < cBxIds[cBxIds.size() - 1]) ? 1 : 0;
                        cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBx % cMaxBxCounter) - cBxDifference;
                        cBxDifferences.push_back(cBxDifference);
                        //LOG(INFO) << BOLDBLUE << "\t.....BxDifference is " << +cBxDifference << RESET;
                    }
                    cBxIds.push_back(cBx);
                    //LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;

                } // hybrids or CICs
            }     // modules or optical links
        }         // events
        // figure out the differences between the bxIds
        auto cFirstDifference = cBxDifferences[0];
        std::adjacent_difference(cBxDifferences.begin(), cBxDifferences.end(), cBxDifferences.begin());
        cBxDifferences.erase(cBxDifferences.begin()); // erase the first element
        for(auto cDifference: cBxDifferences) LOG(DEBUG) << BOLDBLUE << "\t..." << +cDifference << RESET;
        // all elements are equal
        if(cFirstDifference != 0 && std::equal(cBxDifferences.begin() + 1, cBxDifferences.end(), cBxDifferences.begin()))
        {
            LOG(INFO) << BOLDGREEN << "Found differences between bxIds to always be the same : " << +cFirstDifference << RESET;
            LOG(INFO) << BOLDGREEN << "Going to fix the manual package delay to " << +cPackageDelay << RESET;
            cFinalDelay   = cPackageDelay;
            cCorrectDelay = true;
        }
        else
            LOG(INFO) << BOLDRED << "Found differences between bxIds to be different from one another." << RESET;

    } // pkg delay
    if(!cCorrectDelay) return cCorrectDelay;

    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);

    // reconfigure sparsification + FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
        }
    }// OG

    LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;
    // now check for all hybrids 
    ReadNEvents(pBoard, cNevents);
    const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
    for(auto& cEvent: cEventsWithStubs)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            std::vector<uint16_t> cBxIds(0);
            for(auto cHybrid: *cOpticalGroup)
            {
                if(cHybrid->getIndex() > 0) continue;

                cBxIds.push_back( (int)cEvent->BxId(cHybrid->getId()) );
                LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBxIds.at(cBxIds.size()-1) << RESET;

            } // hybrids or CICs
        }// modules or optical links
    }

    //re-enable FEs
    auto& cEnabledFEs = fEnabledFEs.at(pBoard->getIndex());
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cEnabledFEsOG = cEnabledFEs->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            auto&                cEnabledFEsCIC = cEnabledFEsOG->at(cHybrid->getIndex());
            auto                 cEnableMsk     = cEnabledFEsCIC->getSummary<uint8_t>();
            std::vector<uint8_t> cIds(0);
            for(size_t cIndx = 0; cIndx < 8; cIndx++)
            {
                if(((cEnableMsk & (0x1 << cIndx)) >> cIndx) == 1) cIds.push_back(cIndx);
            }
            fCicInterface->EnableFEs(cCic, cIds, true); // make sure all FEs are disabled by default
        }                                               // hybrids
    }
    
    // LOG(INFO) << BOLDMAGENTA << "[BackEndAlignment::FindPackageDelay] Reconfigure ROCs on BeBoard#" << +pBoard->getId() << RESET;
    // Reconfigure(pBoard);
    return cCorrectDelay;
}
bool BackEndAlignment::FindStubLatency(BeBoard* pBoard)
{
    uint32_t cNevents = 10;
    auto     cSetting      = fSettingsMap.find("StubAlignmentThreshold");
    uint32_t cThreshold      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 530;
    cSetting      = fSettingsMap.find("StubAlignmentScanStart");
    uint32_t cScanStart      = (cSetting != std::end(fSettingsMap)) ? cSetting->second : 100;
    
    // sparsification of
    bool cSparsified = pBoard->getSparsification();
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindStubLatency Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindStubLatency Sparsification off " << RESET;

    LOG(INFO) << GREEN << "Trying to find stub latency finding in the back-end" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);

    // read back original masks
    bool cWithPS=false;
    for(auto cOpticalGroup: *pBoard)
    {
        cWithPS = cWithPS || (cOpticalGroup->getFrontEndType()==FrontEndType::OuterTrackerPS);
    }

    // reconfigure fast commands
    // fast command config
    // if PS module want trigger multiplicty to be 3 
    uint8_t                  cMult            = (cWithPS) ? 3 : 0;
    uint8_t                  cTriggerSource   = 6;
    uint16_t                 cDelayAfterReset = 100;
    uint16_t                 cDelayAfterTP    = 300;
    uint16_t                 cDelayTillNext   = 400;
    std::vector<std::string> cFcmdRegs{"trigger_source", "test_pulse.delay_after_fast_reset", "test_pulse.delay_after_test_pulse", "test_pulse.delay_before_next_pulse", "misc.trigger_multiplicity"};
    std::vector<uint16_t>    cFcmdRegVals{cTriggerSource, cDelayAfterReset, cDelayAfterTP, cDelayTillNext, cMult};
    std::vector<uint16_t>    cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled" , 0 );


    // expected spacing between TP and L1A
    auto    cDelay          = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
    size_t  cNinjectedHits  = 0;
    size_t  cNinjectedStubs = 0;
    int     cReTime         = 0;
    uint8_t cPSrow          =10; 
    uint16_t cPSmode        =3; 
    uint8_t                cStubWindow = 1; // stub window in half pixels (1)
    uint8_t                cMode       = cPSmode; // (0) pixel-strip, (1) strip-strip, (2) pixel-pixel, (3) strip-pixel
    std::vector<uint32_t>  cPixelIds(0); // these will be used to generate stubs
    std::vector<uint8_t>   cRows{cPSrow};
    uint8_t cMaxStubs      = 4; 
    //int    cChipId         = 7;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // for 2S - do with ananlogue injection into CBC
            size_t cNinjectedStubsThisHybrid = 0;
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                bool cWithNoise = false;
                // inject stubs with TP
                uint8_t cSeed = 60;//2 + (uint8_t)(cChip->getId()*2); 
                std::vector<uint8_t> cSeeds{cSeed};
                std::vector<int>     cBends{0};
                // make sure we are within the limits of the CIC
                // only inject 3 stubs here
                if(cNinjectedStubsThisHybrid > cMaxStubs)
                {
                    cSeeds.clear();
                    cBends.clear();
                }

                size_t cNhits = 0;
                for(size_t cIndx = 0; cIndx < cSeeds.size(); cIndx += 1)
                {
                    auto cHitList = (static_cast<CbcInterface*>(fReadoutChipInterface))->stubInjectionPattern(cChip, cSeeds[cIndx], cBends[cIndx]);
                    cNinjectedHits += cHitList.size();
                    cNhits += cHitList.size();
                }

                cNinjectedStubs += cSeeds.size();
                cNinjectedStubsThisHybrid += cSeeds.size();
                (static_cast<CbcInterface*>(fReadoutChipInterface))->injectStubs(cChip, cSeeds, cBends, cWithNoise);
                fReadoutChipInterface->WriteChipReg(cChip,"Threshold",cThreshold);
                // // enable stub logic
                // // make sure OR mode is used
                // static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(cChip, "OR", true, true);
                // // set PtCut to maximum
                // fReadoutChipInterface->WriteChipReg(cChip, "PtCut", 14);
                // // no cluster cut
                // fReadoutChipInterface->WriteChipReg(cChip, "ClusterCut", 4);
                LOG(INFO) << BOLDMAGENTA << "Injecting " << +cNhits << " hits and " << +cSeeds.size() << " stubs in CBC#" << +cChip->getId() << " on hybrid#" << +cHybrid->getId() << RESET;
            } // 2S chips  - CBCs

            // for PS - first digital injection in MPAs
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                std::vector<Injection> cInjections(0);
                for(size_t cIndx = 0; cIndx < cRows.size(); cIndx++)
                {
                    Injection cInjection;
                    cInjection.fColumn = 2 * (int)cChip->getId() + 1;
                    cInjection.fRow    = cRows[cIndx];
                    cInjections.push_back(cInjection);
                    uint32_t cPixelId = (uint32_t)(cInjection.fColumn) * 120 + (uint32_t)cInjection.fRow;
                    cPixelIds.push_back(cPixelId);
                } // create injection patterns
                cNinjectedHits += cRows.size();
                cNinjectedStubs += cInjections.size();
                // activate stub mode
                fReadoutChipInterface->WriteChipReg(cChip, "StubMode", cMode);
                fReadoutChipInterface->WriteChipReg(cChip, "StubWindow", cStubWindow);
                cReTime = fReadoutChipInterface->ReadChipReg(cChip, "RetimePix");
                (static_cast<PSInterface*>(fReadoutChipInterface))->digiInjection(cChip, cInjections);
            } // PS chips  - MPAs

            if( cPSmode == 2 ) continue;
            // for PS - digital injection in SSAs
            for(auto cChip: *cHybrid) // for each chip (makes sense)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA) continue;

                // uint8_t cPattern = cDistributeInj ? (1 << (7 - cChip->getId())) : (0x1 << 0);
                fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_ALL", 0x0);
                fReadoutChipInterface->WriteChipReg(cChip, "DigCalibPattern_L_ALL", (0x1 << 0));
                fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x01);
                for(auto cRow: cRows)
                {
                    LOG(DEBUG) << BOLDMAGENTA << "\t...Injecting in"
                               << " strip#" << +cRow << " in SSA#" << +cChip->getId() << RESET;
                    fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS_S" + std::to_string(cRow), 0x9);
                }
                cNinjectedHits += cRows.size();
            } // PS chips  - MPAs

        }// Hybrid
    }// OG

    // find correct hit latency
    bool     cFoundCorrectHitLatency = false;
    uint16_t cHitLatency             = 0;
    int      cExpectedOffset         = -1;
    float    cFraction = (cWithPS) ? 0.5*(1.0/(1+cMult)) : 0.5 ; 
    for(int cOffset = cExpectedOffset ; cOffset < cExpectedOffset + 10; cOffset++)
    {
        if(cFoundCorrectHitLatency) continue;

        int cLatency = cDelay + cOffset;
        if(cLatency < 0) continue;

        LOG(INFO) << BOLDGREEN << "Hit Latency of " << +cLatency << RESET;

        for(auto cOpticalReadout: *pBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) // for each chip (makes sense)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA) fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", (uint16_t)cLatency-1);
                    else  fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", (uint16_t)cLatency);
                } // ROC - only MPAs and CBCs for this test since I'm eihter in p=p mode or 2S
            }     // hybrid
        }         // OG
        
        ReadNEvents(pBoard, cNevents);
        const std::vector<Event*>& cEvents         = this->GetEvents();
        size_t                     cNEventsMatched = 0;
        for(auto cEvent: cEvents)
        {
            size_t cNHits          = 0;
            bool   cCorrectLatency = true;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    // only for the first hybrid
                    // if(cHybrid->getIndex() > 0) continue;
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                        size_t cNHitsThisFE = cEvent->GetHits(cHybrid->getId(), cChip->getId()).size();
                        //LOG (INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << " ... found ..." << +cNHitsThisFE << " clusters in MPA" << +cChip->getId() << RESET;
                        cNHits += cNHitsThisFE;
                    } // ROCs
                }     // hybrids
            }         // OGs
            cCorrectLatency = cCorrectLatency && (cNHits == cNinjectedHits);
            cNEventsMatched += (cCorrectLatency) ? 1 : 0;
        } // event loop

        // check match
        // won't ask for a 100 percent here as I'm not s
        if(cNEventsMatched > cFraction * cEvents.size())
        {
            cHitLatency             = cLatency;
            cFoundCorrectHitLatency = true;
            LOG(INFO) << BOLDGREEN << "For a latency of " << +cLatency << " found " << +cNEventsMatched << " out of " << +cEvents.size() << " events with the correct number of hits for all FEs."
                      << RESET;
        }
        else
            LOG(INFO) << BOLDRED << "For a latency of " << +cLatency << " found " << +cNEventsMatched << " out of " << +cEvents.size() << " events with the correct number of hits for all FEs."
                      << RESET;
    }

    bool cFoundCorrectStubLatency = cFoundCorrectHitLatency;
    int  cCorrectOffset           = 0;
    if(cFoundCorrectStubLatency)
    {
        cFoundCorrectStubLatency = false;
        LOG(INFO) << BOLDGREEN << "Searching for correct stub  latency .. hit latency set to " << +cHitLatency << RESET;
        // // now scan stub latency
        auto cOriginalStubDelay = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
        LOG(INFO) << BOLDMAGENTA << "Original stub delay set to " << +cOriginalStubDelay << RESET;
        for(int cOffset = cScanStart; cOffset >= 20; cOffset--)
        {
            if(cFoundCorrectStubLatency) continue;
            int cStubLatency = cHitLatency - cOffset;
            LOG(INFO) << BOLDGREEN << "Stub Latency of " << +cStubLatency << RESET;

            fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
            ReadNEvents(pBoard, cNevents);
            const std::vector<Event*>& cEvents      = this->GetEvents();
            size_t                     cNStubsFound = 0;
            for(auto cEvent: cEvents)
            {
                for(auto cOpticalGroup: *pBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        size_t cNstubsThisHybrd = 0;
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;
                            if( cEvent->GetHits(cHybrid->getId(), cChip->getId()).size() == 0 ) continue; 
                            
                            auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                            cNstubsThisHybrd += cStubs.size();
                            cNStubsFound += cStubs.size();
                        } // ROCs
                        // LOG(INFO) << BOLDMAGENTA << "Event#" << +cEvent->GetEventCount() << " found " << +cNstubsThisHybrd 
                        //     << " stubs and " << +cHitsThisHybrid
                        //     << " in CIC#" << +cHybrid->getId() 
                        //     << RESET;
                    } // hybrids
                }     // OGs
            }         // events
            cFoundCorrectStubLatency = (cNStubsFound > cFraction * cNinjectedStubs * cEvents.size());
            if(cFoundCorrectStubLatency)
            {
                cCorrectOffset = cOffset;
                LOG(INFO) << BOLDGREEN << "For a stub latency of " << +cStubLatency << " found " << +cNStubsFound << " stubs out of " << +cNinjectedStubs * cEvents.size() << " expected." << RESET;
            }
        } // offset scan
    }

    // adding this here in preparation for stub decoding
    // I think this should belong to the board.. need to fix
    if(cFoundCorrectStubLatency) { 
        pBoard->setStubOffset(cCorrectOffset - cReTime);
        // TO-DO .. remove this
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->SetStubOffset(cCorrectOffset - cReTime); 
        // verification step 
        // print to screen for now 
        bool cConfirm=false;
        if( cConfirm )
        {
            ReadNEvents(pBoard, cNevents);
            const std::vector<Event*>& cEvents      = this->GetEvents();
            for(auto cEvent: cEvents)
            {
                for(auto cOpticalGroup: *pBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA) continue;

                            auto cStubs = cEvent->StubVector(cHybrid->getId(), cChip->getId());
                            auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId()); 
                            if( (int)(cHits.size()) > 0 && (int)cStubs.size() > 0 )
                            {
                                LOG (INFO) << BOLDGREEN << "Event#" << +cEvent->GetEventCount() << " ... found "
                                    << cHits.size() << " hits in FE#" << +cChip->getId() 
                                    << " and " 
                                    << +cStubs.size() 
                                    << " stubs."
                                    << RESET;
                            }
                        } // ROCs
                    } // hybrids
                }// OGs
            }// events
        }
    }

    // reconfigure sparsification
    // this->enableTestPulse(false);
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindStubLatency Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SetSparsification(cCic, cSparsified);
        } // hybrids
    }     // OG
    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindStubLatency Resetting BeBoards regs back to their original values" << RESET;
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cRegVec.push_back({cRegName, cFcmdRegOrigVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled" , cOriginalTLUconfig );
    return cFoundCorrectStubLatency;
}
bool BackEndAlignment::PSAlignment(BeBoard* pBoard)
{
    bool cTuned = true;
    LOG(INFO) << GREEN << "Trying Phase Tuning for PS Chip(s)" << RESET;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid)
            {
                ReadoutChip* cReadoutChip = static_cast<ReadoutChip*>(cChip);

                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    LOG(INFO) << GREEN << "MPA Alignment" << RESET;
                    std::vector<std::string> cRegNames{"ReadoutMode", "ECM", "LFSR_data"};
                    std::vector<uint8_t>     cOriginalValues;
                    uint8_t                  cAlignmentPattern = 0xa0;
                    std::vector<uint8_t>     cRegValues{0x0, 0x08, cAlignmentPattern};

                    for(size_t cIndex = 0; cIndex < 3; cIndex++)
                    {
                        cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                    }

                    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
                    {
                        cTuned = cTuned &&
                                 static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cChip->getIndex(), cLineId, cAlignmentPattern, 8);
                    }

                    for(size_t cIndex = 0; cIndex < 3; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); };
                }
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    LOG(INFO) << GREEN << "SSA Alignment" << RESET;
                    ReadoutChip*             cReadoutChip = static_cast<ReadoutChip*>(cChip);
                    std::vector<std::string> cRegNames{"SLVS_pad_current", "ReadoutMode"};
                    std::vector<uint8_t>     cOriginalValues;
                    std::vector<uint8_t>     cRegValues{0x7, 2};
                    for(size_t cIndex = 0; cIndex < 2; cIndex++)
                    {
                        cOriginalValues.push_back(fReadoutChipInterface->ReadChipReg(cReadoutChip, cRegNames[cIndex]));
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cRegValues[cIndex]);
                    }

                    uint8_t cAlignmentPattern = 0x80;

                    for(uint8_t cLineId = 0; cLineId < 8; cLineId++)
                    {
                        // char cBuffer[11];
                        // sprintf(cBuffer, "OutPattern%d", cLineId);
                        // std::string cRegName = (cLineId == 7) ? "OutPattern7/FIFOconfig" : std::string(cBuffer, sizeof(cBuffer));
                        std::string cRegName = (cLineId == 7) ? "OutPattern7/FIFOconfig" : "OutPattern" + (boost::format("%|01|") % cLineId).str();
                        fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegName, cAlignmentPattern);
                        cTuned = cTuned &&
                                 static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cChip->getIndex(), cLineId, cAlignmentPattern, 8);
                    }

                    for(size_t cIndex = 0; cIndex < 2; cIndex++) { fReadoutChipInterface->WriteChipReg(cReadoutChip, cRegNames[cIndex], cOriginalValues[cIndex]); }
                }
            }
        }
    }

    LOG(INFO) << GREEN << "PS Phase tuning finished succesfully" << RESET;
    return cTuned;
}

bool BackEndAlignment::CICAlignment(BeBoard* pBoard)
{
    // make sure you're only sending one trigger at a time here
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0);

    // force CIC to output repeating 101010 pattern on L1 line
    // needed for phase alignment in back-end
    // force CIC to output empty L1A frames [by disabling all FEs]
    // needed for word alingment in the back-end
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // disable alignment output
            fCicInterface->SelectOutput(cCic, false);
            //
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }
    bool cAligned = true;
    fL1Debug      = true;
    if(!pBoard->isOptical()) cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1PhaseTuning(pBoard, fL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A phase alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }

    // fL1Debug = false;
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1WordAlignment(pBoard, fL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A word alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }
    
    // enable CIC output of alignmnent pattern on stub lines
    // .. and enable all FEs again
    bool cIsPS = false;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // enable alignment output for stubs
            fCicInterface->SelectOutput(cCic, true);
            // check for an MPA
            auto cType     = FrontEndType::MPA;
            auto cMPAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
            cIsPS          = cIsPS || cMPAfound;
            // check for an SSA
            cType          = FrontEndType::SSA;
            auto cSSAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
            cIsPS          = cIsPS || cSSAfound;
        }
    }
    fStubDebug     = true;
    size_t cNlines = cIsPS ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::CICAlignment ... stub alignment on " << +cNlines << "/6 lines from CIC.." << RESET;
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubTuning(pBoard, fStubDebug, cNlines);
    LOG(INFO) << BOLDMAGENTA << "Now looking at output of all hybrids " << RESET;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << RESET;
            fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
            (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, cNlines);
        } // hybrids
    }     // OG

    // disable CIC output of pattern on stub + l1 lines
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, false);
        } // hybrids [CICs]
    }     // optical groups [modules]

    // reconfigure FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::CICAlignment Resetting FE_ENABLE" << RESET;
    auto& cEnabledFEs = fEnabledFEs.at(pBoard->getIndex());
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cEnabledFEsOG = cEnabledFEs->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto&                cCic           = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            auto&                cEnabledFEsCIC = cEnabledFEsOG->at(cHybrid->getIndex());
            auto                 cEnableMsk     = cEnabledFEsCIC->getSummary<uint8_t>();
            std::vector<uint8_t> cIds(0);
            for(size_t cIndx = 0; cIndx < 8; cIndx++)
            {
                if(((cEnableMsk & (0x1 << cIndx)) >> cIndx) == 1) cIds.push_back(cIndx);
            }
            fCicInterface->EnableFEs(cCic, cIds, true); // make sure all FEs are disabled by default
        }                                               // hybrids
    }                                                   // OG

    // re-load configuration of fast command block from register map loaded from xml file
    LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(pBoard);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMultiplicity);
    return cAligned;
}

bool BackEndAlignment::CBCAlignment(BeBoard* pBoard)
{
    bool cAligned = false;

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cReadoutChip: *cHybrid)
            {
                ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                // fBeBoardInterface->WriteBoardReg (pBoard,
                // "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cReadoutChip->getId() );
                // original mask
                const ChannelGroup<NCHANNELS>* cOriginalMask = static_cast<const ChannelGroup<NCHANNELS>*>(cReadoutChip->getChipOriginalMask());
                // original threshold
                uint16_t cThreshold = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "VCth");
                // original HIT OR setting
                uint16_t cHitOR = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "HitOr");

                // make sure hit OR is turned off
                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theReadoutChip, "HitOr", 0);
                // make sure pT cut is set to maximum
                // make sure hit OR is turned off
                auto cPtCut = static_cast<CbcInterface*>(fReadoutChipInterface)->ReadChipReg(theReadoutChip, "PtCut");
                static_cast<CbcInterface*>(fReadoutChipInterface)->WriteChipReg(theReadoutChip, "PtCut", 14);

                LOG(INFO) << BOLDBLUE << "Running phase tuning and word alignment on FE" << +cHybrid->getId() << " CBC" << +cReadoutChip->getId() << "..." << RESET;
                uint8_t              cBendCode_phAlign = 2;
                std::vector<uint8_t> cBendLUT          = static_cast<CbcInterface*>(fReadoutChipInterface)->readLUT(theReadoutChip);
                auto                 cIterator         = std::find(cBendLUT.begin(), cBendLUT.end(), cBendCode_phAlign);
                // if bend code isn't there ... quit
                if(cIterator == cBendLUT.end()) continue;

                int    cPosition    = std::distance(cBendLUT.begin(), cIterator);
                double cBend_strips = -7. + 0.5 * cPosition;
                // LOG(DEBUG) << BOLDBLUE << "Bend code of " << +cBendCode_phAlign << " found in register " << cPosition << " so a bend of " << cBend_strips << RESET;

                uint8_t              cSuccess = 0x00;
                std::vector<uint8_t> cSeeds{0x82, 0x8E, 0x9E};
                std::vector<int>     cBends(cSeeds.size(), static_cast<int>(cBend_strips * 2));
                static_cast<CbcInterface*>(fReadoutChipInterface)->injectStubs(theReadoutChip, cSeeds, cBends);
                // first align lines with stub seeds
                uint8_t cLineId = 1;
                for(size_t cIndex = 0; cIndex < 3; cIndex++)
                {
                    cSuccess = cSuccess |
                               (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cSeeds[cIndex], 8)
                                << cIndex);
                    cLineId++;
                }
                // then align lines with stub bends
                uint8_t cAlignmentPattern = (cBendCode_phAlign << 4) | cBendCode_phAlign;
                cSuccess                  = cSuccess |
                           (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8)
                            << (cLineId - 1));
                cLineId++;
                // finally sync bit + last bend
                cAlignmentPattern = (1 << 7) | cBendCode_phAlign;
                bool cTuned =
                    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8);
                if(!cTuned)
                {
                    LOG(INFO) << BOLDMAGENTA << "Checking if error bit is set ..." << RESET;
                    // check if error bit is set
                    cAlignmentPattern = (1 << 7) | (1 << 6) | cBendCode_phAlign;
                    cSuccess =
                        cSuccess |
                        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, cHybrid->getIndex(), cReadoutChip->getIndex(), cLineId, cAlignmentPattern, 8)
                         << (cLineId - 1));
                }
                else
                    cSuccess = cSuccess | (static_cast<uint8_t>(cTuned) << (cLineId - 1));

                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>(cSeeds[0]) << ", " << std::bitset<8>(cSeeds[1]) << ", " << std::bitset<8>(cSeeds[2]) << " on stub lines  0, 1 and 2."
                          << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((cBendCode_phAlign << 4) | cBendCode_phAlign) << " on stub line  4." << RESET;
                LOG(INFO) << BOLDMAGENTA << "Expect pattern : " << std::bitset<8>((1 << 7) | cBendCode_phAlign) << " on stub line  5." << RESET;
                LOG(INFO) << BOLDMAGENTA << "After alignment of last stub line ... stub lines 0-5: " << RESET;
                (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, 5);

                // now unmask all channels and set threshold and hit or logic back to their original values
                fReadoutChipInterface->maskChannelsGroup(theReadoutChip, cOriginalMask);
                LOG(INFO) << BOLDBLUE << "Setting threshold and HitOR back to orginal value [ " << +cThreshold << " ] DAC units." << RESET;
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "VCth", cThreshold);
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "HitOr", cHitOR);
                fReadoutChipInterface->WriteChipReg(theReadoutChip, "PtCut", cPtCut);
            }
        }
    }

    return cAligned;
}
bool BackEndAlignment::Align()
{
    LOG(INFO) << BOLDBLUE << "Starting back-end alignment procedure .... " << RESET;
    bool cAligned = true;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        // read back register map before you've done anything
        auto cBoardRegisterMap = theBoard->getBeBoardRegMap();
        bool cWithCIC          = false;
        bool cWithCBC          = false;
        bool cWithSSA          = false;
        bool cWithMPA          = false;
        for(auto cOpticalReadout: *cBoard)
        {
            if(cOpticalReadout->getIndex() > 0) break;
            for(auto cHybrid: *cOpticalReadout)
            {
                if(cHybrid->getIndex() > 0) break;
                cWithCIC = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic != NULL;
                for(auto cReadoutChip: *cHybrid)
                {
                    cWithCBC = cWithCBC || cReadoutChip->getFrontEndType() == FrontEndType::CBC3;
                    cWithSSA = cWithSSA || cReadoutChip->getFrontEndType() == FrontEndType::SSA;
                    cWithMPA = cWithMPA || cReadoutChip->getFrontEndType() == FrontEndType::MPA;
                } // ROcs
            }     // Hybrids
        }         // OGs
        if(cWithCIC)
        {
            cAligned = this->FindStubLatency(theBoard);
        }
        else
        {
            if(cWithCBC) { this->CBCAlignment(theBoard); }
            else if(cWithMPA or cWithSSA)
            {
                this->PSAlignment(theBoard);
            }
            else
            {
            }
        }
        // re-load configuration of fast command block from register map loaded from xml file
        LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock(theBoard);
    }
    return cAligned;
}
void BackEndAlignment::writeObjects() {}
// State machine control functions
void BackEndAlignment::Running()
{
    Initialise();
    fSuccess = this->Align();
    if(!fSuccess)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
// gui::message("Backend alignment failed"); //How
#ifdef __USE_ROOT__
        SaveResults();
        WriteRootFile();
        CloseResultFile();
#endif
        Destroy();
        exit(FAILED_BACKEND_ALIGNMENT);
    }
    Reset();
}

void BackEndAlignment::Stop()
{
    dumpConfigFiles();
    // Destroy();
}

void BackEndAlignment::Pause() {}

void BackEndAlignment::Resume() {}
