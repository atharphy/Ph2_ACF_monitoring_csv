/*!
        \file                DQMHistogrammer.h
        \brief               base class to create and fill monitoring histograms
        \author              Suchandra Dutta and Subir Sarkar
        \version             1.0
        \date                13/10/15
        Support :            mail to : Suchandra.Dutta@cern.ch, Subir.Sarkar@cern.ch

*/

#ifndef __DQMHISTOGRAMMER_H__
#define __DQMHISTOGRAMMER_H__

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <TStyle.h>
#pragma GCC diagnostic pop

#include "../Utils/easylogging++.h"
#include "../tools/Tool.h"

#include <map>
#include <string>
#include <vector>

class TH1I;
class TH1D;
class TH2I;
class TProfile;
class TTree;
class Tool;
class TString;
/*!
 * \class DQMHistogrammer
 * \brief Class to create and fill monitoring histograms
 */
class DQMHistogrammer : public Tool
{
  public:
    /*!
     * constructor
     */
    DQMHistogrammer(bool addTree = false, int ncol = 2, bool eventFilter = true, bool addDebugHisto = false);

    /*!
     * destructor
     */
    virtual ~DQMHistogrammer();
    /*!
     * Book histograms
     */
    void bookHistos(const Ph2_HwInterface::EventDataMap& evmap);
    void bookEventTrendHisto(TH1I*& th, const TString& name, const TString& title, int size);

    /*!
     * Fill histogram
     */
    void fillHistos(const std::vector<Ph2_HwInterface::Event*>& event_list, int nevtp, const int data_size);
    void saveHistos(const std::string& out_file);
    void resetHistos();
    void fillSensorHistos(int ncbc, const std::vector<int>& even_values, const std::vector<int>& odd_values);
    void fillCBCHistos(unsigned long ievt, std::string cbc_hid, uint32_t error, uint32_t address, int nstub, const std::vector<uint32_t>& channles);
    void fillEventTrendHisto(TH1I* th, unsigned long ival, unsigned int val);
    bool getEventFlag(const unsigned long& ievt, const int data_size);

  private:
    bool addTree_;
    int  nColumn_;
    bool filterEvent_;
    bool skipDebugHist_;

    uint32_t dataBuffer_; // (32 bit words line)
    long     pCounter_;   // (get rid of first 47 events)
    uint32_t periodicity_;
    uint32_t periodicityOffset_;
    uint32_t eventBlock_;
    uint32_t skipEvents_;
    long     lineOffset_;

    TTree* tree_;
    // Following same convention as HitProfile histo naming
    uint32_t l1Accept_;
    uint32_t tdcCounter_;
    uint32_t totalHits_;
    uint32_t totalStubs_;
    bool     eventFlag_;
    uint32_t eventCountCBC_;

    std::vector<unsigned int>* cbcErrorVal_;
    std::vector<unsigned int>* cbcPLAddressVal_;
    std::vector<unsigned int>* dut0C0chData_;
    std::vector<unsigned int>* dut0C1chData_;
    std::vector<unsigned int>* dut1C0chData_;
    std::vector<unsigned int>* dut1C1chData_;
    struct CBCHistos
    {
        TH1I*     errBitH;
        TH1I*     errBitVsEvtH;
        TH1I*     plAddH;
        TH1I*     plAddVsEvtH;
        TH1I*     nStubsH;
        TH1I*     evenChnOccuH;
        TH1I*     oddChnOccuH;
        TProfile* tdcVsEvenChnOccuH;
        TProfile* tdcVsOddChnOccuH;
    };
    std::map<std::string, CBCHistos> cbcHMap_;

    TH2I* hitCorrC0H_;
    TH2I* hitCorrC1H_;
    TH1D* hitDelCorrC0H;
    TH1D* hitDelCorrC1H;

    TH2I* dut0HitProfH_;
    TH2I* dut1HitProfH_;

    TH1I* dut0HitProfUnfoldedH_;
    TH1I* dut1HitProfUnfoldedH_;

    TH1I* dut0C0HitProfH_;
    TH1I* dut0C1HitProfH_;
    TH1I* dut1C0HitProfH_;
    TH1I* dut1C1HitProfH_;

    TH1I* sensCorrH_;
    TH1I* l1AcceptH_;
    TH1I* tdcCounterH_;
    TH1I* totalNumberHitsH_;
    TH1I* totalNumberStubsH_;
    TH1I* periodicityFlagVsEvtH_;

    TH1I* plAddPhaseDiffH_;
    TH2I* plAddPhaseCorrH_;

    TH2I* cbcErrorCorrH_;
    TH1I* plAddPhaseDiffVsEvtH_;
    TH1I* bunchCounterVsEvtH_;
    TH1I* orbitCounterVsEvtH_;
    TH1I* lumiCounterVsEvtH_;
    TH1I* l1AcceptVsEvtH_;
    TH1I* cbcCounterVsEvtH_;
    TH1I* tdcCounterVsEvtH_;
};
#endif
