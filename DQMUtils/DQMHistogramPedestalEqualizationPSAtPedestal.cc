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

    fDetectorContainer->addReadoutChipQueryFunction(selectSSAfunction, selectSSAfunctionName);
    HistContainer<TH2F> theTH2FChipStripSCurve("SCurve", "SCurve", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5, 256, 0, 255);
    theTH2FChipStripSCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH2FChipStripSCurve.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipStripSCurveHistograms, theTH2FChipStripSCurve);
    HistContainer<TH1F> theTH1FChipStripMax("Max", "Max", NSSACHANNELS, -0.5, NSSACHANNELS - 0.5);
    theTH1FChipStripMax.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1FChipStripMax.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipStripMaxHistograms, theTH1FChipStripMax);

    fDetectorContainer->removeReadoutChipQueryFunction(selectSSAfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);
    HistContainer<TH2F> theTH2FChipPixelSCurve("SCurve", "SCurve", NMPAROWS*NSSACHANNELS, -0.5, NMPAROWS*NSSACHANNELS - 0.5, 256, 0, 255);
    theTH2FChipPixelSCurve.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH2FChipPixelSCurve.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelSCurveHistograms, theTH2FChipPixelSCurve);

    HistContainer<TH1F> theTH1FChipPixelMax("Max", "Max", NMPAROWS*NSSACHANNELS, -0.5, NMPAROWS*NSSACHANNELS - 0.5);
    theTH1FChipPixelMax.fTheHistogram->GetXaxis()->SetTitle("Channel");
    theTH1FChipPixelMax.fTheHistogram->GetYaxis()->SetTitle("Threshold [VcTh]");
    RootContainerFactory::bookChipHistograms<HistContainer<TH1F>>(theOutputFile, theDetectorStructure, fDetectorChipPixelMaxHistograms, theTH1FChipPixelMax);
 
    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);

       
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
    // ContainerSerialization myStreamer("PedestalEqualizationPSAtPedestalOccupancy");
    
    // if(myStreamer.attachDeserializer(inputStream))
    // {
    // //     // It matched! Decoding data
    //     std::cout << "Matched PedestalEqualizationPSAtPedestal!!!!!\n";
    // //     // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
    //     std::vector<uint16_t> dacList;
    //     std::vector<DetectorDataContainer> theDetectorData = myStreamer.deserializeChipContainer<Occupancy, Occupancy>(fDetectorContainer, dacList);
    // //     // Filling the histograms
    //     fillSCurvePlots(theDetectorData, dacList); // FIXME!
    //     return true;
    // }
    //the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}


void DQMHistogramPedestalEqualizationPSAtPedestal::fillSCurvePlots(const std::vector<DetectorDataContainer>& detectorContainerVector, const std::vector<uint16_t>&         dacList)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;


    if(dacList.size() != detectorContainerVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        std::cout << " dacIt " << dacIt << std::endl;
        for(auto cBoard: detectorContainerVector.at(dacIt))
        {
            std::cout << " board " << cBoard->getId() << std::endl;
            for(auto cOpticalGroup: *cBoard)
            {
                std::cout << " cOpticalGroup " << cOpticalGroup->getId() << std::endl;
                for(auto cHybrid: *cOpticalGroup)
                {
                    std::cout << " cHybrid " << cHybrid->getId() << std::endl;

                    for(auto cChip: *cHybrid)
                    {
                        std::cout << " cChip " << cChip->getId() << std::endl;
                        ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                        auto     cType = theReadoutChip->getFrontEndType();

                        TH2F* cChipSCurve = nullptr;
                        if(cType == FrontEndType::SSA2)
                        {
                            std::cout << " get cChipSCurve SSA" << std::endl;
                            cChipSCurve = fDetectorChipStripSCurveHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH2F>>()
                                          .fTheHistogram;

                            std::cout << " done SSA" << std::endl;
                        }
                        else if(cType == FrontEndType::MPA2)
                        {
                            std::cout << " get cChipSCurve MPA" << std::endl;
                            cChipSCurve = fDetectorChipPixelSCurveHistograms.getObject(cBoard->getId())
                                              ->getObject(cOpticalGroup->getId())
                                              ->getObject(cHybrid->getId())
                                              ->getObject(cChip->getId())
                                              ->getSummary<HistContainer<TH2F>>()
                                              .fTheHistogram;

                        std::cout << " done MPA" << std::endl;
                        }

                        std::cout << " done getting hists" << std::endl;
                        auto theChipContainer = detectorContainerVector.at(dacIt).getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        if(theChipContainer->hasChannelContainer() == false) continue;
                        std::cout << " done theChipContainer" << std::endl;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                    
                                float tmpOccupancy      = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
                                float tmpOccupancyError = theChipContainer->getChannel<Occupancy>(row, col).fOccupancyError;
                                auto bin = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                                cChipSCurve->SetBinContent(bin + 1, dacIt + 1, tmpOccupancy);
                                cChipSCurve->SetBinError(bin + 1, dacIt + 1, tmpOccupancyError);
                            }
                        }
                    }
                }
            }
        }
    }



    //fillMaxPlots();
}

void DQMHistogramPedestalEqualizationPSAtPedestal::fillMaxPlots(const DetectorDataContainer& dacOccupancyContainers)
{
    for(auto cBoard: dacOccupancyContainers)
    {
        std::cout << " board " << cBoard->getId() << std::endl;
        for(auto cOpticalGroup: *cBoard)
        {
            std::cout << " cOpticalGroup " << cOpticalGroup->getId() << std::endl;
            for(auto cHybrid: *cOpticalGroup)
            {
                std::cout << " cHybrid " << cHybrid->getId() << std::endl;

                for(auto cChip: *cHybrid)
                {
                    std::cout << " cChip " << cChip->getId() << std::endl;
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                    auto     cType = theReadoutChip->getFrontEndType();

                    TH1F* cChipMax    = nullptr;
                    if(cType == FrontEndType::SSA2)
                    {

                        std::cout << " get cChipMax SSA" << std::endl;
                        cChipMax = fDetectorChipStripMaxHistograms.getObject(cBoard->getId())
                                      ->getObject(cOpticalGroup->getId())
                                      ->getObject(cHybrid->getId())
                                      ->getObject(cChip->getId())
                                      ->getSummary<HistContainer<TH1F>>()
                                      .fTheHistogram;
                        std::cout << " done SSA" << std::endl;
                    }
                    else if(cType == FrontEndType::MPA2)
                    {

                        std::cout << " get cChipMax MPA" << std::endl;
                        cChipMax = fDetectorChipPixelMaxHistograms.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getSummary<HistContainer<TH1F>>()
                                          .fTheHistogram;
                        std::cout << " done MPA" << std::endl;
                    }

                    std::cout << " done getting hists" << std::endl;
                    // auto theChipContainer = detectorContainerVector.at(dacIt).getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    // if(theChipContainer->hasChannelContainer() == false) continue;
                    // std::cout << " done theChipContainer" << std::endl;
                    

                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto bin = linearizeRowAndCols(row, col, cChip->getNumberOfCols());  
                                // float maxOccupancy = -1;
                                // uint16_t maxDac = -1;
                                std::cout << " bin " << bin <<  std::endl;
                                const auto& occupancyMap = cChip->getChannel<std::map<uint16_t, float>>(row, col);
                                std::cout << " occupancyMap " <<  std::endl;
                        if (occupancyMap.empty()) continue;

                        auto maxIter = std::max_element(
                            occupancyMap.begin(),
                            occupancyMap.end(),
                            [](const auto& a, const auto& b) {
                                return a.second < b.second;
                            });
                            std::cout << " max " <<  std::endl;
                        uint16_t maxDac = maxIter->first;
                                // for (uint16_t dac = 1; dac <=    cChipSCurve->GetNbinsY(); dac++)
                                // {                  
                                // float tmpOccupancy = cChipSCurve->GetBinContent(bin + 1, dac);

                                // if (tmpOccupancy > maxOccupancy) 
                                // {
                                //     maxOccupancy = tmpOccupancy;
                                //     maxDac = dac;
                                // }

                                cChipMax->SetBinContent(bin +1, maxDac);

                            
                        }
                    }
                }
            }
        }
    }
}
