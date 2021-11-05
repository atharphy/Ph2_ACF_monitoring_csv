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

#include <signal.h>
#include "Tool.h"
using namespace Ph2_HwDescription;

struct PrintConfig
{
    uint8_t  fVerbose    = 0;
    uint32_t fPrintEvery = 1;
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
    void SetName(std::string pName) { fMyName = pName; };
    void TriggerMonitor(uint32_t pDelta_s=1); 
    static void StopTriggerMonitor(int signum){ LOG (INFO) << BOLDRED << "Caught Ctrl+C from command line [signum == " << signum << RESET; throw Exception("Ctrl+C caught from command line..");};

  protected:
    // success or fail
    bool fSuccess{false};
    // chips found on the detector
    uint8_t fWithCIC{0};
    uint8_t fWithLpGBT{0};
    uint8_t fWithMPA{0};
    uint8_t fWithSSA{0};
    uint8_t fWithCBC{0};
    // readout related items
    uint32_t fNevents{100};
    uint32_t fReadoutPause{100};
    uint32_t fEventCounter{0};
    // waits 
    uint32_t fThreadWait{100};//in us
    // stop trigger monitor  
    uint8_t  fStopTriggerMonitor{0};

    void PrintData(Ph2_HwDescription::BeBoard* pBoard);
    void EventPrintout(Ph2_HwDescription::BeBoard* pBoard, Ph2_HwInterface::Event* pEvent);
    void ContinousReadout(Ph2_HwDescription::BeBoard* pBoard);
    void WaitForTriggers(Ph2_HwDescription::BeBoard* pBoard);
    void ContinousReadoutTh(uint8_t cBrdId);
    void CatchStop();

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
