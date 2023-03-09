#ifndef __DQM_METADATA__
#define __DQM_METADATA__

#include "DQMUtils/DQMHistogramBase.h"

class TTree;

class DQMMetadata : public DQMHistogramBase
{
  public:
    DQMMetadata();
    ~DQMMetadata();

    virtual void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    void fillObjectNames(const DetectorDataContainer& theNameContainer);
    void fillUsername(const DetectorDataContainer& theUsernameContainer);
    void fillHostName(const DetectorDataContainer& theHostNameContainer);
    void fillDetectorConfiguration(const DetectorDataContainer& theDetectorConfigurationContainer);
    void fillReadoutChipConfiguration(const DetectorDataContainer& theReadoutChipConfigurationContainer, bool original);

    virtual bool fill(std::vector<char>& dataBuffer) override;
    virtual void process() override;
    virtual void reset() override;

  private:
    DetectorContainer*    fDetectorContainer;
    DetectorDataContainer fNameContainer;
    DetectorDataContainer fUsernameContainer;
    DetectorDataContainer fHostNameContainer;
    DetectorDataContainer fDetectorConfigurationContainer;
    DetectorDataContainer fOriginalReadoutChipConfigurationContainer;
    DetectorDataContainer fFinalReadoutChipConfigurationContainer;
};

#endif