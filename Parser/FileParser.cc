#include "Parser/FileParser.h"
#include "HWDescription/Cbc.h"
#include "HWDescription/Cic.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/MPA2.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWDescription/RD53A.h"
#include "HWDescription/RD53B.h"
#include "HWDescription/SSA2.h"
#include "HWDescription/lpGBT.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace Ph2_Parser
{
void FileParser::parseHW(const std::string& pFilename, DetectorContainer* pDetectorContainer, std::ostream& os)
{
    int i, j;

    pugi::xml_document doc;
    openHWconfig(pFilename, doc);

    os << RESET << "\n\n";

    for(i = 0; i < 80; i++) os << "*";
    os << "\n";

    for(j = 0; j < 35; j++) os << " ";
    os << BOLDRED << "HW SUMMARY" << RESET << std::endl;

    for(i = 0; i < 80; i++) os << "*";
    os << "\n";

    // ##################################
    // # Iterate over the BeBoard Nodes #
    // ##################################
    for(pugi::xml_node cBeBoardNode = doc.child("HwDescription").child("BeBoard"); cBeBoardNode; cBeBoardNode = cBeBoardNode.next_sibling())
    {
        if(static_cast<std::string>(cBeBoardNode.name()) == "BeBoard") { parseBeBoard(cBeBoardNode, pDetectorContainer, os); }
    }

    for(i = 0; i < 80; i++) os << "*";

    os << "\n";

    for(j = 0; j < 32; j++) os << " ";

    os << BOLDRED << "END OF HW SUMMARY" << RESET << std::endl;

    for(i = 0; i < 80; i++) os << "*";

    os << std::endl;
}

void FileParser::openHWconfig(const std::string& pFilename, pugi::xml_document& doc)
{
    pugi::xml_parse_result result = doc.load_file(pFilename.c_str());
    if(!result) // Try if it is not a file, but a string containing the full xml
        result = doc.load_string(pFilename.c_str());

    if(!result)
    {
        LOG(ERROR) << BOLDRED << "ERROR : Unable to open the file : " << RESET << pFilename << std::endl;
        LOG(ERROR) << BOLDRED << "Error description : " << RED << result.description() << RESET << std::endl;
        throw Exception("Unable to parse XML source!");
    }
}

std::map<uint16_t, std::tuple<std::string, std::string, std::string>> FileParser::getRegManagerInfoList(const std::string& pFilename)
{
    pugi::xml_document doc;
    openHWconfig(pFilename, doc);

    std::map<uint16_t, std::tuple<std::string, std::string, std::string>> theRegManagerMap;
    for(pugi::xml_node cBeBoardNode = doc.child("HwDescription").child("BeBoard"); cBeBoardNode; cBeBoardNode = cBeBoardNode.next_sibling())
    {
        if(static_cast<std::string>(cBeBoardNode.name()) != "BeBoard") continue;
        pugi::xml_node cBeBoardConnectionNode = cBeBoardNode.child("connection");

        std::string cId                                         = cBeBoardConnectionNode.attribute("id").value();
        std::string cUri                                        = cBeBoardConnectionNode.attribute("uri").value();
        std::string cAddressTable                               = expandEnvironmentVariables(cBeBoardConnectionNode.attribute("address_table").value());
        theRegManagerMap[cBeBoardNode.attribute("Id").as_int()] = std::tuple<std::string, std::string, std::string>(cId, cUri, cAddressTable);
    }

    return theRegManagerMap;
}

void FileParser::parseBeBoard(pugi::xml_node pBeBordNode, DetectorContainer* pDetectorContainer, std::ostream& os)
{
    uint32_t cBeId    = pBeBordNode.attribute("Id").as_uint();
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
        else if(cEventTypeString == "SCAS")
            cBeBoard->setEventType(EventType::SCAS);
        else if(cEventTypeString == "SSAAS")
            cBeBoard->setEventType(EventType::SSAAS);
        else if(cEventTypeString == "MPAAS")
            cBeBoard->setEventType(EventType::MPAAS);
        else if(cEventTypeString == "MPA")
            cBeBoard->setEventType(EventType::MPA);
        else if(cEventTypeString == "SSA")
            cBeBoard->setEventType(EventType::SSA);
        else if(cEventTypeString == "PSAS")
            cBeBoard->setEventType(EventType::PSAS);
        else if(cEventTypeString == "VR2S")
            cBeBoard->setEventType(EventType::VR2S);
        else
            cBeBoard->setEventType(EventType::VR);
    }

    uint8_t cBoardReset = convertAnyInt(pBeBordNode.attribute("boardReset").value());
    cBeBoard->setReset(cBoardReset);

    uint8_t configureBoardFlag = convertAnyInt(pBeBordNode.attribute("configure").value());
    cBeBoard->setToConfigure(configureBoardFlag);

    uint8_t cReset = convertAnyInt(pBeBordNode.attribute("linkReset").value());
    cBeBoard->setLinkReset(cReset);

    os << BOLDBLUE << "|"
       << "----" << pBeBordNode.name() << " --> " << pBeBordNode.first_attribute().name() << ": " << BOLDYELLOW << pBeBordNode.attribute("Id").value() << BOLDBLUE << ", BoardType: " << BOLDYELLOW
       << cBoardType << BOLDBLUE << ", EventType: " << BOLDYELLOW << cEventTypeString << RESET << std::endl;

    pugi::xml_node cBeBoardConnectionNode = pBeBordNode.child("connection");

    std::string cId           = cBeBoardConnectionNode.attribute("id").value();
    std::string cUri          = cBeBoardConnectionNode.attribute("uri").value();
    std::string cAddressTable = expandEnvironmentVariables(cBeBoardConnectionNode.attribute("address_table").value());

    cBeBoard->setConnectionId(cId);
    cBeBoard->setConnectionUri(cUri);
    cBeBoard->setAddressTable(cAddressTable);

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
            parseRegister(cBeBoardRegNode, cNameString, cValue, cBeBoard, os);
        }
    }

    // Iterate the OpticalGroup node
    for(pugi::xml_node pOpticalGroupNode = pBeBordNode.child("OpticalGroup"); pOpticalGroupNode; pOpticalGroupNode = pOpticalGroupNode.next_sibling())
    {
        if(static_cast<std::string>(pOpticalGroupNode.name()) == "OpticalGroup")
        {
            cBeBoard->setOptical(false);
            parseOpticalGroupContainer(pOpticalGroupNode, cBeBoard, os);
        }
    }
    pugi::xml_node cSLinkNode = pBeBordNode.child("SLink");
    parseSLink(cSLinkNode, cBeBoard, os);
}

void FileParser::parseOpticalGroupContainer(pugi::xml_node pOpticalGroupNode, BeBoard* pBoard, std::ostream& os)
{
    std::string   cFilePath       = "";
    uint32_t      cOpticalGroupId = pOpticalGroupNode.attribute("Id").as_uint();
    uint32_t      cFMCId          = (std::string(pOpticalGroupNode.attribute("FMCId").value()) == "L12") ? 12 : 8;
    uint32_t      cBoardId        = pBoard->getId();
    OpticalGroup* theOpticalGroup = pBoard->addOpticalGroupContainer(cOpticalGroupId, new OpticalGroup(cBoardId, cFMCId, cOpticalGroupId));
    theOpticalGroup->setOptical(false);

    uint8_t cLinkReset = convertAnyInt(pOpticalGroupNode.attribute("reset").value());
    theOpticalGroup->setReset(cLinkReset);
    for(pugi::xml_node theChild: pOpticalGroupNode.children())
    {
        if(static_cast<std::string>(theChild.name()) == "Hybrid") { parseHybridContainer(theChild, theOpticalGroup, os, pBoard); }
        else if(static_cast<std::string>(theChild.name()) == "lpGBT_Files")
        {
            cFilePath = expandEnvironmentVariables(theChild.attribute("path").value());
            if((cFilePath.empty() == false) && (cFilePath.at(cFilePath.length() - 1) != '/')) cFilePath.append("/");
        }
        else if(static_cast<std::string>(theChild.name()) == "lpGBT")
        {
            std::string fileName = cFilePath + expandEnvironmentVariables(theChild.attribute("configfile").value());
            os << BOLDBLUE << "|\t|----" << theChild.name() << " --> File: " << BOLDYELLOW << fileName << RESET << std::endl;
            uint8_t cChipId      = theChild.attribute("Id").as_uint();
            uint8_t cChipVersion = theChild.attribute("version").as_uint();
            bool    cIsOptical   = theChild.attribute("optical").as_bool();

            lpGBT* thelpGBT = new lpGBT(cBoardId, cFMCId, cOpticalGroupId, cChipId, fileName);
            thelpGBT->setVersion(cChipVersion);
            thelpGBT->setOptical(cIsOptical);

            theOpticalGroup->setOptical(cIsOptical);
            pBoard->setOptical(cIsOptical);

            // ####################################################
            // # Load settings to tune Vref with external voltage #
            // ####################################################
            for(pugi::xml_node lpGBTChild: theChild.children())
            {
                if(static_cast<std::string>(lpGBTChild.name()) == "TuneVrefSettings")
                {
                    std::string cADC              = lpGBTChild.attribute("ADC").as_string();
                    uint16_t       cReferenceVoltage = lpGBTChild.attribute("ReferenceVoltage").as_uint();
                    os << BOLDCYAN << "|\t|\t|---- LpGBT TuneVrefSettings:" << RESET << std::endl;
                    os << GREEN << "|\t|\t|\t|---- ADC: " << RED << cADC << RESET << std::endl;
                    os << GREEN << "|\t|\t|\t|---- ReferenceVoltage: " << RED << +cReferenceVoltage << RESET << std::endl;
                    thelpGBT->setTuneVrefADC(cADC);
                    thelpGBT->setTuneVrefVoltage(cReferenceVoltage);
                }
            }

            theOpticalGroup->addlpGBT(thelpGBT);

            // ####################################################
            // # Initialize LpGBT settings from XML (only for IT) #
            // ####################################################
            if(pBoard->getBoardType() == BoardType::RD53)
            {
                for(const pugi::xml_attribute& attr: theChild.attributes())
                {
                    os << BOLDBLUE << "|\t|\t|---- " << attr.name() << ": " << BOLDYELLOW << attr.value() << "\n" << RESET;
                    if(std::string(attr.name()) == "ChipAddress")
                        thelpGBT->setChipAddress(convertAnyInt(theChild.attribute("ChipAddress").value()));
                    else if(std::string(attr.name()) == "RxHSLPolarity")
                        thelpGBT->setRxHSLPolarity(theChild.attribute("RxHSLPolarity").as_uint());
                    else if(std::string(attr.name()) == "TxHSLPolarity")
                        thelpGBT->setTxHSLPolarity(theChild.attribute("TxHSLPolarity").as_uint());
                    else if(std::string(attr.name()) == "RxDataRate")
                        thelpGBT->setRxDataRate(theChild.attribute("RxDataRate").as_uint());
                    else if(std::string(attr.name()) == "TxDataRate")
                        thelpGBT->setTxDataRate(theChild.attribute("TxDataRate").as_uint());
                    else if(std::string(attr.name()) == "ClockFrequency")
                        thelpGBT->setClocksFrequency(theChild.attribute("ClockFrequency").as_uint());
                }
            }
            else
            {
                thelpGBT->addRxGroups({0, 1, 2, 3, 4, 5, 6}); // By default we always use all 6 groups and
                thelpGBT->addRxChannels({0, 2});              // always channel 0 and channel 2 of each group
            }

            pugi::xml_node clpGBTSettings = theChild.child("Settings");
            if(clpGBTSettings != nullptr)
            {
                os << BOLDCYAN << "|\t|\t|---- LpGBT Settings:" << RESET << std::endl;

                for(const pugi::xml_attribute& attr: clpGBTSettings.attributes())
                {
                    std::string regname  = attr.name();
                    uint16_t    regvalue = convertAnyInt(attr.value());
                    thelpGBT->setReg(regname, regvalue, true);
                    os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;
                }
            }
        }
        else if(static_cast<std::string>(theChild.name()) == "NTCProperties")
        {
            std::string cNTCType        = std::string(theChild.attribute("type").value());
            std::string cNTCADC         = std::string(theChild.attribute("ADC").value());
            std::string cNTCLookUpTable = std::string(theChild.attribute("lookUpTable").value());
            theOpticalGroup->addNTC(cNTCType, cNTCADC, cNTCLookUpTable);
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
                parseRegister(cNode, cAttributeString, pValue, pBoard, os);
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

        for(pugi::xml_node cNode = pSLinkNode.child("ConditionData"); cNode; cNode = cNode.next_sibling())
        {
            if(cNode != nullptr)
            {
                uint8_t     cUID      = 0;
                uint8_t     cHybridId = 0;
                uint8_t     cCbcId    = 0;
                uint8_t     cPage     = 0;
                uint8_t     cAddress  = 0;
                uint32_t    cValue    = 0;
                std::string cRegName;

                std::string cTypeString = cNode.attribute("type").value();

                if(cTypeString == "HV")
                {
                    cUID      = 5;
                    cHybridId = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId    = convertAnyInt(cNode.attribute("Sensor").value());
                    cValue    = convertAnyInt(cNode.first_child().value());
                }
                else if(cTypeString == "TDC")
                {
                    cUID      = 3;
                    cHybridId = 0xFF;
                }
                else if(cTypeString == "User")
                {
                    cUID      = convertAnyInt(cNode.attribute("UID").value());
                    cHybridId = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId    = convertAnyInt(cNode.attribute("CbcId").value());
                    cValue    = convertAnyInt(cNode.first_child().value());
                }
                else if(cTypeString == "I2C")
                {
                    cUID      = 1;
                    cRegName  = cNode.attribute("Register").value();
                    cHybridId = convertAnyInt(cNode.attribute("Id").value());
                    cCbcId    = convertAnyInt(cNode.attribute("CbcId").value());

                    for(auto cOpticalGroup: *pBoard)
                        for(auto cHybrid: *cOpticalGroup)
                        {
                            if(cHybrid->getId() != cHybridId) continue;

                            for(auto cCbc: *cHybrid)
                            {
                                if(cCbc->getId() != cCbcId)
                                    continue;
                                else if(cHybrid->getId() == cHybridId && cCbc->getId() == cCbcId)
                                {
                                    ChipRegItem cRegItem = static_cast<ReadoutChip*>(cCbc)->getRegItem(cRegName);
                                    cPage                = cRegItem.fPage;
                                    cAddress             = cRegItem.fAddress;
                                    cValue               = cRegItem.fValue;
                                }
                                else
                                    LOG(ERROR) << BOLDRED << "SLINK ERROR: no Chip with Id " << +cCbcId << " on Hybrid " << +cHybridId << " - check your SLink Settings!" << RESET;
                            }
                        }
                }

                cSet->addCondData(cRegName, cUID, cHybridId, cCbcId, cPage, cAddress, cValue);
                os << BLUE << "|"
                   << " "
                   << "|"
                   << "       "
                   << "|"
                   << "----" << cNode.name() << ": Type " << RED << cTypeString << " " << cRegName << BLUE << ", UID " << RED << +cUID << BLUE << ", HybridId " << RED << +cHybridId << BLUE
                   << ", CbcId " << RED << +cCbcId << std::hex << BLUE << ", Page " << RED << +cPage << BLUE << ", Address " << RED << +cAddress << BLUE << ", Value " << std::dec << MAGENTA << cValue
                   << RESET << std::endl;
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
    uint32_t    cChipId    = pSSAnode.attribute("Id").as_uint();
    uint32_t    cPartnerId = pSSAnode.attribute("partid").as_uint();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    ReadoutChip* cSSA = pHybrid->addChipContainer(cChipId, new SSA(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getOpticalGroupId(), pHybrid->getId(), cChipId, cPartnerId, 0, cFileName));
    cSSA->setOptical(pHybrid->isOptical());
    cSSA->setNumberOfChannels(NSSACHANNELS);
    cSSA->setClockFrequency(320);
    cSSA->setMasterId(pHybrid->getMasterId());

    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "---- SSA controlled by I2CMaster " << +cSSA->getMasterId() << RESET << "\n";
}

void FileParser::parseSSASettings(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    LOG(INFO) << BOLDBLUE << "Now I'm parsing global PS settings for SSAs " << RESET;
    pugi::xml_node cGlobalSettingsNode = pHybridNode.child("Global");

    if(cGlobalSettingsNode != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----Global SSA Settings: " << RESET << std::endl;
        // first.. thresholds
        pugi::xml_node cThresholdNode = cGlobalSettingsNode.child("Thresholds");
        if(cThresholdNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                unsigned cStripThreshold     = convertAnyInt(cThresholdNode.attribute("stripThreshold").value());
                unsigned cStripThresholdHigh = convertAnyInt(cThresholdNode.attribute("stripThresholdHigh").value());
                if (cStripThreshold > 0xFF)
                {
                    throw std::runtime_error("The stripThreshold register set in the xml is greater than 255. Acceptable values are between 0 and 255.");
                    exit(0);
                }
                if (cStripThresholdHigh > 0xFF)
                {
                    throw std::runtime_error("The cStripThresholdHigh register set in the xml is greater than 255. Acceptable values are between 0 and 255.");
                    exit(0);
                }

                cChip->setReg("Bias_THDAC", cStripThreshold);
                cChip->setReg("Bias_THDACHIGH", cStripThresholdHigh);
                os << BOLDCYAN << "|\t|\t|----Applying global SSA Settings to SSA# " << +cChip->getId() << RESET << std::endl
                   << GREEN << "|\t|\t|\t|---- Threshold: Strips 0x" << std::hex << +cStripThreshold << std::dec << RESET << std::endl
                   << GREEN << "|\t|\t|\t|---- And Threshold High: Strips 0x" << std::hex << +cStripThresholdHigh << std::dec << RESET << std::endl;
            }
        }

        // then hit logic mode
        pugi::xml_node cHitLogicNode = cGlobalSettingsNode.child("HitLogic");
        if(cHitLogicNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                uint16_t cMode = convertAnyInt(cHitLogicNode.attribute("stripMode").value());
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cChip->setReg("SAMPLINGMODE_ALL", cMode);
                    os << BOLDCYAN << "|\t|\t|----Applying global SSA hit logic settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Hit Mode is  0x" << std::hex << +cMode
                    << std::dec << RESET << std::endl;
                }
                else if(cChip->getFrontEndType() == FrontEndType::SSA2)
                {              
                    uint16_t cValueInMemory = cChip->getReg("ENFLAGS");
                    // LOG(INFO) << BOLDCYAN << __LINE__ << "] ENFLAGS from memory 0x" << std::hex << cValueInMemory << std::dec << RESET;
                    cValueInMemory = (cValueInMemory & 0x1F) + ((cMode & 0x03)<<5) ;
                    // LOG(INFO) << BOLDCYAN << __LINE__ << "] cMode ENFLAGS from memory 0x" << std::hex << cValueInMemory << std::dec << RESET;
                    cChip->setReg("ENFLAGS", cValueInMemory);
                    os << BOLDCYAN << "|\t|\t|----ENFLAGS to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Hit Mode is  0x" << std::hex << +cMode
                    << std::dec << RESET << std::endl;
                }

            }
        }

        // then charge injection
        pugi::xml_node cInjectionNode = cGlobalSettingsNode.child("InjectedCharge");
        if(cInjectionNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                int cInjStrps = convertAnyInt(cInjectionNode.attribute("stripCharge").value()); // / SSA2_ELECTRON_CALDAC; // conversion factor to electron with 1 CalDAC = 0.039 fC 
                if (cInjStrps > 0xFF)
                {
                    throw std::runtime_error("The maximum charge that can be injected is 255. Acceptable values are between 0 and 255.");
                    exit(0);
                }
                cChip->setReg("Bias_CALDAC", cInjStrps);
                os << BOLDCYAN << "|\t|\t|----Applying global SSA injection settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Injected Charge is  0x" << std::hex << +cInjStrps
                   << std::dec << RESET << std::endl;
            }
        }

        // latencies
        pugi::xml_node cLatencyNode = cGlobalSettingsNode.child("Latencies");
        if(cLatencyNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                int cLatency = convertAnyInt(cLatencyNode.attribute("stripLatency").value());
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cChip->setReg("L1-Latency_LSB", cLatency & 0xFF);
                    cChip->setReg("L1-Latency_MSB", (cLatency >> 8) & 0xFF);
                }
                else if(cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    // std::bitset<9> b_latency(cLatency);
                    // std::cout << " cLatency for strips is " << cLatency << " binary " << b_latency << std::endl;
                    // std::cout << " initial control_1 value " << cChip->getReg("control_1") << std::endl;
                    uint16_t control_1 = cChip->getReg("control_1");
                    control_1 = (control_1 & 0xEF) + (((cLatency>>8)&0x1) <<4);
                    cChip->setReg("control_1", control_1);
                    cChip->setReg("control_3", cLatency&0xFF);
                    //std::cout << std::hex << "Latency: " << cLatency << " control 1  " <<  cChip->getReg("control_1") << " control_3 " << cChip->getReg("control_3") << std::dec << std::endl;
                }
                os << BOLDCYAN << "|\t|\t|----Applying global SSA latency settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Latency is  0x" << std::hex << +cLatency
                   << std::dec << GREEN << " MSB is 0x" << std::hex << ((cLatency >> 8) & 0xFF) << std::dec << GREEN << " LSB is 0x" << std::hex << (cLatency & 0xFF) << std::dec << RESET << std::endl;
            }
        }

        // hip cut
        pugi::xml_node cHIPmode = cGlobalSettingsNode.child("HipLogic");
        if(cHIPmode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                int cCut = convertAnyInt(cHIPmode.attribute("stripCut").value());
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {
                    cChip->setReg("HIPCUT_ALL", cCut);
                    os << BOLDCYAN << "|\t|\t|----Applying global SSA HIP settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- HIP cut is  0x" << std::hex << +cCut << std::dec
                    << RESET << std::endl;
                }
                else if (cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    uint16_t cMemStripControl2 = cChip->getReg("StripControl2");
                    cMemStripControl2 = (cMemStripControl2 & 0xF8) + (cCut&0x7); // HipCut is the 3 LSB of the StripControl2 register
                    cChip->setReg("StripControl2", cMemStripControl2);
                    os << BOLDCYAN << "|\t|\t|----Applying global SSA HIP settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- HIP cut is  0x" << std::hex << +cCut << " read for StripControl2 is 0x" << cChip->getReg("StripControl2") << std::dec
                    << RESET << std::endl;  
                }   
            }
        }

        // timing
        pugi::xml_node cSamplingDelay = cGlobalSettingsNode.child("SamplingDelay");
        if(cSamplingDelay != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::SSA && cChip->getFrontEndType() != FrontEndType::SSA2) continue;
                int cCoarse = convertAnyInt(cSamplingDelay.attribute("stripCoarse").value());
                int cFine   = convertAnyInt(cSamplingDelay.attribute("stripFine").value());
                cChip->setReg("ClockDeskewing_coarse", cCoarse);
                ChipRegMask cMask;
                cMask.fNbits    = 3;
                cMask.fBitShift = 0;
                cChip->setRegBits("ClockDeskewing_fine", cMask, cFine);

                os << BOLDCYAN << "|\t|\t|----Applying global SSA Sampling Delay settings to SSA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Coarse delay will be set to "
                   << cCoarse * 3.125 << " ns " << GREEN << " Fine delay will be set to " << cFine * 0.2 << " ns." << RESET << std::endl;
            }
        }
    }
}
void FileParser::parseSSA2Container(pugi::xml_node pSSAnode, Hybrid* pHybrid, std::string cFilePrefix, std::ostream& os)
{
    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "----" << pSSAnode.name() << "  " << pSSAnode.first_attribute().name() << " :" << pSSAnode.attribute("Id").value()
       << ", File: " << expandEnvironmentVariables(pSSAnode.attribute("configfile").value()) << RESET << std::endl;

    // Get ID of SSA then add to the Hybrid!
    uint32_t    cChipId    = pSSAnode.attribute("Id").as_uint();
    uint32_t    cPartnerId = pSSAnode.attribute("partid").as_uint();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pSSAnode.attribute("configfile").value());
    ReadoutChip* cSSA2 = pHybrid->addChipContainer(cChipId, new SSA2(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getOpticalGroupId(), pHybrid->getId(), cChipId, cPartnerId, 0, cFileName));
    cSSA2->setOptical(pHybrid->isOptical());
    cSSA2->setNumberOfChannels(NSSACHANNELS);
    cSSA2->setClockFrequency(320);
    cSSA2->setMasterId(pHybrid->getMasterId());
}

void FileParser::parseSSA2Settings(pugi::xml_node pHybridNode, ReadoutChip* pSSA)
{
    // FrontEndType cType = pSSA->getFrontEndType();
}

void FileParser::parseMPAContainer(pugi::xml_node pMPANode, Hybrid* pHybrid, std::string cFilePrefix, std::ostream& os)
{
    // Get ID of MPA then add to the Hybrid!
    uint32_t    cChipId    = pMPANode.attribute("Id").as_uint();
    uint32_t    cPartnerId = pMPANode.attribute("partid").as_uint();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pMPANode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pMPANode.attribute("configfile").value());
    ReadoutChip* cMPA = pHybrid->addChipContainer(cChipId, new MPA(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getOpticalGroupId(), pHybrid->getId(), cChipId, cPartnerId, cFileName));
    cMPA->setOptical(pHybrid->isOptical());
    cMPA->setNumberOfChannels(NSSACHANNELS, NMPACOLS);
    cMPA->setClockFrequency(320);
    cMPA->setMasterId(pHybrid->getMasterId());

    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "---- MPA controlled by I2CMaster " << +cMPA->getMasterId() << RESET << std::endl;
}

// Irene
void FileParser::parseMPA2Container(pugi::xml_node pMPANode, Hybrid* pHybrid, std::string cFilePrefix, std::ostream& os)
{ // Get ID of MPA then add to the Hybrid!
    uint32_t    cChipId    = pMPANode.attribute("Id").as_uint();
    uint32_t    cPartnerId = pMPANode.attribute("partid").as_uint();
    std::string cFileName;
    if(!cFilePrefix.empty())
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");

        cFileName = cFilePrefix + expandEnvironmentVariables(pMPANode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(pMPANode.attribute("configfile").value());

    ReadoutChip* cMPA = pHybrid->addChipContainer(cChipId, new MPA2(pHybrid->getBeBoardId(), pHybrid->getFMCId(), pHybrid->getOpticalGroupId(), pHybrid->getId(), cChipId, cPartnerId, cFileName));

    cMPA->setOptical(pHybrid->isOptical());
    cMPA->setNumberOfChannels(NSSACHANNELS, NMPACOLS);
    cMPA->setClockFrequency(320);
    cMPA->setMasterId(pHybrid->getMasterId());

    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "---- MPA2 controlled by I2CMaster " << +cMPA->getMasterId() << RESET << std::endl;
}

void FileParser::parseMPASettings(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    LOG(INFO) << BOLDBLUE << "Now I'm parsing global PS settings for MPA " << RESET;
    pugi::xml_node cGlobalSettingsNode = pHybridNode.child("Global");
    if(cGlobalSettingsNode != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----Global MPA Settings: " << RESET << std::endl;
        // first.. thresholds
        pugi::xml_node cThresholdNode = cGlobalSettingsNode.child("Thresholds");
        if(cThresholdNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cThresholdPxls = convertAnyInt(cThresholdNode.attribute("pixelThreshold").value());
                if (cThresholdPxls > 0xFF)
                {
                    throw std::runtime_error("The pixelThreshold register set in the xml is greater than 255. Acceptable values are between 0 and 255.");
                    exit(0);
                }
                
                for(size_t cIndx = 0; cIndx < 7; cIndx++)
                {
                    std::stringstream cRegName;
                    cRegName << "ThDAC" << +cIndx;
                    cChip->setReg(cRegName.str(), cThresholdPxls);
                }
                os << BOLDCYAN << "|\t|\t|----Applying global threshold MPA Settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Threshold: Pxls 0x" << std::hex
                   << +cThresholdPxls << std::dec << RESET << std::endl;
            }
        }

        // now stub mode
        pugi::xml_node cStubLogicNode = cGlobalSettingsNode.child("StubLogic");
        if(cStubLogicNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                uint8_t cMode   = static_cast<uint8_t>(convertAnyInt(cStubLogicNode.attribute("mode").value()));
                uint8_t cWindow = static_cast<uint8_t>(convertAnyInt(cStubLogicNode.attribute("window").value()));
                uint8_t cRegVal = (cMode << 6) | cWindow;
                cChip->setReg("ECM", cRegVal);
                LOG(INFO) << BOLDCYAN << "|\t|\t|----Applying global MPA stub settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Stub Mode is  0x" << std::hex << +cMode << std::dec
                   << RESET << GREEN << "|\t|\t|\t|---- Stub Window is  " << (float)cWindow / 2. << " half-pixels " << RESET << GREEN << "|\t|\t|\t|---- register value [ECM] is set to 0x" << std::hex
                   << +cRegVal << " and we read back from memory " << cChip->getReg("ECM") << std::dec << RESET;
            }
        }
        // then hit logic mode
        pugi::xml_node cHitLogicNode = cGlobalSettingsNode.child("HitLogic");
        if(cHitLogicNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cMode       = convertAnyInt(cHitLogicNode.attribute("pixelMode").value());
                int cClusterCut = convertAnyInt(cHitLogicNode.attribute("pixelClusterCut").value());

                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cChip->setReg("ModeSel_ALL", cMode); // Irene
                }
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                {
                    uint16_t pixelControl = cChip->getReg("PixelControl_ALL");
                    pixelControl = (pixelControl & 0xE0) + ((cClusterCut & 0x07) << 2) + (cMode & 0x03);
                    cChip->setReg("PixelControl_ALL", pixelControl);
                    LOG(INFO) << BOLDRED << "PixelControl_ALL 0x" << std::hex << pixelControl << std::dec << " getting from memory 0x"<< std::hex << cChip->getReg("PixelControl_ALL") << std::dec <<RESET;
                }

                os << BOLDCYAN << "|\t|\t|----Applying global MPA hit logic settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Hit Mode is  0x" << std::hex << +cMode
                   << std::dec << RESET << std::endl;
            }
        }

        // then charge injection
        pugi::xml_node cInjectionNode = cGlobalSettingsNode.child("InjectedCharge");
        if(cInjectionNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cInjPxls = convertAnyInt(cInjectionNode.attribute("pixelCharge").value()); // / MPA2_ELECTRON_CALDAC. conversion factor from DAQ to electrons;
                if (cInjPxls > 0xFF)
                {
                    throw std::runtime_error("The maximum charge that can be injected is 255. Acceptable values are between 0 and 255.");
                    exit(0);
                }
                for(size_t cIndx = 0; cIndx < 7; cIndx++)
                {
                    std::stringstream cRegName;
                    cRegName << "CalDAC" << +cIndx;
                    cChip->setReg(cRegName.str(), cInjPxls);
                }
                os << BOLDCYAN << "|\t|\t|----Applying global MPA injection settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Injected Charge is  0x" << std::hex << +cInjPxls
                   << std::dec << RESET << std::endl;
            }
        }

        // latencies
        pugi::xml_node cLatencyNode = cGlobalSettingsNode.child("Latencies");
        if(cLatencyNode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cLatency      = convertAnyInt(cLatencyNode.attribute("pixelLatency").value());
                int cRetimePix    = convertAnyInt(cLatencyNode.attribute("retimePix").value());
                int cLatencyRx320 = ((convertAnyInt(cLatencyNode.attribute("LatencyRx320L1").value())) & 0x7) + ((convertAnyInt(cLatencyNode.attribute("LatencyRx320Trigger").value()) & 0x7)<< 3);
                int cEdgeSelT1Raw = convertAnyInt(cLatencyNode.attribute("EdgeSelT1Raw").value());
                int cEdgeSelTrig  = convertAnyInt(cLatencyNode.attribute("EdgeSelTrig").value());
                if (cEdgeSelTrig == 1) cEdgeSelTrig = 255; // 255 is 0xFF in hex and sets 1 for all SSAs.
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cChip->setReg("L1Offset_1_ALL", cLatency & 0xFF);        // Irene
                    cChip->setReg("L1Offset_2_ALL", (cLatency >> 8) & 0xFF); // Irene
                }
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                {
                    cChip->setReg("MemoryControl_1_ALL", cLatency & 0xFF);
                    
                    uint16_t memoryControl_2 = cChip->getReg("MemoryControl_2_ALL");
                    memoryControl_2 = (memoryControl_2 & 0xFE) + ((cLatency >> 8) & 0x01) ;
                    cChip->setReg("MemoryControl_2_ALL", memoryControl_2);
                    
                    ChipRegMask cMask;
                    cMask.fNbits    = 3;
                    cMask.fBitShift = 2;
                    cChip->setRegBits("Control_1", cMask, cRetimePix);
                    uint16_t cValueInMemory = cChip->getReg("Control_1");

                    LOG(INFO) << BOLDRED << __LINE__ << "RETIME PIX: 0x" << std::hex << cRetimePix << " CONTROL_1: 0x" << cValueInMemory  << std::dec << RESET;
                    cChip->setReg("LatencyRx320", cLatencyRx320 & 0x3F);

                    LOG(INFO) << BOLDRED << __LINE__ << "cEdgeSelT1Raw: 0x" << std::hex << cEdgeSelT1Raw << std::dec << RESET;
                    cChip->setReg("EdgeSelT1Raw", cEdgeSelT1Raw);

                    LOG(INFO) << BOLDRED << __LINE__ << "cEdgeSelTrig: 0x" << std::hex << cEdgeSelTrig << std::dec << RESET;
                    cChip->setReg("EdgeSelTrig", cEdgeSelTrig);

                }
                os << BOLDCYAN << "|\t|\t|----Applying global MPA latency settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Latency is  0x" << std::hex << +cLatency
                   << std::dec << GREEN << " MSB is 0x" << std::hex << ((cLatency >> 8) & 0xFF) << std::dec << GREEN << " LSB is 0x" << std::hex << (cLatency & 0xFF) << std::dec << RESET << std::endl;
            }
        }

        // hip cut
        pugi::xml_node cHIPmode = cGlobalSettingsNode.child("HipLogic");
        if(cHIPmode != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cCut = convertAnyInt(cHIPmode.attribute("pixelCut").value());
                if(cChip->getFrontEndType() == FrontEndType::MPA) 
                    cChip->setReg("HipCut_ALL", cCut);
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                {
                    ChipRegMask cMask;
                    cMask.fNbits    = 3;
                    cMask.fBitShift = 5;
                    cChip->setRegBits("PixelControl_ALL", cMask, cCut);
                }
                os << BOLDCYAN << "|\t|\t|----Applying global MPA HIP settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- HIP cut is  0x" << std::hex << +cCut << std::dec
                   << RESET << std::endl;
            }
        }

        // timing
        pugi::xml_node cSamplingDelay = cGlobalSettingsNode.child("SamplingDelay");
        if(cSamplingDelay != nullptr)
        {
            for(auto cChip: *pHybrid)
            {
                if(cChip->getFrontEndType() != FrontEndType::MPA && cChip->getFrontEndType() != FrontEndType::MPA2) continue;
                int cCoarse = convertAnyInt(cSamplingDelay.attribute("pixelCoarse").value());
                int cFine   = convertAnyInt(cSamplingDelay.attribute("pixelFine").value());
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    cChip->setReg("PhaseShift", cCoarse); // Irene
                }
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                {
                    //cChip->setReg("Mask", 0x70);
                    //cChip->setReg("Control_1", (cCoarse << 4));//LORENZO ONLY THE TOP 3 BITS?? WHAT IS Control_1?
                    //LORENZO, IF SO THEN THIS IS THE WAY TO DO IT
                    ChipRegMask cMask;
                    cMask.fNbits    = 3;
                    cMask.fBitShift = 5;
                    cChip->setRegBits("Control_1", cMask, cCoarse);

                    uint16_t cValueInMemory = cChip->getReg("Control_1");

                    LOG(INFO) << BOLDRED << __LINE__ << "COARSE: 0x" << std::hex << cCoarse << " CONTROL_1: 0x" << cValueInMemory  << std::dec << RESET;
                    cFine = (cFine & 0xCF) + 0x30;//Setting bit 4 and 5 to 1 (4->Enable DLL, 5->DoNot Bypass)
                    cChip->setReg("ConfDLL", (cFine));
                }
                cChip->setReg("ConfDLL", cFine);

                os << BOLDCYAN << "|\t|\t|----Applying global MPA Sampling Delay settings to MPA# " << +cChip->getId() << RESET << GREEN << "|\t|\t|\t|---- Coarse delay will be set to "
                   << cCoarse * 3.125 << " ns " << GREEN << " Fine delay will be set to " << cFine * 0.2 << " ns." << RESET << std::endl;
            }
        }
    }

    // THRESHOLD & LATENCY
    // pugi::xml_node cThresholdNode = pChipnode.child("Thresholds");

    // if(cThresholdNode != nullptr)
    // {
    //     uint8_t cThresholdPxls   = static_cast<uint8_t>(  convertAnyInt(cThresholdNode.attribute("pixelThreshold").value())/94. ) ;
    //     for(size_t cIndx=0; cIndx < 7 ; cIndx++)
    //     {
    //         std::stringstream cRegName;
    //         cRegName << "ThDAC" << +cIndx;
    //         pChip->setReg(cRegName.str(), cThresholdPxls);
    //     }
    //     os << GREEN << "|\t|\t|\t|---- Threshold: Pxls 0x" << RED << std::hex << +cThresholdPxls << std::dec << RESET << std::endl;
    // }
}

void FileParser::parseHybridContainer(pugi::xml_node pHybridNode, OpticalGroup* pOpticalGroup, std::ostream& os, BeBoard* pBoard)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    bool cEnable = pHybridNode.attribute("enable").as_bool();

    if(cEnable)
    {
        os << BOLDBLUE << "|       |"
           << "----" << pHybridNode.name() << " --> " << BOLDBLUE << pHybridNode.first_attribute().name() << ": " << BOLDYELLOW << pHybridNode.attribute("Id").value() << BOLDBLUE
           << ", Enable: " << BOLDYELLOW << expandEnvironmentVariables(pHybridNode.attribute("enable").value()) << BOLDBLUE << RESET << std::endl;

        Hybrid* cHybrid;
        if(pBoard->getBoardType() == BoardType::RD53)
        {
            cHybrid = pOpticalGroup->addHybridContainer(pHybridNode.attribute("Id").as_int(),
                                                        new Hybrid(pOpticalGroup->getBeBoardId(), pOpticalGroup->getFMCId(), pOpticalGroup->getOpticalGroupId(), pHybridNode.attribute("Id").as_int()));

            uint8_t cHybridReset = convertAnyInt(pHybridNode.attribute("reset").value());
            cHybrid->setReset(cHybridReset);
        }
        else
        {
            uint8_t cHybridId = 2 * pOpticalGroup->getId() + pHybridNode.attribute("Id").as_uint();
            uint8_t cMasterId;
            if(pHybridNode.attribute("i2cMaster")) { cMasterId = pHybridNode.attribute("i2cMaster").as_int(); } // can overwrite default from xml
            else
                cMasterId = (cHybridId % 2 == 0) ? 2 : 0; // default for OT hybrids is that RHS is connected to master 2, LHS connected to master 1
            os << BOLDBLUE << "I2C Master Id is " << +cMasterId << RESET << std::endl;

            uint8_t invertClock;
            if(pHybridNode.attribute("invertClock")) { invertClock = pHybridNode.attribute("invertClock").as_int(); } // can overwrite default from xml
            else invertClock = 1; // default for OT hybrids is that RHS is connected to master 2, LHS connected to master 1
            os << BOLDBLUE << "invertClock " << +invertClock << RESET << std::endl;

            cHybrid = pOpticalGroup->addHybridContainer(cHybridId, new OuterTrackerHybrid(pOpticalGroup->getBeBoardId(), pOpticalGroup->getFMCId(), pOpticalGroup->getOpticalGroupId(), cHybridId));

            cHybrid->setMasterId(cMasterId);
            cHybrid->setInvertClock(invertClock);

            cHybrid->setOptical(pBoard->isOptical());
            os << BOLDBLUE << "|       |       | HybridOpticalId is " << +cHybrid->getOpticalGroupId() << RESET;
        }

        std::string cConfigFileDirectory;
        for(pugi::xml_node cChild: pHybridNode.children())
        {
            std::string cName          = cChild.name();
            std::string cNextName      = cChild.next_sibling().name();
            bool        cIsTrackerASIC = cName.find("CBC") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("SSA") != std::string::npos;
            cIsTrackerASIC             = cIsTrackerASIC || cName.find("SSA2") != std::string::npos;
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
                        cHybrid->setNPixelChips(cHybrid->getNPixelChips() + 1);
                        const auto frontEndType = cName.find("RD53A") != std::string::npos ? FrontEndType::RD53A : FrontEndType::RD53B;
                        pBoard->setFrontEndType(frontEndType);
                        parseRD53(cChild, cHybrid, cConfigFileDirectory, os, frontEndType);
                        if(cNextName.empty() || cNextName != cName) parseGlobalRD53Settings(pHybridNode, cHybrid, os);
                    }
                    else if(cName.find("CBC") != std::string::npos)
                    {
                        cHybrid->setNStripChips(cHybrid->getNStripChips() + 1);
                        pBoard->setFrontEndType(FrontEndType::CBC3);
                        parseCbcContainer(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) parseGlobalCbcSettings(pHybridNode, cHybrid, os);
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
                        Cic* cCic = new Cic(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getOpticalGroupId(), cHybrid->getId(), cChipId, cFileName);
                        static_cast<OuterTrackerHybrid*>(cHybrid)->addCic(cCic);
                        cCic->setFrontEndType(cType);
                        cCic->setOptical(cHybrid->isOptical());
                        cCic->setMasterId(cHybrid->getMasterId());

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
                                if(cChildGlobal.attribute("driveStrength"))
                                {
                                    uint8_t cDriveStrength = cChildGlobal.attribute("driveStrength").as_uint();
                                    cCic->setDriveStrength(cDriveStrength);
                                }

                                if(cChildGlobal.attribute("edgeSelect"))
                                {
                                    uint8_t cEdgeSelect = cChildGlobal.attribute("edgeSelect").as_uint();
                                    LOG(INFO) << BOLDBLUE << "Setting edge select to " << +cEdgeSelect << RESET;
                                    cCic->setEdgeSelect(cEdgeSelect);
                                }
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

                                    uint16_t cValueFromFile = cChildGlobal.attribute(cAttribute.c_str()).as_uint();
                                    if(cAttribute == "clockFrequency")
                                    {
                                        cValueFromFile = (cValueFromFile == 320) ? 0 : 1;
                                        cCic->setClockFrequency(cValueFromFile);
                                    }
                                    if(cAttribute == "clockFrequency" && cCIC1) continue;
                                    if(cAttribute == "enableSparsification")
                                    {
                                        pBoard->setSparsification(bool(cValueFromFile));
                                        LOG(INFO) << BOLDYELLOW << "Board sparisfication set to " << pBoard->getSparsification() << RESET;
                                    }

                                    os << GREEN << "|\t|\t|\t|---- Setting " << cAttribute << " to  " << cValueFromFile << "\n" << RESET;
                                    LOG(DEBUG) << BOLDBLUE << " Global settings " << cAttribute << " [ " << *it << " ]-- set to " << cValueFromFile << RESET;

                                    std::string cRegName  = cCIC1 ? std::string(*it) : "FE_CONFIG";
                                    auto        cRegValue = cCic->getReg(cRegName);
                                    uint16_t    cNewValue = cCIC1 ? cValueFromFile : ((cRegValue & cMask) | (cValueFromFile << cBitPosition));

                                    LOG(INFO) << BOLDBLUE << "  Setting [ " << cRegName << " " << *it << " == " << +cValueFromFile << "]-- set to. Mask " << std::bitset<5>(cMask) << " -- old value "
                                              << std::bitset<5>(cRegValue) << " -- new value " << std::bitset<5>(cNewValue) << RESET;
                                    cCic->setReg(cRegName, cNewValue);
                                }
                                if(cChildGlobal.attribute("driveStrength"))
                                {
                                    uint8_t cDriveStrength = cChildGlobal.attribute("driveStrength").as_uint();
                                    cCic->setDriveStrength(cDriveStrength);
                                }

                                if(cChildGlobal.attribute("edgeSelect"))
                                {
                                    uint8_t cEdgeSelect = cChildGlobal.attribute("edgeSelect").as_uint();
                                    cCic->setEdgeSelect(cEdgeSelect);
                                }
                            }
                        }
                    }
                    else if(cName == "SSA")
                    {
                        cHybrid->setNStripChips(cHybrid->getNStripChips() + 1);
                        pBoard->setFrontEndType(FrontEndType::SSA);
                        parseSSAContainer(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) parseSSASettings(pHybridNode, cHybrid, os);
                    }
                    else if(cName == "SSA2")
                    {
                        cHybrid->setNStripChips(cHybrid->getNStripChips() + 1);
                        pBoard->setFrontEndType(FrontEndType::SSA2);
                        parseSSA2Container(cChild, cHybrid, cConfigFileDirectory, os);
                    }
                    else if(cName == "SSA2")
                    {
                        cHybrid->setNStripChips(cHybrid->getNStripChips() + 1);
                        LOG(INFO) << BOLDBLUE << "Implement for SSA2" << RESET;
                        pBoard->setFrontEndType(FrontEndType::SSA2);
                        parseSSA2Container(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) parseSSASettings(pHybridNode, cHybrid, os);
                    }
                    else if(cName == "MPA")
                    {
                        cHybrid->setNPixelChips(cHybrid->getNPixelChips() + 1);
                        pBoard->setFrontEndType(FrontEndType::MPA);
                        parseMPAContainer(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) parseMPASettings(pHybridNode, cHybrid, os);
                    }
                    else if(cName == "MPA2") // Irene
                    {
                        std::cout << " MPA2 "<<std::endl;
                        cHybrid->setNPixelChips(cHybrid->getNPixelChips() + 1);
                        pBoard->setFrontEndType(FrontEndType::MPA2);
                        parseMPA2Container(cChild, cHybrid, cConfigFileDirectory, os);
                        if(cNextName.empty() || cNextName != cName) parseMPASettings(pHybridNode, cHybrid, os);
                    }
                }
            }
        }

        if(pBoard->getBoardType() == BoardType::RD53 && pOpticalGroup->flpGBT != nullptr)
            parseHybridToLpGBT(pHybridNode, cHybrid, pOpticalGroup->flpGBT, os);
        else if(pBoard->getBoardType() != BoardType::RD53)
            parseGlobalHybridMask(pHybridNode, cHybrid, os);
    }
}

void FileParser::parseGlobalHybridMask(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    os << BOLDCYAN << "|"
       << "  Parsing global hybrid settings "
       << "\n";

    pugi::xml_node cGlobalSettingsNode = pHybridNode.child("Global");
    for(pugi::xml_node cChildGlobal: cGlobalSettingsNode.children())
    {
        std::string cName = cChildGlobal.name();
        if(cName.find("Masked") == std::string::npos) continue;

        os << BOLDCYAN << "\t|\t|\t|" << cName << "\n";

        std::vector<uint8_t>                     cChipIds(0);
        std::map<uint8_t, std::vector<uint16_t>> cMapOfMaks;  // key cChipId, ChannelIds
        std::map<uint8_t, FrontEndType>          cMapOfTypes; // key cChipId, value Type
        for(const pugi::xml_attribute cAttribute: cChildGlobal.attributes())
        {
            std::string       cAttrName = cAttribute.name();
            std::string       cList     = std::string(cAttribute.value());
            std::string       ctoken;
            std::stringstream cStr(cList);
            os << GREEN << "|\t|\t|\t|---- " << cAttrName << " : ";
            char cDelimiter = ';';
            if(cAttrName.find("Id") != std::string::npos)
            {
                while(std::getline(cStr, ctoken, cDelimiter))
                {
                    uint8_t cItem = convertAnyInt(ctoken.c_str());
                    os << GREEN << "|\n|\t|\t|\t|\t|----- " << +cItem;
                    // check if item exists in map
                    cChipIds.push_back(cItem);
                    auto cIter = cMapOfMaks.find(cItem);
                    if(cIter == cMapOfMaks.end())
                    {
                        std::vector<uint16_t> cMskedChnls;
                        cMapOfMaks[cItem]  = cMskedChnls;
                        FrontEndType cType = FrontEndType::CBC3;
                        if(cAttrName.find("MPA") != std::string::npos) cType = FrontEndType::MPA;
                        if(cAttrName.find("MPA2") != std::string::npos) cType = FrontEndType::MPA2;
                        if(cAttrName.find("SSA") != std::string::npos) cType = FrontEndType::SSA;
                        if(cAttrName.find("SSA2") != std::string::npos) cType = FrontEndType::SSA2;
                        if(cAttrName.find("CBCId") != std::string::npos) cType = FrontEndType::CBC3;
                        cMapOfTypes[cItem] = cType;
                    }
                    else
                        cMapOfMaks[cItem].clear();
                }
            }
            if(cAttrName.find("Rows") != std::string::npos)
            {
                std::string cPrimaryToken;
                int         cIndx = 0;
                while(std::getline(cStr, cPrimaryToken, cDelimiter))
                {
                    std::stringstream cPrim(cPrimaryToken);
                    while(std::getline(cPrim, ctoken, ','))
                    {
                        auto    cChipId   = cChipIds[cIndx];
                        uint8_t cItem     = convertAnyInt(ctoken.c_str());
                        auto    cChipType = cMapOfTypes[cChipId];
                        os << GREEN << "|\n|\t|\t|\t|\t|" << +cChipId << "\t|\t|\t|\t|\t|----- " << +cItem;
                        cMapOfMaks[cChipId].push_back(cItem + (cChipType == FrontEndType::CBC3 ? 0 : 1));
                    }
                    cIndx++;
                }
            }
            os << "\n";
        }

        std::sort(cChipIds.begin(), cChipIds.end());
        cChipIds.erase(std::unique(cChipIds.begin(), cChipIds.end()), cChipIds.end());
        for(auto cChipId: cChipIds)
        {
            auto        cType        = cMapOfTypes[cChipId];
            std::string cRegNameBase = "";
            if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
            {
                os << GREEN << "|\t|\t|\t|\t| ---- ChipId" << +cChipId << " have " << cMapOfMaks[cChipId].size() << " MPA pixels to mask..."
                   << "\n";
                cRegNameBase = "ENFLAGS_P";
            }
            else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
            {
                os << GREEN << "|\t|\t|\t|\t| ---- ChipId" << +cChipId << " have " << cMapOfMaks[cChipId].size() << " SSA strips to mask..."
                   << "\n";
                cRegNameBase = "ENFLAGS_S";
            }
            else
            {
                os << GREEN << "|\t|\t|\t|\t| ---- ChipId" << +cChipId << " have " << cMapOfMaks[cChipId].size() << " CBC strips to mask..."
                   << "\n";
                cRegNameBase = "MaskChannel-";
            }

            // configure register map for each chip
            for(auto cChnlId: cMapOfMaks[cChipId])
            {
                std::stringstream cRegName;
                cRegName << cRegNameBase;
                // get the index of the bit to shift
                uint8_t cBitShift  = 0;
                uint8_t cMaskValue = 0;
                if(cType == FrontEndType::CBC3)
                {
                    uint8_t cRegisterIndex = 1 + 8 * (cChnlId / 8);
                    cRegName << std::setfill('0') << std::setw(3) << +(7 + cRegisterIndex) << "-to-" << std::setfill('0') << std::setw(3) << +(cRegisterIndex);
                    cBitShift = (cChnlId) % 8;
                }
                else
                {
                    cRegName << cChnlId;
                }
                // get the original value of the register
                os << GREEN << "|\t|\t|\t|\t|\t|  ---- Preparing registers to mask channel " << +cChnlId << " - controled by register " << cRegName.str() << " \n";
                for(auto cChip: *pHybrid)
                {
                    if(cChip->getId() != cChipId) continue;
                    auto    cRegValue = cChip->getReg(cRegName.str());
                    uint8_t cRegMask  = (0x1 << cBitShift); //
                    cRegMask          = ~(cRegMask);
                    uint8_t cValue    = (cRegValue & cRegMask) | (cMaskValue << cBitShift);
                    // write the new value
                    os << GREEN << "|\t|\t|\t|\t|\t|\t|  ---- register set to 0x" << std::hex << +cValue << std::dec << "\n";
                    cChip->setReg(cRegName.str(), cValue);
                }
            }

            // set original mask for each Chip
            for(auto cChip: *pHybrid)
            {
                if(cChip->getId() != cChipId) continue;
                os << GREEN << "|\t|\t|\t|\t|\t   ---- Applying channel mask to Chip" << +cChip->getId() << "\n";
                cChip->setChipOriginalMask(cMapOfMaks[cChipId]);
            }
        }
        if(cMapOfMaks.size() == 0)
        {
            os << BOLDCYAN << "\t|\t|\t| Nothing masked on hybrid#" << +pHybrid->getId() << "\n";
            for(auto cChip: *pHybrid)
            {
                os << GREEN << "|\t|\t|\t|\t|\t   ---- Applying no channel mask to Chip" << +cChip->getId() << "\n";
                std::vector<uint16_t> cEmptyList(0);
                cChip->setChipOriginalMask(cEmptyList);
            }
        }
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

    uint32_t     cChipId = pCbcNode.attribute("Id").as_uint();
    ReadoutChip* cCbc    = cHybrid->addChipContainer(cChipId, new Cbc(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getOpticalGroupId(), cHybrid->getId(), cChipId, cFileName));
    cCbc->setOptical(cHybrid->isOptical());
    cCbc->setClockFrequency(320);
    cCbc->setNumberOfChannels(254);
    cCbc->setMasterId(cHybrid->getMasterId());

    os << BOLDCYAN << "|"
       << "  "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "   "
       << "|"
       << "---- CBC controlled by I2CMaster " << +cCbc->getMasterId() << RESET << std::endl;

    // parse the specific CBC settings so that Registers take precedence
    parseCbcSettings(pCbcNode, cCbc, os);

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
                parseCbcSettings(cGlobalCbcSettingsNode, static_cast<ReadoutChip*>(cCbc), os);
            else
            {
                std::ofstream cDummy;
                parseCbcSettings(cGlobalCbcSettingsNode, static_cast<ReadoutChip*>(cCbc), cDummy);
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
            pCbc->setReg("TestPulseDel&ChanGroup", reverseBits<8>((cChanGroup & 0x07) << 5 | (cDelay & 0x1F)));
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
        uint8_t cPipeLogic, cStubLogic, cOr254, cTestClock, cTpgClock, cDll, cHIPcount;
        uint8_t cAmuxValue;

        cPipeLogic = convertAnyInt(cMiscNode.attribute("pipelogic").value());
        cStubLogic = convertAnyInt(cMiscNode.attribute("stublogic").value());
        cOr254     = convertAnyInt(cMiscNode.attribute("or254").value());
        cHIPcount  = convertAnyInt(cMiscNode.attribute("hipCount").value());
        cDll       = reverseBits<5>(static_cast<uint8_t>(convertAnyInt(cMiscNode.attribute("dll").value())));
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

            ChipRegMask cMask;
            cMask.fNbits    = 3;
            cMask.fBitShift = 5;
            pCbc->setRegBits("HIP&TestMode", cMask, cHIPcount);

            os << GREEN << "|\t|\t|\t|----Misc Settings: "
               << " PipelineLogicSource: " << RED << +cPipeLogic << GREEN << ", StubLogicSource: " << RED << +cStubLogic << GREEN << ", OR254: " << RED << +cOr254 << GREEN << ", TPG Clock: " << RED
               << +cTpgClock << GREEN << ", Test Clock 40: " << RED << +cTestClock << GREEN << ", DLL: " << RED << convertAnyInt(cMiscNode.attribute("dll").value()) << ", HIPCount: " << +cHIPcount
               << RESET << std::endl;
        }

        os << GREEN << "|\t|\t|\t|----Analog Mux "
           << "value: " << RED << +cAmuxValue << " (0x" << std::hex << +cAmuxValue << std::dec << ", 0b" << std::bitset<5>(cAmuxValue) << ")" << RESET << std::endl;
    }
}

void FileParser::parseSettings(const std::string& pFilename, SettingsMap& pSettingsMap, std::ostream& os)
{
    pugi::xml_document doc;
    openHWconfig(pFilename, doc);

    if(doc.child("HwDescription").child("Settings") == 0) LOG(WARNING) << BOLDRED << "No -Settings- tag found in XML file: " << BOLDYELLOW << pFilename << RESET;
    for(pugi::xml_node nSettings = doc.child("HwDescription").child("Settings"); nSettings == doc.child("HwDescription").child("Settings") && nSettings != 0; nSettings = nSettings.next_sibling())
    {
        os << std::endl;

        for(pugi::xml_node nSetting = nSettings.child("Setting"); nSetting; nSetting = nSetting.next_sibling())
        {
            if((strcmp(nSetting.attribute("name").value(), "RegNameDAC1") == 0) || (strcmp(nSetting.attribute("name").value(), "RegNameDAC2") == 0) ||
               (strcmp(nSetting.attribute("name").value(), "DataOutputDir") == 0) || (strcmp(nSetting.attribute("name").value(), "KIRA_ID") == 0))
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

            // #############################################################################################
            // # Retrieve links, groups and channels from CIC node attirbutes and propagate to LpGBT class #
            // #############################################################################################
            plpGBT->addRxGroups(cRxGroups);
            plpGBT->addRxChannels(cRxChannels);
            plpGBT->addTxGroups(cTxGroups);
            plpGBT->addTxChannels(cTxChannels);

            // ################################################################
            // # In the case of IT propagate LpGBT mapping the front-end chip #
            // ################################################################
            uint8_t cChipId = cChild.attribute("Id").as_uint();
            static_cast<RD53*>(cHybrid->getObject(cChipId))->setRxGroup(cRxGroups[0]);
            static_cast<RD53*>(cHybrid->getObject(cChipId))->setRxChannel(cRxChannels[0]);
            static_cast<RD53*>(cHybrid->getObject(cChipId))->setTxGroup(cTxGroups[0]);
            static_cast<RD53*>(cHybrid->getObject(cChipId))->setTxChannel(cTxChannels[0]);
        }
    }
}

// ########################
// # RD53 specific parser #
// ########################
template <class T, size_t N>
auto parseString(const std::string& data)
{
    std::array<T, N> result;
    std::transform(data.begin(), data.end(), result.begin(), [](char c) { return c - '0'; });
    return result;
}

void FileParser::parseRD53(pugi::xml_node theChipNode, Hybrid* cHybrid, std::string cFilePrefix, std::ostream& os, const FrontEndType& frontEndType)
{
    std::string cFileName;

    if(cFilePrefix.empty() == false)
    {
        if(cFilePrefix.at(cFilePrefix.length() - 1) != '/') cFilePrefix.append("/");
        cFileName = cFilePrefix + expandEnvironmentVariables(theChipNode.attribute("configfile").value());
    }
    else
        cFileName = expandEnvironmentVariables(theChipNode.attribute("configfile").value());

    const uint32_t    chipId     = theChipNode.attribute("Id").as_uint();
    const uint32_t    chipLane   = theChipNode.attribute("Lane").as_uint();
    const uint8_t     cRxGroup   = theChipNode.attribute("RxGroups").as_uint();
    const uint8_t     cRxChannel = theChipNode.attribute("RxChannels").as_uint();
    const uint8_t     cTxGroup   = theChipNode.attribute("TxGroups").as_uint();
    const uint8_t     cTxChannel = theChipNode.attribute("TxChannels").as_uint();
    const std::string cfgComment = theChipNode.attribute("Comment").as_string();

    os << BOLDBLUE << "|\t|\t|----" << theChipNode.name() << " --> Id: " << BOLDYELLOW << chipId << BOLDBLUE << ", Lane: " << BOLDYELLOW << chipLane << BOLDBLUE << ", File: " << BOLDYELLOW
       << cFileName << BOLDBLUE << ", RxGroup: " << BOLDYELLOW << +cRxGroup << BOLDBLUE << ", RxChannel: " << BOLDYELLOW << +cRxChannel << BOLDBLUE << ", TxGroup: " << BOLDYELLOW << +cTxGroup
       << BOLDBLUE << ", TxChannel: " << BOLDYELLOW << +cTxChannel << BOLDBLUE << ", Comment: " << BOLDYELLOW << cfgComment << RESET << std::endl;

    ReadoutChip* theChip;
    if(frontEndType == FrontEndType::RD53A)
        theChip = cHybrid->addChipContainer(chipId, new RD53A(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getOpticalGroupId(), cHybrid->getId(), chipId, chipLane, cFileName, cfgComment));
    else
        theChip = cHybrid->addChipContainer(chipId, new RD53B(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getOpticalGroupId(), cHybrid->getId(), chipId, chipLane, cFileName, cfgComment));
    theChip->setNumberOfChannels(static_cast<RD53*>(theChip)->getNRows(), static_cast<RD53*>(theChip)->getNCols());

    parseRD53Settings(theChipNode, theChip, os);
}

void FileParser::parseGlobalRD53Settings(pugi::xml_node pHybridNode, Hybrid* pHybrid, std::ostream& os)
{
    // ###############################################
    // # Load frontend chip global register settings #
    // ###############################################
    pugi::xml_node cGlobalChipSettings = pHybridNode.child("Global");
    if(cGlobalChipSettings != nullptr)
    {
        os << BOLDCYAN << "|\t|\t|----Global RD53 Settings:" << RESET << std::endl;

        for(const pugi::xml_attribute& attr: cGlobalChipSettings.attributes())
        {
            const std::string regname  = attr.name();
            const uint16_t    regvalue = convertAnyInt(attr.value());

            for(auto theChip: *pHybrid)
            {
                if(static_cast<std::string>(attr.value()).empty() == false)
                    theChip->getRegItem(regname).fDefValue = regvalue;
                else
                    os << BOLDBLUE << "|\t|\t|\t|\t|----Chip Id: " << BOLDYELLOW << theChip->getId() << BOLDBLUE << " " << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase
                       << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;
                theChip->getRegItem(regname).fPrmptCfg = true;
            }

            if(static_cast<std::string>(attr.value()).empty() == false)
                os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << regvalue << std::dec << " (" << regvalue << ")" << RESET << std::endl;
        }
    }
}

void FileParser::parseRD53Settings(pugi::xml_node theChipNode, ReadoutChip* theChip, std::ostream& os)
{
    // #########################################
    // # Load frontend chip lane configuration #
    // #########################################
    pugi::xml_node laneConfigNode = theChipNode.child("LaneConfig");
    if(laneConfigNode != nullptr)
    {
        const bool    isPrimary = laneConfigNode.attribute("primary").as_bool(true);
        const uint8_t master    = laneConfigNode.attribute("master").as_uint(0);

        const std::string outputLanesConfig = laneConfigNode.attribute("outputLanes").as_string("0001");
        if(outputLanesConfig.size() != 4) throw std::runtime_error("The \"outputLanes\" attribute of LaneConfig should contain 4 characters ('0' up to '4').");
        auto outputLanesEnabled = parseString<uint8_t, 4>(outputLanesConfig);

        const std::string singleChannelInputsConfig = laneConfigNode.attribute("singleChannelInputs").as_string("0000");
        if(singleChannelInputsConfig.size() != 4) throw std::runtime_error("The \"singleChannelInputs\" attribute of LaneConfig should contain 4 characters ('0' or '1').");
        auto singleChannelInputs = parseString<bool, 4>(singleChannelInputsConfig);

        const std::string dualChannelInputConfig = laneConfigNode.attribute("dualChannelInput").as_string("0000");
        if(dualChannelInputConfig.size() != 4) throw std::runtime_error("The \"dualChannelInput\" attribute of LaneConfig should contain 4 characters ('0' or '1').");
        auto dualChannelInput = parseString<bool, 4>(dualChannelInputConfig);

        static_cast<RD53*>(theChip)->laneConfig = LaneConfig(isPrimary, master, outputLanesEnabled, singleChannelInputs, dualChannelInput);

        os << BOLDBLUE << "|\t|\t|\t|----Lanes configuration --> primary: " << BOLDYELLOW << isPrimary << BOLDBLUE << " - master: " << BOLDYELLOW << +master << BOLDBLUE
           << " - output lanes: " << BOLDYELLOW << outputLanesConfig << BOLDBLUE << " - single channel inputs: " << BOLDYELLOW << singleChannelInputsConfig << BOLDBLUE
           << " - dual channel input: " << BOLDYELLOW << dualChannelInputConfig << RESET << std::endl;
    }

    // ########################################
    // # Load frontend chip register settings #
    // ########################################
    const pugi::xml_node cLocalChipSettings = theChipNode.child("Settings");
    if(cLocalChipSettings != nullptr)
    {
        if(theChip->getFrontEndType() == FrontEndType::RD53A)
            os << BOLDCYAN << "|\t|\t|----FrontEndType: " << BOLDYELLOW << "RD53A" << RESET << std::endl;
        else
            os << BOLDCYAN << "|\t|\t|----FrontEndType: " << BOLDYELLOW << "RD53B" << RESET << std::endl;

        for(const pugi::xml_attribute& attr: cLocalChipSettings.attributes())
        {
            const std::string regname = attr.name();

            if(static_cast<std::string>(attr.value()).empty() == false)
            {
                theChip->getRegItem(regname).fDefValue = convertAnyInt(attr.value());

                os << GREEN << "|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << theChip->getRegItem(regname).fDefValue << std::dec << " ("
                   << theChip->getRegItem(regname).fDefValue << ")" << RESET << std::endl;
            }
            else
                os << BOLDBLUE << "|\t|\t|\t|\t|----" << regname << ": " << BOLDYELLOW << std::hex << "0x" << std::uppercase << theChip->getRegItem(regname).fDefValue << std::dec << " ("
                   << theChip->getRegItem(regname).fDefValue << ")" << RESET << std::endl;
            theChip->getRegItem(regname).fPrmptCfg = true;
        }
    }
}
// ########################

std::string FileParser::parseMonitor(const std::string& pFilename, DetectorMonitorConfig& theDetectorMonitorConfig, std::ostream& os)
{
    pugi::xml_document doc;
    openHWconfig(pFilename, doc);

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

    for(pugi::xml_node monitorElement = theMonitorNode.child("MonitoringElement"); monitorElement; monitorElement = monitorElement.next_sibling())
    {
        if(convertAnyInt(monitorElement.attribute("enable").value()) == 0) continue;

        const std::string chipName     = monitorElement.attribute("device").value();
        const std::string registerName = monitorElement.attribute("register").value();
        os << BOLDRED << "Monitoring" << RESET << " -- " << BOLDCYAN << chipName << RESET << ":" << BOLDYELLOW << "Register " << registerName << RESET << std::endl;
        theDetectorMonitorConfig.addElementToMonitor(chipName, registerName);
    }

    if(theDetectorMonitorConfig.getNumberOfMonitoredRegisters() == 0) return "None";
    return theMonitorNode.attribute("type").value();
}

void FileParser::parseCommunicationSettings(const std::string& pFilename, CommunicationSettingConfig& theCommunicationSettingConfig, std::ostream& os)
{
    pugi::xml_document doc;
    openHWconfig(pFilename, doc);
    auto theCommunicationSettingsNode = doc.child("HwDescription").child("CommunicationSettings");
    if(bool(theCommunicationSettingsNode))
    {
        auto retrieveDQMParameters = [&theCommunicationSettingsNode](CommunicationSettingConfig::CommunicationSetting& theCommunicationSetting, const std::string& theNodeName) {
            auto theDQMnode = theCommunicationSettingsNode.child(theNodeName.c_str());
            if(bool(theDQMnode))
            {
                theCommunicationSetting = CommunicationSettingConfig::CommunicationSetting(std::string(theDQMnode.attribute("ip").value()),
                                                                                           uint16_t(convertAnyInt(theDQMnode.attribute("port").value())),
                                                                                           bool(convertAnyInt(theDQMnode.attribute("enableConnection").value())));
            }
        };

        retrieveDQMParameters(theCommunicationSettingConfig.fControllerCommunication, "Controller");
        retrieveDQMParameters(theCommunicationSettingConfig.fDQMCommunication, "DQM");
        retrieveDQMParameters(theCommunicationSettingConfig.fMonitorDQMCommunication, "MonitorDQM");
        retrieveDQMParameters(theCommunicationSettingConfig.fPowerSupplyDQMCommunication, "PowerSupplyClient");
    }
}

} // namespace Ph2_Parser
