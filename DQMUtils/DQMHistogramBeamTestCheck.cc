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
    uint32_t cNCh         = 0;
    uint32_t cNSeedChsS0  = 0;
    uint32_t cNSeedChsS1  = 0;
    uint32_t cNChannelsS0 = 0;
    uint32_t cNChannelsS1 = 0;
    uint32_t cNLinks      = 0; 
    for(auto board: theDetectorStructure)
    {
        uint32_t cLinks=0; 
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
        if( cLinks >= cNLinks ) cNLinks = cLinks; 
    }         // board

    LOG(INFO) << BOLDYELLOW << "Total number of channels in S0s connected to this BeBoard " << cNChannelsS0*cNLinks*2 << RESET;
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

    HistContainer<TH1F> hLatencyS0("LatencyValueS0", "Latency Value [bottom sensor]", fLatencyRange / cBinSize, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistogramsS0, hLatencyS0);

    HistContainer<TH1F> hLatencyS1("LatencyValueS1", "Latency Value [top sensor]", fLatencyRange / cBinSize, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHistogramsS1, hLatencyS1);

    HistContainer<TH1F> hStub("StubValue", "Stub Value", fLatencyRange, 0, 0 + fLatencyRange);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubHistograms, hStub);

    HistContainer<TH2F> hLatencyScan2D("LatencyScan2D", "LatencyScan2D", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, fLatencyRange, fStartLatency, fStartLatency + fLatencyRange);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyScan2DHistograms, hLatencyScan2D);

    // hit map vs. latency
    HistContainer<TH2F> hLatencyHitMap("LatencyHitMap", "Latency HitMap", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, cNCh, 0, cNCh);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fLatencyHitMaps, hLatencyHitMap);

    // hit count for TDC phase + latency
    HistContainer<TH2F> hLatencyTDC("LatencyTDC", "Latency TDC", fLatencyRange, fStartLatency, fStartLatency + fLatencyRange, TDCBINS, 0, TDCBINS);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fLatencyTDCHistograms, hLatencyTDC);

    //
    HistContainer<TH1F> hTriggerTDC("TriggerTDC", "Trigger TDC", TDCBINS, 0, TDCBINS);
    RootContainerFactory::bookBoardHistograms(theOutputFile, theDetectorStructure, fTriggerTDCHistograms, hTriggerTDC);

    // Cluster Count
    HistContainer<TH2F> hClusterOccupancy("CLusterOccupancy", "Cluster Occupancy", 1024, 0, 1024, 40 / 0.25, 0, 40);
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fClusterOccupancyHistograms, hClusterOccupancy);

    // hit maps per hybrid
    float cBinSizeY = (cNChannelsS0 == 8 * NSSACHANNELS) ? 1.0 / (float)NMPACOLS : 1.;
    LOG(INFO) << BOLDYELLOW << "Bin size in Y is " << cBinSizeY << RESET;
    HistContainer<TH2F> hMitMapS0("HitOccupancyS0", "Hit Occupancy [S0]", cNChannelsS0, 0, cNChannelsS0, 2 / cBinSizeY, 0, 2);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHitMapS0Histograms, hMitMapS0);
    cBinSizeY = 1.;
    HistContainer<TH2F> hMitMapS1("HitOccupancyS1", "Hit Occupancy [S1]", cNChannelsS1, 0, cNChannelsS1, 2 / cBinSizeY, 0, 2);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fHitMapS1Histograms, hMitMapS1);

    // then stubs
    cBinSizeY = (cNChannelsS0 == 8 * NSSACHANNELS) ? 1.0 / (float)NMPACOLS : 1;
    HistContainer<TH2F> hStubMapS0("StubOccupancyS0", "Stub Occupancy [S0]", cNChannelsS0, 0, cNChannelsS0, 2 / cBinSizeY, 0, 2);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubMapS0Histograms, hStubMapS0);
    cBinSizeY = 1.;
    HistContainer<TH2F> hStubMapS1("StubOccupancyS1", "Stub Occupancy [S1]", cNChannelsS1, 0, cNChannelsS1, 2 / cBinSizeY, 0, 2);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubMapS1Histograms, hStubMapS1);

    // bend histogrsm per hybrid
    HistContainer<TH1F> hBendDist("BendDistribution", "Stub Bend Distribution [strips]", 30 / 0.5, -15, 15);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBendHistrograms, hBendDist);

    // number of stubs per hybrid 
    HistContainer<TH2F> hStubCount("StubCount", "Stub Count [strips]", TDCBINS, 0 , TDCBINS,  30 / 0.5, -15, 15 );
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStubCountHistrograms, hStubCount);
    HistContainer<TH2F> hEventCount("EventCount", "EventCount Count [strips]", TDCBINS, 0 , TDCBINS,  30 / 0.5, -15, 15 );
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fEventCountHistrograms, hEventCount);
    


    // correlation plots per hybrid 
    HistContainer<TH2F> hHitCorrS0S1("HitCorrelationS0S1", "Channel [S0]; Channel [S1]", cNChannelsS0, 0, cNChannelsS0,  cNChannelsS1 , 0 , cNChannelsS1 );
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fCorrelationS0S1Histograms, hHitCorrS0S1);
    HistContainer<TH2F> hStubCorrS0("StubCorrelationsS0", "Stub [S0]; Channel [S0]", cNChannelsS0, 0, cNChannelsS0,  cNChannelsS0 , 0 , cNChannelsS0 );
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fCorrelationStubS0Histograms, hStubCorrS0);
    HistContainer<TH2F> hStubCorrS1("StubCorrelationsS1", "Stub [S0]; Channel [S1]", cNChannelsS0, 0, cNChannelsS0,  cNChannelsS1 , 0 , cNChannelsS1 );
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fCorrelationStubS1Histograms, hStubCorrS1);
    HistContainer<TH2F> hStubCorrLinksS0("StubCorrelationsLinksS0", "Stubs [S0]; Stubs [S0]", cNChannelsS0*cNLinks*2, 0, cNChannelsS0*cNLinks*2,  cNChannelsS0*cNLinks*2 , 0 , cNChannelsS0*cNLinks*2 );
    RootContainerFactory::bookBoardHistograms(theOutputFile, theDetectorStructure, fCorrelationLinksS0Histogram, hStubCorrLinksS0);
    
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

    for(auto board: fHitMapS0Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "HitOccupancy_S0_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "HitOccupancy [S0] plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Channel Number [local x S0]");
                cHistogram->GetYaxis()->SetTitle("Module Side [local y S0]");
                cHistogram->GetZaxis()->SetTitle("<Count>");
                cHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fHitMapS1Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "HitOccupancy_S1_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "HitOccupancy [S1] plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Channel Number [local x S1]");
                cHistogram->GetYaxis()->SetTitle("Module Side [local y S1]");
                cHistogram->GetZaxis()->SetTitle("<Count>");
                cHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fStubMapS0Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "StubOccupancy_S0_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "StubOccupancy [S0] plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Seed Channel Number [local x S0]");
                cHistogram->GetYaxis()->SetTitle("Module Side [local y S0]");
                cHistogram->GetZaxis()->SetTitle("<Count>");
                cHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fStubMapS1Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "StubOccupancy_S1_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "StubOccupancy [S1] plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Seed Channel Number [local x S1]");
                cHistogram->GetYaxis()->SetTitle("Module Side [local y S1]");
                cHistogram->GetZaxis()->SetTitle("<Count>");
                cHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fBendHistrograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "BendDistribution_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "BendDistribution plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Bend [strips]");
                cHistogram->GetYaxis()->SetTitle("Count");
                cHistogram->DrawCopy();
            }
        }
    }

    for(auto board: fStubCountHistrograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "StubCountDistribution_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "StubCountDistribution_ plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("TDC Phase");
                cHistogram->GetYaxis()->SetTitle("Calculated Bend [strips/pixels]");
                cHistogram->GetZaxis()->SetTitle("Count [Stubs]");
                cHistogram->DrawCopy();
            }
        }
    }
    
    for(auto board: fEventCountHistrograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "EventCountDistribution_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "EventCountDistribution_ plot " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("TDC Phase");
                cHistogram->GetYaxis()->SetTitle("Calculated Bend [strips/pixels]");
                cHistogram->GetZaxis()->SetTitle("Count [Events]");
                cHistogram->DrawCopy();
            }
        }
    }
    



    //correlation plots S0 S1
    for(auto board: fCorrelationS0S1Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "CorrelationS0S1_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "Correlation [S0:S1] " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Row [S0]");
                cHistogram->GetYaxis()->SetTitle("Row [S1]");
                cHistogram->DrawCopy();
            }
        }
    }
    //correlation plots Stubs S0 
    for(auto board: fCorrelationStubS0Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "CorrelationStubS0_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "Correlation [Stubs:S0] " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Stub ");
                cHistogram->GetYaxis()->SetTitle("Row [S0]");
                cHistogram->DrawCopy();
            }
        }
    }
    for(auto board: fCorrelationStubS1Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName  = "CorrelationStubS1_" + std::to_string(hybrid->getId());
                std::string cCanvasTitle = "Correlation [Stubs:S1] " + std::to_string(hybrid->getId());

                TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
                cCanvas->cd();
                auto& cHistogram = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                cHistogram->GetXaxis()->SetTitle("Stub ");
                cHistogram->GetYaxis()->SetTitle("Row [S1]");
                cHistogram->DrawCopy();
            }
        }
    }
    for(auto board: fCorrelationLinksS0Histogram)
    {
        std::string cCanvasName  = "CorrelationLinks_" + std::to_string(board->getId());
        std::string cCanvasTitle = "Correlation [Stubs:Stubs] " + std::to_string(board->getId());

        TCanvas* cCanvas = new TCanvas(cCanvasName.data(), cCanvasTitle.data(), 500, 500);
        cCanvas->cd();
        auto& cHistogram = board->getSummary<HistContainer<TH2F>>().fTheHistogram;
        cHistogram->GetXaxis()->SetTitle("Stubs on Link");
        cHistogram->GetYaxis()->SetTitle("Stubs on Link");
        cHistogram->DrawCopy();
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
void DQMHistogramBeamTestCheck::fillLatencyPlots(uint16_t pLatency, uint16_t pTriggerId, DetectorDataContainer& pOccupancy, DetectorDataContainer& pTDCsummary)
{
    LOG(DEBUG) << BOLDMAGENTA << "Filling latency plots with TDC summary .." << RESET;
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
                    auto  cBin   = cHist->FindBin((float)pLatency + (float)(pTriggerId) / 100.);
                    LOG(DEBUG) << BOLDYELLOW << "Latency of " << +pLatency << " trigger# " << +pTriggerId << " bin#" << cBin << RESET;
                    cHist->SetBinContent(cBin, cOcc * chip->size());
                    cHist->SetBinError(cBin, cError * chip->size());
                    uint16_t cChnlIndx = 0;
                    uint16_t cOffset   = chip->getId() * chip->size() / 2;
                    for(auto channel: *chip->getChannelContainer<Occupancy>())
                    {
                        uint16_t cStripOffset = (cChnlIndx % 2 == 0) ? cOffset : cHitMap->GetYaxis()->GetNbins() / 2 + cOffset;
                        uint16_t cStripId     = cStripOffset + cChnlIndx / 2;
                        if(hybrid->getId() % 2 == 0)
                        {
                            cStripOffset = (cChnlIndx % 2 == 0) ? cHitMap->GetYaxis()->GetNbins() / 2 : cHitMap->GetYaxis()->GetNbins();
                            cStripId     = cStripOffset - (chip->getId() * chip->size() / 2 + cChnlIndx / 2);
                        }
                        if(channel.fOccupancy > 0)
                            LOG(DEBUG) << BOLDMAGENTA << "\t\t..ROC#" << +chip->getId() << " Channel " << cChnlIndx << " strip number " << cChnlIndx / 2.0 << " strip offset is " << cStripOffset
                                       << " global strip number " << +cStripId << " hit is in S" << +(cChnlIndx % 2 == 0) << " - have found " << channel.fOccupancy << " hits." << RESET;

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
                        LOG(DEBUG) << BOLDMAGENTA << "\t\t..TDC phase of " << +cTDC << " latency of " << pLatency << " bin of " << +cBin << " OG" << +opticalGroup->getId() << " Hybrid"
                                   << +hybrid->getId() << " Chip" << +chip->getId() << " - on average have found " << cNhits << " channels with a hit [per chip per event]." << RESET;
                        cLatencyTDC->SetBinContent(cBin, cNhits);
                        cLatencyTDC->SetBinError(cBin, std::sqrt((float)cNhits)); // for now
                    }
                }
            }
        }
    }
}

void DQMHistogramBeamTestCheck::fillLatencyPlots(DetectorDataContainer& theLatencyS0, DetectorDataContainer& theLatencyS1)
{
    LOG(INFO) << BOLDMAGENTA << "Filling latency plots for S0/S1 .." << RESET;

    for(auto board: theLatencyS0)
    {
        auto& cBrdHitsS1 = theLatencyS1.at(board->getIndex());
        for(auto opticalGroup: *board)
        {
            auto& cOGHitsS1 = cBrdHitsS1->at(opticalGroup->getIndex());
            for(auto cHybridHitsS0: *opticalGroup)
            {
                auto& cHybridHitsS1 = cOGHitsS1->at(cHybridHitsS0->getIndex());

                bool cFillS0 = (cHybridHitsS0->hasSummary());
                bool cFillS1 = (cHybridHitsS1->hasSummary());

                TH1F* hybridLatencyHistogram = fLatencyHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(cHybridHitsS0->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridLatencyHistogramS0 =
                    fLatencyHistogramsS0.at(board->getIndex())->at(opticalGroup->getIndex())->at(cHybridHitsS0->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                TH1F* hybridLatencyHistogramS1 =
                    fLatencyHistogramsS1.at(board->getIndex())->at(opticalGroup->getIndex())->at(cHybridHitsS1->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    uint32_t hits_total = 0;
                    if(cFillS0)
                    {
                        uint32_t hits  = cHybridHitsS0->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                        float    error = 0;
                        if(hits > 0) error = sqrt(float(hits));
                        auto cBin = hybridLatencyHistogramS0->FindBin((float)(fStartLatency + i));
                        hybridLatencyHistogramS0->SetBinContent(cBin, hits);
                        hybridLatencyHistogramS0->SetBinError(cBin, error);
                        hits_total = hits;
                    }
                    if(cFillS1)
                    {
                        uint32_t hits  = cHybridHitsS1->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                        float    error = 0;
                        if(hits > 0) error = sqrt(float(hits));
                        auto cBin = hybridLatencyHistogramS1->FindBin((float)(fStartLatency + i));
                        hybridLatencyHistogramS1->SetBinContent(cBin, hits);
                        hybridLatencyHistogramS1->SetBinError(cBin, error);
                        hits_total += hits;
                    }
                    auto cBin = hybridLatencyHistogram->FindBin((float)(fStartLatency + i));
                    hybridLatencyHistogram->SetBinContent(cBin, hits_total);
                    float error = (hits_total > 0) ? sqrt(float(hits_total)) : 0;
                    hybridLatencyHistogram->SetBinError(cBin, error);
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
    LOG(DEBUG) << BOLDBLUE << "Filling Stub Latency Plots..." << RESET;
    for(auto board: theStubLatency)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                if(!hybrid->hasSummary()) continue;
                TH1F* hybridLatencyHistogram = fStubHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                LOG(DEBUG) << BOLDBLUE << ".....Hybrid#" << +hybrid->getId() << RESET;

                for(uint32_t i = 0; i < fLatencyRange; i++)
                {
                    uint32_t hits  = hybrid->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[i];
                    float    error = 0;
                    if(hits > 0) { error = sqrt(float(hits)); }
                    auto cBin = hybridLatencyHistogram->FindBin((float)i);
                    hybridLatencyHistogram->SetBinContent(cBin, hits);
                    hybridLatencyHistogram->SetBinError(cBin, error);
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
        // bool  cFill                  = (theTriggerTDC.hasSummary());
        // if(!cFill){ LOG (INFO) << BOLDYELLOW << "No TDC container to fill for " << +board->getIndex() << RESET; }
        // retreive TDC counts for the board
        auto sum = board->getSummary<GenericDataArray<TDCBINS, uint16_t>>();
        for(uint32_t tdcValue = 0; tdcValue < TDCBINS; ++tdcValue)
        {
            TH1F* boardTriggerTDCHistogram = fTriggerTDCHistograms.at(board->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
            auto  cBin                     = boardTriggerTDCHistogram->GetXaxis()->FindBin(tdcValue);
            boardTriggerTDCHistogram->SetBinContent(cBin, sum[tdcValue]);
        }
    }
}

void DQMHistogramBeamTestCheck::fillHitMaps(DetectorDataContainer& theHitMap, DetectorDataContainer& theStubMap, DetectorDataContainer& theTDCMap )
{
    LOG(INFO) << BOLDBLUE << "Filling Hit/Stub Maps..." << RESET;
    for(auto board: fStubMapS0Histograms)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // hit map
                TH2F* cHitMapS0 = fHitMapS0Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH2F* cHitMapS1 = fHitMapS1Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                LOG(INFO) << BOLDYELLOW << "Hit map [S0] has " << cHitMapS0->GetXaxis()->GetNbins() << " in X  and " << cHitMapS0->GetYaxis()->GetNbins() << " in Y." << RESET;
                LOG(INFO) << BOLDYELLOW << "Hit map [S1] has " << cHitMapS1->GetXaxis()->GetNbins() << " in X  and " << cHitMapS1->GetYaxis()->GetNbins() << " in Y." << RESET;
                float cLocalY = (hybrid->getId() % 2 == 0) ? 0 : 1;
                // stub map
                TH2F* cStubMapS0 = fStubMapS0Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH2F* cStubMapS1 = fStubMapS1Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                LOG(INFO) << BOLDYELLOW << "Stub map [S0] has " << cStubMapS0->GetXaxis()->GetNbins() << " in X  and " << cStubMapS0->GetYaxis()->GetNbins() << " in Y." << RESET;
                LOG(INFO) << BOLDYELLOW << "Stub map [S1] has " << cStubMapS1->GetXaxis()->GetNbins() << " in X  and " << cStubMapS1->GetYaxis()->GetNbins() << " in Y." << RESET;
                for(auto chip: *hybrid)
                {
                    uint16_t cDivider = (chip->size() == NCHANNELS) ? 2 : 1;
                    // uint16_t cOffset      = (hybrid->getId() % 2 == 0) ? (7 - chip->getId()%8) * chip->size() / cDivider : (chip->getId()%8) * chip->size() / cDivider;
                    uint16_t cChnlIndx    = 0;
                    auto&    cChipStubOCc = theStubMap.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());
                    for(auto channel: *cChipStubOCc->getChannelContainer<Occupancy>())
                    {
                        // this is only valid for MPA
                        if(chip->size() != NMPACHANNELS && chip->size() != NCHANNELS) continue;
                        // sensor id
                        uint8_t  cSensorId  = (chip->size() == NCHANNELS) ? (cChnlIndx % 2 != 0) : (chip->size() != NMPACHANNELS);
                        uint32_t cNChannels = (cSensorId == 0) ? cStubMapS0->GetXaxis()->GetNbins() / 8. : cStubMapS1->GetXaxis()->GetNbins() / 8.;
                        // rows and columns
                        uint32_t cNRows = (chip->size() == NCHANNELS) ? NCHANNELS/cDivider : NSSACHANNELS;
                        uint32_t cNCols = (chip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                        uint32_t cRow   = (chip->size() == NCHANNELS) ? cChnlIndx/cDivider : cChnlIndx % cNRows;
                        uint32_t cCol   = (cNCols == 1) ? 0 : cChnlIndx / cNRows;
                        // local x , local y
                        uint16_t cXOffset = (hybrid->getId() % 2 == 0) ? (7 - chip->getId() % 8) * cNChannels  : (chip->getId() % 8) * cNChannels ;
                        uint16_t cLocalX  = cXOffset + cRow ;
                        if(hybrid->getId() % 2 == 0) cLocalX = cXOffset + (cNChannels - cRow) ;
                        cLocalY = (hybrid->getId() % 2 == 0 ) ? 0 + cCol * 1. / cNCols : 1 + (cNCols - (1+cCol)) * 1. / cNCols;

                        auto cBin = (cSensorId == 0) ? cStubMapS0->FindBin((float)cLocalX, (float)cLocalY) : cStubMapS1->FindBin((float)cLocalX, (float)cLocalY);
                        if(channel.fOccupancy > 0)
                            LOG(DEBUG) << BOLDMAGENTA << "\t\t..ROC#" << +chip->getId() << " Hybrid#" << +hybrid->getId() << " Sensor" << +cSensorId 
                                    << " Channel " << +cChnlIndx << " Row " << +cRow << " Column " << +cCol
                                    << " Offset is " << cXOffset << " Local x coordinate " << +cLocalX << " Local y coordinate " << cLocalY << " bin# " << cBin << " Channel " << cChnlIndx
                                    << " offset is " << cXOffset << " - have found " << channel.fOccupancy << " hits " << RESET;

                        // uint16_t cStripOffset = cOffset;
                        // uint16_t cStripId     = cOffset + cChnlIndx / cDivider;
                        // // on the RHS hybrid IDs are 7,6,5,4,3,2,1,0 [from 0,0 if 0,0 is the connection between the SEH and the RHS]
                        // if(hybrid->getId() % 2 == 0) cStripId = cOffset + (chip->size() - cChnlIndx) / cDivider;
                        // auto    cBin      = cStubMapS0->FindBin((float)cStripId, (float)cLocalY);
                        // if(channel.fOccupancy > 0)
                        //     LOG(DEBUG) << BOLDBLUE << "\t\t..ROC#" << +chip->getId() << " Hybrid#" << +hybrid->getId() << " Sensor" << +cSensorId << " Local x coordinate " << +cStripId << " bin# "
                        //                << cBin << " Channel " << cChnlIndx << " offset is " << cStripOffset << " - have found " << channel.fOccupancy << " stubs " << RESET;

                        if(cSensorId == 0)
                        {
                            cStubMapS0->SetBinContent(cBin, channel.fOccupancy);
                            cStubMapS0->SetBinError(cBin, channel.fOccupancyError);
                        }
                        else
                        {
                            cStubMapS1->SetBinContent(cBin, channel.fOccupancy);
                            cStubMapS1->SetBinError(cBin, channel.fOccupancyError);
                        }
                        cChnlIndx++;
                    } // chanenls

                    cChnlIndx         = 0;
                    auto& cChipHitOCc = theHitMap.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());
                    for(auto channel: *cChipHitOCc->getChannelContainer<Occupancy>())
                    {
                        // sensor id
                        uint8_t cSensorId = (chip->size() == NCHANNELS) ? (cChnlIndx % 2 != 0) : (chip->size() != NMPACHANNELS);
                        // offset for plotting
                        uint32_t cNChannels = (cSensorId == 0) ? cHitMapS0->GetXaxis()->GetNbins() / 8. : cHitMapS1->GetXaxis()->GetNbins() / 8.;
                        // rows and columns
                        uint32_t cNRows = (chip->size() == NCHANNELS) ? NCHANNELS/cDivider : NSSACHANNELS;
                        uint32_t cNCols = (chip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                        uint32_t cRow   = (chip->size() == NCHANNELS) ? cChnlIndx/cDivider : cChnlIndx % cNRows;
                        uint32_t cCol   = (cNCols == 1) ? 0 : cChnlIndx / cNRows;
                        // local x , local y
                        uint16_t cXOffset = (hybrid->getId() % 2 == 0) ? (7 - chip->getId() % 8) * cNChannels  : (chip->getId() % 8) * cNChannels ;
                        uint16_t cLocalX  = cXOffset + cRow ;
                        if(hybrid->getId() % 2 == 0) cLocalX = cXOffset + (cNChannels - cRow) ;
                        if( cSensorId == 0 && chip->size() != NCHANNELS ) cLocalY = (hybrid->getId() % 2 == 0 ) ? 0 + cCol * 1. / cNCols : 1 + (cNCols - (1+cCol)) * 1. / cNCols;
                        else cLocalY = (hybrid->getId() % 2 == 0 ) ? 0 : 1 ;

                        // on the RHS hybrid IDs are 7,6,5,4,3,2,1,0 [from 0,0 if 0,0 is the connection between the SEH and the RHS]
                        auto cBinX = (cSensorId == 0) ? cHitMapS0->GetXaxis()->FindBin( cLocalX ) : cHitMapS1->GetXaxis()->FindBin( cLocalX );
                        auto cBin = (cSensorId == 0) ? cHitMapS0->FindBin((float)cLocalX, (float)cLocalY) : cHitMapS1->FindBin((float)cLocalX, (float)cLocalY);
                        if(channel.fOccupancy > 0 && hybrid->getId()%2 == 1 )
                            LOG(DEBUG) << BOLDMAGENTA << "\t\t..ROC#" << +chip->getId() << " Hybrid#" << +hybrid->getId() << " Sensor" << +cSensorId <<  " [" << cNChannels << " channels]"
                                    << " Row " << +cRow << " Column " << +cCol << " BinX " << cBinX 
                                    << " Offset is " << cXOffset << " Local x coordinate " << +cLocalX << " Local y coordinate " << cLocalY << " bin# " << cBin << " Channel " << cChnlIndx
                                    << " offset is " << cXOffset << " - have found " << channel.fOccupancy << " hits " << RESET;
                        if(cSensorId == 0)
                        {
                            cHitMapS0->SetBinContent(cBin, channel.fOccupancy);
                            cHitMapS0->SetBinError(cBin, channel.fOccupancyError);
                        }
                        else
                        {
                            cHitMapS1->SetBinContent(cBin, channel.fOccupancy);
                            cHitMapS1->SetBinError(cBin, channel.fOccupancyError);
                        }
                        cChnlIndx++;
                    } // chanenls

                    // TDC hit map 
                    TH2F* cLatencyTDC = fLatencyTDCHistograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    for(uint8_t cTDC = 0; cTDC < TDCBINS; cTDC++)
                    {
                        // when validating just use the mid-range value 
                        auto cBin = cLatencyTDC->FindBin(fStartLatency + fLatencyRange/2.0, (float)cTDC);
                        uint32_t cNhits =
                            theTDCMap.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDC];
                        LOG(DEBUG) << BOLDMAGENTA << "\t\t..TDC phase of " << +cTDC << " latency of [1] " << " bin of " << +cBin << " OG" << +opticalGroup->getId() << " Hybrid"
                                   << +hybrid->getId() << " Chip" << +chip->getId() << " - on average have found " << cNhits << " channels with a hit [per chip per event]." << RESET;
                        cLatencyTDC->SetBinContent(cBin, cNhits);
                        cLatencyTDC->SetBinError(cBin, std::sqrt((float)cNhits)); // for now
                    }
                    
                }     // ROCs
            }         // hybrids
        }             // OG
    }                 // board
}
void DQMHistogramBeamTestCheck::fillCorrelations(DetectorDataContainer& theHitMapS0, DetectorDataContainer& theHitMapS1, DetectorDataContainer& theStubMap)
{
    LOG(DEBUG) << BOLDBLUE << "Filling Correlation plots Hit/Stub Maps..." << RESET;

    for(auto board: fStubMapS0Histograms)
    {
        auto& cLinkCorrS0 = fCorrelationLinksS0Histogram.at(board->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // hit map
                TH2F* cS0S1 = fCorrelationS0S1Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                // stub map
                for(auto chip: *hybrid)
                {
                    uint16_t cDivider = (chip->size() == NCHANNELS) ? 2 : 1;
                    uint16_t cChnlIndx    = 0;
                    auto&    cChipS0Occ = theHitMapS0.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());
                    // hits 
                    int cNChannels = cS0S1->GetXaxis()->GetNbins() / 8.;
                    int cNRows = (chip->size() == NCHANNELS) ? NCHANNELS/cDivider : NSSACHANNELS;
                    int cNCols = (chip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                    for(auto channel: *cChipS0Occ->getChannelContainer<Occupancy>())
                    {
                        // this is only valid for MPAs/CBCs
                        if(chip->size() != NMPACHANNELS && chip->size() != NCHANNELS) continue;
                        // rows and columns
                        uint32_t cRow   = (chip->size() == NCHANNELS) ? cChnlIndx/cDivider : cChnlIndx % cNRows;
                        uint32_t cCol   = (cNCols == 1) ? 0 : cChnlIndx / cNRows;
                        // local x , local y
                        uint16_t cXOffset = (hybrid->getId() % 2 == 0) ? (7 - chip->getId() % 8) * cNChannels  : (chip->getId() % 8) * cNChannels ;
                        uint16_t cLocalX  = cXOffset + cRow ;
                        if(hybrid->getId() % 2 == 0) cLocalX = cXOffset + (cNChannels - cRow) ;
                        if( channel.fOccupancy > 0 )
                        {
                            for( auto cOtherChip : *hybrid)
                            {
                                // this is only valid for MPAs/CBCs
                                if(cOtherChip->size() != NMPACHANNELS && chip->size() != NCHANNELS) continue;
                                auto&    cChipS1Occ = theHitMapS1.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(cOtherChip->getIndex());
                                cNRows = (cOtherChip->size() == NCHANNELS) ? NCHANNELS : NSSACHANNELS;
                                cNCols = (cOtherChip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                                size_t cChnlIndxS1=0;
                                for(auto channelS1: *cChipS1Occ->getChannelContainer<Occupancy>())
                                {
                                    uint32_t cRowS1   = (cOtherChip->size() == NCHANNELS) ? cChnlIndxS1/cDivider : cChnlIndxS1 % cNRows;
                                    uint32_t cColS1   = 0;
                                    // local x , local y
                                    uint16_t cXOffsetS1 = (hybrid->getId() % 2 == 0) ? (7 - cOtherChip->getId() % 8) * cNChannels  : (cOtherChip->getId() % 8) * cNChannels ;
                                    uint16_t cLocalXS1  = cXOffsetS1 + cRowS1 ;
                                    if(hybrid->getId() % 2 == 0) cLocalXS1 = cXOffsetS1 + (cNChannels - cRowS1) ;
                                    if(channelS1.fOccupancy > 0  )
                                    {
                                        auto cBin = cS0S1->FindBin((float)cLocalX, (float)cLocalXS1);
                                        auto cBinContent = cS0S1->GetBinContent( cBin ); 
                                        cS0S1->SetBinContent( cBin, cBinContent + 1 ) ; 
                                        cS0S1->SetBinError( cBin, std::sqrt(cBinContent + 1 )) ; 
                                        if( cLocalX != cLocalXS1 ) 
                                        //if( chip->getId() != cOtherChip->getId()  )
                                            LOG(DEBUG) << BOLDYELLOW << " Correlation plot S0:S1 " 
                                                << " Hybrid#" << +hybrid->getId()
                                                << "\t\t..ROC#" << +chip->getId() 
                                                << "\t\t.. ROC#" << +cOtherChip->getId() 
                                                << " Row [S0] " << +cRow << " Column [S0] " << +cCol
                                                << " Row [S1] " << +cRowS1 << " Column [S1] " << +cColS1
                                                << RESET;
                                    }//print 
                                    cChnlIndxS1++;
                                }///all hit in other sensor for this chip 
                            }
                        }// hit in this channel 
                        cChnlIndx++;
                    } // chanenls
                    // now correlation for stubs 
                    TH2F* cStubS0 = fCorrelationStubS0Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    TH2F* cStubS1 = fCorrelationStubS1Histograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    auto&    cChipStubOcc = theStubMap.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());
                    cChnlIndx         = 0;
                    for(auto channel: *cChipStubOcc->getChannelContainer<Occupancy>())
                    {
                        // this is only valid for MPAs/CBCs
                        if(chip->size() != NMPACHANNELS && chip->size() != NCHANNELS) continue;
                        uint32_t cRow   = (chip->size() == NCHANNELS) ? cChnlIndx/cDivider : cChnlIndx % cNRows;
                        uint8_t  cSensorId = (chip->size() == NCHANNELS) ? ( (cChnlIndx%2 == 0 ) ?  0 : 1 ) : 0; 
                        // local x , local y
                        uint16_t cXOffset = (hybrid->getId() % 2 == 0) ? (7 - chip->getId() % 8) * cNChannels  : (chip->getId() % 8) * cNChannels ;
                        uint16_t cLocalX  = cXOffset + cRow ;
                        if(hybrid->getId() % 2 == 0) cLocalX = cXOffset + (cNRows - cRow) ;
                        if( channel.fOccupancy > 0 )
                        {
                            uint32_t cLinkOffset = opticalGroup->getIndex()*(2*cNChannels*8) + (hybrid->getId()%2)*(cNChannels*8) ;
                            LOG(DEBUG) << BOLDYELLOW << " Correlation plot Stubs:HitsS1 " 
                                                << " Link#" << +opticalGroup->getId()
                                                << " Hybrid#" << +hybrid->getId()
                                                << " ROC#" << +chip->getId() 
                                                << " Stub Row [S" << +cSensorId 
                                                << "] " 
                                                << +cRow 
                                                << " Local X " << cLocalX
                                                << " local offset on link is " << cLinkOffset 
                                                << " link position is this " << cLinkOffset + cLocalX 
                                                << " which is X-axis bin " << cLinkCorrS0->GetXaxis()->FindBin( cLinkOffset + cLocalX )
                                                << RESET;
                            // look for correlations in all other links 
                            for( auto otherOGs : *board )
                            {
                                for( auto otherHybrids : *otherOGs ) 
                                {
                                    for( auto otherChips : *otherHybrids ) 
                                    {
                                        auto&    cOtherStubOcc = theStubMap.at(board->getIndex())->at(otherOGs->getIndex())->at(otherHybrids->getIndex())->at(otherChips->getIndex());
                                        uint32_t cChnlIndxOthers         = 0;
                                        for(auto cOthers: *cOtherStubOcc->getChannelContainer<Occupancy>())
                                        {
                                            uint32_t cLinkOffsetOther = otherOGs->getIndex()*(2*cNChannels*8) + (otherHybrids->getId()%2)*(cNChannels*8) ;
                                            uint32_t cRowOther   = (otherChips->size() == NCHANNELS) ? cChnlIndxOthers/cDivider : cChnlIndxOthers % cNRows;
                                            uint16_t cXOffsetOther = (otherHybrids->getId() % 2 == 0) ? (7 - otherChips->getId() % 8) * cNChannels  : (otherChips->getId() % 8) * cNChannels ;
                                            uint16_t cLocalXOther  = cXOffsetOther + cRowOther ;
                                            if(otherHybrids->getId() % 2 == 0) cLocalXOther = cXOffsetOther + (cNRows - cRowOther) ;
                                            if( cOthers.fOccupancy > 0 ) 
                                            {
                                                auto cBin = cLinkCorrS0->FindBin((float)(cLinkOffset + cLocalX), (float)(cLinkOffsetOther + cLocalXOther));
                                                auto cBinContent = cLinkCorrS0->GetBinContent( cBin ); 
                                                cLinkCorrS0->SetBinContent( cBin, cBinContent + 1 ) ; 
                                                cLinkCorrS0->SetBinError( cBin, std::sqrt(cBinContent + 1 )) ; 
                                                
                                                LOG (DEBUG) << BOLDYELLOW << "\t\t\t.. Link#"  << +otherOGs->getId() 
                                                    << " Hybrid#" << +otherHybrids->getId()
                                                    << " ROC#" << +otherChips->getId() 
                                                    << " Row " << +cRowOther 
                                                    << " Local X " << cLocalX
                                                    << " local offset on link is " << cLinkOffsetOther 
                                                    << " link position is this " << cLinkOffsetOther + cLocalXOther 
                                                    << " which is X-axis bin " << cLinkCorrS0->GetXaxis()->FindBin( cLinkOffsetOther + cLocalXOther )
                                                    << RESET;
                                                }
                                            cChnlIndxOthers++;   
                                        } 
                                    }//chips 
                                }//Hybrds
                            }//OGs
                            // look for correlations with hits in sensor 0 
                            for( auto cOtherChip : *hybrid)
                            {
                                auto&    cChipS0Occ = theHitMapS0.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(cOtherChip->getIndex());
                                cNRows = (cOtherChip->size() == NCHANNELS) ? NCHANNELS/cDivider : NSSACHANNELS;
                                cNCols = (cOtherChip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                                size_t cChnlIndxS1=0;
                                for(auto channelS1: *cChipS0Occ->getChannelContainer<Occupancy>())
                                {
                                    uint8_t  cSensorIdOther = (cOtherChip->size() == NCHANNELS) ? ( (cChnlIndxS1%2 == 0 ) ?  0 : 1 ) : 0; 
                                    TH2F*    cHist  = (cSensorIdOther == 0 ) ? cStubS0 : cStubS1;
                                    uint32_t cRowS1   = (cOtherChip->size() == NCHANNELS) ? cChnlIndxS1/cDivider : cChnlIndxS1 % cNRows;
                                    //if( cSensorIdOther != 0 ) continue; 
                                    // local x , local y
                                    uint16_t cXOffsetS1 = (hybrid->getId() % 2 == 0) ? (7 - cOtherChip->getId() % 8) * cNRows  : (cOtherChip->getId() % 8) * cNRows ;
                                    uint16_t cLocalXS1  = cXOffsetS1 + cRowS1 ;
                                    if(hybrid->getId() % 2 == 0) cLocalXS1 = cXOffsetS1 + (cNRows - cRowS1) ;
                                    if(channelS1.fOccupancy > 0  )
                                    {
                                        auto cBin = cHist->FindBin((float)cLocalX, (float)cLocalXS1);
                                        auto cBinContent = cHist->GetBinContent( cBin ); 
                                        cHist->SetBinContent( cBin, cBinContent + 1 ) ; 
                                        cHist->SetBinError( cBin, std::sqrt(cBinContent + 1 )) ; 
                                        LOG(DEBUG) << BOLDMAGENTA << "\t\t\t\t.. ROC#" << +cOtherChip->getId() 
                                                << " Hit Row [S" << +cSensorIdOther 
                                                << "] " 
                                                << +cRowS1 
                                                << " channel "
                                                << cChnlIndxS1
                                                << " local x  " << cLocalXS1 
                                                << " bin X-axis " << cHist->GetXaxis()->FindBin(cLocalX)
                                                << " entries in histogram "
                                                << cHist->GetEntries() 
                                                << RESET;
                                    }//print 
                                    cChnlIndxS1++;
                                }///all hit in S0
                            }//other chips 
                            // look for correlations with hits in sensor 1
                            for( auto cOtherChip : *hybrid)
                            {
                                auto&    cChipS1Occ = theHitMapS1.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(cOtherChip->getIndex());
                                cNRows = (cOtherChip->size() == NCHANNELS) ? NCHANNELS/cDivider : NSSACHANNELS;
                                cNCols = (cOtherChip->size() == NMPACHANNELS) ? NMPACOLS : 1;
                                size_t cChnlIndxS1=0;
                                for(auto channelS1: *cChipS1Occ->getChannelContainer<Occupancy>())
                                {
                                    int  cSensorIdOther = (cOtherChip->size() == NCHANNELS) ? ( (cChnlIndxS1%2 == 0 ) ?  0 : 1 ) : 0; 
                                    TH2F*    cHist  = (cSensorIdOther == 0 ) ? cStubS0 : cStubS1;
                                    int cRowS1   = (cOtherChip->size() == NCHANNELS) ? cChnlIndxS1/cDivider : cChnlIndxS1 % cNRows;
                                    // local x , local y
                                    int cXOffsetS1 = (hybrid->getId() % 2 == 0) ? (7 - cOtherChip->getId() % 8) * cNRows  : (cOtherChip->getId() % 8) * cNRows ;
                                    int cLocalXS1  = cXOffsetS1 + cRowS1 ;
                                    if(hybrid->getId() % 2 == 0) cLocalXS1 = cXOffsetS1 + (cNRows - cRowS1) ;
                                    if(channelS1.fOccupancy > 0  )
                                    {
                                        auto cBin = cHist->FindBin((float)cLocalX, (float)cLocalXS1);
                                        LOG(DEBUG) << BOLDBLUE << "\t\t\t\t.. ROC#" << +cOtherChip->getId() 
                                                << " Hit Row [S" << +cSensorIdOther 
                                                << "] " 
                                                << +cRowS1 
                                                << " channel "
                                                << cChnlIndxS1
                                                << " local x offset is " << cXOffsetS1
                                                << " number of rows is " << cNRows
                                                << " local x  " << cLocalXS1 
                                                << " test " << (cNRows - cRowS1)
                                                << " # of entries is "
                                                << cHist->GetEntries() 
                                                << RESET;
                                        auto cBinContent = cHist->GetBinContent( cBin ); 
                                        cHist->SetBinContent( cBin, cBinContent + 1 ) ; 
                                        cHist->SetBinError( cBin, std::sqrt(cBinContent + 1 )) ; 
                                    }//print 
                                    cChnlIndxS1++;
                                }///all hit in S1
                            }//other chips 
                        }// hit in this channel 
                        cChnlIndx++;
                    } // channels

                }     // ROCs

            }         // hybrids
        }             // OG
    }                 // board
}
void DQMHistogramBeamTestCheck::fillBendPlots(DetectorDataContainer& theMap)
{
    LOG(INFO) << BOLDBLUE << "Filling Bend  histograms..." << RESET;
    for(auto board: theMap)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // hit map
                TH1F* cHist  = fBendHistrograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                auto  cBends = hybrid->getSummary<GenericDataArray<BENDBINS, uint16_t>>();
                for(uint32_t cIndx = 0; cIndx < BENDBINS; ++cIndx)
                {
                    float cBend = -7.0 + cIndx * 0.5;
                    auto  cBin  = cHist->GetXaxis()->FindBin(cBend);
                    cHist->SetBinContent(cBin, cBends[cIndx]);
                    cHist->SetBinError(cBin, std::sqrt(cBends[cIndx]));
                }

            } // hybrids
        }     // OG
    }         // board
}
// fill counts

void DQMHistogramBeamTestCheck::fillCountPlots(DetectorDataContainer& theEventCount, DetectorDataContainer& theStubCount)
{
    LOG(INFO) << BOLDBLUE << "Filling Stub count  histograms..." << RESET;
    for(auto board: theStubCount)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                // hit map
                auto& cStbsSmry = hybrid->getSummary<std::vector<std::vector<uint32_t>>>();
                auto& cEvntSmry = theEventCount.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
                
                TH2F* cHistStbs  = fStubCountHistrograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                TH2F* cHistEvnts  = fEventCountHistrograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                for(uint32_t cTDC = 0; cTDC < TDCBINS; ++cTDC)
                {
                    for(uint32_t cBnd = 0; cBnd < BENDBINS; ++cBnd)
                    {
                        float cBend = -7.5 + cBnd * 0.5;
                        auto  cBin  = cHistStbs->FindBin(cTDC, cBend);
                        cHistStbs->SetBinContent(cBin, cStbsSmry[cTDC][cBnd] );
                        cHistStbs->SetBinError(cBin, std::sqrt(cStbsSmry[cTDC][cBnd]));
                        // 
                        cBin  = cHistEvnts->FindBin(cTDC, cBend);
                        cHistEvnts->SetBinContent(cBin, cEvntSmry[cTDC][cBnd] );
                        cHistEvnts->SetBinError(cBin, std::sqrt(cEvntSmry[cTDC][cBnd]));
                    }
                }
            } // hybrids
        }     // OG
    }         // board

    // for(auto board: theEventCount)
    // {
    //     for(auto opticalGroup: *board)
    //     {
    //         for(auto hybrid: *opticalGroup)
    //         {
    //             // hit map
    //             auto& cEvntSmry = fEventSubSet.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->getSummary<std::vector<std::vector<uint32_t>>>();
    //             TH1F* cHist  = fEventCountHistrograms.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->getSummary<HistContainer<TH2F>>().fTheHistogram;
    //             auto  cBends = hybrid->getSummary<GenericDataArray<BENDBINS, uint16_t>>();
    //             for(uint32_t cIndx = 0; cIndx < BENDBINS; ++cIndx)
    //             {
    //                 float cBend = -7.5 + cIndx * 0.5;
    //                 auto  cBin  = cHist->GetXaxis()->FindBin(cBend);
    //                 cHist->SetBinContent(cBin, cBends[cIndx]);
    //                 cHist->SetBinError(cBin, std::sqrt(cBends[cIndx]));
    //             }
    //         } // hybrids
    //     }     // OG
    // }         // board


                

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
