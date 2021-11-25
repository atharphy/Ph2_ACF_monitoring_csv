#include "WorkerTester.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

WorkerTester::WorkerTester() : Tool() {}

WorkerTester::~WorkerTester() {}

void WorkerTester::PrintFSMState(uint8_t pLinkId)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    uint32_t cStatus = cFWInterface->ReadReg("fc7_daq_stat.command_processor_block.worker.lpgbtsc_fsm_state");
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
    //this->WriteStackReg({{"fc7_daq_ctrl.command_processor_block.cpb_ctrl_reg.core_reset", 0x01}, {"fc7_daq_ctrl.command_processor_block.cpb_ctrl_reg.core_reset",0x00}});
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | 16 << 0);
    cFWInterface->WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", cCommandVector);
    cFWInterface->ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(0));
}

void WorkerTester::WriteCommandCPB(const std::vector<uint32_t>& pCommandVector, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    uint8_t cWordIndex = 0;
    if(pVerbose)
    {
        LOG(INFO) << "----------------------" << RESET;
        for(auto cCommandWord: pCommandVector)
        {
            LOG(INFO) << GREEN << "Write command word " << +cWordIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cCommandWord << std::dec << RESET;
            cWordIndex++;
        }
    }
    cFWInterface->WriteBlockReg("fc7_daq_ctrl.command_processor_block.cpb_command_fifo", pCommandVector);
    std::this_thread::sleep_for(std::chrono::microseconds(0));
}

std::vector<uint32_t> WorkerTester::ReadReplyCPB(uint8_t pNWords, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    std::vector<uint32_t> cReplyVector = cFWInterface->ReadBlockReg("fc7_daq_ctrl.command_processor_block.cpb_reply_fifo", pNWords);
    uint8_t               cFifoIndex   = 0;
    if(pVerbose)
    {
        for(auto cReplyWord: cReplyVector)
        {
            LOG(INFO) << YELLOW << "Read reply word " << +cFifoIndex << " value 0x" << std::setfill('0') << std::setw(8) << std::hex << +cReplyWord << std::dec << RESET;
            cFifoIndex++;
        }
    }
    return cReplyVector;
}

uint8_t WorkerTester::ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 2;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
    uint8_t               cReadBack        = cReplyVector[0] & 0xFF;
    if(pVerbose) 
    {
        LOG(INFO) << BOLDMAGENTA << "ReadLpGBTRegister from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
        PrintFSMState(pLinkId);
    }
    return cReadBack;
}

bool WorkerTester::WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 3;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pRegisterAddress << 0);
    cCommandVector.push_back(pRegisterValue << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector     = ReadReplyCPB(1);
    uint8_t cReadBack = cReplyVector[0] & 0xFF;
    if(pVerbose) 
    {
        LOG(INFO) << BOLDMAGENTA << "WriteLpGBTRegister from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
        PrintFSMState(pLinkId);
    }
    return (cReadBack == pRegisterValue);
}

uint8_t WorkerTester::I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint8_t pNBytes, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 4, cMasterConfig = (pNBytes << 2) | 3;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | cMasterConfig << 6);
    cCommandVector.push_back(pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cReadBack = cReplyVector[0] & 0xFF;
    if(pVerbose) 
    {
        LOG(INFO) << BOLDMAGENTA << "I2CRead from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
        PrintFSMState(pLinkId);
    }
    return cReadBack;
}

bool WorkerTester::I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pSlaveAddress, uint32_t pSlaveData, uint8_t pNBytes, bool pVerbose)
{
    D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    cFWInterface->WriteReg("fc7_daq_cnfg.command_processor_block.link_select", pLinkId);
    // ResetCPB();
    uint8_t cWorkerId = 16 + pLinkId, cFunctionId = 5, cMasterConfig = (pNBytes << 2) | 3;
    std::vector<uint32_t> cCommandVector;
    cCommandVector.clear();
    cCommandVector.push_back(cWorkerId << 24 | cFunctionId << 16 | pMasterId << 14 | cMasterConfig << 6);
    cCommandVector.push_back(pSlaveData << 8 | pSlaveAddress << 0);
    WriteCommandCPB(cCommandVector);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::vector<uint32_t> cReplyVector = ReadReplyCPB(1);
    uint8_t cI2CStatus = cReplyVector[0] & 0xFF;
    if(pVerbose) 
    {
        LOG(INFO) << BOLDMAGENTA << "I2CWrite from Link#" << +pLinkId << " -- workerId is " << +cWorkerId << RESET;
        PrintFSMState(pLinkId);
    }
    return (cI2CStatus == 4);
}

bool WorkerTester::TestICRead(OpticalGroup* cOpticalGroup)
{
    //Reading the ConfigPins which is hard wired
    LOG(INFO) << BOLDMAGENTA << "Testing IC Read on OpticalGroup " << cOpticalGroup->getId() << RESET;
    uint16_t cConfigPinsReg = 0x140;
    uint8_t cConfigPinsVal = ReadLpGBTRegister(cOpticalGroup->getId(), cConfigPinsReg);
    LOG(INFO) << YELLOW << "LpGBT Mode = " << WHITE << ((cConfigPinsVal & 0xF0) >> 4) << RESET;
    LOG(INFO) << "----------------------" << RESET;
    return true;
}

bool WorkerTester::TestICRead()
{
    for(auto cBoard : *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup : *cBoard)
        {
            TestICRead(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}

bool WorkerTester::TestICWrite(OpticalGroup* cOpticalGroup)
{
    //Write to I2C Master 0 slave address register and readback
    LOG(INFO) << BOLDMAGENTA << "Testing IC Write on OpticalGroup " << cOpticalGroup->getId() << RESET;
    uint16_t cI2CM0AddressReg = 0x0f1;
    bool cSuccess = false;
    for(uint8_t i = 0; i<10; i++)
    {
        cSuccess = WriteLpGBTRegister(cOpticalGroup->getId(), cI2CM0AddressReg, i);
    }
    if(cSuccess){ LOG(INFO) << GREEN << "Successfully written and checked all values" << RESET;}
    else{ LOG(INFO) << RED << "Failed on at least one write and check" << RESET;}
    return cSuccess;
}

bool WorkerTester::TestICWrite()
{
    for(auto cBoard : *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup : *cBoard)
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
    //reset i2C masters
    LOG(INFO) << GREEN << "Reseting I2C Masters" << RESET;
    std::vector<uint8_t> cBitPosition = {2, 1, 0};
    uint8_t              cResetMask   = 0;
    std::vector<uint8_t> cMasters = {0,1,2};
    for(const auto& cMaster: cMasters) cResetMask |= (1 << cBitPosition[cMaster]);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, 0);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, cResetMask);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["RST0"].fAddress, 0);
}

void WorkerTester::SetHybridClocks(OpticalGroup* cOpticalGroup)
{
    ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
    //enabling CIC clock
    //clk 6
    LOG(INFO) << "Enabling CIC clock" << RESET;
    uint8_t cInvert = 0, cDriveStr = 7, cFrequency = 4;
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK6ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK6ChnCntrL"].fAddress, 0);
    //clk26
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK26ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK26ChnCntrL"].fAddress, 0);
    //clk1
    cInvert = 1, cDriveStr = 7, cFrequency = 4;
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK1ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK1ChnCntrL"].fAddress, 0);
    //clk11
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK11ChnCntrH"].fAddress, cInvert << 6 | cDriveStr << 3 | cFrequency);
    WriteLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["EPCLK11ChnCntrL"].fAddress, 0);
}

void WorkerTester::EnableHybridChips(OpticalGroup* cOpticalGroup)
{
    ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
    std::vector<uint8_t> cGPIOs = {0,1,3,6,9,12};
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
    //reset toggling
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

void WorkerTester::PrepareForTests()
{
    for(auto cBoard : *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        D19cFWInterface* cFWInterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        cFWInterface->ConfigureBoard(cBoard);
        for(auto cOpticalGroup : *cBoard)
        {
            if(cOpticalGroup->flpGBT == nullptr) throw std::runtime_error("Missing lpGBT");
            ChipRegMap clpGBTRegMap = cOpticalGroup->flpGBT->getRegMap();
            if( ReadLpGBTRegister(cOpticalGroup->getId(), clpGBTRegMap["PUSMStatus"].fAddress) != 18) 
                throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));
            
            LOG(INFO) << BOLDGREEN << "lpGBT Configured [READY]" << RESET;
            ResetI2CMasters(cOpticalGroup);
            SetHybridClocks(cOpticalGroup);
            EnableHybridChips(cOpticalGroup);
        }
    }
}

bool WorkerTester::TestI2CRead(OpticalGroup* cOpticalGroup)
{
    LOG(INFO) << BOLDMAGENTA << "Testing I2C Write on OpticalGroup " << cOpticalGroup->getId() << RESET;
    std::vector<uint8_t> cMasters = {2};
    uint8_t cSlaveAddress = 0x60; //CIC
    uint8_t cRegisterAddress = 0x99; //Calibration Pattern 0 : Default = 0xA1
    uint8_t cRegisterValue = 0xCC; 
    int cRegisterReadBack = -1;
    for(auto cMaster : cMasters)
    {
        
        uint16_t cInvertedRegister = ((cRegisterAddress & (0xFF << 8 * 0)) << 8) | ((cRegisterAddress & (0xFF << 8 * 1)) >> 8);
        uint32_t cSlaveData                 = (cRegisterValue << 16) | cInvertedRegister;
        LOG(INFO) << BLUE << "Writing value = 0x" << std::hex << +cRegisterValue << std::dec << " to register 0x" << std::hex << +cRegisterValue << std::dec << RESET;
        I2CWrite(cOpticalGroup->getId(), cMaster, cSlaveAddress, cSlaveData, 3);

        cSlaveData                 = cInvertedRegister;
        I2CWrite(cOpticalGroup->getId(), cMaster, cSlaveAddress, cSlaveData, 2);
        cRegisterReadBack = I2CRead(cOpticalGroup->getId(), cMaster, cSlaveAddress, 1);
        LOG(INFO) << MAGENTA << "Reading value = 0x" << std::hex << +cRegisterValue << std::dec << " from register 0x" << std::hex << +cRegisterValue << std::dec << RESET;
    }
    return (cRegisterValue == cRegisterReadBack);
}

bool WorkerTester::TestI2CRead()
{
    for(auto cBoard : *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup : *cBoard)
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
    std::vector<uint8_t> cMasters = {2};
    uint8_t cSlaveAddress = 0x60; //CIC
    uint8_t cRegisterAddress = 0x99; //Calibration Pattern 0 : Default = 0xA1
    uint8_t cRegisterValue = 0xCC; 
    bool cSuccess = false;
    for(auto cMaster : cMasters)
    {
        uint16_t cInvertedRegister = ((cRegisterAddress & (0xFF << 8 * 0)) << 8) | ((cRegisterAddress & (0xFF << 8 * 1)) >> 8);
        uint32_t cSlaveData                 = (cRegisterValue << 16) | cInvertedRegister;
        LOG(INFO) << BLUE << "Writing value = 0x" << std::hex << +cRegisterValue << std::dec << " to register 0x" << std::hex << +cRegisterValue << std::dec << RESET;
        cSuccess = I2CWrite(cOpticalGroup->getId(), cMaster, cSlaveAddress, cSlaveData, 3);
        if(cSuccess){ LOG(INFO) << GREEN << "I2C Write status is SUCCESS" << RESET;}
        else{ LOG(INFO) << RED << "I2C Write status is FAILURE" << RESET; }
    }
    return cSuccess;
}

bool WorkerTester::TestI2CWrite()
{
    for(auto cBoard : *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        for(auto cOpticalGroup : *cBoard)
        {
            TestI2CWrite(cOpticalGroup);
            LOG(INFO) << "\n" << RESET;
        }
    }
    return true;
}






