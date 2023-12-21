/*!
  Filename :                              OpticalGroup.cc
  Content :                               OpticalGroup Description class
  Programmer :                    Lorenzo BIDEGAIN
  Version :               1.0
  Date of Creation :              25/06/14
  Support :                               mail to : lorenzo.bidegain@gmail.com
*/

#include "OpticalGroup.h"

namespace Ph2_HwDescription
{
// Default C'tor
OpticalGroup::OpticalGroup() : FrontEndDescription(), OpticalGroupContainer(0) {}

OpticalGroup::OpticalGroup(const FrontEndDescription& pFeDesc, uint8_t pOpticalGroupId) : FrontEndDescription(pFeDesc), OpticalGroupContainer(pOpticalGroupId) {}

OpticalGroup::OpticalGroup(uint8_t pBeBoardId, uint8_t pFMCId, uint8_t pOpticalGroupId) : FrontEndDescription(pBeBoardId, pFMCId, pOpticalGroupId, 0), OpticalGroupContainer(pOpticalGroupId) {}

void OpticalGroup::takeSnapshot()
{
    if(flpGBT != nullptr)
    {
        flpGBT->takeSnapshot();
    }  
}

void OpticalGroup::clearSnapshot()
{
    if(flpGBT != nullptr)
    {
        flpGBT->clearSnapshot();
    }
}

} // namespace Ph2_HwDescription
