/*!
        \file                DQMHistogramPSBiasCal.h
        \brief               base class to create and fill monitoring histograms
        \author              Fabio Ravera, Lorenzo Uplegger
        \version             1.0
        \date                6/5/19
        Support :            mail to : fabio.ravera@cern.ch
 */

#include "DQMUtils/DQMHistogramPSBiasCal.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/HistContainer.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramPSBiasCal::DQMHistogramPSBiasCal() {}

//========================================================================================================================
DQMHistogramPSBiasCal::~DQMHistogramPSBiasCal() {}

//========================================================================================================================
void DQMHistogramPSBiasCal::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // copy detector structrure
    fDetectorContainer = &theDetectorStructure;

    // find front-end types
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
    }

    std::vector<FrontEndType> cStripTypes             = {FrontEndType::SSA, FrontEndType::SSA2};
    std::vector<FrontEndType> cPixelTypes             = {FrontEndType::MPA, FrontEndType::MPA2};
    auto                      selectStripChipFunction = [cStripTypes](const ChipContainer* pChip) {
        return (std::find(cStripTypes.begin(), cStripTypes.end(), static_cast<const ReadoutChip*>(pChip)->getFrontEndType()) != cStripTypes.end());
    };
    auto selectPixelChipFunction = [cPixelTypes](const ChipContainer* pChip) {
        return (std::find(cPixelTypes.begin(), cPixelTypes.end(), static_cast<const ReadoutChip*>(pChip)->getFrontEndType()) != cPixelTypes.end());
    };

    // find maximum number of channels
    // std::vector<size_t> cNPixelChannels(0), cNStripChannels(0);
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             for(auto cChip: *cHybrid)
    //             {
    //                 auto cNChannels = theDetectorStructure.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->size();
    //                 auto cType      = cChip->getFrontEndType();
    //                 if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2) { cNStripChannels.push_back(cNChannels); }
    //                 else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
    //                 {
    //                     cNPixelChannels.push_back(cNChannels);
    //                 }
    //             }
    //         }
    //     }
    // }

    // auto cSetting = pSettingsMap.find("PlotSCurves");
    // fPlotSCurves  = (cSetting != std::end(pSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    // cSetting      = pSettingsMap.find("FitSCurves");
    // fFitSCurves   = (cSetting != std::end(pSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    // if(fFitSCurves) fPlotSCurves = true;

    std::string queryFunctionName = "ChipType";
    if(fWithSSA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectStripChipFunction, queryFunctionName);

            //uint16_t nYbins = (fWithSSA) ? 255 : 1024;
            // float    minY   = -0.5;
            //float    maxY   = (fWithSSA) ? 254.5 : 1023.5;

        HistContainer<TH1F> theTH1FChipStripVref("VREFdac", "VREFdac", 32, 0, 32);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipStripVrefHistograms, theTH1FChipStripVref);

        fDetectorContainer->removeReadoutChipQueryFunction(queryFunctionName);
    }
    

    if(fWithMPA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectPixelChipFunction, queryFunctionName);


        HistContainer<TH1F> theTH1FChipPixelVref("VREFdac", "VREFdac", 32, 0, 32);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipPixelVrefHistograms, theTH1FChipPixelVref);

        
        // Reset query function from only including strip chips in the data container
        fDetectorContainer->removeReadoutChipQueryFunction(queryFunctionName);
    }

}

//========================================================================================================================
bool DQMHistogramPSBiasCal::fill(std::string& inputStream)
{
    ContainerSerialization theDACSerialization("PSBiasCalVrefDac");
    if(theDACSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Vref DAC!!!!!\n";
        DetectorDataContainer theVREFDACData  = theDACSerialization.deserializeBoardContainer<std::pair<uint32_t, float>, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);
    
        fillDACPlots(theVREFDACData);
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramPSBiasCal::process()
{
    fitSlopes();

    // for(auto cBoard: *fDetectorContainer)
    // {
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             // std::string validationCanvasName = "Validation_B_" + std::to_string(cBoard->getId()) + "_O_" + std::to_string(cOpticalGroup->getId()) + "_H_" + std::to_string(cHybrid->getId());
    //             // TCanvas* cValidation = new TCanvas(validationCanvasName.data(), validationCanvasName.data(), 0, 0, 650, fPlotSCurves ? 900 : 650);

    //             // cValidation->Divide(cHybrid->size(), fPlotSCurves ? 3 : 2);

    //             for(auto cChip: *cHybrid)
    //             {
    //                 // auto cType = cChip->getFrontEndType();
    //                 // if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
    //                 // {
    //                 //     cValidation->cd(cChip->getIndex() + 1 + cHybrid->size() * 0);
    //                 //     TH1F* validationHistogram = fDetectorStripValidationHistograms.getObject(cBoard->getId())
    //                 //                                     ->getObject(cOpticalGroup->getId())
    //                 //                                     ->getObject(cHybrid->getId())
    //                 //                                     ->getObject(cChip->getId())
    //                 //                                     ->getSummary<HistContainer<TH1F>>()
    //                 //                                     .fTheHistogram;
    //                 //     validationHistogram->SetStats(false);
    //                 //     validationHistogram->DrawCopy();
    //                 //     gPad->SetLogy();

    //                 // }
    //                 // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
    //                 // {
    //                 //     cValidation->cd(cChip->getIndex() + 1 + cHybrid->size() * 0);
    //                 //     TH1F* validationHistogram = fDetectorPixelValidationHistograms.getObject(cBoard->getId())
    //                 //                                     ->getObject(cOpticalGroup->getId())
    //                 //                                     ->getObject(cHybrid->getId())
    //                 //                                     ->getObject(cChip->getId())
    //                 //                                     ->getSummary<HistContainer<TH1F>>()
    //                 //                                     .fTheHistogram;
    //                 //     validationHistogram->SetStats(false);
    //                 //     validationHistogram->DrawCopy();
    //                 //     gPad->SetLogy();
    //                 // }


    //                 // if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
    //                 // {
    //                 //     TH1F* cChipStripDACHist = fDetectorChipStripDACHistograms.getObject(cBoard->getId())
    //                 //                                      ->getObject(cOpticalGroup->getId())
    //                 //                                      ->getObject(cHybrid->getId())
    //                 //                                      ->getObject(cChip->getId())
    //                 //                                      ->getSummary<HistContainer<TH2F>>()
    //                 //                                      .fTheHistogram;
    //                 //     TH1D* cTmp = cChipStripSCurveHist->ProjectionY();
    //                 //     cChipStripSCurveHist->GetYaxis()->SetRangeUser(cTmp->GetBinCenter(cTmp->FindFirstBinAbove(0)) - 10, cTmp->GetBinCenter(cTmp->FindLastBinAbove(0.99)) + 10);
    //                 //     // cSCurveHist->GetZaxis()->SetRangeUser(0,1.);
    //                 //     delete cTmp;
    //                 //     cValidation->cd(cChip->getIndex() + 1 + cHybrid->size() * 2);
    //                 //     cChipStripSCurveHist->SetStats(false);
    //                 //     cChipStripSCurveHist->DrawCopy("colz");
    //                 //     fDetectorChannelStripNoiseHistograms.getObject(cBoard->getId())
    //                 //         ->getObject(cOpticalGroup->getId())
    //                 //         ->getObject(cHybrid->getId())
    //                 //         ->getObject(cChip->getId())
    //                 //         ->getSummary<HistContainer<TH1F>>()
    //                 //         .fTheHistogram->GetYaxis()
    //                 //         ->SetRangeUser(0., 20.);
    //                 // }
    //                 // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
    //                 // {
    //                 //     TH2F* cChipPixelSCurveHist = fDetectorChipPixelSCurveHistograms.getObject(cBoard->getId())
    //                 //                                      ->getObject(cOpticalGroup->getId())
    //                 //                                      ->getObject(cHybrid->getId())
    //                 //                                      ->getObject(cChip->getId())
    //                 //                                      ->getSummary<HistContainer<TH2F>>()
    //                 //                                      .fTheHistogram;
    //                 //     TH1D* cTmp = cChipPixelSCurveHist->ProjectionY();
    //                 //     cChipPixelSCurveHist->GetYaxis()->SetRangeUser(cTmp->GetBinCenter(cTmp->FindFirstBinAbove(0)) - 10, cTmp->GetBinCenter(cTmp->FindLastBinAbove(0.99)) + 10);
    //                 //     // cSCurveHist->GetZaxis()->SetRangeUser(0,1.);
    //                 //     delete cTmp;
    //                 //     cValidation->cd(cChip->getIndex() + 1 + cHybrid->size() * 2);
    //                 //     cChipPixelSCurveHist->SetStats(false);
    //                 //     cChipPixelSCurveHist->DrawCopy("colz");
    //                 //     fDetectorChannelPixelNoiseHistograms.getObject(cBoard->getId())
    //                 //         ->getObject(cOpticalGroup->getId())
    //                 //         ->getObject(cHybrid->getId())
    //                 //         ->getObject(cChip->getId())
    //                 //         ->getSummary<HistContainer<TH1F>>()
    //                 //         .fTheHistogram->GetYaxis()
    //                 //         ->SetRangeUser(0., 20.);
    //                 // }
                    
    //             }

    //             if(fWithCBC || fWithSSA)
    //             {
    //                 fDetectorHybridStripNoiseHistograms.getObject(cBoard->getId())
    //                     ->getObject(cOpticalGroup->getId())
    //                     ->getObject(cHybrid->getId())
    //                     ->getSummary<HistContainer<TH1F>>()
    //                     .fTheHistogram->GetXaxis()
    //                     ->SetRangeUser(-0.5, fNStripChannels * 8 - 0.5);
    //                 fDetectorHybridStripNoiseHistograms.getObject(cBoard->getId())
    //                     ->getObject(cOpticalGroup->getId())
    //                     ->getObject(cHybrid->getId())
    //                     ->getSummary<HistContainer<TH1F>>()
    //                     .fTheHistogram->GetYaxis()
    //                     ->SetRangeUser(0., 20.);

                    
    //             }
    //             if(fWithMPA)
    //             {
    //                 fDetectorHybridPixelNoiseHistograms.getObject(cBoard->getId())
    //                     ->getObject(cOpticalGroup->getId())
    //                     ->getObject(cHybrid->getId())
    //                     ->getSummary<HistContainer<TH1F>>()
    //                     .fTheHistogram->GetXaxis()
    //                     ->SetRangeUser(-0.5, fNPixelChannels * 8 - 0.5);
    //                 fDetectorHybridPixelNoiseHistograms.getObject(cBoard->getId())
    //                     ->getObject(cOpticalGroup->getId())
    //                     ->getObject(cHybrid->getId())
    //                     ->getSummary<HistContainer<TH1F>>()
    //                     .fTheHistogram->GetYaxis()
    //                     ->SetRangeUser(0., 20.);
    //             }
    //         }
    //     }
    // }
}

//========================================================================================================================
void DQMHistogramPSBiasCal::reset(void) {}

//========================================================================================================================
void DQMHistogramPSBiasCal::fillDACPlots(DetectorDataContainer& theDAC)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto     cType                  = cChip->getFrontEndType();
                    // uint32_t cNChannels             = 0;
                    TH1F * fStripVrefHistograms = nullptr;
                    TH1F * fPixelVrefHistograms = nullptr;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                    {
            
                        fStripVrefHistograms = fChipStripVrefHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;
                        std::cout << " Fill SSA "<<std::endl;
                        std::cout << " DAC, VREF "<< theDAC.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<std::pair<uint32_t, float>>().first << " " << theDAC.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<std::pair<uint32_t, float>>().second << std::endl;
                        fStripVrefHistograms->Fill(theDAC.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<std::pair<uint32_t, float>>().first,theDAC.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<std::pair<uint32_t, float>>().second);

                    }
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                    {
                        fPixelVrefHistograms = fChipPixelVrefHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;
                        
                        std::cout << " Fill MPA "<<std::endl;
                        fPixelVrefHistograms->Fill(theDAC.getObject(cBoard->getId())
                                 ->getObject(cOpticalGroup->getId())
                                 ->getObject(cHybrid->getId())
                                 ->getObject(cChip->getId())
                                 ->getSummary<std::pair<uint32_t, float>>().first,theDAC.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<std::pair<uint32_t, float>>().second);
                    }



                    // auto cChannelContainer = thePedestalAndNoise.getObject(cBoard->getId())
                    //                              ->getObject(cOpticalGroup->getId())
                    //                              ->getObject(cHybrid->getId())
                    //                              ->getObject(cChip->getId())
                    //                              ->getChipContainer<ThresholdAndNoise>();
                    // if(cChannelContainer == nullptr) continue;
                    // uint16_t cChannelNumber = 0;
                    // for(auto cChannel: *cChannelContainer)
                    // {
                    //     float cNoise       = (std::isnan(cChannel.fNoise)) ? 666 : cChannel.fNoise;
                    //     float cNoiseErr    = (std::isnan(cChannel.fNoiseError)) ? 666 : cChannel.fNoiseError;
                    //     float cPedestal    = (std::isnan(cChannel.fThreshold)) ? 666 : cChannel.fThreshold;
                    //     float cPedestalErr = (std::isnan(cChannel.fThreshold)) ? 666 : cChannel.fThresholdError;
                    //     cChipPedestalHistogram->Fill(cPedestal);
                    //     cChipNoiseHistogram->Fill(cNoise);
                    //     cHybridNoiseHistogram->Fill(cNoise);

                    //     cChannelNoiseHistogram->SetBinContent(cChannelNumber + 1, cNoise);
                    //     cChannelNoiseHistogram->SetBinError(cChannelNumber + 1, cNoiseErr);
                    //     cChannelPedestalHistogram->SetBinContent(cChannelNumber + 1, cPedestal);
                    //     cChannelPedestalHistogram->SetBinError(cChannelNumber + 1, cPedestalErr);
                    //     cHybridChannelNoiseHistogram->SetBinContent(cNChannels * (cChip->getId() % 8) + cChannelNumber + 1, cNoise);
                    //     cHybridChannelNoiseHistogram->SetBinError(cNChannels * (cChip->getId() % 8) + cChannelNumber + 1, cNoiseErr);

                        
                    //     if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                    //     { cChannel2DPixelNoiseHistogram->SetBinContent(int(cChannelNumber % 120) + 1, int(cChannelNumber / 120) + 1, cNoise); }
                    //     ++cChannelNumber;
                    // }
                }
            }
        }
    }
}



//========================================================================================================================
void DQMHistogramPSBiasCal::fitSlopes()
{
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             for(auto cChip: *cHybrid)
    //             {
    //                 // // ChipDataContainer* theChipThresholdAndNoise =
    //                 //     // fThresholdAndNoiseContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

    //                 // // auto     cType      = cChip->getFrontEndType();
    //                 // // uint32_t cNChannels = 0;
    //                 // // if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2) { cNChannels = fNStripChannels; }
    //                 // // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
    //                 // // {
    //                 //     // cNChannels = fNPixelChannels;
    //                 // // }

    //                 // // for(uint32_t cChannel = 0; cChannel < cNChannels; cChannel++)
    //                 // // {
    //                 //     // TH1F *cChannelSCurve = nullptr, *cChannelNoiseHistogram = nullptr, *cChannelPedestalHistogram = nullptr;
    //                 //     // if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
    //                 //     // {
    //                 //         // cChannelSCurve = fDetectorChannelStripSCurveHistograms.getObject(cBoard->getId())
    //                 //                             //  ->getObject(cOpticalGroup->getId())
    //                 //                             //  ->getObject(cHybrid->getId())
    //                 //                             //  ->getObject(cChip->getId())
    //                 //                             //  ->getChannel<HistContainer<TH1F>>(cChannel)
    //                 //                             //  .fTheHistogram;

    //                 //         // cChannelNoiseHistogram = fDetectorChannelStripNoiseHistograms.getObject(cBoard->getId())
    //                 //                                     //  ->getObject(cOpticalGroup->getId())
    //                 //                                     //  ->getObject(cHybrid->getId())
    //                 //                                     //  ->getObject(cChip->getId())
    //                 //                                     //  ->getSummary<HistContainer<TH1F>>()
    //                 //                                     //  .fTheHistogram;

    //                 //         // cChannelPedestalHistogram = fDetectorChannelStripPedestalHistograms.getObject(cBoard->getId())
    //                 //                                         // ->getObject(cOpticalGroup->getId())
    //                 //                                         // ->getObject(cHybrid->getId())
    //                 //                                         // ->getObject(cChip->getId())
    //                 //                                         // ->getSummary<HistContainer<TH1F>>()
    //                 //                                         // .fTheHistogram;
    //                 //     // }
    //                 //     // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
    //                 //     // {
    //                 //         // cChannelSCurve = fDetectorChannelPixelSCurveHistograms.getObject(cBoard->getId())
    //                 //                             //  ->getObject(cOpticalGroup->getId())
    //                 //                             //  ->getObject(cHybrid->getId())
    //                 //                             //  ->getObject(cChip->getId())
    //                 //                             //  ->getChannel<HistContainer<TH1F>>(cChannel)
    //                 //                             //  .fTheHistogram;

    //                 //         // cChannelNoiseHistogram = fDetectorChannelPixelNoiseHistograms.getObject(cBoard->getId())
    //                 //                                     //  ->getObject(cOpticalGroup->getId())
    //                 //                                     //  ->getObject(cHybrid->getId())
    //                 //                                     //  ->getObject(cChip->getId())
    //                 //                                     //  ->getSummary<HistContainer<TH1F>>()
    //                 //                                     //  .fTheHistogram;

    //                 //         // cChannelPedestalHistogram = fDetectorChannelPixelPedestalHistograms.getObject(cBoard->getId())
    //                 //                                         // ->getObject(cOpticalGroup->getId())
    //                 //                                         // ->getObject(cHybrid->getId())
    //                 //                                         // ->getObject(cChip->getId())
    //                 //                                         // ->getSummary<HistContainer<TH1F>>()
    //                 //                                         // .fTheHistogram;
    //                 //     // }

    //                 //     // float cChannelNoise = cChannelNoiseHistogram->GetBinContent(cChannel + 1);

    //                 //     // float cChannelPedestal = cChannelPedestalHistogram->GetBinContent(cChannel + 1);

    //                 //     // TF1* cFit = new TF1("SCurveFit", MyErf, cChannelPedestal - (cChannelNoise * 5), cChannelPedestal + (cChannelNoise * 5), 2);

    //                 //     // cFit->SetParameter(0, cChannelPedestal);
    //                 //     // cFit->SetParameter(1, cChannelNoise);

    //                 //     Fit
    //                 //     // cChannelSCurve->Fit(cFit, "RQ+0");

    //                 //     // theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fThreshold      = cFit->GetParameter(0);
    //                 //     // theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fNoise          = cFit->GetParameter(1);
    //                 //     // theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fThresholdError = cFit->GetParError(0);
    //                 //     // theChipThresholdAndNoise->getChannel<ThresholdAndNoise>(cChannel).fNoiseError     = cFit->GetParError(1);
    //                 // // }
    //             }
    //         }
    //     }
    // }

    //fillPedestalAndNoisePlots(fThresholdAndNoiseContainer);
}
