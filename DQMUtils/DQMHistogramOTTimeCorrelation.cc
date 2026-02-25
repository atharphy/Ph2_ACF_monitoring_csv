/*!
 * \file DQMOTTimeCorrelation.cc
 * \brief DQM class for OTTimeCorrelations
 * \author [Your Name]
 * \date [Date]
 */

#include "DQMUtils/DQMHistogramOTTimeCorrelation.h"

DQMHistogramOTTimeCorrelation::DQMHistogramOTTimeCorrelation() {}

DQMHistogramOTTimeCorrelation::~DQMHistogramOTTimeCorrelation() {}

void DQMHistogramOTTimeCorrelation::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    this->book(theOutputFile, theDetectorStructure, pSettingsMap, "");
}

void DQMHistogramOTTimeCorrelation::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap, std::string suffix)
{
    fDetectorContainer = &theDetectorStructure;
    fOutputFile        = theOutputFile;
    fSettingsMap       = pSettingsMap;

    LOG(INFO) << "Booking DQMHistogramOTTimeCorrelation histograms with suffix: " << suffix;

    // BOOKING SSA HISTOGRAMS
    HistContainer<TH2F> hSameEvSSA(("SameEvHistSSA" + suffix).c_str(), "SSA Same event correlation", 1920, 0, 1920, 1920, 0, 1920);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSameEvCorrSSAHistogramsMap[suffix], hSameEvSSA);

    auto                depth = OTTimeCorrelationConfig::getN();
    HistContainer<TH2F> hStripFWTC_SSA(("StripTCFWHistSSA" + suffix).c_str(), "SSA FW same strip time correlation", 1920, 0, 1920, depth, 0, depth);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSameStripFWTCorrSSAHistogramsMap[suffix], hStripFWTC_SSA);

    HistContainer<TH2F> hStripBWTC_SSA(("StripTCBWHistSSA" + suffix).c_str(), "SSA BW same strip time correlation", 1920, 0, 1920, depth, 0, depth);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSameStripBWTCorrSSAHistogramsMap[suffix], hStripBWTC_SSA);

    HistContainer<TH2F> hMinHitsSSA(("MinHitsFWHistSSA" + suffix).c_str(), "SSA FW MinHits", depth, 0, depth, 10, 0, 4);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fMinHitsFWSSAHistogramsMap[suffix], hMinHitsSSA);

    HistContainer<TH2F> hMinHitsBWSSA(("MinHitsBWHistSSA" + suffix).c_str(), "SSA BW MinHits", depth, 0, depth, 10, 0, 4);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fMinHitsBWSSAHistogramsMap[suffix], hMinHitsBWSSA);

    HistContainer<TH3F> h3DTSFWCorrSSA(("3DTSFWCorrSSA" + suffix).c_str(), "SSA FW time and space correlation", 1920, 0, 1920, 1920, 0, 1920, 9, 0, 9);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f3DTSFWCorrSSAHistogramsMap[suffix], h3DTSFWCorrSSA);

    HistContainer<TH3F> h3DTSBWCorrSSA(("3DTSBWCorrSSA" + suffix).c_str(), "SSA BW time and space correlation", 1920, 0, 1920, 1920, 0, 1920, 9, 0, 9);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f3DTSBWCorrSSAHistogramsMap[suffix], h3DTSBWCorrSSA);

    // BOOKING MPA HISTOGRAMS
    HistContainer<TH2F> hSameEvMPA(("SameEvHistMPA" + suffix).c_str(), "MPA Same event correlation", 1920, 0, 1920, 1920, 0, 1920);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSameEvCorrMPAHistogramsMap[suffix], hSameEvMPA);

    HistContainer<TH2F> hPixelFWTC_MPA(("PixelTCFWHistMPA" + suffix).c_str(), "MPA FW same pixel time correlation", 1920, 0, 1920, depth, 0, depth);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSamePixelFWTCorrMPAHistogramsMap[suffix], hPixelFWTC_MPA);

    HistContainer<TH2F> hPixelBWTC_MPA(("PixelTCBWHistMPA" + suffix).c_str(), "MPA BW same pixel time correlation", 1920, 0, 1920, depth, 0, depth);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSamePixelBWTCorrMPAHistogramsMap[suffix], hPixelBWTC_MPA);

    HistContainer<TH2F> hMinHitsMPA(("MinHitsFWHistMPA" + suffix).c_str(), "MPA FW MinHits", depth, 0, depth, 10, 0, 4);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fMinHitsFWMPAHistogramsMap[suffix], hMinHitsMPA);

    HistContainer<TH2F> hMinHitsBWMPA(("MinHitsBWHistMPA" + suffix).c_str(), "MPA BW MinHits", depth, 0, depth, 10, 0, 4);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fMinHitsBWMPAHistogramsMap[suffix], hMinHitsBWMPA);

    HistContainer<TH3F> h3DTSFWCorrMPA(("3DTSFWCorrMPA" + suffix).c_str(), "MPA FW time and space correlation", 1920, 0, 1920, 1920, 0, 1920, 9, 0, 9);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f3DTSFWCorrMPAHistogramsMap[suffix], h3DTSFWCorrMPA);

    HistContainer<TH3F> h3DTSBWCorrMPA(("3DTSBWCorrMPA" + suffix).c_str(), "MPA BW time and space correlation", 1920, 0, 1920, 1920, 0, 1920, 9, 0, 9);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, f3DTSBWCorrMPAHistogramsMap[suffix], h3DTSBWCorrMPA);

    // SLICES (both SSA and MPA)
    int nSlices = OTTimeCorrelationConfig::nSlices();

    fTSCorrSliceFWSSAMap[suffix].resize(nSlices);
    fTSCorrSliceBWSSAMap[suffix].resize(nSlices);
    fTSCorrSliceFWMPAMap[suffix].resize(nSlices);
    fTSCorrSliceBWMPAMap[suffix].resize(nSlices);

    for(int iz = 0; iz < nSlices; ++iz)
    {
        // SSA Slices
        TString             hnameSSA = Form("TSFWCorrSliceSSA_%d_%s", iz, suffix.c_str());
        HistContainer<TH2F> hTSCorrSliceSSA(hnameSSA, Form("SSA FW correlation ev=%.2d", iz), 1920, 0, 1920, 1920, 0, 1920);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fTSCorrSliceFWSSAMap[suffix][iz], hTSCorrSliceSSA);

        TString             hnameBWSSA = Form("TSBWCorrSliceSSA_%d_%s", iz, suffix.c_str());
        HistContainer<TH2F> hTSCorrSliceBWSSA(hnameBWSSA, Form("SSA BW correlation ev=%.2d", iz), 1920, 0, 1920, 1920, 0, 1920);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fTSCorrSliceBWSSAMap[suffix][iz], hTSCorrSliceBWSSA);

        // MPA Slices
        TString             hnameMPA = Form("TSFWCorrSliceMPA_%d_%s", iz, suffix.c_str());
        HistContainer<TH2F> hTSCorrSliceMPA(hnameMPA, Form("MPA FW correlation ev=%.2d", iz), 1920, 0, 1920, 1920, 0, 1920);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fTSCorrSliceFWMPAMap[suffix][iz], hTSCorrSliceMPA);

        TString             hnameBWMPA = Form("TSBWCorrSliceMPA_%d_%s", iz, suffix.c_str());
        HistContainer<TH2F> hTSCorrSliceBWMPA(hnameBWMPA, Form("MPA BW correlation ev=%.2d", iz), 1920, 0, 1920, 1920, 0, 1920);
        RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fTSCorrSliceBWMPAMap[suffix][iz], hTSCorrSliceBWMPA);
    }

    HistContainer<TH1F> hMPAError(("hMPAError" + suffix).c_str(), "MPA chip errors", 8, 0, 8);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fMPAErrorHistogramsMap[suffix], hMPAError);

    HistContainer<TH1F> hSSAError(("hSSAError" + suffix).c_str(), "SSA chip errors", 8, 0, 8);
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fSSAErrorHistogramsMap[suffix], hSSAError);
}

void DQMHistogramOTTimeCorrelation::process()
{
    // Process histograms if needed
}

bool DQMHistogramOTTimeCorrelation::fill(std::string& inputStream)
{
    // Standard fill from raw data buffer if needed
    return true;
}

void DQMHistogramOTTimeCorrelation::reset()
{
    // Reset histograms
}

void DQMHistogramOTTimeCorrelation::fillErrorHist(const ChipErrorData& errorData, uint32_t trgBurst, uint32_t trgDel, std::string suffix)
{
    if(fMPAErrorHistogramsMap.count(suffix))
    {
        for(auto board: fMPAErrorHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH1F* theHistogram = opticalGroup->getSummary<HistContainer<TH1F>>().fTheHistogram;
                theHistogram->SetTitle(Form("MPA chip errors Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                for(const auto& [chipId, errorCount]: errorData.mpaErrors) { theHistogram->SetBinContent(chipId + 1, errorCount); }
            }
        }
    }
    else { LOG(WARNING) << "No MPA error histograms found for suffix: " << suffix; }

    if(fSSAErrorHistogramsMap.count(suffix))
    {
        for(auto board: fSSAErrorHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH1F* theHistogram = opticalGroup->getSummary<HistContainer<TH1F>>().fTheHistogram;
                theHistogram->SetTitle(Form("SSA chip errors Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));
                for(const auto& [chipId, errorCount]: errorData.ssaErrors) { theHistogram->SetBinContent(chipId + 1, errorCount); }
            }
        }
    }
    else { LOG(WARNING) << "No SSA error histograms found for suffix: " << suffix; }
}

void DQMHistogramOTTimeCorrelation::fillSSAData(const OTTimeCorrelationStripData& sData, uint32_t trgBurst, uint32_t trgDel, std::string suffix)
{
    if(fSameEvCorrSSAHistogramsMap.count(suffix))
    {
        for(auto board: fSameEvCorrSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Same event correlations Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                std::vector<int> stripData = sData.stripData;

                for(size_t i = 0; i < stripData.size(); ++i)
                {
                    for(size_t j = 0; j < stripData.size(); ++j) { theHistogram->Fill(stripData[i], stripData[j]); }
                }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    if(fSameStripFWTCorrSSAHistogramsMap.count(suffix))
    {
        for(auto board: fSameStripFWTCorrSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("FW strip time correlations Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& lastCorr = OTTimeCorrelationStripData::getLastFWCorrelation();
                for(size_t i = 0; i < lastCorr.channels.size(); ++i) { theHistogram->Fill(lastCorr.channels[i], lastCorr.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    if(fSameStripBWTCorrSSAHistogramsMap.count(suffix))
    {
        for(auto board: fSameStripBWTCorrSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("BW strip time correlations Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& lastCorr = OTTimeCorrelationStripData::getLastBWCorrelation();
                for(size_t i = 0; i < lastCorr.channels.size(); ++i) { theHistogram->Fill(lastCorr.channels[i], lastCorr.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    if(!fMinHitsFWSSAHistogramsMap.count(suffix) || !fMinHitsBWSSAHistogramsMap.count(suffix) || !f3DTSFWCorrSSAHistogramsMap.count(suffix) || !f3DTSBWCorrSSAHistogramsMap.count(suffix))
    {
        LOG(WARNING) << "No histograms found for suffix: " << suffix;
    }
    else
    {
        for(auto board: fMinHitsFWSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Min hits Board %d OG %d,  tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));
                theHistogram->GetYaxis()->SetTitle("log(# hits)");

                auto& lastHits = OTTimeCorrelationStripData::getLastFWMinHits();
                for(size_t i = 0; i < lastHits.size(); ++i) { theHistogram->Fill(i, std::log10(lastHits[i])); }
            }
        }
        for(auto board: fMinHitsBWSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Min hits BW Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));
                theHistogram->GetYaxis()->SetTitle("log(# hits)");

                auto& lastHits = OTTimeCorrelationStripData::getLastBWMinHits();
                for(size_t i = 0; i < lastHits.size(); ++i) { theHistogram->Fill(i, std::log10(lastHits[i])); }
            }
        }
    }

    if(!f3DTSFWCorrSSAHistogramsMap.count(suffix) || !f3DTSBWCorrSSAHistogramsMap.count(suffix)) { LOG(WARNING) << "No histograms found for suffix: " << suffix; }
    else
    {
        for(auto board: f3DTSFWCorrSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH3F* theHistogram = opticalGroup->getSummary<HistContainer<TH3F>>().fTheHistogram;
                theHistogram->SetTitle(Form("FW time and space corr Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& last3D = OTTimeCorrelationStripData::getLastFW3DCorrelation();
                for(size_t i = 0; i < last3D.ch_0.size(); ++i) { theHistogram->Fill(last3D.ch_0[i], last3D.ch_1[i], last3D.indexes[i]); }
            }
        }

        for(auto board: f3DTSBWCorrSSAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH3F* theHistogram = opticalGroup->getSummary<HistContainer<TH3F>>().fTheHistogram;
                theHistogram->SetTitle(Form("BW time and space corr Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& last3D = OTTimeCorrelationStripData::getLastBW3DCorrelation();
                for(size_t i = 0; i < last3D.ch_0.size(); ++i) { theHistogram->Fill(last3D.ch_0[i], last3D.ch_1[i], last3D.indexes[i]); }
            }
        }
    }

    // Fill the 2D slice histograms
    if(fTSCorrSliceFWSSAMap.count(suffix) && fTSCorrSliceBWSSAMap.count(suffix))
    {
        auto& fwSlicesVec = fTSCorrSliceFWSSAMap.at(suffix);
        auto& bwSlicesVec = fTSCorrSliceBWSSAMap.at(suffix);

        auto& lastFW3D = OTTimeCorrelationStripData::getLastFW3DCorrelation();
        auto& lastBW3D = OTTimeCorrelationStripData::getLastBW3DCorrelation();

        int nSlices      = OTTimeCorrelationStripData::nSlices();
        int safeFWSlices = std::min((int)nSlices, (int)fwSlicesVec.size());
        int safeBWSlices = std::min((int)nSlices, (int)bwSlicesVec.size());

        for(int iz = 0; iz < safeFWSlices; ++iz)
        {
            for(auto board: fwSlicesVec[iz])
            {
                for(auto opticalGroup: *board)
                {
                    TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    if(!theHistogram) continue;

                    theHistogram->SetTitle(Form("FW TS Slice %d Board %d OG %d, tBurst %d, tDel %d", iz, board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                    for(size_t i = 0; i < lastFW3D.ch_0.size(); ++i)
                    {
                        if(lastFW3D.indexes[i] == iz) { theHistogram->Fill(lastFW3D.ch_0[i], lastFW3D.ch_1[i]); }
                    }
                }
            }
        }

        for(int iz = 0; iz < safeBWSlices; ++iz)
        {
            for(auto board: bwSlicesVec[iz])
            {
                for(auto opticalGroup: *board)
                {
                    TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    if(!theHistogram) continue;

                    theHistogram->SetTitle(Form("BW TS Slice %d Board %d OG %d, tBurst %d, tDel %d", iz, board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                    for(size_t i = 0; i < lastBW3D.ch_0.size(); ++i)
                    {
                        if(lastBW3D.indexes[i] == iz) { theHistogram->Fill(lastBW3D.ch_0[i], lastBW3D.ch_1[i]); }
                    }
                }
            }
        }
    }
}

void DQMHistogramOTTimeCorrelation::fillMPAData(const OTTimeCorrelationPixelData& pData, uint32_t trgBurst, uint32_t trgDel, std::string suffix)
{
    // Same Event Correlation (Pixel)
    if(fSameEvCorrMPAHistogramsMap.count(suffix))
    {
        for(auto board: fSameEvCorrMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Same event correlations MPA Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                const std::vector<int>& pixelData = pData.pixelData;

                for(size_t i = 0; i < pixelData.size(); ++i)
                {
                    for(size_t j = 0; j < pixelData.size(); ++j) { theHistogram->Fill(pixelData[i], pixelData[j]); }
                }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    // Time Correlation FW/BW (Pixel)
    if(fSamePixelFWTCorrMPAHistogramsMap.count(suffix))
    {
        for(auto board: fSamePixelFWTCorrMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("FW pixel time correlations Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& lastCorr = OTTimeCorrelationPixelData::getLastFWCorrelation();
                for(size_t i = 0; i < lastCorr.channels.size(); ++i) { theHistogram->Fill(lastCorr.channels[i], lastCorr.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    if(fSamePixelBWTCorrMPAHistogramsMap.count(suffix))
    {
        for(auto board: fSamePixelBWTCorrMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("BW pixel time correlations Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& lastCorr = OTTimeCorrelationPixelData::getLastBWCorrelation();
                for(size_t i = 0; i < lastCorr.channels.size(); ++i) { theHistogram->Fill(lastCorr.channels[i], lastCorr.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    // Min Hits FW/BW (Pixel)
    if(fMinHitsFWMPAHistogramsMap.count(suffix) && fMinHitsBWMPAHistogramsMap.count(suffix))
    {
        for(auto board: fMinHitsFWMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Min hits MPA Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));
                theHistogram->GetYaxis()->SetTitle("log(# hits)");

                auto& lastHits = OTTimeCorrelationPixelData::getLastFWMinHits();
                for(size_t i = 0; i < lastHits.size(); ++i) { theHistogram->Fill(i, std::log10(lastHits[i])); }
            }
        }
        for(auto board: fMinHitsBWMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                theHistogram->SetTitle(Form("Min hits BW MPA Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));
                theHistogram->GetYaxis()->SetTitle("log(# hits)");

                auto& lastHits = OTTimeCorrelationPixelData::getLastBWMinHits();
                for(size_t i = 0; i < lastHits.size(); ++i) { theHistogram->Fill(i, std::log10(lastHits[i])); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    // 3D Time and Space Correlation (Pixel)
    if(f3DTSFWCorrMPAHistogramsMap.count(suffix))
    {
        for(auto board: f3DTSFWCorrMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH3F* theHistogram = opticalGroup->getSummary<HistContainer<TH3F>>().fTheHistogram;
                theHistogram->SetTitle(Form("FW pixel TS corr Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& last3D = OTTimeCorrelationPixelData::getLastFW3DCorrelation();
                for(size_t i = 0; i < last3D.ch_0.size(); ++i) { theHistogram->Fill(last3D.ch_0[i], last3D.ch_1[i], last3D.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    if(f3DTSBWCorrMPAHistogramsMap.count(suffix))
    {
        for(auto board: f3DTSBWCorrMPAHistogramsMap.at(suffix))
        {
            for(auto opticalGroup: *board)
            {
                TH3F* theHistogram = opticalGroup->getSummary<HistContainer<TH3F>>().fTheHistogram;
                theHistogram->SetTitle(Form("BW pixel TS corr Board %d OG %d, tBurst %d, tDel %d", board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                auto& last3D = OTTimeCorrelationPixelData::getLastBW3DCorrelation();
                for(size_t i = 0; i < last3D.ch_0.size(); ++i) { theHistogram->Fill(last3D.ch_0[i], last3D.ch_1[i], last3D.indexes[i]); }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }

    // 2D Slices (Pixel)
    if(fTSCorrSliceFWMPAMap.count(suffix) && fTSCorrSliceBWMPAMap.count(suffix))
    {
        auto& fwSlicesVec = fTSCorrSliceFWMPAMap.at(suffix);
        auto& bwSlicesVec = fTSCorrSliceBWMPAMap.at(suffix);

        auto& lastFW3D = OTTimeCorrelationPixelData::getLastFW3DCorrelation();
        auto& lastBW3D = OTTimeCorrelationPixelData::getLastBW3DCorrelation();

        int nSlices      = OTTimeCorrelationPixelData::nSlices();
        int safeFWSlices = std::min((int)nSlices, (int)fwSlicesVec.size());
        int safeBWSlices = std::min((int)nSlices, (int)bwSlicesVec.size());

        for(int iz = 0; iz < safeFWSlices; ++iz)
        {
            for(auto board: fwSlicesVec[iz])
            {
                for(auto opticalGroup: *board)
                {
                    TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    if(!theHistogram) continue;

                    theHistogram->SetTitle(Form("MPA FW TS Slice %d Board %d OG %d, tBurst %d, tDel %d", iz, board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                    for(size_t i = 0; i < lastFW3D.ch_0.size(); ++i)
                    {
                        if(lastFW3D.indexes[i] == iz) { theHistogram->Fill(lastFW3D.ch_0[i], lastFW3D.ch_1[i]); }
                    }
                }
            }
        }

        for(int iz = 0; iz < safeBWSlices; ++iz)
        {
            for(auto board: bwSlicesVec[iz])
            {
                for(auto opticalGroup: *board)
                {
                    TH2F* theHistogram = opticalGroup->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    if(!theHistogram) continue;

                    theHistogram->SetTitle(Form("MPA BW TS Slice %d Board %d OG %d, tBurst %d, tDel %d", iz, board->getId(), opticalGroup->getId(), trgBurst, trgDel));

                    for(size_t i = 0; i < lastBW3D.ch_0.size(); ++i)
                    {
                        if(lastBW3D.indexes[i] == iz) { theHistogram->Fill(lastBW3D.ch_0[i], lastBW3D.ch_1[i]); }
                    }
                }
            }
        }
    }
    else { LOG(WARNING) << "No histograms found for suffix: " << suffix; }
}