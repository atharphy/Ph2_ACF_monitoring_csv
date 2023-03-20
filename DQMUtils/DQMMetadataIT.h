/*!
  \file                  DQMMetadataIT.h
  \brief                 Header file of DQM Metadata
  \author                Mauro DINARDO
  \version               1.0
  \date                  09/03/23
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef DQMMetadataIT
#define DQMMEtadataIT

#include "DQMUtils/DQMMetadata.h"

class DQMMetadataIT : public DQMMetadata
{
  public:
    DQMMetadataIT();
    ~DQMMetadataIT();

    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;

    bool fill(std::string& inputStream) override;
    void process() override;
    void reset(void) override;
};

#endif
