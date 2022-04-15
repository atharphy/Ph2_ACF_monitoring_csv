#ifndef WorkerTester_h__
#define WorkerTester_h__

#include "../Utils/Utilities.h"
#include "Tool.h"
#include <chrono>

using namespace Ph2_HwDescription;

class WorkerTester : public Tool
{
  public:
    WorkerTester(bool pNewImp, bool pVerbose, uint8_t pLpGbtVers);
    ~WorkerTester();

    void PrepareForTests();
    void PrintFSMState();

    void SetLpGbtVersion(uint8_t pLpGbtVers);

    // NEW
    // function for CPB command/reply
    void                  WriteCommandCPB(const std::vector<uint32_t>& pCommandVector);
    std::vector<uint32_t> ReadReplyCPB(uint8_t pNWords);
    void                  ResetCPB();
    // function for IC transactions
    bool    WriteLpGBTRegister_New(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue);
    uint8_t ReadLpGBTRegister_New(uint8_t pLinkId, uint16_t pRegisterValue);
    bool    IsICToolDone();
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite_New(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData);
    uint8_t I2CRead_New(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress);
    bool    IsI2CToolDone();
    // function for front-end slow control
    bool    WriteFERegister_New(Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify = true);
    uint8_t ReadFERegister_New(Chip* pChip, uint16_t pRegisterAddress);
    bool    IsFEToolDone();

    // OLD
    bool    WriteLpGBTRegister_Old(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue);
    uint8_t ReadLpGBTRegister_Old(uint8_t pLinkId, uint16_t pRegisterValue);
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite_Old(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData);
    uint8_t I2CRead_Old(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress);
    // function for front-end slow control
    bool    WriteFERegister_Old(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify = true);
    uint8_t ReadFERegister_Old(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress);

    // Generic
    bool    WriteLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterAddress, uint8_t pRegisterValue);
    uint8_t ReadLpGBTRegister(uint8_t pLinkId, uint16_t pRegisterValue);
    // function for I2C transactions using lpGBT I2C Masters
    bool    I2CWrite(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress, uint32_t pSlaveData);
    uint8_t I2CRead(uint8_t pLinkId, uint8_t pMasterId, uint8_t pMasterConfig, uint8_t pSlaveAddress);
    // function for front-end slow control
    bool    WriteFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress, uint8_t pRegisterValue, bool pVerify = true);
    uint8_t ReadFERegister(Ph2_HwDescription::Chip* pChip, uint16_t pRegisterAddress);

    bool TestICRead();
    bool TestICWrite();
    bool TestI2CRead();
    bool TestI2CWrite();
    bool TestFERead();
    bool TestFEWrite();

    void Benchmark(int pNIterations);

    void MeasureIPbusTransaction(int pNIterations);

  private:
    bool    fNewImp    = true;
    bool    fVerbose   = false;
    uint8_t fLpGbtVers = 0;

    void ResetI2CMasters(OpticalGroup* cOpticalGroup);
    void SetHybridClocks(OpticalGroup* cOpticalGroup);
    void EnableHybridChips(OpticalGroup* cOptialGroup);
    void EnableMPAClocks(OpticalGroup* cOpticalGroup);
    void PrintI2CMasterRegisters(Ph2_HwDescription::Chip* pChip, uint8_t pMaster);
    void PrintLpGBTReplyFrame();

    bool TestICRead(OpticalGroup* cOpticalGroup);
    bool TestICWrite(OpticalGroup* cOpticalGroup);
    bool TestI2CRead(OpticalGroup* cOpticalGroup);
    bool TestI2CWrite(OpticalGroup* cOpticalGroup);
    bool TestFERead(OpticalGroup* cOpticalGroup);
    bool TestFEWrite(OpticalGroup* cOpticalGroup);

    std::map<FrontEndType, uint8_t> fChipCodeMap = {{FrontEndType::CBC3, 1}, {FrontEndType::MPA, 2}, {FrontEndType::SSA, 3}, {FrontEndType::CIC, 4}, {FrontEndType::CIC2, 5}};

    std::map<FrontEndType, uint8_t> fChipAddressMap = {{FrontEndType::CBC3, 0x40}, {FrontEndType::MPA, 0x40}, {FrontEndType::SSA, 0x20}, {FrontEndType::CIC, 0x60}, {FrontEndType::CIC2, 0x60}};

    std::map<FrontEndType, std::string> fChipTypeMap = {{FrontEndType::CBC3, "CBC3"},
                                                        {FrontEndType::MPA, "MPA"},
                                                        {FrontEndType::SSA, "SSA"},
                                                        {FrontEndType::CIC, "CIC1"},
                                                        {FrontEndType::CIC2, "CIC2"}};
};
#endif
