#include "DQMUtils/DQMHistogramOTalignLpGBTinputs.h"
#include "RootUtils/RootContainerFactory.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"

#include "TFile.h"

//========================================================================================================================
DQMHistogramOTalignLpGBTinputs::DQMHistogramOTalignLpGBTinputs() {}

//========================================================================================================================
DQMHistogramOTalignLpGBTinputs::~DQMHistogramOTalignLpGBTinputs() {}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES
    // make fDetectorData ready to receive the information fromm the stream
    ContainerFactory::copyStructure(theDetectorStructure, fDetectorData);
    // SoC utilities only - END
    
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::process()
{
    // This step it is not necessary, unless you want to format / draw histograms,
    // otherwise they will be automatically saved
    
}

//========================================================================================================================
void DQMHistogramOTalignLpGBTinputs::reset(void)
{
    // Clear histograms if needed
}

//========================================================================================================================
bool DQMHistogramOTalignLpGBTinputs::fill(std::string& inputStream)
{
    // SoC utilities only - BEGIN
    // THIS PART IT IS JUST TO SHOW HOW DATA ARE DECODED FROM THE TCP STREAM WHEN WE WILL GO ON THE SOC
    // IF YOU DO NOT WANT TO GO INTO THE SOC WITH YOUR CALIBRATION YOU DO NOT NEED THE FOLLOWING COMMENTED LINES

    // As example, I'm expecting to receive a data stream from an uint32_t contained from calibration "OTalignLpGBTinputs"
    // ContainerSerialization myStreamer("OTalignLpGBTinputs");
    
    // if(myStreamer.attachDeserializer(inputStream))
    // {
    //     // It matched! Decoding data
    //     std::cout << "Matched OTalignLpGBTinputs!!!!!\n";
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
