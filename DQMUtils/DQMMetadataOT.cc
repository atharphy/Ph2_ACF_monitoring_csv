#include "DQMUtils/DQMMetadataOT.h"
#include "HWDescription/ReadoutChip.h"
#include "RootUtils/StringContainer.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"

using namespace Ph2_HwDescription;

DQMMetadataOT::DQMMetadataOT() : DQMMetadata() {}

DQMMetadataOT::~DQMMetadataOT() {}

void DQMMetadataOT::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    DQMMetadata::book(theOutputFile, theDetectorStructure, pSettingsMap);

    // child book here
    StringContainer theFWcompilationTimestampStringContainer("FWcompilationTimestamp");
    RootContainerFactory::bookBoardHistograms<StringContainer>(theOutputFile, theDetectorStructure, fFWcompilationTimestampContainer, theFWcompilationTimestampStringContainer);

    StringContainer theCICFuseIdStringContainer("CICFuseId");
    RootContainerFactory::bookHybridHistograms<StringContainer>(theOutputFile, theDetectorStructure, fCICFuseIdContainer, theCICFuseIdStringContainer);

    StringContainer theInitialCICConfigurationStringContainer("InitialCICConfiguration");
    RootContainerFactory::bookHybridHistograms<StringContainer>(theOutputFile, theDetectorStructure, fInitialCICConfigurationContainer, theInitialCICConfigurationStringContainer);

    StringContainer theFinalCICConfigurationStringContainer("FinalCICConfiguration");
    RootContainerFactory::bookHybridHistograms<StringContainer>(theOutputFile, theDetectorStructure, fFinalCICConfigurationContainer, theFinalCICConfigurationStringContainer);

    auto selectMPASSAfunction = [](const ChipContainer* theChip)
    { return ((static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2) || (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2)); };
    std::string selectMPASSAfunctionName = "SelectMPASSAfunction";
    fDetectorContainer->addReadoutChipQueryFunction(selectMPASSAfunction, selectMPASSAfunctionName);
    StringContainer theIsReadoutChipCalibratedStringContainer("IsReadoutChipCalibrated");
    RootContainerFactory::bookChipHistograms<StringContainer>(theOutputFile, theDetectorStructure, fIsReadoutChipCalibratedContainer, theIsReadoutChipCalibratedStringContainer);
    fDetectorContainer->removeReadoutChipQueryFunction(selectMPASSAfunctionName);
}

void DQMMetadataOT::fillFWcompilationTimestamp(const DetectorDataContainer& theFWcompilationTimestampContainer)
{
    for(const auto board: theFWcompilationTimestampContainer)
    {
        auto* theTreeContainerBoard = fFWcompilationTimestampContainer.getObject(board->getId());
        if(!board->hasSummary()) continue;
        theTreeContainerBoard->getSummary<StringContainer>().saveString(board->getSummary<std::string>().c_str());
    }
}

void DQMMetadataOT::fillCICFuseId(const DetectorDataContainer& theCICFuseIdContainer)
{
    for(const auto board: theCICFuseIdContainer)
    {
        auto* theTreeContainerBoard = fCICFuseIdContainer.getObject(board->getId());

        for(const auto opticalGroup: *board)
        {
            auto* theTreeContainerOpticalGroup = theTreeContainerBoard->getObject(opticalGroup->getId());

            for(const auto hybrid: *opticalGroup)
            {
                auto* theTreeContainerHybrid = theTreeContainerOpticalGroup->getObject(hybrid->getId());
                if(!hybrid->hasSummary()) continue;
                theTreeContainerHybrid->getSummary<StringContainer>().saveString(hybrid->getSummary<std::string>().c_str());
            }
        }
    }
}

void DQMMetadataOT::fillCICConfiguration(const DetectorDataContainer& theCICConfigurationContainer, bool initialValue)
{
    for(const auto board: theCICConfigurationContainer)
    {
        BoardDataContainer* theTreeContainerBoard;
        if(initialValue)
            theTreeContainerBoard = fInitialCICConfigurationContainer.getObject(board->getId());
        else
            theTreeContainerBoard = fFinalCICConfigurationContainer.getObject(board->getId());

        for(const auto opticalGroup: *board)
        {
            auto* theTreeContainerOpticalGroup = theTreeContainerBoard->getObject(opticalGroup->getId());

            for(const auto hybrid: *opticalGroup)
            {
                auto* theTreeContainerHybrid = theTreeContainerOpticalGroup->getObject(hybrid->getId());
                if(!hybrid->hasSummary()) continue;
                theTreeContainerHybrid->getSummary<StringContainer>().saveString(hybrid->getSummary<std::string>().c_str());
            }
        }
    }
}

void DQMMetadataOT::fillIsReadoutChipCalibrated(const DetectorDataContainer& theReadoutChipIsCalibratedContainer)
{
    for(const auto board: theReadoutChipIsCalibratedContainer)
    {
        BoardDataContainer* theTreeContainerBoard = fIsReadoutChipCalibratedContainer.getObject(board->getId());

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

bool DQMMetadataOT::fill(std::string& inputStream)
{
    bool motherClassFillResult = DQMMetadata::fill(inputStream);
    if(motherClassFillResult) { return true; }
    else
    {
        // child fill here

        ContainerSerialization theFWcompilationTimestampSerialization("MetadataFWcompilationTimestamp");
        ContainerSerialization theCICFuseIdSerialization("MetadataCICFuseId");
        ContainerSerialization theCICConfigurationSerialization("MetadataCICConfiguration");
        ContainerSerialization theReadoutChipIsCalibratedSerialization("MetadataReadoutChipIsCalibrated");

        if(theFWcompilationTimestampSerialization.attachDeserializer(inputStream))
        {
            // std::cout << "Matched Metadata FWcompilationTimestamp!!!!!\n";
            DetectorDataContainer theDetectorData =
                theFWcompilationTimestampSerialization.deserializeBoardContainer<EmptyContainer, EmptyContainer, EmptyContainer, std::string, EmptyContainer>(fDetectorContainer);
            fillFWcompilationTimestamp(theDetectorData);
            return true;
        }
        if(theCICFuseIdSerialization.attachDeserializer(inputStream))
        {
            // std::cout << "Matched Metadata CICFuseId!!!!!\n";
            DetectorDataContainer theDetectorData =
                theCICFuseIdSerialization.deserializeBoardContainer<EmptyContainer, EmptyContainer, std::string, EmptyContainer, EmptyContainer>(fDetectorContainer);
            fillCICFuseId(theDetectorData);
            return true;
        }
        if(theCICConfigurationSerialization.attachDeserializer(inputStream))
        {
            // std::cout << "Matched Metadata CICConfiguration!!!!!\n";
            bool                  isInitial;
            DetectorDataContainer theDetectorData = theCICConfigurationSerialization.deserializeHybridContainer<EmptyContainer, EmptyContainer, std::string>(fDetectorContainer, isInitial);
            fillCICConfiguration(theDetectorData, isInitial);
            return true;
        }
        if(theReadoutChipIsCalibratedSerialization.attachDeserializer(inputStream))
        {
            // std::cout << "Matched Metadata ReadoutChipConfiguration!!!!!\n";
            DetectorDataContainer theDetectorData = theReadoutChipIsCalibratedSerialization.deserializeChipContainer<EmptyContainer, std::string>(fDetectorContainer);
            fillIsReadoutChipCalibrated(theDetectorData);
            return true;
        }
    }

    return false;
}

void DQMMetadataOT::process()
{
    DQMMetadata::process();

    // child process here
}

void DQMMetadataOT::reset(void)
{
    DQMMetadata::reset();

    // child reset here
}
