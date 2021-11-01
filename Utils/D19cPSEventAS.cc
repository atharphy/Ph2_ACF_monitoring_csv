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
    LOG(DEBUG) << BOLDBLUE << "Setting event for Async MPA " << RESET;
    auto    cDataIterator = pData.begin();
    do
    {
        //uint32_t cPSModuleId = (pBoard->getId() << 16) | (cOpticalGroup->getId() << 8 ) | cHybrid->getId(); 
        uint8_t cBoardId   = (*cDataIterator >> 16) & 0xFF ;
        uint8_t cOpticalId = (*cDataIterator >> 8) & 0xFF ; 
        uint8_t cHybridId  = (*cDataIterator >> 0) & 0xFF ; 
        cDataIterator++;
        auto cCicFeId = ( (*cDataIterator) >> 16 ) & 0xFF ;
        uint8_t cIsSSA = (cCicFeId & (0x1 << 3)) >> 3 ; 
        auto cCounterInfo = ( (*cDataIterator) & 0xFFFF );
        if( cBoardId == pBoard->getId() )
        {
            for(auto cOpticalGroup: *pBoard)
            {
                if( cOpticalGroup->getId() != cOpticalId ) continue;
                for(auto cFe: *cOpticalGroup)
                {
                    if( cFe->getId() != cOpticalId ) continue;

                    auto cFeIndx = getFeIndex(cFe->getId());
                    for(auto cChip: *cFe)
                    {
                        auto cMappedId = getChipIdMapped( cFe->getId(), cChip->getId() );
                        if( cMappedId != ( cCicFeId & 0x7 ) ) continue;
                        if( cChip->getId() > 7 &&  cIsSSA == 1 ) continue; 
                        if( cChip->getId() < 8 &&  cIsSSA == 0 ) continue; 
                        auto cChipIndx = getROCIndex(cFeIndx,cChip->getId());
                        fCounterData[cFeIndx][cChipIndx].push_back(cCounterInfo);
                        LOG (DEBUG) << BOLDBLUE <<  "BeBoard" << +cBoardId << " OG" << +cOpticalId << " Hybrid" << +cHybridId 
                            << " CicFE" << +cCicFeId << " HybridFE" << +cChip->getId() << " : "
                            << +cIsSSA << " , "
                            << " : " << cCounterInfo << "[" <<  fCounterData[cFeIndx][cChipIndx].size() << "]" << RESET; 
                    }
                }
            }
        }

        cDataIterator++;
    }while( cDataIterator < pData.end() );
    /*
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
                int         cDebugCnt        = (cChip->getFrontEndType() == FrontEndType::MPA) ? 250 : 25;
                std::string cTypePrnt        = (cChip->getFrontEndType() == FrontEndType::MPA) ? "Pxl" : "Strp";
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
                        if(cFrstPxl % cDebugCnt == 0)
                            LOG(DEBUG) << BOLDBLUE << "ROC#" << +cRocIndex << " [" << cTypePrnt << "#" << +cFrstPxl << " ," << cTypePrnt << "#" << cNxtPxl << " ]"
                                       << " .. hits: " << +(cWord & 0xFFFF) << " , " << +((cWord & (0xFFFF << 16)) >> 16) << RESET;
                        cDataIterator++;
                    } // every 2 channels are packed into one 32 bit word
                }     // chnl loop
                cRocIndex++;
            } // chips
            cFeIndex++;
        } // hybrids
    }// opticalGroup*/
}
// required by event but not sure if makes sense for AS
void D19cPSEventAS::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    if( cTestChannelGroup == nullptr ){ 
        LOG (INFO) << BOLDRED << "!!!!" << RESET;
        return;
    }
    //LOG (INFO) << cTestChannelGroup->getNumberOfRows() << " : " << cTestChannelGroup->getNumberOfCols() << RESET;
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                std::vector<uint32_t> cHits = GetHits(hybrid->getId(), chip->getId());
                float cOcc=0; 
                size_t cChnl=0;
                for(auto cHit: cHits)
                {
                    //uint32_t cRow = cChnl%cTestChannelGroup->getNumberOfRows(); 
                    //uint32_t cCol = (cTestChannelGroup->getNumberOfCols() > 1 ) ? cChnl/cTestChannelGroup->getNumberOfRows() : 1; 
                    if(cTestChannelGroup->isChannelEnabled(cChnl)) { 
                        uint32_t cRow = cChnl%cTestChannelGroup->getNumberOfRows(); 
                        uint32_t cCol;
                        if( cTestChannelGroup->getNumberOfCols() == 0 ) cCol = 0; 
                        else  cCol = cChnl/cTestChannelGroup->getNumberOfRows();
                        chip->getChannel<Occupancy>(cRow, cCol).fOccupancy += cHit;
                        cOcc += cHit;
                    }
                    cChnl++;
                }
                LOG (DEBUG) << BOLDBLUE << "ROC#" << +chip->getId() << " chip occupancy is " << cOcc/chip->size() << RESET;
            }
        }
    }
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
