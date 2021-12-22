#include "D19cPSCounterFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
	D19cPSCounterFWInterface::D19cPSCounterFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler) : D19cFWInterface(puHalConfigFileName, pBoardId) {
	}
	D19cPSCounterFWInterface::D19cPSCounterFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler) : D19cFWInterface(pId, pUri, pAddressTable) {
	}
	D19cPSCounterFWInterface::~D19cPSCounterFWInterface() {}

	void D19cPSCounterFWInterface::PS_Open_shutter()
	{
	    uint8_t cReSync   = 0;
	    uint8_t cCalPulse = 0;
	    uint8_t cL1A      = 1;
	    uint8_t cBC0      = 0;
	    this->Compose_fast_command(cReSync, cL1A, cCalPulse, cBC0);
	}

	void D19cPSCounterFWInterface::PS_Close_shutter()
	{
	    uint8_t cReSync   = 0;
	    uint8_t cCalPulse = 0;
	    uint8_t cL1A      = 0;
	    uint8_t cBC0      = 1;
	    this->Compose_fast_command(cReSync, cL1A, cCalPulse, cBC0);
	}
	void D19cPSCounterFWInterface::PS_Clear_counters()
	{
	    uint8_t cReSync   = 0;
	    uint8_t cCalPulse = 0;
	    uint8_t cL1A      = 1;
	    uint8_t cBC0      = 1;
	    // clear
	    this->Compose_fast_command(cReSync, cL1A, cCalPulse, cBC0);
	}
	void D19cPSCounterFWInterface::PS_Inject()
	{
	    uint8_t cReSync   = 0;
	    uint8_t cCalPulse = 1;
	    uint8_t cL1A      = 0;
	    uint8_t cBC0      = 0;
	    // clear
	    this->Compose_fast_command(cReSync, cL1A, cCalPulse, cBC0);
	}
	void D19cPSCounterFWInterface::PS_Start_counters_read()
	{
	    uint8_t cReSync   = 1;
	    uint8_t cCalPulse = 0;
	    uint8_t cL1A      = 0;
	    uint8_t cBC0      = 1;
	    this->Compose_fast_command(cReSync, cL1A, cCalPulse, cBC0);
	}

	// some overlap for now...
	void D19cPSCounterFWInterface::PS_Send_pulses(uint32_t pNtriggers, bool manual)
	{
	    if(manual)
	    {
	        for(uint16_t numit = 0; numit < pNtriggers; numit++) this->PS_Inject();
	    }
	    else
	    {
	        this->WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNtriggers);
	        this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);

	        usleep(10);

	        this->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
	        uint32_t nsleeps   = 0;
	        uint32_t maxsleeps = 1000;
	        while(ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") and (nsleeps < maxsleeps))
	        {
	            nsleeps += 1;
	            usleep(10);
	        }
	        if(nsleeps == maxsleeps)
	        {
	            LOG(INFO) << "Cal pulses timeout";
	            PS_Clear_counters();
	            this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
	            usleep(10);
	            this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
	            usleep(10);
	            PS_Send_pulses(pNtriggers, manual);
	        }
	        WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
	    }
	}

	void D19cPSCounterFWInterface::Compose_fast_command(uint32_t resync_en, uint32_t l1a_en, uint32_t cal_pulse_en, uint32_t bc0_en)
	{
	    uint32_t encode_resync    = resync_en << 16;
	    uint32_t encode_cal_pulse = cal_pulse_en << 17;
	    uint32_t encode_l1a       = l1a_en << 18;
	    uint32_t encode_bc0       = bc0_en << 19;
	    uint32_t encode_duration  = fDuration << 28;

	    uint32_t final_command = encode_resync + encode_l1a + encode_cal_pulse + encode_bc0 + encode_duration;
	    // std::lock_guard<std::recursive_mutex> theGuard(fMutex);
	    WriteReg("fc7_daq_ctrl.fast_command_block.control", final_command);
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
	}

	//method to read MPA counters over I2C
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
	                    bool                  cWrite = false;
	                    std::vector<uint32_t> cVec;
	                    cVec.clear();
	                    std::vector<uint32_t> cReplies;
	                    cReplies.clear();
	                    // I think it would also work to loop over rows
	                    // then columns
	                    // 16 columns , 120 rows?
	                    if( !pBoard->isOptical() )
	                    {
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
		                        this->EncodeReg(cReg_Counters_MSB, cChip, cVec, true, cWrite);
		                        // LSB
		                        ChipRegItem cReg_Counters_LSB;
		                        cReg_Counters_LSB.fPage    = 0x00;
		                        cReg_Counters_LSB.fAddress = cBaseRegisterLSB + cChnl;
		                        cReg_Counters_LSB.fValue   = 0x00;
		                        this->EncodeReg(cReg_Counters_LSB, cChip, cVec, true, cWrite);
		                    }
		                }
	                    // read back
	                    if( pBoard->isOptical() ) this->ReadChipBlockReg(cVec);
	                    // set in data vector
	                    uint32_t cDataWord    = 0x0000;
	                    uint32_t cWordCounter = 0;
	                    uint16_t cIndx        = 0;
	                    for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
	                    {
	                        uint8_t cMPAId;
	                        bool    cFailed = false;
	                        bool    cRead;
	                        // address
	                        uint32_t    cBaseRegisterLSB = ((12 + 8 * (cChnl / 120)) << 8) + 0x81;
	                        uint32_t    cBaseRegisterMSB = cBaseRegisterLSB + 128;
	                        ChipRegItem cReg_Counters_MSB;
	                        cReg_Counters_MSB.fPage    = 0x00;
	                        cReg_Counters_MSB.fAddress = cBaseRegisterMSB + cChnl;
	                        cReg_Counters_MSB.fValue   = 0x00;
	                        ChipRegItem cReg_Counters_LSB;
	                        cReg_Counters_LSB.fPage    = 0x00;
	                        cReg_Counters_LSB.fAddress = cBaseRegisterLSB + cChnl;
	                        cReg_Counters_LSB.fValue   = 0x00;
	                        if( !pBoard->isOptical() ) this->DecodeReg(cReg_Counters_MSB, cMPAId, cVec[cIndx], cRead, cFailed);
	                        else cVec[cIndx] = this->ReadFERegister( cChip, cReg_Counters_MSB.fAddress);

	                        if( !pBoard->isOptical() ) this->DecodeReg(cReg_Counters_LSB, cMPAId, cVec[cIndx + 1], cRead, cFailed);
	                        else cVec[cIndx] = this->ReadFERegister( cChip, cReg_Counters_LSB.fAddress);
	                        
	                        cIndx += 2;
	                        uint16_t cCounterValue = ((cReg_Counters_MSB.fValue & 0xFF) << 8) | (cReg_Counters_LSB.fValue & 0xFF);
	                        if(cChnl < 10)
	                        {
	                            LOG(DEBUG) << BOLDMAGENTA << "Strip#" << +cChnl << " : " << +cCounterValue << " hits."
	                                       << " LSB " << +(cReg_Counters_LSB.fValue & 0xFF) << " MSB " << +(cReg_Counters_MSB.fValue & 0xFF) << RESET;
	                        }
	                        cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
	                        if((cWordCounter & 0x1) == 1)
	                        {
	                            pData.push_back(cDataWord);
	                            cDataWord = 0x0000;
	                        }
	                        cWordCounter++;
	                    }
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
	// // method to read SSA counters over I2C
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
	                    LOG(DEBUG) << BOLDBLUE << "Directly reading back counters from SSA" << +cChip->getId() << RESET;
	                    bool                  cWrite = false;
	                    std::vector<uint32_t> cVec;
	                    cVec.clear();
	                    std::vector<uint32_t> cReplies;
	                    cReplies.clear();
	                    if( !pBoard->isOptical() )
	                    {
		                    for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
		                    {
		                        // MSB
		                        ChipRegItem cReg_Counters_MSB;
		                        cReg_Counters_MSB.fPage = 0x00;
		                        if(cChip->getFrontEndType() == FrontEndType::SSA)
		                            cReg_Counters_MSB.fAddress = 0x0801 + cChnl;
		                        else
		                            cReg_Counters_MSB.fAddress = 0x0680 + cChnl;
		                        cReg_Counters_MSB.fValue = 0x00;
		                        this->EncodeReg(cReg_Counters_MSB, cChip, cVec, true, cWrite);
		                        // this->ReadChipBlockReg( cVec );
		                        // cReplies.push_back(cVec[0]);
		                        // cVec.clear();
		                        // LSB
		                        ChipRegItem cReg_Counters_LSB;
		                        cReg_Counters_LSB.fPage = 0x00;
		                        if(cChip->getFrontEndType() == FrontEndType::SSA)
		                            cReg_Counters_LSB.fAddress = 0x0901 + cChnl;
		                        else
		                            cReg_Counters_LSB.fAddress = 0x0580 + cChnl;
		                        cReg_Counters_LSB.fValue = 0x00;
		                        this->EncodeReg(cReg_Counters_LSB, cChip, cVec, true, cWrite);
		                        // this->ReadChipBlockReg( cVec );
		                        // cReplies.push_back(cVec[0]);
		                        // cVec.clear();
		                    }
		                }
	                    // read back
	                    if( !pBoard->isOptical() ) this->ReadChipBlockReg(cVec);
	                    // set in data vector
	                    uint32_t cDataWord    = 0x0000;
	                    uint32_t cWordCounter = 0;
	                    uint16_t cIndx        = 0;
	                    for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
	                    {
	                        uint8_t     cSSAId;
	                        bool        cFailed = false;
	                        bool        cRead;
	                        ChipRegItem cReg_Counters_MSB;
	                        cReg_Counters_MSB.fPage = 0x00;
	                        if(cChip->getFrontEndType() == FrontEndType::SSA)
	                            cReg_Counters_MSB.fAddress = 0x0801 + cChnl;
	                        else
	                            cReg_Counters_MSB.fAddress = 0x0680 + cChnl;
	                        cReg_Counters_MSB.fValue = 0x00;
	                        ChipRegItem cReg_Counters_LSB;
	                        cReg_Counters_LSB.fPage = 0x00;
	                        if(cChip->getFrontEndType() == FrontEndType::SSA)
	                            cReg_Counters_LSB.fAddress = 0x0901 + cChnl;
	                        else
	                            cReg_Counters_LSB.fAddress = 0x0580 + cChnl;
	                        cReg_Counters_LSB.fValue = 0x00;
	                        if( !pBoard->isOptical() ) this->DecodeReg(cReg_Counters_MSB, cSSAId, cVec[cIndx], cRead, cFailed);
	                        else cVec[cIndx] = this->ReadFERegister( cChip, cReg_Counters_MSB.fAddress ); 
	                        if( !pBoard->isOptical() ) this->DecodeReg(cReg_Counters_LSB, cSSAId, cVec[cIndx + 1], cRead, cFailed);
	                        else cVec[cIndx+1] = this->ReadFERegister( cChip, cReg_Counters_LSB.fAddress ); 
	                        cIndx += 2;
	                        uint16_t cCounterValue = ((cReg_Counters_MSB.fValue & 0xFF) << 8) | (cReg_Counters_LSB.fValue & 0xFF);
	                        if(cChnl < 10)
	                        {
	                            LOG(DEBUG) << BOLDMAGENTA << "Strip#" << +cChnl << " : " << +cCounterValue << " hits."
	                                       << " LSB " << +(cReg_Counters_LSB.fValue & 0xFF) << " MSB " << +(cReg_Counters_MSB.fValue & 0xFF) << RESET;
	                        }
	                        cDataWord = (cDataWord) | (cCounterValue << (cWordCounter & 0x1) * 16);
	                        if((cWordCounter & 0x1) == 1)
	                        {
	                            pData.push_back(cDataWord);
	                            cDataWord = 0x0000;
	                        }
	                        cWordCounter++;
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
	    fDuration   = 0;
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
	    auto        cData     = ReadBlockRegOffsetValue("fc7_daq_ddr3", cNWords, 0);
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
	
	uint32_t D19cPSCounterFWInterface::GetData(BeBoard* pBoard, std::vector<uint32_t>& pData)
	{
	    EventType cEventType = pBoard->getEventType();
	    bool      cAsync     = cEventType == EventType::SCAS;
	    if( !cAsync ) return 0;

	    bool      cWithMPA   = false;
	    bool      cWithSSA   = false;
	    bool      cWithSSA2  = false;
	    for(auto cOpticalGroup: *pBoard)
	    {
	        for(auto cFe: *cOpticalGroup)
	        {
	            for(auto cChip: *cFe)
	            {
	                cWithMPA  = cWithMPA || (cChip->getFrontEndType() == FrontEndType::MPA);
	                cWithSSA  = cWithSSA || (cChip->getFrontEndType() == FrontEndType::SSA);
	                cWithSSA2 = cWithSSA2 || (cChip->getFrontEndType() == FrontEndType::SSA2);
	            } // chips
	        }     // hybrids
	    }       

        uint32_t cIterations = 0;
        while(pData.size() == 0 and cIterations < 5)
        {
            if(cIterations > 0) LOG(INFO) << BOLDRED << "Retrying retreival of PS counter information.. Attempt#" << +cIterations << RESET;

            if(fPSCounterFast == 1)
            {
                LOG(DEBUG) << BOLDBLUE << "Reading PS Hit counters over stub lines..." << RESET;
                if( !pBoard->isOptical() ) this->ReadPSSCCountersFast(pBoard, pData);
                continue;
            }
            else
            {
            	pData.clear();
            	// first read back MPA counter information 
	            if(cWithMPA)  this->ReadMPACounters(pBoard, pData);
	            // then SSA counter information 
	            if(cWithSSA || cWithSSA)  this->ReadSSACounters(pBoard, pData);
            }
            cIterations++;
        }
	    return 1;
	}

	bool D19cPSCounterFWInterface::WaitForData(BeBoard* pBoard)
	{
	    LOG(INFO) << BOLDBLUE << "Waiting for data from the PS Counter FWInterface.... Attempt#" << fReadoutAttempts << RESET;
		bool cFailed        = false;
		auto cNevents       = this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept");
    	auto cTriggerSource = this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    	auto     cMultiplicity         = this->ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");

    	std::vector<std::pair<std::string, uint32_t>> cVecReg;
	    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents * (cMultiplicity + 1)});
	    // if(cEventType == EventType::PSAS)
    // {
    //     for(auto cOpticalGroup: *pBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             LOG(DEBUG) << BOLDBLUE << "Capturing RAW PS counter data in uDTC for hybrid#" << +cHybrid->getId() << RESET;
    //             // clear stub buffer
    //             fStubBuffer.clear();
    //             // count number of chips expected
    //             this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
    //             this->WriteReg("fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);

    //             // make sure uDTC vetos fast commands to CIC in this mode
    //             auto cVetoCIC = this->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto");
    //             this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.select_even", 0x1);

    //             uint8_t cTriggerForPS = 12;
    //             LOG(DEBUG) << BOLDMAGENTA << "CIC fast command VETO set to " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto") << RESET;
    //             LOG(DEBUG) << BOLDMAGENTA << "Injecting " << +cNevents << " times." << RESET;
    //             LOG(DEBUG) << BOLDMAGENTA << "Trigger multiplicity was set to " << +cMultiplicity << RESET;
    //             cVecReg.clear();

    //             cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
    //             cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNevents});
    //             cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    //             cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_timeout_enable", 0});
    //             cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerForPS});
    //             this->ReconfigureTriggerFSM(cVecReg);
    //             cVecReg.clear();

    //             std::vector<uint8_t> cFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5}; //  Index Hybrid FE Id , Value CIC FE Id
    //             std::vector<uint8_t> cFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2}; // Index hybrid FE Id , Value CIC FE Id
    //             std::vector<uint8_t> cMapping      = (cHybrid->getId() % 2 == 0) ? cFeMappingPSR : cFeMappingPSL;
    //             uint32_t             cPSModuleId   = (pBoard->getId() << 16) | (cOpticalGroup->getId() << 8) | cHybrid->getId();
    //             auto                 cPSModuleIter = fPSModulesCounterData.find(cPSModuleId);
    //             // bool cAllCountersReceived=false;
    //             // uint16_t cNominalOffset = 120*16*8-3*8+1;
    //             uint8_t cReadoutAttempt        = 0;
    //             bool    cCounterReadoutSuccess = false;
    //             uint8_t cMaxReadoutAttempts    = 5;
    //             do
    //             {
    //                 if(cPSModuleIter != fPSModulesCounterData.end())
    //                 {
    //                     PSCounterData cDummy;
    //                     cDummy.clear();
    //                     fPSModulesCounterData[cPSModuleId] = cDummy;
    //                 }

    //                 this->ResetTriggerFSM();
    //                 this->Stop();

    //                 this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", 0x1);
    //                 // configure DDR3 readout for counters
    //                 this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x1);
    //                 this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);
    //                 // configure raw mode
    //                 this->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 1);

    //                 size_t   cAttempt = 0;
    //                 uint32_t cStartReceived;
    //                 do
    //                 {
    //                     auto cTriggerState = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
    //                     std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    //                     LOG(DEBUG) << BOLDBLUE << "Attempt#" << +cAttempt << " Async SSA [trigger source == " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source")
    //                                << " ] [ number of injections is " << +this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept") << " ]"
    //                                << "Trigger state before start is " << +cTriggerState << RESET;

    //                     this->Start();
    //                     uint32_t cIteration = 0;
    //                     do
    //                     {
    //                         auto cNtriggers = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    //                         cTriggerState   = this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state");
    //                         LOG(DEBUG) << BOLDBLUE << "D19cFWInterface::WaitForData TriggerSource 12 Trigger State: " << +cTriggerState << "Running.. .Iteration#" << +cIteration << " ... received "
    //                                    << +cNtriggers << " triggers." << RESET;
    //                         std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    //                         cIteration++;
    //                     } while(cTriggerState && cIteration < 1000);
    //                     uint32_t cNInjections = ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
    //                     LOG(DEBUG) << BOLDBLUE << "Trigger state after end is " << +cTriggerState << " - fast command core counted " << cNInjections << " injections." << RESET;

    //                     cStartReceived         = this->ReadReg("fc7_daq_stat.physical_interface_block.async_counter_decode.received_start");
    //                     cCounterReadoutSuccess = (GetCounterData(1, cHybrid->getId(), 0));
    //                     if(!cCounterReadoutSuccess) LOG(DEBUG) << BOLDRED << "Counter readout failed.. to receive start pattern .. will try again..." << RESET;
    //                     cAttempt++;
    //                 } while(!cCounterReadoutSuccess && cAttempt < 10);
    //                 if(!cCounterReadoutSuccess)
    //                 {
    //                     LOG(INFO) << BOLDRED << "Failed to receive start pattern from Hybrid#" << +cHybrid->getId() << " after 10 attempts" << RESET;
    //                     throw Exception("Too many failures when attempting to receive start pattern from MPAs..something is wrong!");
    //                 }
    //                 else
    //                     LOG(DEBUG) << BOLDMAGENTA << "PS data capture block received start signal after " << +cStartReceived << " 40 MHz clock cycles."
    //                                << " [ readout attempt#" << +(cAttempt - 1) << " ]" << RESET;

    //                 // reconfigure original veto
    //                 this->WriteReg("fc7_daq_cnfg.fast_command_block.ps_async_en.cic_veto", cVetoCIC);
    //                 // disable DDR3 dump of counters
    //                 this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x0);
    //                 this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);

    //                 // decode raw counter data
    //                 // expected number of stubs
    //                 std::vector<uint8_t> pIds(0);
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     if(cChip->getFrontEndType() != FrontEndType::MPA) continue;
    //                     pIds.push_back(cMapping[cChip->getId() % 8]);
    //                     // cNFEs += (cChip->getFrontEndType()==FrontEndType::MPA)?1:0;
    //                 }
    //                 cPSModuleIter->second.clear();
    //                 cCounterReadoutSuccess = DecodeRawCounterDataPS(cPSModuleIter->second, pIds);
    //                 if(!cCounterReadoutSuccess) LOG(INFO) << BOLDRED << "Not all counters from all FEs have been received.. trying again.." << RESET;
    //                 cReadoutAttempt++;
    //             } while(!cCounterReadoutSuccess && cReadoutAttempt < cMaxReadoutAttempts);
    //             if(!cCounterReadoutSuccess)
    //             {
    //                 LOG(INFO) << BOLDRED << "Failed to receive all counters from Hybrid#" << +cHybrid->getId() << " after " << +(1 + cMaxReadoutAttempts) << " attempts" << RESET;
    //                 // throw Exception("Too many failures when attempting to read-back all PS counters from a hybrid..something is wrong!");
    //             }

    //             // now add caveat for MPA1/SSA 1
    //             for(auto& cFECounters: cPSModuleIter->second)
    //             {
    //                 auto    cFeId    = cFECounters.first;
    //                 uint8_t cFromSSA = (cFeId >> 3);
    //                 if(cFECounters.second.size() == 0) continue;

    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     auto& cIdCIC = cMapping[cChip->getId() % 8];
    //                     if(cFromSSA && cChip->getFrontEndType() == FrontEndType::MPA) continue;
    //                     if(!cFromSSA && cChip->getFrontEndType() == FrontEndType::SSA) continue;
    //                     if((cFeId & 0x7) != cIdCIC) continue;

    //                     // SSA1 last strip is missing from the fast counter readout
    //                     // MPA1 first pixel is 0 when read over the fast counter readout
    //                     // MPA1 last pixel is missing when read over the fast counter readout
    //                     std::vector<uint16_t> cChnls(0);
    //                     cChnls.push_back((cChip->getFrontEndType() == FrontEndType::MPA) ? 0 : 120 - 1);
    //                     if(cChip->getFrontEndType() == FrontEndType::MPA) cChnls.push_back(120 * 16 - 1);
    //                     std::vector<uint16_t> cCounterValues(0);
    //                     for(auto cChnl: cChnls)
    //                     {
    //                         int              cRowNumber       = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl / 120 : 1;
    //                         int              cPixelNumber     = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl % 120 : 0;
    //                         int              cBaseRegisterLSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
    //                         int              cBaseRegisterMSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
    //                         std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
    //                         std::vector<int> cValues(0);
    //                         for(auto cReg: cRegs)
    //                         {
    //                             ChipRegItem cReg_Counters_MSB;
    //                             cReg_Counters_MSB.fPage    = 0x00;
    //                             cReg_Counters_MSB.fAddress = cReg;
    //                             cValues.push_back(ReadFERegister(cChip, cReg));
    //                         }
    //                         cCounterValues.push_back(((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF));
    //                         LOG(DEBUG) << BOLDYELLOW << "Hit counter from Chnl#" << +cChnl << " read-back over I2C .. value is " << cCounterValues[cCounterValues.size() - 1] << RESET;
    //                     }

    //                     // MPA1 first pixel is 0 when read over the fast counter readout
    //                     // MPA1 last pixel is missing when read over the fast counter readout
    //                     if(cChip->getFrontEndType() == FrontEndType::MPA)
    //                     {
    //                         cFECounters.second[0] = cCounterValues[0];
    //                         cFECounters.second.push_back(cCounterValues[1]);
    //                         // LOG (INFO) << BOLDMAGENTA << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
    //                     }
    //                     // SSA1 last strip is missing from the fast counter readout
    //                     else
    //                     {
    //                         cFECounters.second.push_back(cCounterValues[0]);
    //                         LOG(DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters - last counter (I2C) is " << cCounterValues[0]
    //                                    << " - last counter stub lines is " << cFECounters.second[cFECounters.second.size() - 1] << RESET;
    //                     }
    //                     // LOG (INFO) << BOLDBLUE << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
    //                     // LOG (DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " Id in CIC should be " << +cIdCIC << " " << cFECounters.second.size() << " counters read back over stub lines" <<
    //                     // RESET;
    //                 }
    //             }
    //             // now check SSA counters
    //             bool   cCheckSSAs      = false;
    //             size_t cMaxSSACounters = NSSACHANNELS;
    //             for(auto& cFECounters: cPSModuleIter->second)
    //             {
    //                 auto    cFeId    = cFECounters.first & 0x7;
    //                 uint8_t cFromSSA = (cFECounters.first >> 3);
    //                 if(cFECounters.second.size() == 0) continue;

    //                 // only check SSAs
    //                 if(cFromSSA == 0) continue;
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     auto& cIdCIC = cMapping[cChip->getId() % 8];
    //                     if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
    //                     if(cFeId != cIdCIC) continue;

    //                     bool cReadI2C = cCheckSSAs || cFECounters.second.size() != cMaxSSACounters;
    //                     if(!cReadI2C) continue;

    //                     LOG(DEBUG) << BOLDYELLOW << "CIC FeId " << +cFeId << " SSA Id on hybrid" << +(cChip->getId() % 8) << " -- id from map " << +cIdCIC << RESET;
    //                     std::vector<float>                         cDifference(0);
    //                     std::vector<float>                         cI2C(0);
    //                     std::vector<float>                         cSLVS(0);
    //                     std::vector<std::pair<uint16_t, uint16_t>> cMissed(0);
    //                     for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
    //                     {
    //                         int                   cRowNumber       = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl / 120 : 1;
    //                         int                   cPixelNumber     = (cChip->getFrontEndType() == FrontEndType::MPA) ? 1 + cChnl % 120 : 0;
    //                         int                   cBaseRegisterLSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (9 << 7) | cPixelNumber) : 0x0901 + cChnl;
    //                         int                   cBaseRegisterMSB = (cChip->getFrontEndType() == FrontEndType::MPA) ? ((cRowNumber << 11) | (10 << 7) | cPixelNumber) : 0x0801 + cChnl;
    //                         std::vector<uint16_t> cI2CVals(0);
    //                         for(int cAttempt = 0; cAttempt < 1; cAttempt++)
    //                         {
    //                             std::vector<int> cRegs{cBaseRegisterMSB, cBaseRegisterLSB};
    //                             std::vector<int> cValues(0);
    //                             for(auto cReg: cRegs)
    //                             {
    //                                 ChipRegItem cReg_Counters_MSB;
    //                                 cReg_Counters_MSB.fPage    = 0x00;
    //                                 cReg_Counters_MSB.fAddress = cReg;
    //                                 cValues.push_back(ReadFERegister(cChip, cReg));
    //                             }
    //                             cI2CVals.push_back(((cValues[0] & 0xFF) << 8) | (cValues[1] & 0xFF));
    //                             if(cChnl == 0) LOG(DEBUG) << BOLDYELLOW << cI2CVals[cI2CVals.size() - 1] << RESET;
    //                         }
    //                         uint16_t cCounterI2C = cI2CVals[0];
    //                         cI2C.push_back(cCounterI2C);
    //                         cSLVS.push_back(cFECounters.second[cChnl]);
    //                         if(cFECounters.second[cChnl] < cNevents)
    //                         {
    //                             std::pair<uint16_t, uint16_t> cPr;
    //                             cPr.first  = cCounterI2C;
    //                             cPr.second = cFECounters.second[cChnl];
    //                             cMissed.push_back(cPr);
    //                         }
    //                         if(cCounterI2C != cFECounters.second[cChnl])
    //                         {
    //                             LOG(DEBUG) << BOLDRED << "SSA#" << +cChip->getId() << " Mismatch in counter#" << +cChnl << " : " << cFECounters.second[cChnl] << " , " << cCounterI2C << RESET;
    //                             cDifference.push_back(cCounterI2C - cFECounters.second[cChnl]);
    //                             cFECounters.second[cChnl] = cCounterI2C;
    //                         }
    //                         else
    //                         {
    //                             LOG(DEBUG) << BOLDGREEN << "SSA#" << +cChip->getId() << " Match in counter#" << +cChnl << " : " << cFECounters.second[cChnl] << " , " << cCounterI2C << RESET;
    //                         }
    //                     }
    //                     if(cDifference.size() > 0)
    //                     {
    //                         float cMeanMismatch = std::accumulate(cDifference.begin(), cDifference.end(), 0.) / cDifference.size();
    //                         float cI2CAvg       = std::accumulate(cI2C.begin(), cI2C.end(), 0.) / cI2C.size();
    //                         float cSLVSCAvg     = std::accumulate(cSLVS.begin(), cSLVS.end(), 0.) / cSLVS.size();
    //                         LOG(DEBUG) << BOLDRED << "\t\t..CIC FeId " << +cFeId << " SSA Id on hybrid" << +(cChip->getId() % 8) << " decoded " << cFECounters.second.size()
    //                                    << " counters from SLVS lines.."
    //                                    << " -- found " << +cDifference.size() << " mismatches between stub lines and I2C"
    //                                    << " mean mismatch [I2C - SLVS] " << cMeanMismatch << " mean I2C value " << cI2CAvg << " mean slvs value " << cSLVSCAvg
    //                                    << " number of times I've seen a count < Ninjected " << cMissed.size() << RESET;
    //                         for(auto cLowerVal: cMissed) LOG(DEBUG) << BOLDRED << "\t\t.. counted [I2C] " << cLowerVal.first << " [SLVS] " << cLowerVal.second << " injected " << cNevents << RESET;
    //                     }
    //                 }
    //             }

    //             // check that counter information is correct
    //             // bool cAllFound=true;
    //             for(auto cFECounters: cPSModuleIter->second)
    //             {
    //                 auto    cFeId    = cFECounters.first;
    //                 uint8_t cFromSSA = (cFeId >> 3);

    //                 // if( !cAllFound ) continue;
    //                 for(auto cChip: *cHybrid)
    //                 {
    //                     auto& cIdCIC = cMapping[cChip->getId() % 8];
    //                     if(cFromSSA && cChip->getFrontEndType() == FrontEndType::MPA) continue;
    //                     if(!cFromSSA && cChip->getFrontEndType() == FrontEndType::SSA) continue;
    //                     if((cFeId & 0x7) != cIdCIC) continue;

    //                     if(cChip->getFrontEndType() == FrontEndType::MPA)
    //                     {
    //                         LOG(DEBUG) << BOLDMAGENTA << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
    //                         // if( cFECounters.second.size() != cMaxMPACounters ) cAllFound = false;
    //                     }
    //                     // SSA1 last strip is missing from the fast counter readout
    //                     else
    //                     {
    //                         LOG(DEBUG) << BOLDYELLOW << "ROC#" << +cChip->getId() << " read-back " << cFECounters.second.size() << " hit counters." << RESET;
    //                         // if( cFECounters.second.size() != cMaxSSACounters ) cAllFound = false;
    //                     }
    //                 }
    //             }

    //             // configure DDR3 readout for counters
    //             this->WriteReg("fc7_daq_cnfg.ddr3_debug.ps_async_counter_enable", 0x0);
    //             this->WriteReg("fc7_daq_cnfg.ddr3_debug.stub_enable", 0x0);
    //             // configure raw mode
    //             this->WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", 0);
    //         }
    //     }
    // }
		if( cTriggerSource == 10 ) // trigger source for injection with antenna 
    	{
    		LOG(INFO) << BOLDBLUE << "D19cPSCounterFWInterface::Async SSA [trigger source == 10]" << RESET;
	        this->ReconfigureTriggerFSM(cVecReg);
	        // resync + clear counters
	        this->PS_Clear_counters();
	        // start triggers
	        this->Start();
	        uint32_t cIterations = 0;
	        do
	        {
	            LOG(DEBUG) << "Trigger State: " << BOLDGREEN << "Running" << RESET;
	            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
	            cIterations++;
	        } while(this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") && cIterations < 10);
	        cFailed = (this->ReadReg("fc7_daq_stat.fast_command_block.general.fsm_state") || cIterations == 10);
	        this->PS_Close_shutter();
	        this->Stop();
	        cVecReg.clear();
    	}
    	else if( cTriggerSource == 12 ) // trigger soruce for fast counters .. need to fill this out 
    	{
    		//TO-DO.. fill out 
    	}
	    return cFailed;
	}
	void D19cPSCounterFWInterface::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
	{
		auto cNtriggersToAccept = this->ReadReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept");
	    // write number of triggers to accept
	    this->WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", pNEvents);
	    LOG(INFO) << BOLDMAGENTA << "D19cPSCounterFWInterface::ReadNEvents asking for " << +pNEvents << " events "
	               << " number of triggers to accept is currently " << +cNtriggersToAccept << RESET;
	    bool cFailed = WaitForData(pBoard);
	    if(!cFailed)
	    {
	        LOG(INFO) << BOLDMAGENTA << "D19cPSCounterFWInterface::ReadNEvents WaitForData Succeeded now going to try and GetData" << RESET;
	        // if trigger multiplicity is not 0 check
	        auto cNevents      = this->GetData(pBoard, pData);
	        LOG(INFO) << BOLDYELLOW << "D19cPSCounterFWInterface::ReadNEvents GetData returned " << cNevents << " and " << pData.size() << " 32-bit words" << RESET;
	        if(cNevents == 0 || pData.size() == 0 ) 
	        {
	            if(fReadoutAttempts < 10)
	            {
	                fReadoutAttempts++;
	                LOG(INFO) << BOLDRED << "D19cPSCounterFWInterface::ReadNEvents Failed to read back correct number of words from FC7.. Will try again" << RESET;
	                this->ReadNEvents(pBoard, pNEvents, pData);
	            }
	            else
	            {
	                LOG(INFO) << BOLDRED << "After " << +fReadoutAttempts << " attempts at reading out data .. I'm giving up! " << RESET;
	                throw Exception("Too many failures when attempting to read data from the FC7..somethign is wrong!");
	            }
	        }
	        WriteReg("fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNtriggersToAccept);
	        // fDDR3Offset = 0;
	    }
	    // again check if failed to re-run in case
	    else if(fReadoutAttempts < 10)
	    {
	        LOG(INFO) << BOLDRED << "Failed to readout all events..... Retrying..." << RESET;
			pData.clear();
	        fReadoutAttempts++;
	        // try again
	        this->ReadNEvents(pBoard, pNEvents, pData);
	    }
	    else
	    {
	        LOG(INFO) << BOLDRED << "After " << +fReadoutAttempts << " attempts at reading out data .. I'm giving up! " << RESET;
	        throw Exception("Too many failures when attempting to read data from the FC7..somethign is wrong!");
	    }
	    // reset readout attempts
	    fReadoutAttempts = 0;
	}
	// void D19cPSCounterFWInterface::ReadASEvent(BeBoard* pBoard, std::vector<uint32_t>& pData)
	// {
	//     uint32_t raw_mode_en = 0;
	//     WriteReg("fc7_daq_cnfg.physical_interface_block.ps_counters_raw_en", raw_mode_en);
	//     uint32_t ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
	//     // ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");
	//     // std::cout<<"ps_counters_ready "<<ps_counters_ready<<std::endl;

	//     std::chrono::milliseconds cWait(10);

	//     uint32_t chans = 0;

	//     for(auto cOpticalGroup: *pBoard)
	//     {
	//         for(auto cHybrid: *cOpticalGroup)
	//         {
	//             if(fFirmwareFrontEndType == FrontEndType::SSA) chans += NSSACHANNELS * cHybrid->size();
	//             if(fFirmwareFrontEndType == FrontEndType::MPA) chans += NMPACHANNELS * cHybrid->size();
	//         }
	//     }

	//     std::vector<uint32_t> count(chans, 0);

	//     std::vector<std::pair<std::string, uint32_t>> cVecReg;
	//     // cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.fast_reset", 1});
	//     // cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.fast_orbit_reset", 1});
	//     this->WriteStackReg(cVecReg);

	//     PS_Start_counters_read();
	//     // ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

	//     // std::cout<<"ps_counters_ready "<<ps_counters_ready<<std::endl;

	//     uint32_t timeout = 0;

	//     while((ps_counters_ready == 0) & (timeout < 50))
	//     {
	//         std::this_thread::sleep_for(cWait);
	//         ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

	//         timeout += 1;
	//     }
	//     if(timeout >= 50)
	//     {
	//         std::cout << "fail" << std::endl;
	//         return;
	//     }

	//     if(raw_mode_en == 1)
	//     {
	//         uint32_t cycle = 0;
	//         for(int i = 0; i < 20000; i++)
	//         {
	//             uint32_t fifo1_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo1_data");
	//             uint32_t fifo2_word = ReadReg("fc7_daq_ctrl.physical_interface_block.fifo2_data");

	//             uint32_t line1 = (fifo1_word & 0x0000FF) >> 0;  // to_number(fifo1_word,8,0)
	//             uint32_t line2 = (fifo1_word & 0x00FF00) >> 8;  // to_number(fifo1_word,16,8)
	//             uint32_t line3 = (fifo1_word & 0xFF0000) >> 16; //  to_number(fifo1_word,24,16)

	//             uint32_t line4 = (fifo2_word & 0x0000FF) >> 0; // to_number(fifo2_word,8,0)
	//             uint32_t line5 = (fifo2_word & 0x00FF00) >> 8; // to_number(fifo2_word,16,8)

	//             if(((line1 & 0x80) == 128) && ((line4 & 0x80) == 128))
	//             {
	//                 uint32_t temp = ((line2 & 0x20) << 9) | ((line3 & 0x20) << 8) | ((line4 & 0x20) << 7) | ((line5 & 0x20) << 6) | ((line1 & 0x10) << 6) | ((line2 & 0x10) << 5) | ((line3 & 0x10) << 4) |
	//                                 ((line4 & 0x10) << 3) | ((line5 & 0x80) >> 1) | ((line1 & 0x40) >> 1) | ((line2 & 0x40) >> 2) | ((line3 & 0x40) >> 3) | ((line4 & 0x40) >> 4) | ((line5 & 0x40) >> 5) |
	//                                 ((line1 & 0x20) >> 5);
	//                 // LOG (INFO) << BOLDBLUE <<"temp "<<temp - 1 << RESET;
	//                 if(temp != 0)
	//                 {
	//                     count[cycle] = temp - 1;

	//                     cycle += 1;
	//                 }
	//             }
	//         }
	//     }
	//     else
	//     {
	//         pData = ReadBlockRegValue("fc7_daq_ctrl.physical_interface_block.fifo2_data", chans);
	//     }
	//     std::this_thread::sleep_for(cWait);
	//     ps_counters_ready = ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.ps_counters_ready");

	//     if(fSaveToFile) fFileHandler->setData(pData);
	// }
	// bool D19cPSCounterFWInterface::DecodeRawCounterDataPS(PSCounterData& pFeCounters, std::vector<uint8_t> pIds)
	// {
	//     std::string                                   cStartPattern = "111111111111111";
	//     size_t                                        cBxId         = 0;
	//     std::vector<std::pair<uint32_t, std::string>> cBxCars;
	//     auto                                          cStubBufferIter = fStubBuffer.begin();
	//     std::string                                   cStubPkt        = "";
	//     size_t                                        cPktLength      = 0;
	//     size_t                                        cStubPktCounter = 0;
	//     size_t                                        cNFEs           = 0;
	//     // find first packet with more than 0 stubs
	//     bool                       cStartPatternFound = true;
	//     size_t                     cNcountersDecoded  = 0;
	//     size_t                     cMaxSSACounters    = NSSACHANNELS - 1;
	//     size_t                     cMaxMPACounters    = NMPACOLS * NSSACHANNELS - 1;
	//     size_t                     cMaxCountersSize   = (cMaxSSACounters + cMaxMPACounters) * pIds.size();
	//     std::map<uint8_t, uint8_t> cMPAdoneMap;
	//     std::map<uint8_t, uint8_t> cSSAdoneMap;
	//     for(auto cId: pIds)
	//     {
	//         cMPAdoneMap[cId] = 0;
	//         cSSAdoneMap[cId] = 0;
	//     }
	//     bool cStubZeroFound = false;
	//     do
	//     {
	//         // if( cStubPktCounter == 1 ) cMaxCountersSize = ( cNFEs == 0 ) ? (16*120 + 120 ) *8 : (16*120 + 120 ) *cNFEs;
	//         for(size_t cClk = 0; cClk < 8; cClk++)
	//         {
	//             LOG(DEBUG) << BOLDMAGENTA << "Bx" << +cBxId << " : " << std::bitset<6>(*cStubBufferIter & 0x3F) << RESET;
	//             if(((*cStubBufferIter & 0x3F) >> 5) == 1 || cPktLength > 0) // configuration bit is 1
	//             {
	//                 std::stringstream cStream;
	//                 cStream << std::bitset<6>(*cStubBufferIter & 0x3F);
	//                 cStubPkt += cStream.str();
	//                 cPktLength += 6;
	//             }

	//             if(cPktLength == 6 * 8 * 8)
	//             {
	//                 // std::pair<uint32_t,std::string> cBxCar;
	//                 // cBxCar.first = *( cBxCounter.begin()  + std::distance( cStubBuffer.begin(), cStubBufferIter) ) ;
	//                 // cBxCar.second = cStubPkt;
	//                 std::vector<uint8_t>                         cSizes{1, 9, 12, 6}; // Cnfg, Status, BxId, Nstubs
	//                 std::vector<std::pair<std::string, uint8_t>> cHdrFlds;
	//                 cHdrFlds.push_back(std::make_pair("Cnfg", 1));
	//                 cHdrFlds.push_back(std::make_pair("Status", 9));
	//                 cHdrFlds.push_back(std::make_pair("BxId", 12));
	//                 cHdrFlds.push_back(std::make_pair("Nstbs", 6));
	//                 size_t                   cShft = 0;
	//                 std::stringstream        cStream;
	//                 std::vector<std::string> cHdrVals;
	//                 for(auto cFld: cHdrFlds)
	//                 {
	//                     auto cSubStr = cStubPkt.substr(cShft, cFld.second);
	//                     cHdrVals.push_back(cSubStr);
	//                     if(cFld.first == "BxId" || cFld.first == "Nstbs") { cStream << BOLDYELLOW << "\t" << cFld.first << "=" << std::stoi(cSubStr, 0, 2) << "\t"; }
	//                     else
	//                         cStream << BOLDYELLOW << "\t" << cFld.first << "=" << cSubStr << "\t";
	//                     cShft += cFld.second;
	//                 }
	//                 size_t cNstubs = std::stoi(cHdrVals[3], 0, 2);
	//                 // if( cNstubs < 8 ) //there should be at most 8 stubs here
	//                 // if( cNstubs >= pIds.size() && cNstubs < 8 ) //there should be at most 8 stubs here
	//                 //{
	//                 LOG(DEBUG) << BOLDYELLOW << cStream.str() << " : " << RESET;
	//                 for(size_t cStubId = 0; cStubId < cNstubs; cStubId++)
	//                 {
	//                     std::vector<std::pair<std::string, uint8_t>> cStubFlds;
	//                     cStubFlds.push_back(std::make_pair("Offset", 3));
	//                     cStubFlds.push_back(std::make_pair("FeId", 3));
	//                     cStubFlds.push_back(std::make_pair("Stub", 15));
	//                     size_t cMinPktLength = 3 + 3 + 15;
	//                     if(cStubPkt.length() < cMinPktLength)
	//                     {
	//                         LOG(DEBUG) << BOLDMAGENTA << "!!" << cStubPkt.length() << " -- " << cMinPktLength << RESET;
	//                         continue;
	//                     }
	//                     std::stringstream cStubOutput;
	//                     int               cFeId        = -1;
	//                     int               cDecodedFeId = cFeId;
	//                     for(auto cFld: cStubFlds)
	//                     {
	//                         if(cStubZeroFound) continue;
	//                         auto cSubStr = cStubPkt.substr(cShft, cFld.second);
	//                         LOG(DEBUG) << BOLDYELLOW << cFld.first << ":" << cSubStr << RESET;
	//                         if(cFld.first == "FeId")
	//                         {
	//                             cDecodedFeId = std::stoi(cSubStr, 0, 2);
	//                             cFeId        = cDecodedFeId;
	//                         }
	//                         if(cFld.first == "Stub" && cSubStr != "000000000000000")
	//                         {
	//                             if(cStubPktCounter == 0)
	//                             {
	//                                 cStartPatternFound = cStartPatternFound && (cSubStr == cStartPattern); // first stub needs to be all 1's
	//                                 cNFEs++;
	//                             }
	//                             if(cStartPatternFound && cStubPktCounter > 0)
	//                             {
	//                                 // if( std::find( pIds.begin(), pIds.end() , cDecodedFeId ) == pIds.end() ) continue;
	//                                 uint16_t cCounterValue = std::stoi(cSubStr.substr(8, 6) + cSubStr.substr(0, 7), 0, 2) - 1;
	//                                 auto     cFindCounters = pFeCounters.find(cFeId);
	//                                 if(cFindCounters == pFeCounters.end()) // for the MPA
	//                                 {
	//                                     std::vector<uint16_t> cCountersThisFe;
	//                                     cCountersThisFe.clear();
	//                                     pFeCounters[cFeId] = cCountersThisFe;
	//                                 }
	//                                 else
	//                                 {
	//                                     // MPA done .. can prepare SSA
	//                                     if(cMPAdoneMap[cDecodedFeId] == 1)
	//                                     {
	//                                         cFeId         = cDecodedFeId | (1 << 3);
	//                                         cFindCounters = pFeCounters.find(cFeId);
	//                                         if(cFindCounters == pFeCounters.end()) // for the MPA
	//                                         {
	//                                             LOG(DEBUG) << BOLDYELLOW << "Starting to fill SSA counters from FeId" << +cDecodedFeId << RESET;
	//                                             std::vector<uint16_t> cCountersThisFe;
	//                                             cCountersThisFe.clear();
	//                                             pFeCounters[cFeId] = cCountersThisFe;
	//                                         }
	//                                     }
	//                                 }
	//                                 uint8_t cWithMPA = (cFeId >> 3 == 0);
	//                                 if(cWithMPA == 1 && cMPAdoneMap[cDecodedFeId] == 0)
	//                                 {
	//                                     // if( pFeCounters[cFeId].size() == 0 || pFeCounters[cFeId].size() == cMaxMPACounters-1)
	//                                     if(pFeCounters[cFeId].size() % 75 == 0 && pFeCounters[cFeId].size() > 0)
	//                                         LOG(DEBUG) << BOLDMAGENTA << "MPA [FeId " << +cFeId << " ] counter#" << +pFeCounters[cFeId].size() << " --> " << cCounterValue << RESET;
	//                                     pFeCounters[cFeId].push_back(cCounterValue);
	//                                     if(pFeCounters[cFeId].size() == cMaxMPACounters) cMPAdoneMap[cDecodedFeId] = 1;
	//                                 }

	//                                 if(cWithMPA == 0 && cMPAdoneMap[cDecodedFeId] == 1 && cSSAdoneMap[cDecodedFeId] == 0)
	//                                 {
	//                                     // if( pFeCounters[cFeId].size() == 0 )//|| pFeCounters[cFeId].size() == cMaxSSACounters-1)
	//                                     if(pFeCounters[cFeId].size() % 10 == 0)
	//                                         LOG(DEBUG) << BOLDYELLOW << "SSA [FeId " << +cFeId << " ] counter#" << +pFeCounters[cFeId].size() << " --> " << cCounterValue << RESET;
	//                                     pFeCounters[cFeId].push_back(cCounterValue);
	//                                     if(pFeCounters[cFeId].size() == cMaxSSACounters) cSSAdoneMap[cDecodedFeId] = 1;
	//                                 }
	//                             }
	//                         }
	//                         if(cFld.first == "Stub" && cSubStr == "000000000000000")
	//                         {
	//                             cStubZeroFound = true;
	//                             LOG(DEBUG) << BOLDRED << "Found a stub from MPA that is all 00s.. this should not happen and is a decoder problem.. will look into it!!" << RESET;
	//                         }
	//                         cShft += cFld.second;
	//                     }
	//                     if(cStubZeroFound) continue;
	//                 }
	//                 //}
	//                 cStubPktCounter += (cNstubs > 0) ? 1 : 0;
	//                 cPktLength = 0;
	//                 cStubPkt   = "";
	//             }
	//             cStubBufferIter++;
	//         }
	//         // check how many counters have been decoded
	//         cNcountersDecoded = 0;
	//         for(auto cId: pIds)
	//         {
	//             // auto cFindCounters = pFeCounters.find(cId);
	//             // if( cFindCounters == pFeCounters.end() ) continue;

	//             cNcountersDecoded += pFeCounters[cId].size();
	//         }
	//         // expect this to increment every 8 Bx?
	//         LOG(DEBUG) << BOLDYELLOW << "Decoded " << cNcountersDecoded << " counters from enabled FEs" << RESET;
	//         cBxId++;
	//     } while(cStubBufferIter < fStubBuffer.end() && cNcountersDecoded < cMaxCountersSize && !cStubZeroFound);

	//     bool cAllCountersReceived = true;
	//     for(auto cId: pIds)
	//     {
	//         auto cFindCounters = pFeCounters.find(cId);
	//         if(cFindCounters == pFeCounters.end())
	//         {
	//             cAllCountersReceived = false;
	//             continue;
	//         }

	//         auto& cCounters_MPA = pFeCounters[cId];
	//         auto& cCounters_SSA = pFeCounters[(1 << 3) | cId];

	//         LOG(DEBUG) << BOLDMAGENTA << "Mean hit count - MPA#" << +cId << " : " << std::accumulate(cCounters_MPA.begin(), cCounters_MPA.end(), 0.) / cCounters_MPA.size() << RESET;
	//         LOG(DEBUG) << BOLDYELLOW << "Mean hit count - SSA#" << +cId << " : " << std::accumulate(cCounters_SSA.begin(), cCounters_SSA.end(), 0.) / cCounters_SSA.size() << RESET;
	//         // if( pFeCounters[(1<<3)|cId].size() != cMaxSSACounters )
	//         //{
	//         LOG(DEBUG) << BOLDYELLOW << "FECounters from FeId" << +cId << " --  from MPA " << pFeCounters[cId].size() << " --  from SAA " << pFeCounters[(1 << 3) | cId].size() << RESET;
	//         //}
	//         cAllCountersReceived = cAllCountersReceived && (cMPAdoneMap[cId] == 1 && cSSAdoneMap[cId] == 1);
	//     }
	//     return cStartPatternFound && cAllCountersReceived;
	// }
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
	                        LOG(DEBUG) << BOLDGREEN << "D19cPSCounterFWInterface::CheckStartPattern CheckForStartPattern from PS counters - Bx " << std::stoi(cHdrVals[2], 0, 2) << "\t stub#" << +cStubId << " : "
	                                   << cStubOutput.str() << RESET;
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
}