#include "WorkerTester.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

WorkerTester::WorkerTester(bool pNewImp, bool pVerbose, uint8_t pLpGbtVers) : Tool(), fNewImp(pNewImp), fVerbose(pVerbose), fLpGbtVers(pLpGbtVers) {}

WorkerTester::~WorkerTester() {}

void WorkerTester::SetLpGbtVersion(uint8_t pLpGbtVers)
{
    fBeBoardInterface->setBoard(0);
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cFWInterface->WriteReg("fc7_daq_cnfg.optical_block.lpgbt.version", pLpGbtVers);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG(INFO) << YELLOW << "lpGBT version : " << +cFWInterface->ReadReg("fc7_daq_cnfg.optical_block.lpgbt.version") << RESET;
    fLpGbtVers = pLpGbtVers;
}

void WorkerTester::PrintFSMState()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint32_t         cStatus      = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    LOG(INFO) << "Worker FSM status : " << +(cStatus & 0xFF) << RESET;
    LOG(INFO) << "IC FSM status : " << +((cStatus & (0xFF << 8)) >> 8) << RESET;
    LOG(INFO) << "I2C FSM status : " << +((cStatus & (0xFF << 16)) >> 16) << RESET;
    LOG(INFO) << "FE FSM status : " << +((cStatus & (0xFF << 24)) >> 24) << RESET;
}

void WorkerTester::ResetCPB()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    // Soft reset the GBT-SC worker
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    uint8_t cWorkerId = 0, cFunctionId = 2;
    // reset shoudl be 0x00020010
    // this->WriteStackReg({{"fc7_daq_ctrl.command_processor_block.cpb_ctrl_reg.core_reset", 0x01}, {"fc7_daq_ctrl.command_processor_block.cpb_ctrl_reg.core_reset",0x00}});
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    cFWInterface->WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    cFWInterface->ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void WorkerTester::WriteCommandCPB(const std::vector<uint32_t>& pCommandVector)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t          cWordIndex   = 0;
    if(fVerbose)
    {
        LOG(INFO) << "----------------------" << RESET;
        for(auto cCommandWord: pCommandVector)
        {
            LOG(INFO) << GREEN << "Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cCommandWord << std::dec << RESET;
            cWordIndex++;
        }
    }
    cFWInterface->WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommandVector);
}

std::vector<uint32_t> WorkerTester::ReadReplyCPB(uint8_t pNWords)
{
    D19cFWInterface*      cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    std::vector<uint32_t> cReplyVector = cFWInterface->ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t               cFifoIndex   = 0;
    if(fVerbose)
    {
        for(auto cReplyWord: cReplyVector)
        {
            LOG(INFO) << YELLOW << "Read reply word " << +cFifoIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cReplyWord << std::dec << RESET;
            cFifoIndex++;
        }
        // PrintFSMState();
        LOG(INFO) << "----------------------" << RESET;
    }
    return cReplyVector;
}

uint8_t WorkerTester::ReadLpGBTRegister_New(uint8_t pLinkId, uint16_t pRegisterAddress)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 2;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsICToolDone()) { continue; }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t               cReadBack    = cReplyVector[0] & 0xFF;
    return cReadBack;
}

bool WorkerTester::WriteLpGBTRegister_New(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 3;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    cCommandVector.push_back(pRegisterValue << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsICToolDone()) { continue; }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t               cReadBack    = cReplyVector[0] & 0xFF;
    if(cReadBack != pRegisterValue)
    {
        LOG(INFO) << RED << "lpGBTWrite : Wrong value read back" << RESET;
        return false;
    }
    return true;
}

bool WorkerTester::IsICToolDone()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint32_t         cStatus      = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool             cWorkerDone  = (cStatus & 0xFF) == 1;
    bool             cICToolDone  = ((cStatus & (0xFF << 8)) >> 8) == 1;
    return cWorkerDone && cICToolDone;
}

uint8_t WorkerTester::I2CRead_New(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 4;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsI2CToolDone()) { continue; }
    std::vector<uint32_t> cReplyVector  = ReadReplyCPB(1);
    uint8_t               cReadBack     = cReplyVector[0] & 0xFF;
    uint8_t               cI2CErrorCode = cReplyVector[0] & (0xFF << 8);
    if(cI2CErrorCode != 0)
    {
        LOG(INFO) << RED << "I2CRead : I2C ERROR read ... " << RESET;
        exit(0);
        return false;
    }
    return cReadBack;
}

bool WorkerTester::I2CWrite_New(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 5;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | pMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsI2CToolDone())
    {
        // LOG(INFO) << BOLDRED << "I2C Try counter : " << +cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters") & 0xFF00 << RESET;
        // if(pVerbose) PrintFSMState();
        continue;
    }
    std::vector<uint32_t> cReplyVector    = ReadReplyCPB(1);
    uint8_t               cI2CTransStatus = cReplyVector[0] & 0xFF;
    if(cI2CTransStatus != 4)
    {
        // PrintLpGBTReplyFrame();
        LOG(INFO) << RED << "I2CWrite : Wrong I2C status read back ... I2C status " << +cI2CTransStatus << RESET;
        PrintI2CMasterRegisters(fDetectorContainer->at(0)->at(0)->flpGBT, pMasterId);
        exit(0);
        return false;
    }
    return true;
}

bool WorkerTester::IsI2CToolDone()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint32_t         cStatus      = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool             cWorkerDone  = (cStatus & 0xFF) == 1;
    bool             cI2CToolDone = ((cStatus & (0xFF << 16)) >> 16) == 1;
    return cWorkerDone && cI2CToolDone;
}

uint8_t WorkerTester::ReadFERegister_New(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t          cLinkId      = pChip->getOpticalGroupId();
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId   = 16 + cLinkId;
    uint8_t cFunctionId = 6;
    uint8_t cMasterId   = pChip->getMasterId();
    uint8_t cChipCode   = fChipCodeMap[pChip->getFrontEndType()];
    uint8_t cChipId =
        (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8); // use modulo 8 to accomodate for how MPAs are numbered
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
    cCommandVector.push_back(pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsFEToolDone())
    {
        // LOG(INFO) << BOLDRED << "FE Try counter : " << +cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters") & 0xFF0000 << RESET;
        // if(fVerbose) PrintFSMState();
        continue;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t               cReadBack    = cReplyVector[0] & 0xFF;
    LOG(DEBUG) << GREEN << "ReadFERegister : successfully read 0x" << std::hex << +cReadBack << std::dec << " from register : 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
    return cReadBack;
}

bool WorkerTester::WriteFERegister_New(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t          cLinkId      = pChip->getOpticalGroupId();
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", cLinkId);
    uint8_t cWorkerId   = 16 + cLinkId;
    uint8_t cFunctionId = 7;
    uint8_t cMasterId   = pChip->getMasterId();
    uint8_t cChipCode   = fChipCodeMap[pChip->getFrontEndType()];
    uint8_t cChipId =
        (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : (pChip->getId() % 8); // use modulo 8 to accomodate for how MPAs are numbered
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pVerify << 8 | cMasterId << 6 | cChipCode << 3 | cChipId << 0);
    cCommandVector.push_back(pRegisterValue << 16 | pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    while(!IsFEToolDone())
    {
        // LOG(INFO) << BOLDRED << "FE Try counter : " << +cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_try_counters") & 0xFF0000<< RESET;
        // if(fVerbose) PrintFSMState();
        continue;
    }
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    if(pVerify)
    {
        uint8_t cReadBack = cReplyVector[0] & 0xFF;
        if(cReadBack != pRegisterValue)
        {
            LOG(INFO) << RED << "WriteFERegister with Verification : Wrong value read back" << RESET;
            return false;
        }
        LOG(DEBUG) << GREEN << "WriteFERegister with Verification : successfully written 0x" << std::hex << +cReadBack << std::dec << " to register : 0x" << std::hex << +pRegisterAddress << std::dec
                   << RESET;
        return true;
    }
    else
    {
        uint8_t cI2CStatus = cReplyVector[0] & 0xFF;
        if(cI2CStatus != 4)
        {
            LOG(INFO) << RED << "WrireFERegister without verificaiton : Wrong I2C status read back ... I2C status " << +cI2CStatus << RESET;
            return false;
        }
        LOG(DEBUG) << GREEN << "WriteFERegister without verification : successfully written 0x" << std::hex << +pRegisterValue << std::dec << " to register : 0x" << std::hex << +pRegisterAddress
                   << std::dec << RESET;
        return true;
    }
}

bool WorkerTester::IsFEToolDone()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint32_t         cStatus      = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
    bool             cWorkerDone  = (cStatus & 0xFF) == 1;
    bool             cFEToolDone  = ((cStatus & (0xFF << 24)) >> 24) == 1;
    return cWorkerDone && cFEToolDone;
}

bool WorkerTester::WriteLpGBTRegister_Old(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue)
{
    D19cFWInterface* cFWInterface       = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    size_t           cExpectedReplySize = 10 * 1;
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    // Use new Command Processor Block
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 3;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    cCommandVector.push_back(pRegisterValue << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(cExpectedReplySize);
    uint8_t               cParityCheck     = cReplyVector[2] & 0xFF;
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    // always check parity
    size_t cIter = 0, cMaxIter = 10;
    bool   cValidTransaction = (cParityCheck == 1) && ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress));
    while(!cValidTransaction && cIter < cMaxIter)
    {
        // ResetCPB();
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector      = ReadReplyCPB(cExpectedReplySize);
        cParityCheck      = cReplyVector[2] & 0xFF;
        cReadBackRegAddr  = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cReadBack         = cReplyVector[7] & 0xFF;
        cValidTransaction = (cParityCheck == 1) && ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress));
        cIter++;
    }
    // throw exception based on failure
    if(cIter > 1) LOG(INFO) << BOLDYELLOW << "D19cFWInterface::WriteLpGBTRegister had to try " << cIter << "/10 possible attempts to complete transaction." << RESET;
    if(cParityCheck != 1) throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - failed parity check"));
    if(cReadBack != pRegisterValue)
        throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register value"));
    if(cReadBackRegAddr != pRegisterAddress)
        throw std::runtime_error(std::string("[D19cFWInterface::WriteLpGBTRegister] : Received corrupted reply from command processor block - mismatch in read-back lpGBT register address"));
    return ((cReadBack == pRegisterValue) && (cReadBackRegAddr == pRegisterAddress));
}

uint8_t WorkerTester::ReadLpGBTRegister_Old(uint8_t pLinkId, uint16_t pRegisterAddress)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    size_t                cExpectedReplySize = 10 * 1;
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 2;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(cExpectedReplySize);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    size_t                cIter = 0, cMaxIter = 10;
    while((cReadBackRegAddr != pRegisterAddress) && cIter < cMaxIter)
    {
        // ResetCPB();
        if(fVerbose) LOG(INFO) << BOLDRED << "[Iter# " << cIter << "/10 of D19cFWInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block ... retrying" << RESET;
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector     = ReadReplyCPB(cExpectedReplySize);
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cIter++;
    };
    if(cIter == cMaxIter) throw std::runtime_error(std::string("[D19cFWInterface::ReadLpGBTRegister] : Received corrupted reply from command processor block"));
    // LOG(DEBUG) << BOLDWHITE << "\t Reading 0x" << std::hex << +cReadBack << std::dec << " from [0x" << std::hex << +pRegisterAddress << std::dec << "]" << RESET;
    return cReadBack;
}

bool WorkerTester::I2CWrite_Old(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 5;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(pMasterConfig << 24 | pSlaveData << 0);
    WriteCommandCPB(cCommandVector);
    // std::this_thread::sleep_for(std::chrono::microseconds(50));
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(10);
    uint8_t               fI2Cstatus   = cReplyVector[7] & 0xFF;
    size_t                cIter = 0, cMaxIter = 10;
    while(fI2Cstatus != 4 && cIter < cMaxIter)
    {
        // ResetCPB();
        LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] I2CM" << +pMasterId << " status is 0x" << std::hex << +fI2Cstatus << std::dec << RESET;
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        cReplyVector = ReadReplyCPB(10);
        fI2Cstatus   = cReplyVector[7] & 0xFF;
        cIter++;
    }
    if(fI2Cstatus != 4) LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CWrite] I2CM" << +pMasterId << " status is 0x" << std::hex << +fI2Cstatus << std::dec << RESET;
    return (fI2Cstatus == 4);
}

uint8_t WorkerTester::I2CRead_Old(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 4;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 8 | pSlaveAddress << 0);
    cCommandVector.push_back(pMasterConfig << 24);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(10);
    uint8_t               cReadBack        = cReplyVector[7] & 0xFF;
    uint16_t              cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
    size_t                cIter = 0, cMaxIter = 10;
    uint16_t              cI2CReadByteRegAddr = 0;
    // pick correct register address to check
    if(pMasterId == 2) cI2CReadByteRegAddr = 0x018d;
    if(pMasterId == 1) cI2CReadByteRegAddr = 0x178;
    if(pMasterId == 0) cI2CReadByteRegAddr = 0x0163;
    // check reply
    bool cCheckReadByte = true;
    bool cFail          = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
    cFail               = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter);
    while(cFail)
    {
        LOG(INFO) << BOLDRED << "[D19cFWInterface::I2CRead] : Received corrupted reply from command processor block ... retrying" << RESET;
        // ResetCPB();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        cReplyVector.clear();
        WriteCommandCPB(cCommandVector);
        std::this_thread::sleep_for(std::chrono::microseconds(50));
        cReplyVector = ReadReplyCPB(10);
        // std::this_thread::sleep_for(std::chrono::microseconds(10));
        cReadBack        = cReplyVector[7] & 0xFF;
        cReadBackRegAddr = ((cReplyVector[6] & 0xFF) << 8 | (cReplyVector[5] & 0xFF));
        cFail            = cCheckReadByte ? (cReadBackRegAddr != cI2CReadByteRegAddr) : false;
        cFail            = cFail && (cI2CReadByteRegAddr && cIter < cMaxIter);
        cIter++;
    };
    if(cIter == cMaxIter) throw std::runtime_error(std::string("[D19cFWInterface::I2CRead] : Corrupted CPB reply frame"));
    return cReadBack;
}

bool WorkerTester::WriteFERegister_Old(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify)
{
    auto    cLinkId   = pChip->getOpticalGroupId();
    uint8_t cMasterId = pChip->getMasterId();
    uint8_t cChipId   = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = fChipAddressMap[pChip->getFrontEndType()] + cChipId;
    // +1 for CBC address
    cChipAddress += (pChip->getFrontEndType() == FrontEndType::CBC3) ? 1 : 0;
    // CBC addresses are only 8 bits
    uint32_t cSlaveData = 0x00;
    uint8_t  cNbytes    = 3;
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
    {
        uint16_t cInvertedRegister = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
        cSlaveData                 = (pRegisterValue << 16) | cInvertedRegister;
    }
    else
    {
        cNbytes    = 2;
        cSlaveData = (pRegisterValue << 8) | (pRegisterAddress & 0xFF);
    }
    uint8_t cMasterConfig = (cNbytes << 2) | 3;
    bool    cSuccess      = I2CWrite(cLinkId, cMasterId, cMasterConfig, cChipAddress, cSlaveData);
    if(pVerify && cSuccess)
    {
        uint8_t cReadBack = ReadFERegister(pChip, pRegisterAddress);
        uint8_t cIter = 0, cMaxIter = 10;
        while(cReadBack != pRegisterValue && cIter < cMaxIter)
        {
            if(cIter == cMaxIter - 1)
            {
                LOG(INFO) << BOLDRED << "I2C ReadBack Mismatch in hybrid " << +pChip->getHybridId() << " Chip " << +cChipId << " register 0x" << std::hex << +pRegisterAddress << std::dec
                          << " asked to write 0x" << std::hex << +pRegisterValue << std::dec << " and read back 0x" << std::hex << +cReadBack << std::dec << RESET;
            }
            // dont re-write  - just try and read again
            // cReadBack = ReadFERegister(pChip, pRegisterAddress);

            // this was repeating both the write and the read
            cSuccess = I2CWrite(cLinkId, cMasterId, cMasterConfig, cChipAddress, cSlaveData);
            if(cSuccess) { cReadBack = ReadFERegister(pChip, pRegisterAddress); }
            cIter++;
        }
        if(cReadBack != pRegisterValue) { throw std::runtime_error(std::string("I2C readback mismatch")); }
    }
    else if(!cSuccess)
    {
        std::stringstream cErrorMsg;
        cErrorMsg << "I2C Write FAILED on Hybrid#" << +pChip->getHybridId() << " Link#" << +cLinkId << " Chip#" << +pChip->getId();
        LOG(INFO) << BOLDRED << cErrorMsg.str() << RESET;
        throw std::runtime_error(std::string(cErrorMsg.str()));
    }
    return cSuccess;
}

uint8_t WorkerTester::ReadFERegister_Old(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress)
{
    auto    cLinkId   = pChip->getOpticalGroupId();
    uint8_t cMasterId = pChip->getMasterId();
    uint8_t cChipId   = (pChip->getFrontEndType() == FrontEndType::CIC || pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId();
    if(pChip->getFrontEndType() == FrontEndType::MPA) cChipId = cChipId % 8;
    uint8_t cChipAddress = fChipAddressMap[pChip->getFrontEndType()] + cChipId;
    // +1 for CBC address
    cChipAddress += (pChip->getFrontEndType() == FrontEndType::CBC3) ? 1 : 0;
    uint8_t  cNbytes       = 2;
    uint8_t  cMasterConfig = (cNbytes << 2) | 3;
    uint32_t cSlaveData    = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    if(pChip->getFrontEndType() != FrontEndType::CBC3)
        cSlaveData = ((pRegisterAddress & (0xFF << 8 * 0)) << 8) | ((pRegisterAddress & (0xFF << 8 * 1)) >> 8);
    else
    {
        cNbytes    = 1;
        cSlaveData = (pRegisterAddress & 0xFF);
    }
    cMasterConfig = (cNbytes << 2) | 3;
    I2CWrite(cLinkId, cMasterId, cMasterConfig, cChipAddress, cSlaveData);
    cNbytes            = 1;
    cMasterConfig      = (cNbytes << 2) | 3;
    uint32_t cReadBack = I2CRead(cLinkId, cMasterId, cMasterConfig, cChipAddress);
    return cReadBack;
}

bool WorkerTester::WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue)
{
    if(fNewImp)
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "New WriteLpGBTRegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << " , Register value 0x" << std::hex << +pRegisterValue << std::dec
                      << RESET;
        return WriteLpGBTRegister_New(pLinkId, pRegisterAddress, pRegisterValue);
    }
    else
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "Old WriteLpGBTRegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << " , Register value 0x" << std::hex << +pRegisterValue << std::dec
                      << RESET;
        return WriteLpGBTRegister_Old(pLinkId, pRegisterAddress, pRegisterValue);
    }
}

uint8_t WorkerTester::ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress)
{
    if(fNewImp)
    {
        if(fVerbose) LOG(INFO) << MAGENTA << "New ReadLpGBTRegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
        return ReadLpGBTRegister_New(pLinkId, pRegisterAddress);
    }
    else
    {
        if(fVerbose) LOG(INFO) << MAGENTA << "Old ReadLpGBTRegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
        return ReadLpGBTRegister_Old(pLinkId, pRegisterAddress);
    }
}

// function for I2C transactions using lpGBT I2C Masters
bool WorkerTester::I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData)
{
    if(fNewImp)
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "New I2CWrite, MasterId 0x" << std::hex << +pMasterId << std::dec << " , MasterConfig 0x" << std::hex << +pMasterConfig << std::dec << " , Slave address 0x"
                      << std::hex << +pSlaveAddress << std::dec << " , Slave data 0x" << std::hex << +pSlaveData << std::dec << RESET;
        return I2CWrite_New(pLinkId, pMasterId, pMasterConfig, pSlaveAddress, pSlaveData);
    }
    else
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "Old I2CWrite, MasterId 0x" << std::hex << +pMasterId << std::dec << " , MasterConfig 0x" << std::hex << +pMasterConfig << std::dec << " , Slave address 0x"
                      << std::hex << +pSlaveAddress << std::dec << " , Slave data 0x" << std::hex << +pSlaveData << std::dec << RESET;
        return I2CWrite_Old(pLinkId, pMasterId, pMasterConfig, pSlaveAddress, pSlaveData);
    }
}

uint8_t WorkerTester::I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress)
{
    if(fNewImp)
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "New I2CRead, MasterId 0x" << std::hex << +pMasterId << std::dec << " , MasterConfig 0x" << std::hex << +pMasterConfig << std::dec << " , Slave address 0x"
                      << std::hex << +pSlaveAddress << std::dec << RESET;
        return I2CRead_New(pLinkId, pMasterId, pMasterConfig, pSlaveAddress);
    }
    else
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "Old I2CRead, MasterId 0x" << std::hex << +pMasterId << std::dec << " , MasterConfig 0x" << std::hex << +pMasterConfig << std::dec << " , Slave address 0x"
                      << std::hex << +pSlaveAddress << std::dec << RESET;
        return I2CRead_Old(pLinkId, pMasterId, pMasterConfig, pSlaveAddress);
    }
}

// function for front-end slow control
bool WorkerTester::WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify)
{
    if(fNewImp)
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "New WriteFERegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << " , Register Value 0x" << std::hex << +pRegisterValue << std::dec
                      << RESET;
        return WriteFERegister_New(pChip, pRegisterAddress, pRegisterValue, pVerify);
    }
    else
    {
        if(fVerbose)
            LOG(INFO) << MAGENTA << "Old WriteFERegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << " , Register Value 0x" << std::hex << +pRegisterValue << std::dec
                      << RESET;
        return WriteFERegister_Old(pChip, pRegisterAddress, pRegisterValue, pVerify);
    }
}

uint8_t WorkerTester::ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress)
{
    if(fNewImp)
    {
        if(fVerbose) LOG(INFO) << MAGENTA << "New ReadFERegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
        return ReadFERegister_New(pChip, pRegisterAddress);
    }
    else
    {
        if(fVerbose) LOG(INFO) << MAGENTA << "Old ReadFERegister, Register address 0x" << std::hex << +pRegisterAddress << std::dec << RESET;
        return ReadFERegister_Old(pChip, pRegisterAddress);
    }
}

bool WorkerTester::TestICRead(OpticalGroup* cOpticalGroup)
{
    // Reading the ConfigPins which is hard wired
    LOG(INFO) << BOLDMAGENTA << "Testing IC Read on OpticalGroup " << cOpticalGroup->getId() << RESET;
    uint16_t cConfigPinsReg = 0x140;
    uint8_t  cConfigPinsVal = ReadLpGBTRegister(cOpticalGroup->getId(), cConfigPinsReg);
    LOG(INFO) << YELLOW << "LpGBT Mode = " << WHITE << ((cConfigPinsVal & 0xF0) >> 4) << RESET;
    LOG(INFO) << "----------------------" << RESET;
    return true;
}

bool WorkerTester::TestICRead()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestICRead(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

bool WorkerTester::TestICWrite(OpticalGroup* cOpticalGroup)
{
    // Write to I2C Master 0 slave address register and readback
    LOG(INFO) << BOLDMAGENTA << "Testing IC Write on OpticalGroup " << cOpticalGroup->getId() << RESET;
    uint16_t cI2CM0AddressReg = 0x0f1;
    bool     cSuccess         = true;
    for(uint8_t cIteration = 0; cIteration < 10; cIteration++)
    {
        for(uint8_t i = 0; i < 255; i++) { cSuccess &= WriteLpGBTRegister(cOpticalGroup->getId(), cI2CM0AddressReg, i); }
    }
    if(cSuccess) { LOG(INFO) << GREEN << "Successfully written and checked all values" << RESET; }
    else
    {
        LOG(INFO) << RED << "Failed on at least one write and check" << RESET;
    }
    return cSuccess;
}

bool WorkerTester::TestICWrite()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestICWrite(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

void WorkerTester::ResetI2CMasters(OpticalGroup* cOpticalGroup)
{
    ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
    // reset i2C masters
    LOG(INFO) << GREEN << "Reseting I2C Masters" << RESET;
    std::vector<uint8_t> cBitPosition = {2, 1, 0};
    uint8_t              cResetMask   = 0;
    std::vector<uint8_t> cMasters     = {0, 1, 2};
    for(const auto& cMaster: cMasters) cResetMask |= (1 << cBitPosition[cMaster]);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, 0);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, cResetMask);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, 0);

    for(const auto& cMaster: cMasters) { WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["I2CM" + std::to_string(cMaster) + "Config"].fAddress, 1 << 5 | 1 << 3); }
}

void WorkerTester::SetHybridClocks(OpticalGroup* cOpticalGroup)
{
    ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
    // enabling CIC clock
    // clk 6
    LOG(INFO) << "Enabling CIC clock" << RESET;
    uint8_t cInvert = 0, cDriveStr = 7, cFrequency = 4;
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK6ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK6ChnCntrL"].fAddress, 0);
    // clk26
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK26ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK26ChnCntrL"].fAddress, 0);
    // clk1
    cInvert = 1, cDriveStr = 7, cFrequency = 4;
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK1ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK1ChnCntrL"].fAddress, 0);
    // clk11
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK11ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK11ChnCntrL"].fAddress, 0);
}

void WorkerTester::EnableHybridChips(OpticalGroup* cOpticalGroup)
{
    ChipRegMap           clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
    std::vector<uint8_t> cGPIOs       = {0, 1, 3, 6, 9, 12};
    LOG(INFO) << "Setting GPIO direction" << RESET;
    uint8_t cDirH = ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIODirH"].fAddress);
    uint8_t cDirL = ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIODirL"].fAddress);
    for(auto cGPIO: cGPIOs)
    {
        if(cGPIO < 8)
            cDirL = (cDirL & ~(1 << cGPIO)) | (1 << cGPIO);
        else
            cDirH = (cDirH & ~(1 << (cGPIO - 8))) | (1 << (cGPIO - 8));
    }
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIODirH"].fAddress, cDirH);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIODirL"].fAddress, cDirL);

    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // reset toggling
    LOG(INFO) << "Enabling Chips" << RESET;
    uint8_t cOutH = ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIOOutH"].fAddress);
    uint8_t cOutL = ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIOOutL"].fAddress);
    for(auto cGPIO: cGPIOs)
    {
        if(cGPIO < 8)
            cOutL = (cOutL & ~(1 << cGPIO)) | (1 << cGPIO);
        else
            cOutH = (cOutH & ~(1 << (cGPIO - 8))) | (1 << (cGPIO - 8));
    }
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIOOutH"].fAddress, cOutH);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PIOOutL"].fAddress, cOutL);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}

void WorkerTester::EnableMPAClocks(OpticalGroup* cOpticalGroup)
{
    LOG(INFO) << "Enabling clocks to MPA" << RESET;
    for(auto cHybrid: *cOpticalGroup)
    {
        for(auto cChip: *cHybrid)
        {
            if(cChip->getFrontEndType() != FrontEndType::SSA) continue;
            ChipRegMap cChipRegMap = cChip->getRegMap();
            WriteFERegister(cChip, cChipRegMap["SLVS_pad_current"].fAddress, 0x7);
        }
    }
}

void WorkerTester::PrepareForTests()
{
    // ResetCPB();
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        cFWInterface->ConfigureBoard(cBoard);
        SetLpGbtVersion(fLpGbtVers);
        for(auto cOpticalGroup: *cBoard)
        {
            if(cOpticalGroup->flpGBT == nullptr) throw std::runtime_error("Missing lpGBT");
            ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
            if(ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PUSMStatus"].fAddress) != 18) throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));

            LOG(INFO) << BOLDGREEN << "lpGBT Configured [READY]" << RESET;
            ResetI2CMasters(cOpticalGroup);
            SetHybridClocks(cOpticalGroup);
            EnableHybridChips(cOpticalGroup);
            EnableMPAClocks(cOpticalGroup);
        }
    }
}

bool WorkerTester::TestI2CRead(OpticalGroup* cOpticalGroup)
{
    LOG(INFO) << BOLDMAGENTA << "Testing I2C Read on OpticalGroup " << cOpticalGroup->getId() << RESET;
    std::vector<uint8_t> cMasters          = {2};
    uint8_t              cSlaveAddress     = 0x60; // CIC
    uint16_t             cRegisterAddress  = 0x99; // Calibration Pattern 0 : Default = 0xA1
    uint8_t              cRegisterValue    = 0xCC;
    int                  cRegisterReadBack = -1;
    for(auto cMaster: cMasters)
    {
        uint16_t cInvertedRegister = ((cRegisterAddress & (0xFF << 8 * 0)) << 8) | ((cRegisterAddress & (0xFF << 8 * 1)) >> 8);
        uint32_t cSlaveData        = (cRegisterValue << 16) | cInvertedRegister;
        LOG(INFO) << BLUE << "Writing value = 0x" << std::hex << +cRegisterValue << std::dec << " to register 0x" << std::hex << +cRegisterAddress << std::dec << RESET;
        uint8_t cNbytes       = 3;
        uint8_t cMasterConfig = (cNbytes << 2) | 3;
        I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress, cSlaveData);

        cSlaveData    = cInvertedRegister;
        cNbytes       = 2;
        cMasterConfig = (cNbytes << 2) | 3;
        I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress, cSlaveData);
        cNbytes           = 1;
        cMasterConfig     = (cNbytes << 2) | 3;
        cRegisterReadBack = I2CRead(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress);
        LOG(INFO) << MAGENTA << "Reading value = 0x" << std::hex << +cRegisterReadBack << std::dec << " from register 0x" << std::hex << +cRegisterAddress << std::dec << RESET;
    }
    return (cRegisterValue == cRegisterReadBack);
}

bool WorkerTester::TestI2CRead()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestI2CRead(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

bool WorkerTester::TestI2CWrite(OpticalGroup* cOpticalGroup)
{
    LOG(INFO) << BOLDMAGENTA << "Testing I2C Write on OpticalGroup " << cOpticalGroup->getId() << RESET;
    std::vector<uint8_t> cMasters         = {2};
    uint8_t              cSlaveAddress    = 0x20;
    uint16_t             cRegisterAddress = 0x1018;
    uint8_t              cRegisterValue   = 0x07;
    bool                 cSuccess         = false;
    for(auto cMaster: cMasters)
    {
        uint16_t cInvertedRegister = ((cRegisterAddress & (0xFF << 8 * 0)) << 8) | ((cRegisterAddress & (0xFF << 8 * 1)) >> 8);
        uint32_t cSlaveData        = (cRegisterValue << 16) | cInvertedRegister;
        uint8_t  cMasterConfig     = (3 << 2) | 3;
        // LOG(INFO) << BLUE << "Writing value = 0x" << std::hex << +cRegisterValue << std::dec << " to register 0x" << std::hex << +cRegisterAddress << std::dec << RESET;
        cSuccess = I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 1, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 2, cSlaveData);
        // cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress+3, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 4, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 5, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 6, cSlaveData);
        cSuccess &= I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress + 7, cSlaveData);
        if(cSuccess) { LOG(INFO) << GREEN << "I2C Write status is SUCCESS" << RESET; }
        else
        {
            LOG(INFO) << RED << "I2C Write status is FAILURE" << RESET;
        }
    }
    return cSuccess;
}

bool WorkerTester::TestI2CWrite()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestI2CWrite(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

bool WorkerTester::TestFERead(OpticalGroup* cOpticalGroup)
{
    bool cGlobalSuccess = false;
    for(auto cHybrid: *cOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic != nullptr)
        {
            ChipRegMap cChipRegMap    = cCic->getRegMap();
            uint8_t    cRegisterValue = 0xAA;
            bool       cWriteSuccess  = WriteFERegister(cCic, cChipRegMap["CALIB_PATTERN0"].fAddress, cRegisterValue, true);
            uint8_t    cReadBack      = ReadFERegister(cCic, cChipRegMap["CALIB_PATTERN0"].fAddress);
            bool       cReadSuccess   = (cReadBack == cRegisterValue);
            if(cReadSuccess) { LOG(INFO) << GREEN << "FE Read on CIC is SUCCESS " << RESET; }
            else
            {
                LOG(INFO) << RED << "FE Read on CIC is FAILURE" << RESET;
            }
            cGlobalSuccess = cGlobalSuccess & cWriteSuccess & cReadSuccess;
        }
        for(auto cChip: *cHybrid)
        {
            if((cChip->getId() % 8) > 0) continue;
            ChipRegMap cChipRegMap = cChip->getRegMap();

            if(cChip->getFrontEndType() == FrontEndType::SSA)
            {
                uint8_t cRegisterValue = 0xBB;
                bool    cWriteSuccess  = WriteFERegister(cChip, cChipRegMap["Bias_THDAC"].fAddress, cRegisterValue, true);
                uint8_t cReadBack      = ReadFERegister(cChip, cChipRegMap["Bias_THDAC"].fAddress);
                bool    cReadSuccess   = (cReadBack == cRegisterValue);
                if(cReadSuccess) { LOG(INFO) << GREEN << "FE Read on SSA is SUCCESS " << RESET; }
                else
                {
                    LOG(INFO) << RED << "FE Write on SSA is FAILURE" << RESET;
                }
                cGlobalSuccess = cGlobalSuccess & cWriteSuccess & cReadSuccess;
            }
            else if(cChip->getFrontEndType() == FrontEndType::MPA)
            {
                uint8_t cRegisterValue = 0xCC;
                bool    cWriteSuccess  = WriteFERegister(cChip, cChipRegMap["ThDAC0"].fAddress, cRegisterValue, true);
                uint8_t cReadBack      = ReadFERegister(cChip, cChipRegMap["ThDAC0"].fAddress);
                bool    cReadSuccess   = (cReadBack == cRegisterValue);
                if(cReadSuccess) { LOG(INFO) << GREEN << "FE Read on MPA is SUCCESS " << RESET; }
                else
                {
                    LOG(INFO) << RED << "FE Write on MPA is FAILURE" << RESET;
                }
                cGlobalSuccess = cGlobalSuccess & cWriteSuccess & cReadSuccess;
            }
        }
    }
    return cGlobalSuccess;
}

bool WorkerTester::TestFERead()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestFERead(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

bool WorkerTester::TestFEWrite(OpticalGroup* cOpticalGroup)
{
    bool cGlobalSuccess = false;
    for(auto cHybrid: *cOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic != nullptr)
        {
            ChipRegMap cChipRegMap = cCic->getRegMap();
            bool       cSuccess    = WriteFERegister(cCic, cChipRegMap["CALIB_PATTERN0"].fAddress, 0x55, true);
            if(cSuccess) { LOG(INFO) << GREEN << "FE Write with verify on CIC is SUCCESS " << RESET; }
            else
            {
                LOG(INFO) << RED << "FE Write with verify on CIC is FAILURE" << RESET;
            }
            cGlobalSuccess &= cSuccess;
        }
        for(auto cChip: *cHybrid)
        {
            if((cChip->getId() % 8) > 0) continue;
            ChipRegMap cChipRegMap = cChip->getRegMap();

            if(cChip->getFrontEndType() == FrontEndType::SSA)
            {
                bool cSuccess = WriteFERegister(cChip, cChipRegMap["Bias_THDAC"].fAddress, 0x66, true);
                if(cSuccess) { LOG(INFO) << GREEN << "FE Write with verify on SSA is SUCCESS " << RESET; }
                else
                {
                    LOG(INFO) << RED << "FE Write with verify on SSA is FAILURE" << RESET;
                }
                cGlobalSuccess &= cSuccess;
            }
            else if(cChip->getFrontEndType() == FrontEndType::MPA)
            {
                bool cSuccess = WriteFERegister(cChip, cChipRegMap["ThDAC0"].fAddress, 0x77, true);
                if(cSuccess) { LOG(INFO) << GREEN << "FE Write with verify on MPA is SUCCESS " << RESET; }
                else
                {
                    LOG(INFO) << RED << "FE Write with verify on MPA is FAILURE" << RESET;
                }
                cGlobalSuccess &= cSuccess;
            }
        }
    }
    return cGlobalSuccess;
}

bool WorkerTester::TestFEWrite()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup: *cBoard)
        {
            TestFEWrite(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

void WorkerTester::Benchmark(int pNIterations)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // auto& cChip = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // if(cChip != nullptr)
                for(auto cChip: *cHybrid)
                {
                    // if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
                    ChipRegMap cChipRegMap = cChip->getRegMap();

                    uint8_t  cChipId          = ((cChip->getFrontEndType() == FrontEndType::CIC) || (cChip->getFrontEndType() == FrontEndType::CIC2)) ? 0 : cChip->getId();
                    uint16_t cRegisterAddress = 0;
                    if(cChip->getFrontEndType() == FrontEndType::CIC)
                        cRegisterAddress = cChipRegMap["CALIB_PATTERN0"].fAddress;
                    else if(cChip->getFrontEndType() == FrontEndType::MPA)
                        cRegisterAddress = cChipRegMap["ThDAC0"].fAddress;
                    else if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cRegisterAddress = cChipRegMap["Bias_THDAC"].fAddress;
                    uint16_t cInvertedRegister = ((cRegisterAddress & (0xFF << 8 * 0)) << 8) | ((cRegisterAddress & (0xFF << 8 * 1)) >> 8);
                    uint32_t cSlaveDataRead    = cInvertedRegister;
                    uint8_t  cMaster           = (cChip->getHybridId() == 0) ? 2 : 1;
                    uint8_t  cSlaveAddress     = fChipAddressMap[cChip->getFrontEndType()] + cChipId % 8;

                    LOG(INFO) << BOLDMAGENTA << "Bencharking " << fChipTypeMap[cChip->getFrontEndType()] << "_" << +cChipId << RESET;
                    auto cStart = std::chrono::system_clock::now();
                    for(int cIteration = 0; cIteration < pNIterations; cIteration++)
                    {
                        for(uint8_t cValue = 0; cValue < 255; cValue++)
                        {
                            // std::this_thread::sleep_for(std::chrono::milliseconds(500));
                            bool cSuccess = WriteFERegister(cChip, cRegisterAddress, cValue);
                            if(!cSuccess)
                            {
                                // PrintLpGBTReplyFrame();
                                LOG(INFO) << RED << "Benchmarking iteration " << +cIteration << ": read wrong value using FE functions" << RESET;
                                // ResetCPB();
                                exit(0);
                            }
                        }
                    }
                    auto cEnd      = std::chrono::system_clock::now();
                    auto cDuration = std::chrono::duration_cast<std::chrono::microseconds>(cEnd - cStart);
                    LOG(INFO) << "One FE register write with verification using FE functions takes in average " << (cDuration.count() / (255 * pNIterations)) << " us" << RESET;

                    cStart = std::chrono::system_clock::now();
                    for(int cIteration = 0; cIteration < pNIterations; cIteration++)
                    {
                        for(uint8_t cValue = 0; cValue < 255; cValue++)
                        {
                            // Write
                            uint32_t cSlaveDataWrite = (cValue << 16) | cInvertedRegister;
                            uint8_t  cNbytes         = 3;
                            uint8_t  cMasterConfig   = (cNbytes << 2) | 3;
                            I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress, cSlaveDataWrite);
                            // PrintI2CMasterRegisters(cOpticalGroup->flpGBT, cMaster);
                            // Read
                            cNbytes       = 2;
                            cMasterConfig = (cNbytes << 2) | 3;
                            I2CWrite(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress, cSlaveDataRead);
                            // PrintI2CMasterRegisters(cOpticalGroup->flpGBT, cMaster);
                            cNbytes           = 1;
                            cMasterConfig     = (cNbytes << 2) | 3;
                            uint8_t cReadBack = I2CRead(cOpticalGroup->getId(), cMaster, cMasterConfig, cSlaveAddress);
                            // PrintI2CMasterRegisters(cOpticalGroup->flpGBT, cMaster);
                            if(cReadBack != cValue)
                            {
                                // PrintLpGBTReplyFrame();
                                LOG(INFO) << RED << "Benchmarking iteration " << +cIteration << ": read wrong value using I2C functions" << RESET;
                                // ResetCPB();
                                exit(0);
                            }
                            LOG(DEBUG) << BLUE << "Read using I2C functions value 0x" << std::hex << +cReadBack << std::dec << " from register 0x" << std::hex << +cRegisterAddress << std::dec
                                       << RESET;
                        }
                    }
                    cEnd      = std::chrono::system_clock::now();
                    cDuration = std::chrono::duration_cast<std::chrono::microseconds>(cEnd - cStart);
                    LOG(INFO) << "One FE register write with verification using I2C functions takes in average " << (cDuration.count() / (255 * pNIterations)) << " us" << RESET;
                }
            }
        }
    }
}

void WorkerTester::MeasureIPbusTransaction(int pNIterations)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t          pLinkId      = 0;
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint8_t               cWorkerId = 16 + pLinkId, cFunctionId = 2;
    uint16_t              pRegisterAddress = 0x0140;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);

    double cTotalWrite = 0, cTotalRead = 0;
    for(int i = 0; i < pNIterations; i++)
    {
        auto cWriteStart = std::chrono::system_clock::now();
        WriteCommandCPB(cCommandVector);
        auto   cWriteEnd      = std::chrono::system_clock::now();
        double cWriteDuration = std::chrono::duration_cast<std::chrono::microseconds>(cWriteEnd - cWriteStart).count();
        cTotalWrite += cWriteDuration;

        auto                  cReadStart    = std::chrono::system_clock::now();
        std::vector<uint32_t> cReplyVector  = ReadReplyCPB(1);
        auto                  cReadEnd      = std::chrono::system_clock::now();
        double                cReadDuration = std::chrono::duration_cast<std::chrono::microseconds>(cReadEnd - cReadStart).count();
        cTotalRead += cReadDuration;
    }
    LOG(INFO) << "One IPbus command write transaction takes in average " << (cTotalWrite / pNIterations) << " us" << RESET;
    LOG(INFO) << "One IPbus command read transaction takes in average " << (cTotalRead / pNIterations) << " us" << RESET;
}

void WorkerTester::PrintI2CMasterRegisters(Ph2_HwDescription::Chip* pChip, uint8_t pMaster)
{
    ChipRegMap cChipRegMap = pChip->getRegMap();
    uint8_t    cLinkId     = pChip->getOpticalGroupId();
    LOG(INFO) << "I2CM" << +pMaster << "Address = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Address"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Data0 = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Data0"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Data1 = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Data1"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Data2 = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Data2"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Data3 = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Data3"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Cmd = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Cmd"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Ctrl = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Ctrl"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "Status = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "Status"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "TranCnt = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "TranCnt"].fAddress) << std::dec << RESET;
    LOG(INFO) << "I2CM" << +pMaster << "ReadByte = 0x" << std::hex << +ReadLpGBTRegister(cLinkId, cChipRegMap["I2CM" + std::to_string(pMaster) + "ReadByte"].fAddress) << std::dec << RESET;
}

void WorkerTester::PrintLpGBTReplyFrame()
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    uint8_t cWord0 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block0.word0");
    LOG(INFO) << "Word0 : 0x" << std::hex << +cWord0 << std::dec << RESET;

    uint8_t cWord1 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block0.word1");
    LOG(INFO) << "Word1 : 0x" << std::hex << +cWord1 << std::dec << RESET;

    uint8_t cWord2 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block0.word2");
    LOG(INFO) << "Word2 : 0x" << std::hex << +cWord2 << std::dec << RESET;

    uint8_t cWord3 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block0.word3");
    LOG(INFO) << "Word3 : 0x" << std::hex << +cWord3 << std::dec << RESET;

    uint8_t cWord4 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block1.word4");
    LOG(INFO) << "Word4 : 0x" << std::hex << +cWord4 << std::dec << RESET;

    uint8_t cWord5 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block1.word5");
    LOG(INFO) << "Word5 : 0x" << std::hex << +cWord5 << std::dec << RESET;

    uint8_t cWord6 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block1.word6");
    LOG(INFO) << "Word6 : 0x" << std::hex << +cWord6 << std::dec << RESET;

    uint8_t cWord7 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block1.word7");
    LOG(INFO) << "Word7 : 0x" << std::hex << +cWord7 << std::dec << RESET;

    uint8_t cWord8 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block2.word8");
    LOG(INFO) << "Word8 : 0x" << std::hex << +cWord8 << std::dec << RESET;

    uint8_t cWord9 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block2.word9");
    LOG(INFO) << "Word9 : 0x" << std::hex << +cWord9 << std::dec << RESET;

    uint8_t cWord10 = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.reply_debug_block2.word10");
    LOG(INFO) << "Word10 : 0x" << std::hex << +cWord10 << std::dec << RESET;
}
