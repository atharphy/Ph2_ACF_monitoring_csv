#include "D19cPSCounterFWInterface.h"
// #include "FEConfigurationInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
    
D19cPSCounterFWInterface::D19cPSCounterFWInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : L1ReadoutInterface(pId, pUri, pAddressTable) {
    fFEConfigurationInterface = nullptr;
}
D19cPSCounterFWInterface::D19cPSCounterFWInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : L1ReadoutInterface(puHalConfigFileName, pBoardId) {
    fFEConfigurationInterface = nullptr;
}
D19cPSCounterFWInterface::~D19cPSCounterFWInterface() {}

void D19cPSCounterFWInterface::PS_Open_shutter()
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 1;
    uint8_t cBC0      = 0;
    fFastCommandInterface->ComposeFastCommand(cReSync, cL1A, cCalPulse, cBC0);
}

void D19cPSCounterFWInterface::PS_Close_shutter()
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 1;
    fFastCommandInterface->ComposeFastCommand(cReSync, cL1A, cCalPulse, cBC0);
}
void D19cPSCounterFWInterface::PS_Clear_counters()
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 1;
    uint8_t cBC0      = 1;
    // clear
    fFastCommandInterface->ComposeFastCommand(cReSync, cL1A, cCalPulse, cBC0);
}
void D19cPSCounterFWInterface::PS_Inject()
{
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 1;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 0;
    // clear
    fFastCommandInterface->ComposeFastCommand(cReSync, cL1A, cCalPulse, cBC0);
}
void D19cPSCounterFWInterface::PS_Start_counters_read()
{
    uint8_t cReSync   = 1;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 1;
    fFastCommandInterface->ComposeFastCommand(cReSync, cL1A, cCalPulse, cBC0);
}

// some overlap for now...
void D19cPSCounterFWInterface::PS_Send_pulses(uint32_t pNtriggers, bool manual)
{
    if(manual)
    {
        for(uint16_t numit = 0; numit < pNtriggers; numit++) this->PS_Inject();
    }
    else fTriggerInterface->RunTriggerFSM();  
}

// method to read MPA counters over I2C
void D19cPSCounterFWInterface::ReadMPACounters(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    // get event type
    EventType cEventType = pBoard->getEventType();
    if(cEventType == EventType::SCAS)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from MPA" << +cChip->getId() << RESET;
                    std::vector<ChipRegItem> cRegItems; 
                    auto cId = Compose_Id(pBoard, cOpticalGroup, cFe, cChip); 
                    auto cIterator = fPSCounterData.find(cId);
                    if( cIterator != fPSCounterData.end() ) cIterator->second.clear();
                    for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        // address
                        uint32_t cBaseRegisterLSB = ((12 + 8 * (cChnl / 120)) << 8) + 0x81;
                        uint32_t cBaseRegisterMSB = cBaseRegisterLSB + 128;
                        // MSB
                        ChipRegItem cReg_Counters_MSB;
                        cReg_Counters_MSB.fPage    = 0x00;
                        cReg_Counters_MSB.fAddress = cBaseRegisterMSB + cChnl;
                        cReg_Counters_MSB.fValue   = 0x00;
                        cRegItems.push_back( cReg_Counters_MSB);
                        // LSB
                        ChipRegItem cReg_Counters_LSB;
                        cReg_Counters_LSB.fPage    = 0x00;
                        cReg_Counters_LSB.fAddress = cBaseRegisterLSB + cChnl;
                        cReg_Counters_LSB.fValue   = 0x00;
                        cRegItems.push_back( cReg_Counters_LSB);
                    }
                    if( !fFEConfigurationInterface->MultiRead(cChip, cRegItems) ) continue;
                    LOG (INFO) << BOLDYELLOW << "Read-back " << cRegItems.size() << " counters from MPA#" << +cChip->getId() << RESET; 
                    // fill counter information 
                    for(auto cIter = cRegItems.begin(); cIter < cRegItems.end(); cIter+=2 )
                    {
                        auto cMSB = (*cIter).fValue; 
                        auto cLSB = (*(cIter+1)).fValue; 
                        fPSCounterData[cId].push_back( (cMSB << 8) | cLSB );
                    }
                    // // set in data vector
                    // uint32_t cDataWord    = 0x0000;
                    // uint32_t cWordCounter = 0;
                    // uint16_t cIndx        = 0;
                    // for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    // {
                    //     uint8_t cMPAId;
                    //     bool    cFailed = false;
                    //     bool    cRead;
                    //     // address
                    //     uint32_t    cBaseRegisterLSB = ((12 + 8 * (cChnl / 120)) << 8) + 0x81;
                    //     uint32_t    cBaseRegisterMSB = cBaseRegisterLSB + 128;
                    //     ChipRegItem cReg_Counters_MSB;
                    //     cReg_Counters_MSB.fPage    = 0x00;
                    //     cReg_Counters_MSB.fAddress = cBaseRegisterMSB + cChnl;
                    //     cReg_Counters_MSB.fValue   = 0x00;
                    //     ChipRegItem cReg_Counters_LSB;
                    //     cReg_Counters_LSB.fPage    = 0x00;
                    //     cReg_Counters_LSB.fAddress = cBaseRegisterLSB + cChnl;
                    //     cReg_Counters_LSB.fValue   = 0x00;
                    //     if(!pBoard->isOptical())
                    //         this->DecodeReg(cReg_Counters_MSB, cMPAId, cVec[cIndx], cRead, cFailed);
                    //     else
                    //         cVec[cIndx] = this->ReadFERegister(cChip, cReg_Counters_MSB.fAddress);

                    //     if(!pBoard->isOptical())
                    //         this->DecodeReg(cReg_Counters_LSB, cMPAId, cVec[cIndx + 1], cRead, cFailed);
                    //     else
                    //         cVec[cIndx] = this->ReadFERegister(cChip, cReg_Counters_LSB.fAddress);

                    //     cIndx += 2;
                    //     uint16_t cCounterValue = ((cReg_Counters_MSB.fValue & 0xFF) << 8) | (cReg_Counters_LSB.fValue & 0xFF);
                    //     if(cChnl < 10)
                    //     {
                    //         LOG(DEBUG) << BOLDMAGENTA << "Strip#" << +cChnl << " : " << +cCounterValue << " hits."
                    //                    << " LSB " << +(cReg_Counters_LSB.fValue & 0xFF) << " MSB " << +(cReg_Counters_MSB.fValue & 0xFF) << RESET;
                    //     }
                    //     cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
                    //     if((cWordCounter & 0x1) == 1)
                    //     {
                    //         pData.push_back(cDataWord);
                    //         cDataWord = 0x0000;
                    //     }
                    //     cWordCounter++;
                    // }
                } // chip loop
            }     // hybrid loop
        }         // hybrid loop
        // clear counters after they have been read
        this->PS_Clear_counters();
    }
    else
    {
        LOG(ERROR) << BOLDRED << "Trying to read MPA counters when EventType does not match..." << RESET;
        throw std::runtime_error(std::string("Trying to read MPA counters when EventType does not match..."));
    }
}
uint32_t D19cPSCounterFWInterface::Compose_Id( BeBoard* pBoard, OpticalGroup* pGroup, Hybrid* pHybrid, Chip* pChip)
{
    uint8_t  cType = ( pChip->getFrontEndType() == FrontEndType::MPA ) ? 1 : 0; 
    uint32_t cId = (pBoard->getId() << (3+6+4+1+4)) | (pGroup->getId() << (3+6+4+1) ) | (pHybrid->getId() << (3+6+1) ) | ( pChip->getId() << (3+1) ) | cType ;
    return cId;             
}

// method to read SSA counters over I2C
void D19cPSCounterFWInterface::ReadSSACounters(BeBoard* pBoard, std::vector<uint32_t>& pData)
{
    // get event type
    EventType cEventType = pBoard->getEventType();
    if(cEventType == EventType::SCAS)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    auto cId = Compose_Id(pBoard, cOpticalGroup, cFe, cChip); 
                    auto cIterator = fPSCounterData.find(cId);
                    if( cIterator != fPSCounterData.end() ) cIterator->second.clear();
                    LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from SSA" << +cChip->getId() << "\t" << cId << RESET;
                    std::vector<ChipRegItem> cRegItems; 
                    for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        // MSB
                        ChipRegItem cReg_Counters_MSB;
                        cReg_Counters_MSB.fPage = 0x00;
                        cReg_Counters_MSB.fAddress = (cChip->getFrontEndType() == FrontEndType::SSA)? 0x0801 + cChnl : 0x0680 + cChnl;
                        cReg_Counters_MSB.fValue = 0x00;
                        cRegItems.push_back(cReg_Counters_MSB);    
                        // LSB
                        ChipRegItem cReg_Counters_LSB;
                        cReg_Counters_LSB.fPage = 0x00;
                        cReg_Counters_LSB.fAddress = (cChip->getFrontEndType() == FrontEndType::SSA)? 0x0901 + cChnl : 0x0580 + cChnl;
                        cRegItems.push_back(cReg_Counters_LSB);
                    }
                    if( !fFEConfigurationInterface->MultiRead(cChip, cRegItems) ) continue;
                    LOG (INFO) << BOLDYELLOW << "Read-back " << cRegItems.size() << " counters from SSAA#" << +cChip->getId() << RESET; 
                    // fill counter information 
                    for(auto cIter = cRegItems.begin(); cIter < cRegItems.end(); cIter+=2 )
                    {
                        auto cMSB = (*cIter).fValue; 
                        auto cLSB = (*(cIter+1)).fValue; 
                        fPSCounterData[cId].push_back( (cMSB << 8) | cLSB );
                    }

                } // chip loop
            }     // hybrid loop
        }         // hybrid loop
        // clear counters after they have been read
        this->PS_Clear_counters();
    }
    else
    {
        LOG(ERROR) << BOLDRED << "Trying to read SSA counters when EventType does not match..." << RESET;
        throw std::runtime_error(std::string("Trying to read SSA counters when EventType does not match..."));
    }
}
bool D19cPSCounterFWInterface::ReadPSCountersFast(uint8_t pRawMode, size_t pChipId, size_t pHybridId)
{
    bool                                          cSuccess = false;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    fDuration              = 0;
    uint32_t cIteration    = 0;
    auto     cDecoderState = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
    // wait until fifo is ready to start readout of counters
    do
    {
        LOG(DEBUG) << BOLDMAGENTA << "\t\t..D19cFWInterface::WaitForData DECODER State: " << +cDecoderState << "Running.. .Iteration#" << +cIteration << RESET;
        cDecoderState = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.state");
        cIteration++;
    } while(cDecoderState != 0); // idle state is 0
    LOG(DEBUG) << BOLDMAGENTA << "Decoder in IDLE state after " << +cIteration << " iterations." << RESET;

    std::this_thread::sleep_for(std::chrono::microseconds(1500));
    size_t      cNbits    = 200e3 * 8 * 6;
    size_t      cNWords   = cNbits / 32; // number of 32-bit words to read from DDR3
    auto        cData     = ReadBlockRegOffset("fc7_daq_ddr3", cNWords, 0);
    std::string cDataWord = "";
    size_t      cIndx     = 0;
    auto        cIter     = cData.begin();
    uint16_t    cBxId     = 0;
    if(pRawMode == 0)
    {
        do
        {
            uint8_t cHeader = ((*cIter) & (0xF << 28)) >> 28;
            if(cHeader == 0x5)
            {
                cDataWord = "";
                cDataWord += std::bitset<32>(*cIter).to_string();
                cIter++;
                cIndx++;
                cDataWord += std::bitset<32>(*cIter).to_string();
                LOG(INFO) << BOLDBLUE << "Indx" << cIndx << " : Bx#" << +cBxId << " : " << cDataWord << RESET;
                cBxId++;
            }
            cIter++;
            cIndx++;
        } while(cIter < cData.end());
        return true;
    }
    else // raw counter readout - have to parse stubs in sw
    {
        // clear stub buffer
        fStubBuffer.clear();
        cIndx = 0;
        cIter += 4;
        std::vector<uint32_t> cBxCounter;
        do
        {
            std::stringstream cPacket512;
            for(uint8_t cFrag = 0; cFrag < 256 / 32; cFrag++)
            {
                if(cIter >= cData.end()) break;
                cPacket512 << std::bitset<32>(*cIter);
                // LOG (INFO) << BOLDGREEN << std::bitset<32>(*cIter);
                cIter++;
            }
            if(cIter < cData.end() && cPacket512.str().length() >= 80)
            {
                std::pair<std::string, std::string> cDataWrd;
                cDataWrd.first  = cPacket512.str().substr(0, 32);     //(uint32_t)std::stoi( cPacket512.str().substr(0,32), 0, 2) ;
                cDataWrd.second = cPacket512.str().substr(32, 6 * 8); // if 640 this needs to change
                for(size_t cClk = 0; cClk < 8; cClk++)
                {
                    fStubBuffer.push_back(static_cast<uint8_t>(std::stoi(cDataWrd.second.substr(6 * cClk, 6), 0, 2)));
                    // LOG (INFO) << BOLDBLUE << "Bx " << cDataWrd.first << " : " << std::bitset<6>(fStubBuffer[fStubBuffer.size()-1]) << RESET;
                    // cBxCounter.push_back( static_cast<uint32_t>( std::stoi( cPacket512.str().substr(0,32), 0, 2 ) ) );
                }
            }
            cIndx++;
        } while(cIter < cData.end());
        cSuccess = CheckStartPattern();
    }
    return cSuccess;
}
// method to read SSA/MPA counters over stub lines on single chip cards
void D19cPSCounterFWInterface::ReadPSSCCountersFast(BeBoard* pBoard, std::vector<uint32_t>& pData, uint8_t pRawMode)
{
    this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", 0x0);
    this->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", pRawMode);
    this->WriteReg("fc7_daq_cnfg.physical_interface_block.first_counter_delay", fPSCounterDelay);
    pData.clear();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            for(auto cChip: *cFe)
            {
                uint8_t cPairId = (cChip->getId() % 2 == 0) ? 1 : 0;
                uint8_t cChipId = (fPairSelect) ? cPairId : cChip->getId();

                this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cChipId);
                auto cStatus = this->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
                LOG(DEBUG) << BOLDBLUE << "Fast SSA counter readback... Chip#" << +cChip->getId() << " PS counters status [pre-start] is " << +cStatus << " [ offset is " << +fPSCounterDelay << "]"
                           << RESET;
                PS_Start_counters_read();
                do
                {
                    LOG(DEBUG) << BOLDBLUE << "PS counters status is " << +cStatus << RESET;
                    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
                    cStatus = this->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
                } while(cStatus == 0);

                LOG(DEBUG) << BOLDBLUE << "PS counters " << BOLDGREEN << " READY " << RESET;
                uint32_t cDataWord    = 0x0000;
                uint32_t cWordCounter = 0;
                for(int cChannelId = 0; cChannelId < (int)cChip->size(); cChannelId++)
                {
                    if(pRawMode == 1) // moved over from old MPA method .. needs to be checked/generatlized for both SSA/MPA case
                    {
                        uint32_t cycle = 0;
                        // MPA will output 16*120 + 120 counters
                        // SSA witll output 120 counters
                        size_t                cNCounters = (cChip->getFrontEndType() == FrontEndType::MPA) ? 2040 : cChip->size();
                        std::vector<uint16_t> count(cNCounters, 0);
                        for(int i = 0; i < 20000; i++)
                        {
                            uint32_t fifo1_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo1_data");
                            uint32_t fifo2_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");

                            uint32_t line1 = (fifo1_word & 0x0000FF) >> 0;  // to_number(fifo1_word,8,0)
                            uint32_t line2 = (fifo1_word & 0x00FF00) >> 8;  // to_number(fifo1_word,16,8)
                            uint32_t line3 = (fifo1_word & 0xFF0000) >> 16; //  to_number(fifo1_word,24,16)

                            uint32_t line4 = (fifo2_word & 0x0000FF) >> 0; // to_number(fifo2_word,8,0)
                            uint32_t line5 = (fifo2_word & 0x00FF00) >> 8; // to_number(fifo2_word,16,8)

                            if(((line1 & 0x80) == 128) && ((line4 & 0x80) == 128))
                            {
                                uint32_t temp = ((line2 & 0x20) << 9) | ((line3 & 0x20) << 8) | ((line4 & 0x20) << 7) | ((line5 & 0x20) << 6) | ((line1 & 0x10) << 6) | ((line2 & 0x10) << 5) |
                                                ((line3 & 0x10) << 4) | ((line4 & 0x10) << 3) | ((line5 & 0x80) >> 1) | ((line1 & 0x40) >> 1) | ((line2 & 0x40) >> 2) | ((line3 & 0x40) >> 3) |
                                                ((line4 & 0x40) >> 4) | ((line5 & 0x40) >> 5) | ((line1 & 0x20) >> 5);
                                if(temp != 0)
                                {
                                    count[cycle] = temp - 1;
                                    cycle += 1;
                                }
                            }
                        }
                    }
                    else
                    {
                        uint32_t fifo2_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");
                        cDataWord           = (cDataWord) | (fifo2_word << (cWordCounter & 0x1) * 16);
                        if(cChannelId < 5 || cChannelId > 115)
                        {
                            LOG(INFO) << BOLDGREEN << "Chip#" << +cChip->getId() << " Pair#" << +cPairId << " Chnl#" << +cChannelId << "\t\t" << std::bitset<32>(fifo2_word) << " [ " << fifo2_word
                                      << " ] " << RESET;
                        }
                        if((cWordCounter & 0x1) == 1)
                        {
                            pData.push_back(cDataWord);
                            cDataWord = 0x0000;
                        }
                        cWordCounter++;
                    }
                }
            }
        }
    }
}

void D19cPSCounterFWInterface::FillData()
{   
    // TO-DO 
    // use fPSCounterData to fill 32-bit word vector 
    // this should match what you expect in the event decoder 
}
bool D19cPSCounterFWInterface::WaitForData()
{
    auto cTriggerSource = this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    if(cTriggerSource == 10 || cTriggerSource == 12 ) 
    {
        LOG(INFO) << BOLDBLUE << "D19cPSCounterFWInterface::Async triggers" << RESET;
        // fTriggerInterface->ReconfigureTriggerFSM();
        return fTriggerInterface->RunTriggerFSM();
    }
    else return false; // wrong trigger source for this type of readout 
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
    do
    {
        for(size_t cClk = 0; cClk < 8; cClk++)
        {
            LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
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
                cStubFlds.push_back(std::make_pair("FeId", 3));
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
                    if(cStartPatternFound)
                        LOG(DEBUG) << BOLDGREEN << "D19cPSCounterFWInterface::CheckStartPattern CheckForStartPattern from PS counters - Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId
                                   << " : " << cStubOutput.str() << RESET;
                    else
                        LOG(DEBUG) << BOLDRED << "Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : " << cStubOutput.str() << RESET;
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
} // namespace Ph2_HwInterface