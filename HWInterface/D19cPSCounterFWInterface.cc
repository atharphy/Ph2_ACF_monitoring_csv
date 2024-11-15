#include "HWInterface/D19cPSCounterFWInterface.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Chip.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWDescription/ChipRegItem.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/OpticalGroup.h"
#include "HWInterface/FEConfigurationInterface.h"
#include "HWInterface/FastCommandInterface.h"
#include "HWInterface/RegManager.h"
#include "HWInterface/TriggerInterface.h"
#include "Utils/ConsoleColor.h"
#include "Utils/ContainerFactory.h"
#include "Utils/D19cPSEventAS.h"
#include "Utils/DataContainer.h"
#include "Utils/easylogging++.h"
#include <thread>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cPSCounterFWInterface::D19cPSCounterFWInterface(RegManager* theRegManager) : L1ReadoutInterface(theRegManager) {}

D19cPSCounterFWInterface::~D19cPSCounterFWInterface() {}


void D19cPSCounterFWInterface::configureFastReadout(bool enableFastReadout)
{
    fPSCounterFast = enableFastReadout; 
    D19cPSEventAS::configureFastReadout(fPSCounterFast);
}

std::pair<ChipRegItem, ChipRegItem> D19cPSCounterFWInterface::getChannelCounterRegister(uint16_t row, uint16_t col, bool isMPA)
{
    uint32_t cBaseRegisterLSB, cBaseRegisterMSB;
    cBaseRegisterLSB = 0;
    cBaseRegisterMSB = 0;
    if(isMPA)
    {
        cBaseRegisterLSB = (((row + 1) << 11) | (4 << 7) | (col + 1));
        cBaseRegisterMSB = (((row + 1) << 11) | (5 << 7) | (col + 1));
    }
    else
    {
        cBaseRegisterLSB = 0x0580 + col;
        cBaseRegisterMSB = 0x0680 + col;
    }

    // MSB
    ChipRegItem cReg_Counters_MSB;
    cReg_Counters_MSB.fPage    = 0x00;
    cReg_Counters_MSB.fAddress = cBaseRegisterMSB;
    cReg_Counters_MSB.fValue   = 0x00;
    // LSB
    ChipRegItem cReg_Counters_LSB;
    cReg_Counters_LSB.fPage    = 0x00;
    cReg_Counters_LSB.fAddress = cBaseRegisterLSB;
    cReg_Counters_LSB.fValue   = 0x00;

    return {cReg_Counters_MSB, cReg_Counters_LSB};
}

// method to read counter from register
void D19cPSCounterFWInterface::SlowRead(const BeBoard* pBoard)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                std::vector<ChipRegItem> cRegItems;

                for(uint8_t row = 0; row < cChip->getNumberOfRows(); ++row)
                {
                    for(uint8_t col = 0; col < cChip->getNumberOfCols(); ++col)
                    {
                        auto theChannelRegisterPair = getChannelCounterRegister(row, col, cChip->getFrontEndType() == FrontEndType::MPA2);
                        cRegItems.push_back(theChannelRegisterPair.first);
                        cRegItems.push_back(theChannelRegisterPair.second);
                    }
                }

                if(!fFEConfigurationInterface->MultiRead(cChip, cRegItems)) continue;
                
                for(auto cIter = cRegItems.begin(); cIter < cRegItems.end(); cIter += 4)
                {
                    auto cMSBCounterOdd = (*cIter).fValue;
                    auto cLSBCounterOdd = (*(cIter + 1)).fValue;
                    auto cMSBCounterEven = (*(cIter + 2)).fValue;
                    auto cLSBCounterEven = (*(cIter + 3)).fValue;

                    uint32_t cValue = ((cChip->getFrontEndType() == FrontEndType::MPA2 ? 1 : 0) << 31) | (cMSBCounterEven << 23) | (cLSBCounterEven << 15) | (cMSBCounterOdd << 8) | cLSBCounterOdd;
                    fData.push_back(cValue);
                }
            } // chip loop
        }     // hybrid loop
    }         // board loop
}


bool D19cPSCounterFWInterface::FastRead(const Ph2_HwDescription::BeBoard* theBoard)
{
    BoardDataContainer theMissingCounterContainer;
    ContainerFactory::copyAndInitChip<std::vector<std::pair<uint8_t, uint8_t>>>(*theBoard, theMissingCounterContainer);
    for(auto theOpticalGroup: *theBoard)
    {
        fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 1 << theOpticalGroup->getId());
        auto cNFIFOentries = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.num_fifo_entry");
        if(cNFIFOentries < 2041)
        {
            LOG(WARNING) << WARNING_FORMAT << "Imcomplete counter packer" << RESET;
            return false;
        }
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
        // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en") << std::dec << std::endl;

        // sleep(5);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto   cDDR3state  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
        size_t cIterations = 0;
        while((cDDR3state >> 3) != 1 && cIterations < 10) // while not in idle state
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cDDR3state     = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_ddr3_packer.fsm_state");
            cIterations++;
        }
        if((cDDR3state >> 3) != 1)
        {
            LOG(WARNING) << WARNING_FORMAT << "Failed to read DDR3" << RESET;
            return false;
        }
        
        auto moduleData = fTheRegManager->ReadBlockRegOffset("fc7_daq_ddr3", cNFIFOentries * 16, 0x20000 * theOpticalGroup->getId());

        for(auto theHybrid: *theOpticalGroup)
        {
            
            for(size_t dataCounterPacketNumber = 1; dataCounterPacketNumber < 2041; ++dataCounterPacketNumber)
            {
                uint8_t pixelCol = (dataCounterPacketNumber - 1)%120;
                uint8_t pixelRow = (dataCounterPacketNumber - 1)/120;

                size_t hybridDataSize = 8;
                size_t hybridDataStart = (dataCounterPacketNumber * 2 + (theHybrid->getId() % 2)) * hybridDataSize;
                auto startPointer = moduleData.begin() + hybridDataStart;
                std::vector<uint32_t> hybridPacket = {*(startPointer+3), *(startPointer+2), *(startPointer+1), *(startPointer+0), *(startPointer+7), *(startPointer+6), *(startPointer+5), *(startPointer+4)};
                
                uint8_t numberOfCounters = (hybridPacket[5] >> 8) & 0x3F;
                if(numberOfCounters < 8)
                {
                    std::vector<bool> packetFound(8, false);
                    for(uint8_t counterNumber = 8 - numberOfCounters; counterNumber < 8; ++counterNumber)
                    {
                        uint8_t counterStart = 21 * counterNumber;
                        uint8_t counterEnd = 21 * (counterNumber+1) -1;

                        uint8_t firstWord  = counterStart / 32;
                        uint8_t secondWord = counterEnd / 32;

                        uint32_t counterPacket;
                        if(firstWord == secondWord)
                        {
                            counterPacket = (hybridPacket[firstWord] >> (counterStart % 32) ) & 0x1FFFFF;
                        }
                        else
                        {
                            counterPacket = (hybridPacket[firstWord] >> (counterStart % 32) | (hybridPacket[secondWord] << (32 - counterStart % 32))) & 0x1FFFFF;
                        }

                        packetFound[(counterPacket >> 15) & 0x7] = true;
                    }

                    for(auto theChip: *theHybrid)
                    {
                        if((pixelRow < 16 && theChip->getFrontEndType() == FrontEndType::SSA2) || (pixelRow >= 16 && theChip->getFrontEndType() == FrontEndType::MPA2)) continue;
                        uint8_t idForCIC = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic->getMapping()[theChip->getId() % 8];
                        if(!packetFound[idForCIC])
                        {
                            theMissingCounterContainer.getChip(theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::vector<std::pair<uint8_t, uint8_t>>>().push_back({pixelRow, pixelCol});
                        }
                    }
                }
                else if(numberOfCounters>8)
                {
                    LOG(ERROR) << ERROR_FORMAT << "Number of counters = " << +numberOfCounters << " greater than 8, not able to handle this case" << RESET;
                    throw std::runtime_error("Number of counters greater than 8");
                }

                fData.insert(fData.end(), hybridPacket.begin(), hybridPacket.end());
            }
        }
    }

    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0);

    // integrate missing counters with I2C readout
    for(auto theOpticalGroup: *theBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            for(auto theChip: *theHybrid)
            {
                const auto& missingChannelVector = theMissingCounterContainer.getChip(theOpticalGroup->getId(), theHybrid->getId(), theChip->getId())->getSummary<std::vector<std::pair<uint8_t, uint8_t>>>();
                if(missingChannelVector.size() == 0) continue;
                uint8_t idForCIC = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic->getMapping()[theChip->getId() % 8];
                std::vector<ChipRegItem> registersToRead;
                for(const auto& missingChannel: missingChannelVector)
                {
                    auto theChannelRegisterPair = getChannelCounterRegister(missingChannel.first, missingChannel.second, theChip->getFrontEndType() == FrontEndType::MPA2);
                    registersToRead.push_back(theChannelRegisterPair.first);
                    registersToRead.push_back(theChannelRegisterPair.second);
                }

                fFEConfigurationInterface->MultiRead(theChip, registersToRead);

                uint16_t registerCounter = 0;
                for(const auto& missingChannel: missingChannelVector)
                {
                    uint16_t counterValue = registersToRead[registerCounter*2].fValue << 8 | registersToRead[registerCounter*2 + 1].fValue;
                    uint32_t fakeStubPacket = (7 << 19) | (idForCIC) << 16 | ((counterValue & 0x7F) << 8) | ((counterValue >> 7) & 0x7F);

                    uint32_t channelOffset = theOpticalGroup->getId() * 2040 + missingChannel.first * 120 + missingChannel.second + 256 * theHybrid->getId();
                    uint8_t numberOfCounters = (fData[channelOffset + 5] >> 8) & 0x3F;

                    uint8_t counterNumber = 8 - (numberOfCounters + 1);

                    uint8_t counterStart = 21 * counterNumber;
                    uint8_t counterEnd = 21 * (counterNumber+1) -1;

                    uint32_t firstWord  = counterStart / 32;
                    uint32_t secondWord = counterEnd / 32;

                    uint32_t mask = 0x1FFFF;

                    if(firstWord == secondWord)
                    {
                        fData[channelOffset + firstWord] = (fData[channelOffset + firstWord] & (~(mask << (counterStart % 32)))) | ((fakeStubPacket & mask) << (counterStart % 32));
                    }
                    else
                    {
                        fData[channelOffset + firstWord]  = (fData[channelOffset + firstWord] & (~(mask << (counterStart % 32)))) | ((fakeStubPacket & mask) << (counterStart % 32));
                        fData[channelOffset + secondWord] = (fData[channelOffset + firstWord] & (~(mask >> (32 - counterStart % 32)))) | ((fakeStubPacket & mask) >> (32 - counterStart % 32));
                    }

                    ++registerCounter;

                }

            }
        }
    }

    return true;

}

bool D19cPSCounterFWInterface::ReadEvents(const BeBoard* theBoard)
{
    // clear data vector
    fData.clear();

    uint32_t delayAfterFastReset = 100;
    uint32_t delayAfterTestPulse = 50;
    uint32_t delayBeforeNextPulse = 50;
    uint32_t afterClearCounters = 100;
    uint32_t afterCloseShutter = 50;
    uint32_t afterOpenShutter = 50;

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren = " << fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren") << std::endl;
    
    std::vector<std::pair<std::string, uint32_t>> firstListOfBoardRegisters;
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.clear_counters", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.open_shutter", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.close_shutter", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.sync_block.enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.stub_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.ddr3_debug.scan_chain_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 12});
    firstListOfBoardRegisters.push_back({"fc7_daq_ctrl.dio5_block.control.load_config", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});
    firstListOfBoardRegisters.push_back({"fc7_daq_ctrl.physical_interface_block.control.decoder_reset", 0x1});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", delayAfterFastReset});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", delayAfterTestPulse});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", delayBeforeNextPulse});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_clear_counters", afterClearCounters});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_close_shutter", afterCloseShutter});
    firstListOfBoardRegisters.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_delay.after_open_shutter", afterOpenShutter});

    fTheRegManager->WriteStackReg(firstListOfBoardRegisters);

    // make sure trigger mult is taken into account
    auto cMultiplicity = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fNEvents           = fNEvents * (cMultiplicity + 1);

    fTriggerInterface->SetNTriggersToAccept(fNEvents);

    fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
    uint32_t waitForDataCollection = (delayAfterFastReset + afterOpenShutter + afterClearCounters + afterCloseShutter + (delayAfterTestPulse + delayBeforeNextPulse) * fNEvents) / 1000;
    std::this_thread::sleep_for(std::chrono::microseconds(waitForDataCollection + 5000));

    if(fPSCounterFast)
    {

        size_t readDDR3Iteration    = 0;
        size_t maxReadDDR3Iterations    = 30;
        while(readDDR3Iteration < maxReadDDR3Iterations)
        {

            size_t searchStartPatternIteration    = 0;
            size_t maxSearchStartPatternIterations    = 30;

            while(searchStartPatternIteration < maxSearchStartPatternIterations)
            {
                size_t fsmStartIterations    = 0;
                bool allCompleted;
                while(fsmStartIterations < 30)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    allCompleted = true;
                    for(auto theOpticalGroup: *theBoard)
                    {
                        for(auto theHybrid: *theOpticalGroup)
                        {
                            fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());

                            auto cDecoderState  = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.store_fsm_state");
                            if(cDecoderState != 0x00)
                            {
                                allCompleted = false;
                                break;
                            }
                        }
                        if(!allCompleted) break;
                    }
                    if(allCompleted) break;
                    fsmStartIterations++;
                }

                if(!allCompleted)
                {
                    LOG(ERROR) << ERROR_FORMAT << "Fast counter FSM did not run, please contact Fabio Ravera" << RESET;
                    throw std::runtime_error("Fast counter FSM did not run");
                }
                
                bool allStartPatternFound = true;

                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        fTheRegManager->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
                        uint32_t startPatternNotFound = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.start_pattern_not_found");
                        if(startPatternNotFound == 1)
                        {
                            LOG(WARNING) << WARNING_FORMAT << "Start pattern not found for OpticalGroup " << +theOpticalGroup->getId() << " hybrid " << theHybrid->getId() % 2 << RESET;
                            allStartPatternFound = false;
                        }
                    }
                }

                if(!allStartPatternFound)
                {
                    fTheRegManager->WriteStackReg(firstListOfBoardRegisters);
                    fTriggerInterface->SetNTriggersToAccept(fNEvents);
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    fTheRegManager->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
                    std::this_thread::sleep_for(std::chrono::microseconds(waitForDataCollection + 5000));
                    ++searchStartPatternIteration;
                }
                else break;
            }
            if(searchStartPatternIteration >= maxSearchStartPatternIterations)
            {
                LOG(ERROR) << ERROR_FORMAT << "Start pattern not found after " << searchStartPatternIteration << " trials" << RESET;
                throw std::runtime_error("Start pattern not found");
            }

            if(FastRead(theBoard)) break;
            else ++readDDR3Iteration;
        }

        if(readDDR3Iteration >= maxReadDDR3Iterations)
        {
            LOG(ERROR) << ERROR_FORMAT << "DDR3 not read after " << readDDR3Iteration << " trials" << RESET;
            throw std::runtime_error("DDR3 not read");
        }
    }
    else
    {
        SlowRead(theBoard);
    }

    return true;

}

} // namespace Ph2_HwInterface
