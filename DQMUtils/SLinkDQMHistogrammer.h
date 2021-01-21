/*!
        \file                SLinkDQMHistogrammer.h
        \brief               base class to create and fill monitoring histograms
        \author              Suchandra Dutta and Subir Sarkar
        \version             1.0
        \date                6/11/17
        Support :            mail to : Suchandra.Dutta@cern.ch, Subir.Sarkar@cern.ch
*/

#ifndef __SLINKDQMHISTOGRAMMER_H__
#define __SLINKDQMHISTOGRAMMER_H__

#include "../Utils/easylogging++.h"
#include "../tools/Tool.h"

#include <map>
#include <string>
#include <vector>

class TH1I;
class TH1F;
class TH1D;
class TH2I;
class TProfile;
class TTree;
class DQMEvent;
class TString;
class StubInfo;
/*!
 * \class SLinkDQMHistogrammer
 * \brief Class to create and fill monitoring histograms
 */
#ifdef __OTSDAQ__
namespace ots
{
#endif

class SLinkDQMHistogrammer
{
  public:
    /*!
     * constructor
     */
    SLinkDQMHistogrammer(int evtType = 0, bool addTree = false, bool skipHist = false);

    /*!
     * destructor
     */
    virtual ~SLinkDQMHistogrammer();
    /*!
     * Book histograms
     */
    void bookHistograms(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& link_mapping);
    void bookNtuple(uint8_t link_id);
    /*!
     * Fill histograms
     */
    void fillHistograms(const std::vector<DQMEvent*>& event_list);
    void fillHistograms(const DQMEvent* event);
    void saveHistograms(const std::string& dqmFile, const std::string& flatTreeFile);
    void resetHistograms();
    void fillEventTrendHisto(TH1I* th, unsigned long ival, unsigned int val);

  private:
    int  eventType_;
    bool addTree_;
    bool skipDebugHist_;

    struct ROUHistos
    {
        TH1I*     l1ACounterH_;
        TH1I*     errBitH_;
        TH1I*     plAddH_;
        TH1I*     channelOccupancyH_;
        TProfile* hitVsVCTHH_;

        bool     bookedHistos;
        uint32_t vcthSetting_;
    };
    struct FEUHistos
    {
        TH1I* bottomSensorHitProfH_;
        TH1I* topSensorHitProfH_;

        TH2I* hitCorrH_;
        TH1D* deltaHitH_;
        TH1I* L1CounterDiffH_;
        TH1I* PLAddPhaseDiffH_;
        TH2I* PLAddPhaseCorrH_;
        TH1I* rouErrorH_;

        bool bookedHistos;
    };
    struct MODHistos
    {
        TH1I* totalNumberHitsH_;
        TH1I* totalNumberStubsH_;
        TH1I* hitCountH_;
        TH2I* bottomSensorHitProfH_;
        TH2I* topSensorHitProfH_;
        TH1I* bottomSensorHitProfUnfoldH_;
        TH1I* topSensorHitProfUnfoldH_;
        TH2I* hitCorrH_;

        std::map<uint8_t, FEUHistos>  feuHMap_;
        std::map<uint16_t, ROUHistos> rouHMap_;

        uint8_t totROUs_;
        uint8_t nROUsPerFEU_;

        TH1I* senCorrH_;
        TH1I* stubCountH_;
        TH1F* stubPositionH_;
        TH1I* stubBendH_;

        bool     bookedHistos;
        uint32_t hvSetting_;
    };
    struct ntupleElements
    {
        std::vector<uint16_t>* hitEven;
        std::vector<uint16_t>* hitOdd;
        std::vector<uint16_t>* error;
        std::vector<uint16_t>* plAdd;
        std::vector<uint16_t>* l1Counter;
    };
    std::map<uint8_t, MODHistos>       moduleHMap_;
    std::map<uint8_t, ntupleElements*> ntupleMap_;

    TH1I* l1AcceptH_;

    void bookMODHistograms(uint8_t mod_id, std::vector<uint8_t>& rou_vec);
    void bookFEUHistograms(uint8_t mod_id, uint8_t fe_id, FEUHistos& feu_h, uint8_t n_rou);
    void bookROUHistograms(uint8_t mod_id, uint8_t fe_id, uint8_t i_rou, ROUHistos& rou_h);

    void fillROUProperties(MODHistos& mod_h, std::vector<uint16_t>& errs, std::vector<uint16_t>& adds, std::vector<uint16_t>& counts);

    void fillMODHistograms(MODHistos& mod_h, uint32_t& tot_hits, uint32_t& tot_stubs, std::vector<uint16_t>& even_list, std::vector<uint16_t>& odd_list);

    void fillNtupleBranch(uint8_t                      id,
                          const std::vector<uint16_t>& even_list,
                          const std::vector<uint16_t>& odd_list,
                          const std::vector<uint16_t>& errs,
                          const std::vector<uint16_t>& adds,
                          const std::vector<uint16_t>& counts);
    void fillStubInformation(MODHistos& feu_h, const std::vector<StubInfo>& stubs);
    void readConditionData(const DQMEvent* evt);

  public:
    TTree* _ftree;
};
#ifdef __OTSDAQ__
}
#endif

#endif
