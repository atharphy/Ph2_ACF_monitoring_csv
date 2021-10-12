#include "../Utils/D19cSSA2EventAS.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cSSA2EventAS::D19cSSA2EventAS(const BeBoard* pBoard, uint32_t pNSSA2, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA2 * pNFe)
{
    fNSSA2 = pNSSA2;
    SetEvent(pBoard, pNSSA2, list);
}
D19cSSA2EventAS::D19cSSA2EventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fEventDataVector.clear();
    fNSSA2 = 0;
    fFeIds.clear();
    fROCIds.clear();
    fCounterData.clear();
    // assuming that FEIds aren't shared between links
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            fFeIds.push_back(cFe->getId());
            fNSSA2 += cFe->size();
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cROCIds(0);
            cROCIds.clear();
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA2)
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
void D19cSSA2EventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async SSA2 " << RESET;
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
                if(cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    auto& cChipCounterData = cHybridCounterData[cRocIndex];
                    for(uint8_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        if(cChnl % 2 != 0)
                        {
                            auto cWord = *(cDataIterator);
                            cChipCounterData.push_back((cWord & 0xFFFF));
                            cChipCounterData.push_back((cWord & (0xFFFF << 16)) >> 16);
                            LOG(INFO) << BOLDBLUE << "ROC#" << +cRocIndex << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                            cDataIterator++;
                        } //
                    }     // chnl loop
                    cRocIndex++;
                } //[if SSA2]
            }     // chips
            cFeIndex++;
        } // hybrids
    }     // opticalGroup
}

// required by event but not sure if makes sense for AS
void D19cSSA2EventAS::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                std::vector<uint32_t> hVec = GetHits(hybrid->getId(), chip->getId());
                unsigned int          i    = 0;

                for(ChannelContainer<Occupancy>::iterator channel = chip->begin<Occupancy>(); channel != chip->end<Occupancy>(); channel++, i++)
                {
                    if(cTestChannelGroup->isChannelEnabled(i)) { channel->fOccupancy += hVec[i]; }
                }
            }
        }
    }
}

void D19cSSA2EventAS::SetEvent(const BeBoard* pBoard, uint32_t pNSSA2, const std::vector<uint32_t>& list)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            uint32_t nc = 0;
            for(auto cChip: *cHybrid)
            {
                fEventDataVector[encodeVectorIndex(cHybrid->getId(), cChip->getId(), pNSSA2)] = std::vector<uint32_t>(list.begin() + nc * 120, list.begin() + (nc + 1) * 120);
                // std::cout<<fEventDataVector[encodeVectorIndex (cHybrid->getId(), cChip->getId(), pNSSA2)
                // ][5]<<std::endl;

                nc += 1;
            }
        }
    }
}

uint32_t D19cSSA2EventAS::GetNHits(uint8_t pFeId, uint8_t pSSA2Id) const
{
    uint8_t cFeIndex   = getFeIndex(pFeId);
    uint8_t cRocIndex  = getROCIndex(pFeId, pSSA2Id);
    auto&   cHitVecotr = fCounterData.at(cFeIndex).at(cRocIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id,fNSSA2));
    // return std::accumulate(hitVector.begin()+1, hitVector.end(), 0);
}
std::vector<uint32_t> D19cSSA2EventAS::GetHits(uint8_t pFeId, uint8_t pSSA2Id) const
{
    uint8_t cFeIndex  = getFeIndex(pFeId);
    uint8_t cRocIndex = getROCIndex(pFeId, pSSA2Id);
    return fCounterData.at(cFeIndex).at(cRocIndex);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id,fNSSA2));
    // LOG (INFO) << BOLDBLUE << hitVector[0] << RESET;
    // return hitVector;
}

} // namespace Ph2_HwInterface
