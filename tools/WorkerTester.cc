#include "WorkerTester.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

WorkerTester::WorkerTester(bool pVerbose, uint8_t pLpGbtVers) : Tool(), fVerbose(pVerbose), fLpGbtVers(pLpGbtVers) {}

WorkerTester::~WorkerTester() {}

void WorkerTester::SetLpGbtVersion(uint8_t pLpGbtVers)
{
    fBeBoardInterface->setBoard(0);
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    cFWInterface->WriteReg("fc7_daq_cnfg.optical_block.lpgbt.version", pLpGbtVers);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG(INFO) << YELLOW << "lpGBT version : " << +cFWInterface->ReadReg("fc7_daq_cnfg.optical_block.lpgbt.version") << RESET;
    fLpGbtVers = pLpGbtVers;
}

bool WorkerTester::TestICRead(OpticalGroup* cOpticalGroup)
{
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();
    ChipRegItem cRegItem = cChipRegMap["ConfigPins"];
    // Reading the ConfigPins which is hard wired
    LOG(INFO) << BOLDMAGENTA << "Testing IC Read on OpticalGroup " << cOpticalGroup->getId() << RESET;
    uint8_t  cConfigPinsVal = cFWInterface->SingleRegisterRead(cChip, cRegItem);
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
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();
    ChipRegItem cRegItem = cChipRegMap["I2CM0Address"];
    // Write to I2C Master 0 slave address register and readback
    LOG(INFO) << BOLDMAGENTA << "Testing IC Write on OpticalGroup " << cOpticalGroup->getId() << RESET;
    bool     cSuccess         = true;
    for(uint8_t cIteration = 0; cIteration < 10; cIteration++)
    {
        for(uint8_t i = 0; i < 255; i++)
        { 
            cRegItem.fValue = i;
            cSuccess &= cFWInterface->SingleRegisterWriteRead(cChip, cRegItem); 
        }
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
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();
    ChipRegItem cRegItem = cChipRegMap["RST0"];
    // reset i2C masters
    LOG(INFO) << GREEN << "Reseting I2C Masters" << RESET;
    std::vector<uint8_t> cBitPosition = {2, 1, 0};
    uint8_t              cResetMask   = 0;
    std::vector<uint8_t> cMasters     = {0, 1, 2};
    for(const auto& cMaster: cMasters) cResetMask |= (1 << cBitPosition[cMaster]);
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    cRegItem.fValue = cResetMask;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);

    for(const auto& cMaster: cMasters) 
    { 
        cRegItem = cChipRegMap["I2CM" + std::to_string(cMaster) + "Config"];
        cRegItem.fValue = 1 << 5 | 1 << 3;
        cFWInterface->SingleRegisterWriteRead(cChip, cRegItem); 
    }
}

void WorkerTester::SetHybridClocks(OpticalGroup* cOpticalGroup)
{
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();
    // enabling CIC clock
    // clk 6
    LOG(INFO) << "Enabling CIC clock" << RESET;
    uint8_t cInvert = 0, cDriveStr = 7, cFrequency = 4;
    ChipRegItem cRegItem = cChipRegMap["EPCLK6ChnCntrH"];
    cRegItem.fValue = cInvert << 6 | cDriveStr << 3 | cFrequency;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    //
    cRegItem = cChipRegMap["EPCLK6ChnCntrL"];
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    // clk26
    cRegItem = cChipRegMap["EPCLK26ChnCntrH"];
    cRegItem.fValue = cInvert << 6 | cDriveStr << 3 | cFrequency;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    //
    cRegItem = cChipRegMap["EPCLK26ChnCntrL"];
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    // clk1
    cInvert = 1, cDriveStr = 7, cFrequency = 4;
    cRegItem = cChipRegMap["EPCLK1ChnCntrH"];
    cRegItem.fValue = cInvert << 6 | cDriveStr << 3 | cFrequency;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    //
    cRegItem = cChipRegMap["EPCLK1ChnCntrL"];
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    // clk11
    cRegItem = cChipRegMap["EPCLK11ChnCntrH"];
    cRegItem.fValue = cInvert << 6 | cDriveStr << 3 | cFrequency;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    //
    cRegItem = cChipRegMap["EPCLK11ChnCntrL"];
    cRegItem.fValue = 0;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
}

void WorkerTester::EnableHybridChips(OpticalGroup* cOpticalGroup)
{
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();

    std::vector<uint8_t> cGPIOs       = {0, 1, 3, 6, 9, 12};
    LOG(INFO) << "Setting GPIO direction" << RESET;
    ChipRegItem cRegItem = cChipRegMap["PIODirH"];
    uint8_t cDirH = cFWInterface->SingleRegisterRead(cChip, cRegItem);
    cRegItem = cChipRegMap["PIODirL"];
    uint8_t cDirL = cFWInterface->SingleRegisterRead(cChip, cRegItem);
    for(auto cGPIO: cGPIOs)
    {
        if(cGPIO < 8)
            cDirL = (cDirL & ~(1 << cGPIO)) | (1 << cGPIO);
        else
            cDirH = (cDirH & ~(1 << (cGPIO - 8))) | (1 << (cGPIO - 8));
    }
    cRegItem = cChipRegMap["PIODirH"];
    cRegItem.fValue = cDirH;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
    cRegItem = cChipRegMap["PIODirL"];
    cRegItem.fValue = cDirL;
    cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);

    std::this_thread::sleep_for(std::chrono::microseconds(10));
    // reset toggling
    LOG(INFO) << "Enabling Chips" << RESET;
    cRegItem = cChipRegMap["PIOOutH"];
    uint8_t cOutH = cFWInterface->SingleRegisterRead(cChip, cRegItem);
    cRegItem = cChipRegMap["PIOOutL"];
    uint8_t cOutL = cFWInterface->SingleRegisterRead(cChip, cRegItem);
    for(auto cGPIO: cGPIOs)
    {
        if(cGPIO < 8)
            cOutL = (cOutL & ~(1 << cGPIO)) | (1 << cGPIO);
        else
            cOutH = (cOutH & ~(1 << (cGPIO - 8))) | (1 << (cGPIO - 8));
    }
    cRegItem = cChipRegMap["PIOOutH"];
    cRegItem.fValue = cOutH;
    cFWInterface->SingleRegisterWriteRead(cChip, cChipRegMap["PIOOutH"]);
    cRegItem = cChipRegMap["PIOOutL"];
    cRegItem.fValue = cOutL;
    cFWInterface->SingleRegisterWriteRead(cChip, cChipRegMap["PIOOutL"]);
    std::this_thread::sleep_for(std::chrono::microseconds(10));
}

void WorkerTester::EnableMPAClocks(OpticalGroup* cOpticalGroup)
{
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    LOG(INFO) << "Enabling clocks to MPA" << RESET;
    for(auto cHybrid: *cOpticalGroup)
    {
        for(auto cChip: *cHybrid)
        {
            if(cChip->getFrontEndType() != FrontEndType::SSA) continue;
            ChipRegMap cChipRegMap = cChip->getRegMap();
            ChipRegItem cRegItem = cChipRegMap["SLVS_pad_current"];
            cRegItem.fValue = 0x7;
            cFWInterface->SingleRegisterWrite(cChip, cRegItem, false);
        }
    }
}

void WorkerTester::PrepareForTests()
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        cFWInterface->ConfigureBoard(cBoard);
        SetLpGbtVersion(fLpGbtVers);
        for(auto cOpticalGroup: *cBoard)
        {
            if(cOpticalGroup->flpGBT == nullptr) throw std::runtime_error("Missing lpGBT");
            auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
            ChipRegMap cChipRegMap = cChip->getRegMap();
            ChipRegItem cRegItem = cChipRegMap["PUSMStatus"];
            if(cFWInterface->SingleRegisterRead(cChip, cRegItem) != 18) throw std::runtime_error(std::string("lpGBT Power-Up State Machine NOT DONE"));

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

    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cOpticalInterface* cFEInterface = static_cast<D19cOpticalInterface*>(cFWInterface->getFEConfigurationInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();

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
        cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress, cSlaveData);

        cSlaveData    = cInvertedRegister;
        cNbytes       = 2;
        cMasterConfig = (cNbytes << 2) | 3;
        cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress, cSlaveData);
        cNbytes           = 1;
        cMasterConfig     = (cNbytes << 2) | 3;
        cRegisterReadBack = cFEInterface->SingleByteReadI2C(cChip, cMaster, cMasterConfig, cSlaveAddress);
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
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19cOpticalInterface* cFEInterface = static_cast<D19cOpticalInterface*>(cFWInterface->getFEConfigurationInterface());
    auto cChip = static_cast<Chip*>(cOpticalGroup->flpGBT);
    ChipRegMap cChipRegMap = cChip->getRegMap();

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
        cSuccess = cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 1, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 2, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 4, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 5, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 6, cSlaveData);
        cSuccess &= cFEInterface->MultiByteWriteI2C(cChip, cMaster, cMasterConfig, cSlaveAddress + 7, cSlaveData);
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
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    bool cGlobalSuccess = false;
    for(auto cHybrid: *cOpticalGroup)
    {
        ChipRegItem cRegItem;
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic != nullptr)
        {
            ChipRegMap cChipRegMap    = cCic->getRegMap();
            cRegItem = cChipRegMap["CALIB_PATTERN0"];
            uint8_t cRegisterValue = 0xAA;
            cRegItem.fValue = cRegisterValue;
            bool       cWriteSuccess  = cFWInterface->SingleRegisterWriteRead(cCic, cRegItem);
            uint8_t    cReadBack      = cFWInterface->SingleRegisterRead(cCic, cRegItem);
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
                cRegItem = cChipRegMap["Bias_THDAC"];
                uint8_t cRegisterValue = 0xBB;
                cRegItem.fValue = cRegisterValue;
                bool    cWriteSuccess  = cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
                uint8_t cReadBack      = cFWInterface->SingleRegisterRead(cChip, cRegItem);
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
                cRegItem = cChipRegMap["ThDAC0"];
                uint8_t cRegisterValue = 0xCC;
                cRegItem.fValue = cRegisterValue;
                bool    cWriteSuccess  = cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
                uint8_t cReadBack      = cFWInterface->SingleRegisterRead(cChip, cRegItem);
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
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    bool cGlobalSuccess = false;
    for(auto cHybrid: *cOpticalGroup)
    {
        ChipRegItem cRegItem;
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic != nullptr)
        {
            ChipRegMap cChipRegMap = cCic->getRegMap();
            cRegItem = cChipRegMap["CALIB_PATTERN0"];
            cRegItem.fValue = 0x55;
            bool       cSuccess    = cFWInterface->SingleRegisterWriteRead(cCic, cRegItem);
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
                cRegItem = cChipRegMap["Bias_THDAC"];
                cRegItem.fValue = 0x66;
                bool cSuccess = cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
                if(cSuccess) { LOG(INFO) << GREEN << "FE Write with verify on SSA is SUCCESS " << RESET; }
                else
                {
                    LOG(INFO) << RED << "FE Write with verify on SSA is FAILURE" << RESET;
                }
                cGlobalSuccess &= cSuccess;
            }
            else if(cChip->getFrontEndType() == FrontEndType::MPA)
            {
                cRegItem = cChipRegMap["ThDAC0"];
                cRegItem.fValue = 0x77;
                bool cSuccess = cFWInterface->SingleRegisterWriteRead(cChip, cRegItem);
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

            fBeBoardInterface->setBoard(cBoard->getId());
            D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
            for(auto cHybrid: *cOpticalGroup)
            {
                // auto& cChip = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // if(cChip != nullptr)
                for(auto cChip: *cHybrid)
                {
                    // if(cChip->getFrontEndType() == FrontEndType::MPA) continue;
                    ChipRegMap cChipRegMap = cChip->getRegMap();
                    uint8_t  cChipId          = ((cChip->getFrontEndType() == FrontEndType::CIC) || (cChip->getFrontEndType() == FrontEndType::CIC2)) ? 0 : cChip->getId();
                    ChipRegItem cRegItem;
                    if(cChip->getFrontEndType() == FrontEndType::CIC)
                        cRegItem = cChipRegMap["CALIB_PATTERN0"];
                    else if(cChip->getFrontEndType() == FrontEndType::MPA)
                        cRegItem = cChipRegMap["ThDAC0"];
                    else if(cChip->getFrontEndType() == FrontEndType::SSA)
                        cRegItem = cChipRegMap["Bias_THDAC"];

                    LOG(INFO) << BOLDMAGENTA << "Bencharking " << fChipTypeMap[cChip->getFrontEndType()] << "_" << +cChipId << RESET;
                    std::vector<ChipRegItem> cRegItems;
                    auto cStart = std::chrono::system_clock::now();
                    for(int cIteration = 0; cIteration < pNIterations; cIteration++)
                    {
                        for(uint8_t cValue = 0; cValue < 255; cValue++)
                        {
                            cRegItem.fValue = cValue;
                            cRegItems.push_back(cRegItem);
                        }
                    }
                    bool cSuccess = cFWInterface->MultiRegisterWriteRead(cChip, cRegItems);
                    auto cEnd      = std::chrono::system_clock::now();
                    auto cDuration = std::chrono::duration_cast<std::chrono::microseconds>(cEnd - cStart);
                    LOG(INFO) << "One FE register write with verification using FE functions takes in average " << (cDuration.count() / (255 * pNIterations)) << " us" << RESET;
                }
            }
        }
    }
}

void WorkerTester::MeasureIPbusTransaction(int pNIterations)
{
    D19cFWInterface* cFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    D19clpGBTSlowControlWorkerInterface* cSlowControlInterface = static_cast<D19clpGBTSlowControlWorkerInterface*>(cFWInterface->getlpGBTSlowControlInterface());
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
        cSlowControlInterface->WriteCommand(cCommandVector);
        auto   cWriteEnd      = std::chrono::system_clock::now();
        double cWriteDuration = std::chrono::duration_cast<std::chrono::microseconds>(cWriteEnd - cWriteStart).count();
        cTotalWrite += cWriteDuration;

        auto                  cReadStart    = std::chrono::system_clock::now();
        std::vector<uint32_t> cReplyVector  = cSlowControlInterface->ReadReply(1);
        auto                  cReadEnd      = std::chrono::system_clock::now();
        double                cReadDuration = std::chrono::duration_cast<std::chrono::microseconds>(cReadEnd - cReadStart).count();
        cTotalRead += cReadDuration;
    }
    LOG(INFO) << "One IPbus command read transaction takes in average " << (cTotalRead / pNIterations) << " us" << RESET;
}
