#ifndef _D19cL1ReadoutInterface_H__
#define __D19cL1ReadoutInterface_H__

#include "L1ReadoutInterface.h"

namespace Ph2_HwInterface
{

class D19cL1ReadoutInterface : public L1ReadoutInterface
{

  public:
    D19cL1ReadoutInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cL1ReadoutInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cL1ReadoutInterface();

  public:
    void FillData() override;
    bool WaitForReadout() override;
    bool WaitForNTriggers() override;
    bool ReadEvents(const Ph2_HwDescription::BeBoard* pBoard) override;
    bool PollReadoutData(const Ph2_HwDescription::BeBoard* pBoard, bool pWait = false ) override;
    
    void SetWait(uint32_t pWait_us) { fWait_us = pWait_us; }

  private:
    bool WaitForData();
    bool CheckReadoutReq(); 
    bool ResetReadout();
    bool CheckForWordsInReadout();
    void CountFwEvents(); 

    uint32_t fWait_us{100};
    uint32_t fReadoutAttempts{0}; 
    uint32_t fDDR3Offset{0};
    uint8_t  fWaitForReadoutReq{0};

};
} // namespace Ph2_HwInterface
#endif
