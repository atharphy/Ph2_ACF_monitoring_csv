/*!
  \file                  MetadataHandlerIT.h
  \brief                 Header file of Metadata Handler for IT
  \author                Mauro DINARDO
  \version               1.0
  \date                  09/03/23
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef MetadataHandlerIT_H
#define MetadataHandlerIT_H

#include "tools/MetadataHandler.h"

#ifdef __USE_ROOT__
#include "DQMUtils/DQMMetadataIT.h"
#endif

class MetadataHandlerIT : public MetadataHandler
{
  public:
    MetadataHandlerIT();
    ~MetadataHandlerIT();

    void initMetadataHardwareSpecific() override;
    void fillInitalConditionsHardwareSpecific() override;
    void fillFinalConditionsHardwareSpecific() override;

  private:
    void fillEndOfCalib(DetectorDataContainer& theEndOfCalibContainer);
};

#endif
