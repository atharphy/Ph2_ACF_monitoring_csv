#ifndef __DQM_METADATA_TREE_OT__
#define __DQM_METADATA_TREE_OT__

#include "DQMUtils/DQMHistogramBase.h"

class TTree;

class DQMMetadata : public DQMHistogramBase
{
  public:
    DQMMetadata();
    ~DQMMetadata();

    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    void fillObjectNames(const DetectorDataContainer& theNameContainer);

    bool fill(std::vector<char>& dataBuffer) override;
    void process()                           override;
    void reset(void)                         override {}

  private:
    DetectorContainer*    fDetectorContainer;
    DetectorDataContainer fNameContainer;
};

#endif