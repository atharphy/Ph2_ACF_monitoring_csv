#include "../Utils/D19cPSEventAS.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"
#include <numeric>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cPSEventAS::D19cPSEventAS(const BeBoard* pBoard, uint32_t pNMPA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNMPA * pNFe)
{
    fNMPA = pNMPA;
    SetEvent(pBoard, pNMPA, list);
}
D19cPSEventAS::D19cPSEventAS(const BeBoard* pBoard, const std::vector<uint32_t>& list)
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
                RocCounterData cRocData;
                cRocData.clear();
                cHybridCounterData.push_back(cRocData);
                cROCIds.push_back(cChip->getId());
            } // chip
            fCounterData.push_back(cHybridCounterData);
            fROCIds.push_back(cROCIds);
        } // hybrids
    }     // opticalGroup
    // // first check if there are also SSAs here
    // // if there are then data will come SSAs then MPAs
    // // because of the order of the configuration
    // // so reverse the list
    // std::reverse(list.begin(),list.end());
    this->Set(pBoard, list);
}
void D19cPSEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async PS counters " << RESET;
    auto                      cDataIterator = pData.begin();
    auto                      cFeTypes      = pBoard->connectedFrontEndTypes();
    std::vector<FrontEndType> cValidTypes{FrontEndType::MPA, FrontEndType::SSA, FrontEndType::SSA2};
    for(auto cValidType: cValidTypes)
    {
        if(std::find(cFeTypes.begin(), cFeTypes.end(), cValidType) == cFeTypes.end()) continue;

        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                uint8_t cFeIndex = getFeIndex(cFe->getId());
                for(auto cChip: *cFe)
                {
                    if(cChip->getFrontEndType() != cValidType) continue;

                    uint8_t cRocIndex = getROCIndex(cFeIndex, cChip->getId());
                    fCounterData[cFeIndex][cRocIndex].clear();
                    for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl += 2)
                    {
                        // each 32-bit word hold information from two counters
                        for(int cOffset = 0; cOffset < 2; cOffset++)
                        {
                            if( cChnl < 10 ) LOG (DEBUG) << BOLDYELLOW << "Chnl#" << cChnl + cOffset << "\t" << ((*cDataIterator & ( 0x7FFF <<  15*cOffset) ) >> 15*cOffset)  << RESET;
                            fCounterData[cFeIndex][cRocIndex].push_back((*cDataIterator & (0x7FFF << 15 * cOffset)) >> 15 * cOffset);
                        }
                        cDataIterator++;
                    } // channels
                }     // chips
            }         // hybrids
        }             // optical groups
    }                 // valid types.. MPA then SSA
}
// required by event but not sure if makes sense for AS
void D19cPSEventAS::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId)
{
    uint8_t cFeIndex  = getFeIndex(hybridId);
    uint8_t cRocIndex = getROCIndex(cFeIndex, chipContainer->getId());
    LOG(DEBUG) << BOLDYELLOW << "FEIndex " << +cFeIndex << " ROCIndex " << +cRocIndex << " -- " << fCounterData.at(cFeIndex).at(cRocIndex).size() << RESET;
    std::vector<uint32_t> cHits = GetHits(hybridId, chipContainer->getId());
    LOG(DEBUG) << BOLDYELLOW << "FEIndex " << +cFeIndex << " ROCIndex " << +cRocIndex << " -- " << cHits.size() << RESET;
    float  cOcc  = 0;
    size_t cChnl = 0;
    for(auto cHit: cHits)
    {
        if(testChannelGroup == nullptr) break;
        if(testChannelGroup->isChannelEnabled(cChnl))
        {
            if( cChnl < 10 ) LOG (DEBUG) << BOLDYELLOW << "Chnl#" << cChnl << "\t" << cHit  << RESET;
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
    LOG(DEBUG) << BOLDYELLOW << "ROC#" << +chipContainer->getId() << " chip occupancy is " << cOcc / chipContainer->size() << RESET;
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

uint32_t D19cPSEventAS::GetNHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex   = getFeIndex(pFeId);
    uint8_t cRocIndex  = getROCIndex(cFeIndex, pSSAId);
    auto&   cHitVecotr = fCounterData.at(cFeIndex).at(cRocIndex);
    return std::accumulate(cHitVecotr.begin(), cHitVecotr.end(), 0);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId,fNMPA));
    // return std::accumulate(hitVector.begin()+1, hitVector.end(), 0);
}
std::vector<uint32_t> D19cPSEventAS::GetHits(uint8_t pFeId, uint8_t pSSAId) const
{
    uint8_t cFeIndex  = getFeIndex(pFeId);
    uint8_t cRocIndex = getROCIndex(cFeIndex, pSSAId);
    return fCounterData.at(cFeIndex).at(cRocIndex);
    // const std::vector<uint32_t> &hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId,fNMPA));
    // LOG (INFO) << BOLDBLUE << hitVector[0] << RESET;
    // return hitVector;
}

} // namespace Ph2_HwInterface
