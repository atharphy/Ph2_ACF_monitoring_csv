#include "D19cElectricalSlowControlFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cElectricalSlowControlFWInterface::D19cElectricalSlowControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler)
    : BeBoardFWInterface(puHalConfigFileName, pBoardId)
{
}
D19cElectricalSlowControlFWInterface::D19cElectricalSlowControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler)
    : BeBoardFWInterface(pId, pUri, pAddressTable)
{
}
D19cElectricalSlowControlFWInterface::~D19cElectricalSlowControlFWInterface() {}

void D19cElectricalSlowControlFWInterface::EncodeReg(const ChipRegItem& pRegItem, Chip* pChip, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    // need to check how this should be done .. wait
    // uint8_t pCbcId       = pChip->getId() % 8;
    // uint8_t pFeId        = pChip->getHybridId();
    // auto    cMapIterator = fI2CSlaveMap.find(pCbcId);
    // bool    cFound       = (cMapIterator != fI2CSlaveMap.end());
    // if(!cFound)
    // {
    //     uint8_t cNBytes     = (pChip->getFrontEndType() == FrontEndType::SSA2 || pChip->getFrontEndType() == FrontEndType::SSA || pChip->getFrontEndType() == FrontEndType::MPA) ? 2 : 1;
    //     uint8_t cLastValue = 1;
    //     std::vector<uint32_t> cI2CSlaveDescription    = {pChip->getChipAddress(), cNBytes, 1, 1, 1, cLastValue};
    //     LOG(INFO) << BOLDBLUE << "Adding chip with address " << +pChip->getChipAddress() << " to I2C slave map.." << RESET;
    //     fI2CSlaveMap[pChip->getId()] = cI2CSlaveDescription;
    //     // update I2C slave map
    //     for(auto cIterator = fI2CSlaveMap.begin(); cIterator != fI2CSlaveMap.end(); cIterator++)
    //     {
    //         // auto cChipId = cIterator->first;
    //         auto cDescription = cIterator->second;
    //         // setting the params
    //         uint32_t shifted_i2c_address             = (cDescription[0]) << 25;
    //         uint32_t shifted_register_address_nbytes = cDescription[1] << 10;
    //         uint32_t shifted_data_wr_nbytes          = cDescription[2] << 5;
    //         uint32_t shifted_data_rd_nbytes          = cDescription[3] << 0;
    //         uint32_t shifted_stop_for_rd_en          = cDescription[4] << 24;
    //         uint32_t shifted_nack_en                 = cDescription[5] << 23;

    //         // writing the item to the firmware
    //         if(fFirmwareFrontEndType != FrontEndType::CBC3)
    //         {
    //             uint32_t    final_item = shifted_i2c_address + shifted_register_address_nbytes + shifted_data_wr_nbytes + shifted_data_rd_nbytes + shifted_stop_for_rd_en + shifted_nack_en;
    //             std::string curreg     = "fc7_daq_cnfg.command_processor_block.i2c_address_table.slave_" + std::to_string(std::distance(fI2CSlaveMap.begin(), cIterator)) + "_config";
    //             LOG(INFO) << BOLDMAGENTA << "Writing " << std::bitset<32>(final_item) << " to register " << curreg << RESET;
    //             this->WriteReg(curreg, final_item);
    //         }
    //     }
    // }
    // cFound       = (cMapIterator != fI2CSlaveMap.end());
    // if(cFound)
    // {
    //     // remember .. encoded command the chip id is .. the index and not the id!!
    //     uint8_t pIndex = std::distance(fI2CSlaveMap.begin(), cMapIterator);
    //     // LOG(INFO) << BOLDGREEN << "Encoding register from chip " << +pCbcId << " which is index " << +pIndex << " in I2C map " << RESET;
    //     // use fBroadcastCBCId for broadcast commands
    //     bool pUseMask = false;
    //     if(ReadReg("fc7_daq_stat.command_processor_block.i2c.master_version") >= 1)
    //     {
    //         // new command consists of one word if its read command, and of two words if its write. first word is always
    //         // the same
    //         pVecReq.push_back((0 << 28) | (0 << 27) | (pFeId << 23) | (pIndex << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0));
    //         // only for write commands
    //         if(pWrite) pVecReq.push_back((0 << 28) | (pWrite << 27) | (pRegItem.fValue << 0));
    //     }
    //     else
    //     {
    //         pVecReq.push_back((0 << 28) | (pFeId << 24) | (pCbcId << 20) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) |
    //                           pRegItem.fValue);
    //     }
    // }
    // else
    // {
    //     LOG(INFO) << BOLDRED << "Could not find address in I2C map.. " << RESET;
    // }
}
void D19cElectricalSlowControlFWInterface::DecodeReg(ChipRegItem& pRegItem, uint8_t& pCbcId, uint32_t pWord, bool& pRead, bool& pFailed)
{
    if(ReadReg("fc7_daq_stat.command_processor_block.i2c.master_version") >= 1)
    {
        // pFeId    =  ( ( pWord & 0x07800000 ) >> 27) ;
        pCbcId            = ((pWord & 0x007c0000) >> 22);
        pFailed           = 0;
        pRegItem.fPage    = 0;
        pRead             = true;
        pRegItem.fAddress = (pWord & 0x0000FF00) >> 8;
        pRegItem.fValue   = (pWord & 0x000000FF);
    }
    else
    {
        // pFeId    =  ( ( pWord & 0x00f00000 ) >> 24) ;
        pCbcId            = ((pWord & 0x00f00000) >> 20);
        pFailed           = 0;
        pRegItem.fPage    = ((pWord & 0x00020000) >> 17);
        pRead             = (pWord & 0x00010000) >> 16;
        pRegItem.fAddress = (pWord & 0x0000FF00) >> 8;
        pRegItem.fValue   = (pWord & 0x000000FF);
    }
}

void D19cElectricalSlowControlFWInterface::BCEncodeReg(const ChipRegItem& pRegItem, uint8_t pNCbc, std::vector<uint32_t>& pVecReq, bool pReadBack, bool pWrite)
{
    // use fBroadcastCBCId for broadcast commands
    bool pUseMask = false;
    pVecReq.push_back((2 << 28) | (pReadBack << 19) | (pUseMask << 18) | ((pRegItem.fPage) << 17) | ((!pWrite) << 16) | (pRegItem.fAddress << 8) | pRegItem.fValue);
}

bool D19cElectricalSlowControlFWInterface::WriteChipBlockReg(std::vector<uint32_t>& pVecReg, uint8_t& pWriteAttempts, bool pReadback)
{
    uint8_t cMaxWriteAttempts = 5;
    // the actual write & readback command is in the vector
    std::vector<uint32_t> cReplies;
    bool                  cSuccess = !WriteI2C(pVecReg, cReplies, pReadback, false);

    // here make a distinction: if pReadback is true, compare only the read replies using the binary predicate
    // else, just check that info is 0 and thus the CBC acqnowledged the command if the writeread is 0
    std::vector<uint32_t> cWriteAgain;

    if(pReadback)
    {
        // now use the Template from BeBoardFWInterface to return a vector with all written words that have been read
        // back incorrectly
        cWriteAgain = get_mismatches(pVecReg.begin(), pVecReg.end(), cReplies.begin(), cmd_reply_comp);

        // now clear the initial cmd Vec and set the read-back
        pVecReg.clear();
        pVecReg = cReplies;
    }
    else
    {
        // since I do not read back, I can safely just check that the info bit of the reply is 0 and that it was an
        // actual write reply then i put the replies in pVecReg so I can decode later in CBCInterface cWriteAgain =
        // get_mismatches (pVecReg.begin(), pVecReg.end(), cReplies.begin(), D19cFWInterface::cmd_reply_ack);
        pVecReg.clear();
        pVecReg = cReplies;
    }

    // now check the size of the WriteAgain vector and assert Success or not
    // also check that the number of write attempts does not exceed cMaxWriteAttempts
    if(cWriteAgain.empty())
        cSuccess = true;
    else
    {
        cSuccess = false;

        // if the number of errors is greater than 100, give up
        if(cWriteAgain.size() < 100 && pWriteAttempts < cMaxWriteAttempts)
        {
            if(pReadback)
                LOG(INFO) << BOLDRED << "(WRITE#" << std::to_string(pWriteAttempts) << ") There were " << cWriteAgain.size() << " Readback Errors -trying again!" << RESET;
            else
                LOG(INFO) << BOLDRED << "(WRITE#" << std::to_string(pWriteAttempts) << ") There were " << cWriteAgain.size() << " CBC CMD acknowledge bits missing -trying again!" << RESET;

            pWriteAttempts++;
            this->WriteChipBlockReg(cWriteAgain, pWriteAttempts, true);
        }
        else if(pWriteAttempts >= cMaxWriteAttempts)
        {
            cSuccess       = false;
            pWriteAttempts = 0;
        }
        else
            throw Exception("Too many CBC readback errors - no functional I2C communication. Check the Setup");
    }

    return cSuccess;
}

bool D19cElectricalSlowControlFWInterface::BCWriteChipBlockReg(std::vector<uint32_t>& pVecReg, bool pReadback)
{
    // std::lock_guard<std::recursive_mutex> theGuard(fMutex);

    std::vector<uint32_t> cReplies;
    bool                  cSuccess = !WriteI2C(pVecReg, cReplies, false, true);

    // just as above, I can check the replies - there will be NCbc * pVecReg.size() write replies and also read replies
    // if I chose to enable readback this needs to be adapted
    if(pReadback)
    {
        // TODO: actually, i just need to check the read write and the info bit in each reply - if all info bits are 0,
        // this is as good as it gets, else collect the replies that faild for decoding - potentially no iterative
        // retrying
        // TODO: maybe I can do something with readback here - think about it
        for(auto& cWord: cReplies)
        {
            // it was a write transaction!
            if(((cWord >> 16) & 0x1) == 0)
            {
                // infor bit is 0 which means that the transaction was acknowledged by the CBC
                // if ( ( (cWord >> 20) & 0x1) == 0)
                cSuccess = true;
                // else cSuccess == false;
            }
            else
                cSuccess = false;

            // LOG(INFO) << std::bitset<32>(cWord) ;
        }

        // cWriteAgain = get_mismatches (pVecReg.begin(), pVecReg.end(), cReplies.begin(),
        // Cbc3Fc7FWInterface::cmd_reply_ack);
        pVecReg.clear();
        pVecReg = cReplies;
    }

    return cSuccess;
}

bool D19cElectricalSlowControlFWInterface::WriteI2C(std::vector<uint32_t>& pVecSend, std::vector<uint32_t>& pReplies, bool pReadback, bool pBroadcast)
{
    // std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    bool cFailed(false);
    // reset the I2C controller
    WriteReg("fc7_daq_ctrl.command_processor_block.i2c.control.reset_fifos", 0x1);
    // usleep (10);
    try
    {
        WriteBlockReg("fc7_daq_ctrl.command_processor_block.i2c.command_fifo", pVecSend);
    }
    catch(Exception& except)
    {
        throw except;
    }

    uint32_t cNReplies      = 0;
    size_t   cNReadoutChips = 1;
    if(pBroadcast)
    {
        uint32_t cHybridEnableReg = this->ReadReg("fc7_daq_cnfg.global.hybrid_enable");
        for(size_t cIndx = 0; cIndx < 32; cIndx++)
        {
            if((cHybridEnableReg & (0x1 >> cIndx)) == 1)
            {
                char name[50];
                std::sprintf(name, "fc7_daq_cnfg.global.chips_enable_hyb_%02d", (int)cIndx);
                std::string name_str(name);
                uint8_t     cReadoutChipEnReg = this->ReadReg(name_str);
                for(size_t cIndx2 = 0; cIndx2 < 8; cIndx2++)
                {
                    if((cReadoutChipEnReg & (0x1 >> cIndx2)) == 1) cNReadoutChips++;
                }
            }
        }
    }
    for(auto word: pVecSend)
    {
        // if read or readback for write == 1, then count
        if(ReadReg("fc7_daq_stat.command_processor_block.i2c.master_version") >= 1)
        {
            // uint32_t cWord = (pLinkId << 29) | (0 << 28) | (0 << 27) | (pFeId << 23) | (pCbcId << 18) | (pReadBack << 17) | ((!pWrite) << 16) | (pRegItem.fPage << 8) | (pRegItem.fAddress << 0);
            if((((word & 0x08000000) >> 27) == 0) && ((((word & 0x00010000) >> 16) == 1) or (((word & 0x00020000) >> 17) == 1))) { cNReplies += cNReadoutChips; }
        }
        else
        {
            if((((word & 0x00010000) >> 16) == 1) or (((word & 0x00080000) >> 19) == 1)) { cNReplies += cNReadoutChips; }
        }
    }
    // std::this_thread::sleep_for (std::chrono::microseconds (fWait_us) );
    // usleep (20);
    cFailed = ReadI2C(cNReplies, pReplies);

    return cFailed;
}

bool D19cElectricalSlowControlFWInterface::ReadI2C(uint32_t pNReplies, std::vector<uint32_t>& pReplies)
{
    bool cFailed(false);

    uint32_t single_WaitingTime = SINGLE_I2C_WAIT * pNReplies;
    uint32_t max_Attempts       = 100;
    uint32_t counter_Attempts   = 0;

    // read the number of received replies from ndata and use this number to compare with the number of expected replies
    // and to read this number 32-bit words from the reply FIFO
    uint32_t cNReplies = 0;
    while(cNReplies != pNReplies)
    {
        std::this_thread::sleep_for(std::chrono::microseconds(single_WaitingTime));
        cNReplies = ReadReg("fc7_daq_stat.command_processor_block.i2c.nreplies");

        if(counter_Attempts > max_Attempts)
        {
            LOG(INFO) << "Error: Read " << cNReplies << " I2C replies whereas " << pNReplies << " are expected!";
            ReadErrors();
            cFailed = true;
            break;
        }
        counter_Attempts++;
    }

    try
    {
        pReplies = ReadBlockRegValue("fc7_daq_ctrl.command_processor_block.i2c.reply_fifo", cNReplies);
    }
    catch(Exception& except)
    {
        throw except;
    }

    // reset the i2c controller here?
    return cFailed;
}
void D19cElectricalSlowControlFWInterface::ChipI2CRefresh()
{
    // std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    WriteReg("fc7_daq_ctrl.fast_command_block.control.fast_i2c_refresh", 0x1);
}

bool D19cElectricalSlowControlFWInterface::cmd_reply_comp(const uint32_t& cWord1, const uint32_t& cWord2) { return true; }

bool D19cElectricalSlowControlFWInterface::cmd_reply_ack(const uint32_t& cWord1, const uint32_t& cWord2)
{
    // if it was a write transaction (>>17 == 0) and
    // the CBC id matches it is false
    if(((cWord2 >> 16) & 0x1) == 0 && (cWord1 & 0x00F00000) == (cWord2 & 0x00F00000))
        return true;
    else
        return false;
}
void D19cElectricalSlowControlFWInterface::ReadErrors()
{
    int error_counter = ReadReg("fc7_daq_stat.general.global_error.counter");

    if(error_counter == 0)
        LOG(INFO) << "No Errors detected";
    else
    {
        std::vector<uint32_t> pErrors = ReadBlockRegValue("fc7_daq_stat.general.global_error.full_error", error_counter);

        for(auto& cError: pErrors)
        {
            int error_block_id = (cError & 0x0000000f);
            int error_code     = ((cError & 0x00000ff0) >> 4);
            LOG(ERROR) << "Block: " << BOLDRED << error_block_id << RESET << ", Code: " << BOLDRED << error_code << RESET;
        }
    }
}
} // namespace Ph2_HwInterface