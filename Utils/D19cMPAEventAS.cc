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
                if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;
                
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
void D19cMPAEventAS::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    LOG(INFO) << BOLDBLUE << "Setting event for Async MPA " << RESET;
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
                if( cChip->getFrontEndType() != FrontEndType::MPA ) continue;
                
                std::string cTypePrnt        = "Pxl";
                //int         cDebugCnt        = (cChip->getFrontEndType() == FrontEndType::MPA) ? 250 : 25;
                auto&       cChipCounterData = cHybridCounterData[cRocIndex];
                
                for(uint16_t cChnl = 0; cChnl < cChip->size(); cChnl++)
                {
                    if(cChnl % 2 == 0)
                    {
                        auto cWord    = *(cDataIterator);
                        int  cFrstPxl = cChnl;
                        int  cNxtPxl  = cChnl + 1;
                        cChipCounterData.push_back((cWord & 0xFFFF));
                        cChipCounterData.push_back((cWord & (0xFFFF << 16)) >> 16);
                        //if(cFrstPxl % cDebugCnt == 0)
                        if( cNxtPxl < 30 ) LOG(DEBUG) << BOLDBLUE << "ROC#" << +cRocIndex << " [" << cTypePrnt << "#" << +cFrstPxl << " ," << cTypePrnt << "#" << cNxtPxl << " ]"
                                       << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                        cDataIterator++;
                    } // every 2 channels are packed into one 32 bit word
                }     // chnl loop
                cRocIndex++;
            } // chips
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
                if( chip->size() != 16*120 ) continue;
                
                std::vector<uint32_t> cHits = GetHits(hybrid->getId(), chip->getId());
                float cOcc=0; 
                size_t cChnl=0;
                for(auto cHit: cHits)
                {
                    //uint32_t cRow = cChnl%cTestChannelGroup->getNumberOfRows(); 
                    //uint32_t cCol = (cTestChannelGroup->getNumberOfCols() > 1 ) ? cChnl/cTestChannelGroup->getNumberOfRows() : 1; 
                    if(cTestChannelGroup->isChannelEnabled(cChnl)) { 
                        if( cChnl < 10 ) LOG (INFO) << BOLDBLUE << cChnl << " found " << cHit << RESET;
                        chip->getChannelContainer<Occupancy>()->at(cChnl).fOccupancy += cHit; 
                        cOcc += cHit;
                    }
                    cChnl++;
                }
                LOG (INFO) << BOLDBLUE << "ROC#" << +chip->getId() << " chip occupancy is " << cOcc/chip->size() << RESET;
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
                fEventDataVector[encodeVectorIndex(cHybrid->getId(), cChip->getId()%8, pNMPA)] = std::vector<uint32_t>(list.begin() + nc * 1920, list.begin() + (nc + 1) * 1920);
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
