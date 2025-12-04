#ifndef __METADATA_HANDLER_OT_H__
#define __METADATA_HANDLER_OT_H__

#include "tools/MetadataHandler.h"

class MetadataHandlerOT : public MetadataHandler
{
  public:
    MetadataHandlerOT(std::string startOfTestTime);
    ~MetadataHandlerOT();

    void initMetadataHardwareSpecific() override;
    void fillInitialConditionsHardwareSpecific() override;
    void fillFinalConditionsHardwareSpecific() override;

  private:
    void fillFWcompilationTimestampContainer(DetectorDataContainer& theFWcompilationTimestampContainer);
    void fillCICFuseIdContainer(DetectorDataContainer& theCICFuseIdContainer);
    void fillCICConfigurationContainer(DetectorDataContainer& theCICConfigurationContainer);
    void fillIsReadoutChipCalibratedContainer(DetectorDataContainer& theReadoutChipIsCalibratedContainer);
};

#endif
