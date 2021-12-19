#include "../Utils/D19SCEventAS.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19SCEventAS::D19SCEventAS(const BeBoard* pBoard, uint32_t pNSSA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA * pNFe)
{
    fNSSA = pNSSA;
    SetEvent(pBoard, pNSSA, list);
}
D19SCEventAS::D19SCEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fEventDataVector.clear();
    fNSSA = 0;
    fFeIds.clear();
    fROCIds.clear();
    fCounterData.clear();
    // assuming that FEIds aren't shared between links
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            fFeIds.push_back(cFe->getId());
            fNSSA += cFe->size();
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cROCIds(0);
            cROCIds.clear();
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    RocCounterData cRocData;
                    cRocData.clear();
                    cHybridCounterData.push_back(cRocData);
                    cROCIds.push_back(cChip->getId());
                }
            } // chip
            fCounterData.push_back(cHybridCounterData);
            fROCIds.push_back(cROCIds);
        } // hybrids
    }     // opticalGroup
    this->Set(pBoard, list);
}
void D19SCEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async SSA " << RESET;
    auto    cDataIterator = pData.begin();
    uint8_t cFeIndex      = 0;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            auto&   cHybridCounterData = fCounterData[cFeIndex];
            uint8_t cRocIndex          = 0;
            // loop over chips
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    auto& cChipCounterData = cHybridCounterData[cRocIndex];
                    for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        if(cChnl % 2 != 0)
                        {
                            auto cWord = *(cDataIterator);
                            cChipCounterData.push_back((cWord & 0xFFFF));
                            cChipCounterData.push_back((cWord & (0xFFFF << 16)) >> 16);
                            LOG(DEBUG) << BOLDBLUE << "ROC#" << +cRocIndex << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                            cDataIterator++;
                        } //
                    }     // chnl loop
                    cRocIndex++;
                } //[if SSA]
            }     // chips
            cFeIndex++;
        } // hybrids
    }     // opticalGroup
}

// required by event but not sure if makes sense for AS
void D19SCEventAS::fillDataContainer(BoardDataContainer* boardContainer, const BoardDataContainer* theChannelGroupHandler, int groupNumber)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                std::vector<uint32_t> hVec              = GetHits(hybrid->getId(), chip->getId());
                unsigned int          i                 = 0;
                auto                  cTestChannelGroup = getChannelGroup(theChannelGroupHandler, groupNumber, opticalGroup->getId(), hybrid->getId(), chip->getId());

                for(ChannelContainer<Occupancy>::iterator channel = chip->begin<Occupancy>(); channel != chip->end<Occupancy>(); channel++, i++)
                {
                    if(cTestChannelGroup->isChannelEnabled(i)) { channel->fOccupancy += hVec[i]; }
                }
            }
        }
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

uint32_t D19SCEventAS::GetNHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex   = getFeIndex(pFeId);
    uint8_t cRocIndex  = getROCIndex(pFeId, pSSAId);
    auto&   cHitVecotr = fCounterData.at(cFeIndex).at(cRocIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
}
std::vector<uint32_t> D19SCEventAS::GetHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex  = getFeIndex(pFeId);
    uint8_t cRocIndex = getROCIndex(pFeId, pSSAId);
    return fCounterData.at(cFeIndex).at(cRocIndex);
}

} // namespace Ph2_HwInterface
