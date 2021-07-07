#include "FileParser.h"
#include "../HWDescription/Cbc.h"
#include "../HWDescription/Cic.h"
#include "../HWDescription/Hybrid.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWDescription/RD53.h"
#include "../HWDescription/lpGBT.h"
#include "../Utils/Utilities.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace Ph2_System
{
void FileParser::parseHW(const std::string& pFilename, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os, bool pIsFile)
{
    if(pIsFile && pFilename.find(".xml") != std::string::npos) { parseHWxml(pFilename, pBeBoardFWMap, pDetectorContainer, os, pIsFile); }
    else if(!pIsFile)
    {
        parseHWxml(pFilename, pBeBoardFWMap, pDetectorContainer, os, pIsFile);
    }
    else
    {
        LOG(ERROR) << BOLDRED << "Could not parse settings file " << pFilename << " - it is not .xml" << RESET;
    }
}

void FileParser::parseSettings(const std::string& pFilename, SettingsMap& pSettingsMap, std::ostream& os, bool pIsFile)
{
    if((pIsFile && pFilename.find(".xml") != std::string::npos) || (!pIsFile))
        parseSettingsxml(pFilename, pSettingsMap, os, pIsFile);
    else
        LOG(ERROR) << BOLDRED << "Could not parse settings file " << pFilename << " - it is not .xml" << RESET;
}

void FileParser::parseHWxml(const std::string& pFilename, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os, bool pIsFile)
{
    int i, j;

    pugi::xml_document     doc;
    pugi::xml_parse_result result;

    if(pIsFile)
        result = doc.load_file(pFilename.c_str());
    else
        result = doc.load(pFilename.c_str());

    if(!result)
    {
        os << BOLDRED << "ERROR :\n Unable to open the file : " << RESET << pFilename << std::endl;
        os << BOLDRED << "Error description : " << RED << result.description() << RESET << std::endl;

        if(!pIsFile) os << "Error offset: " << result.offset << " (error at [..." << (pFilename.c_str() + result.offset) << "]\n" << std::endl;

        throw Exception("Unable to parse XML source!");
        return;
    }

    os << RESET << "\n\n";

    for(i = 0; i < 80; i++) os << "*";
    os << "\n";

    for(j = 0; j < 35; j++) os << " ";
    os << BOLDRED << "HW SUMMARY" << RESET << std::endl;

    for(i = 0; i < 80; i++) os << "*";
    os << "\n";

    const std::string strUhalConfig = expandEnvironmentVariables(doc.child("HwDescription").child("Connections").attribute("name").value());

    // Iterate over the BeBoard Nodes
    for(pugi::xml_node cBeBoardNode = doc.child("HwDescription").child("BeBoard"); cBeBoardNode; cBeBoardNode = cBeBoardNode.next_sibling())
    {
        if(static_cast<std::string>(cBeBoardNode.name()) == "BeBoard") { this->parseBeBoard(cBeBoardNode, pBeBoardFWMap, pDetectorContainer, os); }
    }

    for(i = 0; i < 80; i++) os << "*";

    os << "\n";

    for(j = 0; j < 32; j++) os << " ";

    os << BOLDRED << "END OF HW SUMMARY" << RESET << std::endl;

    for(i = 0; i < 80; i++) os << "*";

    os << std::endl;
}

void FileParser::parseBeBoard(pugi::xml_node pBeBordNode, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os)
{
    uint32_t cBeId    = pBeBordNode.attribute("Id").as_int();
    BeBoard* cBeBoard = pDetectorContainer->addBoardContainer(cBeId, new BeBoard(cBeId)); // FIX Change it to Reference!!!!

    pugi::xml_attribute cBoardTypeAttribute = pBeBordNode.attribute("boardType");

    if(cBoardTypeAttribute == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Error: Board Type not specified - aborting!" << RESET;
        exit(EXIT_FAILURE);
    }

    std::string cBoardType = cBoardTypeAttribute.value();

    bool     cConfigureCDCE = false;
    uint32_t cClockRateCDCE = 120;
    for(pugi::xml_node cChild: pBeBordNode.children("CDCE"))
    {
        for(pugi::xml_attribute cAttribute: cChild.attributes())
        {
            if(std::string(cAttribute.name()) == "configure") cConfigureCDCE = cConfigureCDCE | (convertAnyInt(cAttribute.value()) == 1);
            if(std::string(cAttribute.name()) == "clockRate") cClockRateCDCE = convertAnyInt(cAttribute.value());
        }
    }
    cBeBoard->setCDCEconfiguration(cConfigureCDCE, cClockRateCDCE);

    if(cBoardType == "D19C")
        cBeBoard->setBoardType(BoardType::D19C);
    else if(cBoardType == "RD53")
        cBeBoard->setBoardType(BoardType::RD53);
    else
    {
        LOG(ERROR) << BOLDRED << "Error: Unknown Board Type: " << cBoardType << " - aborting!" << RESET;
        std::string errorstring = "Unknown Board Type " + cBoardType;
        throw Exception(errorstring.c_str());
        exit(EXIT_FAILURE);
    }

    pugi::xml_attribute cEventTypeAttribute = pBeBordNode.attribute("eventType");
    std::string         cEventTypeString;

    if(cEventTypeAttribute == nullptr)
    {
        cBeBoard->setEventType(EventType::VR);
        cEventTypeString = "VR";
    }
    else
    {
        cEventTypeString = cEventTypeAttribute.value();
        if(cEventTypeString == "ZS")
            cBeBoard->setEventType(EventType::ZS);
        else if(cEventTypeString == "Async")
            cBeBoard->setEventType(EventType::SSAAS);
        else if(cEventTypeString == "MPAAS")
            cBeBoard->setEventType(EventType::MPAAS);
        else
            cBeBoard->setEventType(EventType::VR);
    }

    os << BOLDBLUE << "|"
       << "----" << pBeBordNode.name() << " --> " << pBeBordNode.first_attribute().name() << ": " << BOLDYELLOW << pBeBordNode.attribute("Id").value() << BOLDBLUE << ", BoardType: " << BOLDYELLOW
       << cBoardType << BOLDBLUE << ", EventType: " << BOLDYELLOW << cEventTypeString << RESET << std::endl;

    pugi::xml_node cBeBoardConnectionNode = pBeBordNode.child("connection");

    std::string cId           = cBeBoardConnectionNode.attribute("id").value();
    std::string cUri          = cBeBoardConnectionNode.attribute("uri").value();
    std::string cAddressTable = expandEnvironmentVariables(cBeBoardConnectionNode.attribute("address_table").value());

    if(cBeBoard->getBoardType() == BoardType::D19C) { pBeBoardFWMap[cBeBoard->getId()] = new D19cFWInterface(cId.c_str(), cUri.c_str(), cAddressTable.c_str()); }
    else if(cBeBoard->getBoardType() == BoardType::RD53)
        pBeBoardFWMap[cBeBoard->getId()] = new RD53FWInterface(cId.c_str(), cUri.c_str(), cAddressTable.c_str());

    os << BOLDCYAN << "|"
       << "       "
       << "|"
       << "----"
       << "Board Id:      " << BOLDYELLOW << cId << std::endl
       << BOLDCYAN << "|"
       << "       "
       << "|"
       << "----"
       << "URI:           " << BOLDYELLOW << cUri << std::endl
       << BOLDCYAN << "|"
       << "       "
       << "|"
       << "----"
       << "Address Table: " << BOLDYELLOW << cAddressTable << std::endl
       << RESET;

    for(pugi::xml_node cBeBoardRegNode = pBeBordNode.child("Register"); cBeBoardRegNode; cBeBoardRegNode = cBeBoardRegNode.next_sibling())
    {
        if(std::string(cBeBoardRegNode.name()) == "Register")
        {
            std::string cNameString;
            double      cValue;
            this->parseRegister(cBeBoardRegNode, cNameString, cValue, cBeBoard, os);
        }
    }

    // Iterate the OpticalGroup node
    bool cWithOptical = false;
    for(pugi::xml_node pOpticalGroupNode = pBeBordNode.child("OpticalGroup"); pOpticalGroupNode; pOpticalGroupNode = pOpticalGroupNode.next_sibling())
    {
        if(static_cast<std::string>(pOpticalGroupNode.name()) == "OpticalGroup")
        {
            this->parseOpticalGroupContainer(pOpticalGroupNode, cBeBoard, os);

            for(pugi::xml_node cChild: pOpticalGroupNode.children("GBT"))
            {
                std::string cName = cChild.name();
                os << BOLDBLUE << "|"
                   << "----" << cName << "\n"
                   << RESET;
                uint8_t cGBTId = 0;
                for(pugi::xml_attribute cAttribute: cChild.attributes())
                {
                    if(std::string(cAttribute.name()) == "enable") cWithOptical = cWithOptical | (convertAnyInt(cAttribute.value()) == true);

                    if(std::string(cAttribute.name()) == "Id") // T.B.D store this somewhere...but where
                    {
                        cGBTId = convertAnyInt(cAttribute.value());
                        os << BOLDBLUE << "|"
                           << "       "
                           << "|"
                           << "----"
                           << " GBT :      " << BOLDYELLOW << +cGBTId << std::endl
                           << RESET;
                    }
                    if(cBeBoard->getBoardType() == BoardType::D19C)
                    {
                        if(std::string(cAttribute.name()) == "phaseTap") // T.B.D store this somewhere...but where
                        {
                            static_cast<D19cFWInterface*>(pBeBoardFWMap[cBeBoard->getId()])->setGBTxPhase(convertAnyInt(cAttribute.value()));
                            os << BOLDBLUE << "\t|"
                               << "       "
                               << "|"
                               << "----"
                               << " phase is :      " << BOLDYELLOW << cAttribute.value() << std::endl
                               << RESET;
                        }
                        if(std::string(cAttribute.name()) == "txPolarity") // T.B.D store this somewhere...but where
                        {
                            auto cPolarity = convertAnyInt(cAttribute.value());
                            static_cast<D19cFWInterface*>(pBeBoardFWMap[cBeBoard->getId()])->setTxPolarity(cGBTId, cPolarity);
                            os << BOLDBLUE << "\t|"
                               << "       "
                               << "|"
                               << "----"
                               << " Tx Polarity is :      " << BOLDYELLOW << cAttribute.value() << std::endl
                               << RESET;
                        }
                        if(std::string(cAttribute.name()) == "rxPolarity") // T.B.D store this somewhere...but where
                        {
                            auto cPolarity = convertAnyInt(cAttribute.value());
                            static_cast<D19cFWInterface*>(pBeBoardFWMap[cBeBoard->getId()])->setRxPolarity(cGBTId, cPolarity);
                            os << BOLDBLUE << "\t|"
                               << "       "
                               << "|"
                               << "----"
                               << " Rx Polairty is :      " << BOLDYELLOW << cAttribute.value() << std::endl
                               << RESET;
                        }
                    }
                }
            }
        }
    }

    cBeBoard->setOptical(cWithOptical);

    pugi::xml_node cSLinkNode = pBeBordNode.child("SLink");
    this->parseSLink(cSLinkNode, cBeBoard, os);
}

void FileParser::parseOpticalGroupContainer(pugi::xml_node pOpticalGroupNode, BeBoard* pBoard, std::ostream& os)
{
    std::string   cFilePath       = "";
    uint32_t      cOpticalGroupId = pOpticalGroupNode.attribute("Id").as_int();
    uint32_t      cFMCId          = pOpticalGroupNode.attribute("FMCId").as_int();
    uint32_t      cBoardId        = pBoard->getId();
    OpticalGroup* theOpticalGroup = pBoard->addOpticalGroupContainer(cBoardId, new OpticalGroup(cBoardId, cFMCId, cOpticalGroupId));

    for(pugi::xml_node theChild: pOpticalGroupNode.children())
    {
        if(static_cast<std::string>(theChild.name()) == "Hybrid") { this->parseHybridContainer(theChild, theOpticalGroup, os, pBoard); }
        else if(static_cast<std::string>(theChild.name()) == "lpGBT_Interface")
        {
            pBoard->setUseOpticalLink(convertAnyInt(theChild.attribute("useOpticalLink").value()));
            pBoard->setUseCPB(convertAnyInt(theChild.attribute("useCPB").value()));
        }
        else if(static_cast<std::string>(theChild.name()) == "lpGBT_Files")
        {
            cFilePath = expandEnvironmentVariables(theChild.attribute("path").value());
            if((cFilePath.empty() == false) && (cFilePath.at(cFilePath.length() - 1) != '/')) cFilePath.append("/");
        }
        else if(static_cast<std::string>(theChild.name()) == "lpGBT")
        {
            std::string fileName = cFilePath + expandEnvironmentVariables(theChild.attribute("configfile").value());
            os << BOLDBLUE << "|\t|----" << theChild.name() << " --> File: " << BOLDYELLOW << fileName << RESET << std::endl;
            lpGBT* thelpGBT = new lpGBT(cBoardId, cFMCId, cOpticalGroupId, fileName);
            theOpticalGroup->addlpGBT(thelpGBT);

            // Initialize LpGBT settings from XML (only for IT)
            if(pBoard->getBoardType() == BoardType::RD53)
            {
                for(const pugi::xml_attribute& attr: theChild.attributes())
                {
                    os << BOLDBLUE << "|\t|\t|---- " << attr.name() << ": " << BOLDYELLOW << attr.value() << "\n" << RESET;
                    if(std::string(attr.name()) == "RxHSLPolarity")
                        thelpGBT->setRxHSLPolarity(convertAnyInt(theChild.attribute("RxHSLPolarity").value()));
                    else if(std::string(attr.name()) == "TxHSLPolarity")
                        thelpGBT->setTxHSLPolarity(convertAnyInt(theChild.attribute("TxHSLPolarity").value()));
                    else if(std::string(attr.name()) == "RxDataRate")
                        thelpGBT->setRxDataRate(convertAnyInt(theChild.attribute("RxDataRate").value()));
                    else if(std::string(attr.name()) == "TxDataRate")
                        thelpGBT->setTxDataRate(convertAnyInt(theChild.attribute("TxDataRate").value()));
                    else if(std::string(attr.name()) == "ClockFrequency")
                        thelpGBT->setClocksFrequency(convertAnyInt(theChild.attribute("ClockFrequency").value()));
                }
            }

            pugi::xml_node clpGBTSettings = theChild.child("Settings");
            if(clpGBTSettings != nullptr)
            {
                os << BOLDCYAN << "|\t|\t|----LpGBT settings" << RESET << std::endl;

                for(const pugi::xml_attribute& attr: clpGBTSettings.attributes())
                {
                    std::string regname  = attr.name();
                    uint16_t    regvalue = convertAnyInt(attr.value());
                    thelpGBT->setReg(regname, regvalue, true);
                    os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;
                }
            }
        }
    }
}

void FileParser::parseRegister(pugi::xml_node pRegisterNode, std::string& pAttributeString, double& pValue, BeBoard* pBoard, std::ostream& os)
{
    if(std::string(pRegisterNode.name()) == "Register")
    {
        if(std::string(pRegisterNode.first_child().value()).empty())
        {
            if(!pAttributeString.empty()) pAttributeString += ".";

            pAttributeString += pRegisterNode.attribute("name").value();

            for(pugi::xml_node cNode = pRegisterNode.child("Register"); cNode; cNode = cNode.next_sibling())
            {
                std::string cAttributeString = pAttributeString;
                this->parseRegister(cNode, cAttributeString, pValue, pBoard, os);
            }
        }
        else
        {
            if(!pAttributeString.empty()) pAttributeString += ".";

            pAttributeString += pRegisterNode.attribute("name").value();
            pValue = convertAnyDouble(pRegisterNode.first_child().value());
            os << GREEN << "|\t|\t|"
               << "----" << pAttributeString << ": " << BOLDYELLOW << pValue << RESET << std::endl;
            pBoard->setReg(pAttributeString, pValue);
        }
    }
}

void FileParser::parseSLink(pugi::xml_node pSLinkNode, BeBoard* pBoard, std::ostream& os)
{
    ConditionDataSet* cSet = new ConditionDataSet();

    if(pSLinkNode != nullptr && std::string(pSLinkNode.name()) == "SLink")
    {
        os << BLUE << "|"
           << "  "
           << "|" << std::endl
           << "|"
           << "   "
           << "|"
           << "----" << pSLinkNode.name() << RESET << std::endl;

        pugi::xml_node cDebugModeNode = pSLinkNode.child("DebugMode");
        std::string    cDebugString;

        // the debug mode node exists
        if(cDebugModeNode != nullptr)
        {
            cDebugString = cDebugModeNode.attribute("type").value();

            if(cDebugString == "FULL")
                cSet->setDebugMode(SLinkDebugMode::FULL);
            else if(cDebugString == "SUMMARY")
                cSet->setDebugMode(SLinkDebugMode::SUMMARY);
            else if(cDebugString == "ERROR")
                cSet->setDebugMode(SLinkDebugMode::ERROR);
        }
        else
        {
            SLinkDebugMode pMode = cSet->getDebugMode();

            if(pMode == SLinkDebugMode::FULL)
                cDebugString = "FULL";
            else if(pMode == SLinkDebugMode::SUMMARY)
                cDebugString = "SUMMARY";
            else if(pMode == SLinkDebugMode::ERROR)
                cDebugString = "ERROR";
        }

        os << BLUE << "|"
           << " "
           << "|"
           << "       "
           << "|"
           << "----" << pSLinkNode.child("DebugMode").name() << MAGENTA << " : SLinkDebugMode::" << cDebugString << RESET << std::endl;

        // now loop the condition data node
        for(pugi::xml_node cNode = pSLinkNode.child("ConditionData"); cNode; cNode = cNode.next_sibling())
        {
            if(cNode != nullptr)
            {
                uint8_t     cUID     = 0;
                uint8_t     cFeId    = 0;
                uint8_t     cCbcId   = 0;
                uint8_t     cPage    = 0;
                uint8_t     cAddress = 0;
                uint32_t    cValue   = 0;
                std::string cRegName;

                std::string cTypeString = cNode.attribute("type").value();

                if(cTypeString == "HV")
                {
                    cUID   = 5;
                    cFeId  = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId = convertAnyInt(cNode.attribute("Sensor").value());
                    cValue = convertAnyInt(cNode.first_child().value());
                }
                else if(cTypeString == "TDC")
                {
                    cUID  = 3;
                    cFeId = 0xFF;
                }
                else if(cTypeString == "User")
                {
                    cUID   = convertAnyInt(cNode.attribute("UID").value());
                    cFeId  = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId = convertAnyInt(cNode.attribute("CbcId").value());
                    cValue = convertAnyInt(cNode.first_child().value());
                }
                else if(cTypeString == "I2C")
                {
                    // here is where it gets nasty
                    cUID     = 1;
                    cRegName = cNode.attribute("Register").value();
                    cFeId    = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId   = convertAnyInt(cNode.attribute("CbcId").value());

                    // ok, now I need to loop th CBCs to find page & address and the initial value

                    for(auto cOpticalGroup: *pBoard)
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            if(cHybrid->getId() != cFeId) continue;

                            for(auto cCbc: *cHybrid)
                            {
                                if(cCbc->getId() != cCbcId)
                                    continue;
                                else if(cHybrid->getId() == cFeId && cCbc->getId() == cCbcId)
                                {
                                    ChipRegItem cRegItem = static_cast<ReadoutChip*>(cCbc)->getRegItem(cRegName);
                                    cPage                = cRegItem.fPage;
                                    cAddress             = cRegItem.fAddress;
                                    cValue               = cRegItem.fValue;
                                }
                                else
                                    LOG(ERROR) << BOLDRED << "SLINK ERROR: no Chip with Id " << +cCbcId << " on Fe " << +cFeId << " - check your SLink Settings!" << RESET;
                            }
                        }
                }

                cSet->addCondData(cRegName, cUID, cFeId, cCbcId, cPage, cAddress, cValue);
                os << BLUE << "|"
                   << " "
                   << "|"
                   << "       "
                   << "|"
                   << "----" << cNode.name() << ": Type " << RED << cTypeString << " " << cRegName << BLUE << ", UID " << RED << +cUID << BLUE << ", FeId " << RED << +cFeId << BLUE << ", CbcId "
                   << RED << +cCbcId << std::hex << BLUE << ", Page " << RED << +cPage << BLUE << ", Address " << RED << +cAddress << BLUE << ", Value " << std::dec << MAGENTA << cValue << RESET
                   << std::endl;
            }
        }
    }

    pBoard->addConditionDataSet(cSet);
}

void FileParser::parseSSAContainer(pugi::xml_node pSSAnode, Hybrid* pHybrid, std::string cFilePrefix, std::ostream& os)
{
    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "----" << pSSAnode.name() << "  " << pSSAnode.first_attribute().name() << " :" << pSSAnode.attribute("Id").value()
       << ", File: " << expandEnvironmentVariables(pSSAnode.attribute("configfile").value()) << RESET << std::endl;

    // Get ID of SSA then add to the Hybrid!
    uint32_t    cChipId    = pSSAnode.attribute("Id").as_int();
    uint32_t    cPartnerId = pSSAnode.attribute("partid").as_int();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    ReadoutChip* cSSA = pHybrid->addChipContainer(cChipId, new SSA(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getId(), cChipId, cPartnerId, 0, cFileName));
    cSSA->setNumberOfChannels(120);
    this->parseSSASettings(pSSAnode, cSSA);
}

void FileParser::parseSSASettings(pugi::xml_node pHybridNode, ReadoutChip* pSSA)
{
    // FrontEndType cType = pSSA->getFrontEndType();
}

void FileParser::parseMPA(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::string cFilePrefix)
{ // Get ID of MPA then add to the Hybrid!
    uint32_t    cChipId    = pHybridNode.attribute("Id").as_int();
    uint32_t    cPartnerId = pHybridNode.attribute("partid").as_int();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pHybridNode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pHybridNode.attribute("configfile").value());
    ReadoutChip* cMPA = pHybrid->addChipContainer(cChipId, new MPA(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getId(), cChipId, cPartnerId, cFileName));
    cMPA->setNumberOfChannels(1920);
    this->parseMPASettings(pHybridNode, cMPA);
}

void FileParser::parseMPASettings(pugi::xml_node pHybridNode, ReadoutChip* pMPA)
{
    // FrontEndType cType = pMPA->getFrontEndType();
}

void FileParser::parseHybridContainer(pugi::xml_node pHybridNode, OpticalGroup* pOpticalGroup, std::ostream& os, BeBoard* pBoard)
{
    bool cStatus = pHybridNode.attribute("Status").as_bool();

    if(cStatus)
    {
        os << BOLDBLUE << "|       |"
           << "----" << pHybridNode.name() << " --> " << BOLDBLUE << pHybridNode.first_attribute().name() << ": " << BOLDYELLOW << pHybridNode.attribute("Id").value() << BOLDBLUE
           << ", Status: " << BOLDYELLOW << expandEnvironmentVariables(pHybridNode.attribute("Status").value()) << BOLDBLUE << RESET << std::endl;

        Hybrid* cHybrid;
        if(pBoard->getBoardType() == BoardType::RD53)
        {
            cHybrid = pOpticalGroup->addHybridContainer(
                pHybridNode.attribute("Id").as_int(), new Hybrid(pOpticalGroup->getBeBoardId(), pOpticalGroup->getFMCId(), pHybridNode.attribute("Id").as_int(), pHybridNode.attribute("Id").as_int()));
        }
        else
        {
            cHybrid = pOpticalGroup->addHybridContainer(
                pHybridNode.attribute("Id").as_int(),
                new OuterTrackerHybrid(pOpticalGroup->getBeBoardId(), pOpticalGroup->getFMCId(), pHybridNode.attribute("Id").as_int(), pHybridNode.attribute("Id").as_int()));
            static_cast<OuterTrackerHybrid*>(cHybrid)->setLinkId(pHybridNode.attribute("LinkId").as_int());
        }

        std::string cConfigFileDirectory;
        for(pugi::xml_node cChild: pHybridNode.children())
        {
            std::string cName          = cChild.name();
            std::string cNextName      = cChild.next_sibling().name();
            bool        cIsTrackerASIC = cName.find("CBC") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("SSA") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("MPA") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("CIC") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("RD53") != std::string::npos;

            if(cIsTrackerASIC)
            {
                if(cName.find("_Files") != std::string::npos) { cConfigFileDirectory = expandEnvironmentVariables(static_cast<std::string>(cChild.attribute("path").value())); }
                else
                {
                    int         cChipId   = cChild.attribute("Id").as_int();
                    std::string cFileName = expandEnvironmentVariables(static_cast<std::string>(cChild.attribute("configfile").value()));

                    if(cName.find("RD53") != std::string::npos)
                    {
                        pBoard->setFrontEndType(FrontEndType::RD53);
                        this->parseRD53(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) this->parseGlobalRD53Settings(pHybridNode, cHybrid, os);
                    }
                    else if(cName.find("CBC") != std::string::npos)
                    {
                        pBoard->setFrontEndType(FrontEndType::CBC3);
                        this->parseCbcContainer(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) this->parseGlobalCbcSettings(pHybridNode, cHybrid, os);
                    }
                    else if(cName.find("CIC") != std::string::npos)
                    {
                        bool         cCIC1 = (cName.find("CIC2") == std::string::npos);
                        FrontEndType cType = cCIC1 ? FrontEndType::CIC : FrontEndType::CIC2;
                        pBoard->setFrontEndType(cType);
                        if(!cConfigFileDirectory.empty())
                        {
                            if(cConfigFileDirectory.at(cConfigFileDirectory.length() - 1) != '/') cConfigFileDirectory.append("/");

                            cFileName = cConfigFileDirectory + cFileName;
                        }
                        LOG(INFO) << BOLDBLUE << "Loading configuration for CIC from " << cFileName << RESET;
                        os << BOLDCYAN << "|"
                           << "  "
                           << "|"
                           << "   "
                           << "|"
                           << "----" << cName << "  "
                           << "Id" << cChipId << " , File: " << cFileName << RESET << std::endl;
                        Cic* cCic = new Cic(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getId(), cChipId, cFileName);
                        static_cast<OuterTrackerHybrid*>(cHybrid)->addCic(cCic);
                        cCic->setFrontEndType(cType);

                        os << GREEN << "|\t|\t|\t|----FrontEndType: ";
                        if(cType == FrontEndType::CIC)
                            os << RED << "CIC";
                        else
                            os << RED << "CIC2";

                        os << RESET << std::endl;
                        // Now global settings
                        pugi::xml_node cGlobalSettingsNode = pHybridNode.child("Global");
                        for(pugi::xml_node cChildGlobal: cGlobalSettingsNode.children())
                        {
                            std::string cNameGlobal = cChildGlobal.name();
                            if(cNameGlobal.find("CIC") != std::string::npos || cNameGlobal.find("CIC2") != std::string::npos)
                            {
                                LOG(INFO) << BOLDBLUE << " Global settings " << cNameGlobal << RESET;
                                std::vector<std::string> cAttributes{"clockFrequency", "enableBend", "enableLastLine", "enableSparsification"};
                                std::vector<std::string> cRegNames{"", "BEND_SEL", "N_OUTPUT_TRIGGER_LINES_SEL", "CBC_SPARSIFICATION_SEL"};
                                std::vector<uint16_t>    cBitPositions{1, 2, 3, 4};
                                for(auto it = cRegNames.begin(); it != cRegNames.end(); ++it)
                                {
                                    auto     cIndex       = std::distance(cRegNames.begin(), it);
                                    auto     cAttribute   = cAttributes[cIndex];
                                    auto     cBitPosition = cBitPositions[cIndex];
                                    uint16_t cMask        = (~(1 << cBitPosition)) & 0xFF;

                                    uint16_t cValueFromFile = cChildGlobal.attribute(cAttribute.c_str()).as_int();
                                    if(cAttribute == "clockFrequency") cValueFromFile = (cValueFromFile == 320) ? 0 : 1;
                                    if(cAttribute == "clockFrequency" && cCIC1) continue;
                                    if(cAttribute == "enableSparsification") pBoard->setSparsification(bool(cValueFromFile));

                                    os << GREEN << "|\t|\t|\t|---- Setting " << cAttribute << " to  " << cValueFromFile << "\n" << RESET;
                                    LOG(DEBUG) << BOLDBLUE << " Global settings " << cAttribute << " [ " << *it << " ]-- set to " << cValueFromFile << RESET;

                                    std::string cRegName  = cCIC1 ? std::string(*it) : "FE_CONFIG";
                                    auto        cRegValue = cCic->getReg(cRegName);
                                    uint16_t    cNewValue = cCIC1 ? cValueFromFile : ((cRegValue & cMask) | (cValueFromFile << cBitPosition));

                                    LOG(INFO) << BOLDBLUE << "  Setting [ " << cRegName << " " << *it << " == " << +cValueFromFile << "]-- set to. Mask " << std::bitset<5>(cMask) << " -- old value "
                                              << std::bitset<5>(cRegValue) << " -- new value " << std::bitset<5>(cNewValue) << RESET;
                                    cCic->setReg(cRegName, cNewValue);
                                }
                            }
                        }
                    }
                    else if(cName == "SSA")
                    {
                        pBoard->setFrontEndType(FrontEndType::SSA);
                        this->parseSSAContainer(cChild, cHybrid, cConfigFileDirectory, os);
                    }
                    else if(cName == "MPA")
                    {
                        pBoard->setFrontEndType(FrontEndType::MPA);
                        this->parseMPA(cChild, cHybrid, cConfigFileDirectory);
                    }
                }
            }
        }

        // Finally map front-end to LpGBT
        if(pBoard->getBoardType() == BoardType::RD53 && pOpticalGroup->flpGBT != nullptr) this->parseHybridToLpGBT(pHybridNode, cHybrid, pOpticalGroup->flpGBT, os);
    }
}

void FileParser::parseCbcContainer(pugi::xml_node pCbcNode, Hybrid* cHybrid, std::string cFilePrefix, std::ostream& os)
{
    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "----" << pCbcNode.name() << "  " << pCbcNode.first_attribute().name() << " :" << pCbcNode.attribute("Id").value()
       << ", File: " << expandEnvironmentVariables(pCbcNode.attribute("configfile").value()) << RESET << std::endl;

    std::string cFileName;

    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pCbcNode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pCbcNode.attribute("configfile").value());

    uint32_t     cChipId = pCbcNode.attribute("Id").as_int();
    ReadoutChip* cCbc    = cHybrid->addChipContainer(cChipId, new Cbc(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getId(), cChipId, cFileName));
    cCbc->setNumberOfChannels(254);

    // parse the specific CBC settings so that Registers take precedence
    this->parseCbcSettings(pCbcNode, cCbc, os);

    for(pugi::xml_node cCbcRegisterNode = pCbcNode.child("Register"); cCbcRegisterNode; cCbcRegisterNode = cCbcRegisterNode.next_sibling())
    {
        cCbc->setReg(std::string(cCbcRegisterNode.attribute("name").value()), convertAnyInt(cCbcRegisterNode.first_child().value()));
        os << BLUE << "|\t|\t|\t|----Register: " << std::string(cCbcRegisterNode.attribute("name").value()) << " : " << RED << std::hex << "0x" << convertAnyInt(cCbcRegisterNode.first_child().value())
           << RESET << std::dec << std::endl;
    }
}

void FileParser::parseGlobalCbcSettings(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    LOG(INFO) << BOLDBLUE << "Now I'm parsing global..." << RESET;
    // use this to parse GlobalCBCRegisters and the Global CBC settings
    // i deliberately pass the Hybrid object so I can loop the CBCs of the Hybrid inside this method
    // this has to be called at the end of the parseCBC() method
    // Global_CBC_Register takes precedence over Global
    pugi::xml_node cGlobalCbcSettingsNode = pHybridNode.child("Global");

    if(cGlobalCbcSettingsNode != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----Global CBC Settings: " << RESET << std::endl;

        int cCounter = 0;

        for(auto cCbc: *pHybrid)
        {
            if(cCounter == 0)
                this->parseCbcSettings(cGlobalCbcSettingsNode, static_cast<ReadoutChip*>(cCbc), os);
            else
            {
                std::ofstream cDummy;
                this->parseCbcSettings(cGlobalCbcSettingsNode, static_cast<ReadoutChip*>(cCbc), cDummy);
            }

            cCounter++;
        }
    }

    // now that global has been applied to each CBC, handle the GlobalCBCRegisters
    for(pugi::xml_node cCbcGlobalNode = pHybridNode.child("Global_CBC_Register");
        cCbcGlobalNode != pHybridNode.child("CBC") && cCbcGlobalNode != pHybridNode.child("CBC_Files") && cCbcGlobalNode != nullptr;
        cCbcGlobalNode = cCbcGlobalNode.next_sibling())
    {
        if(cCbcGlobalNode != nullptr)
        {
            std::string regname  = std::string(cCbcGlobalNode.attribute("name").value());
            uint32_t    regvalue = convertAnyInt(cCbcGlobalNode.first_child().value());

            for(auto cCbc: *pHybrid) static_cast<ReadoutChip*>(cCbc)->setReg(regname, uint8_t(regvalue));

            os << BOLDGREEN << "|"
               << " "
               << "|"
               << "   "
               << "|"
               << "----" << cCbcGlobalNode.name() << "  " << cCbcGlobalNode.first_attribute().name() << " :" << regname << " =  0x" << std::hex << std::setw(2) << std::setfill('0') << RED << regvalue
               << std::dec << RESET << std::endl;
        }
    }
}

void FileParser::parseCbcSettings(pugi::xml_node pCbcNode, ReadoutChip* pCbc, std::ostream& os)
{
    // parse the cbc settings here and put them in the corresponding registers of the Chip object
    // call this for every CBC, Register nodes should take precedence over specific settings??
    FrontEndType cType = pCbc->getFrontEndType();
    os << GREEN << "|\t|\t|\t|----FrontEndType: ";
    os << GREEN << "|\t|\t|\t|----FrontEndType: ";

    if(cType == FrontEndType::CBC3) os << RED << "CBC3";

    os << RESET << std::endl;

    // THRESHOLD & LATENCY
    pugi::xml_node cThresholdNode = pCbcNode.child("Settings");

    if(cThresholdNode != nullptr)
    {
        uint16_t cThreshold  = convertAnyInt(cThresholdNode.attribute("threshold").value());
        bool     cSetLatency = (cThresholdNode.attribute("latency") != nullptr);
        uint16_t cLatency    = convertAnyInt(cThresholdNode.attribute("latency").value());

        // the moment the cbc object is constructed, it knows which chip type it is
        if(cType == FrontEndType::CBC3)
        {
            // for beam test ... remove for now
            pCbc->setReg("VCth1", (cThreshold & 0x00FF));
            pCbc->setReg("VCth2", (cThreshold & 0x0300) >> 8);
            if(cSetLatency)
            {
                pCbc->setReg("TriggerLatency1", (cLatency & 0x00FF));
                uint8_t cLatReadValue = pCbc->getReg("FeCtrl&TrgLat2") & 0xFE;
                pCbc->setReg("FeCtrl&TrgLat2", (cLatReadValue | ((cLatency & 0x0100) >> 8)));
            }
        }

        os << GREEN << "|\t|\t|\t|----VCth: " << RED << std::hex << "0x" << cThreshold << std::dec << " (" << cThreshold << ")" << RESET << std::endl;
        if(cSetLatency) os << GREEN << "|\t|\t|\t|----TriggerLatency: " << RED << std::hex << "0x" << cLatency << std::dec << " (" << cLatency << ")" << RESET << std::endl;
    }

    // TEST PULSE
    pugi::xml_node cTPNode = pCbcNode.child("TestPulse");

    if(cTPNode != nullptr)
    {
        // pugi::xml_node cAmuxNode = pCbcNode.child ("Misc");
        uint8_t cEnable, cPolarity, cGroundOthers;
        uint8_t cAmplitude, cChanGroup, cDelay;

        cEnable       = convertAnyInt(cTPNode.attribute("enable").value());
        cPolarity     = convertAnyInt(cTPNode.attribute("polarity").value());
        cAmplitude    = convertAnyInt(cTPNode.attribute("amplitude").value());
        cChanGroup    = convertAnyInt(cTPNode.attribute("channelgroup").value());
        cDelay        = convertAnyInt(cTPNode.attribute("delay").value());
        cGroundOthers = convertAnyInt(cTPNode.attribute("groundothers").value());

        if(cType == FrontEndType::CBC3)
        {
            pCbc->setReg("TestPulsePotNodeSel", cAmplitude);
            pCbc->setReg("TestPulseDel&ChanGroup", reverseBits((cChanGroup & 0x07) << 5 | (cDelay & 0x1F)));
            uint8_t cAmuxValue = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
            pCbc->setReg("MiscTestPulseCtrl&AnalogMux", (((cPolarity & 0x01) << 7) | ((cEnable & 0x01) << 6) | ((cGroundOthers & 0x01) << 5) | (cAmuxValue & 0x1F)));
        }

        os << GREEN << "|\t|\t|\t|----TestPulse: "
           << "enabled: " << RED << +cEnable << GREEN << ", polarity: " << RED << +cPolarity << GREEN << ", amplitude: " << RED << +cAmplitude << GREEN << " (0x" << std::hex << +cAmplitude << std::dec
           << ")" << RESET << std::endl;
        os << GREEN << "|\t|\t|\t|               channelgroup: " << RED << +cChanGroup << GREEN << ", delay: " << RED << +cDelay << GREEN << ", groundohters: " << RED << +cGroundOthers << RESET
           << std::endl;
    }

    // CLUSTERS & STUBS
    pugi::xml_node cStubNode = pCbcNode.child("ClusterStub");

    if(cStubNode != nullptr)
    {
        uint8_t cCluWidth, cPtWidth, cLayerswap, cOffset1, cOffset2, cOffset3, cOffset4;

        cCluWidth  = convertAnyInt(cStubNode.attribute("clusterwidth").value());
        cPtWidth   = convertAnyInt(cStubNode.attribute("ptwidth").value());
        cLayerswap = convertAnyInt(cStubNode.attribute("layerswap").value());
        cOffset1   = convertAnyInt(cStubNode.attribute("off1").value());
        cOffset2   = convertAnyInt(cStubNode.attribute("off2").value());
        cOffset3   = convertAnyInt(cStubNode.attribute("off3").value());
        cOffset4   = convertAnyInt(cStubNode.attribute("off4").value());

        if(cType == FrontEndType::CBC3)
        {
            uint8_t cLogicSel = pCbc->getReg("Pipe&StubInpSel&Ptwidth");
            pCbc->setReg("Pipe&StubInpSel&Ptwidth", ((cLogicSel & 0xF0) | (cPtWidth & 0x0F)));
            pCbc->setReg("LayerSwap&CluWidth", (((cLayerswap & 0x01) << 3) | (cCluWidth & 0x07)));
            pCbc->setReg("CoincWind&Offset34", (((cOffset4 & 0x0F) << 4) | (cOffset3 & 0x0F)));
            pCbc->setReg("CoincWind&Offset12", (((cOffset2 & 0x0F) << 4) | (cOffset1 & 0x0F)));

            os << GREEN << "|\t|\t|\t|----Cluster & Stub Logic: "
               << "ClusterWidthDiscrimination: " << RED << +cCluWidth << GREEN << ", PtWidth: " << RED << +cPtWidth << GREEN << ", Layerswap: " << RED << +cLayerswap << RESET << std::endl;
            os << GREEN << "|\t|\t|\t|                          Offset1: " << RED << +cOffset1 << GREEN << ", Offset2: " << RED << +cOffset2 << GREEN << ", Offset3: " << RED << +cOffset3 << GREEN
               << ", Offset4: " << RED << +cOffset4 << RESET << std::endl;
        }
    }

    // MISC
    pugi::xml_node cMiscNode = pCbcNode.child("Misc");

    if(cMiscNode != nullptr)
    {
        uint8_t cPipeLogic, cStubLogic, cOr254, cTestClock, cTpgClock, cDll;
        uint8_t cAmuxValue;

        cPipeLogic = convertAnyInt(cMiscNode.attribute("pipelogic").value());
        cStubLogic = convertAnyInt(cMiscNode.attribute("stublogic").value());
        cOr254     = convertAnyInt(cMiscNode.attribute("or254").value());
        cDll       = reverseBits(static_cast<uint8_t>(convertAnyInt(cMiscNode.attribute("dll").value())) & 0x1F);
        // LOG (DEBUG) << convertAnyInt (cMiscNode.attribute ("dll").value() ) << " " << +cDll << " " << std::bitset<5>
        // (cDll);
        cTpgClock  = convertAnyInt(cMiscNode.attribute("tpgclock").value());
        cTestClock = convertAnyInt(cMiscNode.attribute("testclock").value());
        cAmuxValue = convertAnyInt(cMiscNode.attribute("analogmux").value());

        if(cType == FrontEndType::CBC3)
        {
            pCbc->setReg("40MhzClk&Or254", (((cTpgClock & 0x01) << 7) | ((cOr254 & 0x01) << 6) | (cTestClock & 0x01) << 5 | (cDll)));
            // LOG (DEBUG) << BOLDRED << std::bitset<8> (pCbc->getReg ("40MhzClk&Or254") ) << RESET;
            uint8_t cPtWidthRead = pCbc->getReg("Pipe&StubInpSel&Ptwidth");
            pCbc->setReg("Pipe&StubInpSel&Ptwidth", (((cPipeLogic & 0x03) << 6) | ((cStubLogic & 0x03) << 4) | (cPtWidthRead & 0x0F)));

            uint8_t cAmuxRead = pCbc->getReg("MiscTestPulseCtrl&AnalogMux");
            pCbc->setReg("MiscTestPulseCtrl&AnalogMux", ((cAmuxRead & 0xE0) | (cAmuxValue & 0x1F)));

            os << GREEN << "|\t|\t|\t|----Misc Settings: "
               << " PipelineLogicSource: " << RED << +cPipeLogic << GREEN << ", StubLogicSource: " << RED << +cStubLogic << GREEN << ", OR254: " << RED << +cOr254 << GREEN << ", TPG Clock: " << RED
               << +cTpgClock << GREEN << ", Test Clock 40: " << RED << +cTestClock << GREEN << ", DLL: " << RED << convertAnyInt(cMiscNode.attribute("dll").value()) << RESET << std::endl;
        }

        os << GREEN << "|\t|\t|\t|----Analog Mux "
           << "value: " << RED << +cAmuxValue << " (0x" << std::hex << +cAmuxValue << std::dec << ", 0b" << std::bitset<5>(cAmuxValue) << ")" << RESET << std::endl;
    }

    // CHANNEL MASK
    pugi::xml_node cDisableNode = pCbcNode.child("ChannelMask");

    if(cDisableNode != nullptr)
    {
        std::string       cList = std::string(cDisableNode.attribute("disable").value());
        std::string       ctoken;
        std::stringstream cStr(cList);
        os << GREEN << "|\t|\t|\t|----List of disabled Channels: ";

        int cIndex = 0;

        while(std::getline(cStr, ctoken, ','))
        {
            if(cIndex != 0) os << GREEN << ", ";

            uint8_t cChannel = convertAnyInt(ctoken.c_str());
            // cDisableVec.push_back (cChannel);

            if(cChannel == 0 || cChannel > 254)
                LOG(ERROR) << BOLDRED << "Error: channels for mask have to be between 1 and 254!" << RESET;
            else
            {
                // get the reigister string name from the map in Definition.h
                uint8_t cRegisterIndex = (cChannel - 1) / 8;
                // get the index of the bit to shift
                uint8_t cBitShift = (cChannel - 1) % 8;
                // get the original value of the register
                uint8_t cReadValue;

                if(cType == FrontEndType::CBC3)
                {
                    // get the original value of the register
                    cReadValue = pCbc->getReg(ChannelMaskMapCBC3[cRegisterIndex]);
                    // clear bit cBitShift
                    cReadValue &= ~(1 << cBitShift);
                    // write the new value
                    pCbc->setReg(ChannelMaskMapCBC3[cRegisterIndex], cReadValue);
                    LOG(DEBUG) << ChannelMaskMapCBC3[cRegisterIndex] << " " << std::bitset<8>(cReadValue);
                }

                os << BOLDCYAN << +cChannel;
            }

            cIndex++;
        }

        os << RESET << std::endl;
    }
}

void FileParser::parseSettingsxml(const std::string& pFilename, SettingsMap& pSettingsMap, std::ostream& os, bool pIsFile)
{
    pugi::xml_document     doc;
    pugi::xml_parse_result result;

    if(pIsFile == true)
        result = doc.load_file(pFilename.c_str());
    else
        result = doc.load(pFilename.c_str());

    if(result == false)
    {
        os << BOLDRED << "ERROR : Unable to open the file " << RESET << pFilename << std::endl;
        os << BOLDRED << "Error description: " << RED << result.description() << RESET << std::endl;

        if(pIsFile == false) os << "Error offset: " << result.offset << " (error at [..." << (pFilename.c_str() + result.offset) << "]" << std::endl;

        throw Exception("Unable to parse XML source!");
    }

    for(pugi::xml_node nSettings = doc.child("HwDescription").child("Settings"); nSettings == doc.child("HwDescription").child("Settings"); nSettings = nSettings.next_sibling())
    {
        os << std::endl;

        for(pugi::xml_node nSetting = nSettings.child("Setting"); nSetting; nSetting = nSetting.next_sibling())
        {
            if((strcmp(nSetting.attribute("name").value(), "RegNameDAC1") == 0) || (strcmp(nSetting.attribute("name").value(), "RegNameDAC2") == 0))
            {
                std::string value(nSetting.first_child().value());
                value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
                pSettingsMap[nSetting.attribute("name").value()] = value;

                os << BOLDRED << "Setting" << RESET << " -- " << BOLDCYAN << nSetting.attribute("name").value() << RESET << ":" << BOLDYELLOW
                   << boost::any_cast<std::string>(pSettingsMap[nSetting.attribute("name").value()]) << RESET << std::endl;
            }
            else
            {
                pSettingsMap[nSetting.attribute("name").value()] = convertAnyDouble(nSetting.first_child().value());

                os << BOLDRED << "Setting" << RESET << " -- " << BOLDCYAN << nSetting.attribute("name").value() << RESET << ":" << BOLDYELLOW
                   << boost::any_cast<double>(pSettingsMap[nSetting.attribute("name").value()]) << RESET << std::endl;
            }
        }
    }
}

void FileParser::parseHybridToLpGBT(pugi::xml_node pHybridNode, Ph2_HwDescription::Hybrid* cHybrid, Ph2_HwDescription::lpGBT* plpGBT, std::ostream& os)
{
    for(pugi::xml_node cChild: pHybridNode.children())
    {
        std::string cChildName = cChild.name();
        if(cChildName.find("_Files") != std::string::npos) continue;
        if(cChildName.find("RD53") != std::string::npos)
        {
            std::vector<uint8_t> cRxGroups   = splitToVector(cChild.attribute("RxGroups").value(), ',');
            std::vector<uint8_t> cRxChannels = splitToVector(cChild.attribute("RxChannels").value(), ',');
            std::vector<uint8_t> cTxGroups   = splitToVector(cChild.attribute("TxGroups").value(), ',');
            std::vector<uint8_t> cTxChannels = splitToVector(cChild.attribute("TxChannels").value(), ',');

            // Retrieve groups and channels from CIC node attirbutes and propagate to LpGBT class
            plpGBT->addRxGroups(cRxGroups);
            plpGBT->addRxChannels(cRxChannels);
            plpGBT->addTxGroups(cTxGroups);
            plpGBT->addTxChannels(cTxChannels);

            // In the case of IT propagate LpGBT mapping the front-end chip
            uint8_t cChipId = cChild.attribute("Id").as_int();
            static_cast<RD53*>(cHybrid->at(cChipId))->setRxGroup(cRxGroups[0]);
            static_cast<RD53*>(cHybrid->at(cChipId))->setRxChannel(cRxChannels[0]);
            static_cast<RD53*>(cHybrid->at(cChipId))->setTxGroup(cTxGroups[0]);
            static_cast<RD53*>(cHybrid->at(cChipId))->setTxChannel(cTxChannels[0]);
        }
    }
}

// ########################
// # RD53 specific parser #
// ########################
void FileParser::parseRD53(pugi::xml_node theChipNode, Hybrid* cHybrid, std::string cFilePrefix, std::ostream& os)
{
    std::string cFileName;

    if(cFilePrefix.empty() == false)
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");
        cFileName = cFilePrefix + expandEnvironmentVariables(theChipNode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(theChipNode.attribute("configfile").value());

    uint32_t chipId     = theChipNode.attribute("Id").as_int();
    uint32_t chipLane   = theChipNode.attribute("Lane").as_int();
    uint8_t  cRxGroup   = theChipNode.attribute("RxGroups").as_int();
    uint8_t  cRxChannel = theChipNode.attribute("RxChannels").as_int();
    uint8_t  cTxGroup   = theChipNode.attribute("TxGroups").as_int();
    uint8_t  cTxChannel = theChipNode.attribute("TxChannels").as_int();

    os << BOLDBLUE << "|\t|\t|----" << theChipNode.name() << " --> Id: " << BOLDYELLOW << chipId << BOLDBLUE << ", Lane: " << BOLDYELLOW << chipLane << BOLDBLUE << ", File: " << BOLDYELLOW
       << cFileName << BOLDBLUE << ", RxGroup: " << BOLDYELLOW << +cRxGroup << BOLDBLUE << ", RxChannel: " << BOLDYELLOW << +cRxChannel << BOLDBLUE << ", TxGroup: " << BOLDYELLOW << +cTxGroup
       << BOLDBLUE << ", TxChannel: " << BOLDYELLOW << +cTxChannel << RESET << std::endl;

    ReadoutChip* theChip = cHybrid->addChipContainer(chipId, new RD53(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getId(), chipId, chipLane, cFileName));
    theChip->setNumberOfChannels(RD53::nRows, RD53::nCols);

    this->parseRD53Settings(theChipNode, theChip, os);
}

void FileParser::parseGlobalRD53Settings(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    pugi::xml_node cGlobalChipSettings = pHybridNode.child("Global");
    if(cGlobalChipSettings != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----Global RD53 Settings:" << RESET << std::endl;

        for(const pugi::xml_attribute& attr: cGlobalChipSettings.attributes())
        {
            std::string regname  = attr.name();
            uint16_t    regvalue = convertAnyInt(attr.value());
            os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;

            for(auto theChip: *pHybrid) static_cast<ReadoutChip*>(theChip)->setReg(regname, regvalue, true);
        }
    }
}

void FileParser::parseRD53Settings(pugi::xml_node theChipNode, ReadoutChip* theChip, std::ostream& os)
{
    pugi::xml_node cLocalChipSettings = theChipNode.child("Settings");
    if(cLocalChipSettings != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----FrontEndType: " << BOLDYELLOW << "RD53" << RESET << std::endl;

        for(const pugi::xml_attribute& attr: cLocalChipSettings.attributes())
        {
            std::string regname  = attr.name();
            uint16_t    regvalue = convertAnyInt(attr.value());
            theChip->setReg(regname, regvalue, true);
            os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;
        }
    }
}
// ########################

std::string FileParser::parseMonitor(const std::string& pFilename, DetectorMonitorConfig& theDetectorMonitorConfig, std::ostream& os, bool pIsFile)
{
    if(pIsFile && pFilename.find(".xml") != std::string::npos)
        return parseMonitorxml(pFilename, theDetectorMonitorConfig, os, pIsFile);
    else if(!pIsFile)
        return parseMonitorxml(pFilename, theDetectorMonitorConfig, os, pIsFile);
    else
        LOG(ERROR) << BOLDRED << "Could not parse monitor file " << pFilename << " - it is not .xml" << RESET;
    return "None";
}

std::string FileParser::parseMonitorxml(const std::string& pFilename, DetectorMonitorConfig& theDetectorMonitorConfig, std::ostream& os, bool pIsFile)
{
    pugi::xml_document     doc;
    pugi::xml_parse_result result;

    if(pIsFile == true)
        result = doc.load_file(pFilename.c_str());
    else
        result = doc.load(pFilename.c_str());

    if(result == false)
    {
        os << BOLDRED << "ERROR : Unable to open the file " << RESET << pFilename << std::endl;
        os << BOLDRED << "Error description: " << RED << result.description() << RESET << std::endl;

        if(pIsFile == false) os << "Error offset: " << result.offset << " (error at [..." << (pFilename.c_str() + result.offset) << "]" << std::endl;

        throw Exception("Unable to parse XML source!");
        return "None";
    }

    if(!bool(doc.child("HwDescription").child("MonitoringSettings")))
    {
        os << BOLDYELLOW << "Monitoring not defined in " << pFilename << RESET << std::endl;
        os << BOLDYELLOW << "No monitoring will be run" << RESET << std::endl;
        return "None";
    }

    pugi::xml_node theMonitorNode = doc.child("HwDescription").child("MonitoringSettings").child("Monitoring");
    if(std::string(theMonitorNode.attribute("enable").value()) == "0") return "None";

    theDetectorMonitorConfig.fSleepTimeMs = atoi(theMonitorNode.child("MonitoringSleepTime").first_child().value());

    os << std::endl;

    auto const theMonitoringElements = theMonitorNode.child("MonitoringElements");
    if(theMonitoringElements != nullptr)
    {
        for(auto const& attr: theMonitoringElements.attributes())
        {
            uint16_t regvalue = convertAnyInt(attr.value());
            if(regvalue == 1)
            {
                auto const& regname = attr.name();
                os << BOLDRED << "Monitoring" << RESET << " -- " << BOLDCYAN << regname << RESET << ":" << BOLDYELLOW << "Yes" << RESET << std::endl;
                theDetectorMonitorConfig.fMonitorElementList.emplace_back(regname);
            }
            else
            {
                auto const& regname = attr.name();
                os << BOLDRED << "Monitoring" << RESET << " -- " << BOLDCYAN << regname << RESET << ":" << BOLDYELLOW << "No";
                if(regvalue != 0) os << BOLDRED << " (invalid configuration value: " << BOLDYELLOW << regvalue << BOLDRED << " -> must be 0 or 1)";

                os << RESET << std::endl;
            }
        }
    }

    if(theDetectorMonitorConfig.fMonitorElementList.size() == 0) return "None";
    return theMonitorNode.attribute("type").value();
}
} // namespace Ph2_System
