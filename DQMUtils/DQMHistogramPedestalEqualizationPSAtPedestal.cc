#include "DQMUtils/DQMHistogramPedestalEqualizationPSAtPedestal.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Occupancy.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramPedestalEqualizationPSAtPedestal::DQMHistogramPedestalEqualizationPSAtPedestal() {}

//========================================================================================================================
DQMHistogramPedestalEqualizationPSAtPedestal::~DQMHistogramPedestalEqualizationPSAtPedestal() {}

//========================================================================================================================
void DQMHistogramPedestalEqualizationPSAtPedestal::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    auto        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string selectSSAfunctionName = "SelectSSAfunction";

    auto        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string selectMPAfunctionName = "SelectMPAfunction";

    bool doDebugHists = findValueInSettings<double>(pSettingsMap, "PedestalEqualizationPSAtPedestal_SaveDebugHists", 1) > 0;

    fDetectorContainer->addReadoutChipQueryFunction(selectSSAfunction, selectSSAfunctionName);

    if(doDebugHists)
    {
        HistContainer<TH2F> theTH2FChipStripSCurve("ThresholdScanAtPedestal", "Threshold scan at pedestal", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, 256, -0.5, 256 - 0.5);
        theTH2FChipStripSCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH2FChipStripSCurve.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipStripSCurveHistograms, theTH2FChipStripSCurve);

        HistContainer<TH2F> theTH2FChipStripTrimCurve("TrimCurve", "TrimCurve", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, 32, -0.5, 31.5);
        theTH2FChipStripTrimCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH2FChipStripTrimCurve.fTheHistogram->GetYaxis()->SetTitle("Trim Bits");
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipStripTrimCurveHistograms, theTH2FChipStripTrimCurve);

        HistContainer<TH1F> theTH1FChipStripSmallestThreshold("TheSmallestThresholdForMaxOccupancy", "TheSmallestThresholdForMaxOccupancy", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
        theTH1FChipStripSmallestThreshold.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH1FChipStripSmallestThreshold.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripSmallestHistograms, theTH1FChipStripSmallestThreshold);

        HistContainer<TH1F> theTH1FChipStripLargestThreshold("TheLargestThresholdForMaxOccupancy", "TheLargestThresholdForMaxOccupancy", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
        theTH1FChipStripLargestThreshold.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH1FChipStripLargestThreshold.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripLargestHistograms, theTH1FChipStripLargestThreshold);
    }

    HistContainer<TH1F> theTH1FChipStripMax("ChannelPedestal", "Channel pedestal", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    theTH1FChipStripMax.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1FChipStripMax.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripMaxHistograms, theTH1FChipStripMax);

    HistContainer<TH1I> theTH1IStripTrimBits("ChannelTrimBit", "Channel trim bit", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    theTH1IStripTrimBits.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1IStripTrimBits.fTheHistogram->GetYaxis()->SetTitle("Trim bit");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1I>>(theOutputFile, theDetectorStructure, fDetectorStripTrimBitsHistograms, theTH1IStripTrimBits);

    fDetectorContainer->removeReadoutChipQueryFunction(selectSSAfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);

    if(doDebugHists)
    {
        HistContainer<TH2F> theTH2FChipPixelSCurve("ThresholdScanAtPedestal", "Threshold scan at pedestal", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5, 256, -0.5, 256 - 0.5);
        theTH2FChipPixelSCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH2FChipPixelSCurve.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelSCurveHistograms, theTH2FChipPixelSCurve);

        HistContainer<TH2F> theTH2FChipPixelTrimCurve("TrimCurve", "TrimCurve", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5, 32, -0.5, 31.5);
        theTH2FChipPixelTrimCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH2FChipPixelTrimCurve.fTheHistogram->GetYaxis()->SetTitle("Trim Bits");
        RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelTrimCurveHistograms, theTH2FChipPixelTrimCurve);

        HistContainer<TH1F> theTH1FChipPixelSmallestThreshold(
            "TheSmallestThresholdForMaxOccupancy", "TheSmallestThresholdForMaxOccupancy", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5);
        theTH1FChipPixelSmallestThreshold.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH1FChipPixelSmallestThreshold.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelSmallestHistograms, theTH1FChipPixelSmallestThreshold);

        HistContainer<TH1F> theTH1FChipPixelLargestThreshold("TheLargestThresholdForMaxOccupancy", "TheLargestThresholdForMaxOccupancy", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5);
        theTH1FChipPixelLargestThreshold.fTheHistogram->GetXaxis()->SetTitle("Channel");
        theTH1FChipPixelLargestThreshold.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
        RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelLargestHistograms, theTH1FChipPixelLargestThreshold);
    }

    HistContainer<TH1F> theTH1FChipPixelMax("ChannelPedestal", "Channel pedestal", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5);
    theTH1FChipPixelMax.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1FChipPixelMax.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelMaxHistograms, theTH1FChipPixelMax);

    HistContainer<TH2F> theTH2FChipPixelMax("2DChannelPedestal", "2D channel pedestal", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPAROWS, -0.5, NMPAROWS - 0.5);
    theTH2FChipPixelMax.fTheHistogram->GetXaxis()->SetTitle("Col");
    theTH2FChipPixelMax.fTheHistogram->GetYaxis()->SetTitle("Row");
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelMax2DHistograms, theTH2FChipPixelMax);

    HistContainer<TH1I> theTH1IPixelTrimBits("ChannelTrimBit", "Channel trim bit", NMPAROWS * NSSACHANNELS, -0.5, NMPAROWS * NSSACHANNELS - 0.5);
    theTH1IPixelTrimBits.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1IPixelTrimBits.fTheHistogram->GetYaxis()->SetTitle("Trim bit");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1I>>(theOutputFile, theDetectorStructure, fDetectorPixelTrimBitsHistograms, theTH1IPixelTrimBits);

    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);

    HistContainer<TH1F> theTH1FChipMax("PedestalDistribution", "Pedestal distribution", 256, -0.5, 256 - 0.5);
    theTH1FChipMax.fTheHistogram->GetXaxis()->SetTitle("Threshold [VcTh]");
    theTH1FChipMax.fTheHistogram->GetYaxis()->SetTitle("Entries");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipMaxHistograms, theTH1FChipMax);
}

//========================================================================================================================
void DQMHistogramPedestalEqualizationPSAtPedestal::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
}

//========================================================================================================================
void DQMHistogramPedestalEqualizationPSAtPedestal::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramPedestalEqualizationPSAtPedestal::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "PedestalEqualizationPSAtPedestal"
    ContainerSerialization theOccupancyStreamer("PedestalEqualizationPSAtPedestalOccupancy");
    ContainerSerialization theOccupancyTrimBitsStreamer("PedestalEqualizationPSAtPedestalOccupancyTrimBits");

    ContainerSerialization theMaxStreamer("PedestalEqualizationPSAtPedestalMax");
    ContainerSerialization theReferenceStreamerSmall("PedestalEqualizationPSAtPedestalReferenceChannelSmall");
    ContainerSerialization theReferenceStreamerLarge("PedestalEqualizationPSAtPedestalReferenceChannelLarge");
    ContainerSerialization theTrimBitsStreamer("PedestalEqualizationPSAtPedestalTrimBits");

    if(theOccupancyStreamer.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        uint16_t dacIt;
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theOccupancyStreamer.deserializeChipContainer<Occupancy, uint16_t>(fDetectorContainer, dacIt);
        // Filling the histograms
        fillSCurvePlots(theDetectorData, dacIt);
        return true;
    }
    if(theOccupancyTrimBitsStreamer.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        uint16_t dacIt;
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theOccupancyTrimBitsStreamer.deserializeChipContainer<Occupancy, uint16_t>(fDetectorContainer, dacIt);
        // Filling the histograms
        fillTrimCurvePlots(theDetectorData, dacIt);
        return true;
    }

    if(theMaxStreamer.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theMaxStreamer.deserializeChipContainer<uint16_t, EmptyContainer>(fDetectorContainer);
        // Filling the histograms
        fillMaxPlots(theDetectorData);
        return true;
    }
    if(theTrimBitsStreamer.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theTrimBitsStreamer.deserializeChipContainer<uint16_t, EmptyContainer>(fDetectorContainer);
        // Filling the histograms
        fillTrimBitsPlots(theDetectorData);
        return true;
    }

    if(theReferenceStreamerSmall.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theReferenceStreamerSmall.deserializeChipContainer<EmptyContainer, std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>(fDetectorContainer);
        // Filling the histograms
        fillReferenceChannelPlots(theDetectorData, true);
        return true;
    }
    if(theReferenceStreamerLarge.attachDeserializer(inputStream))
    {
        // It matched! Decoding data
        // std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
        // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
        DetectorDataContainer theDetectorData = theReferenceStreamerLarge.deserializeChipContainer<EmptyContainer, std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>(fDetectorContainer);
        // Filling the histograms
        fillReferenceChannelPlots(theDetectorData, false);
        return true;
    }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    //  for this stream)
    return false;
    // SoC utilities only - END
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillReferenceChannelPlots(const DetectorDataContainer& theThresholdAtMaxOccupancyContainer, bool isSmallest)
{
    for(auto cBoard: theThresholdAtMaxOccupancyContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto         cType          = theReadoutChip->getFrontEndType();

                    TH1F* cChipHist = nullptr;
                    if(cType == FrontEndType::SSA2)
                    {
                        if(isSmallest)
                            cChipHist = fDetectorChipStripSmallestHistograms.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                        else
                            cChipHist = fDetectorChipStripLargestHistograms.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        if(isSmallest)
                            cChipHist = fDetectorChipPixelSmallestHistograms.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                        else
                            cChipHist = fDetectorChipPixelLargestHistograms.getObject(cBoard->getId())
                                            ->getObject(cOpticalGroup->getId())
                                            ->getObject(cHybrid->getId())
                                            ->getObject(cChip->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;
                    }

                    if(cChip->hasSummary() == false) continue;
                    auto theContent = cChip->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();
                    auto bin        = linearizeRowAndCols(theContent.first.first, theContent.first.second, cChip->getNumberOfCols());
                    cChipHist->SetBinContent(bin + 1, theContent.second);
                }
            }
        }
    }
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillSCurvePlots(const DetectorDataContainer& detectorContainer, const uint16_t dacIt)
{
    for(auto cBoard: detectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto         cType          = theReadoutChip->getFrontEndType();

                    TH2F* cChipSCurve = nullptr;
                    if(cType == FrontEndType::SSA2)
                    {
                        cChipSCurve = fDetectorChipStripSCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cChipSCurve = fDetectorChipPixelSCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }

                    auto theChipContainer = detectorContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            float tmpOccupancy      = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
                            float tmpOccupancyError = theChipContainer->getChannel<Occupancy>(row, col).fOccupancyError;
                            auto  bin               = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            cChipSCurve->SetBinContent(bin + 1, dacIt + 1, tmpOccupancy);
                            cChipSCurve->SetBinError(bin + 1, dacIt + 1, tmpOccupancyError);
                        }
                    }
                }
            }
        }
    }
}
void DQMHistogramPedestalEqualizationPSAtPedestal::fillTrimCurvePlots(const DetectorDataContainer& detectorContainer, const uint16_t dacIt)
{
    for(auto cBoard: detectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto         cType          = theReadoutChip->getFrontEndType();

                    TH2F* cChipSCurve = nullptr;
                    if(cType == FrontEndType::SSA2)
                    {
                        cChipSCurve = fDetectorChipStripTrimCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cChipSCurve = fDetectorChipPixelTrimCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;
                    }

                    auto theChipContainer = detectorContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            float tmpOccupancy      = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
                            float tmpOccupancyError = theChipContainer->getChannel<Occupancy>(row, col).fOccupancyError;
                            auto  bin               = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            cChipSCurve->SetBinContent(bin + 1, dacIt + 1, tmpOccupancy);
                            cChipSCurve->SetBinError(bin + 1, dacIt + 1, tmpOccupancyError);
                        }
                    }
                }
            }
        }
    }
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillSCurvePlotsVector(const std::vector<DetectorDataContainer>& detectorContainerVector, const std::vector<uint16_t>& dacList)
{
    if(dacList.size() != detectorContainerVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt) { fillSCurvePlots(detectorContainerVector.at(dacIt), dacIt); }
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillTrimCurvePlotsVector(const std::vector<DetectorDataContainer>& detectorContainerVector, const std::vector<uint16_t>& dacList)
{
    if(dacList.size() != detectorContainerVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt) { fillTrimCurvePlots(detectorContainerVector.at(dacIt), dacIt); }
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillMaxPlots(const DetectorDataContainer& dacOccupancyContainers)
{
    for(auto cBoard: dacOccupancyContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto         cType          = theReadoutChip->getFrontEndType();

                    TH1F* cChipMax             = nullptr;
                    TH2F* cChipMax2D           = nullptr;
                    TH1F* cChipMaxDistribution = fDetectorChipMaxHistograms.getObject(cBoard->getId())
                                                     ->getObject(cOpticalGroup->getId())
                                                     ->getObject(cHybrid->getId())
                                                     ->getObject(cChip->getId())
                                                     ->getSummary<HistContainer<TH1F>>()
                                                     .fTheHistogram;
                    // cChipMaxDistribution->Reset();
                    if(cType == FrontEndType::SSA2)
                    {
                        cChipMax = fDetectorChipStripMaxHistograms.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<HistContainer<TH1F>>()
                                       .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        cChipMax = fDetectorChipPixelMaxHistograms.getObject(cBoard->getId())
                                       ->getObject(cOpticalGroup->getId())
                                       ->getObject(cHybrid->getId())
                                       ->getObject(cChip->getId())
                                       ->getSummary<HistContainer<TH1F>>()
                                       .fTheHistogram;
                        cChipMax2D = fDetectorChipPixelMax2DHistograms.getObject(cBoard->getId())
                                         ->getObject(cOpticalGroup->getId())
                                         ->getObject(cHybrid->getId())
                                         ->getObject(cChip->getId())
                                         ->getSummary<HistContainer<TH2F>>()
                                         .fTheHistogram;
                    }

                    if(cChip->hasChannelContainer() == false) continue;
                    cChipMaxDistribution->Reset();
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            auto     bin    = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            uint16_t maxDac = cChip->getChannel<uint16_t>(row, col);
                            cChipMax->SetBinContent(bin + 1, maxDac);
                            if(cChipMax2D != nullptr) cChipMax2D->SetBinContent(col + 1, row + 1, maxDac);
                            cChipMaxDistribution->Fill(maxDac);
                        }
                    }
                    fDetectorChipMaxHistograms.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getObject(cChip->getId())
                        ->getSummary<HistContainer<TH1F>>()
                        .fTheHistogram = cChipMaxDistribution;
                }
            }
        }
    }
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillTrimBitsPlots(const DetectorDataContainer& TrimBitContainers)
{
    for(auto cBoard: TrimBitContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto         cType          = theReadoutChip->getFrontEndType();

                    TH1I* hTrimBits = nullptr;
                    if(cType == FrontEndType::SSA2)
                    {
                        hTrimBits = fDetectorStripTrimBitsHistograms.getObject(cBoard->getId())
                                        ->getObject(cOpticalGroup->getId())
                                        ->getObject(cHybrid->getId())
                                        ->getObject(cChip->getId())
                                        ->getSummary<HistContainer<TH1I>>()
                                        .fTheHistogram;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {
                        hTrimBits = fDetectorPixelTrimBitsHistograms.getObject(cBoard->getId())
                                        ->getObject(cOpticalGroup->getId())
                                        ->getObject(cHybrid->getId())
                                        ->getObject(cChip->getId())
                                        ->getSummary<HistContainer<TH1I>>()
                                        .fTheHistogram;
                    }

                    auto theChipContainer = TrimBitContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(theChipContainer->hasChannelContainer() == false) continue;

                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t trimBit = theChipContainer->getChannel<uint16_t>(row, col);
                            auto     bin     = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            hTrimBits->SetBinContent(bin + 1, trimBit);
                        }
                    }
                }
            }
        }
    }
}
