#ifndef _D19cPSCounterFWInterface_H__
#define __D19cPSCounterFWInterface_H__

#include "L1ReadoutInterface.h"
#include "FEConfigurationInterface.h"
#include "FastCommandInterface.h"

namespace Ph2_HwInterface
{
#ifndef PSCounterData
typedef std::map<uint32_t, std::vector<uint16_t>> PSCounterData; // counter data per FEId 
#endif

class D19cPSCounterFWInterface : public L1ReadoutInterface
{

  public:
    D19cPSCounterFWInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cPSCounterFWInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);
    ~D19cPSCounterFWInterface();

  public:
    void FillData() override;

    void ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait = false);

    void SetPSCounterDelay(uint8_t pDelay) { fPSCounterDelay = pDelay; };
    void SetPSCounterMode(uint8_t pMode) { fPSCounterFast = pMode; };
    void SetPSPairSelect(uint8_t pMode) { fPairSelect = pMode; };

    void SetDuration(uint32_t pDuration) { fDuration = pDuration; }
    void SetWait(uint32_t pWait_us) { fWait_us = pWait_us; }

    // functions to control injection
    void PS_Send_pulses(uint32_t pNtriggers, bool manual);
    void PS_Inject();
    void PS_Open_shutter();
    void PS_Close_shutter();
    void PS_Clear_counters();
    void PS_Start_counters_read();

    // function to link FEConfigurationInterface 
    void LinkFEConfigurationInterface(FEConfigurationInterface* pInterface) {fFEConfigurationInterface=pInterface;}
    void LinkFastCommandInterface(FastCommandInterface* pInterface) {fFastCommandInterface=pInterface;}

  private:
    FEConfigurationInterface* fFEConfigurationInterface; 
    FastCommandInterface*    fFastCommandInterface;

    uint32_t fDuration{0};
    uint32_t fWait_us{100};
    uint32_t fPSCounterDelay{29};
    uint8_t  fPSCounterFast{0};
    uint8_t  fPairSelect{0};
    uint32_t fReadoutAttempts{0};

    std::vector<uint8_t> fStubBuffer;
    PSCounterData         fPSCounterData;
    uint32_t Compose_Id( Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::OpticalGroup* pGroup, Ph2_HwDescription::Hybrid* pHybrid, Ph2_HwDescription::Chip* pChip);

    // function read-back counters
    void ReadMPACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void ReadSSACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void ReadPSSCCountersFast(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData, uint8_t pRawMode = 0);
    bool ReadPSCountersFast(uint8_t pRawMode, size_t pChipId, size_t pHybridId);
    
    // 
    bool CheckStartPattern();

    // data get/wait functions
    bool     WaitForData();
};
} // namespace Ph2_HwInterface
#endif
