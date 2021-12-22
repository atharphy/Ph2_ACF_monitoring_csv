#ifndef _D19cPSCounterFWInterface_H__
#define __D19cPSCounterFWInterface_H__

#include "D19cFWInterface.h"
#include <string>


namespace Ph2_HwInterface
{

#ifndef PSCounterData
typedef std::map<uint8_t, std::vector<uint16_t>> PSCounterData;
typedef std::map<uint32_t, PSCounterData>        PSModuleCounterData;
#endif

class D19cPSCounterFWInterface : public D19cFWInterface
{
  public:
    D19cPSCounterFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    D19cPSCounterFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler);

    D19cPSCounterFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    D19cPSCounterFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler);
    ~D19cPSCounterFWInterface();

  public:
    void  ReadNEvents(Ph2_HwDescription::BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait=false);

    void SetPSCounterDelay(uint8_t pDelay) { fPSCounterDelay = pDelay; };
    void SetPSCounterMode(uint8_t pMode) { fPSCounterFast = pMode; };
    void SetPSPairSelect(uint8_t pMode) { fPairSelect = pMode; };

    void SetDuration(uint32_t pDuration){fDuration=pDuration;}
    void SetWait(uint32_t pWait_us){fWait_us=pWait_us;}

    // functions to control injection 
    void PS_Send_pulses(uint32_t pNtriggers, bool manual);
    void PS_Inject();
    void PS_Open_shutter();
    void PS_Close_shutter();
    void PS_Clear_counters();
    void PS_Start_counters_read();

  private:
    uint32_t fDuration{0};
    uint32_t fWait_us{100};
    uint32_t fPSCounterDelay{29};
    uint8_t  fPSCounterFast{0};
    uint8_t  fPairSelect{0};
    uint32_t fReadoutAttempts{0};

    std::vector<uint8_t> fStubBuffer;
    PSModuleCounterData  fPSModulesCounterData;

    void Compose_fast_command(uint32_t resync_en, uint32_t l1a_en, uint32_t cal_pulse_en, uint32_t bc0_en); 

    // function read-back counters
    void ReadMPACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void ReadSSACounters(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    void ReadPSSCCountersFast(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData, uint8_t pRawMode=0);
    bool ReadPSCountersFast(uint8_t pRawMode, size_t pChipId, size_t pHybridId);
    bool CheckStartPattern();
    bool DecodeRawCounterDataPS(PSCounterData& pFeCounters, std::vector<uint8_t> pIds);
    
    // data get/wait functions  
    uint32_t GetData(Ph2_HwDescription::BeBoard* pBoard, std::vector<uint32_t>& pData);
    bool     WaitForData(Ph2_HwDescription::BeBoard* pBoard);
    
};
} // namespace Ph2_HwInterface
#endif
