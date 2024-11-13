#include "HWInterface/D19cPSCounterFWInterface.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Chip.h"
#include "HWDescription/ChipRegItem.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/OpticalGroup.h"
#include "HWInterface/FEConfigurationInterface.h"
#include "HWInterface/FastCommandInterface.h"
#include "HWInterface/RegManager.h"
#include "HWInterface/TriggerInterface.h"
#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"
#include <thread>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cPSCounterFWInterface::D19cPSCounterFWInterface(RegManager* theRegManager) : L1ReadoutInterface(theRegManager)
{
    // handshake should always be off for this readout mode
    fHandshake = 0;
}

D19cPSCounterFWInterface::~D19cPSCounterFWInterface() {}

bool D19cPSCounterFWInterface::ResetReadout()
{
    LOG(INFO) << BOLDRED << "Nothing to reset for PS counter interface.." << RESET;
    return true;
}

void D19cPSCounterFWInterface::PS_Open_shutter()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalL1A();
}

void D19cPSCounterFWInterface::PS_Close_shutter()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterReset();
}
void D19cPSCounterFWInterface::PS_Clear_counters()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterResetL1A();
}
void D19cPSCounterFWInterface::PS_Inject()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCalPulse();
}
void D19cPSCounterFWInterface::PS_Start_counters_read()
{
    for(uint16_t numit = 0; numit < fFCDupe; numit++) fFastCommandInterface->SendGlobalCounterResetResync();
}
void D19cPSCounterFWInterface::PS_Send_pulses(uint32_t pNtriggers, bool manual)
{
    if(manual)
    {
        for(uint16_t numit = 0; numit < pNtriggers; numit++) this->PS_Inject();
    }
    else
        fTriggerInterface->RunTriggerFSM();
}

// compose id for counter data
uint32_t D19cPSCounterFWInterface::Compose_Id(const BeBoard* pBoard, const OpticalGroup* pGroup, const Hybrid* pHybrid, const Chip* pChip)
{
    uint8_t  cType = (pChip->getFrontEndType() == FrontEndType::MPA2) ? 1 : 0;
    uint32_t cId   = (pBoard->getId() << (3 + 6 + 4 + 1 + 4)) | (pGroup->getId() << (3 + 6 + 4 + 1)) | (pHybrid->getId() << (3 + 6 + 1)) | (pChip->getId() << (3 + 1)) | cType;
    return cId;
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
                std::stringstream cChipType;
                cChip->printChipType(cChipType);

                // LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from Chip#" << +cChip->getId() << RESET;
                std::vector<ChipRegItem> cRegItems;
                auto                     cId       = Compose_Id(pBoard, cOpticalGroup, cHybrid, cChip);
                auto                     cIterator = fPSCounterData.find(cId);
                if(cIterator != fPSCounterData.end()) cIterator->second.clear();
                for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                {
                    uint32_t cBaseRegisterLSB, cBaseRegisterMSB;
                    cBaseRegisterLSB = 0;
                    cBaseRegisterMSB = 0;
                    if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        int cRowNumber   = 1 + cChnl / 120;
                        int cPixelNumber = 1 + cChnl % 120;

                        cBaseRegisterLSB = ((cRowNumber << 11) | (9 << 7) | cPixelNumber);
                        cBaseRegisterMSB = ((cRowNumber << 11) | (10 << 7) | cPixelNumber);
                        if(cChip->getFrontEndType() == FrontEndType::MPA2)
                        {
                            cBaseRegisterLSB -= 0x280;
                            cBaseRegisterMSB -= 0x280;
                        }
                    }
                    if(cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        cBaseRegisterLSB = 0x0580 + cChnl;
                        cBaseRegisterMSB = 0x0680 + cChnl;
                    }

                    // MSB
                    ChipRegItem cReg_Counters_MSB;
                    cReg_Counters_MSB.fPage    = 0x00;
                    cReg_Counters_MSB.fAddress = cBaseRegisterMSB;
                    cReg_Counters_MSB.fValue   = 0x00;
                    cRegItems.push_back(cReg_Counters_MSB);
                    // LSB
                    ChipRegItem cReg_Counters_LSB;
                    cReg_Counters_LSB.fPage    = 0x00;
                    cReg_Counters_LSB.fAddress = cBaseRegisterLSB;
                    cReg_Counters_LSB.fValue   = 0x00;
                    cRegItems.push_back(cReg_Counters_LSB);
                }
                if(!fFEConfigurationInterface->MultiRead(cChip, cRegItems)) continue;
                // LOG(DEBUG) << BOLDYELLOW << "Read-back " << cRegItems.size() << " counters from " << cChipType.str() << "#" << +cChip->getId() << "#" << +cId << RESET;
                // fill counter information
                for(auto cIter = cRegItems.begin(); cIter < cRegItems.end(); cIter += 4)
                {
                    auto cMSBCounterEven = (*cIter).fValue;
                    auto cLSBCounterEven = (*(cIter + 1)).fValue;
                    auto cMSBCounterOdd = (*(cIter + 2)).fValue;
                    auto cLSBCounterOdd = (*(cIter + 3)).fValue;
                    uint32_t cValue = (cId << 31) | (cMSBCounterOdd << 23) | (cLSBCounterOdd << 15) | (cMSBCounterEven << 8) | cLSBCounterEven;
                    fData.push_back(cValue);
                }
            } // chip loop
        }     // hybrid loop
    }         // board loop
    // PS_Clear_counters();
}


bool D19cPSCounterFWInterface::FastRead(const Ph2_HwDescription::BeBoard* theBoard)
{
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
                // uint16_t pixelCol = (dataCounterPacketNumber - 1)%120;
                // uint16_t pixelRow = (dataCounterPacketNumber - 1)/120;

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
                    
                }
                else if(numberOfCounters>8)
                {
                    LOG(WARNING) << WARNING_FORMAT << "Number of stub = " << +numberOfCounters << " greater than 8, not able to handle this case" << RESET;
                }
                // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] numberOfCounters = " << +numberOfCounters << std::endl;
                
                // for(auto word : hybridPacket) std::cout << std::hex << word << std::dec << " ";
                // std::cout << std::endl;
            }
            abort();
        }
    }

    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0);
    fTheRegManager->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.ddr3_wren", 0);

    return true;

}

void D19cPSCounterFWInterface::GetCounterData(const BeBoard* pBoard)
{
    // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::GetCounterData" << RESET;
    auto cFrontEndTypes = pBoard->connectedFrontEndTypes();
    // LOG(DEBUG) << BOLDYELLOW << cFrontEndTypes.size() << " different types of Chips connected to BeBoard#" << +pBoard->getId() << RESET;
    if(fPSCounterFast == 0) // readout over registers
    {
        SlowRead(pBoard);
    }
    else // readout over fast interface
    {
        FastRead(pBoard);
    }
}
void D19cPSCounterFWInterface::FillData()
{
    // use fPSCounterData to fill 32-bit word vector
    // this should match what you expect in the event decoder
    fData.clear();
    // counter data will be filled into data vector
    // each 32-bit word contains 2 counters (30 bits)
    // will first fill in MPA data .. then SSA data
    // order of MPAs/SSAs will be the same as that defined
    // MSB indicates if its an MPA/SSA
    // 1 for MPA, 0 for SSA
    // by the hybrid node in the xml
    for(auto cCountersFromFE: fPSCounterData)
    {
        // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::FillData Filling data vector with counter information from Id" << cCountersFromFE.first << RESET;
        for(auto cIter = cCountersFromFE.second.begin(); cIter < cCountersFromFE.second.end(); cIter += 2)
        {
            uint32_t cValue = (cCountersFromFE.first << 31) | (*(cIter + 1) << 15) | (*cIter);
            // LOG (DEBUG) << BOLDYELLOW << "\t... First counter 0x" << std::hex << (*cIter)
            //     << " .. second counter is 0x" << *(cIter+1)
            //     << " .. value saved in 32-bit word is 0x" << cValue
            //     << std::dec
            //     << RESET;
            fData.push_back(cValue);
        }
    }
}
bool D19cPSCounterFWInterface::WaitForNTriggers()
{
    // fTriggerInterface->ResetTriggerFSM();
    // make sure counters have been cleared and reset
    // not sure its needed but.. to be safe

    // PS_Open_shutter();
    // for(size_t cIndx=0; cIndx < fNEvents; cIndx++) PS_Inject();
    // PS_Close_shutter();
    // return true;

    // fData.clear();
    // auto cMultiplicity = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    // fNEvents           = fNEvents * (cMultiplicity + 1);
    // fTriggerInterface->SetNTriggersToAccept(fNEvents);

    // // wait for trigger state machine to send all triggers
    auto cTriggerSource = this->fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData After resetting trigger FSM.. trigger source is " << cTriggerSource << RESET;

    if(cTriggerSource == 10 || cTriggerSource == 12)
    {
        // LOG(DEBUG) << BOLDYELLOW << "D19cPSCounterFWInterface::WaitForData Running Trigger FSM ..." << RESET;
        return fTriggerInterface->RunTriggerFSM();
    }
    else
    {
        LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData  USING WRONG TRIGGER SOURCE FOR THIS TEST... " << cTriggerSource << RESET;
        return false; // wrong trigger source for this type of readout
    }
}
bool D19cPSCounterFWInterface::WaitForReadout()
{
    LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData.. .no real data readout" << RESET;
    return false;
}
bool D19cPSCounterFWInterface::PollReadoutData(const Ph2_HwDescription::BeBoard* pBoard, bool pWait)
{
    LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::WaitForData.. .no real data readout" << RESET;
    return false;
}
bool D19cPSCounterFWInterface::ReadEventsOld(const BeBoard* pBoard)
{
    // clear data vector
    fData.clear();
    // make sure trigger mult is taken into account
    auto cMultiplicity = fTheRegManager->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fNEvents           = fNEvents * (cMultiplicity + 1);

    fTriggerInterface->SetNTriggersToAccept(fNEvents);

    // make sure handshake is configured
    fTheRegManager->WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable", fHandshake);
    bool byrow   = false;
    bool bypixel = false;
    bool success = true;
    // fTriggerInterface->ResetTriggerFSM();
    // make sure counters have been cleared and reset
    // not sure its needed but.. to be safe
    PS_Close_shutter();
    fFastCommandInterface->SendGlobalReSync();
    PS_Clear_counters();

    if(byrow or bypixel)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        ChipRegItem cReg_maskall;
                        cReg_maskall.fPage    = 0x00;
                        cReg_maskall.fAddress = 0x00;
                        cReg_maskall.fValue   = 0x00;

                        ChipRegItem cReg_unmaskall;
                        cReg_unmaskall.fPage    = 0x00;
                        cReg_unmaskall.fAddress = 0x801;
                        cReg_unmaskall.fValue   = 0x00;

                        fFEConfigurationInterface->SingleRead(cChip, cReg_unmaskall);

                        cReg_unmaskall.fAddress = 0x00;
                        cReg_maskall.fValue     = (cReg_unmaskall.fValue & 0x9E);

                        for(size_t cRow = 1; cRow < 17; cRow++)
                        {
                            if(byrow)
                            {
                                std::vector<ChipRegItem> cRegItems{cReg_maskall};

                                ChipRegItem cReg_unmaskrow;
                                cReg_unmaskrow.fPage    = 0x00;
                                cReg_unmaskrow.fAddress = (cRow << 11);
                                cReg_unmaskrow.fValue   = cReg_unmaskall.fValue;

                                // LOG(INFO) << BOLDRED << "NEW ROW " <<int(cRow)<< RESET;

                                cRegItems.push_back(cReg_unmaskrow);
                                fFEConfigurationInterface->MultiWrite(cChip, cRegItems);
                                WaitForNTriggers();

                                fFEConfigurationInterface->SingleWrite(cChip, cReg_unmaskall);
                            }
                            else if(bypixel)
                            {
                                for(size_t cCol = 1; cCol < 121; cCol++)
                                {
                                    // fReadoutChipInterface->maskPixel(0,0);
                                    // fReadoutChipInterface->maskRowCol(cRow,0,1);
                                    std::vector<ChipRegItem> cRegItems{cReg_maskall};

                                    ChipRegItem cReg_unmaskrow;
                                    cReg_unmaskrow.fPage    = 0x00;
                                    cReg_unmaskrow.fAddress = (cRow << 11) + cCol;
                                    cReg_unmaskrow.fValue   = cReg_unmaskall.fValue;

                                    // LOG(INFO) << BOLDRED << "NEW ROW " <<int(cRow)<< " NEW COL " <<int(cCol)<< RESET;
                                    // LOG(INFO) << BOLDRED << "ADDR " <<cReg_unmaskrow.fAddress<<" VAL "<<+cReg_unmaskall.fValue<< RESET;

                                    cRegItems.push_back(cReg_unmaskrow);
                                    fFEConfigurationInterface->MultiWrite(cChip, cRegItems);
                                    WaitForNTriggers();

                                    // LOG(INFO) << BOLDRED << "Done NEW TRIGs " << RESET;

                                    fFEConfigurationInterface->SingleWrite(cChip, cReg_unmaskall);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    else
        WaitForNTriggers();
    if(success)
    {
        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents triggers succesfully sent" << RESET;
        GetCounterData(pBoard);

        FillData();
        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadEvents filled data vector with " << fData.size() << " 32-bit words" << RESET;
        return (fData.size() > 0);
    }
    else
        LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::ReadEvents did not receive all triggers..." << RESET;

    return false;
}
bool D19cPSCounterFWInterface::CheckStartPattern()
{
    std::string                                   cStartPattern = "111111111111111";
    size_t                                        cBxId         = 0;
    std::vector<std::pair<uint32_t, std::string>> cBxCars;
    auto                                          cStubBufferIter = fStubBuffer.begin();
    std::string                                   cStubPkt        = "";
    size_t                                        cPktLength      = 0;
    // std::vector<uint16_t> cCounters(0);
    size_t cStubCounter = 0;
    bool   cStartFound  = false;
    // find first packet with more than 0 stubs
    do {
        for(size_t cClk = 0; cClk < 8; cClk++)
        {
            // LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
            if(((*cStubBufferIter & 0x3F) >> 5) == 1 || cPktLength > 0) // configuration bit is 1
            {
                std::stringstream cStream;
                cStream << std::bitset<6>(*cStubBufferIter & 0x3F);
                cStubPkt += cStream.str();
                cPktLength += 6;
            }

            if(cPktLength == 6 * 8 * 8)
            {
                // std::pair<uint32_t,std::string> cBxCar;
                // cBxCar.first = *( cBxCounter.begin()  + std::distance( cStubBuffer.begin(), cStubBufferIter) ) ;
                // cBxCar.second = cStubPkt;
                std::vector<uint8_t>                         cSizes{1, 9, 12, 6}; // Cnfg, Status, BxId, Nstubs
                std::vector<std::pair<std::string, uint8_t>> cHdrFlds;
                cHdrFlds.push_back(std::make_pair("Cnfg", 1));
                cHdrFlds.push_back(std::make_pair("Status", 9));
                cHdrFlds.push_back(std::make_pair("BxId", 12));
                cHdrFlds.push_back(std::make_pair("Nstbs", 6));
                size_t                   cShft = 0;
                std::stringstream        cStream;
                std::vector<std::string> cHdrVals;
                for(auto cFld: cHdrFlds)
                {
                    auto cSubStr = cStubPkt.substr(cShft, cFld.second);
                    cHdrVals.push_back(cSubStr);
                    if(cFld.first == "BxId" || cFld.first == "Nstbs") { cStream << BOLDYELLOW << "\t" << cFld.first << "=" << std::stoi(cSubStr, 0, 2) << "\t"; }
                    else
                        cStream << BOLDYELLOW << "\t" << cFld.first << "=" << cSubStr << "\t";
                    cShft += cFld.second;
                }
                size_t cNstubs = std::stoi(cHdrVals[3], 0, 2);
                // LOG (INFO) << BOLDBLUE << cStream.str() << "\t" << cStubPkt.substr(0,cShft) << ":" << cStubPkt.substr(cShft, 8*21) << RESET;
                std::vector<uint32_t>                        cStubs(0);
                std::vector<std::pair<std::string, uint8_t>> cStubFlds;
                cStubFlds.push_back(std::make_pair("Offset", 3));
                cStubFlds.push_back(std::make_pair("HybridId", 3));
                cStubFlds.push_back(std::make_pair("Stub", 15));
                size_t cSizeAvailable = cStubPkt.length() - cShft;
                if(cSizeAvailable < (3 + 3 + 15) * cNstubs) continue;
                for(size_t cStubId = 0; cStubId < cNstubs; cStubId++)
                {
                    if(cStartFound) continue;
                    std::stringstream cStubOutput;
                    bool              cStartPatternFound = false;
                    for(auto cFld: cStubFlds)
                    {
                        auto cSubStr = cStubPkt.substr(cShft, cFld.second);
                        if(cFld.first != "Stub") { cStubOutput << BOLDBLUE << "\t" << cFld.first << "\t" << cSubStr << RESET; }
                        else
                        {
                            cStartPatternFound     = (cSubStr == cStartPattern); // first stub needs to be all 1's
                            uint16_t cCounterValue = std::stoi(cSubStr.substr(8, 6) + cSubStr.substr(0, 7), 0, 2) - 1;
                            cStubOutput << BOLDBLUE << "\t" << cFld.first << "\t" << cSubStr << " [ " << cCounterValue << " ] " << RESET;
                            // cCounters.push_back( cCounterValue );
                        }
                        cShft += cFld.second;
                    }
                    // if(cStartPatternFound)
                    //     LOG(DEBUG) << BOLDGREEN << "D19cPSCounterFWInterface::CheckStartPattern CheckForStartPattern from PS counters - Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" <<
                    //     +cStubId
                    //                << " : " << cStubOutput.str() << RESET;
                    // else
                    //     LOG(DEBUG) << BOLDRED << "Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : " << cStubOutput.str() << RESET;
                    cStartFound = cStartPatternFound;
                    cStubCounter++;
                }
                cPktLength = 0;
                cStubPkt   = "";
                // cBxCars.push_back(cBxCar);
            }
            cStubBufferIter++;
        }
        cBxId++;
    } while(cStubBufferIter < fStubBuffer.end() && !cStartFound);
    return cStartFound;
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
