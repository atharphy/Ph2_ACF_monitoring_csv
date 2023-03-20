#include "DQMUtils/DQMMetadataIT.h"

DQMMetadataIT::DQMMetadataIT() : DQMMetadata() {}

DQMMetadataIT::~DQMMetadataIT() {}

void DQMMetadataIT::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    DQMMetadata::book(theOutputFile, theDetectorStructure, pSettingsMap);

    // child book here
}

bool DQMMetadataIT::fill(std::string& inputStream)
{
    bool motherClassFillResult = DQMMetadata::fill(inputStream);
    if(motherClassFillResult) { return true; }
    else
    {
        // child fill here
    }

    return false;
}

void DQMMetadataIT::process()
{
    DQMMetadata::process();

    // child process here
}

void DQMMetadataIT::reset(void)
{
    DQMMetadata::reset();

    // child reset here
}
