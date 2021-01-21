/*!
  \file                    FileParser.h
  \brief                   Class to parse configuration files
  \author                  Georg Auzinger
  \version                 1.0
  \date                    01/07/2016
  Support :                mail to : georg.auzinger@SPAMNOT.cern.sh
*/

#ifndef FILEPARSER_H
#define FILEPARSER_H

#include "../HWDescription/Chip.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OpticalGroup.h"
#include "../HWInterface/BeBoardFWInterface.h"
#include "../HWInterface/D19cFWInterface.h"
#include "../HWInterface/RD53FWInterface.h"
#include "../Utils/ConditionDataSet.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Exception.h"
#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"

#include "pugixml.hpp"
#include <iostream>
#include <stdlib.h>
#include <string>
#include <unordered_map>
#include <vector>

/*!
 * \namespace Ph2_System
 * \brief Namespace regrouping the framework wrapper
 */
namespace Ph2_System
{
using BeBoardVec   = std::vector<Ph2_HwDescription::BeBoard*>;                 /*!< Vector of Board pointers */
using BeBoardFWMap = std::map<uint16_t, Ph2_HwInterface::BeBoardFWInterface*>; /*!< Map of Board connected */
using SettingsMap  = std::unordered_map<std::string, double>;                  /*!< Maps the settings */

/*!
 * \class FileParser
 * \brief parse a predefined HW structure
 */
class FileParser
{
  public:
    FileParser() {}
    ~FileParser() {}

    void parseHW(const std::string& pFilename, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os, bool pIsFile);
    void parseSettings(const std::string& pFilename, SettingsMap& pSettingsMap, std::ostream& os, bool pIsFile);

  protected:
    /*!
     * \convert a voltage level to it's 8bit DAC value
     * \param pVoltage: the Voltage level
     * \return corresponding 8-bit DAC value
     */
    uint32_t Vto8Bit(float pVoltage) { return static_cast<uint32_t>(pVoltage / 3.3 * 256 + 0.5); }

  private:
    /*!
     * \brief Initialize the hardware via  XML config file
     * \param pFilename : HW Description file
     *\param os : ostream to dump output
     */
    void parseHWxml(const std::string& pFilename, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os, bool pIsFile);

    /*!
     * \brief Initialize the hardware via xml config file
     * \param pFilename : HW Description file
     *\param os : ostream to dump output
     */
    void parseSettingsxml(const std::string& pFilename, SettingsMap& pSettingsMap, std::ostream& os, bool pIsFile);

    void parseBeBoard(pugi::xml_node pBeBordNode, BeBoardFWMap& pBeBoardFWMap, DetectorContainer* pDetectorContainer, std::ostream& os);
    void parseRegister(pugi::xml_node pRegisterNode, std::string& pAttributeString, uint32_t& pValue, Ph2_HwDescription::BeBoard* pBoard, std::ostream& os);
    void parseSLink(pugi::xml_node pSLinkNode, Ph2_HwDescription::BeBoard* pBoard, std::ostream& os);
    void parseOpticalGroupContainer(pugi::xml_node pOpticalGroupNode, Ph2_HwDescription::BeBoard* pBoard, std::ostream& os);
    void parseModuleContainer(pugi::xml_node pModuleNode, Ph2_HwDescription::OpticalGroup* pOpticalGroup, std::ostream& os, Ph2_HwDescription::BeBoard* pBoard);
    void parseCbcContainer(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* cModule, std::string cFilePrefix, std::ostream& os);
    void parseCbcSettings(pugi::xml_node pCbcNode, Ph2_HwDescription::ReadoutChip* pCbc, std::ostream& os);
    void parseGlobalCbcSettings(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* pModule, std::ostream& os);
    //
    void parseSSAContainer(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* cModule, std::string cFilePrefix, std::ostream& os);
    void parseSSASettings(pugi::xml_node pModuleNode, Ph2_HwDescription::ReadoutChip* cSSA);
    //
    void parseMPA(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* cModule, std::string cFilePrefix);
    void parseMPASettings(pugi::xml_node pModuleNode, Ph2_HwDescription::ReadoutChip* cMPA);

    // ########################
    // # RD53 specific parser #
    // ########################
    void parseRD53(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* cModule, std::string cFilePrefix, std::ostream& os);
    void parseRD53Settings(pugi::xml_node pRd53Node, Ph2_HwDescription::ReadoutChip* pRD53, std::ostream& os);
    void parseGlobalRD53Settings(pugi::xml_node pModuleNode, Ph2_HwDescription::Module* pModule, std::ostream& os);
    // ########################

    std::map<uint8_t, std::string> ChannelMaskMapCBC2 = {
        {0, "MaskChannelFrom008downto001"},  {1, "MaskChannelFrom016downto009"},  {2, "MaskChannelFrom024downto017"},  {3, "MaskChannelFrom032downto025"},  {4, "MaskChannelFrom040downto033"},
        {5, "MaskChannelFrom048downto041"},  {6, "MaskChannelFrom056downto049"},  {7, "MaskChannelFrom064downto057"},  {8, "MaskChannelFrom072downto065"},  {9, "MaskChannelFrom080downto073"},
        {10, "MaskChannelFrom088downto081"}, {11, "MaskChannelFrom096downto089"}, {12, "MaskChannelFrom104downto097"}, {13, "MaskChannelFrom112downto105"}, {14, "MaskChannelFrom120downto113"},
        {15, "MaskChannelFrom128downto121"}, {16, "MaskChannelFrom136downto129"}, {17, "MaskChannelFrom144downto137"}, {18, "MaskChannelFrom152downto145"}, {19, "MaskChannelFrom160downto153"},
        {20, "MaskChannelFrom168downto161"}, {21, "MaskChannelFrom176downto169"}, {22, "MaskChannelFrom184downto177"}, {23, "MaskChannelFrom192downto185"}, {24, "MaskChannelFrom200downto193"},
        {25, "MaskChannelFrom208downto201"}, {26, "MaskChannelFrom216downto209"}, {27, "MaskChannelFrom224downto217"}, {28, "MaskChannelFrom232downto225"}, {29, "MaskChannelFrom240downto233"},
        {30, "MaskChannelFrom248downto241"}, {31, "MaskChannelFrom254downto249"}};

    std::map<uint8_t, std::string> ChannelMaskMapCBC3 = {
        {0, "MaskChannel-008-to-001"},  {1, "MaskChannel-016-to-009"},  {2, "MaskChannel-024-to-017"},  {3, "MaskChannel-032-to-025"},  {4, "MaskChannel-040-to-033"},  {5, "MaskChannel-048-to-041"},
        {6, "MaskChannel-056-to-049"},  {7, "MaskChannel-064-to-057"},  {8, "MaskChannel-072-to-065"},  {9, "MaskChannel-080-to-073"},  {10, "MaskChannel-088-to-081"}, {11, "MaskChannel-096-to-089"},
        {12, "MaskChannel-104-to-097"}, {13, "MaskChannel-112-to-105"}, {14, "MaskChannel-120-to-113"}, {15, "MaskChannel-128-to-121"}, {16, "MaskChannel-136-to-129"}, {17, "MaskChannel-144-to-137"},
        {18, "MaskChannel-152-to-145"}, {19, "MaskChannel-160-to-153"}, {20, "MaskChannel-168-to-161"}, {21, "MaskChannel-176-to-169"}, {22, "MaskChannel-184-to-177"}, {23, "MaskChannel-192-to-185"},
        {24, "MaskChannel-200-to-193"}, {25, "MaskChannel-208-to-201"}, {26, "MaskChannel-216-to-209"}, {27, "MaskChannel-224-to-217"}, {28, "MaskChannel-232-to-225"}, {29, "MaskChannel-240-to-233"},
        {30, "MaskChannel-248-to-241"}, {31, "MaskChannel-254-to-249"}};
};
} // namespace Ph2_System

#endif
