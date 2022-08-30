#ifndef WorkerTester_h__
#define WorkerTester_h__

#include "../HWInterface/D19cOpticalInterface.h"
#include "../HWInterface/D19clpGBTSlowControlWorkerInterface.h"
#include "../Utils/Utilities.h"
#include "Tool.h"
#include <chrono>

using namespace Ph2_HwDescription;

class WorkerTester : public Tool
{
  public:
    WorkerTester(bool pVerbose, uint8_t pLpGbtVers);
    ~WorkerTester();

    void PrepareForTests();

    void SetLpGbtVersion(uint8_t pLpGbtVers);

    bool TestICRead();
    bool TestICWrite();
    bool TestI2CRead();
    bool TestI2CWrite();
    bool TestFERead();
    bool TestFEWrite();

    void Benchmark(int pNIterations);

    void MeasureIPbusTransaction(int pNIterations);

  private:
    bool    fVerbose   = false;
    uint8_t fLpGbtVers = 0;

    void ResetI2CMasters(OpticalGroup* cOpticalGroup);
    void SetHybridClocks(OpticalGroup* cOpticalGroup);
    void EnableHybridChips(OpticalGroup* cOptialGroup);
    void EnableMPAClocks(OpticalGroup* cOpticalGroup);

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
