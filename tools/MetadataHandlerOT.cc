#include "tools/MetadataHandlerOT.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMMetadataOT.h"
#endif

using namespace Ph2_HwDescription;

MetadataHandlerOT::MetadataHandlerOT(std::string startOfTestTime) : MetadataHandler(startOfTestTime) {}

MetadataHandlerOT::~MetadataHandlerOT() {}

void MetadataHandlerOT::initMetadataHardwareSpecific()
{
#ifdef __USE_ROOT__
    fDQMMetadata = new DQMMetadataOT();
#endif
}

void MetadataHandlerOT::fillInitialConditionsHardwareSpecific()
{
    bool                  isInitialValue = true;
    DetectorDataContainer theCICFuseIdContainer;
    ContainerFactory::copyAndInitHybrid<std::string>(*fDetectorContainer, theCICFuseIdContainer);
    fillCICFuseIdContainer(theCICFuseIdContainer);

    DetectorDataContainer theCICConfigurationContainer;
    ContainerFactory::copyAndInitHybrid<std::string>(*fDetectorContainer, theCICConfigurationContainer);
    fillCICConfigurationContainer(theCICConfigurationContainer);

    auto selectMPASSAfunction = [](const ChipContainer* theChip)
    { return ((static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2) || (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2)); };
    std::string selectMPASSAfunctionName = "SelectMPASSAfunction";
    fDetectorContainer->addReadoutChipQueryFunction(selectMPASSAfunction, selectMPASSAfunctionName);
    DetectorDataContainer theReadoutChipIsCalibratedContainer;
    ContainerFactory::copyAndInitChip<std::string>(*fDetectorContainer, theReadoutChipIsCalibratedContainer);
    fillIsReadoutChipCalibratedContainer(theReadoutChipIsCalibratedContainer);
    fDetectorContainer->removeReadoutChipQueryFunction(selectMPASSAfunctionName);


#ifdef __USE_ROOT__
    auto* theOTDQMMetadata = static_cast<DQMMetadataOT*>(fDQMMetadata);
    theOTDQMMetadata->fillCICFuseId(theCICFuseIdContainer);
    theOTDQMMetadata->fillCICConfiguration(theCICConfigurationContainer, isInitialValue);
    fDetectorContainer->addReadoutChipQueryFunction(selectMPASSAfunction, selectMPASSAfunctionName);
    fDQMMetadata->fillIsReadoutChipCalibrated(theReadoutChipIsCalibratedContainer);
    fDetectorContainer->removeReadoutChipQueryFunction(selectMPASSAfunctionName);
    
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theCICFuseIdSerialization("MetadataCICFuseId");
        theCICFuseIdSerialization.streamByBoardContainer(fDQMStreamer, theCICFuseIdContainer);

        ContainerSerialization theCICConfigurationSerialization("MetadataCICConfiguration");
        theCICConfigurationSerialization.streamByHybridContainer(fDQMStreamer, theCICConfigurationContainer, isInitialValue);

        fDetectorContainer->addReadoutChipQueryFunction(selectMPASSAfunction, selectMPASSAfunctionName);
        ContainerSerialization theReadoutChipIsCalibratedSerialization("MetadataReadoutChipIsCalibrated");
        theReadoutChipIsCalibratedSerialization.streamByChipContainer(fDQMStreamer, theReadoutChipIsCalibratedContainer);
        fDetectorContainer->removeReadoutChipQueryFunction(selectMPASSAfunctionName);
    }
#endif
}

void MetadataHandlerOT::fillFinalConditionsHardwareSpecific()
{
    bool isInitialValue = false;

    DetectorDataContainer theCICConfigurationContainer;
    ContainerFactory::copyAndInitHybrid<std::string>(*fDetectorContainer, theCICConfigurationContainer);
    fillCICConfigurationContainer(theCICConfigurationContainer);

#ifdef __USE_ROOT__
    auto* theOTDQMMetadata = static_cast<DQMMetadataOT*>(fDQMMetadata);
    theOTDQMMetadata->fillCICConfiguration(theCICConfigurationContainer, isInitialValue);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theCICConfigurationSerialization("MetadataCICConfiguration");
        theCICConfigurationSerialization.streamByHybridContainer(fDQMStreamer, theCICConfigurationContainer, isInitialValue);
    }
#endif
}

void MetadataHandlerOT::fillCICFuseIdContainer(DetectorDataContainer& theCICFuseIdContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                uint32_t chipFuseId = 0;
                if(static_cast<OuterTrackerHybrid*>(cHybrid)->fCic->getFrontEndType() == FrontEndType::CIC2)
                    chipFuseId = fCicInterface->ReadChipFuseID(static_cast<OuterTrackerHybrid*>(cHybrid)->fCic);
                theCICFuseIdContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getSummary<std::string>() = std::to_string(chipFuseId);
            }
        }
    }
}

void MetadataHandlerOT::fillCICConfigurationContainer(DetectorDataContainer& theCICConfigurationContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                theCICConfigurationContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getSummary<std::string>() =
                    static_cast<OuterTrackerHybrid*>(cHybrid)->fCic->getRegMapStream().str();
            }
        }
    }
}

void MetadataHandlerOT::fillIsReadoutChipCalibratedContainer(DetectorDataContainer& theReadoutChipIsCalibratedContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    bool isCalibrated = cChip->getIsCalibrationDataLoaded();
                    theReadoutChipIsCalibratedContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getObject(cChip->getId())
                        ->getSummary<std::string, EmptyContainer>() = convertToString(isCalibrated);
                }
            }
        }
    }
}