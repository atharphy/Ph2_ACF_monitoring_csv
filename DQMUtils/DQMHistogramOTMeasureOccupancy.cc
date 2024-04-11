#include "DQMUtils/DQMHistogramOTMeasureOccupancy.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/ReadoutChip.h"
#include "Utils/Occupancy.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramOTMeasureOccupancy::DQMHistogramOTMeasureOccupancy() {}

//========================================================================================================================
DQMHistogramOTMeasureOccupancy::~DQMHistogramOTMeasureOccupancy() {}

//========================================================================================================================
void DQMHistogramOTMeasureOccupancy::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    // SoC utilities only - END

    auto selectCBCfunction = [] (const ChipContainer* theChip) {return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::CBC3);};
    std::string selectCBCfunctionName = "SelectCBCfunction";

    auto selectSSAfunction = [] (const ChipContainer* theChip) {return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2);};
    std::string selectSSAfunctionName = "SelectSSAfunction";

    auto selectMPAfunction = [] (const ChipContainer* theChip) {return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2);};
    std::string selectMPAfunctionName = "SelectMPAfunction";

    fDetectorContainer->addReadoutChipQueryFunction(selectCBCfunction, selectCBCfunctionName);
    HistContainer<TH1F> theCBCoccupancyHistogram("ChannelOccupancy", "Channel Occupancy", NCHANNELS, -0.5, NCHANNELS - 0.5);
    theCBCoccupancyHistogram.fTheHistogram->GetXaxis()->SetTitle("channel");
    theCBCoccupancyHistogram.fTheHistogram->GetYaxis()->SetTitle("occupancy");
    theCBCoccupancyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fOccupancyHistogramContainer, theCBCoccupancyHistogram);
    fDetectorContainer->removeReadoutChipQueryFunction(selectCBCfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectSSAfunction, selectSSAfunctionName);
    HistContainer<TH1F> theSSAoccupancyHistogram("ChannelOccupancy", "Channel Occupancy", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    theSSAoccupancyHistogram.fTheHistogram->GetXaxis()->SetTitle("channel");
    theSSAoccupancyHistogram.fTheHistogram->GetYaxis()->SetTitle("occupancy");
    theSSAoccupancyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fOccupancyHistogramContainer, theSSAoccupancyHistogram);
    fDetectorContainer->removeReadoutChipQueryFunction(selectSSAfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);
    HistContainer<TH2F> theMPAoccupancyHistogram("ChannelOccupancy", "Channel Occupancy", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, NMPAROWS, -0.5, NMPAROWS - 0.5);
    theMPAoccupancyHistogram.fTheHistogram->GetXaxis()->SetTitle("col");
    theMPAoccupancyHistogram.fTheHistogram->GetYaxis()->SetTitle("row");
    theMPAoccupancyHistogram.fTheHistogram->SetStats(false);
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fOccupancyHistogramContainer, theMPAoccupancyHistogram);
    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);
}

//========================================================================================================================
void DQMHistogramOTMeasureOccupancy::fillOccupancy(const DetectorDataContainer& theOccupancyContainer)
{
    for(auto theBoard: theOccupancyContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    if(!theChip->hasChannelContainer()) continue;
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getObject(theChip->getId());
                    const ChipDataContainer* theChipContainer = fOccupancyHistogramContainer.getChip(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId(), theChip->getId());
                    // using TH1F and TH2F inheritance from TH1
                    TH1* theOccupancyHistogram;
                    if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2) theOccupancyHistogram = theChipContainer->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    else theOccupancyHistogram = theChipContainer->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    
                    for(uint16_t row = 0; row < theChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < theChip->getNumberOfCols(); ++col)
                        {
                            auto theOccupancy = theChip->getChannel<Occupancy>(row, col);
                            theOccupancyHistogram->SetBinContent(col + 1, row + 1, theOccupancy.fOccupancy);
                            theOccupancyHistogram->SetBinError  (col + 1, row + 1, theOccupancy.fOccupancyError);
                        }
                    }
                }
            }
        }
    }
}


//========================================================================================================================
void DQMHistogramOTMeasureOccupancy::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTMeasureOccupancy::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTMeasureOccupancy::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    ContainerSerialization theOccupancySerialization("OTMeasureOccupancyOccupancy");

    if(theOccupancySerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched OTMeasureOccupancy Occupancy!!!!!\n";
        DetectorDataContainer theDetectorData = theOccupancySerialization.deserializeChipContainer<Occupancy, Occupancy>(fDetectorContainer);
        fillOccupancy(theDetectorData);
        return true;
    }
    return false;
    // SoC utilities only - END
}
