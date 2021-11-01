/*!
        \file                DQMHistogramBeamTestCheck.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramBeamTestCheck.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"
#include "../Utils/ThresholdAndNoise.h"
#include "../Utils/Utilities.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramBeamTestCheck::DQMHistogramBeamTestCheck()
{
    fStartLatency = 999;
    fLatencyRange = 999;
}

//========================================================================================================================
DQMHistogramBeamTestCheck::~DQMHistogramBeamTestCheck() {}

//========================================================================================================================
void DQMHistogramBeamTestCheck::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    uint32_t cNCh = 0;
    for(auto board: theDetectorStructure)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                uint32_t cN = 0;
                for(auto chip: *hybrid) { cN += chip->size(); } // chip
                if(cN > cNCh) cNCh = cN;
            } // hybrid
        }     // OG
    }         // board

    // need to get settings from settings map
    parseSettings(pSettingsMap);

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    LOG(INFO) << "Setting histograms with range " << fLatencyRange << " and start value " << fStartLatency;

    HistContainer<TH1F> hLatency("LatencyValue", "Latency Value", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistograms, hLatency);

    HistContainer<TH1F> hLatencyS0("LatencyValueS0", "Latency Value [bottom sensor]", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistogramsS0, hLatencyS0);

    HistContainer<TH1F> hLatencyS1("LatencyValueS1", "Latency Value [top sensor]", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistogramsS1, hLatencyS1);

    HistContainer<TH1F> hStub("StubValue", "Stub Value", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubHistograms, hStub);

    HistContainer<TH2F> hLatencyScan2D("LatencyScan2D", "LatencyScan2D", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyScan2DHistograms, hLatencyScan2D);

    HistContainer<TH1F> hTriggerTDC("TriggerTDC", "Trigger TDC", TDCBINS, -0.5, TDCBINS - 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fTriggerTDCHistograms, hTriggerTDC);

    // hit map vs. latency
    HistContainer<TH2F> hLatencyHitMap("LatencyHitMap", "Latency HitMap", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, cNCh, 0, cNCh);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHitMaps, hLatencyHitMap);

    // hit count for TDC phase + latency
    HistContainer<TH2F> hLatencyTDC("LatencyTDC", "Latency TDC", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, TDCBINS, 0, TDCBINS);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyTDCHistograms, hLatencyTDC);

    // Cluster Count
    HistContainer<TH2F> hClusterOccupancy("CLusterOccupancy", "Cluster Occupancy", 1024, 0, 1024, 40 / 0.25, 0, 40);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fClusterOccupancyHistograms, hClusterOccupancy);
}

//========================================================================================================================
bool DQMHistogramBeamTestCheck::fill(std::vector<char>& dataBuffer)
{
    HybridContainerStream<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, uint16_t>>                            theLatencyStream("LatencyScan");
    HybridContainerStream<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, uint16_t>>                            theStubStream("LatencyScanStub");
    HybridContainerStream<EmptyContainer, EmptyContainer, GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>> the2DStream("LatencyScan2D");
    HybridContainerStream<EmptyContainer, EmptyContainer, GenericDataArray<TDCBINS, uint16_t>>                            theTriggerTDCStream("LatencyScanTriggerTDC");

    if(theLatencyStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched Latency Stream!!!!!\n";
        theLatencyStream.decodeHybridData(fDetectorData);
        fillLatencyPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    if(theTriggerTDCStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched TriggerTDC!!!!!\n";
        theTriggerTDCStream.decodeHybridData(fDetectorData);
        fillTriggerTDCPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    if(theTriggerTDCStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched Stub Latency!!!!!\n";
        theStubStream.decodeHybridData(fDetectorData);
        fillStubLatencyPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    if(the2DStream.attachBuffer(&dataBuffer))
    {
        std::cout << "Matched 2D Latency!!!!!\n";
        the2DStream.decodeHybridData(fDetectorData);
        fill2DLatencyPlots(fDetectorData);
        fDetectorData.cleanDataStored();
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramBeamTestCheck::process()
{
    // latency plot
    for(auto board: fLatencyHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TCanvas* latencyCanvas = new TCanvas(("Latency_" + std::to_string(hybrid->getId()) + "_Summary").data(), ("Latency_" + std::to_string(hybrid->getId()) + "_Summary").data(), 500, 500);
                // latencyCanvas->DivideSquare(hybrid->size());
                latencyCanvas->cd();
                TH1F* latencyHistogram = hybrid->getSummary<HistContainer<TH1F>>().fTheHistogram;
                latencyHistogram->GetXaxis()->SetTitle("Trigger Latency");
                latencyHistogram->GetYaxis()->SetTitle("< Hit Occupancy > Bottom Sensor");
                latencyHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fLatencyHistogramsS0)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TCanvas* latencyCanvas =
                    new TCanvas(("Latency_" + std::to_string(hybrid->getId()) + "_BottomSensor").data(), ("Latency_" + std::to_string(hybrid->getId()) + "_BottomSensor").data(), 500, 500);
                // latencyCanvas->DivideSquare(hybrid->size());
                latencyCanvas->cd();
                TH1F* latencyHistogram = hybrid->getSummary<HistContainer<TH1F>>().fTheHistogram;
                latencyHistogram->GetXaxis()->SetTitle("Trigger Latency");
                latencyHistogram->GetYaxis()->SetTitle("< Hit Occupancy > Bottom Sensor");
                latencyHistogram->DrawCopy();
            }
        }
    }
    // latency plot
    for(auto board: fLatencyHistogramsS1)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TCanvas* latencyCanvas =
                    new TCanvas(("Latency_" + std::to_string(hybrid->getId()) + "_TopSensor").data(), ("Latency_" + std::to_string(hybrid->getId()) + "_TopSensor").data(), 500, 500);
                // latencyCanvas->DivideSquare(hybrid->size());
                latencyCanvas->cd();
                TH1F* latencyHistogram = hybrid->getSummary<HistContainer<TH1F>>().fTheHistogram;
                latencyHistogram->GetXaxis()->SetTitle("Trigger Latency");
                latencyHistogram->GetYaxis()->SetTitle("< Hit Occupancy > Top Sensor");
                latencyHistogram->DrawCopy();
            }
        }
    }
    // hit map
    for(auto board: fLatencyHitMaps)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TCanvas* cCanvas = new TCanvas(("LatencyHitMap_" + std::to_string(hybrid->getId())).data(), ("Latency Hit Map " + std::to_string(hybrid->getId())).data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Trigger Latency");
                cHistogram->GetYaxis()->SetTitle("Strip Number");
                cHistogram->GetZaxis()->SetTitle("< Hit Occupancy >");
                cHistogram->DrawCopy();
            }
        }
    }
    // TDC trigger latency plot
    for(auto board: fLatencyTDCHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    std::string cCanvasName  = "LatencyTDC_" + std::to_string(chip->getId()) + std::to_string(hybrid->getId());
                    std::string cCanvasTitle = "Latency TDC plot " + std::to_string(chip->getId()) + std::to_string(hybrid->getId());

                    TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                    cCanvas->cd();
                    auto& cHistogram = chip->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    cHistogram->GetXaxis()->SetTitle("Trigger Latency");
                    cHistogram->GetYaxis()->SetTitle("TDC Phase");
                    cHistogram->GetZaxis()->SetTitle("< Hit Occupancy >");
                    cHistogram->DrawCopy();
                }
            }
        }
    }
    for(auto board: fClusterOccupancyHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    std::string cCanvasName  = "ClusterOccupancy_" + std::to_string(chip->getId()) + std::to_string(hybrid->getId());
                    std::string cCanvasTitle = "ClusterOccupancy plot " + std::to_string(chip->getId()) + std::to_string(hybrid->getId());

                    TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                    cCanvas->cd();
                    auto& cHistogram = chip->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    cHistogram->GetXaxis()->SetTitle("Threshold");
                    cHistogram->GetYaxis()->SetTitle("Cluster Occupancy");
                    cHistogram->GetZaxis()->SetTitle("<Count>");
                    cHistogram->DrawCopy();
                }
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramBeamTestCheck::reset(void) {}

void DQMHistogramBeamTestCheck::fillClusterOccupancyPlots(DetectorDataContainer& pOccupancy)
{
    for(auto board: pOccupancy)
    {
        auto& cBrdClstrs = pOccupancy.at(board->getIndex());
        for(auto opticalGroup: *board)
        {
            auto& cOGClstrs = cBrdClstrs->at(opticalGroup->getIndex());
            for(auto hybrid: *opticalGroup)
            {
                auto& cHybrdClstrs = cOGClstrs->at(hybrid->getIndex());
                for(auto chip: *hybrid)
                {
                    TH2F* chipClstOccHist =
                        fClusterOccupancyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    auto& cChipClstrs = cHybrdClstrs->at(chip->getIndex());
                    auto  cSize       = cChipClstrs->getSummary<GenericDataArray<VECSIZE, float>>().getSize();
                    for(size_t cIndx = 0; cIndx < cSize; cIndx++)
                    {
                        auto cClusterOccupancy = cChipClstrs->getSummary<GenericDataArray<VECSIZE, float>>()[cIndx];
                        auto cBinNum           = chipClstOccHist->FindBin(cIndx, cClusterOccupancy);
                        chipClstOccHist->SetBinContent(cBinNum, 1);
                        chipClstOccHist->SetBinError(cBinNum, 0);
                    }
                }
            }
        }
    }
}
//
void DQMHistogramBeamTestCheck::fillLatencyPlots(uint16_t pLatency, DetectorDataContainer& pOccupancy, DetectorDataContainer& pTDCsummary)
{
    // float cOccGlbl = pOccupancy.getSummary<Occupancy, Occupancy>().fOccupancy;
    // LOG (INFO) << BOLDBLUE << "Global Occ is " << cOccGlbl << RESET;
    for(auto board: pOccupancy)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // float cNhits=0;
                TH2F* cHitMap = fLatencyHitMaps.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH1F* cHist   = fLatencyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(auto chip: *hybrid)
                {
                    float cOcc   = chip->getSummary<Occupancy>().fOccupancy;
                    float cError = chip->getSummary<Occupancy>().fOccupancyError;
                    auto  cBin   = cHist->FindBin((float)pLatency);
                    cHist->SetBinContent(cBin, cOcc * chip->size());
                    cHist->SetBinError(cBin, cError * chip->size());
                    uint16_t cChnlIndx = 0;
                    uint16_t cOffset   = chip->getId() * chip->size() / 2.;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        uint16_t cStripOffset = (cChnlIndx % 2 == 0) ? cOffset : cHitMap->GetYaxis()->GetNbins() / 2. + cOffset;
                        uint16_t cStripId     = cStripOffset + cChnlIndx / 2.0;
                        if(hybrid->getId() % 2 == 0)
                        {
                            cStripOffset = (cChnlIndx % 2 == 0) ? cHitMap->GetYaxis()->GetNbins() / 2. : cHitMap->GetYaxis()->GetNbins();
                            cStripId     = cStripOffset - (chip->getId() * chip->size() / 2. + cChnlIndx / 2.0);
                        }
                        cBin = cHitMap->FindBin((float)pLatency, cStripId);
                        cHitMap->SetBinContent(cBin, channel.fOccupancy);
                        cHitMap->SetBinError(cBin, channel.fOccupancyError);
                        cChnlIndx++;
                    }

                    TH2F* cLatencyTDC =
                        fLatencyTDCHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    for(uint8_t cTDC = 0; cTDC < TDCBINS; cTDC++)
                    {
                        cBin = cLatencyTDC->FindBin((float)pLatency, (float)cTDC);
                        uint32_t cNhits =
                            pTDCsummary.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDC];
                        // LOG (INFO) << BOLDMAGENTA << "\t\t..TDC phase of " << +cTDC << " latency of " << pLatency
                        //         << " bin of " << +cBin
                        //         <<  " OG" << +opticalGroup->getId()
                        //         << " Hybrid" << +hybrid->getId()
                        //         << " Chip" << +chip->getId()
                        //         << " - on average have found " << cNhits << " channels with a hit [per chip per event]."
                        //         << RESET;
                        cLatencyTDC->SetBinContent(cBin, cNhits);
                        cLatencyTDC->SetBinError(cBin, std::sqrt((float)cNhits)); // for now
                    }
                }
                // float cError = 0;
                // if(cNhits > 0) cError = sqrt(float(cNhits));
                // auto cBin = cHist->FindBin( (float)pLatency );
                // cHist->SetBinContent(cBin, cNhits);
                // cHist->SetBinError(cBin, cError);
            }
        }
    }
}

void DQMHistogramBeamTestCheck::fillLatencyPlots(DetectorDataContainer& theLatencyS0, DetectorDataContainer& theLatencyS1)
{
    for(auto board: theLatencyS0)
    {
        auto& cBrdHitsS1 = theLatencyS1.at(board->getIndex());
        for(auto opticalGroup: *board)
        {
            auto& cOGHitsS1 = cBrdHitsS1->at(opticalGroup->getIndex());
            for(auto hybrid: *opticalGroup)
            {
                auto& cHybridHitsS1 = cOGHitsS1->at(hybrid->getIndex());

                bool cFillS0 = (hybrid->hasSummary());
                bool cFillS1 = (cHybridHitsS1->hasSummary());

                // if(cFillS0) LOG (INFO) << BOLDYELLOW << "Filling latnecy plots for S0" << RESET;
                // if(cFillS1) LOG (INFO) << BOLDYELLOW << "Filling latency plots for S1" << RESET;
                TH1F* hybridLatencyHistogramS0 = fLatencyHistogramsS0.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridLatencyHistogramS1 = fLatencyHistogramsS1.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    if(cFillS0)
                    {
                        uint32_t hits  = hybrid->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                        float    error = 0;
                        if(hits > 0) error = sqrt(float(hits));
                        hybridLatencyHistogramS0->SetBinContent(i, hits);
                        hybridLatencyHistogramS0->SetBinError(i, error);
                    }
                    if(cFillS1)
                    {
                        uint32_t hits  = cHybridHitsS1->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                        float    error = 0;
                        if(hits > 0) error = sqrt(float(hits));
                        hybridLatencyHistogramS1->SetBinContent(i, hits);
                        hybridLatencyHistogramS1->SetBinError(i, error);
                    }
                }
            }
        }
    }
}
void DQMHistogramBeamTestCheck::fillLatencyPlots(DetectorDataContainer& theLatency)
{
    for(auto board: theLatency)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                bool  cFill                  = (hybrid->hasSummary());
                TH1F* hybridLatencyHistogram = fLatencyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    if(cFill)
                    {
                        uint32_t hits  = hybrid->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                        float    error = 0;
                        if(hits > 0) error = sqrt(float(hits));
                        hybridLatencyHistogram->SetBinContent(i, hits);
                        hybridLatencyHistogram->SetBinError(i, error);
                    }
                }
            }
        }
    }
}
//
void DQMHistogramBeamTestCheck::fillStubLatencyPlots(DetectorDataContainer& theStubLatency)
{
    for(auto board: theStubLatency)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1F* hybridLatencyHistogram = fStubHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    uint32_t hits = hybrid->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];

                    float error = 0;
                    if(hits > 0) error = sqrt(float(hits));

                    hybridLatencyHistogram->SetBinContent(i, hits);
                    hybridLatencyHistogram->SetBinError(i, error);
                }
            }
        }
    }
}
void DQMHistogramBeamTestCheck::fill2DLatencyPlots(DetectorDataContainer& the2DLatency)
{
    for(auto board: the2DLatency)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1F* hybridLatencyHistogram = fStubHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    for(uint8_t cStubLatency = 0; cStubLatency < i + fStartLatency; cStubLatency++)
                    {
                        uint32_t hits = hybrid->getSummary<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>>()[cStubLatency][i];

                        hybridLatencyHistogram->SetBinContent(cStubLatency, i, hits);
                    }
                }
            }
        }
    }
}
void DQMHistogramBeamTestCheck::fillTriggerTDCPlots(DetectorDataContainer& theTriggerTDC)
{
    for(auto board: theTriggerTDC)
    {
        for(uint32_t tdcValue = 0; tdcValue < TDCBINS; ++tdcValue)
        {
            auto  sum                      = board->at(0)->at(0)->getSummary<GenericDataArray<TDCBINS, uint16_t>>();
            TH1F* boardTriggerTDCHistogram = fTriggerTDCHistograms.at(board->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            boardTriggerTDCHistogram->SetBinContent(tdcValue + 1, sum[tdcValue]);
        }
    }
}

void DQMHistogramBeamTestCheck::parseSettings(const Ph2_System::SettingsMap& pSettingsMap)
{
    auto cSetting = pSettingsMap.find("StartLatency");
    if(cSetting != std::end(pSettingsMap))
        fStartLatency = cSetting->second;
    else
        fStartLatency = 0;

    cSetting = pSettingsMap.find("LatencyRange");
    if(cSetting != std::end(pSettingsMap))
        fLatencyRange = cSetting->second;
    else
        fLatencyRange = 512;
}
