#include "DQMUtils/DQMMetadataOT.h"

DQMMetadataOT::DQMMetadataOT() : DQMMetadata() {}

DQMMetadataOT::~DQMMetadataOT() {}

void DQMMetadataOT::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    DQMMetadata::book(theOutputFile, theDetectorStructure, pSettingsMap);

    // child book here
}

bool DQMMetadataOT::fill(std::vector<char>& dataBuffer)
{
    bool motherClassFillResult = DQMMetadata::fill(dataBuffer);
    if(motherClassFillResult) { return true; }
    else
    {
        // child fill here
    }

    return false;
}

void DQMMetadataOT::process()
{
    DQMMetadata::process();

    // child process here
}

void DQMMetadataOT::reset(void)
{
    DQMMetadata::reset();

    // child reset here
}
