#ifndef __METADATA_HANDLER_H__
#define __METADATA_HANDLER_H__

#include "tools/Tool.h"

class DQMMetadata;

class MetadataHandler : public Tool
{
  public:
    MetadataHandler();
    ~MetadataHandler();

    void initMetadata();
    void fillInitalConditions();
    void fillFinalConditions();

    virtual void initMetadataHardwareSpecific()         = 0;
    virtual void fillInitalConditionsHardwareSpecific() = 0;
    virtual void fillFinalConditionsHardwareSpecific()  = 0;

  protected:
    DQMMetadata* fDQMMetadata{nullptr};

  private:
    void fillNameContainerWithChipIDs();
    void fillReadoutChipConfigurationContainer(DetectorDataContainer& theReadoutChipConfigurationContainer);
    void fillLpGBTConfigurationContainer(DetectorDataContainer& theLpGBTConfigurationContainer);
    void fillLpGBTFuseIdContainer(DetectorDataContainer& theLpGBTFuseIdContainer);
    void fillVTRxFuseIdContainer(DetectorDataContainer& theVTRxFuseIdContainer);
};

#endif