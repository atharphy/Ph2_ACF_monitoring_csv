#include "tools/MetadataHandlerIT.h"

MetadataHandlerIT::MetadataHandlerIT() : MetadataHandler() {}

MetadataHandlerIT::~MetadataHandlerIT() {}

void MetadataHandlerIT::initMetadataHardwareSpecific()
{
#ifdef __USE_ROOT__
    fDQMMetadata = new DQMMetadataIT();
#endif
}

void MetadataHandlerIT::fillInitalConditionsHardwareSpecific() {}

void MetadataHandlerIT::fillFinalConditionsHardwareSpecific()
{
    __attribute__((unused)) const bool isInitialValue = false;
    DetectorDataContainer              theEndOfCalibContainer;
    ContainerFactory::copyAndInitBoard<std::string>(*fDetectorContainer, theEndOfCalibContainer);
    MetadataHandlerIT::fillEndOfCalib(theEndOfCalibContainer);

#ifdef __USE_ROOT__
    auto* theITDQMMetadata = static_cast<DQMMetadataIT*>(fDQMMetadata);
    theITDQMMetadata->fillEndOfCalib(theEndOfCalibContainer);
#else

    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theMetadataSerialization("MetadataITEndOfCalib");
        theMetadataSerialization.streamByHybridContainer(fDQMStreamer, theEndOfCalibContainer, isInitialValue);
    }
#endif
}

void MetadataHandlerIT::fillEndOfCalib(DetectorDataContainer& theEndOfCalibContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint32_t value = 1314;

        theEndOfCalibContainer.getObject(cBoard->getId())->getSummary<std::string>() = std::to_string(value);
    }
}
