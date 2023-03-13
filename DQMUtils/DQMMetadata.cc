/*!
  \file                  DQMMetadataIT.cc
  \brief                 Implementationof DQM Metadata
  \author                Mauro DINARDO
  \version               1.0
  \date                  09/03/23
  Support:               email to mauro.dinardo@cern.ch
*/

#include "DQMUtils/DQMMetadata.h"
#include "RootUtils/StringContainer.h"
#include "TTree.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"

DQMMetadata::DQMMetadata() : DQMHistogramBase() {}

DQMMetadata::~DQMMetadata() {}

void DQMMetadata::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    fDetectorContainer = &theDetectorStructure;

    ContainerFactory::copyAndInitStructure<EmptyContainer, std::string, std::string, std::string, std::string, EmptyContainer>(theDetectorStructure, fNameContainer);

    EmptyContainer  theEmpty;
    StringContainer theNameStringContainer("NameId");
    RootContainerFactory::bookHistogramsFromStructure<EmptyContainer, StringContainer, StringContainer, StringContainer, StringContainer, EmptyContainer>(
        theOutputFile, theDetectorStructure, fNameContainer, theEmpty, theNameStringContainer, theNameStringContainer, theNameStringContainer, theNameStringContainer, theEmpty);

    StringContainer theUsernameStringContainer("Username");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fUsernameContainer, theUsernameStringContainer);

    StringContainer theHostNameStringContainer("HostName");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fHostNameContainer, theHostNameStringContainer);

    StringContainer theGitCommitHashStringContainer("GitCommitHash");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fGitCommitHashContainer, theGitCommitHashStringContainer);

    StringContainer theFirmwareVersionStringContainer("FirmwareVersion");
    RootContainerFactory::bookBoardHistograms<StringContainer>(theOutputFile, theDetectorStructure, fFirmwareVersionContainer, theFirmwareVersionStringContainer);

    StringContainer theCalibrationNameStringContainer("CalibrationName");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fCalibrationNameContainer, theCalibrationNameStringContainer);

    StringContainer theDetectorConfigurationStringContainer("DetectorConfiguration");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fDetectorConfigurationContainer, theDetectorConfigurationStringContainer);

    StringContainer theCalibrationStartTimestampStringContainer("CalibrationStartTimestamp");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fCalibrationStartTimestampContainer, theCalibrationStartTimestampStringContainer);

    StringContainer theCalibrationStopTimestampStringContainer("CalibrationStopTimestamp");
    RootContainerFactory::bookDetectorHistograms<StringContainer>(theOutputFile, theDetectorStructure, fCalibrationStopTimestampContainer, theCalibrationStopTimestampStringContainer);

    StringContainer theOriginalReadoutChipConfigurationStringContainer("OriginalReadoutChipConfiguration");
    RootContainerFactory::bookChipHistograms<StringContainer>(theOutputFile, theDetectorStructure, fOriginalReadoutChipConfigurationContainer, theOriginalReadoutChipConfigurationStringContainer);

    StringContainer theFinalReadoutChipConfigurationStringContainer("FinalReadoutChipConfiguration");
    RootContainerFactory::bookChipHistograms<StringContainer>(theOutputFile, theDetectorStructure, fFinalReadoutChipConfigurationContainer, theFinalReadoutChipConfigurationStringContainer);
}

void DQMMetadata::fillObjectNames(const DetectorDataContainer& theNameContainer)
{
    for(const auto board: theNameContainer)
    {
        auto* theTreeContainerBoard = fNameContainer.getObject(board->getId());
        theTreeContainerBoard->getSummary<StringContainer, StringContainer>().saveString(board->getSummary<std::string, std::string>().c_str());
        
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] board size = " << board->size() << std::endl;

        for(const auto opticalGroup: *board)
        {
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] opticalGroup id = " << opticalGroup->getId() << std::endl;
            
            auto* theTreeContainerOpticalGroup = theTreeContainerBoard->getObject(opticalGroup->getId());
            theTreeContainerOpticalGroup->getSummary<StringContainer, StringContainer>().saveString(opticalGroup->getSummary<std::string, std::string>().c_str());

            for(const auto hybrid: *opticalGroup)
            {
                auto* theTreeContainerHybrid = theTreeContainerOpticalGroup->getObject(hybrid->getId());
                theTreeContainerHybrid->getSummary<StringContainer, StringContainer>().saveString(hybrid->getSummary<std::string, std::string>().c_str());

                for(const auto chip: *hybrid)
                {
                    auto* theTreeContainerChip = theTreeContainerHybrid->getObject(chip->getId());
                    theTreeContainerChip->getSummary<StringContainer>().saveString(chip->getSummary<std::string>().c_str());
                }
            }
        }
    }
}

void DQMMetadata::fillUsername(const DetectorDataContainer& theUsernameContainer) { fUsernameContainer.getSummary<StringContainer>().saveString(theUsernameContainer.getSummary<std::string>()); }

void DQMMetadata::fillHostName(const DetectorDataContainer& theHostNameContainer) { fHostNameContainer.getSummary<StringContainer>().saveString(theHostNameContainer.getSummary<std::string>()); }

void DQMMetadata::fillGitCommitHash(const DetectorDataContainer& theGitCommitHashContainer) { fGitCommitHashContainer.getSummary<StringContainer>().saveString(theGitCommitHashContainer.getSummary<std::string>()); }

void DQMMetadata::fillFirmwareVersion(const DetectorDataContainer& theFirmwareVersionContainer)
{
    for(const auto board: theFirmwareVersionContainer)
    {
        fFirmwareVersionContainer.getObject(board->getId())->getSummary<StringContainer>().saveString(board->getSummary<std::string>().c_str());
    }
}

void DQMMetadata::fillCalibrationName(const DetectorDataContainer& theCalibrationNameContainer) { fCalibrationNameContainer.getSummary<StringContainer>().saveString(theCalibrationNameContainer.getSummary<std::string>()); }

void DQMMetadata::fillDetectorConfiguration(const DetectorDataContainer& theDetectorConfigurationContainer)
{
    fDetectorConfigurationContainer.getSummary<StringContainer>().saveString(theDetectorConfigurationContainer.getSummary<std::string>());
}

void DQMMetadata::fillCalibrationTimestamp(const DetectorDataContainer& theCalibrationTimestampContainer, bool start)
{
    DetectorDataContainer *theTimestampPlotContainer;
    if(start)
    {
        theTimestampPlotContainer = &fCalibrationStartTimestampContainer;
    }
    else
    {
        theTimestampPlotContainer = &fCalibrationStopTimestampContainer;
    }
    theTimestampPlotContainer->getSummary<StringContainer>().saveString(theCalibrationTimestampContainer.getSummary<std::string>());
}


void DQMMetadata::fillReadoutChipConfiguration(const DetectorDataContainer& theReadoutChipConfigurationContainer, bool original)
{
    for(const auto board: theReadoutChipConfigurationContainer)
    {
        BoardDataContainer* theTreeContainerBoard;
        if(original)
            theTreeContainerBoard = fOriginalReadoutChipConfigurationContainer.getObject(board->getId());
        else
            theTreeContainerBoard = fFinalReadoutChipConfigurationContainer.getObject(board->getId());

        for(const auto opticalGroup: *board)
        {
            auto* theTreeContainerOpticalGroup = theTreeContainerBoard->getObject(opticalGroup->getId());

            for(const auto hybrid: *opticalGroup)
            {
                auto* theTreeContainerHybrid = theTreeContainerOpticalGroup->getObject(hybrid->getId());

                for(const auto chip: *hybrid)
                {
                    if(!chip->hasSummary()) continue;
                    auto* theTreeContainerChip = theTreeContainerHybrid->getObject(chip->getId());
                    theTreeContainerChip->getSummary<StringContainer>().saveString(chip->getSummary<std::string>().c_str());
                }
            }
        }
    }
}

void DQMMetadata::process() {}

void DQMMetadata::reset() {}

bool DQMMetadata::fill(std::vector<char>& dataBuffer)
{
    std::string inputStream(dataBuffer.begin(), dataBuffer.end());

    ContainerSerialization theNameSerialization("MetadataObjectNames");
    ContainerSerialization theUsernameSerialization("MetadataUsername");
    ContainerSerialization theHostNameSerialization("MetadataHostName");
    ContainerSerialization theGitCommitHashSerialization("MetadataGitCommitHash");
    ContainerSerialization theFirmwareVersionSerialization("MetadataFirmwareVersion");
    ContainerSerialization theCalibrationNameSerialization("MetadataCalibrationName");
    ContainerSerialization theDetectorConfigurationSerialization("MetadataDetectorConfiguration");
    ContainerSerialization theCalibrationTimestampSerialization("MetadataCalibrationTimestamp");
    ContainerSerialization theReadoutChipConfigurationSerialization("MetadataReadoutChipConfiguration");

    if(theNameSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata ObjectNames!!!!!\n";
        DetectorDataContainer theDetectorData =
            theNameSerialization.deserializeDetectorContainer<EmptyContainer, std::string, std::string, std::string, std::string, EmptyContainer>(fDetectorContainer);
        fillObjectNames(theDetectorData);

        return true;
    }
    if(theUsernameSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata Username!!!!!\n";
        DetectorDataContainer theDetectorData =
            theUsernameSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer);
        fillUsername(theDetectorData);

        return true;
    }
    if(theHostNameSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata HostName!!!!!\n";
        DetectorDataContainer theDetectorData =
            theHostNameSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer);
        fillHostName(theDetectorData);

        return true;
    }
    if(theGitCommitHashSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata GitCommitHash!!!!!\n";
        DetectorDataContainer theDetectorData =
            theGitCommitHashSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer);
        fillGitCommitHash(theDetectorData);
        return true;
    }
    if(theFirmwareVersionSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata FirmwareVersion!!!!!\n";
        DetectorDataContainer theDetectorData =
            theFirmwareVersionSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string, EmptyContainer>(fDetectorContainer);
        fillFirmwareVersion(theDetectorData);
        return true;
    }
    if(theCalibrationNameSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata CalibrationName!!!!!\n";
        DetectorDataContainer theDetectorData =
            theCalibrationNameSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer);
        fillCalibrationName(theDetectorData);
        return true;
    }
    if(theDetectorConfigurationSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata DetectorConfiguration!!!!!\n";
        DetectorDataContainer theDetectorData =
            theDetectorConfigurationSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer);
        fillDetectorConfiguration(theDetectorData);

        return true;
    }
    if(theCalibrationTimestampSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata CalibrationTimestamp!!!!!\n";
        bool                  isOriginal;
        DetectorDataContainer theDetectorData =
            theCalibrationTimestampSerialization.deserializeDetectorContainer<EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, EmptyContainer, std::string>(fDetectorContainer, isOriginal);
        fillCalibrationTimestamp(theDetectorData, isOriginal);
        return true;
    }
    if(theReadoutChipConfigurationSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched Metadata ReadoutChipConfiguration!!!!!\n";
        bool                  isOriginal;
        DetectorDataContainer theDetectorData = theReadoutChipConfigurationSerialization.deserializeChipContainer<EmptyContainer, std::string>(fDetectorContainer, isOriginal);
        fillReadoutChipConfiguration(theDetectorData, isOriginal);

        return true;
    }

    return false;
}
