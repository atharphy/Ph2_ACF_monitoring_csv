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
#include "RootUtils/GraphContainer.h"
#include "RootUtils/HistContainer.h"
#include "RootUtils/RootContainerFactory.h"
#include "TCanvas.h"
#include "TF1.h"
#include "TFile.h"
#include "TGraph.h"
#include "TH1F.h"
#include "TH2F.h"
#include "Utils/ADCSlope.h"
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

    std::string queryFunctionName = "ChipType";
    if(fWithSSA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectStripChipFunction, queryFunctionName);

        HistContainer<TH1F> theTH1FChipStripVref("VREFdac", "VREFdac", 32, 0, 32);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipStripVrefHistograms, theTH1FChipStripVref);

        GraphContainer<TGraph> theTGraphChipStripSlope(fGraphSize);
        theTGraphChipStripSlope.setNameTitle("ADC_slope", "ADC_slope");
        RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fChipStripSlopeGraphs, theTGraphChipStripSlope);

        HistContainer<TH1F> theTH1FChipStripAVDD("AVDD", "AVDD", 4096, 0, 4096);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipStripAVDDHistograms, theTH1FChipStripAVDD);
        HistContainer<TH1F> theTH1FChipStripDVDD("DVDD", "DVDD", 4096, 0, 4096);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipStripDVDDHistograms, theTH1FChipStripDVDD);

        fDetectorContainer->removeReadoutChipQueryFunction(queryFunctionName);
    }

    if(fWithMPA)
    {
        // Set query function to only include strip chips in the data container
        fDetectorContainer->addReadoutChipQueryFunction(selectPixelChipFunction, queryFunctionName);

        HistContainer<TH1F> theTH1FChipPixelVref("VREFdac", "VREFdac", 32, 0, 32);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipPixelVrefHistograms, theTH1FChipPixelVref);

        GraphContainer<TGraph> theTGraphChipPixelSlope(fGraphSize);
        theTGraphChipPixelSlope.setNameTitle("ADC_slope", "ADC_slope");
        RootContainerFactory::bookChipHistograms<GraphContainer<TGraph>>(theOutputFile, theDetectorStructure, fChipPixelSlopeGraphs, theTGraphChipPixelSlope);

        HistContainer<TH1F> theTH1FChipPixelAVDD("AVDD", "AVDD", 4096, 0, 4096);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipPixelAVDDHistograms, theTH1FChipPixelAVDD);
        HistContainer<TH1F> theTH1FChipPixelDVDD("DVDD", "DVDD", 4096, 0, 4096);
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fChipPixelDVDDHistograms, theTH1FChipPixelDVDD);

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
        LOG(DEBUG) << BOLDMAGENTA << "Matched Vref DAC!!!!!" << RESET;
        DetectorDataContainer theVREFDACData =
            theDACSerialization.deserializeBoardContainer<std::pair<uint32_t, float>, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);

        fillDACPlots(theVREFDACData);
        return true;
    }

    ContainerSerialization theADCSlopeSerialization("PSBiasCalADCSlope");
    if(theADCSlopeSerialization.attachDeserializer(inputStream))
    {
        LOG(DEBUG) << BOLDMAGENTA << "Matched ADC slope!!!!!" << RESET;
        DetectorDataContainer theADCSlopeData = theADCSlopeSerialization.deserializeBoardContainer<ADCSlope, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);

        fillSlopePlots(theADCSlopeData);
        return true;
    }

    ContainerSerialization theAVDDSerialization("PSBiasCalAVDD");
    if(theAVDDSerialization.attachDeserializer(inputStream))
    {
        LOG(DEBUG) << BOLDMAGENTA << "Matched AVDD!!!!!" << RESET;
        DetectorDataContainer theAVDDData = theAVDDSerialization.deserializeBoardContainer<std::pair<uint32_t, float>, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);

        fillVDDPlots(theAVDDData, true);
        return true;
    }
    ContainerSerialization theDVDDSerialization("PSBiasCalDVDD");
    if(theDVDDSerialization.attachDeserializer(inputStream))
    {
        LOG(DEBUG) << BOLDMAGENTA << "Matched DVDD!!!!!" << RESET;
        DetectorDataContainer theDVDDData = theDVDDSerialization.deserializeBoardContainer<std::pair<uint32_t, float>, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);

        fillVDDPlots(theDVDDData, false);
        return true;
    }

    return false;
}

//========================================================================================================================
void DQMHistogramPSBiasCal::process() {}

//========================================================================================================================
void DQMHistogramPSBiasCal::reset(void) {}

//========================================================================================================================
void DQMHistogramPSBiasCal::fillDACPlots(DetectorDataContainer& theDAC)
{
    LOG(DEBUG) << __PRETTY_FUNCTION__ << " Fill DAC Plots " << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto  cType                = cChip->getFrontEndType();
                    TH1F* fStripVrefHistograms = nullptr;
                    TH1F* fPixelVrefHistograms = nullptr;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                    {
                        fStripVrefHistograms = fChipStripVrefHistograms.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<HistContainer<TH1F>>()
                                                   .fTheHistogram;
                        LOG(DEBUG) << BOLDBLUE << " Fill SSA " << RESET;
                        LOG(DEBUG) << BOLDBLUE << " DAC, VREF "
                                   << +theDAC.getObject(cBoard->getId())
                                           ->getObject(cOpticalGroup->getId())
                                           ->getObject(cHybrid->getId())
                                           ->getObject(cChip->getId())
                                           ->getSummary<std::pair<uint8_t, float>>()
                                           .first
                                   << " "
                                   << theDAC.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint8_t, float>>()
                                          .second
                                   << RESET;
                        fStripVrefHistograms->Fill(
                            theDAC.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<std::pair<uint8_t, float>>().first,
                            theDAC.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<std::pair<uint8_t, float>>()
                                .second);
                        fStripVrefHistograms->GetXaxis()->SetTitle("ADC_VREF register");
                        fStripVrefHistograms->GetYaxis()->SetTitle("VREF [V]");
                    }
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                    {
                        fPixelVrefHistograms = fChipPixelVrefHistograms.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<HistContainer<TH1F>>()
                                                   .fTheHistogram;

                        LOG(DEBUG) << BOLDBLUE << " Fill MPA " << RESET;
                        LOG(DEBUG) << BOLDBLUE << " DAC, VREF "
                                   << +theDAC.getObject(cBoard->getId())
                                           ->getObject(cOpticalGroup->getId())
                                           ->getObject(cHybrid->getId())
                                           ->getObject(cChip->getId())
                                           ->getSummary<std::pair<uint8_t, float>>()
                                           .first
                                   << " "
                                   << theDAC.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint8_t, float>>()
                                          .second
                                   << RESET;

                        fPixelVrefHistograms->Fill(
                            theDAC.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<std::pair<uint8_t, float>>().first,
                            theDAC.getObject(cBoard->getId())
                                ->getObject(cOpticalGroup->getId())
                                ->getObject(cHybrid->getId())
                                ->getObject(cChip->getId())
                                ->getSummary<std::pair<uint8_t, float>>()
                                .second);
                        fPixelVrefHistograms->GetXaxis()->SetTitle("ADC_VREF register");
                        fPixelVrefHistograms->GetYaxis()->SetTitle("VREF [V]");
                    } // chip type
                }     // chip
            }         // hybrid
        }             // optical group
    }                 // board
}

//========================================================================================================================
void DQMHistogramPSBiasCal::fillVDDPlots(DetectorDataContainer& theVDD, bool isAVDD)
{
    LOG(DEBUG) << __PRETTY_FUNCTION__ << " Fill VDD Plots " << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto  cType               = cChip->getFrontEndType();
                    TH1F* fStripVDDHistograms = nullptr;
                    TH1F* fPixelVDDHistograms = nullptr;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                    {
                        if(isAVDD)
                        {
                            LOG(DEBUG) << MAGENTA << " AVDD " << RESET;
                            fStripVDDHistograms = fChipStripAVDDHistograms.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<HistContainer<TH1F>>()
                                                      .fTheHistogram;
                        }
                        else
                        {
                            LOG(DEBUG) << MAGENTA << " DVDD " << RESET;
                            fStripVDDHistograms = fChipStripDVDDHistograms.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<HistContainer<TH1F>>()
                                                      .fTheHistogram;
                        }

                        LOG(DEBUG) << BOLDBLUE << " Fill SSA " << RESET;
                        LOG(DEBUG) << BOLDBLUE << " ADC, volts "
                                   << theVDD.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint32_t, float>>()
                                          .first
                                   << " "
                                   << theVDD.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint32_t, float>>()
                                          .second
                                   << RESET;
                        fStripVDDHistograms->Fill(theVDD.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<std::pair<uint32_t, float>>()
                                                      .first,
                                                  theVDD.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<std::pair<uint32_t, float>>()
                                                      .second);

                        fStripVDDHistograms->GetXaxis()->SetTitle("VDD [ADC]");
                        fStripVDDHistograms->GetYaxis()->SetTitle("VDD [V]");
                        fStripVDDHistograms->SetMarkerStyle(20);
                    }
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                    {
                        if(isAVDD)
                        {
                            LOG(DEBUG) << MAGENTA << " AVDD " << RESET;
                            fPixelVDDHistograms = fChipPixelAVDDHistograms.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<HistContainer<TH1F>>()
                                                      .fTheHistogram;
                        }
                        else
                        {
                            LOG(DEBUG) << MAGENTA << " DVDD " << RESET;
                            fPixelVDDHistograms = fChipPixelDVDDHistograms.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<HistContainer<TH1F>>()
                                                      .fTheHistogram;
                        }

                        LOG(DEBUG) << BOLDBLUE << " Fill MPA " << RESET;
                        LOG(DEBUG) << BOLDBLUE << " ADC, volts "
                                   << theVDD.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint32_t, float>>()
                                          .first
                                   << " "
                                   << theVDD.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<std::pair<uint32_t, float>>()
                                          .second
                                   << RESET;

                        fPixelVDDHistograms->Fill(theVDD.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<std::pair<uint32_t, float>>()
                                                      .first,
                                                  theVDD.getObject(cBoard->getId())
                                                      ->getObject(cOpticalGroup->getId())
                                                      ->getObject(cHybrid->getId())
                                                      ->getObject(cChip->getId())
                                                      ->getSummary<std::pair<uint32_t, float>>()
                                                      .second);
                        fPixelVDDHistograms->GetXaxis()->SetTitle("VDD [ADC]");
                        fPixelVDDHistograms->GetYaxis()->SetTitle("VDD [V]");
                        fPixelVDDHistograms->SetMarkerStyle(20);
                    } // chip type
                }     // chip
            }         // hybrid
        }             // optical group
    }                 // board
}

//========================================================================================================================
void DQMHistogramPSBiasCal::fillSlopePlots(DetectorDataContainer& theSlope)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto    cType                = cChip->getFrontEndType();
                    TGraph* fStripADCSlopeGraphs = nullptr;
                    TGraph* fPixelADCSlopeGraphs = nullptr;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                    {
                        fStripADCSlopeGraphs = fChipStripSlopeGraphs.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<GraphContainer<TGraph>>()
                                                   .fTheGraph;

                        auto cChipContainer = theSlope.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<ADCSlope>();

                        LOG(DEBUG) << BLUE << " Fill SSA " << RESET;
                        LOG(DEBUG) << BLUE << " GND ADC " << cChipContainer.fADC_GND << RESET;
                        LOG(DEBUG) << BLUE << " VBG ADC " << cChipContainer.fADC_VBG << " measured V " << cChipContainer.fMeasured_VBG << RESET;
                        float ADCs[fGraphSize]     = {cChipContainer.fADC_GND, cChipContainer.fADC_VBG};
                        float voltages[fGraphSize] = {0, cChipContainer.fMeasured_VBG};
                        for(int i = 0; i < fGraphSize; i++)
                        {
                            fStripADCSlopeGraphs->SetPointX(i, ADCs[i]);
                            fStripADCSlopeGraphs->SetPointY(i, voltages[i]);
                        }
                        fStripADCSlopeGraphs->SetMarkerStyle(20);
                        fStripADCSlopeGraphs->GetXaxis()->SetTitle("ADC output [ADC]");
                        fStripADCSlopeGraphs->GetYaxis()->SetTitle("ADC output [V]");
                        fStripADCSlopeGraphs->Fit("pol1", "Q");
                        LOG(DEBUG) << BLUE << " cChipContainer.fOffset " << cChipContainer.fOffset << " cChipContainer.fSlope " << cChipContainer.fSlope << RESET;
                        TF1* fPol1 = fStripADCSlopeGraphs->GetFunction("pol1");
                        fPol1->SetParameter(0, cChipContainer.fOffset);
                        fPol1->SetParameter(1, cChipContainer.fSlope);
                        fPol1->SetRange(0, 4095);
                        fPol1->SetLineColor(kRed + 1);
                        fPol1->SetLineStyle(2);
                    }
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                    {
                        fPixelADCSlopeGraphs = fChipPixelSlopeGraphs.getObject(cBoard->getId())
                                                   ->getObject(cOpticalGroup->getId())
                                                   ->getObject(cHybrid->getId())
                                                   ->getObject(cChip->getId())
                                                   ->getSummary<GraphContainer<TGraph>>()
                                                   .fTheGraph;

                        auto cChipContainer = theSlope.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<ADCSlope>();

                        LOG(DEBUG) << BLUE << " Fill MPA " << RESET;
                        LOG(DEBUG) << BLUE << " GND ADC " << cChipContainer.fADC_GND << RESET;
                        LOG(DEBUG) << BLUE << " VBG ADC " << cChipContainer.fADC_VBG << " measured V " << cChipContainer.fMeasured_VBG << RESET;
                        float ADCs[fGraphSize]     = {cChipContainer.fADC_GND, cChipContainer.fADC_VBG};
                        float voltages[fGraphSize] = {0, cChipContainer.fMeasured_VBG};
                        for(int i = 0; i < fGraphSize; i++)
                        {
                            fPixelADCSlopeGraphs->SetPointX(i, ADCs[i]);
                            fPixelADCSlopeGraphs->SetPointY(i, voltages[i]);
                        }
                        fPixelADCSlopeGraphs->SetMarkerStyle(20);
                        fPixelADCSlopeGraphs->GetXaxis()->SetTitle("ADC output [ADC]");
                        fPixelADCSlopeGraphs->GetYaxis()->SetTitle("ADC output [V]");
                        fPixelADCSlopeGraphs->Fit("pol1", "Q");
                        LOG(DEBUG) << BLUE << " cChipContainer.fOffset " << cChipContainer.fOffset << " cChipContainer.fSlope " << cChipContainer.fSlope << RESET;
                        TF1* fPol1 = fPixelADCSlopeGraphs->GetFunction("pol1");
                        fPol1->SetParameter(0, cChipContainer.fOffset);
                        fPol1->SetParameter(1, cChipContainer.fSlope);
                        fPol1->SetRange(0, 4095);
                        fPol1->SetLineColor(kRed + 1);
                        fPol1->SetLineStyle(2);
                    }
                } // chip
            }     // hybrid
        }         // optical group
    }             // board
}
