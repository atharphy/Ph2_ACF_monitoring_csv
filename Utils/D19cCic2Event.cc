/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "Utils/D19cCic2Event.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/DataContainer.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"

using namespace Ph2_HwDescription;

const unsigned N2SHYBRIDS = 12;

namespace Ph2_HwInterface
{
// Event implementation
D19cCic2Event::D19cCic2Event(const BeBoard* pBoard, const std::vector<uint32_t>& list, bool pWith8CBC3, bool pWithTLU)
{
    fIsSparsified = pBoard->getSparsification();
    fEventHitList.clear();
    fEventStubList.clear();
    fEventRawList.clear();
    fHybridIds.clear();
    fChipIds.clear();
    fNCbc       = 0;
    fIs8CBC3    = pWith8CBC3;
    fTLUenabled = (uint8_t)pWithTLU;
    // assuming that HybridIds aren't shared between links
    fIs2S = false;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto  cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
            auto& cCic                = cOuterTrackerHybrid->fCic;
            fNCbc += (cCic == NULL) ? cHybrid->fullSize() : 1;
            HybridData cHybridData;
            fEventStubList.push_back(cHybridData);
            fHybridIds.push_back(cHybrid->getId());
            fHybridIdsCic.push_back(cHybrid->getId());

            std::vector<uint8_t> cChipIds(0);
            cChipIds.clear();
            for(auto cChip: *cHybrid)
            {
                // only count MPAs
                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2) continue;

                cChipIds.push_back(cChip->getId() % 8);
                fIs2S = fIs2S || cChip->getFrontEndType() == FrontEndType::CBC3;
            }
            fChipIds.push_back(cChipIds);
            fNStripClusters.push_back(0);
            fNPxlClusters.push_back(0);
            if(fIsSparsified) { fEventHitList.push_back(cHybridData); }
            else
            {
                RawHybridData cRawHybridData;
                fEventRawList.push_back(cRawHybridData);
            }
        } // hybrids
    }     // opticalGroup
    fBeId        = pBoard->getId();
    fBeFWType    = 0;
    fCBCDataType = 0;
    fBeStatus    = 0;

    fFeMapping = (fIs2S) ? fFeMapping2S : fFeMappingPSR;

    this->Set(pBoard, list);
    // this->SetEvent ( pBoard, fNCbc, list );
}

void D19cCic2Event::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    const uint16_t LENGTH_EVENT_HEADER = 4;
    const uint8_t  VALID_L1_HEADER     = 0x0A;
    const uint8_t  VALID_STUB_HEADER   = 0x05;
    uint32_t       cNEvents            = 0;
    for(auto cWord: pData) LOG(DEBUG) << BOLDYELLOW << std::bitset<32>(cWord) << RESET;
    auto cEventIterator = pData.begin();
    do
    {
        uint32_t cHeader    = (0xFFFF0000 & (*cEventIterator)) >> 16;
        uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
        // retrieve chunck of data vector belonging to this event
        if(cHeader == 0xFFFF)
        {
            // uint32_t cDummyCount = (0xFF & (*(cEventIterator + 1))) * 4;
            // LOG(INFO) << BOLDBLUE << "Event " << +cNEvents << "... event header is " << std::bitset<16>(cHeader)
            //     << " ... " << +cEventSize << " 32 bit words ... " << +cDummyCount
            //     << " dummy 32 bit words .. " << RESET;
            // counters from event header
            // TLU Trigger Id
            fExternalTriggerID = (*(cEventIterator + 1) >> 16) & 0x7FFF;
            // TDC + L1A counter
            uint32_t cEvntCntTag = (*(cEventIterator + 2));
            // LOG (INFO) << BOLDMAGENTA << "Event counter information " << std::bitset<32>(cEvntCntTag) << RESET;
            uint32_t cFc7EvtId     = (cEvntCntTag & (0x00FFFFFF));
            fTDC                   = (cEvntCntTag & (0xFF << 24)) >> 24;
            uint8_t cTDCShiftValue = 1;
            if(fTDC < cTDCShiftValue)
                fTDC += (8 - cTDCShiftValue);
            else
                fTDC -= cTDCShiftValue;
            // LOG (INFO) << BOLDMAGENTA << "\t... TDC is " << cFc7EvtId << " eventId is " << cFc7EvtId << RESET;
            // internal counters
            cEvntCntTag = (*(cEventIterator + 3));
            // LOG (INFO) << BOLDMAGENTA << "Event counter information " << std::bitset<32>(cEvntCntTag) << RESET;
            uint16_t cFc7BxId = (cEvntCntTag & (0xFFFF));
            // LOG (INFO) << BOLDMAGENTA << "\t... BxId is " << cFc7BxId <<  RESET;
            if(fTLUenabled == 0) { fExternalTriggerID = (cEvntCntTag & (0xFFFF << 16)) >> 16; }
            fEventCount = cFc7EvtId; // 0x00FFFFFF & *(cEventIterator + 2);
            fBunch      = cFc7BxId;  // 0xFFFFFFFF & *(cEventIterator + 3);
            // LOG (INFO) << BOLDMAGENTA << "\t... EventCount is " << fEventCount << RESET;

            auto     cIterator  = cEventIterator + LENGTH_EVENT_HEADER;
            uint32_t cStatus    = 0x00000000;
            size_t   cChipIndex = 0;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto   cHybridIndex        = getHybridIndex(cHybrid->getId());
                    auto   cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                    auto&  cCic                = cOuterTrackerHybrid->fCic;
                    size_t cNReadoutChips      = (cCic == NULL) ? cHybrid->fullSize() : 1;
                    for(size_t cIndex = 0; cIndex < cNReadoutChips; cIndex++)
                    {
                        uint8_t  cStatusWord    = 0x00;
                        uint32_t cHitInfoHeader = *(cIterator);
                        uint32_t cGoodHitInfo   = (cHitInfoHeader & (0xF << 28)) >> 28;
                        uint32_t cHitInfoSize   = (cHitInfoHeader & 0xFFF) * 4;
                        size_t   cOffset        = std::distance(pData.begin(), cIterator);
                        cStatusWord             = static_cast<uint8_t>(cGoodHitInfo == VALID_L1_HEADER);
                        LOG(DEBUG) << BOLDBLUE << "\t.. ReadoutChip#" << +cHybrid->getIndex() << "...hit info header " << std::bitset<4>(cGoodHitInfo) << "... " << +cHitInfoSize
                                   << " words in hit packet..."
                                   << "... status word " << std::bitset<2>(cStatusWord) << " Event#" << +fEventCount << RESET;
                        if(cStatusWord == 0x01)
                        {
                            bool                          cWithCIC2 = (cCic->getFrontEndType() == FrontEndType::CIC2);
                            std::pair<uint16_t, uint16_t> cL1Information;
                            cL1Information.first  = (*(cIterator + 2) & 0x7FC000) >> 14;
                            cL1Information.second = (*(cIterator + 2) & 0xFF800000) >> 23;
                            LOG(DEBUG) << BOLDBLUE << "\t\t..L1 counter for this event : " << +cL1Information.first << " . L1 data size is " << +(cHitInfoSize) << " status "
                                       << std::bitset<9>(cL1Information.second) << RESET;
                            int cL1Offset = cOffset + 2 + int(cWithCIC2);
                            // for( size_t cLOff=0; cLOff <cHitInfoSize; cLOff++)
                            // {
                            //     LOG (INFO) << BOLDYELLOW << "\t\t\t.." << std::bitset<32>(  (*(cIterator + cLOff) ) ) << RESET;
                            // }

                            if(fIsSparsified)
                            {
                                // LOG (DEBUG) << BOLDBLUE << "sparsified data" << RESET;
                                size_t cEOffset                   = 3;
                                fEventHitList[cHybridIndex].first = cL1Information;
                                fEventHitList[cHybridIndex].second.clear();
                                uint8_t cNStripClusters = 0;
                                if(fIs2S)
                                {
                                    cNStripClusters               = (*(cIterator + 2) & (0x7F << 0)) >> 0;
                                    fNStripClusters[cHybridIndex] = cNStripClusters;
                                    // clusters/hit data first
                                    std::vector<std::bitset<CLUSTER_WORD_SIZE>> cL1Words(cNStripClusters, 0);
                                    this->splitStream(pData, cL1Words, cOffset + cEOffset,
                                                      cNStripClusters); // split 32 bit words in std::vector of CLUSTER_WORD_SIZE bits
                                    // last bit is 0 for strip clusters
                                    for(auto cL1Word: cL1Words)
                                    {
                                        uint32_t cWord = cL1Word.to_ulong() | (0 << 31);
                                        fEventHitList[cHybridIndex].second.push_back(cWord);
                                    }
                                }
                                else
                                {
                                    // P + S clusters
                                    cNStripClusters       = (*(cIterator + 2) & (0x7F << 7)) >> 7;
                                    uint8_t cNPxlClusters = (*(cIterator + 2) & 0x7F);

                                    fNPxlClusters[cHybridIndex]   = cNPxlClusters;
                                    fNStripClusters[cHybridIndex] = cNStripClusters;
                                    // LOG(INFO) << BOLDCYAN << "\t..Found:" << +cNStripClusters << " s-clusters and " << +cNPxlClusters << " p-clusters in this event " << RESET;

                                    // split stream into s and p clusters
                                    std::vector<std::bitset<S_CLUSTER_WORD_SIZE>> cL1SWords(cNStripClusters, 0);
                                    this->splitStream(pData, cL1SWords, cOffset + cEOffset, cNStripClusters);
                                    for(auto cL1Word: cL1SWords)
                                    {
                                        uint32_t cWord = cL1Word.to_ulong() | (0 << 31);
                                        fEventHitList[cHybridIndex].second.push_back(cWord);
                                        // uint16_t cVal  = static_cast<uint16_t>(cL1Word.to_ulong());
                                        // uint16_t cId   = (uint16_t)((cVal & (0x7 << 11)) >> 11);
                                        // uint16_t cAdd  = (uint16_t)((cVal & (0x7F << 4)) >> 4); //( cL1Word.to_ulong() & (0x7F << 4)) << 4;
                                        // uint16_t cWdth = (uint16_t)((cVal & (0x7 << 1)) >> 1);  //( cL1Word.to_ulong() & (0x7 << 1)) << 1;
                                        // uint16_t cMip  = (uint16_t)((cVal & (0x1 << 0)) >> 0);  //( cL1Word.to_ulong() & (0x1 << 0)) << 0;
                                        // LOG(INFO) << BOLDCYAN << "\t..SCluster [ " << std::bitset<S_CLUSTER_WORD_SIZE>(cL1Word) << "] \t... "
                                        //            << std::bitset<16>((uint16_t)((cVal & (0x7 << 11)) >> 11)) << "\t" << +cId << "\t" << +cAdd << "\t" << +cWdth << "\t" << +cMip << RESET;
                                    } // push back s clusters

                                    cEOffset += (cNStripClusters * S_CLUSTER_WORD_SIZE) / 32;
                                    std::vector<std::bitset<P_CLUSTER_WORD_SIZE>> cL1PWords(cNPxlClusters, 0);
                                    this->splitStream(pData, cL1PWords, cOffset + cEOffset, cNPxlClusters, (cNStripClusters * S_CLUSTER_WORD_SIZE) % 32);
                                    // if(cNPxlClusters > 0)
                                    for(auto cL1Word: cL1PWords)
                                    {
                                        uint32_t cWord = cL1Word.to_ulong() | (1 << 31);
                                        fEventHitList[cHybridIndex].second.push_back(cWord);
                                        // uint32_t cVal   = static_cast<uint32_t>(cL1Word.to_ulong());
                                        // uint32_t cId    = (uint32_t)((cVal & (0x7 << 14)) >> 14);
                                        // uint32_t cAdd   = (uint32_t)((cVal & (0x7F << 7)) >> 7); //( cL1Word.to_ulong() & (0x7F << 4)) << 4;
                                        // uint32_t cWdth  = (uint32_t)((cVal & (0x7 << 4)) >> 4);  //( cL1Word.to_ulong() & (0x7 << 1)) << 1;
                                        // uint32_t cZInfo = (uint32_t)((cVal & (0xF << 0)) >> 0);  //( cL1Word.to_ulong() & (0x1 << 0)) << 0;
                                        // LOG(INFO) << BOLDCYAN << "\t...PCluster:" << std::bitset<P_CLUSTER_WORD_SIZE>(cWord) << "] \t... " << std::bitset<32>((uint32_t)((cVal & (0x7 << 14)) >> 14))
                                        //            << "\t" << +cId << "\t" << +cAdd << "\t" << +cWdth << "\t" << +cZInfo << RESET;
                                    } // push back p clusters
                                    // LOG (INFO) << BOLDCYAN << "For Fe#" << +cHybrid->getIndex() << " [ Id " << +cHybrid->getId() << " ] event hist list has " <<
                                    // +fEventHitList[cHybridIndex].second.size() << " words." << RESET;
                                }
                            }
                            else
                            {
                                // LOG (INFO) << BOLDBLUE << "Un-sparsified data from CIC#" << +cHybrid->getId() << RESET;
                                fEventRawList[cHybridIndex].first = cL1Information;
                                fEventRawList[cHybridIndex].second.clear();
                                size_t cFullSize = 8; // going to assume that I will always readout 8*275 block of data
                                if(cWithCIC2)
                                {
                                    const size_t                            cNblocks = RAW_L1_CBC * cFullSize / L1_BLOCK_SIZE; // 275 bits per chip ... 8chips... blocks of 11 bits
                                    std::vector<std::bitset<L1_BLOCK_SIZE>> cL1Words(cNblocks, 0);
                                    this->splitStream(pData, cL1Words, cL1Offset,
                                                      cNblocks); // split 32 bit words in  blocks of 11 bits
                                    // now try and arrange them by Hybrid again ...

                                    for(size_t cChipIndex = 0; cChipIndex < cFullSize; cChipIndex++)
                                    {
                                        std::bitset<RAW_L1_CBC> cBitset(0);
                                        size_t                  cPosition = 0;
                                        for(size_t cBlockIndex = 0; cBlockIndex < RAW_L1_CBC / L1_BLOCK_SIZE; cBlockIndex++) // RAW_L1_CBC/L1_BLOCK_SIZE blocks per chip
                                        {
                                            auto cIndex = cChipIndex + cFullSize * cBlockIndex;
                                            // auto  cIndex   = cChipIndex + cFullSize * cBlockIndex;
                                            if(cIndex >= cL1Words.size())
                                            {
                                                LOG(INFO) << BOLDRED << "\t... un-sparse decoder ... problem decoding block#" << +cIndex << RESET;
                                                continue;
                                            }
                                            auto& cL1block = cL1Words[cIndex];
                                            // LOG(DEBUG) << BOLDBLUE << "\t\t... L1 block " << +cIndex << " -- " << std::bitset<L1_BLOCK_SIZE>(cL1block) << RESET;
                                            for(size_t cNbit = 0; cNbit < cL1block.size(); cNbit++)
                                            {
                                                cBitset[cBitset.size() - 1 - cPosition] = cL1block[cL1block.size() - 1 - cNbit];
                                                cPosition++;
                                            }
                                        }
                                        // LOG(INFO) << BOLDBLUE << "\t...  chip " << +cChipIndex << "\t -- " << std::bitset<RAW_L1_CBC>(cBitset) << RESET;
                                        fEventRawList[cHybridIndex].second.push_back(cBitset);
                                    }
                                    // for( auto cChip : * cHybrid )
                                    // {
                                    //     if( cChip->getFrontEndType() == FrontEndType::SSA) continue;

                                    //     auto cHits = GetHits(cHybrid->getId(), cChip->getId() );
                                    //     //LOG (INFO) << BOLDGREEN << "\t.. Chip#" << +cChip->getId() << " found " << +cHits.size() << " hits." << RESET;
                                    // }
                                }
                                else
                                {
                                    // for(size_t cOffst = 2; cOffst < 2 + cHitInfoSize; cOffst++)
                                    // { LOG(INFO) << BOLDMAGENTA << "Word#" << (cOffst - 2) << " : " << std::bitset<32>(*(cIterator + cOffst)) << RESET; }
                                    const size_t                     cNblocks = cFullSize; // 274 bits per chip ..
                                    const size_t                     cRawL1   = RAW_L1_CBC - 1;
                                    std::vector<std::bitset<cRawL1>> cL1Words(cNblocks, 0);
                                    this->splitStream(pData, cL1Words, cL1Offset,
                                                      cNblocks); // split 32 bit words in  blocks of 274 bits
                                    for(size_t cIndex = 0; cIndex < cFullSize; cIndex++)
                                    {
                                        LOG(DEBUG) << BOLDBLUE << "\t...  chip " << +cIndex << "\t -- " << cL1Words[cIndex] << RESET;
                                        fEventRawList[cHybridIndex].second.push_back(std::bitset<RAW_L1_CBC>((cL1Words[cIndex]).to_string() + "0"));
                                    }
                                }
                            }
                        }
                        else
                        {
                            LOG(INFO) << BOLDRED << "Incorrect L1 header from the firmware " << RESET;
                            throw std::runtime_error(std::string("Incorrect L1 header found when decoding data ... stopping"));
                        }

                        // stub info
                        std::pair<uint16_t, uint16_t> cStubInformation;
                        uint32_t                      cStubInfoHeader = *(cIterator + cHitInfoSize);
                        uint32_t                      cGoodStubInfo   = (cStubInfoHeader & (0xF << 28)) >> 28;
                        uint32_t                      cStubInfoSize   = (cStubInfoHeader & 0xFFF) * 4;
                        cStatusWord                                   = cStatusWord | (static_cast<uint8_t>(cGoodStubInfo == VALID_STUB_HEADER) << 1);
                        // LOG(INFO) << BOLDBLUE << "\t\t.. ReadoutChip#" << +cHybrid->getIndex() << "...stub info header " << std::bitset<4>(cGoodStubInfo) << "... " << +cStubInfoSize << "
                        // words in stub packet."
                        //            << "... status word " << std::bitset<2>(cStatusWord) << RESET;
                        if(cStatusWord == 0x03)
                        {
                            // LOG(DEBUG) << BOLDGREEN << "\t... ReadoutChip#" << +cIndex << " adding stub data.. " << RESET;
                            uint32_t cStubInfo      = *(cIterator + cHitInfoSize + 1);
                            uint8_t  cNStubs        = (cStubInfo & (0x3F << 16)) >> 16;
                            cStubInformation.first  = (cStubInfo & 0xFFF);
                            cStubInformation.second = (cStubInfo & (0x1FF << 22)) >> 22;

                            // if(cNStubs >= 10)
                            // LOG(DEBUG) << BOLDGREEN << "BxId for this event : " << +cStubInformation.first << " . Stub data size is " << +cStubInfoSize << " status "
                            //            << std::bitset<9>(cStubInformation.second) << " -- number of stubs in packet : " << +cNStubs << RESET;
                            fEventStubList[cHybridIndex].first = cStubInformation;
                            fEventStubList[cHybridIndex].second.clear();
                            if(fIs2S)
                            {
                                std::vector<std::bitset<STUB_WORD_SIZE_2S>> cStubWords(cNStubs, 0);
                                this->splitStream(pData, cStubWords, cOffset + cHitInfoSize + 2,
                                                  cNStubs); // split 32 bit words in std::vector of STUB_WORD_SIZE_2S bits
                                for(auto cStubWord: cStubWords) { fEventStubList[cHybridIndex].second.push_back(cStubWord.to_ulong()); }
                            }
                            else
                            {
                                // LOG (INFO) << BOLDGREEN << "splitting stream of stubs for PS case " << RESET;
                                std::vector<std::bitset<STUB_WORD_SIZE_PS>> cStubWords(cNStubs, 0);
                                this->splitStream(pData, cStubWords, cOffset + cHitInfoSize + 2, cNStubs); // split 32 bit words in std::vector of STUB_WORD_SIZE_2S bits
                                for(auto cStubWord: cStubWords)
                                {
                                    // LOG (INFO) << BOLDBLUE << "Stub word " << std::bitset<STUB_WORD_SIZE_PS>(cStubWord) << RESET;
                                    fEventStubList[cHybridIndex].second.push_back(cStubWord.to_ulong());
                                }
                            }
                        }
                        else
                        {
                            LOG(INFO) << BOLDRED << "Incorrect stub header from the firmware" << RESET;
                            // throw std::runtime_error(std::string("Incorrect Stub header found when decoding data ... stopping"));
                        }
                        cStatus = cStatus | (cStatusWord << (cChipIndex * 2));
                        // increment Chip index
                        cChipIndex++;
                        // increment iterator
                        if(cStatusWord == 0x03) { cIterator += cHitInfoSize + cStubInfoSize; }
                    }
                } // hybrid loop
            }     // hybrid loop
            cEventIterator += cEventSize;
        }
        else
        {
            LOG(INFO) << BOLDRED << "Invalid Header D19cCic2Event" << RESET;
            throw std::runtime_error(std::string("Incorrect Event header found when decoding D19cCic2Event data ... stopping"));
        }
        cNEvents++;
    } while(cEventIterator < pData.end());
}

void D19cCic2Event::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId)
{
    std::vector<uint32_t> cHits = this->GetHits(hybridId, chipContainer->getId());
    float                 cOcc  = 0;
    for(auto cHit: cHits)
    {
        if(testChannelGroup->isChannelEnabled(cHit))
        {
            chipContainer->getChannelContainer<Occupancy>()->at(cHit).fOccupancy += 1.;
            cOcc += cHit;
        }
    }
}

void D19cCic2Event::SetEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list)
{
    // get the first CIC
    auto theFirstCIC = static_cast<const OuterTrackerHybrid*>(pBoard->topoGigio(0)->topoGigio(0))->fCic;
    bool cWithCIC2   = (theFirstCIC->getFrontEndType() == FrontEndType::CIC2);

    fIsSparsified = pBoard->getSparsification();

    fEventHitList.clear();
    fEventStubList.clear();
    for(size_t cHybridIndex = 0; cHybridIndex < N2SHYBRIDS * 2; cHybridIndex++)
    {
        HybridData cHybridData;
        fEventStubList.push_back(cHybridData);
        if(fIsSparsified) { fEventHitList.push_back(cHybridData); }
        else
        {
            RawHybridData cRawHybridData;
            fEventRawList.push_back(cRawHybridData);
        }
    }
    fBeId        = pBoard->getId();
    fBeFWType    = 0;
    fCBCDataType = 0;
    fBeStatus    = 0;
    fNCbc        = pNbCbc;

    uint32_t cEventHeader = list.at(0);
    // block size
    fEventSize = 0x0000FFFF & cEventHeader;
    fEventSize *= 4; // block size is in 128 bit words
    fEventDataSize = fEventSize;
    // LOG (DEBUG) << BOLDGREEN << std::bitset<32>(cEventHeader) << " : " << +fEventSize <<  " 32 bit words." << RESET;
    // LOG (DEBUG) << BOLDBLUE << "Event header is " << std::bitset<32>(cEventHeader) << RESET;
    // LOG (DEBUG) << BOLDBLUE << "Block size is " << +fEventSize << " 32 bit words." << RESET;
    // check header
    if(((list.at(0) >> 16) & 0xFFFF) != 0xFFFF) { LOG(ERROR) << "Event header does not contain 0xFFFF start sequence - please, check the firmware"; }

    if(fEventSize != list.size()) LOG(ERROR) << "Vector size doesnt match the BLOCK_SIZE in Header1";

    // dummy size
    uint8_t fDummySize = (0xFF & list.at(1)) >> 0;
    fDummySize *= 4;

    // counters
    fExternalTriggerID = (list.at(1) >> 16) & 0x7FFF;
    fTDC               = (list.at(2) >> 24) & 0xFF;
    fEventCount        = 0x00FFFFFF & list.at(2);
    fBunch             = 0xFFFFFFFF & list.at(3);
    LOG(DEBUG) << BOLDBLUE << "Event" << +fEventCount << " -- BxId " << +fBunch << RESET;

    auto cIterator = list.begin() + EVENT_HEADER_SIZE;
    LOG(DEBUG) << BOLDBLUE << "Event" << +fEventCount << " has " << +list.size() << " 32 bit words [ of which " << +fDummySize << " words are dummy]" << RESET;
    do
    {
        // L1
        size_t   cOffset   = std::distance(list.begin(), cIterator);
        uint32_t cL1Header = *cIterator;
        uint8_t  cHeader   = (cL1Header & 0xF0000000) >> 28;
        if(cHeader != 0xa)
        {
            LOG(ERROR) << BOLDRED << "Invalid L1 header found in uDTC event." << RESET;
            exit(INVALID_L1HEADER);
        }
        uint8_t cErrorCode = (cL1Header & 0xF000000) >> 24;
        if(cErrorCode != 0)
        {
            LOG(ERROR) << BOLDRED << "Error Code " << +cErrorCode << RESET;
            exit(INVALID);
        }

        uint8_t cHybridId = (cL1Header & 0xFF0000) >> 16;
        LOG(DEBUG) << BOLDBLUE << "\t.. Hybrid Id from firmware " << +cHybridId << " .. putting data in event list .. offset " << +cOffset << RESET;
        std::pair<uint16_t, uint16_t> cL1Information;
        uint32_t                      cL1DataSize = (cL1Header & 0xFFF) * 4;
        if(cWithCIC2)
        {
            cL1Information.first  = (*(cIterator + 2) & 0x7FC000) >> 14;
            cL1Information.second = (*(cIterator + 2) & 0xFF800000) >> 23;
            LOG(DEBUG) << BOLDBLUE << "L1 counter for this event : " << +cL1Information.first << " . L1 data size is " << +(cL1DataSize) << " status " << std::bitset<9>(cL1Information.second)
                       << RESET;
        }
        if(fIsSparsified)
        {
            uint8_t cNClusters = (*(cIterator + 2) & 0x7F);
            // clusters/hit data first
            std::vector<std::bitset<CLUSTER_WORD_SIZE>> cL1Words(cNClusters, 0);
            this->splitStream(list, cL1Words, cOffset + 3, cNClusters); // split 32 bit words in std::vector of CLUSTER_WORD_SIZE bits
            fEventHitList[cHybridId].first = cL1Information;
            fEventHitList[cHybridId].second.clear();
            for(auto cL1Word: cL1Words) { fEventHitList[cHybridId].second.push_back(cL1Word.to_ulong()); }
        }
        else
        {
            fEventRawList[cHybridId].first = cL1Information;
            fEventRawList[cHybridId].second.clear();
            // for( uint32_t cIndex=EVENT_HEADER_SIZE+3; cIndex < cL1DataSize; cIndex++)
            //  LOG (INFO) << BOLDBLUE << std::bitset<32>(*(cIterator+cIndex)) << RESET;

            size_t cOpticalGroupIndex = 0;
            size_t cHybridIndex       = 0;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getId() == cHybridId)
                    {
                        cOpticalGroupIndex = cOpticalGroup->getIndex();
                        cHybridIndex       = cHybrid->getIndex();
                    }
                }
            }

            auto   cReadoutChips = pBoard->topoGigio(cOpticalGroupIndex)->topoGigio(cHybridIndex);
            size_t cL1Offset     = cOffset + 2 + cWithCIC2;
            if(cWithCIC2)
            {
                const size_t                            cNblocks = RAW_L1_CBC * cReadoutChips->fullSize() / L1_BLOCK_SIZE; // 275 bits per chip ... 8chips... blocks of 11 bits
                std::vector<std::bitset<L1_BLOCK_SIZE>> cL1Words(cNblocks, 0);
                this->splitStream(list, cL1Words, cL1Offset, cNblocks); // split 32 bit words in  blocks of 11 bits

                // now try and arrange them by CBC again ...
                for(size_t cChipIndex = 0; cChipIndex < cReadoutChips->fullSize(); cChipIndex++)
                {
                    std::bitset<RAW_L1_CBC> cBitset(0);
                    size_t                  cPosition = 0;
                    for(size_t cBlockIndex = 0; cBlockIndex < RAW_L1_CBC / L1_BLOCK_SIZE; cBlockIndex++) // RAW_L1_CBC/L1_BLOCK_SIZE blocks per chip
                    {
                        auto  cIndex   = cChipIndex + cReadoutChips->fullSize() * cBlockIndex;
                        auto& cL1block = cL1Words[cIndex];
                        LOG(DEBUG) << BOLDBLUE << "\t\t... L1 block " << +cIndex << " -- " << std::bitset<L1_BLOCK_SIZE>(cL1block) << RESET;
                        for(size_t cNbit = 0; cNbit < cL1block.size(); cNbit++)
                        {
                            cBitset[cBitset.size() - 1 - cPosition] = cL1block[cL1block.size() - 1 - cNbit];
                            cPosition++;
                        }
                    }
                    LOG(DEBUG) << BOLDBLUE << "\t...  chip " << +cChipIndex << "\t -- " << std::bitset<RAW_L1_CBC>(cBitset) << RESET;
                    fEventRawList[cHybridId].second.push_back(cBitset);
                }
            }
            else
            {
                // std::vector<uint32_t> cData(list.begin()+cOffset, list.begin()+cOffset+cL1DataSize);
                // for(auto cWord : cData )
                //     LOG (INFO) << BOLDMAGENTA << "L1 data : " << std::bitset<32>(cWord) << RESET;
                const size_t                     cNblocks = cReadoutChips->fullSize(); // 274 bits per chip
                const size_t                     cRawL1   = RAW_L1_CBC - 1;
                std::vector<std::bitset<cRawL1>> cL1Words(cNblocks, 0);
                this->splitStream(list, cL1Words, cL1Offset, cNblocks); // split 32 bit words in  blocks of 274 bits
                for(int cIndex = 0; cIndex < (int)(cReadoutChips->fullSize()); cIndex++)
                {
                    LOG(DEBUG) << BOLDMAGENTA << "L1 data : " << cL1Words[cIndex] << RESET;
                    fEventRawList[cHybridId].second.push_back(std::bitset<RAW_L1_CBC>((cL1Words[cIndex]).to_string() + "0"));
                }
            }
        }
        // then stubs
        std::pair<uint16_t, uint16_t> cStubInformation;
        uint32_t                      cStubHeader = *(cIterator + cL1DataSize);
        cHeader                                   = (cStubHeader & 0xF0000000) >> 28;
        if(cHeader != 0x5)
        {
            LOG(ERROR) << BOLDRED << "Invalid stub header found in uDTC event." << RESET;
            exit(INVALID_STUBHEADER);
        }
        uint32_t cStubDataSize  = (cStubHeader & 0xFFF) * 4;
        uint8_t  cNStubs        = (*(cIterator + cL1DataSize + 1) & (0x3F << 16)) >> 16;
        cStubInformation.first  = (*(cIterator + cL1DataSize + 1) & 0xFFF);
        cStubInformation.second = (*(cIterator + cL1DataSize + 1) & (0x1FF << 22)) >> 22;
        // LOG (DEBUG) << BOLDBLUE << "BxId for this event : " << +cStubInformation.first << " . Stub data size is " <<
        // +cStubDataSize << " status " << std::bitset<9>(cStubInformation.second) << " -- number of stubs in packet : "
        // << +cNStubs << RESET;
        std::vector<std::bitset<STUB_WORD_SIZE_2S>> cStubWords(cNStubs, 0);
        this->splitStream(list, cStubWords, cOffset + cL1DataSize + 2,
                          cNStubs); // split 32 bit words in std::vector of STUB_WORD_SIZE_2S bits
        fEventStubList[cHybridId].first = cStubInformation;
        fEventStubList[cHybridId].second.clear();
        for(auto cStubWord: cStubWords) { fEventStubList[cHybridId].second.push_back(cStubWord.to_ulong()); }
        cIterator += cL1DataSize + cStubDataSize;
    } while(cIterator < list.end() - fDummySize);
}

uint8_t D19cCic2Event::GetNStripClusters(uint8_t pHybridId) const
{
    auto cHybridIndex = getHybridIndex(pHybridId);
    if(cHybridIndex >= fNStripClusters.size()) { LOG(INFO) << BOLDRED << " D19cCic2Event::GetNStripClusters out of range..." << RESET; }
    return fNStripClusters[cHybridIndex];
}
uint8_t D19cCic2Event::GetNPixelClusters(uint8_t pHybridId) const
{
    auto cHybridIndex = getHybridIndex(pHybridId);
    if(cHybridIndex >= fNStripClusters.size()) { LOG(INFO) << BOLDRED << " D19cCic2Event::GetNPixelClusters out of range..." << RESET; }
    return fNPxlClusters[cHybridIndex];
}
std::vector<PCluster> D19cCic2Event::GetPixelClusters(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    std::vector<PCluster> cPClusters;
    auto                  cHybridIndex = getHybridIndex(pHybridId);
    if(cHybridIndex >= fEventHitList.size())
    {
        LOG(INFO) << BOLDRED << " D19cCic2Event::GetPixelClusters out of range..." << RESET;
        return cPClusters;
    }
    auto cClusterWords = fEventHitList[cHybridIndex].second;
    // decltype(fEventHitList[0].second) cClusterWords;
    // try
    // {
    //     cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    // }
    // catch(const std::exception& e)
    // {
    //     std::cerr << e.what() << '\n';
    //     return cPClusters;
    // }
    if(cClusterWords.size() == 0) return cPClusters;

    // LOG (INFO) << BOLDGREEN << " D19cCic2Event::GetPixelClusters for Fe" << +pHybridId << "..." << RESET;
    auto cIterator = cClusterWords.begin() + GetNStripClusters(pHybridId);
    auto cEnd      = cClusterWords.end();
    while(cIterator < cEnd)
    {
        uint32_t cVal   = static_cast<uint32_t>((*cIterator));
        uint32_t cId    = (uint32_t)((cVal & (0x7 << 14)) >> 14);
        uint32_t cAdd   = (uint32_t)((cVal & (0x7F << 7)) >> 7); //( cL1Word.to_ulong() & (0x7F << 4)) << 4;
        uint32_t cWdth  = (uint32_t)((cVal & (0x7 << 4)) >> 4);  //( cL1Word.to_ulong() & (0x7 << 1)) << 1;
        uint32_t cZInfo = (uint32_t)((cVal & (0xF << 0)) >> 0);  //( cL1Word.to_ulong() & (0x1 << 0)) << 0;

        uint8_t cChipId       = cId; //((*cIterator) & ((0x7) << (0 + 4 + 3 + 7))) >> (0 + 4 + 3 + 7);
        auto    cChipIdMapped = this->getChipIdMapped(pHybridId, pReadoutChipId);
        // LOG(INFO) << BOLDBLUE << "Retreiving pixel information for Hybrid#" << +pHybridId << " Chip#" << +cChipId << " this is chip Id #" << +cChipIdMapped << " in CIC land" << RESET;
        if(cChipId == cChipIdMapped)
        {
            PCluster aPCluster;

            // LOG (INFO) << BOLDGREEN << "PCLUS ..... " << std::bitset<16>(*cIterator)  << RESET;
            aPCluster.fAddress = cAdd;   //((*cIterator) & ((0x7F) << (0 + 4 + 3))) >> (0 + 4 + 3);
            aPCluster.fWidth   = cWdth;  //((*cIterator) & ((0x7) << (0 + 4))) >> (0 + 4);
            aPCluster.fZpos    = cZInfo; //((*cIterator) & ((0xF) << 0)) >> 0;
            cPClusters.push_back(aPCluster);
            // LOG(INFO) << BOLDGREEN << "P-cluster in chip " << +pReadoutChipId << ", address : " << unsigned(aPCluster.fAddress) << "," << unsigned(aPCluster.fWidth) << ","
            //            << unsigned(aPCluster.fZpos) << RESET;
        }
        cIterator++;
    }

    return cPClusters;

    // uint8_t NSclus = GetNStripClusters(pHybridId, pReadoutChipId);
    // uint8_t NPclus = GetNPixelClusters(pHybridId, pReadoutChipId);
    // auto&  cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;

    // for(int ic=NSclus; ic <(NSclus+NPclus) ; ic++)
    // {
    // //LOG(INFO) << BOLDRED << "firstword " <<firstword<<" nword "<<nword<< RESET;

    // aPCluster.fAddress = (0x00003f80 & cClusterWords[ic]) >> 7;
    // aPCluster.fWidth   = (0x00000070 & cClusterWords[ic]) >> 4;
    // aPCluster.fZpos    =  0x0000000F & cClusterWords[ic];
    // //LOG(INFO) << BOLDRED << "PIX" << RESET;
    // //LOG(INFO) << BOLDRED << std::bitset<17>(cClusterWord) << RESET;

    // //LOG(INFO) << BOLDRED << unsigned(aPCluster.fAddress)<<","<<unsigned(aPCluster.fWidth)<<","<< unsigned(aPCluster.fZpos)<< RESET;
    // result.push_back(aPCluster);

    // }

    // return result;
}
std::vector<SCluster> D19cCic2Event::GetStripClusters(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    std::vector<SCluster> cSClusters;
    auto                  cHybridIndex = getHybridIndex(pHybridId);
    if(cHybridIndex >= fEventHitList.size())
    {
        LOG(INFO) << BOLDRED << " D19cCic2Event::GetPixelClusters out of range..." << RESET;
        return cSClusters;
    }
    auto cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    if(cClusterWords.size() == 0) return cSClusters;

    // why?
    // decltype(fEventHitList[0].second) cClusterWords;
    // try
    // {
    //     cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    // }
    // catch(const std::exception& e)
    // {
    //     std::cerr << e.what() << '\n';
    //     return cSClusters;
    // }
    auto cIterator = cClusterWords.begin();
    auto cEnd      = cClusterWords.begin() + GetNStripClusters(pHybridId);
    while(cIterator < cEnd)
    {
        uint32_t cVal  = static_cast<uint32_t>((*cIterator));
        uint16_t cId   = (uint16_t)((cVal & (0x7 << 11)) >> 11);
        uint16_t cAdd  = (uint16_t)((cVal & (0x7F << 4)) >> 4); //( cL1Word.to_ulong() & (0x7F << 4)) << 4;
        uint16_t cWdth = (uint16_t)((cVal & (0x7 << 1)) >> 1);  //( cL1Word.to_ulong() & (0x7 << 1)) << 1;
        uint16_t cMip  = (uint16_t)((cVal & (0x1 << 0)) >> 0);  //( cL1Word.to_ulong() & (0x1 << 0)) << 0;

        uint8_t cChipId = cId; //((*cIterator) & ((0x7) << (0 + 1 + 3 + 7))) >> (0 + 1 + 3 + 7);

        auto cChipIdMapped = this->getChipIdMapped(pHybridId, pReadoutChipId);
        // LOG(INFO) << BOLDBLUE << "Retreiving strip cluster information for Hybrid#" << +pHybridId << " Chip#" << +cChipId << " this is chip Id #" << +cChipIdMapped << " in CIC land" << RESET;
        if(cChipId == cChipIdMapped)
        {
            // LOG (INFO) << BOLDGREEN << "SCLUS ..... " << std::bitset<14>(*cIterator)  << RESET;
            SCluster cSCluster;
            cSCluster.fAddress = cAdd;  //((*cIterator) & ((0x7F) << (0 + 1 + 3))) >> (0 + 1 + 3);
            cSCluster.fWidth   = cWdth; //((*cIterator) & ((0x7) << (0 + 1))) >> (0 + 1);
            cSCluster.fMip     = cMip;  //((*cIterator) & ((0x1) << 0)) >> 0;
            cSClusters.push_back(cSCluster);
            // LOG(INFO) << BOLDYELLOW << "S-cluster in chip " << +pReadoutChipId << ", address : " << unsigned(cSCluster.fAddress) << "," << unsigned(cSCluster.fWidth) << ","
            //            << unsigned(cSCluster.fMip) << RESET;
        }
        cIterator++;
    };
    return cSClusters;
    // std::vector<SCluster> result;
    // uint8_t NSclus = GetNStripClusters(pHybridId, pReadoutChipId);
    // SCluster aSCluster;
    // auto&  cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    // //uint8_t cSClusterSize = D19C_SCluster_SIZE_32_MPA;
    // //uint8_t deltaword     = 3*32;

    // for(int ic=0; ic <NSclus ; ic++)
    // {
    // //decoding might now be right here...

    //     aSCluster.fAddress = ((0x000007f0 & cClusterWords[ic]) >> 4);
    //     aSCluster.fMip     = (0x0000000e & cClusterWords[ic]) >> 1;
    //     aSCluster.fWidth   = 0x00000001 & cClusterWords[ic];

    //     //LOG(INFO) << BOLDRED << "STRIPS" << RESET;
    //     //LOG (INFO) << BOLDRED << std::bitset<14>(cClusterWords[ic]) << RESET;
    //     //LOG(INFO) << BOLDRED << unsigned(aSCluster.fAddress)<<","<<unsigned(aSCluster.fWidth)<<","<< unsigned(aSCluster.fMip)<< RESET;

    //     result.push_back(aSCluster);

    // }
    // return result;
}

std::bitset<RAW_L1_CBC> D19cCic2Event::getRawL1Word(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    auto   cChipIdMapped = this->getChipIdMapped(pHybridId, pReadoutChipId);
    size_t cIndx         = 7 - cChipIdMapped;
    size_t cHybridIndex  = getHybridIndex(pHybridId);
    // LOG (INFO) << BOLDYELLOW << "D19cCic2Event::getRawL1Word for CIC" << +pHybridId << " CBC" << +pReadoutChipId
    //     << " --- [cHybridIndex,cIndx] == " << +cHybridIndex << "," << +cIndx << " -- raw list has " << fEventRawList.size() << " entries and "
    //     << " --- list of bit-sets has " << +fEventRawList[cHybridIndex].second.size() << " "
    //     << RESET;
    auto& cDataBitset = fEventRawList[cHybridIndex].second[cIndx];
    return cDataBitset;
}

std::string D19cCic2Event::HexString() const
{
    // std::stringbuf tmp;
    // std::ostream os ( &tmp );

    // os << std::hex;

    // for ( uint32_t i = 0; i < fEventSize; i++ )
    // os << std::uppercase << std::setw ( 2 ) << std::setfill ( '0' ) << ( fEventData.at (i) & 0xFF000000 ) << " " << (
    // fEventData.at (i) & 0x00FF0000 ) << " " << ( fEventData.at (i) & 0x0000FF00 ) << " " << ( fEventData.at (i) &
    // 0x000000FF );

    ////os << std::endl;

    // return tmp.str();
    return "";
}

std::string D19cCic2Event::DataHexString(uint8_t pHybridId, uint8_t pCbcId) const
{
    pCbcId = pCbcId % 8;
    std::stringbuf tmp;
    std::ostream   os(&tmp);
    std::ios       oldState(nullptr);
    oldState.copyfmt(os);
    os << std::hex << std::setfill('0');

    // get the CBC event for pHybridId and pCbcId into vector<32bit> cbcData
    std::vector<uint32_t> cbcData;
    GetCbcEvent(pHybridId, pCbcId, cbcData);

    // l1cnt
    os << std::setw(3) << ((cbcData.at(2) & 0x01FF0000) >> 16) << std::endl;
    // pipeaddr
    os << std::setw(3) << ((cbcData.at(2) & 0x000001FF) >> 0) << std::endl;
    // trigdata
    os << std::endl;
    os << std::setw(8) << cbcData.at(3) << std::endl;
    os << std::setw(8) << cbcData.at(4) << std::endl;
    os << std::setw(8) << cbcData.at(5) << std::endl;
    os << std::setw(8) << cbcData.at(6) << std::endl;
    os << std::setw(8) << cbcData.at(7) << std::endl;
    os << std::setw(8) << cbcData.at(8) << std::endl;
    os << std::setw(8) << cbcData.at(9) << std::endl;
    os << std::setw(8) << ((cbcData.at(10) & 0xFFFFFFFC) >> 2) << std::endl;
    // stubdata
    os << std::setw(8) << cbcData.at(13) << std::endl;
    os << std::setw(8) << cbcData.at(14) << std::endl;

    os.copyfmt(oldState);

    return tmp.str();
}

// NOT READY (what is i??????????)
bool     D19cCic2Event::Error(uint8_t pHybridId, uint8_t pCbcId, uint32_t i) const { return Bit(pHybridId, pCbcId, D19C_OFFSET_ERROR_CBC3); }
uint16_t D19cCic2Event::L1Status(uint8_t pHybridId) const
{
    // now only 1 bit per chip - OR of a few error flags
    if(fIsSparsified)
    {
        auto& cHitInformation = fEventHitList[getHybridIndex(pHybridId)].first;
        return cHitInformation.second;
    }
    else
        return 0xFFFF;
}
uint32_t D19cCic2Event::Error(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    // now only 1 bit per chip - OR of a few error flags
    if(fIsSparsified)
    {
        auto&   cHitInformation = fEventHitList[getHybridIndex(pHybridId)].first;
        uint8_t cChipIdMapped   = 0;
        if(pReadoutChipId < 8) cChipIdMapped = 1 + this->getChipIdMapped(pHybridId, pReadoutChipId);
        // auto     cChipIdMapped   = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
        uint32_t cError = (cHitInformation.second & (0x1 << (cChipIdMapped))) >> (cChipIdMapped);

        return cError;
    }
    else
    {
        auto           cDataBitset = getRawL1Word(pHybridId, pReadoutChipId);
        std::bitset<2> cErrorBits(0);
        size_t         cOffset = 0;
        for(size_t cIndex = 0; cIndex < cErrorBits.size(); cIndex++) cErrorBits[cErrorBits.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return (uint32_t)(cErrorBits.to_ulong());
    }
}
uint32_t D19cCic2Event::BxId(uint8_t pHybridId) const
{
    auto& cStubInformation = fEventStubList[getHybridIndex(pHybridId)].first;
    return cStubInformation.first;
}
uint16_t D19cCic2Event::Status(uint8_t pHybridId) const
{
    auto& cStubInformation = fEventStubList[getHybridIndex(pHybridId)].first;
    return cStubInformation.second;
}
uint32_t D19cCic2Event::L1Id(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified)
    {
        auto& cHitInformation = fEventHitList[getHybridIndex(pHybridId)].first;
        return cHitInformation.first;
    }
    else
    {
        auto cDataBitset = getRawL1Word(pHybridId, pReadoutChipId);
        LOG(DEBUG) << BOLDBLUE << "Raw L1 Word is " << cDataBitset << RESET;
        std::bitset<9> cL1Id(0);
        size_t         cOffset = 9 + 2;
        for(size_t cIndex = 0; cIndex < cL1Id.size(); cIndex++) cL1Id[cL1Id.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return cL1Id.to_ulong();
    }
}
// does not apply for sparsified event
uint32_t D19cCic2Event::PipelineAddress(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified) { return 666; }
    else
    {
        auto           cDataBitset = getRawL1Word(pHybridId, pReadoutChipId);
        std::bitset<9> cPipeline(0);
        size_t         cOffset = 2;
        for(size_t cIndex = 0; cIndex < cPipeline.size(); cIndex++) cPipeline[cPipeline.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return cPipeline.to_ulong();
    }
}
std::bitset<NMPACHANNELS> D19cCic2Event::decodePClusters(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    auto&                     cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    std::bitset<NMPACHANNELS> cBitSet(0);
    size_t                    cClusterId = 0;
    for(auto cCluster: cClusterWords)
    {
        uint8_t cChipId       = (cCluster & ((0x7) << (0 + 4 + 3 + 7))) >> (0 + 4 + 3 + 7);
        auto    cChipIdMapped = this->getChipIdMapped(pHybridId, pReadoutChipId);
        if(cChipId == cChipIdMapped)
        {
            // figure out if it is an S or a P cluster
            uint8_t cFlag = (cCluster & (0x1 << 31)) >> 31;
            if(cFlag == 0) // s cluster
            {
                SCluster cSCluster;
                cSCluster.fAddress = (cCluster & ((0x7F) << (0 + 1 + 3))) >> (0 + 1 + 3);
                cSCluster.fWidth   = (cCluster & ((0x7) << (0 + 1))) >> (0 + 1);
                cSCluster.fMip     = (cCluster & ((0x1) << 0)) >> 0;
                // cSClusters.push_back(cSCluster);
                LOG(DEBUG) << BOLDRED << "S-cluster, address : " << unsigned(cSCluster.fAddress) << "," << unsigned(cSCluster.fWidth) << "," << unsigned(cSCluster.fMip) << RESET;
            }
            else
            {
                PCluster aPCluster;
                aPCluster.fAddress = (cCluster & ((0x7F) << (0 + 4 + 3))) >> (0 + 4 + 3);
                aPCluster.fWidth   = (cCluster & ((0x7) << (0 + 4))) >> (0 + 4);
                aPCluster.fZpos    = (cCluster & ((0xF) << 0)) >> 0;
                // cPClusters.push_back(aPCluster);
                LOG(DEBUG) << BOLDGREEN << "P-cluster, address : " << unsigned(aPCluster.fAddress) << "," << unsigned(aPCluster.fWidth) << "," << unsigned(aPCluster.fZpos) << RESET;
            }
            cClusterId++;
        }
    }
    return cBitSet;
}
std::bitset<NCHANNELS> D19cCic2Event::decodeClusters(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    auto&                  cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    size_t                 cClusterId = 0;
    // LOG (INFO) << BOLDBLUE << "Decoding clusters for Hybrid" << +pHybridId << " readout chip " << +pReadoutChipId << RESET;
    if(fIs2S)
    {
        for(auto cClusterWord: cClusterWords)
        {
            uint8_t cChipId       = (cClusterWord & (0x7 << 11)) >> 11;
            auto    cChipIdMapped = this->getChipIdMapped(pHybridId, cChipId);
            if(cChipIdMapped != pReadoutChipId) continue;

            uint8_t cLayerId      = ((cClusterWord & (0xFF << 3)) >> 3) & 0x01;        // LSB is the layer
            uint8_t cStrip        = (((cClusterWord & (0xFF << 3)) >> 3) & 0xFE) >> 1; // strip id
            uint8_t cWidth        = 1 + (cClusterWord & 0x7);
            uint8_t cFirstChannel = 2 * cStrip + cLayerId;

            LOG(INFO) << BOLDBLUE << "Cluster " << +cClusterId << " : " << std::bitset<CLUSTER_WORD_SIZE>(cClusterWord) << "... " << +cWidth << " strip cluster in strip " << +cStrip << " in layer "
                      << +cLayerId << " so first hit is in channel " << +cFirstChannel << " of chip " << +cChipId << " [ real hybrid  " << +cChipIdMapped << " ]" << RESET;

            for(size_t cOffset = 0; cOffset < cWidth; cOffset++)
            {
                LOG(INFO) << BOLDBLUE << "\t\t\t\t.. hit in channel " << +(cFirstChannel + 2 * cOffset) << RESET;
                cBitSet[cFirstChannel + 2 * cOffset] = 1;
            }
            cClusterId++;
        }
    }
    // LOG(INFO) << BOLDBLUE << "Decoded clusters for Hybrid" << +pHybridId << " readout chip " << +pReadoutChipId << " : " << std::bitset<NCHANNELS>(cBitSet) << RESET;
    return cBitSet;
}
bool D19cCic2Event::DataBit(uint8_t pHybridId, uint8_t pReadoutChipId, uint32_t i) const
{
    if(fIsSparsified) { return (decodeClusters(pHybridId, pReadoutChipId)[i] > 0); }
    else
    {
        size_t cOffset = 2 + 9 + 9;
        return (getRawL1Word(pHybridId, pReadoutChipId)[cOffset + i] > 0);
    }
}

std::string D19cCic2Event::DataBitString(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified)
    {
        std::string cBitStream = std::bitset<NCHANNELS>(this->decodeClusters(pHybridId, pReadoutChipId)).to_string();
        // LOG (DEBUG) << BOLDBLUE << "Original bit stream was : " << cBitStream << RESET;
        return cBitStream;
    }
    else
    {
        size_t      cOffset    = 2 + 9 + 9;
        std::string cBitStream = std::bitset<RAW_L1_CBC>(getRawL1Word(pHybridId, pReadoutChipId)).to_string();
        return cBitStream.substr(cOffset, RAW_L1_CBC);
    }
}

std::vector<bool> D19cCic2Event::DataBitVector(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    std::vector<bool> blist;
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pHybridId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { blist.push_back(cDataBitset[cPos] == 1); }
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = getRawL1Word(pHybridId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { blist.push_back(cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1); }
    }
    return blist;
}

std::vector<bool> D19cCic2Event::DataBitVector(uint8_t pHybridId, uint8_t pReadoutChipId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> blist;
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pHybridId, pReadoutChipId);
        for(auto cChannel: channelList) { blist.push_back(cDataBitset[cChannel] == 1); }
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = getRawL1Word(pHybridId, pReadoutChipId);
        for(auto cChannel: channelList) { blist.push_back(cDataBitset[cDataBitset.size() - cOffset - 1 - cChannel] == 1); }
    }
    return blist;
}

std::string D19cCic2Event::GlibFlagString(uint8_t pHybridId, uint8_t pCbcId) const { return ""; }

std::vector<Stub> D19cCic2Event::StubVector(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    auto&             cStubWords = fEventStubList[getHybridIndex(pHybridId)].second;
    std::vector<Stub> cStubVec;
    for(auto cStubWord: cStubWords)
    {
        uint8_t cIdOffset      = (fIs2S) ? (8 + 4) : (8 + 4 + 3);
        uint8_t cAddressOffset = (fIs2S) ? (4) : (4 + 3);
        uint8_t cBendOffset    = (fIs2S) ? 0 : 4;
        uint8_t cBendMask      = (fIs2S) ? 0xF : 0x7;

        // 3 bit chip id
        auto    cChipIdMapped = this->getChipIdMapped(pHybridId, pReadoutChipId);
        uint8_t cChipId       = static_cast<uint8_t>((cStubWord & (0x7 << (cIdOffset))) >> cIdOffset);
        uint8_t cStubAddress  = static_cast<uint8_t>((cStubWord & (0xFF << (cAddressOffset))) >> cAddressOffset);
        uint8_t cStubBend     = static_cast<uint8_t>((cStubWord & (cBendMask << (cBendOffset))) >> cBendOffset);
        uint8_t cRow          = fIs2S ? 0x00 : static_cast<uint8_t>((cStubWord & 0xF));

        LOG(DEBUG) << BOLDGREEN << "Stub package ..... " << std::bitset<18>(cStubWord) << " --  chip id from package " << +cChipId << " [ chip id on hybrid " << +pReadoutChipId << "]"
                   << " stub address is " << +cStubAddress << " stub bend is " << +cStubBend << " stub row is " << +cRow << RESET;
        if(cChipId == cChipIdMapped) { cStubVec.emplace_back(cStubAddress, cStubBend, cRow); }
        // CI
    }
    return cStubVec;
}

std::string D19cCic2Event::StubBitString(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::ostringstream os;

    std::vector<Stub> cStubVector = this->StubVector(pHybridId, pCbcId);

    for(auto cStub: cStubVector) os << std::bitset<8>(cStub.getPosition()) << " " << std::bitset<4>(cStub.getBend()) << " ";

    return os.str();
    // return BitString ( pHybridId, pCbcId, OFFSET_CBCSTUBDATA, WIDTH_CBCSTUBDATA );
}

bool D19cCic2Event::StubBit(uint8_t pHybridId, uint8_t pCbcId) const
{
    // here just OR the stub positions
    std::vector<Stub> cStubVector = this->StubVector(pHybridId, pCbcId);
    return (cStubVector.size() > 0);
}

uint32_t D19cCic2Event::GetNHits(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    uint32_t cNHits = 0;
    if(fIsSparsified)
    {
        // only 2S for now
        auto cDataBitset = this->decodeClusters(pHybridId, pReadoutChipId);
        cNHits           = cDataBitset.count();
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = this->getRawL1Word(pHybridId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { cNHits += (cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1); }
    }
    return cNHits;
}

std::vector<uint32_t> D19cCic2Event::GetHits(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    std::vector<uint32_t> cHits(0);
    if(fIsSparsified)
    {
        if(fIs2S)
        {
            for(auto cCluster: getClusters(pHybridId, pReadoutChipId))
            {
                uint8_t cFirstChannel = 2 * (cCluster.fFirstStrip - 127 * pReadoutChipId) + cCluster.fSensor;
                LOG(DEBUG) << BOLDMAGENTA << "Hybid#" << +pHybridId << " Chip#" << +pReadoutChipId << " Cluster in sensor " << +cCluster.fSensor << " in strip# " << +cCluster.fFirstStrip
                           << " actual strip " << +(cCluster.fFirstStrip - 127 * pReadoutChipId) << " of width " << +cCluster.fClusterWidth << RESET;
                for(int cId = 0; cId < cCluster.fClusterWidth; cId++)
                {
                    LOG(DEBUG) << BOLDMAGENTA << "\t\t.. hit in channel " << +cFirstChannel + 2 * cId << RESET;
                    cHits.push_back(cFirstChannel + cId * 2);
                }
            }
            // auto cDataBitset = this->decodeClusters(pHybridId, pReadoutChipId);
            // for(uint32_t i = 0; i < NCHANNELS; ++i)
            // {
            //     if(cDataBitset[i] > 0) { cHits.push_back(i); }
            // }
        }
        else
        {
            for(auto cCluster: GetPixelClusters(pHybridId, pReadoutChipId))
            {
                for(int cId = 0; cId <= cCluster.fWidth; cId++)
                {
                    uint32_t cHit = ((cCluster.fZpos + 1) << 24) | (cCluster.fAddress - 1) << 8 | cId << 0;
                    if(cCluster.fWidth > 0)
                        LOG(DEBUG) << BOLDBLUE << "Pixel cluster " << +cCluster.fZpos << " [z-pos]; " << +cCluster.fAddress << " [address] " << +cId << " [in cluster]"
                                   << " hit is " << +cHit << RESET;
                    cHits.push_back(cHit);
                }
            }
            for(auto cCluster: GetStripClusters(pHybridId, pReadoutChipId))
            {
                for(int cId = 0; cId <= cCluster.fWidth; cId++)
                {
                    uint32_t cHit = 0 << 24 | (cCluster.fAddress - 1) << 8 | cId << 0;
                    if(cCluster.fWidth > 0)
                        LOG(DEBUG) << BOLDGREEN << "Strip cluster " << +cCluster.fAddress << " [address] " << +cId << " [in cluster]"
                                   << " hit is " << +cHit << RESET;
                    cHits.push_back(cHit);
                }
            }
        }
    }
    else
    {
        if(fIs2S)
        {
            size_t cOffset     = 2 + 9 + 9;
            auto   cDataBitset = this->getRawL1Word(pHybridId, pReadoutChipId);
            for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++)
            {
                if(cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1)
                {
                    LOG(DEBUG) << BOLDYELLOW << " Hit in channel " << +cPos << RESET;
                    cHits.push_back(cPos);
                }
            }
        }
        else
        {
            // To-DO add here for raw PS data
        }
    }
    return cHits;
}

void D19cCic2Event::printL1Header(std::ostream& os, uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    os << GREEN << "HybridId = " << +pHybridId << " CBCId = " << +pReadoutChipId << RESET << std::endl;
    os << YELLOW << "L1 Id: " << this->L1Id(pHybridId, pReadoutChipId) << RESET << std::endl;
    os << YELLOW << "Bx Id: " << this->BxId(pHybridId) << RESET << std::endl;
    os << RED << "Error: " << static_cast<std::bitset<1>>(this->Error(pHybridId, pReadoutChipId)) << RESET << std::endl;
    os << CYAN << "Total number of hits: " << this->GetNHits(pHybridId, pReadoutChipId) << RESET << std::endl;
}

void D19cCic2Event::print(std::ostream& os) const
{
    os << BOLDGREEN << "EventType: d19c CIC2" << RESET << std::endl;
    os << BOLDBLUE << "L1A Counter: " << this->GetEventCount() << RESET << std::endl;
    os << "          Be Id: " << +this->getBeBoardId() << std::endl;
    os << "Event Data size: " << +this->GetEventDataSize() << std::endl;
    os << "Bunch Counter: " << this->GetBunch() << std::endl;
    os << BOLDRED << "    TDC Counter: " << +this->GetTDC() << RESET << std::endl;
    os << BOLDRED << "    TLU Trigger ID: " << +this->GetExternalTriggerId() << RESET << std::endl;

    const int FIRST_LINE_WIDTH = 22;
    const int LINE_WIDTH       = 32;
    const int LAST_LINE_WIDTH  = 8;
    // still need to work this out for sparsified data
    if(!fIsSparsified)
    {
        for(uint8_t cHybridId = 0; cHybridId < fEventRawList.size(); cHybridId++)
        {
            for(size_t cReadoutChipId = 0; cReadoutChipId < fEventRawList[cHybridId].second.size(); cReadoutChipId++)
            {
                // print out information
                printL1Header(os, cHybridId, cReadoutChipId);

                std::vector<uint32_t> cHits = this->GetHits(cHybridId, cReadoutChipId);
                if(cHits.size() == NCHANNELS)
                    os << BOLDRED << "All channels firing!" << RESET << std::endl;
                else
                {
                    int cCounter = 0;
                    for(auto& cHit: cHits)
                    {
                        os << std::setw(3) << cHit << " ";
                        cCounter++;
                        if(cCounter == 10)
                        {
                            os << std::endl;
                            cCounter = 0;
                        }
                    }
                    os << RESET << std::endl;
                }
                // channel data
                std::string data(this->DataBitString(cHybridId, cReadoutChipId));
                os << "Ch. Data:      ";
                for(int i = 0; i < FIRST_LINE_WIDTH; i += 2) os << data.substr(i, 2) << " ";

                os << std::endl;

                for(int i = 0; i < 7; ++i)
                {
                    for(int j = 0; j < LINE_WIDTH; j += 2) os << data.substr(FIRST_LINE_WIDTH + LINE_WIDTH * i + j, 2) << " ";

                    os << std::endl;
                }
                for(int i = 0; i < LAST_LINE_WIDTH; i += 2) os << data.substr(FIRST_LINE_WIDTH + LINE_WIDTH * 7 + i, 2) << " ";
                os << std::endl;
                // stubs
                uint8_t cCounter = 0;
                os << BOLDCYAN << "List of Stubs: " << RESET << std::endl;
                for(auto& cStub: this->StubVector(cHybridId, cReadoutChipId))
                {
                    os << CYAN << "Stub: " << +cCounter << " Position: " << +cStub.getPosition() << " Bend: " << +cStub.getBend() << " Strip: " << cStub.getCenter() << RESET << std::endl;
                    cCounter++;
                }
            }
        }
    }
    os << std::endl;
}
std::vector<Cluster> D19cCic2Event::clusterize(uint8_t pHybridId) const
{
    // even hits ---> bottom sensor : cSensorId ==0 [bottom], cSensorId == 1 [top]
    std::vector<Cluster>   cClusters(0);
    auto&                  cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    for(auto cClusterWord: cClusterWords)
    {
        uint8_t cChipId       = (cClusterWord & (0x7 << 11)) >> 11;
        auto    cChipIdMapped = this->getChipIdMapped(pHybridId, cChipId);
        // auto    cChipIdMapped = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cChipId));

        Cluster cCluster;
        uint8_t cFirst         = ((cClusterWord & (0xFF << 3)) >> 3) & 0x7F;
        uint8_t cSensorId      = (((cClusterWord & (0xFF << 3)) >> 3) & (0x1 << 8)) >> 8;
        cCluster.fFirstStrip   = cChipIdMapped * 127 + std::floor(cFirst / 2.); // I think the MSB is the layer ...
        cCluster.fClusterWidth = 1 + (cClusterWord & 0x7);
        cCluster.fSensor       = cSensorId;
        cClusters.push_back(cCluster);
    }
    return cClusters;
}
// TO-DO : replace all get clusters with clusterize
std::vector<Cluster> D19cCic2Event::getClusters(uint8_t pHybridId, uint8_t pReadoutChipId) const
{
    std::vector<Cluster>   cClusters(0);
    auto&                  cClusterWords = fEventHitList[getHybridIndex(pHybridId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    size_t                 cClusterId = 0;
    for(auto cClusterWord: cClusterWords)
    {
        uint8_t cChipId       = (cClusterWord & (0x7 << 11)) >> 11;
        auto    cChipIdMapped = this->getChipIdMapped(pHybridId, cChipId);
        LOG(DEBUG) << BOLDYELLOW << "Cluster in Chip#" << +pReadoutChipId << " which is " << +cChipIdMapped << RESET;
        if(cChipIdMapped != pReadoutChipId) continue;

        uint8_t cLayerId      = ((cClusterWord & (0xFF << 3)) >> 3) & 0x01;        // LSB is the layer
        uint8_t cStrip        = (((cClusterWord & (0xFF << 3)) >> 3) & 0xFE) >> 1; // strip id
        uint8_t cWidth        = 1 + (cClusterWord & 0x7);
        uint8_t cFirstChannel = 2 * cStrip + cLayerId;

        LOG(DEBUG) << BOLDBLUE << "Cluster " << +cClusterId << " : " << std::bitset<CLUSTER_WORD_SIZE>(cClusterWord) << "... " << +cWidth << " strip cluster in strip " << +cStrip << " in layer "
                   << +cLayerId << " so first hit is in channel " << +cFirstChannel << " of chip " << +cChipId << " [ real hybrid  " << +cChipIdMapped << " ]" << RESET;

        Cluster cCluster;
        cCluster.fSensor       = cLayerId;
        cCluster.fFirstStrip   = pReadoutChipId * 127 + cStrip;
        cCluster.fClusterWidth = cWidth;
        cClusters.push_back(cCluster);
        cClusterId++;
    }
    return cClusters;
}
SLinkEvent D19cCic2Event::GetSLinkEvent(BeBoard* pBoard) const
{
    uint32_t   cEvtCount = this->GetEventCount();
    uint16_t   cBunch    = static_cast<uint16_t>(this->GetBunch());
    uint32_t   cBeStatus = this->fBeStatus;
    SLinkEvent cEvent(EventType::VR, pBoard->getConditionDataSet()->getDebugMode(), FrontEndType::CBC3, cEvtCount, cBunch, SOURCE_ID);

    // get link Ids
    std::vector<uint8_t> cLinkIds(0);
    std::set<uint8_t>    cEnabledHybrids;
    for(auto cOpticalGroup: *pBoard)
    {
        cEnabledHybrids.insert(cOpticalGroup->getId());
        cLinkIds.push_back(cOpticalGroup->getId());
    }

    // payload for the status bits
    GenericPayload cStatusPayload;
    LOG(DEBUG) << BOLDBLUE << "Generating S-link event " << RESET;
    // for the hit payload
    GenericPayload cPayload;
    uint16_t       cCbcCounter = 0;
    // now stub payload
    std::string cStubString = "";
    std::string cHitString  = "";
    //
    GenericPayload cStubPayload;
    for(auto cOpticalGroup: *pBoard)
    {
        uint8_t cLinkId = cOpticalGroup->getId();
        // int cHybridWord=0;
        // int cFirstBitFePayload = cPayload.get_current_write_position();
        uint16_t cCbcPresenceWord   = 0;
        uint8_t  cHybridStubCounter = 0;
        auto     cPositionStubs     = cStubString.length();
        auto     cPositionHits      = cHitString.length();
        for(auto cHybrid: *cOpticalGroup)
        {
            uint8_t cHybridId = cHybrid->getId();
            for(auto cChip: *cHybrid)
            {
                uint8_t cChipId     = cChip->getId();
                auto    cDataBitset = getRawL1Word(cHybridId, cChipId);

                /*auto cIndex = 7 - std::distance( fFeMapping.begin() , std::find( fFeMapping.begin(), fFeMapping.end()
                , cChipId ) ) ; if( cIndex >= (int)fEventDataList[cHybridId].second.size() ) continue; auto& cDataBitset =
                fEventDataList[cHybridId].second[ cIndex ];*/

                uint32_t cError       = this->Error(cHybridId, cChipId);
                uint32_t cPipeAddress = this->PipelineAddress(cHybridId, cChipId);
                uint32_t cL1ACounter  = this->L1Id(cHybridId, cChipId);
                uint32_t cStatusWord  = cError << 18 | cPipeAddress << 9 | cL1ACounter;

                auto cNHits = this->GetNHits(cHybridId, cChip->getId());
                // now get the CBC status summary
                if(pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::ERROR)
                    cStatusPayload.append((cError != 0) ? 1 : 0);
                else if(pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::FULL)
                    // assemble the error bits (63, 62, pipeline address and L1A counter) into a status word
                    cStatusPayload.append(cStatusWord, 20);

                // generate the payload
                // the first line sets the cbc presence bits
                cCbcPresenceWord |= 1 << (cChipId + 8 * (cHybridId % 2));

                // 254 channels + 2 padding bits at the end
                std::bitset<NCHANNELS> cBitsetHitData;
                size_t                 cOffset = 2 + 9 + 9;
                for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++)
                {
                    cBitsetHitData[NCHANNELS - 1 - cPos] = cDataBitset[cDataBitset.size() - cOffset - 1 - cPos];
                    // cBitsetHitData[NCHANNELS-1-cPos] = cDataBitset[cDataBitset.size() - cOffset - 1 -cPos];
                }
                LOG(DEBUG) << BOLDBLUE << "Original biset is " << std::bitset<RAW_L1_CBC>(cDataBitset) << RESET;
                // convert bitset to string.. this is useful in case I need to reverse this later
                std::string cOut = cBitsetHitData.to_string() + "00";
                LOG(DEBUG) << BOLDBLUE << "Packed biset is " << cOut << RESET;
                // std::reverse( cOut.begin(), cOut.end() );
                cHitString += cOut;
                if(cNHits > 0)
                {
                    LOG(DEBUG) << BOLDBLUE << "Readout chip " << +cChip->getId() << " on link " << +cLinkId << RESET;
                    //" : " << cBitsetHitData.to_string() << RESET;
                    auto cHits = this->GetHits(cHybridId, cChip->getId());
                    for(auto cHit: this->GetHits(cHybridId, cChip->getId())) { LOG(DEBUG) << BOLDBLUE << "\t... Hit in channel " << +cHit << RESET; }
                }
                // now stubs
                for(auto cStub: this->StubVector(cHybridId, cChip->getId()))
                {
                    std::bitset<16> cBitsetStubs(((cChip->getId() + 8 * (cHybridId % 2)) << 12) | (cStub.getPosition() << 4) | cStub.getBend());
                    cStubString += cBitsetStubs.to_string();
                    LOG(DEBUG) << BOLDBLUE << "\t.. stub in seed " << +cStub.getPosition() << " and bench code " << std::bitset<4>(cStub.getBend()) << RESET;
                    cHybridStubCounter += 1;
                }
                cCbcCounter++;
            } // end of CBC loop
        }     // end of Hybrid loop

        // for the hit payload, I need to insert the word with the number of CBCs at the index I remembered before
        // cPayload.insert (cCbcPresenceWord, cFirstBitFePayload );
        std::bitset<16> cHybridHeader(cCbcPresenceWord);
        cHitString.insert(cPositionHits, cHybridHeader.to_string());
        // for the stub payload .. do the same
        std::bitset<6> cStubHeader(((cHybridStubCounter << 1) | 0x00) & 0x3F); // 0 for 2S , 1 for PS
        LOG(DEBUG) << BOLDBLUE << "Hybrid " << +cLinkId << " hit header " << cHybridHeader << " - stub header  " << std::bitset<6>(cStubHeader) << " : " << +cHybridStubCounter
                   << " stub(s) found on this link." << RESET;
        cStubString.insert(cPositionStubs, cStubHeader.to_string());
    }
    for(size_t cIndex = 0; cIndex < 1 + cHitString.length() / 64; cIndex++)
    {
        auto            cTmp = cHitString.substr(cIndex * 64, 64);
        std::bitset<64> cBitset(cHitString.substr(cIndex * 64, 64));
        uint64_t        cWord = cBitset.to_ulong() << (64 - cTmp.length());
        LOG(DEBUG) << BOLDBLUE << "Hit word " << cTmp.c_str() << " -- word " << std::bitset<64>(cWord) << RESET;
        cPayload.append(cWord);
    }
    for(size_t cIndex = 0; cIndex < 1 + cStubString.length() / 64; cIndex++)
    {
        auto            cTmp = cStubString.substr(cIndex * 64, 64);
        std::bitset<64> cBitset(cStubString.substr(cIndex * 64, 64));
        uint64_t        cWord = cBitset.to_ulong() << (64 - cTmp.length());
        LOG(DEBUG) << BOLDBLUE << "Stub word " << cTmp.c_str() << " -- word " << std::bitset<64>(cWord) << RESET;
        cStubPayload.append(cWord);
    }
    std::vector<uint64_t> cStubData = cStubPayload.Data<uint64_t>();
    for(auto cStub: cStubData) { LOG(DEBUG) << BOLDBLUE << std::bitset<64>(cStub) << RESET; }
    LOG(DEBUG) << BOLDBLUE << +cCbcCounter << " CBCs present in this event... " << +cEnabledHybrids.size() << " Hybrids enabled." << RESET;

    cEvent.generateTkHeader(cBeStatus, cCbcCounter, cEnabledHybrids, pBoard->getConditionDataSet()->getCondDataEnabled(),
                            false); // Be Status, total number CBC, condition data?, fake data?
    // generate a vector of uint64_t with the chip status
    // if (pBoard->getConditionDataSet()->getDebugMode() != SLinkDebugMode::SUMMARY) // do nothing
    cEvent.generateStatus(cStatusPayload.Data<uint64_t>());

    // PAYLOAD
    cEvent.generatePayload(cPayload.Data<uint64_t>());

    // STUBS
    cEvent.generateStubs(cStubPayload.Data<uint64_t>());

    // condition data, first update the values in the vector for I2C values
    uint32_t cTDC = this->GetTDC();
    pBoard->updateCondData(cTDC);
    cEvent.generateConditionData(pBoard->getConditionDataSet());
    cEvent.generateDAQTrailer();
    return cEvent;
}
// TO-DO - sparsified data
// SLinkEvent D19cCic2Event::GetSLinkEvent (  BeBoard* pBoard ) const
// {
//     uint32_t cEvtCount = this->GetEventCount();
//     uint16_t cBunch = static_cast<uint16_t> (this->GetBunch() );
//     uint32_t cBeStatus = this->fBeStatus;
//     SLinkEvent cEvent (EventType::VR, pBoard->getConditionDataSet()->getDebugMode(), FrontEndType::CBC3, cEvtCount,
//     cBunch, SOURCE_ID );

//     // get link Ids
//     std::vector<uint8_t> cLinkIds(0);
//     std::set<uint8_t> cEnabledHybrids;
//     for(auto cOpticalGroup : *pBoard)
//     {
//         cEnabledHybrids.insert (cOpticalGroup->getId());
//         cLinkIds.push_back(cOpticalGroup->getId() );
//     }

//     //payload for the status bits
//     GenericPayload cStatusPayload;
//     LOG (DEBUG) << BOLDBLUE << "Generating S-link event " << RESET;
//     //for the hit payload
//     GenericPayload cPayload;
//     uint16_t cCbcCounter = 0;
//     // now stub payload
//     std::string cStubString = "";
//     std::string cHitString = "";
//     //
//     GenericPayload cStubPayload;
//     for( auto cOpticalGroup : *pBoard )
//     {
//         uint8_t cLinkId = cOpticalGroup->getId();
//         //int cHybridWord=0;
//         //int cFirstBitFePayload = cPayload.get_current_write_position();
//         uint16_t cCbcPresenceWord = 0;
//         uint8_t cHybridStubCounter = 0;
//         auto cPositionStubs = cStubString.length();
//         auto cPositionHits = cHitString.length();
//         for (auto cHybrid : *cOpticalGroup )
//         {
//             uint8_t cHybridId = cHybrid->getId();
//             for (auto cChip : *cHybrid)
//             {
//                 uint8_t cChipId = cChip->getId() ;
//                 auto cDataBitset = getRawL1Word( cHybridId , cChipId);

//                 /*auto cIndex = 7 - std::distance( fFeMapping.begin() , std::find( fFeMapping.begin(),
//                 fFeMapping.end() , cChipId ) ) ; if( cIndex >= (int)fEventDataList[cHybridId].second.size() )
//                     continue;
//                 auto& cDataBitset = fEventDataList[cHybridId].second[ cIndex ];*/

//                 uint32_t cError = this->Error ( cHybridId , cChipId);
//                 uint32_t cPipeAddress = this->PipelineAddress( cHybridId , cChipId);
//                 uint32_t cL1ACounter = this->L1Id( cHybridId , cChipId);
//                 uint32_t cStatusWord = cError << 18 | cPipeAddress << 9 | cL1ACounter;

//                 auto cNHits = this->GetNHits(cHybridId, cChip->getId() );
//                 //now get the CBC status summary
//                 if (pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::ERROR)
//                    cStatusPayload.append ( (cError != 0) ? 1 : 0);
//                 else if (pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::FULL)
//                     //assemble the error bits (63, 62, pipeline address and L1A counter) into a status word
//                     cStatusPayload.append (cStatusWord, 20);

//                 //generate the payload
//                 //the first line sets the cbc presence bits
//                 cCbcPresenceWord |= 1 << (cChipId + 8*(cHybridId%2));

//                 //254 channels + 2 padding bits at the end
//                 std::bitset<NCHANNELS> cBitsetHitData;
//                 size_t cOffset=2+9+9;
//                 for( uint8_t cPos=0; cPos < NCHANNELS ; cPos++)
//                 {
//                     cBitsetHitData[NCHANNELS-1-cPos] = cDataBitset[cDataBitset.size() - cOffset - 1 -cPos];
//                     //cBitsetHitData[NCHANNELS-1-cPos] = cDataBitset[cDataBitset.size() - cOffset - 1 -cPos];
//                 }
//                 LOG (DEBUG) << BOLDBLUE << "Original biset is " << std::bitset<RAW_L1_CBC>(cDataBitset) << RESET;
//                 // convert bitset to string.. this is useful in case I need to reverse this later
//                 std::string cOut = cBitsetHitData.to_string() + "00";
//                 LOG (DEBUG) << BOLDBLUE << "Packed biset is " << cOut << RESET;
//                 //std::reverse( cOut.begin(), cOut.end() );
//                 cHitString += cOut;
//                 if( cNHits > 0 )
//                 {
//                     LOG (DEBUG) << BOLDBLUE << "Readout chip " << +cChip->getId() << " on link " << +cLinkId <<
//                     RESET;
//                     //" : " << cBitsetHitData.to_string() << RESET;
//                     auto cHits = this->GetHits(cHybridId, cChip->getId() );
//                     for( auto cHit : this->GetHits(cHybridId, cChip->getId() ) )
//                     {
//                         LOG (DEBUG) << BOLDBLUE << "\t... Hit in channel " << +cHit << RESET;
//                     }
//                 }
//                 // now stubs
//                 for( auto cStub : this->StubVector (cHybridId , cChip->getId()) )
//                 {
//                     std::bitset<16> cBitsetStubs( ( (cChip->getId() + 8*(cHybridId%2)) << 12) | (cStub.getPosition()
//                     << 4) | cStub.getBend() ); cStubString += cBitsetStubs.to_string(); LOG (INFO) << BOLDBLUE <<
//                     "\t.. stub in seed " << +cStub.getPosition() << " and bench code " <<
//                     std::bitset<4>(cStub.getBend()) << RESET; cHybridStubCounter+=1;
//                 }
//                 cCbcCounter++;
//             } // end of CBC loop
//         }// end of Hybrid loop

//         //for the hit payload, I need to insert the word with the number of CBCs at the index I remembered before
//         //cPayload.insert (cCbcPresenceWord, cFirstBitFePayload );
//         std::bitset<16> cHybridHeader(cCbcPresenceWord);
//         cHitString.insert( cPositionHits,  cHybridHeader.to_string() );
//         // for the stub payload .. do the same
//         std::bitset<6> cStubHeader( (( cHybridStubCounter << 1 ) | 0x00 ) & 0x3F); // 0 for 2S , 1 for PS
//         LOG (DEBUG) << BOLDBLUE << "Hybrid " << +cLinkId << " hit header " << cHybridHeader << " - stub header  " <<
//         std::bitset<6>(cStubHeader) << " : " << +cHybridStubCounter << " stub(s) found on this link." << RESET;
//         cStubString.insert( cPositionStubs,  cStubHeader.to_string() );
//     }
//     for( size_t cIndex=0; cIndex < 1 + cHitString.length()/64 ; cIndex++ )
//     {
//         auto cTmp = cHitString.substr(cIndex*64, 64 );
//         std::bitset<64> cBitset( cHitString.substr(cIndex*64, 64)) ;
//         uint64_t cWord = cBitset.to_ulong() << (64 - cTmp.length());
//         LOG (DEBUG) << BOLDBLUE << "Hit word " << cTmp.c_str() <<  " -- word " << std::bitset<64>(cWord) << RESET;
//         cPayload.append( cWord );
//     }
//     for( size_t cIndex=0; cIndex < 1 + cStubString.length()/64 ; cIndex++ )
//     {
//         auto cTmp = cStubString.substr(cIndex*64, 64 );
//         std::bitset<64> cBitset( cStubString.substr(cIndex*64, 64)) ;
//         uint64_t cWord = cBitset.to_ulong() << (64 - cTmp.length());
//         LOG (DEBUG) << BOLDBLUE << "Stub word " << cTmp.c_str() <<  " -- word " << std::bitset<64>(cWord) << RESET;
//         cStubPayload.append( cWord );
//     }
//     std::vector<uint64_t> cStubData = cStubPayload.Data<uint64_t>();
//     for( auto cStub : cStubData )
//     {
//        LOG (DEBUG) << BOLDBLUE << std::bitset<64>(cStub) << RESET;
//     }
//     LOG (DEBUG) << BOLDBLUE << +cCbcCounter << " CBCs present in this event... " << +cEnabledHybrids.size() << " Hybrids
//     enabled." << RESET;

//     cEvent.generateTkHeader (cBeStatus, cCbcCounter, cEnabledHybrids, pBoard->getConditionDataSet()->getCondDataEnabled(),
//     false);  // Be Status, total number CBC, condition data?, fake data?
//     //generate a vector of uint64_t with the chip status
//     //if (pBoard->getConditionDataSet()->getDebugMode() != SLinkDebugMode::SUMMARY) // do nothing
//     cEvent.generateStatus (cStatusPayload.Data<uint64_t>() );

//     //PAYLOAD
//     cEvent.generatePayload (cPayload.Data<uint64_t>() );

//     //STUBS
//     cEvent.generateStubs (cStubPayload.Data<uint64_t>() );

//     // condition data, first update the values in the vector for I2C values
//     uint32_t cTDC = this->GetTDC();
//     pBoard->updateCondData (cTDC);
//     cEvent.generateConditionData (pBoard->getConditionDataSet() );
//     cEvent.generateDAQTrailer();
//     return cEvent;
// }
} // namespace Ph2_HwInterface
