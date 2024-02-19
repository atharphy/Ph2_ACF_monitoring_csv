#include "Utils/D19cPSEventAS.h"
#include "HWDescription/Definition.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/DataContainer.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"
#include <algorithm>
#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cPSEventAS::D19cPSEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fEventDataVector.clear();
    fHybridIds.clear();
    fChipIds.clear();
    fCounterData.clear();
    // assuming that FEIds aren't shared between links
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            fHybridIds.push_back(cHybrid->getId());
            HybridCounterData cHybridCounterData;
            cHybridCounterData.clear();
            std::vector<uint8_t> cChipIds(0);
            cChipIds.clear();
            for(auto cChip: *cHybrid)
            {
                ChipCounterData cChipData;
                cChipData.clear();
                cHybridCounterData.push_back(cChipData);
                cChipIds.push_back(cChip->getId());
            } // chip
            fCounterData.push_back(cHybridCounterData);
            fChipIds.push_back(cChipIds);
        } // hybrids
    }     // opticalGroup
    this->Set(pBoard, list);
}
void D19cPSEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async PS counters " << RESET;
    auto cDataIterator  = pData.begin();
    auto cFrontEndTypes = pBoard->connectedFrontEndTypes();
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            uint8_t cHybridIndex = getHybridIndex(cHybrid->getId());
            for(auto cChip: *cHybrid)
            {
                uint8_t cChipIndex = getChipIndex(cHybridIndex, cChip->getId());
                fCounterData[cHybridIndex][cChipIndex].clear();
                for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl += 2)
                {
                    // each 32-bit word hold information from two counters
                    for(int cOffset = 0; cOffset < 2; cOffset++)
                    {
                        if(cChnl < 10) LOG(DEBUG) << BOLDYELLOW << "Chnl#" << cChnl + cOffset << "\t" << ((*cDataIterator & (0x7FFF << 15 * cOffset)) >> 15 * cOffset) << RESET;
                        fCounterData[cHybridIndex][cChipIndex].push_back((*cDataIterator & (0x7FFF << 15 * cOffset)) >> 15 * cOffset);
                    }
                    cDataIterator++;
                } // channels
            }     // chips
        }         // hybrids
    }             // optical groups
}
// required by event but not sure if makes sense for AS
void D19cPSEventAS::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint8_t hybridId)
{
    uint8_t cHybridIndex = getHybridIndex(hybridId);
    uint8_t cChipIndex   = getChipIndex(cHybridIndex, chipContainer->getId());
    LOG(DEBUG) << BOLDYELLOW << "HybridIndex " << +cHybridIndex << " ChipIndex " << +cChipIndex << " -- " << fCounterData.at(cHybridIndex).at(cChipIndex).size() << RESET;
    std::vector<uint32_t> cHits = GetHits(hybridId, chipContainer->getId());
    LOG(DEBUG) << BOLDYELLOW << "HybridIndex " << +cHybridIndex << " ChipIndex " << +cChipIndex << " -- " << cHits.size() << RESET;
    float  cOcc  = 0;
    size_t cChnl = 0;
    for(auto cHit: cHits)
    {
        if(testChannelGroup == nullptr) break;
        if(testChannelGroup->isChannelEnabled(cChnl))
        {
            if(cChnl < 10) LOG(DEBUG) << BOLDYELLOW << "Chnl#" << cChnl << "\t" << cHit << RESET;
            uint32_t cRow = cChnl % testChannelGroup->getNumberOfRows();
            uint32_t cCol;
            if(testChannelGroup->getNumberOfCols() == 0)
                cCol = 0;
            else
                cCol = cChnl / testChannelGroup->getNumberOfRows();

            // if( cChnl < 10 ) LOG (INFO) << BOLDYELLOW << "Chnl#" << cChnl << "\t" << cHit  << "Row " << cRow << " Col" << cCol << RESET;

            chipContainer->getChannel<Occupancy>(cRow, cCol).fOccupancy += cHit;
            cOcc += cHit;
        }
        cChnl++;
    }
    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +chipContainer->getId() << " chip occupancy is " << cOcc / chipContainer->size() << RESET;
}

void D19cPSEventAS::SetEvent(const BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list)
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

uint32_t D19cPSEventAS::GetNHits(uint8_t pHybridId, uint8_t pChipId) const
{
    uint8_t cHybridIndex = getHybridIndex(pHybridId);
    uint8_t cChipIndex   = getChipIndex(cHybridIndex, pChipId);
    auto&   cHitVector   = fCounterData.at(cHybridIndex).at(cChipIndex);
    return std::accumulate(cHitVector.begin(), cHitVector.end(), 0);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pHybridId, pMPAId,fNMPA));
    // return std::accumulate(hitVector.begin()+1, hitVector.end(), 0);
}
std::vector<uint32_t> D19cPSEventAS::GetHits(uint8_t pHybridId, uint8_t pChipId) const
{
    uint8_t cHybridIndex = getHybridIndex(pHybridId);
    uint8_t cChipIndex   = getChipIndex(cHybridIndex, pChipId);
    return fCounterData.at(cHybridIndex).at(cChipIndex);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pHybridId, pMPAId,fNMPA));
    // LOG (INFO) << BOLDBLUE << hitVector[0] << RESET;
    // return hitVector;
}

} // namespace Ph2_HwInterface
