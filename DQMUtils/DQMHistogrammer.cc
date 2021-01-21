/*!
        \file                DQMHistogrammer.h
        \brief               base class to create and fill monitoring histograms
        \author              Suchandra Dutta and Subir Sarkar
        \version             1.0
        \date                13/10/15
        Support :            mail to : Suchandra.Dutta@cern.ch, Subir.Sarkar@cern.ch
*/

#include "../DQMUtils/DQMHistogrammer.h"

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
#include "../Utils/Event.h"
#include "TIterator.h"

using namespace Ph2_HwInterface;

DQMHistogrammer::DQMHistogrammer(bool addTree, int ncol, bool eventFilter, bool skipHist) : Tool(), addTree_(addTree), nColumn_(ncol), filterEvent_(eventFilter), skipDebugHist_(skipHist)
{
    dataBuffer_        = 42;   // (32 bit words line)
    pCounter_          = 1976; // (get rid of first 47 events)
    periodicity_       = 344;
    periodicityOffset_ = 600;
    eventBlock_        = 10000;
    skipEvents_        = 47;
    lineOffset_        = 256;
}

DQMHistogrammer::~DQMHistogrammer()
{
    TIter    next(gROOT->GetList());
    TObject* obj;

    while((obj = next()))
    {
        if(obj->InheritsFrom("TH1") || obj->InheritsFrom("TTree")) delete obj;
    }
}
void DQMHistogrammer::bookHistos(const Ph2_HwInterface::EventDataMap& evmap)
{
    sensCorrH_ = new TH1I("sensorHitcorr", "Sensor Hit Correlation", 4, 0.5, 4.5);
    sensCorrH_->GetXaxis()->SetBinLabel(1, "No hits");
    sensCorrH_->GetXaxis()->SetBinLabel(2, "Even & !Odd");
    sensCorrH_->GetXaxis()->SetBinLabel(3, "Odd & !Even");
    sensCorrH_->GetXaxis()->SetBinLabel(4, "Even & Odd");

    l1AcceptH_   = new TH1I("l1A", "L1A Counter", 10000, 0, 10000);
    tdcCounterH_ = new TH1I("tdc", "TDC counter", 17, -0.5, 16.5);

    uint16_t nCbc    = 0;
    int      evtsize = 500000;

    for(auto const& it: evmap)
    {
        uint16_t cKey  = it.first;
        uint8_t  feId  = (cKey >> 8) & 0xFF;
        uint8_t  cbcId = cKey & 0xFF;

        LOG(INFO) << " fedId " << +feId << " cbcId " << +cbcId << " Columns " << nColumn_ << " nCbc " << nCbc;

        if(nColumn_ == 1 && cbcId > 1) continue;

        nCbc++;
        // create histogram name tags
        std::stringstream ss;
        ss << "fed" << +feId << "cbc" << +cbcId;
        std::string key = ss.str();
        CBCHistos   cbc_h;
        cbc_h.errBitH           = new TH1I(TString("errbit-" + key), TString("Error bit (" + key + ")"), 6, -0.5, 5.5);
        cbc_h.plAddH            = new TH1I(TString("PLAddress-" + key), TString("Pipeline Address (" + key + ")"), 256, -0.5, 255.5);
        cbc_h.nStubsH           = new TH1I(TString("nstubs-" + key), TString("No. Of Stubs (" + key + ")"), 21, -0.5, 20.5);
        cbc_h.evenChnOccuH      = new TH1I(TString("chOccupancy_even-" + key), TString("Even Channel Occupancy (" + key + ")"), 127, 0.5, 127.5);
        cbc_h.oddChnOccuH       = new TH1I(TString("chOccupancy_odd-" + key), TString("Odd Channel Occupancy (" + key + ")"), 127, 0.5, 127.5);
        cbc_h.tdcVsEvenChnOccuH = new TProfile(TString("tdcVsOccupancy_even-" + key), TString("TDC vs Even Channel Occupancy (" + key + ")"), 256, -0.5, 255.5, 0, 127.);
        cbc_h.tdcVsOddChnOccuH  = new TProfile(TString("tdcVsOccupancy_odd-" + key), TString("TDC vs Odd Channel Occupancy (" + key + ")"), 256, -0.5, 255.5, 0, 127.);

        bookEventTrendHisto(cbc_h.errBitVsEvtH, TString("errbit_vs_evt-" + key), TString("Error bit vs Event Number (" + key + ")"), evtsize);
        bookEventTrendHisto(cbc_h.plAddVsEvtH, TString("PLAddress_vs_evt-" + key), TString("Pipeline Address vs Event Number (" + key + ")"), evtsize);

        cbcHMap_.insert({key, cbc_h});
    }

    if(nColumn_ == 1 && nCbc == 4) nCbc = 2;

    int16_t nbin = nCbc * 127;

    if((nCbc % 8) == 0) nbin = 8 * 127;

    if(nCbc == 2)
        periodicity_ = 344;
    else if(nCbc == 16)
        periodicity_ = 347;

    LOG(INFO) << " nCbc " << nCbc << " nbin " << nbin << " Periodicity " << periodicity_;

    dut0HitProfH_ = new TH2I("evenSensor_hitprofile", "Even Sensor Hitmap", nbin, 0.5, nbin + 0.5, nColumn_, -0.5, nColumn_ - 0.5);
    dut1HitProfH_ = new TH2I("oddSensor_hitprofile", "Odd Sensor Hitmap", nbin, 0.5, nbin + 0.5, nColumn_, -0.5, nColumn_ - 0.5);

    if(nColumn_ == 2)
    {
        hitCorrC0H_ = new TH2I("topBottomHitCorrC0", "Hit Correlation between Top Bottom Sensors in Column 0", nbin, 0.5, nbin + 0.5, nbin, 0.5, nbin + 0.5);
        hitCorrC0H_->SetStats(false);
        hitCorrC1H_ = new TH2I("topBottomHitCorrC1", "Hit Correlation between Top Bottom Sensors in Column 1", nbin, 0.5, nbin + 0.5, nbin, 0.5, nbin + 0.5);
        hitCorrC1H_->SetStats(false);
        hitDelCorrC0H = new TH1D("topBottomHitDeltaCorrC0", "Delta Hit Position between Top and Bottom Sensors in Column0", 1 + 2 * nbin, -0.5 - nbin, 0.5 + nbin);
        hitDelCorrC1H = new TH1D("topBottomHitDeltaCorrC1", "Delta Hit Position between Top and Bottom Sensors in Column1", 1 + 2 * nbin, -0.5 - nbin, 0.5 + nbin);

        dut0HitProfUnfoldedH_ = new TH1I("evenSensor_hitprofile_unfold", "Even Sensor Unfolded Hitmap", nbin * 2, 0.5, nbin * 2 + 0.5);
        dut1HitProfUnfoldedH_ = new TH1I("oddSensor_hitprofile_unfold", "Odd Sensor Unfolded Hitmap", nbin * 2, 0.5, nbin * 2 + 0.5);
        dut0C0HitProfH_       = new TH1I("evenSensor_hitprofile_col0", "Even Sensor Hitmap(col 0)", nbin, 0.5, nbin + 0.5);
        dut0C1HitProfH_       = new TH1I("evenSensor_hitprofile_col1", "Even Sensor Hitmap(col 1)", nbin, 0.5, nbin + 0.5);
        dut1C0HitProfH_       = new TH1I("oddSensor_hitprofile_col0", "Odd Sensor Hitmap(col 0)", nbin, 0.5, nbin + 0.5);
        dut1C1HitProfH_       = new TH1I("oddSensor_hitprofile_col1", "Odd Sensor Hitmap (col 1)", nbin, 0.5, nbin + 0.5);
    }
    else
    {
        hitCorrC0H_ = new TH2I("topBottomHitCorrC0", "Hit Correlation between Top Bottom Sensors in Column 0", nbin, 0.5, nbin + 0.5, nbin, 0.5, nbin + 0.5);
        hitCorrC0H_->SetStats(false);
        hitCorrC1H_ = nullptr;

        hitDelCorrC0H         = new TH1D("topBottomHitDeltaCorrC0", "Delta Hit Position between Top and Bottom Sensors in Column0", 1 + 2 * nbin, -0.5 - nbin, 0.5 + nbin);
        hitDelCorrC1H         = 0;
        dut0HitProfUnfoldedH_ = nullptr;
        dut1HitProfUnfoldedH_ = nullptr;
        dut0C0HitProfH_       = new TH1I("evenSensor_hitprofile_col0", "Even Sensor Hitmap(col 0)", nbin, 0.5, nbin + 0.5);
        dut1C0HitProfH_       = new TH1I("oddSensor_hitprofile_col0", "Odd Sensor Hitmap(col 0)", nbin, 0.5, nbin + 0.5);
        dut0C1HitProfH_       = nullptr;
        dut0C1HitProfH_       = nullptr;
    }

    totalNumberHitsH_  = new TH1I("tot_hits", "Total Number of Hits", 101, -0.5, 100.5);
    totalNumberStubsH_ = new TH1I("tot_stubs", "Total Number of Stubs", 101, -0.5, 100.5);

    plAddPhaseDiffH_ = new TH1I("plAddressPhasedDiff", "Pipeline Address Phase difference ", 256, -0.5, 255.5);
    plAddPhaseCorrH_ = new TH2I("plAddressPhasedCorr", "Pipeline Address Phase : CBC0 vs CBC1 ", 256, -0.5, 255.5, 256, -0.5, 255.5);
    cbcErrorCorrH_   = new TH2I("cbcErrorCorr", "CBC Error Cooreation : CBC0 vs CBC1 ", 5, -0.5, 4.5, 5, -0.5, 4.5);

    bookEventTrendHisto(plAddPhaseDiffVsEvtH_, TString("plAddressPhasedDiffVsEvt"), TString("Pipeline Address Phase difference vs Event "), evtsize);

    if(!skipDebugHist_)
    {
        bookEventTrendHisto(bunchCounterVsEvtH_, TString("bunchCounterVsEvt"), TString("Bunch Counter vs Event "), evtsize);
        bookEventTrendHisto(orbitCounterVsEvtH_, TString("orbitCounterVsEvt"), TString("Orbit Counter vs Event "), evtsize);
        bookEventTrendHisto(lumiCounterVsEvtH_, TString("lumiCounterVsEvt"), TString("Lumi Counter vs Event "), evtsize);
        bookEventTrendHisto(l1AcceptVsEvtH_, TString("l1AcceptCounterVsEvt"), TString("L1 Accept Counter vs Event "), evtsize);
        bookEventTrendHisto(cbcCounterVsEvtH_, TString("cbcCounterVsEvt"), TString("CBC Counter vs Event "), evtsize);
        bookEventTrendHisto(tdcCounterVsEvtH_, TString("tdcCounterVsEvt"), TString("TDC Counter vs Event "), evtsize);
        bookEventTrendHisto(periodicityFlagVsEvtH_, TString("pflagVsEvt"), TString("PeriodicityError Flag vs Event "), evtsize);
    }

    cbcErrorVal_     = new std::vector<unsigned int>();
    cbcPLAddressVal_ = new std::vector<unsigned int>();

    dut0C0chData_ = new std::vector<unsigned int>();
    dut0C1chData_ = new std::vector<unsigned int>();
    dut1C0chData_ = new std::vector<unsigned int>();
    dut1C1chData_ = new std::vector<unsigned int>();

    // optionally add a Tree
    if(addTree_)
    {
        tree_ = new TTree("sensorHitTree", "sensorHitTree from RAW files");
        tree_->Branch("l1Accept", &l1Accept_);
        tree_->Branch("tdcCounter", &tdcCounter_);
        tree_->Branch("totalHits", &totalHits_);
        tree_->Branch("totalStubs", &totalStubs_);
        tree_->Branch("eventFlag", &eventFlag_);
        tree_->Branch("eventCbc", &eventCountCBC_);
        tree_->Branch("cbcError", "std::vector<unsigned int>", &cbcErrorVal_);
        tree_->Branch("cbcPLAddress", "std::vector<unsigned int>", &cbcPLAddressVal_);
        tree_->Branch("dut0Ch0data", "std::vector<unsigned int>", &dut0C0chData_);
        tree_->Branch("dut0Ch1data", "std::vector<unsigned int>", &dut0C1chData_);
        tree_->Branch("dut1Ch0data", "std::vector<unsigned int>", &dut1C0chData_);
        tree_->Branch("dut1Ch1data", "std::vector<unsigned int>", &dut1C1chData_);
    }
}
void DQMHistogrammer::fillHistos(const std::vector<Event*>& event_list, int nevtp, const int data_size)
{
    unsigned long ival = nevtp; // as the files is read in chunks
    eventFlag_         = true;

    for(const auto& ev: event_list)
    {
        dut0C0chData_->clear();
        dut0C1chData_->clear();
        dut1C0chData_->clear();
        dut1C1chData_->clear();

        ival++;

        if(filterEvent_)
        {
            eventFlag_ = getEventFlag(ival, data_size);

            if(!eventFlag_ && !skipDebugHist_) fillEventTrendHisto(periodicityFlagVsEvtH_, ival, 1);
        }

        tdcCounter_    = ev->GetTDC();
        l1Accept_      = ev->GetEventCount();
        eventCountCBC_ = ev->GetEventCountCBC();

        totalHits_  = 0;
        totalStubs_ = 0;

        // initialize Tree parameters
        cbcErrorVal_->clear();
        cbcPLAddressVal_->clear();
        uint8_t             ncbc  = 0;
        const EventDataMap& evmap = ev->GetEventDataMap();
        std::vector<int>    oddChVec;
        std::vector<int>    evenChVec;

        for(auto const& it: evmap)
        {
            uint16_t cKey  = it.first;
            uint8_t  feId  = (cKey >> 8) & 0xFF;
            uint8_t  cbcId = cKey & 0xFF;

            ncbc++;

            if(ncbc > cbcHMap_.size()) continue;

            uint32_t          error_cbc     = ev->Error(feId, cbcId);
            uint32_t          pladdress_cbc = ev->PipelineAddress(feId, cbcId);
            int               nstub_cbc     = std::stoi(ev->StubBitString(feId, cbcId), nullptr, 10);
            std::stringstream ss;
            ss << "fed" << +feId << "cbc" << +cbcId;
            std::string key_cbc = ss.str();

            totalStubs_ += nstub_cbc;

            cbcErrorVal_->push_back(error_cbc);
            cbcPLAddressVal_->push_back(pladdress_cbc);

            const std::vector<bool>& dataVec   = ev->DataBitVector(feId, cbcId);
            uint32_t                 nhits_cbc = 0;

            std::vector<uint32_t> fired_channels;

            for(uint32_t ch = 0; ch < dataVec.size(); ++ch)
            {
                int hitposX = -1;
                int hitposY = -1;

                if(dataVec[ch])
                {
                    nhits_cbc++;
                    fired_channels.push_back(ch);
                    uint32_t ichan = ch / 2 + 1;

                    if(cbcId <= 7)
                    {
                        hitposX = 127 * cbcId + ichan;
                        hitposY = 0;
                    }
                    else
                    {
                        hitposX = 2033 - (127 * cbcId + ichan);
                        hitposY = 1;
                    }

                    if(ch % 2 == 0)
                    {
                        evenChVec.push_back(hitposX);

                        if(hitposY == 0)
                            dut0C0chData_->push_back(hitposX);
                        else if(hitposY == 1)
                            dut0C1chData_->push_back(hitposX);
                    }
                    else
                    {
                        oddChVec.push_back(hitposX);

                        if(hitposY == 0)
                            dut1C0chData_->push_back(hitposX);
                        else if(hitposY == 1)
                            dut1C1chData_->push_back(hitposX);
                    }
                }
            }

            if(eventFlag_) fillCBCHistos(ival, key_cbc, error_cbc, pladdress_cbc, nstub_cbc, fired_channels);

            totalHits_ += nhits_cbc;
        }

        if(eventFlag_)
        {
            fillSensorHistos(cbcHMap_.size(), evenChVec, oddChVec);

            if(cbcPLAddressVal_->size() == 2 && cbcErrorVal_->size() == 2 && nColumn_ == 1)
            {
                uint32_t phase = (cbcPLAddressVal_->at(1) - cbcPLAddressVal_->at(0) + 256) % 256;

                if(plAddPhaseDiffH_) plAddPhaseDiffH_->Fill(phase);

                if(plAddPhaseCorrH_) plAddPhaseCorrH_->Fill(cbcPLAddressVal_->at(0), cbcPLAddressVal_->at(1));

                if(cbcErrorCorrH_) cbcErrorCorrH_->Fill(cbcErrorVal_->at(0), cbcErrorVal_->at(1));

                fillEventTrendHisto(plAddPhaseDiffVsEvtH_, ival, phase);
            }

            totalNumberHitsH_->Fill(totalHits_);
            totalNumberStubsH_->Fill(totalStubs_);

            tdcCounterH_->Fill(tdcCounter_);
            l1AcceptH_->Fill(l1Accept_);

            if(!skipDebugHist_)
            {
                fillEventTrendHisto(bunchCounterVsEvtH_, ival, ev->GetBunch());
                fillEventTrendHisto(orbitCounterVsEvtH_, ival, ev->GetOrbit());
                fillEventTrendHisto(lumiCounterVsEvtH_, ival, ev->GetLumi());
                fillEventTrendHisto(l1AcceptVsEvtH_, ival, l1Accept_);
                fillEventTrendHisto(cbcCounterVsEvtH_, ival, ev->GetEventCountCBC());
                fillEventTrendHisto(tdcCounterVsEvtH_, ival, tdcCounter_);
            }

            if(addTree_) tree_->Fill();
        }
    }
}
void DQMHistogrammer::fillCBCHistos(unsigned long ievt, std::string cbc_hid, uint32_t error, uint32_t address, int nstub, const std::vector<uint32_t>& channels)
{
    std::map<std::string, CBCHistos>::iterator iCBCH = cbcHMap_.find(cbc_hid);

    if(iCBCH != cbcHMap_.end())
    {
        CBCHistos& cbc_h = iCBCH->second;
        cbc_h.errBitH->Fill(error);
        fillEventTrendHisto(cbc_h.errBitVsEvtH, ievt, error);
        cbc_h.plAddH->Fill(address);
        fillEventTrendHisto(cbc_h.plAddVsEvtH, ievt, address);
        cbc_h.nStubsH->Fill(nstub);

        for(auto ch: channels)
        {
            uint32_t ichan = ch / 2 + 1;

            if((ch % 2) == 0)
                cbc_h.evenChnOccuH->Fill(ichan);
            else
                cbc_h.oddChnOccuH->Fill(ichan);
        }
    }
}
void DQMHistogrammer::fillSensorHistos(int ncbc, const std::vector<int>& even_values, const std::vector<int>& odd_values)
{
    int xmax = (nColumn_ == 1) ? ncbc * 127 : ncbc / 2 * 127;
    int yval;

    for(auto xval: even_values)
    {
        if(xval <= xmax) yval = 0;

        dut0HitProfH_->Fill(xval, yval);

        if(yval == 0)
            dut0C0HitProfH_->Fill(xval);
        else if(yval == 1)
            dut0C1HitProfH_->Fill(xval);

        if(dut0HitProfUnfoldedH_) dut0HitProfUnfoldedH_->Fill(yval * 127 * 8 + xval);
    }

    for(auto xval: odd_values)
    {
        if(xval <= xmax) yval = 0;

        dut1HitProfH_->Fill(xval, yval);

        if(yval == 0)
            dut1C0HitProfH_->Fill(xval);
        else if(yval == 1)
            dut1C1HitProfH_->Fill(xval);

        if(dut1HitProfUnfoldedH_) dut1HitProfUnfoldedH_->Fill(yval * 127 * 8 + xval);
    }

    if(even_values.size() == odd_values.size())
    {
        for(unsigned int k = 0; k < even_values.size(); k++)
        {
            if(even_values[k] <= xmax)
            {
                hitCorrC0H_->Fill(even_values[k], odd_values[k]);
                hitDelCorrC0H->Fill(even_values[k] - odd_values[k]);
            }
            else
            {
                hitCorrC1H_->Fill(even_values[k], odd_values[k]);
                hitDelCorrC1H->Fill(even_values[k] - odd_values[k]);
            }
        }
    }

    if(!even_values.size() && !odd_values.size())
        sensCorrH_->Fill(1);
    else if(!even_values.size() && odd_values.size())
        sensCorrH_->Fill(2);
    else if(even_values.size() && !odd_values.size())
        sensCorrH_->Fill(3);
    else if(even_values.size() && odd_values.size())
        sensCorrH_->Fill(4);
}
void DQMHistogrammer::saveHistos(const std::string& out_file)
{
    TFile* fout = TFile::Open(out_file.c_str(), "RECREATE");

    // sensor plots
    fout->mkdir("Sensor");
    fout->cd("Sensor");

    if(hitCorrC0H_) hitCorrC0H_->Write();

    if(hitCorrC1H_) hitCorrC1H_->Write();

    if(hitDelCorrC0H) hitDelCorrC0H->Write();

    if(hitDelCorrC1H) hitDelCorrC1H->Write();

    if(dut0HitProfH_) dut0HitProfH_->Write();

    if(dut1HitProfH_) dut1HitProfH_->Write();

    if(dut0HitProfUnfoldedH_) dut0HitProfUnfoldedH_->Write();

    if(dut1HitProfUnfoldedH_) dut1HitProfUnfoldedH_->Write();

    if(dut0C0HitProfH_) dut0C0HitProfH_->Write();

    if(dut0C1HitProfH_) dut0C1HitProfH_->Write();

    if(dut1C0HitProfH_) dut1C0HitProfH_->Write();

    if(dut1C1HitProfH_) dut1C1HitProfH_->Write();

    if(sensCorrH_) sensCorrH_->Write();

    // CBC sections
    for(auto& imap: cbcHMap_)
    {
        CBCHistos& cbc_h = imap.second;
        fout->mkdir(imap.first.c_str());
        fout->cd(imap.first.c_str());
        cbc_h.errBitH->Write();
        cbc_h.errBitVsEvtH->Write();
        cbc_h.plAddH->Write();
        cbc_h.plAddVsEvtH->Write();
        cbc_h.nStubsH->Write();
        cbc_h.evenChnOccuH->Write();
        cbc_h.oddChnOccuH->Write();
        cbc_h.tdcVsEvenChnOccuH->Write();
        cbc_h.tdcVsOddChnOccuH->Write();
    }

    // common ones
    fout->cd();

    if(l1AcceptH_) l1AcceptH_->Write();

    if(tdcCounterH_) tdcCounterH_->Write();

    if(totalNumberHitsH_) totalNumberHitsH_->Write();

    if(totalNumberStubsH_) totalNumberStubsH_->Write();

    if(plAddPhaseDiffH_) plAddPhaseDiffH_->Write();

    if(cbcErrorCorrH_) cbcErrorCorrH_->Write();

    if(plAddPhaseCorrH_) plAddPhaseCorrH_->Write();

    if(plAddPhaseDiffVsEvtH_) plAddPhaseDiffVsEvtH_->Write();

    if(!skipDebugHist_)
    {
        if(bunchCounterVsEvtH_) bunchCounterVsEvtH_->Write();

        if(orbitCounterVsEvtH_) orbitCounterVsEvtH_->Write();

        if(lumiCounterVsEvtH_) lumiCounterVsEvtH_->Write();

        if(l1AcceptVsEvtH_) l1AcceptVsEvtH_->Write();

        if(cbcCounterVsEvtH_) cbcCounterVsEvtH_->Write();

        if(tdcCounterVsEvtH_) tdcCounterVsEvtH_->Write();

        if(periodicityFlagVsEvtH_) periodicityFlagVsEvtH_->Write();
    }

    fout->Close();

    // Optionally save the tree
    if(addTree_)
    {
        TFile* fout2 = TFile::Open("Rawtuple_" + TString(out_file.c_str()), "RECREATE");
        fout2->cd();
        tree_->Write();
        fout2->Close();
    }
}
bool DQMHistogrammer::getEventFlag(const unsigned long& ievt, const int data_size)
{
    if((ievt % eventBlock_) == 0) lineOffset_ += eventBlock_ * data_size;

    bool flag = true;

    for(int i = 1; i <= data_size; i++)
    {
        long line = (ievt - 1) * data_size + i;

        if((line - lineOffset_) % periodicity_ == 0)
        {
            flag = false;
            break;
        }
    }

    return flag;
}
void DQMHistogrammer::resetHistos()
{
    TIter    next(gROOT->GetList());
    TObject* obj;

    while((obj = next()))
    {
        if(obj->InheritsFrom("TH1"))
            dynamic_cast<TH1*>(obj)->Reset();
        else if(obj->InheritsFrom("TTree"))
            dynamic_cast<TTree*>(obj)->Reset();
    }
}
void DQMHistogrammer::bookEventTrendHisto(TH1I*& th, const TString& name, const TString& title, int size)
{
    th = new TH1I(name, title, size, 0.5, size + 0.5);
#ifdef ROOT5
    th->SetBit(TH1::kCanRebin); // valid for Root 5
#endif
#ifdef ROOT6
    th->SetCanExtend(TH1::kXaxis);
#endif
}
void DQMHistogrammer::fillEventTrendHisto(TH1I* th, unsigned long ival, unsigned int val)
{
    if(!th) return;

    if(ival > th->GetXaxis()->GetXmax()) th->RebinAxis(ival, th->GetXaxis());

    th->Fill(ival, val);
}
