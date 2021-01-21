/*!
  Filename :                              Module.cc
  Content :                               Module Description class
  Programmer :                    Lorenzo BIDEGAIN
  Version :               1.0
  Date of Creation :              25/06/14
  Support :                               mail to : lorenzo.bidegain@gmail.com
*/

#include "Module.h"

namespace Ph2_HwDescription
{
// Default C'tor
Module::Module() : FrontEndDescription(), ModuleContainer(0) {}

Module::Module(const FrontEndDescription& pFeDesc, uint8_t pModuleId) : FrontEndDescription(pFeDesc), ModuleContainer(pModuleId) {}

Module::Module(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId) : FrontEndDescription(pBeId, pFMCId, pFeId), ModuleContainer(pModuleId) {}

Module::Module(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId, uint8_t pLinkId) : FrontEndDescription(pBeId, pFMCId, pFeId), ModuleContainer(pFeId), fLinkId(pLinkId) {}
} // namespace Ph2_HwDescription
