/*!

        Filename :                              Module.cc
        Content :                               Module Description class
        Programmer :                    Lorenzo BIDEGAIN
        Version :               1.0
        Date of Creation :              25/06/14
        Support :                               mail to : lorenzo.bidegain@gmail.com

 */

#include "OuterTrackerModule.h"

namespace Ph2_HwDescription
{
// Default C'tor
OuterTrackerModule::OuterTrackerModule() : Module(), fCic(nullptr) {}

OuterTrackerModule::OuterTrackerModule(const FrontEndDescription& pFeDesc, uint8_t pModuleId) : Module(pFeDesc, pModuleId), fCic(nullptr) {}

OuterTrackerModule::OuterTrackerModule(uint8_t pBeId, uint8_t pFMCId, uint8_t pFeId, uint8_t pModuleId) : Module(pBeId, pFMCId, pFeId, pModuleId), fCic(nullptr) {}

} // namespace Ph2_HwDescription