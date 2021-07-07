/*!
        \file                DQMHistogramLatencyScan.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "../DQMUtils/DQMHistogramLatencyScan.h"
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
DQMHistogramLatencyScan::DQMHistogramLatencyScan()
{
    fStartLatency = 999;
    fLatencyRange = 999;
}

//========================================================================================================================
DQMHistogramLatencyScan::~DQMHistogramLatencyScan() {}

//========================================================================================================================
void DQMHistogramLatencyScan::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // need to get settings from settings map
    parseSettings(pSettingsMap);

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    LOG(INFO) << "Setting histograms with range " << fLatencyRange << " and start value " << fStartLatency;

    HistContainer<TH1F> hLatency("LatencyValue", "Latency Value", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistograms, hLatency);

    HistContainer<TH1F> hStub("StubValue", "Stub Value", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fStubHistograms, hStub);

    HistContainer<TH2F> hLatencyScan2D("LatencyScan2D", "LatencyScan2D", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyScan2DHistograms, hLatencyScan2D);

    HistContainer<TH1F> hTriggerTDC("TriggerTDC", "Trigger TDC", TDCBINS, -0.5, TDCBINS - 0.5);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fTriggerTDCHistograms, hTriggerTDC);
}

//========================================================================================================================
bool DQMHistogramLatencyScan::fill(std::vector<char>& dataBuffer)
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
void DQMHistogramLatencyScan::process()
{
    for(auto board: fLatencyHistograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TCanvas* latencyCanvas = new TCanvas(("Latency_" + std::to_string(hybrid->getId())).data(), ("Latency " + std::to_string(hybrid->getId())).data(), 500, 500);

                // latencyCanvas->DivideSquare(hybrid->size());

                latencyCanvas->cd();
                TH1F* latencyHistogram = hybrid->getSummary<HistContainer<TH1F>>().fTheHistogram;
                latencyHistogram->GetXaxis()->SetTitle("Trigger Latency");
                latencyHistogram->GetYaxis()->SetTitle("# of hits");
                latencyHistogram->DrawCopy();
            }
        }
    }
}

//========================================================================================================================

void DQMHistogramLatencyScan::reset(void) {}

void DQMHistogramLatencyScan::fillLatencyPlots(DetectorDataContainer& theLatency)
{
    for(auto board: theLatency)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1F* hybridLatencyHistogram = fLatencyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;

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
void DQMHistogramLatencyScan::fillStubLatencyPlots(DetectorDataContainer& theStubLatency)
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
void DQMHistogramLatencyScan::fill2DLatencyPlots(DetectorDataContainer& the2DLatency)
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
void DQMHistogramLatencyScan::fillTriggerTDCPlots(DetectorDataContainer& theTriggerTDC)
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

void DQMHistogramLatencyScan::parseSettings(const Ph2_System::SettingsMap& pSettingsMap)
{
    auto cSetting = pSettingsMap.find("StartLatency");
    if(cSetting != std::end(pSettingsMap))
        fStartLatency = boost::any_cast<double>(cSetting->second);
    else
        fStartLatency = 0;

    cSetting = pSettingsMap.find("LatencyRange");
    if(cSetting != std::end(pSettingsMap))
        fLatencyRange = boost::any_cast<double>(cSetting->second);
    else
        fLatencyRange = 512;
}
