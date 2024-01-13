#include "Parser/FileDumper.h"
#include "Utils/Container.h"
#include "Utils/ConsoleColor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/lpGBT.h"
#include "HWDescription/OpticalGroup.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/ReadoutChip.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "Parser/ParserDefinitions.h"
#include "Parser/FileParser.h"

using namespace Ph2_HwDescription;

FileDumper::FileDumper(const std::string& outputDirectory)
{
    fOutputDirectory = std::string(getenv("PH2ACF_BASE_DIR")) + "/" + outputDirectory + "/";
}

FileDumper::~FileDumper(){}

void FileDumper::dumpConfigurationFiles(DetectorContainer* theDetectorContainer, const Ph2_Parser::SettingsMap& theSettingMap)
{
    pugi::xml_document doc;

    // Add a declaration node
    pugi::xml_node declarationNode               = doc.prepend_child(pugi::node_declaration);
    declarationNode.append_attribute("version")  = "1.0";
    declarationNode.append_attribute("encoding") = "utf-8";

    pugi::xml_node hwDescriptionNode = doc.append_child(HW_DESCRIPTION_NODE_NAME);

    // Fabio: CBC specific -> to be moved out from Tool
    for(auto board: *theDetectorContainer)
    {
        dumpBoardConfigurationFile(hwDescriptionNode, board);
    }

    pugi::xml_node theSettingMainNode = hwDescriptionNode.append_child(SETTINGS_NODE_NAME);
    for(const auto& theSetting : theSettingMap)
    {
        pugi::xml_node theSettingNode = theSettingMainNode.append_child(SETTING_NODE_NAME);
        theSettingNode.append_child(pugi::node_pcdata).set_value(std::to_string(boost::any_cast<double>(theSetting.second)).c_str());
        theSettingNode.append_attribute(COMMON_NAME_ATTRIBUTE_NAME) = theSetting.first.c_str();
    }

    std::string outputFileName = fOutputDirectory + "/Configuration.xml";

    if(doc.save_file(outputFileName.c_str())) { LOG(INFO) << BOLDBLUE << "XML file " << outputFileName << " created successfully." << std::endl; }
    else
    {
        LOG(ERROR) << BOLDRED << "Error saving XML file." << RESET;
    }
    LOG(INFO) << BOLDBLUE << "Configfiles for all Chips written to " << fOutputDirectory << RESET;
}

void FileDumper::dumpBoardConfigurationFile(pugi::xml_node theMotherNode, BeBoard* theBoard)
{
    pugi::xml_node theBoardNode = theMotherNode.append_child(BEBOARD_NODE_NAME);
    theBoardNode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(theBoard->getId()).c_str();

    auto theBoardTypeAttribute = theBoardNode.append_attribute(BEBOARD_TYPE_ATTRIBUTE_NAME);
    if(theBoard->getBoardType() == BoardType::D19C) theBoardTypeAttribute = BEBOARD_TYPE_ATTRIBUTE_D19C_VALUE;
    else if(theBoard->getBoardType() == BoardType::RD53) theBoardTypeAttribute = BEBOARD_TYPE_ATTRIBUTE_RD53_VALUE;
    else throw std::runtime_error("FileDumper error: BeBoard type not recognized");

    auto theEventTypeAttribute = theBoardNode.append_attribute(BEBOARD_EVENT_TYPE_ATTRIBUTE_NAME);
    if(theBoard->getEventType() == EventType::ZS) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_ZS_VALUE;
    else if(theBoard->getEventType() == EventType::SCAS) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_SCAS_VALUE;
    else if(theBoard->getEventType() == EventType::SSAAS) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_SSAAS_VALUE;
    else if(theBoard->getEventType() == EventType::MPAAS) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_MPAAS_VALUE;
    else if(theBoard->getEventType() == EventType::MPA) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_MPA_VALUE;
    else if(theBoard->getEventType() == EventType::SSA) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_SSA_VALUE;
    else if(theBoard->getEventType() == EventType::PSAS) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_PSAS_VALUE;
    else if(theBoard->getEventType() == EventType::VR2S) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_VR2S_VALUE;
    else if(theBoard->getEventType() == EventType::VR) theEventTypeAttribute = BEBOARD_EVENT_TYPE_ATTRIBUTE_VR_VALUE;
    else throw std::runtime_error("FileDumper error: Event type not recognized");

    theBoardNode.append_attribute(BEBOARD_LINKRESET_ATTRIBUTE_NAME) = (theBoard->getLinkReset() > 0) ? "1" : "0";
    theBoardNode.append_attribute(BEBOARD_BOARDRESET_ATTRIBUTE_NAME) = (theBoard->getReset() > 0) ? "1" : "0";
    theBoardNode.append_attribute(BEBOARD_CONFIGURE_ATTRIBUTE_NAME) = (theBoard->getToConfigure() > 0) ? "1" : "0";

    pugi::xml_node theBoardConnectionNode = theBoardNode.append_child(BEBOARD_CONNECTION_NODE_NAME);
    theBoardConnectionNode.append_attribute(BEBOARD_CONNECTION_ID_ATTRIBUTE_NAME) = theBoard->getConnectionId().c_str();
    theBoardConnectionNode.append_attribute(BEBOARD_CONNECTION_URI_ATTRIBUTE_NAME) = theBoard->getConnectionUri().c_str();
    theBoardConnectionNode.append_attribute(BEBOARD_CONNECTION_ADDRESS_TABLE_ATTRIBUTE_NAME) = theBoard->getAddressTable().c_str();

    //TODO: Correct board config file name after board register dump ir ready
    pugi::xml_node theBoardConfigurationNode = theBoardNode.append_child(BEBOARD_CONFIGURATION_NODE_NAME);
    theBoardConfigurationNode.append_attribute(BEBOARD_CONFIGURATION_FILE_NAME_ATTRIBUTE_NAME) = "/home/modtest/Programming/Ph2_ACF_Dev/settings/BeBoardFiles/uDTC_registers.xml";

    pugi::xml_node theBoardCDCENode = theBoardNode.append_child(BEBOARD_CDCE_NODE_NAME);
    theBoardCDCENode.append_attribute(BEBOARD_CDCE_CONFIGURE_ATTRIBUTE_NAME) = "0"; //Always forcing it to 0 to avoid overriding eprom too many times
    theBoardCDCENode.append_attribute(BEBOARD_CDCE_CLOCKRATE_ATTRIBUTE_NAME) = theBoard->configCDCE().second;

    for(auto opticalGroup: *theBoard)
    {
        dumpOpticalGroupConfigurationFile(theBoardNode, opticalGroup);
    }
}

void FileDumper::dumpOpticalGroupConfigurationFile(pugi::xml_node theMotherNode, OpticalGroup* theOpticalGroup)
{
    pugi::xml_node theOpticalGroupNode = theMotherNode.append_child(OPTICALGROUP_NODE_NAME);
    theOpticalGroupNode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(theOpticalGroup->getId()).c_str();
    auto theOpticalGroupFMCidAttribute = theOpticalGroupNode.append_attribute(OPTICALGROUP_FMCID_ATTRIBUTE_NAME);
    auto theFMCid = theOpticalGroup->getFMCId();
    if(theFMCid == 8) theOpticalGroupFMCidAttribute = OPTICALGROUP_FMCID_ATTRIBUTE_L8_VALUE;
    else if(theFMCid == 12) theOpticalGroupFMCidAttribute = OPTICALGROUP_FMCID_ATTRIBUTE_L12_VALUE;
    else throw std::runtime_error("FileDumper error: FMC Id not recognized");
    theOpticalGroupNode.append_attribute(COMMON_RESET_ATTRIBUTE_NAME) = (theOpticalGroup->getReset() > 0) ? "1" : "0";

    auto theNTCmap = theOpticalGroup->getNTCMap();
    for(auto& theNTC : theNTCmap)
    {
        auto theNTCptopertiesNode = theOpticalGroupNode.append_child(NTCPROPERTIES_NODE_NAME);
        theNTCptopertiesNode.append_attribute(NTCPROPERTIES_TYPE_ATTRIBUTE_NAME) = theNTC.first.c_str();
        theNTCptopertiesNode.append_attribute(NTCPROPERTIES_ADC_ATTRIBUTE_NAME)  = theNTC.second.first.c_str();
        theNTCptopertiesNode.append_attribute(NTCPROPERTIES_LOOKUPTABLE_ATTRIBUTE_NAME)  = theNTC.second.second.c_str();
    }

    auto& clpGBT = theOpticalGroup->flpGBT;
    if(clpGBT != nullptr)
    {
        std::string theLpGBTFilePathNodeName = std::string(LPGBT_NODE_NAME) + CHIP_FILES_APPEND_NODE_NAME;
        auto theLpGBTFilePathNode = theOpticalGroupNode.append_child(theLpGBTFilePathNodeName.c_str());
        theLpGBTFilePathNode.append_attribute(COMMON_PATH_ATTRIBUTE_NAME) = fOutputDirectory.c_str();

        auto theLpGBTNode = theOpticalGroupNode.append_child(LPGBT_NODE_NAME);
        theLpGBTNode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(clpGBT->getId()).c_str();
        theLpGBTNode.append_attribute(LPGBT_VERSION_ATTRIBUTE_NAME) = std::to_string(clpGBT->getVersion()).c_str();
        theLpGBTNode.append_attribute(LPGBT_OPTICAL_ATTRIBUTE_NAME) = clpGBT->isOptical() ? "1" : "0";

        auto cRegMap = clpGBT->getRegMap();

        std::string theFileName = "BE" + std::to_string(theOpticalGroup->getBeBoardId()) + "_OG" + std::to_string(theOpticalGroup->getId()) + "_lpGBT" + std::to_string(clpGBT->getId()) + ".txt";
        std::string theFullFileName = fOutputDirectory + theFileName;
        LOG(DEBUG) << BOLDBLUE << "Dumping lpgbt configuration to " << theFullFileName << RESET;
        clpGBT->saveRegMap(theFullFileName);

        theLpGBTNode.append_attribute(COMMON_CONFIGFILE_ATTRIBUTE_NAME) = theFileName.c_str();
    }

    for(auto hybrid: *theOpticalGroup)
    {
        dumpHybridConfigurationFile(theOpticalGroupNode, hybrid);
    }
}

void FileDumper::dumpHybridConfigurationFile(pugi::xml_node theMotherNode, Hybrid* theHybrid)
{
    pugi::xml_node theHybridNode = theMotherNode.append_child(HYBRID_NODE_NAME);
    theHybridNode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(theHybrid->getId()).c_str();
    theHybridNode.append_attribute(HYBRID_ENABLE_ATTRIBUTE_NAME) = "1"; //If it was disabled, it would not be here
    theHybridNode.append_attribute(COMMON_RESET_ATTRIBUTE_NAME) = (theHybrid->getReset() > 0) ? "1" : "0";

    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
    if(cCic != NULL)
    {
        std::string theCICFilePathNodeName = std::string(CIC_NODE_NAME) + CHIP_FILES_APPEND_NODE_NAME;
        auto theCICFilePathNode = theHybridNode.append_child(theCICFilePathNodeName.c_str());
        theCICFilePathNode.append_attribute(COMMON_PATH_ATTRIBUTE_NAME) = fOutputDirectory.c_str();

        std::string theFileName = "BE" + std::to_string(theHybrid->getBeBoardId()) + "_OG" + std::to_string(theHybrid->getOpticalGroupId()) + "_FE" + std::to_string(theHybrid->getId()) + "_CIC.txt";
        std::string theFullFileName = fOutputDirectory + theFileName;
        LOG(DEBUG) << BOLDBLUE << "Dumping CIC configuration to " << theFullFileName << RESET;
        cCic->saveRegMap(theFullFileName);

        std::string CicNodeName;
        if(cCic->getFrontEndType() == FrontEndType::CIC) CicNodeName = CIC_NODE_NAME;
        else if(cCic->getFrontEndType() == FrontEndType::CIC2) CicNodeName = CIC2_NODE_NAME;
        else throw std::runtime_error("FileDumper error: CIC version not recognized");
        auto theCICnode = theHybridNode.append_child(CicNodeName.c_str());
        theCICnode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(cCic->getId()).c_str();
        theCICnode.append_attribute(COMMON_CONFIGFILE_ATTRIBUTE_NAME) = theFileName.c_str();
    }

    bool cWithCBC  = (std::find_if(theHybrid->begin(), theHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::CBC3; }) != theHybrid->end());
    bool cWithMPA  = (std::find_if(theHybrid->begin(), theHybrid->end(), [](Ph2_HwDescription::Chip* x) { return (x->getFrontEndType() == FrontEndType::MPA) || (x->getFrontEndType() == FrontEndType::MPA2); }) != theHybrid->end());
    bool cWithSSA  = (std::find_if(theHybrid->begin(), theHybrid->end(), [](Ph2_HwDescription::Chip* x) { return (x->getFrontEndType() == FrontEndType::SSA) || (x->getFrontEndType() == FrontEndType::SSA2); }) != theHybrid->end());

    auto appendReadoutChipConfigFilePath = [this, &theHybridNode](std::string theChipString)
    {
        std::string theCICFilePathNodeName = theChipString + CHIP_FILES_APPEND_NODE_NAME;
        auto theCICFilePathNode = theHybridNode.append_child(theCICFilePathNodeName.c_str());
        theCICFilePathNode.append_attribute(COMMON_PATH_ATTRIBUTE_NAME) = fOutputDirectory.c_str();
    };

    if(cWithCBC) appendReadoutChipConfigFilePath(CBC_NODE_NAME);
    if(cWithMPA) appendReadoutChipConfigFilePath(MPA_NODE_NAME);
    if(cWithSSA) appendReadoutChipConfigFilePath(SSA_NODE_NAME);

    for(auto chip: *theHybrid)
    {
        dumpChipConfigurationFile(theHybridNode, chip);
    }
}

void FileDumper::dumpChipConfigurationFile(pugi::xml_node theMotherNode, ReadoutChip* theReadoutChip)
{
    std::string theReadoutChipNodeName;
    if(theReadoutChip->getFrontEndType() == FrontEndType::CBC3) theReadoutChipNodeName = CBC_NODE_NAME;
    else if(theReadoutChip->getFrontEndType() == FrontEndType::MPA) theReadoutChipNodeName = MPA_NODE_NAME;
    else if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2) theReadoutChipNodeName = MPA2_NODE_NAME;
    else if(theReadoutChip->getFrontEndType() == FrontEndType::SSA) theReadoutChipNodeName = SSA_NODE_NAME;
    else if(theReadoutChip->getFrontEndType() == FrontEndType::SSA2) theReadoutChipNodeName = SSA2_NODE_NAME;
    else throw std::runtime_error("FileDumper error: Readout chip type not recognized");

    std::string theFileName = "BE" + std::to_string(theReadoutChip->getBeBoardId()) + "_OG" + std::to_string(theReadoutChip->getOpticalGroupId()) + "_FE" + convertToString(theReadoutChip->getHybridId()) +
                            "_Chip" + convertToString(theReadoutChip->getId());
    if(theReadoutChip->getFrontEndType() == FrontEndType::SSA || theReadoutChip->getFrontEndType() == FrontEndType::SSA2) theFileName += "SSA";
    if(theReadoutChip->getFrontEndType() == FrontEndType::MPA || theReadoutChip->getFrontEndType() == FrontEndType::MPA2) theFileName += "MPA";
    theFileName += ".txt";
    std::string theFullFileName = fOutputDirectory + theFileName;
    LOG(DEBUG) << BOLDBLUE << "Dumping readout chip configuration to " << theFileName << RESET;
    theReadoutChip->saveRegMap(theFullFileName);

    pugi::xml_node theReadoutChipNode = theMotherNode.append_child(theReadoutChipNodeName.c_str());
    theReadoutChipNode.append_attribute(COMMON_ID_ATTRIBUTE_NAME) = std::to_string(theReadoutChip->getId()).c_str();
    theReadoutChipNode.append_attribute(COMMON_CONFIGFILE_ATTRIBUTE_NAME) = theFileName.c_str();
}