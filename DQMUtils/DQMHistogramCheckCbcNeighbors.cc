#include "../DQMUtils/DQMHistogramCheckCbcNeighbors.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "TFile.h"

//========================================================================================================================
DQMHistogramCheckCbcNeighbors::DQMHistogramCheckCbcNeighbors() {}

//========================================================================================================================
DQMHistogramCheckCbcNeighbors::~DQMHistogramCheckCbcNeighbors() {}

//========================================================================================================================
void DQMHistogramCheckCbcNeighbors::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END
    
}

//========================================================================================================================
void DQMHistogramCheckCbcNeighbors::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramCheckCbcNeighbors::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramCheckCbcNeighbors::fill(std::vector<char>& dataBuffer)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "CheckCbcNeighbors"
    // ChannelContainerStream<uint32_t> theHitStreamer("CheckCbcNeighbors");

    // Try to see if the char buffer matched what I'm expection (container of uint32_t from CheckCbcNeighbors
    // procedure)
    // if(theHitStreamer.attachBuffer(&dataBuffer))
    // {
    //     // It matched! Decoding chip data
    //     theHitStreamer.decodeChipData(fDetectorData);
    //     // Filling the histograms
    //     fillCheckCbcNeighborsPlots(fDetectorData);
    //     // Cleaning the data container to be ready for the next TCP string
    //     fDetectorData.cleanDataStored();
    //     return true;
    // }
    // the stream does not match, the expected (DQM interface will try to check if other DQM istogrammers are looking
    // for this stream)
    return false;
    // SoC utilities only - END
}
