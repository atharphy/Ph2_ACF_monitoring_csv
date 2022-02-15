#include "D19cOpticalInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// ##########################################
// # Constructors #
// #########################################

D19cOpticalInterface::D19cOpticalInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : FEConfigurationInterface(pId, pUri, pAddressTable)
{
    fType = ConfigurationType::IC;
}
D19cOpticalInterface::D19cOpticalInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : FEConfigurationInterface(puHalConfigFileName, pBoardId) { fType = ConfigurationType::IC; }
D19cOpticalInterface::~D19cOpticalInterface() {}

// ##########################################
// # ROC Register read/write #
// #########################################

// for now this is just a write followed by a read
bool D19cOpticalInterface::SingleWriteRead(Chip* pChip, ChipRegItem& pWriteReg)
{
    LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleWriteRead" << RESET;
    size_t      cAttempts = 0;
    bool        cSuccess  = false;
    ChipRegItem cReadBackReg;
    cReadBackReg.fAddress = pWriteReg.fAddress;
    cReadBackReg.fPage    = pWriteReg.fPage;
    do
    {
        if(SingleWrite(pChip, pWriteReg))
        {
            LOG(DEBUG) << BOLDGREEN << "D19cOpticalInterface::SingleWriteRead - write worked" << RESET;
            if(SingleRead(pChip, cReadBackReg))
            {
                LOG(DEBUG) << BOLDGREEN << "\t.. D19cOpticalInterface::SingleWriteRead - read worked "
                           << " value read back was 0x" << std::hex << +cReadBackReg.fValue << " expected value is 0x" << +pWriteReg.fValue << std::dec << RESET;
                cSuccess = (cReadBackReg.fValue == pWriteReg.fValue);
            }
        }
        cAttempts++;
    } while(cAttempts < fConfig.fMaxAttempts && !cSuccess && fConfig.fReTry);
    return cSuccess;
}
// for now this is just a loop of WriteRead
bool D19cOpticalInterface::MultiWriteRead(Chip* pChip, std::vector<ChipRegItem>& pWriteRegs)
{
    bool cSuccess = true;
    for(auto cWriteReg: pWriteRegs) cSuccess = cSuccess && SingleWriteRead(pChip, cWriteReg);
    return cSuccess;
}
bool D19cOpticalInterface::SingleWrite(Chip* pChip, ChipRegItem& pItem)
{
    pChip->UpdateModifiedRegMap(pItem);
    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
        return SingleWriteIC(pChip, pItem, false);
    else
        return SingleWriteSlave(pChip, pItem);
}
bool D19cOpticalInterface::SingleWriteIC(Chip* pChip, ChipRegItem& pItem, bool pVerifLoop)
{
    auto cRegMap = pChip->getRegMap();
    auto cIterator    = find_if(cRegMap.begin(), cRegMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
    if( cIterator != cRegMap.end() ){ 
        LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleWriteIC to " << cIterator->first << RESET;
    }
    return WriteLpGBTRegister(pChip->getOpticalId(), pItem.fAddress, pItem.fValue, pVerifLoop);
}
bool D19cOpticalInterface::SingleWriteSlave(Chip* pChip, ChipRegItem& pItem)
{
    auto              cLinkId   = pChip->getOpticalId();
    uint8_t           cMasterId = pChip->getMasterId();
    std::stringstream cOutput;
    pChip->printChipType(cOutput);

    LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleWriteSlave Writing 0x" << std::hex << +pItem.fValue << std::dec << " to [0x" << std::hex << +pItem.fAddress << std::dec << "] I2C master"
               << +cMasterId << " chipType " << cOutput.str() << RESET;
    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t  cChipAddress = pChip->getChipAddress();
    uint32_t cSlaveData   = 0x00;
    uint8_t  cNbytes      = 3;
    uint8_t  cFrequency   = 3;
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

    bool cSuccess = I2CWrite(cLinkId, cMasterId, cChipAddress, cSlaveData, cNbytes, cFrequency, fI2CWriteCount);
    pChip->updateWriteCount(fI2CWriteCount);
    if(fI2CWriteCount != 1)
    {
        LOG(INFO) << BOLDYELLOW << "\t\t... Pre-verfication - took " << +fI2CWriteCount << " I2C writes to succeed in writing " << +pItem.fValue << " on register " << +pItem.fAddress << " for "
                  << cOutput.str() << "#" << +pChip->getId() << " on Hybrid#" << +pChip->getHybridId() << RESET;
    }
    pChip->updateRegWriteCount();
    return cSuccess;
}
bool D19cOpticalInterface::SingleRead(Chip* pChip, ChipRegItem& pItem)
{
    if(pChip->getFrontEndType() == FrontEndType::LpGBT)
        return SingleReadIC(pChip, pItem);
    else
        return SingleReadSlave(pChip, pItem);
}
bool D19cOpticalInterface::SingleReadSlave(Chip* pChip, ChipRegItem& pItem)
{
    if(pItem.fControlReg == 0x1) return true;

    auto    cLinkId   = pChip->getOpticalId();
    uint8_t cMasterId = pChip->getMasterId();

    std::stringstream cOutput;
    pChip->printChipType(cOutput);

    LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleReadSlave Reading from 0x" << std::hex << +pItem.fAddress << std::dec << "] I2C master" << +cMasterId << " chipType " << cOutput.str()
               << RESET;

    uint8_t cChipId = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t  cChipAddress = pChip->getChipAddress(); // fFEAddressMap[pChip->getFrontEndType()] + cChipId;
    uint8_t  cNReadBytes  = 1;
    uint8_t  cNbytes      = 2;
    uint8_t  cFrequency   = 3;
    uint32_t cSlaveData   = ((pItem.fAddress & (0xFF << 8 * 0)) << 8) | ((pItem.fAddress & (0xFF << 8 * 1)) >> 8);
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
    return (fI2CReadCount < fConfig.fMaxAttempts);
}
bool D19cOpticalInterface::SingleReadIC(Chip* pChip, ChipRegItem& pItem)
{
    auto cLinkId = pChip->getOpticalId();
    if(pItem.fControlReg == 0x00)
    {
        auto cRegMap = pChip->getRegMap();
        auto cIterator    = find_if(cRegMap.begin(), cRegMap.end(), [&pItem](const ChipRegPair& obj) { return obj.second.fAddress == pItem.fAddress && obj.second.fPage == pItem.fPage; });
        if( cIterator != cRegMap.end() ){ 
            LOG(DEBUG) << BOLDYELLOW << "D19cOpticalInterface::SingleReadIC to " << cIterator->first << RESET;
        }
        auto cValue  = ReadLpGBTRegister(cLinkId, pItem.fAddress);
        pItem.fValue = cValue;
    }
    return true;
}
// for now this is just looping over single write
bool D19cOpticalInterface::MultiWrite(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    bool cSuccess = true;
    for(auto cRegItem: pRegisterItems) { cSuccess = cSuccess && SingleWrite(pChip, cRegItem); }
    return cSuccess;
}
// for now this is just looping over single read
bool D19cOpticalInterface::MultiRead(Chip* pChip, std::vector<ChipRegItem>& pRegisterItems)
{
    bool cSuccess = true;
    for(auto& cRegItem: pRegisterItems) { cSuccess = cSuccess && SingleRead(pChip, cRegItem); }
    return cSuccess;
}
// ##########################################
// # Read/Write registers with CPB I2C functions #
// #########################################

bool D19cOpticalInterface::WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop)
{
    auto cReTry    = fConfig.fReTry;
    fConfig.fReTry = 1; // for now.. always re-try this
    if(fConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "D19cOpticalInterface::WriteLpGBTRegister" << RESET;
    size_t cExpectedReplySize = 10 * 1;
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fResetEn) ResetCPB();
    // Use new Command Processor Block
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 3;
    // uint8_t cWorkerId = 16, cFunctionId = 3;
    if(fConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "WriteLpGBTRegister to Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;

    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    cCommandVector.push_back(pRegisterValue << 0);
    std::vector<uint32_t> cReplyVector     = WriteCommandCPBandReadReply(cCommandVector, cExpectedReplySize);
    uint8_t               cParityCheck     = cReplyVector[2] & 0xFF;
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    // always check parity
    size_t cIter             = 0;
    bool   cValidTransaction = (cParityCheck == 1);
    while(!cValidTransaction && fConfig.fReTry && cIter < fConfig.fMaxAttempts)
    {
        if(fConfig.fVerbose)
            LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fConfig.fMaxAttempts
                      << " of D19cOpticalInterface::WriteLpGBTRegister] : Received corrupted reply (mismatch in readbacks or failed parity check) from command processor block ... retrying" << RESET;
        ResetCPB();
        cReplyVector.clear();
        cReplyVector      = WriteCommandCPBandReadReply(cCommandVector, cExpectedReplySize);
        cParityCheck      = cReplyVector[2] & 0xFF;
        cReadBackRegAddr  = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cReadBack         = cReplyVector[7] & 0xFF;
        cValidTransaction = (cParityCheck == 1);
        if(cValidTransaction && pVerifLoop)
        {
            cValidTransaction = cValidTransaction && ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress));
            if(fConfig.fVerbose && !cValidTransaction)
                LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fConfig.fMaxAttempts
                          << " of D19cOpticalInterface::WriteLpGBTRegister] : Received corrupted reply (mismatch in readbacks) from command processor block ... retrying" << RESET;
        }
        cIter++;
    }

    fConfig.fReTry = cReTry;
    // throw exception based on failure
    if(cIter > 1) LOG(INFO) << BOLDYELLOW << "D19cOpticalInterface::WriteLpGBTRegister had to try " << cIter << "/" << fConfig.fMaxAttempts << " possible attempts to complete transaction." << RESET;
    if(cParityCheck != 1) throw std::runtime_error(std::string("[D19cOpticalInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - failed parity check"));
    if(pVerifLoop && cReadBack != pRegisterValue)
        throw std::runtime_error(std::string("[D19cOpticalInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register value"));
    if(pVerifLoop && cReadBackRegAddr != pRegisterAddress)
        throw std::runtime_error(std::string("[D19cOpticalInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register address"));
    return (pVerifLoop) ? ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress)) : (cParityCheck == 1);
}

uint8_t D19cOpticalInterface::ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress)
{
    auto cReTry               = fConfig.fReTry;
    fConfig.fReTry            = 1; // for now.. always re-try this
    size_t cExpectedReplySize = 10 * 1;
    this->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    if(fResetEn) ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 2;
    if(fConfig.fVerbose) LOG(INFO) << BOLDMAGENTA << "D19cOpticalInterface::ReadLpGBTRegister from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    std::vector<uint32_t> cReplyVector     = WriteCommandCPBandReadReply(cCommandVector, cExpectedReplySize);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    // uint8_t               cParityCheck     = cReplyVector[2] & 0xFF;
    size_t cIter = 0;
    while((cReadBackRegAddr != pRegisterAddress) && cIter < fConfig.fMaxAttempts && fConfig.fReTry)
    {
        if(fResetEn) ResetCPB();
        if(fConfig.fVerbose)
            LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/" << fConfig.fMaxAttempts
                      << " of D19cOpticalInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block ... retrying" << RESET;
        cReplyVector.clear();
        cReplyVector     = WriteCommandCPBandReadReply(cCommandVector, cExpectedReplySize);
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cIter++;
    };
    fConfig.fReTry = cReTry;
    if(cIter == (size_t)fConfig.fMaxAttempts || cReadBackRegAddr != pRegisterAddress)
        throw std::runtime_error(std::string("[D19cOpticalInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block"));
    return cReadBack;
}

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
} // namespace Ph2_HwInterface