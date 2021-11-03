/*!
 *
 * \file LinkAlignment.h
 * \brief Link alignment class, automated alignment procedure for CIC-lpGBT-BE
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef OTTool_h__
#define OTTool_h__

#include "Tool.h"
using namespace Ph2_HwDescription;

struct PrintConfig
{
    uint8_t  fVerbose=0;
    uint32_t fPrintEvery=1;
};

class OTTool : public Tool
{
  public:
    OTTool();
    ~OTTool();

    // expect thise to be the same for 
    void Prepare();
    virtual void Running() ;
    virtual void Stop() ;
    virtual void Pause() ;
    virtual void Resume() ;
    void Reset();
    void ConfigurePrintout(PrintConfig pCnfg);
    void SetROCRegstoPerserve(FrontEndType pType, std::vector<std::string> pListOfRegs);
    void SetBrdRegstoPerserve(std::vector<std::string> pListOfRegs);
    void SetReadoutPause(uint32_t pReadoutPause) { fReadoutPause = pReadoutPause; }
    void ReadDataFromFile(std::string pRawFileName);
    void ContinousReadout();

protected:
  private:
    // Containers
    DetectorDataContainer fBoardRegContainer;
    bool     fSuccess{false};
    uint8_t  fWithCIC{0};
    uint32_t fNevents{100};
    uint16_t fOptimalLatency;
    uint32_t fReadoutPause{100};

    // configuration of print-out
    PrintConfig fPrintConfig;

    // list of registers to perserve
    std::vector<std::string> fBrdRegsToPerserve;
    DetectorDataContainer    fROCRegsToPerserve;

    // void   ContinousReadout(Ph2_HwDescription::BeBoard* pBoard);
    void   PrintData(Ph2_HwDescription::BeBoard* pBoard);
    void   EventPrintout(Ph2_HwDescription::BeBoard* pBoard, Ph2_HwInterface::Event* pEvent);
    void   ContinousReadout(Ph2_HwDescription::BeBoard* pBoard);
    
};
#endif
