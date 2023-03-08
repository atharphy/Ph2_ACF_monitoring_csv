#ifndef __DQM_METADATA_OT__
#define __DQM_METADATA_OT__

#include "DQMUtils/DQMMetadata.h"

class DQMMetadataOT : public DQMMetadata
{
  public:
    DQMMetadataOT();
    ~DQMMetadataOT();

    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    bool fill(std::vector<char>& dataBuffer) override;
    void process()                           override;
    void reset(void)                         override;
};

#endif