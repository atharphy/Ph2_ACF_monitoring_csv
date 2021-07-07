#include "../Utils/D19cMPAEventAS.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"
#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cMPAEventAS::D19cMPAEventAS(const BeBoard* pBoard, uint32_t pNMPA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNMPA * pNFe)
{
    fNMPA = pNMPA;
    SetEvent(pBoard, pNMPA, list);
}
D19cMPAEventAS::D19cMPAEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
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
            fNSSA += cFe->fullSize();
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cROCIds(0);
            cROCIds.clear();
            for(auto cChip: *cFe)
            {
                if(cChip->getFrontEndType() == FrontEndType::MPA)
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
void D19cMPAEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async MPA " << RESET;
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
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
                    auto& cChipCounterData = cHybridCounterData[cRocIndex];
                    for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                    {
                        auto cWord = *(cDataIterator);
                        cChipCounterData.push_back(cWord);
                        LOG(DEBUG) << BOLDBLUE << "ROC#" << +cRocIndex << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                        cDataIterator++;
                    }
                    cRocIndex++;
                } //[if MPA]
            }     // chips
            cFeIndex++;
        } // hybrids
    }     // opticalGroup
}
// required by event but not sure if makes sense for AS
void D19cMPAEventAS::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
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
                    if(cTestChannelGroup->isChannelEnabled(i)) channel->fOccupancy += hVec[i];
                }
            }
        }
    }
}

void D19cMPAEventAS::SetEvent(const BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list)
{
    std::cout << "MPAASEV" << std::endl;

    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            uint32_t nc = 0;
            for(auto cChip: *cHybrid)
            {
                fEventDataVector[encodeVectorIndex(cHybrid->getId(), cChip->getId(), pNMPA)] = std::vector<uint32_t>(list.begin() + nc * 1920, list.begin() + (nc + 1) * 1920);
                // std::cout<<fEventDataVector[encodeVectorIndex (cHybrid->getId(), cChip->getId(), pNMPA)
                // ][5]<<std::endl;

                nc += 1;
            }
        }
    }
}

uint32_t D19cMPAEventAS::GetNHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex   = getFeIndex(pFeId);
    uint8_t cRocIndex  = getROCIndex(pFeId, pSSAId);
    auto&   cHitVecotr = fCounterData.at(cFeIndex).at(cRocIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId,fNMPA));
    // return std::accumulate(hitVector.begin()+1, hitVector.end(), 0);
}
std::vector<uint32_t> D19cMPAEventAS::GetHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex  = getFeIndex(pFeId);
    uint8_t cRocIndex = getROCIndex(pFeId, pSSAId);
    return fCounterData.at(cFeIndex).at(cRocIndex);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId,fNMPA));
    // LOG (INFO) << BOLDBLUE << hitVector[0] << RESET;
    // return hitVector;
}

} // namespace Ph2_HwInterface
