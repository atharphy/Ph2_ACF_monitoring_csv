#include "Utils/D19cPSEventAS.h"
#include "HWDescription/Definition.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/DataContainer.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"
#include "Utils/DataContainer.h"
#include "Utils/ContainerFactory.h"
#include <algorithm>
#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{

bool D19cPSEventAS::fEnableFastReadout = true;

void D19cPSEventAS::configureFastReadout(bool enableFastReadout)
{
    fEnableFastReadout = enableFastReadout;
}

D19cPSEventAS::D19cPSEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    this->Set(pBoard, list);
}

void D19cPSEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    ContainerFactory::copyAndInitChannel<uint16_t>(*pBoard, fTheOccupancyContainer);
    if(fEnableFastReadout)
    {

    }
    else
    {
        size_t dataIndex = 0;
        for(auto theOpticalGroup: fTheOccupancyContainer)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    uint8_t numberOfCols = theChip->getNumberOfCols();
                    for(uint8_t row = 0; row < theChip->getNumberOfRows(); ++row)
                    {
                        for(uint8_t col = 0; col < numberOfCols; col+=2)
                        {
                            uint32_t rawCounter = pData.at(dataIndex++);
                            theChip->getChannel<uint16_t>(row, col) = rawCounter & 0x7F;
                            theChip->getChannel<uint16_t>(row, col+1) = (rawCounter >> 15) & 0x7F;
                        }
                    }
                }
            }
        }
    }
}

void D19cPSEventAS::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint8_t hybridId)
{
    if(testChannelGroup == nullptr) return;
    auto& theChipEventContainer = fTheOccupancyContainer.getChip(hybridId/2, hybridId%2, chipContainer->getId());

    for(uint8_t row = 0; row < theChipEventContainer->getNumberOfRows(); ++row)
    {
        for(uint8_t col = 0; col < theChipEventContainer->getNumberOfCols(); ++col)
        {
            if(testChannelGroup->isChannelEnabled(row, col))
            {
                chipContainer->getChannel<Occupancy>(row, col).fOccupancy += theChipEventContainer->getChannel<uint16_t>(row, col);
            }
        }
    }
}

uint32_t D19cPSEventAS::GetNHits(uint8_t pHybridId, uint8_t pChipId) const
{
    const auto& theChipEventChannelVector = fTheOccupancyContainer.getChip(pHybridId/2, pHybridId%2, pChipId)->getChannelContainer<uint16_t>();
    return std::accumulate(theChipEventChannelVector->begin(), theChipEventChannelVector->end(), 0);
}
} // namespace Ph2_HwInterface
