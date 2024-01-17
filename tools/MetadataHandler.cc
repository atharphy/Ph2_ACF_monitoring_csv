#include "tools/MetadataHandler.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMMetadata.h"
#endif

MetadataHandler::MetadataHandler() : Tool() {}

MetadataHandler::~MetadataHandler()
{
#ifdef __USE_ROOT__
    if(fResultFile != nullptr && fDQMMetadata != nullptr)
    {
        delete fDQMMetadata;
        fDQMMetadata = nullptr;
    }
#endif
}

void MetadataHandler::initMetadata() { initMetadataHardwareSpecific(); }

void MetadataHandler::fillInitalConditions()
{
    fillNameContainerWithChipIDs();

    std::string theUsername;
    try
    {
        theUsername = std::string(std::getenv("USER"));
    }
    catch(const std::exception& e)
    {
        LOG(WARNING) << e.what();
        LOG(WARNING) << __PRETTY_FUNCTION__ << " Username not set, using dummy name";
        theUsername = "user";
    }

    DetectorDataContainer theUsernameContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theUsernameContainer);
    theUsernameContainer.getSummary<std::string>() = theUsername;

    std::string theHostName;
    try
    {
        theHostName = std::string(std::getenv("HOSTNAME"));
    }
    catch(const std::exception& e)
    {
        LOG(WARNING) << e.what();
        LOG(WARNING) << __PRETTY_FUNCTION__ << " Hostname not set, using dummy name";
        theHostName = "host";
    }
    DetectorDataContainer theHostNameContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theHostNameContainer);
    theHostNameContainer.getSummary<std::string>() = theHostName;

    std::string           theGitCommitHash = GIT_COMMIT_HASH;
    DetectorDataContainer theGitCommitHashContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theGitCommitHashContainer);
    theGitCommitHashContainer.getSummary<std::string>() = theGitCommitHash;

    DetectorDataContainer theCalibrationNameContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theCalibrationNameContainer);
    theCalibrationNameContainer.getSummary<std::string>() = fCalibrationName;

    DetectorDataContainer theDetectorConfigurationContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theDetectorConfigurationContainer);
    theDetectorConfigurationContainer.getSummary<std::string>() = fConfigurationFileContent;

    DetectorDataContainer theCalibrationTimestampContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theCalibrationTimestampContainer);
    theCalibrationTimestampContainer.getSummary<std::string>() = getTimeStampString();

    DetectorDataContainer theReadoutChipConfigurationContainer;
    ContainerFactory::copyAndInitChip<std::string>(*fDetectorContainer, theReadoutChipConfigurationContainer);
    fillReadoutChipConfigurationContainer(theReadoutChipConfigurationContainer);

    DetectorDataContainer theLpGBTConfigurationContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::string>(*fDetectorContainer, theLpGBTConfigurationContainer);
    fillLpGBTConfigurationContainer(theLpGBTConfigurationContainer);

    DetectorDataContainer theLpGBTFuseIdContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::string>(*fDetectorContainer, theLpGBTFuseIdContainer);
    fillLpGBTFuseIdContainer(theLpGBTFuseIdContainer);

    DetectorDataContainer theVTRxFuseIdContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::string>(*fDetectorContainer, theVTRxFuseIdContainer);
    fillVTRxFuseIdContainer(theVTRxFuseIdContainer);
    bool isInitialValue = true;

#ifdef __USE_ROOT__
    fDQMMetadata->book(fResultFile, *fDetectorContainer, fSettingsMap);
    if(fNameContainer != nullptr) fDQMMetadata->fillObjectNames(*fNameContainer);
    fDQMMetadata->fillUsername(theUsernameContainer);
    fDQMMetadata->fillHostName(theHostNameContainer);
    fDQMMetadata->fillGitCommitHash(theGitCommitHashContainer);
    fDQMMetadata->fillCalibrationName(theCalibrationNameContainer);
    fDQMMetadata->fillDetectorConfiguration(theDetectorConfigurationContainer);
    fDQMMetadata->fillCalibrationTimestamp(theCalibrationTimestampContainer, isInitialValue);
    fDQMMetadata->fillReadoutChipConfiguration(theReadoutChipConfigurationContainer, isInitialValue);
    fDQMMetadata->fillLpGBTConfiguration(theLpGBTConfigurationContainer, isInitialValue);
    fDQMMetadata->fillLpGBTFuseId(theLpGBTFuseIdContainer);
    fDQMMetadata->fillVTRxFuseId(theVTRxFuseIdContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theObjectNameSerialization("MetadataObjectNames");
        theObjectNameSerialization.streamByDetectorContainer(fDQMStreamer, *fNameContainer);

        ContainerSerialization theUsernameSerialization("MetadataUsername");
        theUsernameSerialization.streamByDetectorContainer(fDQMStreamer, theUsernameContainer);

        ContainerSerialization theHostNameSerialization("MetadataHostName");
        theHostNameSerialization.streamByDetectorContainer(fDQMStreamer, theHostNameContainer);

        ContainerSerialization theGitCommitHashSerialization("MetadataGitCommitHash");
        theGitCommitHashSerialization.streamByDetectorContainer(fDQMStreamer, theGitCommitHashContainer);

        ContainerSerialization theCalibrationNameSerialization("MetadataCalibrationName");
        theCalibrationNameSerialization.streamByDetectorContainer(fDQMStreamer, theCalibrationNameContainer);

        ContainerSerialization theDetectorConfigurationSerialization("MetadataDetectorConfiguration");
        theDetectorConfigurationSerialization.streamByDetectorContainer(fDQMStreamer, theDetectorConfigurationContainer);

        ContainerSerialization theCalibrationTimestampSerialization("MetadataCalibrationTimestamp");
        theCalibrationTimestampSerialization.streamByDetectorContainer(fDQMStreamer, theCalibrationTimestampContainer, isInitialValue);

        ContainerSerialization theReadoutChipConfigurationSerialization("MetadataReadoutChipConfiguration");
        theReadoutChipConfigurationSerialization.streamByChipContainer(fDQMStreamer, theReadoutChipConfigurationContainer, isInitialValue);

        ContainerSerialization theLpGBTConfigurationSerialization("MetadataLpGBTConfiguration");
        theLpGBTConfigurationSerialization.streamByOpticalGroupContainer(fDQMStreamer, theLpGBTConfigurationContainer, isInitialValue);

        ContainerSerialization theLpGBTFuseIdSerialization("MetadataLpGBTFuseId");
        theLpGBTFuseIdSerialization.streamByBoardContainer(fDQMStreamer, theLpGBTFuseIdContainer);

        ContainerSerialization theVTRxFuseIdSerialization("MetadataVTRxFuseId");
        theVTRxFuseIdSerialization.streamByBoardContainer(fDQMStreamer, theVTRxFuseIdContainer);
    }
#endif

    fillInitalConditionsHardwareSpecific();
}

void MetadataHandler::fillFinalConditions()
{
    bool                  isInitialValue = false;
    DetectorDataContainer theReadoutChipConfigurationContainer;
    ContainerFactory::copyAndInitChip<std::string>(*fDetectorContainer, theReadoutChipConfigurationContainer);
    fillReadoutChipConfigurationContainer(theReadoutChipConfigurationContainer);

    DetectorDataContainer theLpGBTConfigurationContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::string>(*fDetectorContainer, theLpGBTConfigurationContainer);
    fillLpGBTConfigurationContainer(theLpGBTConfigurationContainer);

    DetectorDataContainer theCalibrationTimestampContainer;
    ContainerFactory::copyAndInitDetector<std::string>(*fDetectorContainer, theCalibrationTimestampContainer);
    theCalibrationTimestampContainer.getSummary<std::string>() = getTimeStampString();

#ifdef __USE_ROOT__
    fDQMMetadata->fillReadoutChipConfiguration(theReadoutChipConfigurationContainer, isInitialValue);
    fDQMMetadata->fillLpGBTConfiguration(theLpGBTConfigurationContainer, isInitialValue);
    fDQMMetadata->fillCalibrationTimestamp(theCalibrationTimestampContainer, isInitialValue);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theReadoutChipConfigurationSerialization("MetadataReadoutChipConfiguration");
        theReadoutChipConfigurationSerialization.streamByChipContainer(fDQMStreamer, theReadoutChipConfigurationContainer, isInitialValue);

        ContainerSerialization theLpGBTConfigurationSerialization("MetadataLpGBTConfiguration");
        theLpGBTConfigurationSerialization.streamByOpticalGroupContainer(fDQMStreamer, theLpGBTConfigurationContainer, isInitialValue);

        ContainerSerialization theCalibrationTimestampSerialization("MetadataCalibrationTimestamp");
        theCalibrationTimestampSerialization.streamByDetectorContainer(fDQMStreamer, theCalibrationTimestampContainer, isInitialValue);
    }
#endif

    fillFinalConditionsHardwareSpecific();
}

void MetadataHandler::fillNameContainerWithChipIDs()
{
    if(fNameContainer == nullptr) return;
    for(auto cBoard: *fDetectorContainer)
    {
        fNameContainer->getObject(cBoard->getId())->getSummary<std::string>() = cBoard->getConnectionUri();
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    uint32_t chipFuseId = fReadoutChipInterface->ReadChipFuseID(cChip);
                    fNameContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<std::string>() =
                        std::to_string(chipFuseId);
                }
            }
        }
    }
}

void MetadataHandler::fillReadoutChipConfigurationContainer(DetectorDataContainer& theReadoutChipConfigurationContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    theReadoutChipConfigurationContainer.getObject(cBoard->getId())
                        ->getObject(cOpticalGroup->getId())
                        ->getObject(cHybrid->getId())
                        ->getObject(cChip->getId())
                        ->getSummary<std::string, EmptyContainer>() = cChip->getRegMapStream().str();
                }
            }
        }
    }
}

void MetadataHandler::fillLpGBTConfigurationContainer(DetectorDataContainer& theLpGBTConfigurationContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto theLpGBT = cOpticalGroup->flpGBT;
            if(theLpGBT == nullptr) continue;
            theLpGBTConfigurationContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<std::string, EmptyContainer>() = theLpGBT->getRegMapStream().str();
        }
    }
}

void MetadataHandler::fillLpGBTFuseIdContainer(DetectorDataContainer& theLpGBTFuseIdContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto theLpGBT = cOpticalGroup->flpGBT;
            if(theLpGBT == nullptr) continue;
            uint32_t chipFuseId = flpGBTInterface->ReadChipFuseID(theLpGBT);

            theLpGBTFuseIdContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<std::string, EmptyContainer>() = convertToString(chipFuseId);
        }
    }
}

void MetadataHandler::fillVTRxFuseIdContainer(DetectorDataContainer& theVTRxFuseIdContainer)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto theLpGBT = cOpticalGroup->flpGBT;
            if(theLpGBT == nullptr) continue;
            uint32_t chipFuseId = 0; // flpGBTInterface->ReadVTRxChipFuseID(theLpGBT);
            // Temporary function in lpgbt interface until VTRx interface is implemented
            theVTRxFuseIdContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<std::string, EmptyContainer>() = convertToString(chipFuseId);
        }
    }
}

void MetadataHandler::justBookDQMMetadata()
{
#ifdef __USE_ROOT__
    fDQMMetadata->book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}
