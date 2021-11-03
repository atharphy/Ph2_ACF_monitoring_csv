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
    void Reset();
    void ConfigurePrintout(PrintConfig pCnfg);
    void SetROCRegstoPerserve(FrontEndType pType, std::vector<std::string> pListOfRegs);
    void SetBrdRegstoPerserve(std::vector<std::string> pListOfRegs);
    void SetReadoutPause(uint32_t pReadoutPause) { fReadoutPause = pReadoutPause; }
    void ReadDataFromFile(std::string pRawFileName);
    void ContinousReadout();
    void SetName(std::string pName){ fMyName = fMyName;};

protected:
    // success or fail 
    bool     fSuccess{false};
    // chips found on the detector 
    uint8_t  fWithCIC{0};
    uint8_t  fWithLpGBT{0};
    uint8_t  fWithMPA{0};
    uint8_t  fWithSSA{0};
    uint8_t  fWithCBC{0};
    // readout related items 
    uint32_t fNevents{100};
    uint32_t fReadoutPause{100};

    void   PrintData(Ph2_HwDescription::BeBoard* pBoard);
    void   EventPrintout(Ph2_HwDescription::BeBoard* pBoard, Ph2_HwInterface::Event* pEvent);
    void   ContinousReadout(Ph2_HwDescription::BeBoard* pBoard);

  private:
    // Containers
    DetectorDataContainer fBoardRegContainer;
    // configuration of print-out
    PrintConfig fPrintConfig;
    // name of the tool 
    std::string fMyName;

    // list of registers to perserve
    std::vector<std::string> fBrdRegsToPerserve;
    DetectorDataContainer    fROCRegsToPerserve;
};
#endif
