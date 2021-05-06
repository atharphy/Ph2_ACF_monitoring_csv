/*!

        \file                   OTHybridTester.h
        \brief                  Base class for all hybrids test using a testcard
        \author                 Younes OTARID
        \version                1.0
        \date                   17/12/2020
        Support :               mail to : younes.otarid@cern.ch

 */

#ifndef OTHybridTester_h__
#define OTHybridTester_h__

#include "../HWInterface/DPInterface.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "TAxis.h"
#include "TGraph.h"
#include "TH2I.h"
#include "TMultiGraph.h"
#include "TObject.h"
#include "TString.h"
#include "TTree.h"
#endif

#ifdef __TCUSB__
#include "USB_a.h"
#endif

#include <map>
#include <string>

using namespace Ph2_HwDescription;

class OTHybridTester : public Tool
{
  public:
    OTHybridTester();
    ~OTHybridTester();

    void FindUSBHandler();
#ifdef __TCUSB__
#ifdef __ROH_USB__
    TC_PSROH* GetTCUSBHandler() { return fTC_USB; }
#elif __SEH_USB__
    TC_2SSEH* GetTCUSBHandler() { return fTC_USB; }
#endif
#endif

    // ###################################
    // # LpGBT related functions #
    // ###################################
    // Test lpGBT Up Link with internal pattern
    void LpGBTInjectULInternalPattern(uint32_t pPattern);
    // Test lpGBT Up Link with external pattern
    void LpGBTInjectULExternalPattern(bool pStart, uint8_t pPattern);
    // Check Up Link data in backend fc7
    void LpGBTCheckULPattern(bool pIsExternal = false);
    // Test lpGBT Down Link with internal pattern (Hybrid Fast Command)
    void LpGBTInjectDLInternalPattern(uint8_t pPattern);
    // Test lpGBT I2C Masters
    bool LpGBTTestI2CMaster(const std::vector<uint8_t>& pMasters);
    // Test lpGBT ADC
    void LpGBTTestADC(const std::vector<std::string>& pADCs, uint32_t pMinDAC, uint32_t pMaxDAC, uint32_t pStep);
    // Set GPIO level
    void LpGBTSetGPIOLevel(const std::vector<uint8_t>& pGPIOs, uint8_t Level);
    // Run Eye Openin Monitor
    void LpGBTRunEyeOpeningMonitor(uint8_t pEndOfCountSelect);
    // Run Bit Error Rate Test
    void LpGBTRunBitErrorRateTest(uint8_t pCoarseSource, uint8_t pFineSource, uint8_t pMeasTime, uint32_t pPattern = 0x00000000);

  private:
  protected:
#ifdef __TCUSB__
#ifdef __ROH_USB__
    TC_PSROH* fTC_USB;
#elif __SEH_USB__
    TC_2SSEH* fTC_USB;
#endif
#endif
};

#endif
