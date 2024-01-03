#ifndef __METADATA_HANDLER_IT_H__
#define __METADATA_HANDLER_IT_H__

#include "tools/MetadataHandler.h"

class MetadataHandlerIT : public MetadataHandler
{
  public:
    MetadataHandlerIT();
    ~MetadataHandlerIT();

    void initMetadataHardwareSpecific() override;
    void fillInitalConditionsHardwareSpecific() override;
    void fillFinalConditionsHardwareSpecific() override;
};

#endif