/*!
        \file                DQMHistogramCicFEAlignment.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramCicFEAlignment.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Utilities.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramCicFEAlignment::DQMHistogramCicFEAlignment() {}

//========================================================================================================================
DQMHistogramCicFEAlignment::~DQMHistogramCicFEAlignment() {}

//========================================================================================================================
void DQMHistogramCicFEAlignment::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // need to get settings from settings map
    parseSettings(pSettingsMap);

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH2F> hManualPhaseScan("ManualPhaseScan", ";CIC Input Sampling Phase; Output SLVS Line", 15, 0, 15, 5, 0, 5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fManualPhaseScan, hManualPhaseScan);
}

//========================================================================================================================
bool DQMHistogramCicFEAlignment::fill(std::vector<char>& dataBuffer) { return false; }

//========================================================================================================================
void DQMHistogramCicFEAlignment::process()
{
    for(auto board: fManualPhaseScan)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    std::string cCanvasName  = "ManualPhaseScan_" + std::to_string(chip->getId()) + std::to_string(hybrid->getId());
                    std::string cCanvasTitle = "Manual Phase Scan CIC inputs " + std::to_string(chip->getId()) + std::to_string(hybrid->getId());

                    TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                    cCanvas->cd();
                    auto& cHistogram = chip->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    cHistogram->GetXaxis()->SetTitle("CIC Input Sampling Phase");
                    cHistogram->GetYaxis()->SetTitle("Output SLVS Line");
                    cHistogram->GetZaxis()->SetTitle("Number of successfully transmitted bits");
                    cHistogram->GetXaxis()->SetRangeUser(0, 15);
                    cHistogram->DrawCopy();
                }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramCicFEAlignment::reset(void) {}

void DQMHistogramCicFEAlignment::parseSettings(const Ph2_System::SettingsMap& pSettingsMap)
{
    // auto cSetting = pSettingsMap.find("StartLatency");
    // if(cSetting != std::end(pSettingsMap))
    //     fStartLatency = cSetting->second;
    // else
    //     fStartLatency = 0;

    // cSetting = pSettingsMap.find("LatencyRange");
    // if(cSetting != std::end(pSettingsMap))
    //     fLatencyRange = cSetting->second;
    // else
    //     fLatencyRange = 512;
}

void DQMHistogramCicFEAlignment::fillManualPhaseScan(uint8_t pPhase, uint8_t pLine, DetectorDataContainer& pErrors, DetectorDataContainer& pData)
{
    for(auto board: pErrors)
    {
        auto& cBrdData = pData.at(board->getIndex());
        auto& cBrdHist = fManualPhaseScan.at(board->getIndex());
        for(auto opticalGroup: *board)
        {
            auto& cOGData = cBrdData->at(opticalGroup->getIndex());
            auto& cOGHist = cBrdHist->at(opticalGroup->getIndex());
            for(auto hybrid: *opticalGroup)
            {
                auto& cHybrdData = cOGData->at(hybrid->getIndex());
                auto& cHybrdHist = cOGHist->at(hybrid->getIndex());
                for(auto chip: *hybrid)
                {
                    auto& cChipData   = cHybrdData->at(chip->getIndex());
                    auto& cInputData  = cChipData->getSummary<std::string>();
                    auto& cErrors     = chip->getSummary<uint32_t>();
                    TH2F* cChipHist   = cHybrdHist->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    auto  cBin        = cChipHist->FindBin(pPhase, pLine);
                    auto  cBinContent = cChipHist->GetBinContent(cBin);
                    cChipHist->SetBinContent(cBin, cBinContent + cInputData.length() - cErrors);
                    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +chip->getId() << " " << cChipHist->GetBinContent(cBin) << " correctly transmitted bits " << RESET;
                }
            }
        }
    }
}