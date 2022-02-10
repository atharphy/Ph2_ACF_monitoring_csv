#include "D19cOpticalInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{

// ##########################################
// # Constructors #
// #########################################


D19cOpticalInterface::D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : FEConfigurationInterface(pId, pUri, pAddressTable) {
}
D19cOpticalInterface::D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : FEConfigurationInterface(puHalConfigFileName, pBoardId) {
}
D19cOpticalInterface::~D19cOpticalInterface() {}

// ##########################################
// # ROC Register read/write #
// #########################################

// for now this is just a write followed by a read 
bool D19cOpticalInterface::SingleWriteRead(Chip* pChip, ChipRegItem& pWriteReg )
{
    size_t cAttempts = 0; 
    bool cSuccess = false;
    ChipRegItem cReadBackReg; 
    cReadBackReg.fAddress = pWriteReg.fAddress; 
    cReadBackReg.fPage    = pWriteReg.fPage;
    do 
    {   if( SingleWrite(pChip, pWriteReg ) ) 
        {
            LOG (DEBUG) << BOLDGREEN << "D19cOpticalInterface::SingleWriteRead - write worked" << RESET;   
            if( SingleRead(pChip, cReadBackReg )  ){ 
                LOG (INFO) << BOLDGREEN << "\t.. D19cOpticalInterface::SingleWriteRead - read worked " 
                    << " value read back was 0x" << std::hex << +cReadBackReg.fValue 
                    << " expected value is 0x" << +pWriteReg.fValue 
                    << std::dec 
                    << RESET;
                cSuccess = ( cReadBackReg.fValue == pWriteReg.fValue );
            }
        }
        cAttempts++;
    }while( cAttempts < fConfig.fMaxAttempts && !cSuccess && fConfig.fReTry );
    return cSuccess; 
}
// for now this is just a loop of WriteRead 
bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pWriteRegs)
{
    bool cSuccess=true;
    for(auto cWriteReg : pWriteRegs ) cSuccess = cSuccess && SingleWriteRead(pChip, cWriteReg );
    return cSuccess;
}
bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pItem )
{
    auto    cLinkId = pChip->getOpticalId();
    uint8_t cMasterId = pChip->getMasterId();
    
    LOG(DEBUG) << BOLDBLUE << " Writing 0x" << std::hex << +pItem.fValue << std::dec << " to [0x" << std::hex << +pItem.fAddress << std::dec << "] I2C master" << +cMasterId << RESET;
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = pChip->getChipAddress(); 
    uint32_t cSlaveData = 0x00;
    uint8_t  cNbytes    = 3;
    uint8_t  cFrequency = 3;
    // CBC addresses are only 8 bits
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
    {
        // LOG (INFO) << BOLDYELLOW << "Writing to ... something else " << RESET;
        uint16_t cInvertedRegister = ((pItem.fAddress & (0xFF << 8 * 0)) << 8) | ((pItem.fAddress & (0xFF << 8 * 1)) >> 8);
        cSlaveData                 = (pItem.fValue << 16) | cInvertedRegister;
    }
    else
    {
        cNbytes    = 2;
        cSlaveData = (pItem.fValue << 8) | (pItem.fAddress & 0xFF);
    }
    fI2CWriteCount = 0;
    bool cSuccess    = I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes, cFrequency, fI2CWriteCount);
    pChip->updateWriteCount(fI2CWriteCount);
    if(fI2CWriteCount != 1)
    {
        std::stringstream cOutput;
        pChip->printChipType(cOutput);
        LOG(INFO) << BOLDYELLOW << "\t\t... Pre-verfication - took " << +fI2CWriteCount << " I2C writes to succeed in writing " << +pItem.fValue << " on register " << +pItem.fAddress << " for "
                  << cOutput.str() << "#" << +pChip->getId() << " on Hybrid#" << +pChip->getHybridId() << RESET;
    }
    pChip->updateRegWriteCount();
    return cSuccess;
}
bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem ) 
{
    auto                                  cLinkId   = pChip->getOpticalId();
    uint8_t                               cMasterId = pChip->getMasterId();

    // LOG (INFO) << BOLDGREEN << "D19cOpticalInterface::SingleRead Reading FE register on link " << +cLinkId << RESET;
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = pChip->getChipAddress(); // fFEAddressMap[pChip->getFrontEndType()] + cChipId;
    uint8_t cNReadBytes = 1;
    uint8_t  cNbytes    = 2;
    uint8_t  cFrequency = 3;
    uint32_t cSlaveData = ((pItem.fAddress & (0xFF << 8 * 0)) << 8) | ((pItem.fAddress & (0xFF << 8 * 1)) >> 8);
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
        cSlaveData = ((pItem.fAddress & (0xFF << 8 * 0)) << 8) | ((pItem.fAddress & (0xFF << 8 * 1)) >> 8);
    else
    {
        cNbytes    = 1;
        cSlaveData = (pItem.fAddress & 0xFF);
    }

    fI2CWriteCount = 0;
    fI2CReadCount  = 0;
    {
        std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock
        I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes, cFrequency, fI2CWriteCount);
        pItem.fValue = I2CRead(cLinkId, cMasterId, cChipAddress, cNReadBytes, cFrequency, fI2CReadCount);
    }
    pChip->updateWriteCount(fI2CWriteCount);
    pChip->updateReadCount(fI2CReadCount);
    pChip->updateRegReadCount();
    return (fI2CReadCount < fConfig.fMaxAttempts );
}
// for now this is just looping over single write 
bool D19cOpticalInterface::MultiWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems )
{
    bool cSuccess = true;
    for(auto cRegItem : pRegisterItems )
    {
        cSuccess = cSuccess && SingleWrite(pChip,cRegItem); 
    }
    return cSuccess;
}
// for now this is just looping over single read 
bool D19cOpticalInterface::MultiRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    bool cSuccess = true;
    for(auto& cRegItem : pRegisterItems )
    {
        cSuccess = cSuccess && SingleRead(pChip,cRegItem);
    }
    return cSuccess;
}


// ##########################################
// # Read/Write registers with CPB I2C functions #
// #########################################

bool D19cOpticalInterface::I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, uint8_t pFrequency, uint32_t& theI2CWriteCount)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock

    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 5, cMasterConfig = (pNBytes << 2) | pFrequency;
    if(fConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "I2C write to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    // ResetCPB();
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(cMasterConfig << 24 | pSlaveData << 0);
    std::vector<uint32_t> cReplyVector = WriteCommandCPBandReadReply(cCommandVector, 10);
    fI2Cstatus                         = cReplyVector[7] & 0xFF;
    size_t cIter = 0, cMaxIter = fConfig.fMaxAttempts;
    while(fI2Cstatus != 4 && cIter < cMaxIter && fConfig.fReTry)
    {
        if(fI2Cstatus != 4)
            LOG(DEBUG) << BOLDMAGENTA << "[D19cOpticalInterface::I2CWrite] Iter#" << +cIter << " I2CM" << +pMasterId << " status indicates a failure 0x" << std::hex << +fI2Cstatus << std::dec
                       << " transaction was to write " << +pNBytes << " to slave address " << +pSlaveAddress << " with data 0x" << std::hex << pSlaveData << std::dec << RESET;
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cOpticalInterface::I2CWrite] : I2CM" << +pMasterId << " Transaction Failed" << RESET;
        ResetCPB();
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cOpticalInterface::I2CWrite] : Received corrupted reply from command processor block ... retrying" << RESET;
        cReplyVector.clear();
        std::vector<uint32_t> cReplyVector = WriteCommandCPBandReadReply(cCommandVector, 10);
        fI2Cstatus                         = cReplyVector[7] & 0xFF;
        cIter++;
    }
    theI2CWriteCount += (1 + cIter);
    if(cIter == cMaxIter)
    {
        LOG(INFO) << BOLDRED << "[D19cOpticalInterface::I2CWrite] Iter#" << +cIter << " I2CM" << +pMasterId << " status indicates a failure 0x" << std::hex << +fI2Cstatus << std::dec
                  << " transaction was to write " << +pNBytes << " to slave address " << +pSlaveAddress << " with data 0x" << std::hex << pSlaveData << std::dec << RESET;
    }
    if(fI2Cstatus != 4) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] I2CM" << +pMasterId << " status is 0x" << std::hex << +fI2Cstatus << std::dec << RESET;
    return (fI2Cstatus == 4);
}
uint8_t D19cOpticalInterface::I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, uint8_t pFrequency, uint32_t& theI2CReadCount)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex); // Fabio:: I  do not like this lock

    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 4, cMasterConfig = (pNBytes << 2) | pFrequency;
    if(fConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "I2C Read to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    // ResetCPB();
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(cMasterConfig << 24);
    std::vector<uint32_t> cReplyVector     = WriteCommandCPBandReadReply(cCommandVector, 10);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    size_t                cIter = 0, cMaxIter = fConfig.fMaxAttempts;
    uint16_t              cI2CReadByteRegAddr = 0;
    // pick correct register address to check
    if(pMasterId == 2) cI2CReadByteRegAddr = 0x018d;
    if(pMasterId == 1) cI2CReadByteRegAddr = 0x178;
    if(pMasterId == 0) cI2CReadByteRegAddr = 0x0163;
    // check reply
    bool cCheckReadByte = true;
    bool cFail          = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
    cFail               = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter && fConfig.fReTry);
    while(cFail)
    {
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cOpticalInterface::I2CRead] : Received corrupted reply from command processor block ... retrying" << RESET;
        ResetCPB();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        cReplyVector.clear();
        std::vector<uint32_t> cReplyVector = WriteCommandCPBandReadReply(cCommandVector, 10);
        // std::this_thread::sleep_for(std::chrono::microseconds(10));
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cFail            = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
        cFail            = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter && fConfig.fReTry);
        if(cIter == cMaxIter - 1) LOG(INFO) << BOLDRED << "[D19cOpticalInterface::I2CRead] : Corrupted CPB reply frame" << RESET;
        cIter++;
    };
    theI2CReadCount += (1 + cIter);
    if(cIter == cMaxIter) throw std::runtime_error(std::string("[D19cOpticalInterface::I2CRead] : Corrupted CPB reply frame"));
    return cReadBack;
}


// ##########################################
// # Read/Write new Command Processor Block #
// #########################################
void D19cOpticalInterface::ResetCPB()
{
    LOG(DEBUG) << BOLDBLUE << "Resetting CPB" << RESET;
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset shoudl be 0x00020010
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 10);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}

void D19cOpticalInterface::WriteCommandCPB(const std::vector<uint32_t>& pCommandVector)
{
    uint8_t cWordIndex = 0;
    if(fConfig.fVerbose)
    {
        for(auto cCommandWord: pCommandVector)
        {
            LOG(INFO) << GREEN << "\t Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cCommandWord << std::dec << RESET;
            cWordIndex++;
        }
    }
    WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommandVector);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}

std::vector<uint32_t> D19cOpticalInterface::ReadReplyCPB(uint8_t pNWords)
{
    std::vector<uint32_t> cReplyVector = ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t               cFifoIndex   = 0;
    if(fConfig.fVerbose)
    {
        for(auto cReplyWord: cReplyVector)
        {
            LOG(INFO) << YELLOW << "\t Read reply word " << +cFifoIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cReplyWord << std::dec << RESET;
            cFifoIndex++;
        }
        LOG(INFO) << "\t lpgbtsc FSM state : 0b" << std::bitset<8>(ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state")) << RESET;
    }
    return cReplyVector;
}
std::vector<uint32_t> D19cOpticalInterface::WriteCommandCPBandReadReply(const std::vector<uint32_t>& pCommandVector, uint8_t pNWords)
{
    std::lock_guard<std::recursive_mutex> theGuard(fMutex);
    WriteCommandCPB(pCommandVector);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
    return ReadReplyCPB(pNWords);
}
}