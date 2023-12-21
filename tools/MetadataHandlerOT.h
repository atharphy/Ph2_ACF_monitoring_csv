#ifndef __METADATA_HANDLER_OT_H__
#define __METADATA_HANDLER_OT_H__

#include "tools/MetadataHandler.h"

class MetadataHandlerOT : public MetadataHandler
{
  public:
    MetadataHandlerOT();
    ~MetadataHandlerOT();

    void initMetadataHardwareSpecific() override;
    void fillInitalConditionsHardwareSpecific() override;
    void fillFinalConditionsHardwareSpecific() override;

  private:
    void fillCICFuseIdContainer(DetectorDataContainer& theCICFuseIdContainer);
    void fillCICConfigurationContainer(DetectorDataContainer& theCICConfigurationContainer);

};

#endif