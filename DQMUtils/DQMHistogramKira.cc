/*!
        \file                DQMHistogramKira.h
        \brief               class to create and fill monitoring histograms for KIRA tests
        \author              Roland Koppenhoefer
        \version             1.0
        \date                27/7/22
        Support :            mail to : rkoppenh@cern.ch
 */

#include "DQMUtils/DQMHistogramKira.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerStream.h"
#include "Utils/EmptyContainer.h"
#include "Utils/GenericDataArray.h"
#include "Utils/HybridContainerStream.h"
#include "Utils/Occupancy.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/Utilities.h"

//========================================================================================================================
DQMHistogramKira::DQMHistogramKira()
{
    fStartLatency = 999;
    fLatencyRange = 999;
}

//========================================================================================================================
DQMHistogramKira::~DQMHistogramKira() {}

//========================================================================================================================
void DQMHistogramKira::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    uint32_t cNCh         = 0;
    uint32_t cNSeedChsS0  = 0;
    uint32_t cNSeedChsS1  = 0;
    uint32_t cNChannelsS0 = 0;
    uint32_t cNChannelsS1 = 0;
    uint32_t cNLinks      = 0;
    for(auto board: theDetectorStructure)
    {
        uint32_t cLinks = 0;
        for(auto opticalGroup: *board)
        {
            cLinks++;
            for(auto hybrid: *opticalGroup)
            {
                uint32_t cN       = 0;
                uint32_t cNS0     = 0;
                uint32_t cNS1     = 0;
                uint32_t cNSeedS0 = 0;
                uint32_t cNSeedS1 = 0;

                uint32_t cMaxS0      = 0;
                uint32_t cMaxS1      = 0;
                uint32_t cMaxSeedsS0 = 0;
                uint32_t cMaxSeedsS1 = 0;
                for(auto chip: *hybrid)
                {
                    cN += chip->size();
                    // only account for seeds in MPAs/CBCs
                    if(chip->size() == NMPACHANNELS)
                    {
                        cNSeedS0 = chip->size() / NMPACOLS;
                        cNS0     = chip->size() / NMPACOLS;
                    }
                    else if(chip->size() == NCHANNELS)
                    {
                        cNSeedS0 = chip->size() / 2; // either bottom/top CBC row can be a seed
                        cNS0     = chip->size() / 2;
                        cNS1     = cNS0;
                        cNSeedS1 = cNSeedS0;
                    }
                    else
                    {
                        cNSeedS1 = chip->size();
                        cNS1     = chip->size();
                    }

                    if(cNS0 > cMaxS0) cMaxS0 = cNS0;
                    if(cNS1 > cMaxS1) cMaxS1 = cNS1;
                    if(cNSeedS0 > cMaxSeedsS0) cMaxSeedsS0 = cNSeedS0;
                    if(cNSeedS1 > cMaxSeedsS1) cMaxSeedsS1 = cNSeedS1;
                } // chip

                if(8 * cNS0 > cNChannelsS0) cNChannelsS0 = 8 * cMaxS0;
                if(8 * cNS1 > cNChannelsS1) cNChannelsS1 = 8 * cMaxS1;
                if(8 * cNSeedS0 > cNSeedChsS0) cNSeedChsS0 = 8 * cNSeedS0;
                if(8 * cNSeedS1 > cNSeedChsS1) cNSeedChsS1 = 8 * cNSeedS1;
                if(cN > cNCh) cNCh = cN;

            } // hybrid
        }     // OG
        if(cLinks >= cNLinks) cNLinks = cLinks;
    } // board

    LOG(INFO) << BOLDYELLOW << "Total number of channels in S0s connected to this BeBoard " << cNChannelsS0 * cNLinks * 2 << RESET;
    LOG(INFO) << BOLDYELLOW << "Number of channels in S0 " << cNChannelsS0 << RESET;
    LOG(INFO) << BOLDYELLOW << "Number of channels in S1 " << cNChannelsS1 << RESET;
    LOG(INFO) << BOLDYELLOW << "Number of seeds in S0 " << cNSeedChsS0 << RESET;
    LOG(INFO) << BOLDYELLOW << "Number of seeds in S1 " << cNSeedChsS1 << RESET;
    // need to get settings from settings map
    parseSettings(pSettingsMap);

    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    LOG(INFO) << "Setting histograms with range " << fLatencyRange << " and start value " << fStartLatency;

    float               cBinSize = (1.0);
    HistContainer<TH1F> hLatency("LatencyValue", "Latency Value", fLatencyRange / cBinSize, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistograms, hLatency);

    // hit count for latency
    HistContainer<TH1F> hLatencyChip("LatencyChip", "Latency per Chip", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyTDCHistograms, hLatencyChip);

    // hit count per chip during KIRA test
    HistContainer<TH1F> hKIRAHitsBottom("KIRAHitsBottom", "KIRA Hits in Bottom Sensor", NCHANNELS / 2, 0, NCHANNELS / 2);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fKIRAHitsBottomSensor, hKIRAHitsBottom);
    HistContainer<TH1F> hKIRAHitsTop("KIRAHitsTop", "KIRA Hits in Top Sensor", NCHANNELS / 2, 0, NCHANNELS / 2);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fKIRAHitsTopSensor, hKIRAHitsTop);
}

//========================================================================================================================
bool DQMHistogramKira::fill(std::vector<char>& dataBuffer) { return false; }

//========================================================================================================================
void DQMHistogramKira::process() {}

//========================================================================================================================

void DQMHistogramKira::reset(void) {}

void DQMHistogramKira::fillLatencyPlots(uint16_t pLatency, uint16_t pTriggerId, DetectorDataContainer& pTDCsummary, uint32_t pNevents)
{
    LOG(DEBUG) << BOLDMAGENTA << "Filling chip latency plots .." << RESET;
    for(auto board: pTDCsummary)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    TH1F* cLatencyTDC =
                        fLatencyTDCHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    int      cBin   = cLatencyTDC->FindBin((float)pLatency);
                    uint32_t cNhits = pTDCsummary.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint16_t>();
                    LOG(INFO) << BOLDMAGENTA << "\t\t..Latency of " << pLatency << " bin of " << +cBin << " OG" << +opticalGroup->getId() << " Hybrid" << +hybrid->getId() << " Chip" << +chip->getId()
                              << " - on average have found " << cNhits << " channels with a hit [per chip per event]." << RESET;
                    cLatencyTDC->SetBinContent(cBin, cNhits / 127. / pNevents);
                }
            }
        }
    }
}

void DQMHistogramKira::fillBottomSensorPlots(DetectorDataContainer& pHitContainer, uint32_t pNevents, uint16_t pLED)
{
    LOG(DEBUG) << BOLDMAGENTA << "Filling bottom sensor chip plots .." << RESET;
    for(auto board: pHitContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    // skip all chips that are not directly illuminated by the LED
                    if(hybrid->getIndex() % 2 == 0 && chip->getIndex() != 7 - pLED) continue;
                    if(hybrid->getIndex() % 2 == 1 && chip->getIndex() != pLED) continue;
                    TH1F* cKIRAHitsBottomSensor =
                        fKIRAHitsBottomSensor.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    auto cNhits = pHitContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<GenericDataArray<VECSIZE, float>>();
                    for(uint32_t cIndx = 0; cIndx < 127; cIndx++) { cKIRAHitsBottomSensor->SetBinContent(cIndx + 1, cNhits[cIndx] / (1.0 * pNevents)); }
                }
            }
        }
    }
}

void DQMHistogramKira::fillTopSensorPlots(DetectorDataContainer& pHitContainer, uint32_t pNevents, uint16_t pLED)
{
    LOG(DEBUG) << BOLDMAGENTA << "Filling top sensor chip plots .." << RESET;
    for(auto board: pHitContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    // skip all chips that are not directly illuminated by the LED
                    if(hybrid->getIndex() % 2 == 0 && chip->getIndex() != 7 - pLED) continue;
                    if(hybrid->getIndex() % 2 == 1 && chip->getIndex() != pLED) continue;
                    TH1F* cKIRAHitsTopSensor =
                        fKIRAHitsTopSensor.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    auto cNhits = pHitContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<GenericDataArray<VECSIZE, float>>();
                    for(uint32_t cIndx = 0; cIndx < 127; cIndx++) { cKIRAHitsTopSensor->SetBinContent(cIndx + 1, cNhits[cIndx] / (1.0 * pNevents)); }
                }
            }
        }
    }
}

void DQMHistogramKira::parseSettings(const Ph2_Parser::SettingsMap& pSettingsMap)
{
    auto cSetting = pSettingsMap.find("StartLatency");
    if(cSetting != std::end(pSettingsMap))
        fStartLatency = static_cast<uint16_t>(boost::any_cast<double>(cSetting->second));
    else
        fStartLatency = 0;

    cSetting = pSettingsMap.find("LatencyRange");
    if(cSetting != std::end(pSettingsMap))
        fLatencyRange = static_cast<uint16_t>(boost::any_cast<double>(cSetting->second));
    else
        fLatencyRange = 512;
}
