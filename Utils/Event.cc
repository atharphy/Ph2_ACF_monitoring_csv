/*
  FileName :                     Event.cc
  Content :                      Event handling from DAQ
  Programmer :                   Nicolas PIERRE
  Version :                      1.0
  Date of creation :             10/07/14
  Support :                      mail to : nicolas.pierre@icloud.com
*/

#include "Utils/Event.h"
#include "Utils/easylogging++.h"
#include "Utils/ConsoleColor.h"
#include "Utils/DataContainer.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{

void Event::fillDataContainer(BoardDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid) { fillChipDataContainer(chip, testChannelGroup, hybrid->getId()); }
        }
    }
}

} // namespace Ph2_HwInterface
