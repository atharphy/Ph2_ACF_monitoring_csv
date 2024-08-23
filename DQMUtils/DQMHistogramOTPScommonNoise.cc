#include "DQMUtils/DQMHistogramOTPScommonNoise.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

#include "TFile.h"
#include "TH1F.h"
#include "TH2F.h"

using namespace Ph2_HwDescription;

//========================================================================================================================
DQMHistogramOTPScommonNoise::DQMHistogramOTPScommonNoise() {}

//========================================================================================================================
DQMHistogramOTPScommonNoise::~DQMHistogramOTPScommonNoise() {}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorContainer ready to receive the information fromm the stream
    fDetectorContainer = &theDetectorStructure;
    
    // double theNumberOfEvents    = findValueInSettings<double>(pSettingsMap, "OTPScommonNoise_NumberOfEvents", 10000);
 
    auto        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    std::string selectSSAfunctionName = "SelectSSAfunction";

    auto        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    std::string selectMPAfunctionName = "SelectMPAfunction";

    fDetectorContainer->addReadoutChipQueryFunction(selectSSAfunction, selectSSAfunctionName);

    HistContainer<TH1F> hSSAHits("SSAHits", "StripChipHits", NSSACHANNELS + 2, -0.5, NSSACHANNELS + 1 + 0.5);
    hSSAHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hSSAHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fStripHitHistograms, hSSAHits);

    HistContainer<TH1F> hStripHybridHits("StripHybridHits", "StripHybridHits", NSSACHANNELS * NCHIPS_OT + 2, -0.5, NSSACHANNELS * NCHIPS_OT + 1 + 0.5);
    hStripHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hStripHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStripHybridHitHistograms, hStripHybridHits);
    
    HistContainer<TH1F> hStripModuleHits("StripModuleHits", "StripModuleHits", NSSACHANNELS * NCHIPS_OT * 2 + 2, -0.5, NSSACHANNELS * NCHIPS_OT * 2 + 1 + 0.5);
    hStripModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hStripModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fStripModuleHitHistograms, hStripModuleHits);

    fDetectorContainer->removeReadoutChipQueryFunction(selectSSAfunctionName);

    fDetectorContainer->addReadoutChipQueryFunction(selectMPAfunction, selectMPAfunctionName);

    HistContainer<TH1F> hMPAHits("MPAHits", "MPAHits", NSSACHANNELS * NMPAROWS + 2, -0.5, NSSACHANNELS * NMPAROWS + 1 + 0.5);
    hMPAHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hMPAHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, fPixelHitHistograms, hMPAHits);
    
    HistContainer<TH1F> hPixelHybridHits("PixelHybridHits", "PixelHybridHits", NSSACHANNELS * NMPAROWS * NCHIPS_OT + 2, -0.5, NSSACHANNELS * NMPAROWS * NCHIPS_OT + 1 + 0.5);
    hPixelHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hPixelHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");    
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fPixelHybridHitHistograms, hPixelHybridHits);

    HistContainer<TH1F> hPixelModuleHits("PixelModuleHits", "PixelModuleHits", NSSACHANNELS * NCHIPS_OT * NMPAROWS * 2 + 2, -0.5, NSSACHANNELS * NMPAROWS * NCHIPS_OT * 2 + 1 + 0.5);
    hPixelModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits ");
    hPixelModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of events");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fPixelModuleHitHistograms, hPixelModuleHits);

    for(uint8_t chip = NCHIPS_OT; chip < 2*NCHIPS_OT; chip++)
    {
    
        HistContainer<TH2F> hSSAtoMPAcorrelation(Form("SSA(%d)toMPA(%d)_correlation", chip-NCHIPS_OT, chip),
                                                        Form("SSA(%d) to MPA(%d) correlation", chip-NCHIPS_OT, chip),
                                                        NSSACHANNELS * NMPAROWS + 2, -0.5, NSSACHANNELS * NMPAROWS + 1 + 0.5,
                                                        NSSACHANNELS + 2, -0.5, NSSACHANNELS + 1 + 0.5);
        hSSAtoMPAcorrelation.fTheHistogram->GetXaxis()->SetTitle("Number of hits MPA");
        hSSAtoMPAcorrelation.fTheHistogram->GetYaxis()->SetTitle("Number of hits SSA");
        RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fSSAtoMPAcorrelation[chip], hSSAtoMPAcorrelation);
    }

    fDetectorContainer->removeReadoutChipQueryFunction(selectMPAfunctionName);

    HistContainer<TH2F> hStripPixelHybridHits("StripPixelHybridHits", "StripPixelHybridHits", NSSACHANNELS * NCHIPS_OT + 2, -0.5, NSSACHANNELS * NCHIPS_OT + 1 + 0.5,  NSSACHANNELS * NCHIPS_OT * NMPAROWS + 2, -0.5, NSSACHANNELS * NMPAROWS * NCHIPS_OT + 1 + 0.5);
    hStripPixelHybridHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits pixels");
    hStripPixelHybridHits.fTheHistogram->GetYaxis()->SetTitle("Number of hits strips");    
    RootContainerFactory::bookHybridHistograms(theOutputFile, theDetectorStructure, fStripPixelHybridHistograms, hStripPixelHybridHits);

    HistContainer<TH2F> hStripPixelModuleHits("StripPixelModuleHits", "StripPixelModuleHits", NSSACHANNELS * NCHIPS_OT * 2 + 2, -0.5, NSSACHANNELS * NCHIPS_OT * 2 + 1 + 0.5, NSSACHANNELS * NCHIPS_OT * NMPAROWS * 2 + 2, -0.5, NSSACHANNELS * NMPAROWS * NCHIPS_OT * 2 + 1 + 0.5);
    hStripPixelModuleHits.fTheHistogram->GetXaxis()->SetTitle("Number of hits pixels");
    hStripPixelModuleHits.fTheHistogram->GetYaxis()->SetTitle("Number of hits strips");
    RootContainerFactory::bookOpticalGroupHistograms(theOutputFile, theDetectorStructure, fStripPixelModuleHistograms, hStripPixelModuleHits);
    // SoC utilities only - END
    
}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTPScommonNoise::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTPScommonNoise::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTPScommonNoise"
    // ContainerSerialization myStreamer("OTPScommonNoise");
    
    // if(myStreamer.attachDeserializer(inputStream))
    // {
    //     // It matched! Decoding data
    //     std::cout << "Matched OTPScommonNoise!!!!!\n";
    //     // Need to tell to the streamer what data are contained (in this case in every channel there is an object of type MyType)
    //     DetectorDataContainer theDetectorData = myStreamer.deserializeChannelContainer<MyType>(fDetectorContainer);
    //     // Filling the histograms
    //     myFillplotFunction(theDetectorData);
    //     return true;
    // }
    //the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}

void DQMHistogramOTPScommonNoise::fillChipHitPlots(DetectorDataContainer& theHitData) { return fillChipHitPlots(theHitData, false); }
void DQMHistogramOTPScommonNoise::fillChipHitPlots(DetectorDataContainer& theHitData, bool pFitDistributions)
{

    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getObject(chip->getId());
                    if(theReadoutChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        TH1F*  theHistogram = fStripHitHistograms.getObject(board->getId())
                                                ->getObject(opticalGroup->getId())
                                                ->getObject(hybrid->getId())
                                                ->getObject(chip->getId())
                                                ->getSummary<HistContainer<TH1F>>()
                                                .fTheHistogram;

                        fillEventsVsHitsHist<NSSACHANNELS + 1>(chip, *theHistogram);
                    }
                    else if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        TH1F* theHistogram = fPixelHitHistograms.getObject(board->getId())
                                            ->getObject(opticalGroup->getId())
                                            ->getObject(hybrid->getId())
                                            ->getObject(chip->getId())
                                            ->getSummary<HistContainer<TH1F>>()
                                            .fTheHistogram;

                        fillEventsVsHitsHist<NSSACHANNELS * NMPAROWS + 1 >(chip, *theHistogram);
                    }
                    
                    

                    if(pFitDistributions)
                    {
                        // do fitting
                        LOG(INFO) << BOLDRED << " Fitting not implemented yet... FIXME " << RESET;
                        // TF1* cChipFit = new TF1("chipFit", hitProbabilityFunction, 0, cNChannels + 1, 4);
                        // fitCMNoise(theHistogramEven, cChipFit, cNChannels / 2);
                        // LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() << " even strip common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-"
                        //           << fabs(cChipFit->GetParError(1)) << "%" << RESET;
                        // fitCMNoise(theHistogramOdd, cChipFit, cNChannels / 2);
                        // LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() << " odd strip common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-"
                        //           << fabs(cChipFit->GetParError(1)) << "%" << RESET;
                        // fitCMNoise(theHistogramSum, cChipFit, cNChannels);
                        // LOG(INFO) << BOLDRED << "FE " << hybrid->getId() << " CBC " << chip->getId() << " common mode is " << fabs(cChipFit->GetParameter(1)) << "+/-" << fabs(cChipFit->GetParError(1))
                        //           << "%" << RESET;
                    }
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillHybridHitPlots(DetectorDataContainer& theHitData, bool isStrip)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
               if(isStrip)
                {
                    TH1F* theHistogram = fStripHybridHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    fillEventsVsHitsHist<NSSACHANNELS * NCHIPS_OT + 1>(hybrid, *theHistogram);
                }
                else
                {
                    TH1F* theHistogram = fPixelHybridHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                    fillEventsVsHitsHist<NSSACHANNELS * NMPAROWS * NCHIPS_OT + 1>(hybrid, *theHistogram);
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillModuleHitPlots(DetectorDataContainer& theHitData, bool isStrip)
{
    for(auto board: theHitData)
    {
        for(auto opticalGroup: *board)
        {

            if(isStrip)
            {
                TH1F* theHistogram = fStripModuleHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                fillEventsVsHitsHist<NSSACHANNELS * NCHIPS_OT * 2 + 1>(opticalGroup, *theHistogram);
            }
            else
            {
                TH1F* theHistogram = fPixelModuleHitHistograms.getObject(board->getId())->getObject(opticalGroup->getId())->getSummary<HistContainer<TH1F>>().fTheHistogram;
                fillEventsVsHitsHist<NSSACHANNELS * NMPAROWS * NCHIPS_OT * 2 + 1>(opticalGroup, *theHistogram);
            }
            
        }
    }
}

void DQMHistogramOTPScommonNoise::fillSSAtoMPACorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(board->getId())->getObject(opticalGroup->getId())->getObject(hybrid->getId())->getObject(chip->getId());
                    if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        TH2F* h2DChipSensorCorrelation = fSSAtoMPAcorrelation[chip->getId()].getObject(board->getId())
                                                         ->getObject(opticalGroup->getId())
                                                         ->getObject(hybrid->getId())
                                                         ->getSummary<HistContainer<TH2F>>()
                                                         .fTheHistogram;
                        fillCorrelationHist<NSSACHANNELS + 1, NSSACHANNELS * NMPAROWS + 1>(chip,*h2DChipSensorCorrelation);
                    }
                }
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillStripPixelHybridCorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                TH2F* h2DHybridCorrelation = fStripPixelHybridHistograms.getObject(board->getId())
                                                 ->getObject(opticalGroup->getId())
                                                 ->getObject(hybrid->getId())
                                                 ->getSummary<HistContainer<TH2F>>()
                                                 .fTheHistogram;
                fillCorrelationHist<NSSACHANNELS * NCHIPS_OT + 1, NSSACHANNELS * NMPAROWS * NCHIPS_OT + 1>(hybrid,*h2DHybridCorrelation);
            }
        }
    }
}

void DQMHistogramOTPScommonNoise::fillStripPixelModuleCorrelationPlots(DetectorDataContainer& theSensorData)
{
    for(auto board: theSensorData)
    {
        for(auto opticalGroup: *board)
        {
            TH2F* h2DModuleCorrelation = fStripPixelHybridHistograms.getObject(board->getId())
                                             ->getObject(opticalGroup->getId())
                                             ->getSummary<HistContainer<TH2F>>()
                                             .fTheHistogram;
            fillCorrelationHist<NSSACHANNELS * NCHIPS_OT * 2 + 1, NSSACHANNELS * NMPAROWS * NCHIPS_OT * 2 + 1>(opticalGroup,*h2DModuleCorrelation);
        }
    }
}
