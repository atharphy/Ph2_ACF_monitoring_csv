/*!
        \file                CBCHistogramPulseShape.cc
        \brief               DQM class for Calibration example -> use it as a templare
        \author              Fabio Ravera
        \date                17/1/20
        Support :            mail to : fabio.ravera@cern.ch
*/
#include "../DQMUtils/CBCHistogramPulseShape.h"
#include "../RootUtils/HistContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/Occupancy.h"
#include "../Utils/ThresholdAndNoise.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
CBCHistogramPulseShape::CBCHistogramPulseShape() {}

//========================================================================================================================
CBCHistogramPulseShape::~CBCHistogramPulseShape() {}

//========================================================================================================================
void CBCHistogramPulseShape::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END

    fInitialVcth  = findValueInSettings<double>(pSettingsMap, "PulseShapeInitialVcth", 250);
    fFinalVcth    = findValueInSettings<double>(pSettingsMap, "PulseShapeFinalVcth", 600);
    fVcthStep     = findValueInSettings<double>(pSettingsMap, "PulseShapeVCthStep", 10);
    fInitialDelay = findValueInSettings<double>(pSettingsMap, "PulseShapeInitialDelay", 0);
    fFinalDelay   = findValueInSettings<double>(pSettingsMap, "PulseShapeFinalDelay", 25);
    fDelayStep    = findValueInSettings<double>(pSettingsMap, "PulseShapeDelayStep", 1);

    int delayNbins       = (fFinalDelay - fInitialDelay) / fDelayStep + 1;
    fEffectiveFinalDelay = (delayNbins - 1) * fDelayStep + fInitialDelay;

    float delayHistogramMin = fInitialDelay - fDelayStep / 2.;
    float delayHistogramMax = fEffectiveFinalDelay + fDelayStep / 2.;

    HistContainer<TH1F> theTH1FPulseShapeContainer("PulseShapePerChannel", "PulseShape Per Channel", delayNbins, delayHistogramMin, delayHistogramMax);
    theTH1FPulseShapeContainer.fTheHistogram->GetXaxis()->SetTitle("delay");
    theTH1FPulseShapeContainer.fTheHistogram->GetYaxis()->SetTitle("Vcth");
    theTH1FPulseShapeContainer.fTheHistogram->SetStats(false);
    RootContainerFactory::bookChannelHistograms(theOutputFile, theDetectorStructure, fDetectorChannelPulseShapeHistograms, theTH1FPulseShapeContainer);

    theTH1FPulseShapeContainer.fTheHistogram->SetNameTitle("PulseShapePerChip", "PulseShape Per Chip");
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fDetectorChipPulseShapeHistograms, theTH1FPulseShapeContainer);
}

//========================================================================================================================
void CBCHistogramPulseShape::fillCBCPulseShapePlots(uint16_t delay, DetectorDataContainer& theThresholdAndNoiseContainer)
{
    // float latencyStep = -int(delay/25);
    float binCenterValue = ceil(fFinalDelay / 25.) * 25 - (delay / 25) * 25. - (25. - delay % 25);
    // std::cout<<delay << " - " <<  binCenterValue <<  std::endl;

    for(auto board: theThresholdAndNoiseContainer) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                size_t hybridIndex = hybrid->getIndex();
                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t chipIndex = chip->getIndex();
                    // Retreive the corresponging chip histogram:
                    if(chip->getSummaryContainer<ThresholdAndNoise, ThresholdAndNoise>() == nullptr) continue;
                    TH1F* chipPulseShapeHistogram =
                        fDetectorChipPulseShapeHistograms.at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    int currentBin = chipPulseShapeHistogram->FindBin(binCenterValue);
                    chipPulseShapeHistogram->SetBinContent(currentBin, chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fThreshold);
                    chipPulseShapeHistogram->SetBinError(currentBin, chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fThresholdError);
                    // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                    // by chip and not in one shot) Get channel data and fill the histogram
                    uint8_t channelNumber = 0;
                    for(auto channel: *chip->getChannelContainer<ThresholdAndNoise>()) // for on channel - begin
                    {
                        TH1F* channelPulseShapeHistogram =
                            fDetectorChannelPulseShapeHistograms.at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getChannel<HistContainer<TH1F>>(channelNumber).fTheHistogram;
                        int currentBin = channelPulseShapeHistogram->FindBin(binCenterValue);
                        channelPulseShapeHistogram->SetBinContent(currentBin, channel.fThreshold);
                        channelPulseShapeHistogram->SetBinError(currentBin, channel.fNoise);
                        ++channelNumber;
                    } // for on channel - end
                }     // for on chip - end
            }         // for on hybrid - end
        }             // for on opticalGroup - end
    }                 // for on boards - end
}

//========================================================================================================================
void CBCHistogramPulseShape::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    for(auto board: fDetectorChipPulseShapeHistograms) // for on boards - begin
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                // Create a canvas do draw the plots
                TCanvas* cChipPulseShape = new TCanvas(("Hits_hybrid_" + std::to_string(hybrid->getId())).data(), ("Hits hybrid " + std::to_string(hybrid->getId())).data(), 0, 0, 650, 650);
                cChipPulseShape->Divide(0, hybrid->size());

                for(auto chip: *hybrid) // for on chip - begin
                {
                    size_t       chipIndex     = chip->getIndex();
                    TVirtualPad* currentCanvas = cChipPulseShape->cd(chipIndex + 1);
                    TPad*        myPad         = static_cast<TPad*>(cChipPulseShape->GetPad(chipIndex + 1));
                    // Retreive the corresponging chip histogram:
                    TH1F*   chipPulseShapeHistogram = chip->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    TGaxis* theAxis                 = new TGaxis(myPad->GetUxmin(), myPad->GetUymax(), myPad->GetUxmax() / 3, myPad->GetUymax(), 0., 25., 510, "-");

                    // Format the histogram (here you are outside from the SoC so you can use all the ROOT functions you
                    // need)
                    chipPulseShapeHistogram->DrawCopy();
                    theAxis->SetLabelColor(kRed);
                    theAxis->SetLineColor(kRed);
                    theAxis->Draw();
                    currentCanvas->Modified();
                    currentCanvas->Update();
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on boards - end
}

//========================================================================================================================
void CBCHistogramPulseShape::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool CBCHistogramPulseShape::fill(std::vector<char>& dataBuffer)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // I'm expecting to receive a data stream from an uint32_t contained from calibration "CalibrationExample"
    ChipContainerStream<ThresholdAndNoise, ThresholdAndNoise, uint16_t> theOccupancyStreamer("CBCPulseShape");

    // Try to see if the char buffer matched what I'm expection (container of uint32_t from CalibrationExample
    // procedure)
    if(theOccupancyStreamer.attachBuffer(&dataBuffer))
    {
        // It matched! Decoding chip data
        theOccupancyStreamer.decodeChipData(fDetectorData);
        // Filling the histograms

        fillCBCPulseShapePlots(theOccupancyStreamer.getHeaderElement<0>(), fDetectorData);
        // Cleaning the data container to be ready for the next TCP string
        fDetectorData.cleanDataStored();
        return true;
    }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
