/*!
        \file                SLinkDQMHistogrammer.h
        \brief               base class to create and fill monitoring histograms
        \author              Suchandra Dutta and Subir Sarkar
        \version             1.0
        \date                13/10/15
        Support :            mail to : Suchandra.Dutta@cern.ch, Subir.Sarkar@cern.ch
*/

#include "SLinkDQMHistogrammer.h"
#include "DQMEvent.h"

#include "TDirectory.h"
#include "TFile.h"
#include "TH1.h"
#include "TH1D.h"
#include "TH2I.h"
#include "TProfile.h"
#include "TROOT.h"
#include "TString.h"
#include "TTree.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include "TPaveStats.h"
#include <TStyle.h>
#pragma GCC diagnostic pop
#include "TIterator.h"

#ifdef __OTSDAQ__
using namespace ots;
#endif

SLinkDQMHistogrammer::SLinkDQMHistogrammer(int evtType, bool addTree, bool skipHist) : eventType_(evtType), addTree_(addTree), skipDebugHist_(skipHist) {}

SLinkDQMHistogrammer::~SLinkDQMHistogrammer()
{
    TIter    next(gROOT->GetList());
    TObject* obj;

    while((obj = next()))
    {
        if(obj->InheritsFrom("TH1") || obj->InheritsFrom("TTree")) delete obj;
    }
}

void SLinkDQMHistogrammer::bookHistograms(const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& link_mapping)
{
    std::cout << "nModules: " << link_mapping.size() << std::endl;

    // Open Tree
    if(addTree_) _ftree = new TTree("dqm", "Flat nTuple");

    for(auto const& it: link_mapping)
    {
        uint8_t              linkId = it.first;
        std::vector<uint8_t> rous   = it.second;

        LOG(INFO) << " Booking Histograms for Link Id: " << +linkId << " with " << rous.size() << " ROUs " << std::endl;
        bookMODHistograms(linkId, rous);
        if(addTree_) bookNtuple(linkId);
    }
}
void SLinkDQMHistogrammer::fillHistograms(const std::vector<DQMEvent*>& event_list)
{
    unsigned long ival = 0; // as the files is read in chunks
    for(const auto& ev: event_list)
    {
        ival++;
        // std::cout << " Event # " << ival << std::endl;

        try
        {
            fillHistograms(ev);
        }
        catch(...)
        {
            std::cout << " Failed to read back for histogram filling, Event # " << ival << std::endl;
        }
    }
}
void SLinkDQMHistogrammer::fillHistograms(const DQMEvent* event)
{
    std::vector<uint16_t> rouErrs;
    std::vector<uint16_t> rouPLAdds;
    std::vector<uint16_t> rouL1Counters;
    std::vector<uint16_t> evenChannels;
    std::vector<uint16_t> oddChannels;

    readConditionData(event);

    uint16_t modStripIndex;

    uint32_t totHits;
    uint32_t totStubs;
    for(auto& imod: moduleHMap_)
    {
        uint8_t   modId   = imod.first;
        MODHistos mod_h   = imod.second;
        uint8_t   nRouMod = mod_h.totROUs_;
        if(nRouMod == 0) continue;
        if(!mod_h.bookedHistos) continue;

        uint8_t nrou = 0;
        // std::cout << " Entering Module # " << +modId << std::endl;
        for(auto& irou: mod_h.rouHMap_)
        {
            uint16_t cKey  = irou.first;
            uint8_t  feuId = (cKey >> 8) & 0xFF;
            uint8_t  rouId = cKey & 0xFF;
            TString  hyb;
            if(feuId == 0)
                hyb = "rightHyb";
            else
                hyb = "leftHyb";
            ROUHistos rou_h = irou.second;

            uint16_t dataKey = modId << 8 | rouId;
            //	   uint16_t cKey_swaped = feId << 8 | rouId_swaped;

            nrou++;

            if(!rou_h.bookedHistos) continue;

            // get Readout Data
            const std::pair<ReadoutStatus, std::vector<bool>>& readoutData = event->trkPayload().readoutData(dataKey);

            // Fill Readout Status
            const ReadoutStatus& rs        = readoutData.first;
            uint8_t              error_rou = 0;
            if(rs.error2()) error_rou = 1;
            if(rs.error1()) error_rou |= 1 << 1;

            uint16_t pladdress_rou = rs.PA();
            rouErrs.push_back(error_rou);
            rouPLAdds.push_back(pladdress_rou);
            rouL1Counters.push_back(rs.l1ACounter());

            // Fill Readout channel data
            const std::vector<bool>& channelData = readoutData.second;
            uint32_t                 nhits_rou   = 0;

            for(size_t i = 0; i < channelData.size(); ++i) // CBC channels
            {
                if(!channelData[i]) continue;

                rou_h.channelOccupancyH_->Fill(i);
                modStripIndex = rouId * 127 + std::floor(i / 2);
                //	       else modStripIndex = (rouId-nRouPerFeu)*127 + 127 -1 - std::floor(i/2);
                //	       else modStripIndex = (nRouMod-1-rouId)*127  + std::floor((254-i)/2);

                nhits_rou++;

                if((i % 2) == 0)
                    evenChannels.push_back(modStripIndex);
                else
                    oddChannels.push_back(modStripIndex);
                //	       std::cout << "Link Id " << +modId << " Hybrid " << hyb.Data() << " ROU Id " << +rouId << "
                // RouNew " << +(nRouMod-1-rouId)<< " i " << i
                //			 << " Key " << cKey
                //			 << " modStripIndex " << modStripIndex
                //                        << " oddChannel size " << oddChannels.size()
                //			 << " evenChanles size " << evenChannels.size()
                //                        << std::endl;
            }

            rou_h.plAddH_->Fill(pladdress_rou);
            rou_h.errBitH_->Fill(error_rou);
            rou_h.hitVsVCTHH_->Fill(rou_h.vcthSetting_, nhits_rou);
            totHits += nhits_rou;

            if(nrou == nRouMod)
            {
                // stub data
                try
                {
                    const std::vector<StubInfo>& stubInfos = event->stubData().stubs(modId);
                    fillStubInformation(mod_h, stubInfos); //, varList);
                    totStubs += stubInfos.size();
                }
                catch(...)
                {
                    std::cout << "Caught exception while acessing Stub Data" << std::endl;
                }
                fillMODHistograms(mod_h, totHits, totStubs, evenChannels, oddChannels);
                fillROUProperties(mod_h, rouErrs, rouPLAdds, rouL1Counters);
                fillNtupleBranch(modId, evenChannels, oddChannels, rouErrs, rouPLAdds, rouL1Counters);

                evenChannels.clear();
                oddChannels.clear();
                rouErrs.clear();
                rouPLAdds.clear();
                rouL1Counters.clear();
                totHits  = 0;
                totStubs = 0;
            }
        }
    }
    if(addTree_ && _ftree) _ftree->Fill();
}
void SLinkDQMHistogrammer::fillROUProperties(MODHistos& mod_h, std::vector<uint16_t>& errs, std::vector<uint16_t>& adds, std::vector<uint16_t>& counts)
{
    uint8_t totRous     = mod_h.totROUs_;
    uint8_t nRousPerFeu = mod_h.nROUsPerFEU_;
    if(errs.size() != totRous || adds.size() != totRous || counts.size() != totRous) return;

    for(size_t ir = 0; ir < totRous; ir++)
    {
        uint8_t feu_id = ir / nRousPerFeu;
        auto    fPos   = mod_h.feuHMap_.find(feu_id);
        if(fPos != mod_h.feuHMap_.end())
        {
            FEUHistos feu_h = fPos->second;

            feu_h.rouErrorH_->Fill(ir * 4 + errs[ir]);

            for(size_t ir2 = 0; ir2 < totRous; ir2++)
            {
                if(ir == ir2) continue;
                uint32_t phase = (adds[ir] - adds[ir2] + 511) % 511;
                feu_h.PLAddPhaseDiffH_->Fill(phase);
                feu_h.PLAddPhaseCorrH_->Fill(adds[ir], adds[ir2]);
                feu_h.L1CounterDiffH_->Fill(counts[ir] - counts[ir2]);
            }
        }
    }
}
void SLinkDQMHistogrammer::fillMODHistograms(MODHistos& mod_h, uint32_t& tot_hits, uint32_t& tot_stubs, std::vector<uint16_t>& even_list, std::vector<uint16_t>& odd_list)
{
    if(!mod_h.bookedHistos) return;

    uint8_t  nRousPerFeu = mod_h.nROUsPerFEU_;
    uint16_t strEven;
    uint16_t strOdd;
    uint16_t strEvenSwapped;
    uint16_t strOddSwapped;
    uint8_t  fe_id = 0;

    if(!even_list.size() && !odd_list.size())
        mod_h.hitCountH_->Fill(1);
    else if(!even_list.size() && !odd_list.size())
        mod_h.hitCountH_->Fill(2);
    else if(even_list.size() && !odd_list.size())
        mod_h.hitCountH_->Fill(3);
    else if(even_list.size() && odd_list.size())
        mod_h.hitCountH_->Fill(4);

    for(uint16_t i = 0; i < even_list.size(); i++)
    {
        strOdd = 9999;
        if(even_list[i] < nRousPerFeu * 127)
            fe_id = 0;
        else
            fe_id = 1;
        strEven = even_list[i] - fe_id * nRousPerFeu * 127;
        if(odd_list.size() == even_list.size()) strOdd = odd_list[i] - fe_id * nRousPerFeu * 127;

        if(fe_id == 0)
            strEvenSwapped = strEven;
        else
            strEvenSwapped = fe_id * nRousPerFeu * 127 - strEven;

        //      if (fe_id == 1) std::cout << " i " << i << " original strip Number " << even_list[i] << " strip number "
        //      << strEven << " FeU Id" <<  +fe_id << " strip Number swapped " << strEvenSwapped << std::endl;
        auto fPos = mod_h.feuHMap_.find(fe_id);
        if(fPos != mod_h.feuHMap_.end())
        {
            FEUHistos feu_h = fPos->second;
            feu_h.bottomSensorHitProfH_->Fill(strEven);
            if(strOdd != 9999)
            {
                feu_h.hitCorrH_->Fill(strEven, strOdd);
                feu_h.deltaHitH_->Fill(strEven - strOdd);
            }
        }
        mod_h.bottomSensorHitProfH_->Fill((2 - fe_id), strEvenSwapped);
        mod_h.bottomSensorHitProfUnfoldH_->Fill(even_list[i]);
        if(strOdd != 9999) mod_h.hitCorrH_->Fill(odd_list[i], even_list[i]);
    }
    for(uint16_t i = 0; i < odd_list.size(); i++)
    {
        if(odd_list[i] < nRousPerFeu * 127)
            fe_id = 0;
        else
            fe_id = 1;
        strOdd = odd_list[i] - fe_id * nRousPerFeu * 127;

        if(fe_id == 0)
            strOddSwapped = strOdd;
        else
            strOddSwapped = fe_id * nRousPerFeu * 127 - strOdd;
        auto fPos = mod_h.feuHMap_.find(fe_id);
        if(fPos != mod_h.feuHMap_.end())
        {
            FEUHistos feu_h = fPos->second;
            feu_h.topSensorHitProfH_->Fill(strOdd);
        }
        mod_h.topSensorHitProfH_->Fill((2 - fe_id), strOddSwapped);
        mod_h.topSensorHitProfUnfoldH_->Fill(odd_list[i]);
    }

    mod_h.totalNumberHitsH_->Fill(tot_hits);
    mod_h.totalNumberStubsH_->Fill(tot_stubs);
}
void SLinkDQMHistogrammer::fillNtupleBranch(uint8_t                      id,
                                            const std::vector<uint16_t>& even_list,
                                            const std::vector<uint16_t>& odd_list,
                                            const std::vector<uint16_t>& errs,
                                            const std::vector<uint16_t>& adds,
                                            const std::vector<uint16_t>& counts)
{
    auto fPos = ntupleMap_.find(id);
    if(fPos != ntupleMap_.end())
    {
        fPos->second->hitEven->clear();
        std::copy(even_list.begin(), even_list.end(), back_inserter(*fPos->second->hitEven));

        fPos->second->hitOdd->clear();
        std::copy(odd_list.begin(), odd_list.end(), back_inserter(*fPos->second->hitOdd));

        fPos->second->error->clear();
        std::copy(errs.begin(), errs.end(), back_inserter(*fPos->second->error));

        fPos->second->plAdd->clear();
        std::copy(adds.begin(), adds.end(), back_inserter(*fPos->second->plAdd));

        fPos->second->l1Counter->clear();
        std::copy(counts.begin(), counts.end(), back_inserter(*fPos->second->l1Counter));
    }
}
void SLinkDQMHistogrammer::saveHistograms(const std::string& dqmFile, const std::string& flatTreeFile)
{
    TFile* fout = TFile::Open(dqmFile.c_str(), "RECREATE");
    fout->cd();

    // sensor plots
    for(auto& imod: moduleHMap_)
    {
        uint8_t modId = imod.first;
        TString name  = "Module_";
        name += modId;

        fout->mkdir(name);
        fout->cd(name);
        MODHistos mod_h = imod.second;
        if(mod_h.bookedHistos)
        {
            mod_h.totalNumberHitsH_->Write();
            mod_h.totalNumberStubsH_->Write();
            mod_h.hitCountH_->Write();
            mod_h.bottomSensorHitProfH_->Write();
            mod_h.topSensorHitProfH_->Write();
            mod_h.bottomSensorHitProfUnfoldH_->Write();
            mod_h.topSensorHitProfUnfoldH_->Write();
            mod_h.hitCorrH_->Write();

            mod_h.stubCountH_->Write();
            mod_h.stubPositionH_->Write();
            mod_h.stubBendH_->Write();
        }
        fout->cd();
        for(auto& ifeu: mod_h.feuHMap_)
        {
            uint8_t feId = ifeu.first;

            TString name = "Module_";
            name += modId;
            name += "/";
            if(feId == 0)
                name += "rightHyb";
            else
                name += "leftHyb";

            std::cout << " Saving Histograms for  FeId  " << +feId << " in => " << name.Data() << std::endl;

            fout->mkdir(name);
            fout->cd(name);
            FEUHistos feu_h = ifeu.second;
            if(!feu_h.bookedHistos) continue;

            feu_h.bottomSensorHitProfH_->Write();
            feu_h.topSensorHitProfH_->Write();
            feu_h.hitCorrH_->Write();
            feu_h.deltaHitH_->Write();

            feu_h.L1CounterDiffH_->Write();
            feu_h.PLAddPhaseDiffH_->Write();
            feu_h.PLAddPhaseCorrH_->Write();
            feu_h.rouErrorH_->Write();

            fout->cd();
        }
        for(auto& irou: mod_h.rouHMap_)
        {
            ROUHistos rou_h = irou.second;
            if(!rou_h.bookedHistos) continue;

            uint16_t key   = irou.first;
            uint8_t  feId  = (key >> 8) & 0xFF;
            uint8_t  rouId = key & 0xFF;

            TString name = "Module_";
            name += modId;
            name += "/";
            if(feId == 0)
                name += "rightHyb";
            else
                name += "leftHyb";
            name += "/";
            name += "RouId_";
            name += rouId;
            std::cout << " Saving Histograms for  RouId " << +rouId << " in => " << name.Data() << std::endl;

            fout->mkdir(name);
            fout->cd(name);
            rou_h.errBitH_->Write();
            rou_h.plAddH_->Write();
            rou_h.channelOccupancyH_->Write();
            rou_h.hitVsVCTHH_->Write();
            fout->cd();
        }
    }
    fout->cd();
    fout->Close();

    if(addTree_ && _ftree)
    {
        TFile* trfile = TFile::Open(flatTreeFile.c_str(), "RECREATE");
        _ftree->Print();
        _ftree->Write();
        trfile->Write();
        trfile->Close();
    }
}
void SLinkDQMHistogrammer::resetHistograms()
{
    TIter    next(gROOT->GetList());
    TObject* obj;
    while((obj = next()))
        if(obj->InheritsFrom("TH1")) dynamic_cast<TH1*>(obj)->Reset();
}
void SLinkDQMHistogrammer::bookMODHistograms(uint8_t mod_id, std::vector<uint8_t>& rou_vec)
{
    if(moduleHMap_.find(mod_id) != moduleHMap_.end()) return;

    uint8_t nRouTot    = rou_vec.size();
    uint8_t nRouPerFeu = nRouTot / 2;
    if(nRouTot == 2) nRouPerFeu = 2;

    TString ss_mod = "Module_";
    ss_mod += mod_id;

    MODHistos mod_h;

    uint16_t nbin = nRouPerFeu * 128;

    mod_h.totalNumberHitsH_  = new TH1I("Tot_Hits_" + ss_mod, "TotalNumberOfHits_" + ss_mod, 101, -0.5, 100.5);
    mod_h.totalNumberStubsH_ = new TH1I("Tot_Stubs_" + ss_mod, "TotalNumberOfStubs_" + ss_mod, 101, -0.5, 100.5);

    mod_h.hitCountH_ = new TH1I("SensorHitCount_" + ss_mod, "SensorHitCount_" + ss_mod, 4, 0.5, 4.5);
    mod_h.hitCountH_->GetXaxis()->SetBinLabel(1, "No hits");
    mod_h.hitCountH_->GetXaxis()->SetBinLabel(2, "Even & !Odd");
    mod_h.hitCountH_->GetXaxis()->SetBinLabel(3, "Odd & !Even");
    mod_h.hitCountH_->GetXaxis()->SetBinLabel(4, "Even & Odd");

    mod_h.bottomSensorHitProfH_ = new TH2I("BottomSensorHitProfile_" + ss_mod, "BottomSensorHitProfile_" + ss_mod, 2, 0.5, 2.5, nbin, -0.5, nbin - 0.5);
    mod_h.bottomSensorHitProfH_->GetXaxis()->SetBinLabel(1, "Left Hybrid");
    mod_h.bottomSensorHitProfH_->GetXaxis()->SetBinLabel(2, "Right Hybrid");
    mod_h.bottomSensorHitProfH_->SetStats(false);
    mod_h.topSensorHitProfH_ = new TH2I("TopSensorHitProfile_" + ss_mod, "TopSensorHitProfile_" + ss_mod, 2, 0.5, 2.5, nbin, -0.5, nbin - 0.5);
    mod_h.topSensorHitProfH_->GetXaxis()->SetBinLabel(1, "Left Hybrid");
    mod_h.topSensorHitProfH_->GetXaxis()->SetBinLabel(2, "Right Hybrid");
    mod_h.topSensorHitProfH_->SetStats(false);

    mod_h.bottomSensorHitProfUnfoldH_ = new TH1I("BottomSensorHitProfileUnfolded_" + ss_mod, "BottomSensorHitProfileUnfolded_" + ss_mod, nbin * 2, -0.5, nbin * 2 - 0.5);
    mod_h.bottomSensorHitProfUnfoldH_->SetStats(false);
    mod_h.topSensorHitProfUnfoldH_ = new TH1I("TopSensorHitProfileUnfolded_" + ss_mod, "TopSensorHitProfileUnfolded_" + ss_mod, nbin * 2, -0.5, nbin * 2 - 0.5);
    mod_h.topSensorHitProfUnfoldH_->SetStats(false);

    mod_h.hitCorrH_ = new TH2I("SensorHitCorr_" + ss_mod, "SensorHitCorrelation_" + ss_mod, nbin * 2, -0.5, nbin * 2 - 0.5, nbin * 2, -0.5, nbin * 2 - 0.5);
    mod_h.hitCorrH_->SetStats(false);
    mod_h.hitCorrH_->GetXaxis()->SetTitle("Top Sensor Hit");
    mod_h.hitCorrH_->GetYaxis()->SetTitle("Bottom Sensor Hit");

    // Stub Information
    mod_h.stubCountH_ = new TH1I("StubCount_" + ss_mod, "StubCount_" + ss_mod, 4 * nRouTot, 0.0, nRouTot * 4);
    mod_h.stubCountH_->GetXaxis()->SetNdivisions(4 * 100);

    for(size_t i = 0; i < nRouTot; i++)
    {
        TString tag = "chip";
        tag += i;
        mod_h.stubCountH_->GetXaxis()->SetBinLabel((i * 4 + 1), tag + "(0)");
        mod_h.stubCountH_->GetXaxis()->SetBinLabel((i * 4 + 2), tag + "(1)");
        mod_h.stubCountH_->GetXaxis()->SetBinLabel((i * 4 + 3), tag + "(2)");
        mod_h.stubCountH_->GetXaxis()->SetBinLabel((i * 4 + 4), tag + "(3)");

        mod_h.bottomSensorHitProfUnfoldH_->GetXaxis()->SetBinLabel(i * 127 + 60, tag);
        mod_h.topSensorHitProfUnfoldH_->GetXaxis()->SetBinLabel(i * 127 + 60, tag);
    }

    mod_h.stubPositionH_ = new TH1F("StubPosition" + ss_mod, "StubPosition" + ss_mod, nbin, -0.5, nbin + 0.5);
    mod_h.stubBendH_     = new TH1I("StubBend_" + ss_mod, "StubBend_" + ss_mod, 31, -15.5, 15.5);

    for(size_t irou = 0; irou < nRouTot; irou++)
    {
        uint8_t feu_id = irou / nRouPerFeu;
        if(irou % nRouPerFeu == 0)
        {
            FEUHistos feuH;
            LOG(INFO) << " Booking Histograms for FEU " << +feu_id << " in Link Id " << +mod_id << " With " << +nRouPerFeu << " ROUs" << std::endl;
            bookFEUHistograms(mod_id, feu_id, feuH, nRouPerFeu);
            mod_h.feuHMap_.insert({feu_id, feuH});
        }
        uint8_t rou_id = rou_vec[irou];
        LOG(INFO) << " Booking Histograms for ROU " << +rou_id << " in FEU Id " << +feu_id << " in Link Id " << +mod_id << std::endl;
        ROUHistos rouH;
        bookROUHistograms(mod_id, feu_id, rou_id, rouH);
        uint16_t key = feu_id << 8 | rou_id;
        mod_h.rouHMap_.insert({key, rouH});
    }
    mod_h.totROUs_     = nRouTot;
    mod_h.nROUsPerFEU_ = nRouPerFeu;
    mod_h.bookedHistos = true;
    moduleHMap_.insert({mod_id, mod_h});
}
void SLinkDQMHistogrammer::bookFEUHistograms(uint8_t mod_id, uint8_t feu_id, FEUHistos& feu_h, uint8_t n_rou)
{
    TString ss_feu;
    ss_feu = "Mod_";
    ss_feu += mod_id;
    if(feu_id == 0)
        ss_feu += "_rightHyb";
    else
        ss_feu += "_leftHyb";

    uint16_t nbin = n_rou * 127;

    feu_h.bottomSensorHitProfH_ = new TH1I("BottomSensor_HitProf_" + ss_feu, "BottomSensor_HitProf_" + ss_feu, nbin, -0.5, nbin - 0.5);
    feu_h.topSensorHitProfH_    = new TH1I("TopSensor_HitProf_" + ss_feu, "TopSensor_HitProf_" + ss_feu, nbin, -0.5, nbin - 0.5);
    feu_h.hitCorrH_             = new TH2I("HitCorr_" + ss_feu, "HitCorr_" + ss_feu, nbin, -0.5, nbin - 0.5, nbin, -0.5, nbin - 0.5);
    feu_h.hitCorrH_->SetStats(false);
    feu_h.deltaHitH_ = new TH1D("HitDifference_" + ss_feu, "HitDifference_" + ss_feu, 2 * nbin, -nbin, nbin);

    feu_h.L1CounterDiffH_  = new TH1I("L1CounterDiff_" + ss_feu, "L1CounterDiff_" + ss_feu, 1023, -511.5, 511.5);
    feu_h.PLAddPhaseDiffH_ = new TH1I("PipeLineAddPhasedDiff_" + ss_feu, "PipeLineAddPhasedDiff_" + ss_feu, 1023, -511.5, 511.5);
    feu_h.PLAddPhaseCorrH_ = new TH2I("PipeLineAddPhaseCorr_" + ss_feu, "PipeLineAddPhaseCorr_" + ss_feu, 511, -0.5, 510.5, 511, -0.5, 510.5);
    feu_h.PLAddPhaseCorrH_->SetStats(false);
    feu_h.rouErrorH_ = new TH1I("ROUError_" + ss_feu, "ROUError_" + ss_feu, n_rou * 4, 0.0, n_rou * 4);
    feu_h.rouErrorH_->GetXaxis()->SetNdivisions(4 * 100);

    for(size_t i = 0; i < n_rou; i++)
    {
        TString tag = "Chip";
        tag += i;
        tag += "(";

        feu_h.rouErrorH_->GetXaxis()->SetBinLabel((i * 4 + 1), tag + "0)");
        feu_h.rouErrorH_->GetXaxis()->SetBinLabel((i * 4 + 2), tag + "1)");
        feu_h.rouErrorH_->GetXaxis()->SetBinLabel((i * 4 + 3), tag + "2)");
        feu_h.rouErrorH_->GetXaxis()->SetBinLabel((i * 4 + 4), tag + "3)");
    }

    feu_h.bookedHistos = true;
}
void SLinkDQMHistogrammer::bookROUHistograms(uint8_t mod_id, uint8_t feu_id, uint8_t i_rou, ROUHistos& rou_h)
{
    TString ss_rou = "Mod_";
    ss_rou += mod_id;
    if(feu_id == 0)
        ss_rou += "_rightHyb_";
    else
        ss_rou += "_leftHyb_";
    ss_rou += "_rou_";
    ss_rou += i_rou;

    rou_h.errBitH_           = new TH1I("ErrorBit_" + ss_rou, "ErrotBit_" + ss_rou, 6, -0.5, 5.5);
    rou_h.plAddH_            = new TH1I("PipeLineAdd_" + ss_rou, "PipeLineAdd_" + ss_rou, 256, -0.5, 255.5);
    rou_h.channelOccupancyH_ = new TH1I("ChannelOccupancy_" + ss_rou, "Channel Occupancy_" + ss_rou, 254, -0.5, 253.5);
    rou_h.hitVsVCTHH_        = new TProfile("HitCountVsVCTH_" + ss_rou, "HitCountVsVCTH_" + ss_rou, 500, 0.0, 500.0, 0.0, 300.);
    rou_h.vcthSetting_       = 0;
    rou_h.bookedHistos       = true;
}
void SLinkDQMHistogrammer::bookNtuple(uint8_t mod_id)
{
    ntupleMap_.insert({mod_id, new ntupleElements()});

    TString ss_mod = "Mod_";
    ss_mod += mod_id;

    ntupleMap_[mod_id]->hitEven = new std::vector<uint16_t>();
    _ftree->Branch("hitEven_" + ss_mod, "std::vector<uint16_t>", &ntupleMap_[mod_id]->hitEven);

    ntupleMap_[mod_id]->hitOdd = new std::vector<uint16_t>();
    _ftree->Branch("hitOdd_" + ss_mod, "std::vector<uint16_t>", &ntupleMap_[mod_id]->hitOdd);

    ntupleMap_[mod_id]->error = new std::vector<uint16_t>();
    _ftree->Branch("CBCError_" + ss_mod, "std::vector<uint16_t>", &ntupleMap_[mod_id]->error);

    ntupleMap_[mod_id]->plAdd = new std::vector<uint16_t>();
    _ftree->Branch("CBCPLAddress_" + ss_mod, "std::vector<uint16_t>", &ntupleMap_[mod_id]->plAdd);

    ntupleMap_[mod_id]->l1Counter = new std::vector<uint16_t>();
    _ftree->Branch("CBCL1Counter_" + ss_mod, "std::vector<uint16_t>", &ntupleMap_[mod_id]->l1Counter);
}
void SLinkDQMHistogrammer::fillStubInformation(MODHistos& mod_h, const std::vector<StubInfo>& stubs)
{
    if(!mod_h.bookedHistos) return;

    uint8_t               nrou = mod_h.totROUs_;
    std::vector<uint16_t> scount(nrou, 0);

    for(const auto& istub: stubs)
    {
        if(istub.chipId() < scount.size()) scount[istub.chipId()]++;

        mod_h.stubPositionH_->Fill(istub.chipId() * 127 + istub.address() / 2.);
        mod_h.stubBendH_->Fill(istub.bend());
    }

    for(size_t i = 0; i < nrou; i++) mod_h.stubCountH_->Fill(i * 4 + scount[i]);
}
void SLinkDQMHistogrammer::readConditionData(const DQMEvent* evt)
{
    for(size_t i = 0; i < evt->condData().size(); i++)
    {
        const std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, uint8_t, size_t>& data    = evt->condData().data(i);
        uint8_t                                                                i2cPage = std::get<2>(data);
        uint8_t                                                                i2cReg  = std::get<3>(data);
        uint8_t                                                                uid     = std::get<4>(data);

        // Get HV status
        if(uid == 5)
        {
            uint8_t                                mod_id = std::get<0>(data);
            std::map<uint8_t, MODHistos>::iterator fPosM  = moduleHMap_.find(mod_id);
            if(fPosM != moduleHMap_.end()) fPosM->second.hvSetting_ = std::get<5>(data);
        }

        // Get VCTH status
        if(uid == 1 && i2cPage == 0 && i2cReg == 0X4F)
        {
            uint8_t mod_id = std::get<0>(data);
            uint8_t rou_id = std::get<1>(data);

            std::map<uint8_t, MODHistos>::iterator fPosM = moduleHMap_.find(mod_id);
            if(fPosM != moduleHMap_.end())
            {
                MODHistos mod_h      = fPosM->second;
                uint8_t   nRouPerFeu = mod_h.nROUsPerFEU_;
                uint8_t   fe_id      = rou_id / nRouPerFeu;
                uint16_t  key        = fe_id << 8 | rou_id;

                std::map<uint16_t, ROUHistos>::iterator fPosR = mod_h.rouHMap_.find(key);
                if(fPosR != mod_h.rouHMap_.end()) fPosR->second.vcthSetting_ = std::get<5>(data);
            }
        }
    }
}
