#include "../Utils/D19cSSAEventAS.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cSSAEventAS::D19cSSAEventAS(const BeBoard* pBoard, uint32_t pNSSA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA * pNFe)
{
    fNSSA = pNSSA;
    SetEvent(pBoard, pNSSA, list);
}
D19cSSAEventAS::D19cSSAEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fEventDataVector.clear();
    fNSSA = 0;
    fFeIds.clear();
    fROCIds.clear();
    fCounterData.clear();
    // assuming that FEIds aren't shared between links
    for(auto cModule: *pBoard)
    {
        for(auto cFe: *cModule)
        {
            fFeIds.push_back(cFe->getId());
            fNSSA += cFe->size();
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cROCIds(0);
            cROCIds.clear();
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA)
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
    }     // modules
    this->Set(pBoard, list);
}
void D19cSSAEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async SSA " << RESET;
    auto    cDataIterator = pData.begin();
    uint8_t cFeIndex      = 0;
    for(auto cModule: *pBoard)
    {
        for(auto cFe: *cModule)
        {
            auto&   cHybridCounterData = fCounterData[cFeIndex];
            uint8_t cRocIndex          = 0;
            // loop over chips
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::SSA)
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
    }     // modules
}

// required by event but not sure if makes sense for AS
void D19cSSAEventAS::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
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

void D19cSSAEventAS::SetEvent(const BeBoard* pBoard, uint32_t pNSSA, const std::vector<uint32_t>& list)
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

uint32_t D19cSSAEventAS::GetNHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex   = getFeIndex(pFeId);
    uint8_t cRocIndex  = getROCIndex(pFeId, pSSAId);
    auto&   cHitVecotr = fCounterData.at(cFeIndex).at(cRocIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId,fNSSA));
    // return std::accumulate(hitVector.begin()+1, hitVector.end(), 0);
}
std::vector<uint32_t> D19cSSAEventAS::GetHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex  = getFeIndex(pFeId);
    uint8_t cRocIndex = getROCIndex(pFeId, pSSAId);
    return fCounterData.at(cFeIndex).at(cRocIndex);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId,fNSSA));
    // LOG (INFO) << BOLDBLUE << hitVector[0] << RESET;
    // return hitVector;
}

} // namespace Ph2_HwInterface
