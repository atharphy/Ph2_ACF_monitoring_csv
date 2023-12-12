/*!
        \file                DQMHistogramECV.cc
        \brief               class to create and fill data from ECV measurements
        \author              Stefan Maier
        \version             1.0
        \date                27/7/22
        Support :            mail to : s.maier@kit.edu
 */

#include "DQMUtils/DQMHistogramECV.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TLine.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/EmptyContainer.h"
#include "Utils/GenericDataArray.h"
#include "Utils/Occupancy.h"
#include "Utils/ThresholdAndNoise.h"
#include "Utils/Utilities.h"

//========================================================================================================================
DQMHistogramECV::DQMHistogramECV()
{
}

//========================================================================================================================
DQMHistogramECV::~DQMHistogramECV() {}

//========================================================================================================================
void DQMHistogramECV::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);

    HistContainer<TH2F> hBitErrorScanPolarity0("Hybrid_Clock_Polarity_0", "Polarity 0", 42, 0, 42, 75,0,75);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBitErrorScanPolarity0, hBitErrorScanPolarity0);
    HistContainer<TH2F> hBitErrorScanPolarity1("Hybrid_Clock_Polarity_1", "Polarity 1", 42, 0, 42, 75,0,75);
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fBitErrorScanPolarity1, hBitErrorScanPolarity1);

}

//========================================================================================================================
bool DQMHistogramECV::fill(std::string& inputStream) { return false; }

//========================================================================================================================
void DQMHistogramECV::process()
{
    for(auto board: fBitErrorScanPolarity0)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName   = "ECV_BitterrorRate_HybridClockParity0_B_" + std::to_string(board->getId()) + "_O_" + std::to_string(opticalGroup->getId()) + "_H_" + std::to_string(hybrid->getId());
                TCanvas*    berCanvas = new TCanvas(cCanvasName.data(), cCanvasName.data(), 500, 500);
                berCanvas->cd();
                TH2F* p0 = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                p0->GetXaxis()->SetTitle("Channel : Hybrid clock strength");
                p0->GetYaxis()->SetTitle("lpGBT Phase : CIC Strength");
                int binNumber = 0;
                for (uint32_t phase = 0; phase < 15; phase++)
                {
                    for (uint32_t cicSignalStrength = 1; cicSignalStrength <= 5; cicSignalStrength++)
                    {
                        std::string s = convertUInt32tToString(phase) + ":" + convertUInt32tToString(cicSignalStrength);
                        p0->GetYaxis()->SetBinLabel(75-binNumber, s.c_str() );
                        binNumber++;
                    }
                }
                binNumber = 1;

                for (uint32_t channel = 1; channel <=6; channel++)
                {
                    for (uint32_t hybridClockStrength = 1; hybridClockStrength <= 7; hybridClockStrength++)
                    {
                        std::string s = convertUInt32tToString(channel) + ":" + convertUInt32tToString(hybridClockStrength);
                        p0->GetXaxis()->SetBinLabel(binNumber, s.c_str() );
                        binNumber++;
                    }
                }
                p0->GetYaxis()->SetLabelSize(0.02);

                p0->LabelsOption("v","X");
                p0->DrawCopy("text");
                berCanvas->SetGrid();

            }
        }
    }
    for(auto board: fBitErrorScanPolarity1)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::string cCanvasName   = "ECV_BitterrorRate_HybridClockParity1_B_" + std::to_string(board->getId()) + "_O_" + std::to_string(opticalGroup->getId()) + "_H_" + std::to_string(hybrid->getId());
                TCanvas*    berCanvas = new TCanvas(cCanvasName.data(), cCanvasName.data(), 500, 500);
                berCanvas->cd();
                TH2F* p1 = hybrid->getSummary<HistContainer<TH2F>>().fTheHistogram;
                p1->GetXaxis()->SetTitle("Channel : Hybrid clock strength");
                p1->GetYaxis()->SetTitle("lpGBT Phase : CIC Strength");
                p1->GetYaxis()->SetLabelSize(0.02);

                berCanvas->SetGrid();
                int binNumber = 0;
                for (uint32_t phase = 0; phase < 15; phase++)
                {
                    for (uint32_t cicSignalStrength = 1; cicSignalStrength <= 5; cicSignalStrength++)
                    {
                        std::string s = convertUInt32tToString(phase) + ":" + convertUInt32tToString(cicSignalStrength);
                        p1->GetYaxis()->SetBinLabel(75-binNumber, s.c_str() );
                        binNumber++;
                    }
                }
                binNumber = 1;

                for (uint32_t channel = 1; channel <=6; channel++)
                {
                    for (uint32_t hybridClockStrength = 1; hybridClockStrength <= 7; hybridClockStrength++)
                    {
                        std::string s = convertUInt32tToString(channel) + ":" + convertUInt32tToString(hybridClockStrength);
                        p1->GetXaxis()->SetBinLabel(binNumber, s.c_str() );
                        binNumber++;
                    }
                }
                p1->LabelsOption("v","X");
                p1->DrawCopy("text");
                berCanvas->SetGrid();
                berCanvas->Update();

            }
        }
    }


}

//========================================================================================================================

void DQMHistogramECV::reset(void) {}

void DQMHistogramECV::filllpGBTCICPlot(uint8_t pClockPolarity, uint8_t pClockStrength, uint8_t pCicStrength, uint8_t pPhase, DetectorDataContainer& pBERSummary)
{
    for(auto board: pBERSummary)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                std::vector<float> bers = pBERSummary.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<std::vector<float>>();
                TH2F* cBERSummary;
                if (pClockPolarity == 0)
                    cBERSummary = fBitErrorScanPolarity0.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                else
                    cBERSummary = fBitErrorScanPolarity1.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH2F>>().fTheHistogram;
                
                int ch = 0;
                for (auto channelBer : bers)
                {
                    float value = 1 - channelBer;
                    if (+hybrid->getId() == 0)
                        cBERSummary->SetBinContent(ch*7+pClockStrength,76-pPhase*5-pCicStrength , value);
                    else
                        cBERSummary->SetBinContent((ch%6)*7+pClockStrength,76-pPhase*5-pCicStrength , value);
                    ch ++;
                }

            }
        }
    }
}