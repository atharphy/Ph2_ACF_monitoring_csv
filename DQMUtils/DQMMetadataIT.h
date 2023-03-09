#ifndef __DQM_METADATA_IT__
#define __DQM_METADATA_IT__

#include "DQMUtils/DQMMetadata.h"

class DQMMetadataIT : public DQMMetadata
{
  public:
    DQMMetadataIT();
    ~DQMMetadataIT();

    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    bool fill(std::vector<char>& dataBuffer) override;
    void process() override;
    void reset(void) override;
};

#endif