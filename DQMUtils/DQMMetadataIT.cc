#include "DQMUtils/DQMMetadataIT.h"

DQMMetadataIT::DQMMetadataIT() : DQMMetadata() {}

DQMMetadataIT::~DQMMetadataIT() {}

void DQMMetadataIT::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    DQMMetadata::book(theOutputFile, theDetectorStructure, pSettingsMap);

    // child book here
    StringContainer fEndOfCalibStringContainer("ITEndOfCalib");
    RootContainerFactory::bookBoardHistograms<StringContainer>(theOutputFile, theDetectorStructure, fEndOfCalibContainer, fEndOfCalibStringContainer);
}

void DQMMetadataIT::fillEndOfCalib(const DetectorDataContainer& theDetectorData)
{
    for(const auto cBoard: theDetectorData)
    {
        if(cBoard->hasSummary() == false) continue;
        fEndOfCalibContainer.getObject(cBoard->getId())->getSummary<StringContainer>().saveString(cBoard->getSummary<std::string>().c_str());
    }
}

bool DQMMetadataIT::fill(std::string& inputStream)
{
    const bool motherClassFillResult = DQMMetadata::fill(inputStream);

    if(motherClassFillResult == true)
        return true;
    else
    {
        // child fill here
        ContainerSerialization theMetadataSerialization("MetadataITEndOfCalib");

        if(theMetadataSerialization.attachDeserializer(inputStream))
        {
            DetectorDataContainer theDetectorData = theMetadataSerialization.deserializeBoardContainer<EmptyContainer, EmptyContainer, EmptyContainer, std::string, EmptyContainer>(fDetectorContainer);
            DQMMetadataIT::fillEndOfCalib(theDetectorData);
            return true;
        }
    }

    return false;
}

void DQMMetadataIT::process()
{
    DQMMetadata::process();

    // child process here
}

void DQMMetadataIT::reset(void)
{
    DQMMetadata::reset();

    // child reset here
}
