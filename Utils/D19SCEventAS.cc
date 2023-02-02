#include "Utils/D19SCEventAS.h"
#include "HWDescription/Definition.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/DataContainer.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"

#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19SCEventAS::D19SCEventAS(const BeBoard* pBoard, uint32_t pNSSA, uint32_t pNHybrid, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA * pNHybrid)
{
    fNSSA = pNSSA;
    SetEvent(pBoard, pNSSA, list);
}
D19SCEventAS::D19SCEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fEventDataVector.clear();
    fNSSA = 0;
    fHybridIds.clear();
    fChipIds.clear();
    fCounterData.clear();
    // assuming that FEIds aren't shared between links
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            fHybridIds.push_back(cHybrid->getId());
            fNSSA += cHybrid->size();
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cChipIds(0);
            cChipIds.clear();
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    ChipCounterData cChipData;
                    cChipData.clear();
                    cHybridCounterData.push_back(cChipData);
                    cChipIds.push_back(cChip->getId());
                }
            } // chip
            fCounterData.push_back(cHybridCounterData);
            fChipIds.push_back(cChipIds);
        } // hybrids
    }     // opticalGroup
    this->Set(pBoard, list);
}
void D19SCEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async SSA " << RESET;
    auto    cDataIterator = pData.begin();
    uint8_t cHybridIndex  = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto&   cHybridCounterData = fCounterData[cHybridIndex];
            uint8_t cChipIndex         = 0;
            // loop over chips
            for(auto cChip: *cHybrid)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    auto& cChipCounterData = cHybridCounterData[cChipIndex];
                    for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        if(cChnl % 2 != 0)
                        {
                            auto cWord = *(cDataIterator);
                            cChipCounterData.push_back((cWord & 0xFFFF));
                            cChipCounterData.push_back((cWord & (0xFFFF << 16)) >> 16);
                            LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChipIndex << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                            cDataIterator++;
                        } //
                    }     // chnl loop
                    cChipIndex++;
                } //[if SSA]
            }     // chips
            cHybridIndex++;
        } // hybrids
    }     // opticalGroup
}

// required by event but not sure if makes sense for AS
void D19SCEventAS::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId)
{
    std::vector<uint32_t> hVec = GetHits(hybridId, chipContainer->getId());
    unsigned int          i    = 0;

    for(ChannelContainer<Occupancy>::iterator channel = chipContainer->begin<Occupancy>(); channel != chipContainer->end<Occupancy>(); channel++, i++)
    {
        if(testChannelGroup->isChannelEnabled(i)) { channel->fOccupancy += hVec[i]; }
    }
}

void D19SCEventAS::SetEvent(const BeBoard* pBoard, uint32_t pNSSA, const std::vector<uint32_t>& list)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            uint32_t nc = 0;
            for(auto cChip: *cHybrid)
            {
                fEventDataVector[encodeVectorIndex(cHybrid->getId(), cChip->getId(), pNSSA)] = std::vector<uint32_t>(list.begin() + nc * 120, list.begin() + (nc + 1) * 120);
                // std::cout<<fEventDataVector[encodeVectorIndex (cHybrid->getId(), cChip->getId(), pNSSA)
                // ][5]<<std::endl;

                nc += 1;
            }
        }
    }
}

uint32_t D19SCEventAS::GetNHits(uint8_t pHybridId, uint8_t pSSAId) const
{
    uint8_t cHybridIndex = getHybridIndex(pHybridId);
    uint8_t cChipIndex   = getChipIndex(pHybridId, pSSAId);
    auto&   cHitVecotr   = fCounterData.at(cHybridIndex).at(cChipIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
}
std::vector<uint32_t> D19SCEventAS::GetHits(uint8_t pHybridId, uint8_t pSSAId) const
{
    uint8_t cHybridIndex = getHybridIndex(pHybridId);
    uint8_t cChipIndex   = getChipIndex(pHybridId, pSSAId);
    return fCounterData.at(cHybridIndex).at(cChipIndex);
}

} // namespace Ph2_HwInterface
