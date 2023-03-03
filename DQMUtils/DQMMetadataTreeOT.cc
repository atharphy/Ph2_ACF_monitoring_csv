#include "DQMUtils/DQMMetadataTreeOT.h"
#include "Utils/ContainerFactory.h"
#include "TTree.h"
#include "Utils/EmptyContainer.h"
#include "RootUtils/StringContainer.h"
#include "Utils/ContainerSerialization.h"

DQMMetadataTreeOT::DQMMetadataTreeOT()
: DQMHistogramBase()
{}

DQMMetadataTreeOT::~DQMMetadataTreeOT()
{}

void DQMMetadataTreeOT::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap)
{
    fDetectorContainer = &theDetectorStructure;

    ContainerFactory::copyAndInitStructure<EmptyContainer, std::string, std::string, std::string, std::string, EmptyContainer>(theDetectorStructure, fNameContainer);

    EmptyContainer theEmpty;
    StringContainer theNameStringContainer("NameId");
    RootContainerFactory::bookHistogramsFromStructure<EmptyContainer,StringContainer, StringContainer, StringContainer, StringContainer, EmptyContainer>(theOutputFile, theDetectorStructure, fNameContainer, theEmpty, theNameStringContainer, theNameStringContainer, theNameStringContainer, theNameStringContainer, theEmpty);
}

void DQMMetadataTreeOT::fillObjectNames(const DetectorDataContainer& theNameContainer)
{
    for(const auto board : theNameContainer)
    {
        auto* theTreeContainerBoard = fNameContainer.getObject(board->getId());
        theTreeContainerBoard->getSummary<StringContainer, StringContainer>().setString(board->getSummary<std::string, std::string>().c_str());
        theTreeContainerBoard->getSummary<StringContainer, StringContainer>().write();

        for(const auto opticalGroup : *board)
        {
            auto* theTreeContainerOpticalGroup = theTreeContainerBoard->getObject(opticalGroup->getId());
            theTreeContainerOpticalGroup->getSummary<StringContainer, StringContainer>().setString(opticalGroup->getSummary<std::string, std::string>().c_str());
            theTreeContainerOpticalGroup->getSummary<StringContainer, StringContainer>().write();
  
            for(const auto hybrid : *opticalGroup)
            {
                auto* theTreeContainerHybrid = theTreeContainerOpticalGroup->getObject(hybrid->getId());
                theTreeContainerHybrid->getSummary<StringContainer, StringContainer>().setString(hybrid->getSummary<std::string, std::string>().c_str());
                theTreeContainerHybrid->getSummary<StringContainer, StringContainer>().write();

                for(const auto chip : *hybrid)
                {
                    auto* theTreeContainerChip = theTreeContainerHybrid->getObject(chip->getId());
                    theTreeContainerChip->getSummary<StringContainer, StringContainer>().setString(chip->getSummary<std::string, std::string>().c_str());
                    theTreeContainerChip->getSummary<StringContainer, StringContainer>().write();
                }   
            }   
        }
    }
}

void DQMMetadataTreeOT::process() {}

bool DQMMetadataTreeOT::fill(std::vector<char>& dataBuffer)
{
    std::string inputStream(dataBuffer.begin(), dataBuffer.end());

    ContainerSerialization theNameSerialization("MetadataOTObjectNames");
    
    if(theNameSerialization.attachDeserializer(inputStream))
    {
        std::cout << "Matched MetadataOT ObjectNames!!!!!\n";
        DetectorDataContainer theDetectorData = theNameSerialization.deserializeDetectorContainer<EmptyContainer, std::string, std::string, std::string, std::string, EmptyContainer>(fDetectorContainer);
        fillObjectNames(theDetectorData);
        return true;
    }
    return false;
}




