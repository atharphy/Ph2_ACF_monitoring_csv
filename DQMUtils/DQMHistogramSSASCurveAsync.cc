/*!

*/

#include "DQMHistogramSSASCurveAsync.h"
#include "../RootUtils/HistContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "TCanvas.h"
#include "TFile.h"
#include "TH2F.h"

//========================================================================================================================
DQMHistogramSSASCurveAsync::DQMHistogramSSASCurveAsync() {}

//========================================================================================================================
DQMHistogramSSASCurveAsync::~DQMHistogramSSASCurveAsync() {}

//========================================================================================================================
void DQMHistogramSSASCurveAsync::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END
    // creating the histograms fo all the chips:
    // create the HistContainer<TH2F> as you would create a TH2F (it implements some feature needed to avoid memory
    // leaks in copying histograms like the move constructor)
    size_t chans = 0;
    for(auto board: theDetectorStructure) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                for(auto chip: *hybrid) // for on chip - begin
                { chans = chip->size(); }
            }
        }
    }
    // std::cout<<chans<<std::endl;
    HistContainer<TH2F> theTH2FPedestalContainer("HitPerChannel", "Hit Per Channel", chans, -0.5, float(chans) - 0.5, 255, -0.5, 254.5);
    // create Histograms for all the chips, they will be automatically accosiated to the output file, no need to save
    // them, change the name for every chip or set their directory
    RootContainerFactory::bookChipHistograms<HistContainer<TH2F>>(theOutputFile, theDetectorStructure, fDetectorHitHistograms, theTH2FPedestalContainer);
}

//========================================================================================================================
void DQMHistogramSSASCurveAsync::fillSSASCurveAsyncPlots(DetectorDataContainer& theHitContainer, uint32_t thresh)
{
    for(auto board: theHitContainer) // for on boards - begin
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

                    TH2F* chipHitHistogram = fDetectorHitHistograms.at(boardIndex)->at(opticalGroupIndex)->at(hybridIndex)->at(chipIndex)->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    uint  channelBin       = 1;
                    // Check if the chip data are there (it is needed in the case of the SoC when data may be sent chip
                    // by chip and not in one shot)
                    if(chip->getChannelContainer<std::pair<std::array<uint32_t, 2>, float>>() == nullptr) continue;
                    // Get channel data and fill the histogram
                    for(auto channel: *chip->getChannelContainer<std::pair<std::array<uint32_t, 2>, float>>()) // for on channel -
                                                                                                               // begin
                    {
                        // LOG (INFO) << BOLDRED << channelBin << RESET;
                        chipHitHistogram->SetBinContent(channelBin++, thresh, channel.first[0]);
                        // LOG (INFO) << BOLDRED << "DNE" << RESET;
                    } // for on channel - end
                }
            } // for on chip - end
        }     // for on module - end
    }         // for on boards - end
}

//========================================================================================================================
void DQMHistogramSSASCurveAsync::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    for(auto board: fDetectorHitHistograms) // for on boards - begin
    {
        size_t boardIndex = board->getIndex();
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            size_t opticalGroupIndex = opticalGroup->getIndex();
            for(auto module: *opticalGroup) // for on hybrid - begin
            {
                size_t moduleIndex = module->getIndex();

                // Create a canvas do draw the plots
                TCanvas* cValidation = new TCanvas(("Hits_module_" + std::to_string(module->getId())).data(), ("Hits module " + std::to_string(module->getId())).data(), 0, 0, 650, 650);
                cValidation->Divide(module->size());
                for(auto chip: *module) // for on chip - begin
                {
                    size_t chipIndex = chip->getIndex();
                    cValidation->cd(chipIndex + 1);
                    // Retreive the corresponging chip histogram:
                    TH2F* chipHitHistogram = fDetectorHitHistograms.at(boardIndex)->at(opticalGroupIndex)->at(moduleIndex)->at(chipIndex)->getSummary<HistContainer<TH2F>>().fTheHistogram;
                    // Format the histogram (here you are outside from the SoC so you can use all the ROOT functions you
                    // need)
                    chipHitHistogram->SetStats(false);
                    chipHitHistogram->SetLineColor(kRed);
                    chipHitHistogram->DrawCopy();
                }
            } // for on chip - end
        }     // for on module - end
    }         // for on boards - end
}

//========================================================================================================================
void DQMHistogramSSASCurveAsync::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramSSASCurveAsync::fill(std::vector<char>& dataBuffer)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // I'm expecting to receive a data stream from an uint32_t contained from calibration "SSASCurveAsync"
    ChannelContainerStream<std::pair<std::array<uint32_t, 2>, float>> theHitStreamer("SSASCurveAsync");

    // Try to see if the char buffer matched what I'm expection (container of uint32_t from SSASCurveAsync procedure)
    if(theHitStreamer.attachBuffer(&dataBuffer))
    {
        // It matched! Decoding chip data
        theHitStreamer.decodeChipData(fDetectorData);
        // Filling the histograms
        // fillSSASCurveAsyncPlots(fDetectorData);
        // Cleaning the data container to be ready for the next TCP string
        fDetectorData.cleanDataStored();
        return true;
    }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
