/*
  FileName :                    BeBoardFWInterface.cc
  Content :                     BeBoardFWInterface base class of all type of boards
  Programmer :                  Lorenzo BIDEGAIN, Nicolas PIERRE
  Version :                     1.0
  Date of creation :            31/07/14
  Support :                     mail to : lorenzo.bidegain@gmail.com, nico.pierre@icloud.com
*/

#include "BeBoardFWInterface.h"
#include "FpgaConfig.h"

namespace Ph2_HwInterface
{
BeBoardFWInterface::BeBoardFWInterface(const char* puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId), fSaveToFile(false), fFileHandler(nullptr), fFpgaConfig(nullptr)
{
}

BeBoardFWInterface::BeBoardFWInterface(const char* pId, const char* pUri, const char* pAddressTable)
    : RegManager(pId, pUri, pAddressTable), fSaveToFile(false), fFileHandler(nullptr), fFpgaConfig(nullptr)
{
}

std::string BeBoardFWInterface::readBoardType()
{
    std::string cBoardTypeString;

    uint32_t cBoardType = ReadReg("board_id");

    char cChar = ((cBoardType & cMask4) >> 24);
    cBoardTypeString.push_back(cChar);

    cChar = ((cBoardType & cMask3) >> 16);
    cBoardTypeString.push_back(cChar);

    cChar = ((cBoardType & cMask2) >> 8);
    cBoardTypeString.push_back(cChar);

    cChar = (cBoardType & cMask1);
    cBoardTypeString.push_back(cChar);

    return cBoardTypeString;
}

void BeBoardFWInterface::PowerOn() {}
void BeBoardFWInterface::PowerOff() {}
void BeBoardFWInterface::ReadVer() {}

} // namespace Ph2_HwInterface
