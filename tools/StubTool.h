#ifndef _MYTOOL_H__
#define _MYTOOL_H__

#include "Tool.h"
#ifdef __USE_ROOT__
#include "../Utils/CommonVisitors.h"
#include "../Utils/Visitor.h"
#include "Channel.h"
#include <TGraphErrors.h>

#include "TCanvas.h"
#include "TLine.h"
#include "TString.h"
#include <TF1.h>
#include <TH2.h>

using namespace Ph2_System;

class StubTool : public Tool
{
  public:
    StubTool();
    ~StubTool() {}

    void Initialize();
    void maskChannel(uint8_t iChan);
    void scanStubs();
    void scanStubs_wNoise();
    void scanStubs_ptWidth();
    void scanStubs_swap();
    void scanStubs_clusterWidth(unsigned int teststrip);
    void scanStubs_SoF(unsigned int teststrip);

  private:
    void CheckCbcReg(Ph2_HwDescription::Chip* pCbc);
    void setDelayAndTestGroup(uint32_t pDelayns, uint8_t cTestGroup);
    void parseSettings();
    void setInitialOffsets();

    void maskChannel(Ph2_HwDescription::Chip* pCbc, uint8_t iChan, bool mask = true);

    uint16_t encodeId(uint8_t pFeId, uint8_t pCbcId);

    // from the settings map
    bool     fHoleMode;
    uint32_t fNevents;
    uint8_t  fTPAmplitude;

    std::vector<uint8_t>      findChannelsInTestGroup(uint8_t pTestGroup);
    std::map<double, uint8_t> fWindowOffsetMapCBC3 = {
        {0.0, 0x00},
        {0.5, 0x01},
        {1.0, 0x02},
        {1.5, 0x03},
        {2.0, 0x04},
        {2.5, 0x05},
        {3.0, 0x06},
        {-0.5, 0x0f},
        {-1.0, 0x0e},
        {-1.5, 0x0d},
        {-2.0, 0x0c},
        {-2.5, 0x0b},
        {-3.0, 0x0a}
        // { -3.0 , 0x06}, {-2.5, 0x05 } , {-2.0 , 0x04} , {-1.5, 0x03} , {-1.0, 0x02} , {-0.5,0x01} , {0,0x00},
        //         // { 3.0  , 0x0a} , {2.5 , 0x0b} , {2.0 , 0x0c } , {1.5,0x0d}, {1.0,0x0e}  , {0.5, 0x0f}
        //             };
    };
    void setCorrelationWinodwOffsets(Ph2_HwDescription::Chip* pCbc, double pOffsetR1, double pOffsetR2, double pOffsetR3, double pOffsetR4);

    // method to configure test pulse on the CBC
    void configureTestPulse(Ph2_HwDescription::Chip* pCbc, uint8_t pPulseState);

    // to hold the original register values
    std::map<Ph2_HwDescription::Chip*, uint8_t> fStubLogicValue;
    std::map<Ph2_HwDescription::Chip*, uint8_t> fHIPCountValue;

    double Decoding_stub1(int Stub_pos);
    double Decoding_stub2(int Stub_pos);
    double Decoding_stub3(int Stub_pos);
    double Decoding_stub4(int Stub_pos);

    // for the setup
    uint8_t nChan;
    uint8_t fChan;
    uint8_t fTestGroup;
    int     cst;
    int     tmin;
    int     tmax;
    // class to hold histogram for SCurve

    // Channel* fChannel;
    std::vector<Channel*> fChannelVector;

    // for our convenience
    Ph2_HwDescription::ReadoutChip* fCbc;
    Ph2_HwDescription::BeBoard*     fBoard;

    // root stuff
    TCanvas* fCanvas;
    TH1F*    fPulse;
    TCanvas* fCanvas2;
    TH1F*    ftpvsped;
    TCanvas* fCanvas3;
    TH2F*    fchanvsdel;
    TCanvas* fCanvas4;
    TH2F*    hSTUB_VthVSDel;
    TCanvas* fCanvas5;
    TH2F*    hSTUB_SCAN_tg;
    TCanvas* fCanvas6;
    TH2F*    hSTUB_SCAN_cw;
    TH2F*    hSTUB_SCAN_cbc;
    TH2F*    hSTUB_SCAN_sof;
    TH2F*    hSTUB_SCAN_SOF;
    TH2F*    hSTUB_SCAN_bend;
    TH2F*    hSTUB_SCAN_error;
    TH2F*    hSTUB_SCAN_cw_cbc[8];
    TH2F*    hSTUB_SCAN_sof_cbc[8];
    TH2F*    hSTUB_SCAN_bend_off[8];
    TH2F*    hSTUB_SCAN_bend_pt[8];
};

#endif
#endif
