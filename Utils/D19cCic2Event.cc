/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "../Utils/D19cCic2Event.h"
#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

const unsigned N2SHYBRIDS = 12;

namespace Ph2_HwInterface
{
// Event implementation
D19cCic2Event::D19cCic2Event(const BeBoard* pBoard, const std::vector<uint32_t>& list)
{
    fIsSparsified = pBoard->getSparsification();
    fEventHitList.clear();
    fEventStubList.clear();
    fEventRawList.clear();
    fFeIds.clear();
    fROCIds.clear();
    fNCbc = 0;
    // assuming that FEIds aren't shared between links
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cFe: *cOpticalGroup)
        {
            auto  cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cFe);
            auto& cCic                = cOuterTrackerHybrid->fCic;
            fNCbc += (cCic == NULL) ? cFe->fullSize() : 1;
            FeData cFeData;
            fEventStubList.push_back(cFeData);
            fFeIds.push_back(cFe->getId());
            std::vector<uint8_t> cROCIds(0);
            cROCIds.clear();
            for(auto cChip: *cFe) cROCIds.push_back(cChip->getId());
            fROCIds.push_back(cROCIds);
            if(fIsSparsified) { fEventHitList.push_back(cFeData); }
            else
            {
                RawFeData cRawFeData;
                fEventRawList.push_back(cRawFeData);
            }
        } // hybrids
    }     // opticalGroup
    fBeId        = pBoard->getId();
    fBeFWType    = 0;
    fCBCDataType = 0;
    fBeStatus    = 0;
    this->Set(pBoard, list);
    // this->SetEvent ( pBoard, fNCbc, list );
}

void D19cCic2Event::Set(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
{
    const uint16_t LENGTH_EVENT_HEADER = 4;
    const uint8_t  VALID_L1_HEADER     = 0x0A;
    const uint8_t  VALID_STUB_HEADER   = 0x05;
    uint32_t       cNEvents            = 0;
    auto           cEventIterator      = pData.begin();
    // counters from event header
    fExternalTriggerID = (*(cEventIterator + 1) >> 16) & 0x7FFF;
    fTDC               = (*(cEventIterator + 2) >> 24) & 0xFF;
    fEventCount        = 0x00FFFFFF & *(cEventIterator + 2);
    fBunch             = 0xFFFFFFFF & *(cEventIterator + 3);
    do
    {
        uint32_t cHeader     = (0xFFFF0000 & (*cEventIterator)) >> 16;
        uint32_t cEventSize  = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
        uint32_t cDummyCount = (0xFF & (*(cEventIterator + 1))) * 4;

        LOG(DEBUG) << BOLDBLUE << "Event " << +cNEvents << "... event header is " << std::bitset<16>(cHeader) << " ... " << +cEventSize << " 32 bit words ... " << +cDummyCount
                   << " dummy 32 bit words .. " << RESET;
        // retrieve chunck of data vector belonging to this event
        if(cHeader == 0xFFFF)
        {
            auto     cIterator = cEventIterator + LENGTH_EVENT_HEADER;
            uint32_t cStatus   = 0x00000000;
            size_t   cRocIndex = 0;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cFe: *cOpticalGroup)
                {
                    auto   cOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cFe);
                    auto&  cCic                = cOuterTrackerHybrid->fCic;
                    size_t cNReadoutChips      = (cCic == NULL) ? cFe->fullSize() : 1;
                    for(size_t cIndex = 0; cIndex < cNReadoutChips; cIndex++)
                    {
                        uint8_t  cStatusWord    = 0x00;
                        uint32_t cHitInfoHeader = *(cIterator);
                        uint32_t cGoodHitInfo   = (cHitInfoHeader & (0xF << 28)) >> 28;
                        uint32_t cHitInfoSize   = (cHitInfoHeader & 0xFFF) * 4;
                        size_t   cOffset        = std::distance(pData.begin(), cIterator);
                        cStatusWord             = static_cast<uint8_t>(cGoodHitInfo == VALID_L1_HEADER);
                        LOG(DEBUG) << BOLDBLUE << "\t.. ReadoutChip#" << +cIndex << "...hit info header " << std::bitset<4>(cGoodHitInfo) << "... " << +cHitInfoSize << " words in hit packet..."
                                   << "... status word " << std::bitset<2>(cStatusWord) << RESET;
                        if(cStatusWord == 0x01)
                        {
                            bool                          cWithCIC2 = (cCic->getFrontEndType() == FrontEndType::CIC2);
                            std::pair<uint16_t, uint16_t> cL1Information;
                            cL1Information.first  = (*(cIterator + 2) & 0x7FC000) >> 14;
                            cL1Information.second = (*(cIterator + 2) & 0xFF800000) >> 23;
                            LOG(DEBUG) << BOLDBLUE << "L1 counter for this event : " << +cL1Information.first << " . L1 data size is " << +(cHitInfoSize) << " status "
                                       << std::bitset<9>(cL1Information.second) << RESET;
                            int cL1Offset = cOffset + 2 + int(cWithCIC2);
                            if(fIsSparsified)
                            {
                                uint8_t cNClusters = (*(cIterator + 2) & 0x7F);
                                // clusters/hit data first
                                std::vector<std::bitset<CLUSTER_WORD_SIZE>> cL1Words(cNClusters, 0);
                                this->splitStream(pData, cL1Words, cOffset + 3,
                                                  cNClusters); // split 32 bit words in std::vector of CLUSTER_WORD_SIZE bits
                                fEventHitList[cFe->getIndex()].first = cL1Information;
                                fEventHitList[cFe->getIndex()].second.clear();
                                for(auto cL1Word: cL1Words) { fEventHitList[cFe->getIndex()].second.push_back(cL1Word.to_ulong()); }
                            }
                            else
                            {
                                fEventRawList[cFe->getIndex()].first = cL1Information;
                                fEventRawList[cFe->getIndex()].second.clear();
                                if(cWithCIC2)
                                {
                                    const size_t                            cNblocks = RAW_L1_CBC * cFe->fullSize() / L1_BLOCK_SIZE; // 275 bits per chip ... 8chips... blocks of 11 bits
                                    std::vector<std::bitset<L1_BLOCK_SIZE>> cL1Words(cNblocks, 0);
                                    this->splitStream(pData, cL1Words, cL1Offset,
                                                      cNblocks); // split 32 bit words in  blocks of 11 bits
                                    // now try and arrange them by CBC again ...
                                    for(size_t cChipIndex = 0; cChipIndex < cFe->fullSize(); cChipIndex++)
                                    {
                                        std::bitset<RAW_L1_CBC> cBitset(0);
                                        size_t                  cPosition = 0;
                                        for(size_t cBlockIndex = 0; cBlockIndex < RAW_L1_CBC / L1_BLOCK_SIZE; cBlockIndex++) // RAW_L1_CBC/L1_BLOCK_SIZE blocks per chip
                                        {
                                            auto  cIndex   = cChipIndex + cFe->fullSize() * cBlockIndex;
                                            auto& cL1block = cL1Words[cIndex];
                                            LOG(DEBUG) << BOLDBLUE << "\t\t... L1 block " << +cIndex << " -- " << std::bitset<L1_BLOCK_SIZE>(cL1block) << RESET;
                                            for(size_t cNbit = 0; cNbit < cL1block.size(); cNbit++)
                                            {
                                                cBitset[cBitset.size() - 1 - cPosition] = cL1block[cL1block.size() - 1 - cNbit];
                                                cPosition++;
                                            }
                                        }
                                        LOG(DEBUG) << BOLDBLUE << "\t...  chip " << +cChipIndex << "\t -- " << std::bitset<RAW_L1_CBC>(cBitset) << RESET;
                                        fEventRawList[cFe->getIndex()].second.push_back(cBitset);
                                    }
                                }
                                else
                                {
                                    const size_t                     cNblocks = cFe->fullSize(); // 274 bits per chip ..
                                    const size_t                     cRawL1   = RAW_L1_CBC - 1;
                                    std::vector<std::bitset<cRawL1>> cL1Words(cNblocks, 0);
                                    this->splitStream(pData, cL1Words, cL1Offset,
                                                      cNblocks); // split 32 bit words in  blocks of 274 bits
                                    for(int cIndex = 0; cIndex < (int)(cFe->fullSize()); cIndex++)
                                    {
                                        LOG(DEBUG) << BOLDBLUE << "\t...  chip " << +cIndex << "\t -- " << cL1Words[cIndex] << RESET;
                                        fEventRawList[cFe->getIndex()].second.push_back(std::bitset<RAW_L1_CBC>((cL1Words[cIndex]).to_string() + "0"));
                                    }
                                }
                            }
                        }
                        else
                            throw std::runtime_error(std::string("Incorrect L1 header found when decoding data ... stopping"));

                        // stub info
                        std::pair<uint16_t, uint16_t> cStubInformation;
                        uint32_t                      cStubInfoHeader = *(cIterator + cHitInfoSize);
                        uint32_t                      cGoodStubInfo   = (cStubInfoHeader & (0xF << 28)) >> 28;
                        uint32_t                      cStubInfoSize   = (cStubInfoHeader & 0xFFF) * 4;
                        cStatusWord                                   = cStatusWord | (static_cast<uint8_t>(cGoodStubInfo == VALID_STUB_HEADER) << 1);
                        LOG(DEBUG) << BOLDBLUE << "\t.. ReadoutChip#" << +cIndex << "...stub info header " << std::bitset<4>(cGoodStubInfo) << "... " << +cStubInfoSize << " words in stub packet."
                                   << "... status word " << std::bitset<2>(cStatusWord) << RESET;
                        // for( uint32_t cIndx=0; cIndx < cStubInfoSize ; cIndx++)
                        // {
                        //      LOG (DEBUG) << BOLDBLUE << "\t...#"
                        //         << +cIndx
                        //         << ": " << std::bitset<32>(*(cIterator + cHitInfoSize + cIndx))
                        //         << RESET;
                        // }
                        if(cStatusWord == 0x03)
                        {
                            LOG(DEBUG) << BOLDGREEN << "\t... ReadoutChip#" << +cIndex << " adding stub data.. " << RESET;
                            uint32_t cStubInfo      = *(cIterator + cHitInfoSize + 1);
                            uint8_t  cNStubs        = (cStubInfo & (0x3F << 16)) >> 16;
                            cStubInformation.first  = (cStubInfo & 0xFFF);
                            cStubInformation.second = (cStubInfo & (0x1FF << 22)) >> 22;
                            // LOG (DEBUG) << BOLDBLUE << "BxId for this event : " << +cStubInformation.first << " .
                            // Stub data size is " << +cStubInfoSize << " status " <<
                            // std::bitset<9>(cStubInformation.second) << " -- number of stubs in packet : " << +cNStubs
                            // << RESET;
                            std::vector<std::bitset<STUB_WORD_SIZE>> cStubWords(cNStubs, 0);
                            this->splitStream(pData, cStubWords, cOffset + cHitInfoSize + 2,
                                              cNStubs); // split 32 bit words in std::vector of STUB_WORD_SIZE bits
                            fEventStubList[cFe->getIndex()].first = cStubInformation;
                            fEventStubList[cFe->getIndex()].second.clear();
                            for(auto cStubWord: cStubWords) { fEventStubList[cFe->getIndex()].second.push_back(cStubWord.to_ulong()); }
                        }
                        else
                            throw std::runtime_error(std::string("Incorrect Stub header found when decoding data ... stopping"));
                        cStatus = cStatus | (cStatusWord << (cRocIndex * 2));
                        // increment ROC index
                        cRocIndex++;
                        // increment iterator
                        cIterator += cHitInfoSize + cStubInfoSize;
                    }
                } // hybrid loop
            }     // hybrid loop
        }
        cEventIterator += cEventSize;
        cNEvents++;
    } while(cEventIterator < pData.end());
}
void D19cCic2Event::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            LOG(DEBUG) << BOLDBLUE << "Filling data container for hybrid " << hybrid->getId() << RESET;
            for(auto chip: *hybrid)
            {
                std::vector<uint32_t> cHits = this->GetHits(hybrid->getId(), chip->getId());
                // LOG (DEBUG) << "\t.... " << +cHits.size() << " hits in chip." << RESET;
                for(auto cHit: cHits)
                {
                    if(cTestChannelGroup->isChannelEnabled(cHit)) { chip->getChannelContainer<Occupancy>()->at(cHit).fOccupancy += 1.; }
                }
            }
        }
    }
}
void D19cCic2Event::SetEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list)
{
    // get the first CIC
    auto theFirstCIC = static_cast<OuterTrackerHybrid*>(pBoard->at(0)->at(0))->fCic;
    bool cWithCIC2   = (theFirstCIC->getFrontEndType() == FrontEndType::CIC2);

    fIsSparsified = pBoard->getSparsification();

    fEventHitList.clear();
    fEventStubList.clear();
    for(size_t cFeIndex = 0; cFeIndex < N2SHYBRIDS * 2; cFeIndex++)
    {
        FeData cFeData;
        fEventStubList.push_back(cFeData);
        if(fIsSparsified) { fEventHitList.push_back(cFeData); }
        else
        {
            RawFeData cRawFeData;
            fEventRawList.push_back(cRawFeData);
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

        uint8_t cFeId = (cL1Header & 0xFF0000) >> 16;
        LOG(DEBUG) << BOLDBLUE << "\t.. FE Id from firmware " << +cFeId << " .. putting data in event list .. offset " << +cOffset << RESET;
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
            fEventHitList[cFeId].first = cL1Information;
            fEventHitList[cFeId].second.clear();
            for(auto cL1Word: cL1Words) { fEventHitList[cFeId].second.push_back(cL1Word.to_ulong()); }
        }
        else
        {
            fEventRawList[cFeId].first = cL1Information;
            fEventRawList[cFeId].second.clear();
            // for( uint32_t cIndex=EVENT_HEADER_SIZE+3; cIndex < cL1DataSize; cIndex++)
            //  LOG (INFO) << BOLDBLUE << std::bitset<32>(*(cIterator+cIndex)) << RESET;

            size_t cOpticalGroupIndex = 0;
            size_t cHybridIndex       = 0;
            for(auto cOpticalGroup: *pBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    if(cHybrid->getId() == cFeId)
                    {
                        cOpticalGroupIndex = cOpticalGroup->getIndex();
                        cHybridIndex       = cHybrid->getIndex();
                    }
                }
            }

            auto   cReadoutChips = pBoard->at(cOpticalGroupIndex)->at(cHybridIndex);
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
                    fEventRawList[cFeId].second.push_back(cBitset);
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
                    fEventRawList[cFeId].second.push_back(std::bitset<RAW_L1_CBC>((cL1Words[cIndex]).to_string() + "0"));
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
        std::vector<std::bitset<STUB_WORD_SIZE>> cStubWords(cNStubs, 0);
        this->splitStream(list, cStubWords, cOffset + cL1DataSize + 2,
                          cNStubs); // split 32 bit words in std::vector of STUB_WORD_SIZE bits
        fEventStubList[cFeId].first = cStubInformation;
        fEventStubList[cFeId].second.clear();
        for(auto cStubWord: cStubWords) { fEventStubList[cFeId].second.push_back(cStubWord.to_ulong()); }
        cIterator += cL1DataSize + cStubDataSize;
    } while(cIterator < list.end() - fDummySize);
}

std::bitset<RAW_L1_CBC> D19cCic2Event::getRawL1Word(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto  cChipIdMapped = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto& cDataBitset   = fEventRawList[getFeIndex(pFeId)].second[cChipIdMapped];
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

std::string D19cCic2Event::DataHexString(uint8_t pFeId, uint8_t pCbcId) const
{
    std::stringbuf tmp;
    std::ostream   os(&tmp);
    std::ios       oldState(nullptr);
    oldState.copyfmt(os);
    os << std::hex << std::setfill('0');

    // get the CBC event for pFeId and pCbcId into vector<32bit> cbcData
    std::vector<uint32_t> cbcData;
    GetCbcEvent(pFeId, pCbcId, cbcData);

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
bool D19cCic2Event::Error(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const { return Bit(pFeId, pCbcId, D19C_OFFSET_ERROR_CBC3); }

uint32_t D19cCic2Event::Error(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    // now only 1 bit per chip - OR of a few error flags
    if(fIsSparsified)
    {
        auto&    cHitInformation = fEventHitList[getFeIndex(pFeId)].first;
        auto     cChipIdMapped   = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
        uint32_t cError          = (cHitInformation.second & (0x1 << (1 + cChipIdMapped))) >> (1 + cChipIdMapped);
        return cError;
    }
    else
    {
        auto           cDataBitset = getRawL1Word(pFeId, pReadoutChipId);
        std::bitset<2> cErrorBits(0);
        size_t         cOffset = 0;
        for(size_t cIndex = 0; cIndex < cErrorBits.size(); cIndex++) cErrorBits[cErrorBits.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return (uint32_t)(cErrorBits.to_ulong());
    }
}
uint32_t D19cCic2Event::BxId(uint8_t pFeId) const
{
    auto& cStubInformation = fEventStubList[getFeIndex(pFeId)].first;
    return cStubInformation.first;
}
uint16_t D19cCic2Event::Status(uint8_t pFeId) const
{
    auto& cStubInformation = fEventStubList[getFeIndex(pFeId)].first;
    return cStubInformation.second;
}
uint32_t D19cCic2Event::L1Id(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified)
    {
        auto& cHitInformation = fEventHitList[getFeIndex(pFeId)].first;
        return cHitInformation.first;
    }
    else
    {
        auto cDataBitset = getRawL1Word(pFeId, pReadoutChipId);
        LOG(DEBUG) << BOLDBLUE << "Raw L1 Word is " << cDataBitset << RESET;
        std::bitset<9> cL1Id(0);
        size_t         cOffset = 9 + 2;
        for(size_t cIndex = 0; cIndex < cL1Id.size(); cIndex++) cL1Id[cL1Id.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return cL1Id.to_ulong();
    }
}
// does not apply for sparsified event
uint32_t D19cCic2Event::PipelineAddress(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified) { return 666; }
    else
    {
        auto           cDataBitset = getRawL1Word(pFeId, pReadoutChipId);
        std::bitset<9> cPipeline(0);
        size_t         cOffset = 2;
        for(size_t cIndex = 0; cIndex < cPipeline.size(); cIndex++) cPipeline[cPipeline.size() - 1 - cIndex] = cDataBitset[cDataBitset.size() - cOffset - 1 - cIndex];
        return cPipeline.to_ulong();
    }
}
std::bitset<NCHANNELS> D19cCic2Event::decodeClusters(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto&                  cClusterWords = fEventHitList[getFeIndex(pFeId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    size_t                 cClusterId = 0;
    // LOG (DEBUG) << BOLDBLUE << "Decoding clusters for FE" << +pFeId << " readout chip " << +pReadoutChipId << RESET;
    for(auto cClusterWord: cClusterWords)
    {
        uint8_t cChipId       = ((cClusterWord & (0x7 << 11)) >> 11);
        auto    cChipIdMapped = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cChipId));
        if(cChipIdMapped != pReadoutChipId) continue;

        uint8_t cLayerId      = ((cClusterWord & (0xFF << 3)) >> 3) & 0x01;        // LSB is the layer
        uint8_t cStrip        = (((cClusterWord & (0xFF << 3)) >> 3) & 0xFE) >> 1; // strip id
        uint8_t cWidth        = 1 + (cClusterWord & 0x3);
        uint8_t cFirstChannel = 2 * cStrip + cLayerId;
        ;
        LOG(DEBUG) << BOLDBLUE << "Cluster " << +cClusterId << " : " << std::bitset<CLUSTER_WORD_SIZE>(cClusterWord) << "... " << +cWidth << " strip cluster in strip " << +cStrip << " in layer "
                   << +cLayerId << " so first hit is in channel " << +cFirstChannel << " of chip " << +cChipId << " [ real hybrid  " << +cChipIdMapped << " ]" << RESET;

        for(size_t cOffset = 0; cOffset < cWidth; cOffset++)
        {
            LOG(DEBUG) << BOLDBLUE << "\t\t\t\t.. hit in channel " << +(cFirstChannel + 2 * cOffset) << RESET;
            cBitSet[cFirstChannel + 2 * cOffset] = 1;
        }
        cClusterId++;
    }
    LOG(DEBUG) << BOLDBLUE << "Decoded clusters for FE" << +pFeId << " readout chip " << +pReadoutChipId << " : " << std::bitset<NCHANNELS>(cBitSet) << RESET;
    return cBitSet;
}
bool D19cCic2Event::DataBit(uint8_t pFeId, uint8_t pReadoutChipId, uint32_t i) const
{
    if(fIsSparsified) { return (decodeClusters(pFeId, pReadoutChipId)[i] > 0); }
    else
    {
        size_t cOffset = 0; // 2 + 9 + 9;
        return (getRawL1Word(pFeId, pReadoutChipId)[cOffset + i] > 0);
    }
}

std::string D19cCic2Event::DataBitString(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    if(fIsSparsified)
    {
        std::string cBitStream = std::bitset<NCHANNELS>(this->decodeClusters(pFeId, pReadoutChipId)).to_string();
        // LOG (DEBUG) << BOLDBLUE << "Original bit stream was : " << cBitStream << RESET;
        return cBitStream;
    }
    else
    {
        size_t      cOffset    = 2 + 9 + 9;
        std::string cBitStream = std::bitset<RAW_L1_CBC>(getRawL1Word(pFeId, pReadoutChipId)).to_string();
        return cBitStream.substr(cOffset, RAW_L1_CBC);
    }
}

std::vector<bool> D19cCic2Event::DataBitVector(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    std::vector<bool> blist;
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pFeId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { blist.push_back(cDataBitset[cPos] == 1); }
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = getRawL1Word(pFeId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { blist.push_back(cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1); }
    }
    return blist;
}

std::vector<bool> D19cCic2Event::DataBitVector(uint8_t pFeId, uint8_t pReadoutChipId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> blist;
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pFeId, pReadoutChipId);
        for(auto cChannel: channelList) { blist.push_back(cDataBitset[cChannel] == 1); }
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = getRawL1Word(pFeId, pReadoutChipId);
        for(auto cChannel: channelList) { blist.push_back(cDataBitset[cDataBitset.size() - cOffset - 1 - cChannel] == 1); }
    }
    return blist;
}

std::string D19cCic2Event::GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }

std::vector<Stub> D19cCic2Event::StubVector(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto&             cStubWords    = fEventStubList[getFeIndex(pFeId)].second;
    auto              cChipIdMapped = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    std::vector<Stub> cStubVec;
    for(auto cStubWord: cStubWords)
    {
        uint8_t cChipId      = static_cast<uint8_t>((cStubWord & (0x7 << 12)) >> 12);
        uint8_t cStubAddress = static_cast<uint8_t>((cStubWord & (0xFF << 4)) >> 4);
        uint8_t cStubBend    = static_cast<uint8_t>((cStubWord & (0xF << 0)) >> 0);
        if(cChipId == cChipIdMapped)
        {
            // LOG (DEBUG) << BOLDBLUE << "Stub package ..... " << std::bitset<15>(cStubWord) << " --  chip id from
            // package " << +cChipId << " [ chip id on hybrid " << +pReadoutChipId << "]" << RESET;
            cStubVec.emplace_back(cStubAddress, cStubBend);
        }
    }
    return cStubVec;
}

std::string D19cCic2Event::StubBitString(uint8_t pFeId, uint8_t pCbcId) const
{
    std::ostringstream os;

    std::vector<Stub> cStubVector = this->StubVector(pFeId, pCbcId);

    for(auto cStub: cStubVector) os << std::bitset<8>(cStub.getPosition()) << " " << std::bitset<4>(cStub.getBend()) << " ";

    return os.str();
    // return BitString ( pFeId, pCbcId, OFFSET_CBCSTUBDATA, WIDTH_CBCSTUBDATA );
}

bool D19cCic2Event::StubBit(uint8_t pFeId, uint8_t pCbcId) const
{
    // here just OR the stub positions
    std::vector<Stub> cStubVector = this->StubVector(pFeId, pCbcId);
    return (cStubVector.size() > 0);
}

uint32_t D19cCic2Event::GetNHits(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    uint32_t cNHits = 0;
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pFeId, pReadoutChipId);
        cNHits           = cDataBitset.count();
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = this->getRawL1Word(pFeId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { cNHits += (cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1); }
    }
    return cNHits;
}

std::vector<uint32_t> D19cCic2Event::GetHits(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    std::vector<uint32_t> cHits(0);
    if(fIsSparsified)
    {
        auto cDataBitset = this->decodeClusters(pFeId, pReadoutChipId);
        for(uint32_t i = 0; i < NCHANNELS; ++i)
        {
            if(cDataBitset[i] > 0) { cHits.push_back(i); }
        }
    }
    else
    {
        size_t cOffset     = 2 + 9 + 9;
        auto   cDataBitset = this->getRawL1Word(pFeId, pReadoutChipId);
        for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++)
        {
            if(cDataBitset[cDataBitset.size() - cOffset - 1 - cPos] == 1) cHits.push_back(cPos);
        }
    }
    return cHits;
}

void D19cCic2Event::printL1Header(std::ostream& os, uint8_t pFeId, uint8_t pReadoutChipId) const
{
    os << GREEN << "FEId = " << +pFeId << " CBCId = " << +pReadoutChipId << RESET << std::endl;
    os << YELLOW << "L1 Id: " << this->L1Id(pFeId, pReadoutChipId) << RESET << std::endl;
    os << YELLOW << "Bx Id: " << this->BxId(pFeId) << RESET << std::endl;
    os << RED << "Error: " << static_cast<std::bitset<1>>(this->Error(pFeId, pReadoutChipId)) << RESET << std::endl;
    os << CYAN << "Total number of hits: " << this->GetNHits(pFeId, pReadoutChipId) << RESET << std::endl;
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
        for(uint8_t cFeId = 0; cFeId < fEventRawList.size(); cFeId++)
        {
            for(size_t cReadoutChipId = 0; cReadoutChipId < fEventRawList[cFeId].second.size(); cReadoutChipId++)
            {
                // print out information
                printL1Header(os, cFeId, cReadoutChipId);

                std::vector<uint32_t> cHits = this->GetHits(cFeId, cReadoutChipId);
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
                std::string data(this->DataBitString(cFeId, cReadoutChipId));
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
                for(auto& cStub: this->StubVector(cFeId, cReadoutChipId))
                {
                    os << CYAN << "Stub: " << +cCounter << " Position: " << +cStub.getPosition() << " Bend: " << +cStub.getBend() << " Strip: " << cStub.getCenter() << RESET << std::endl;
                    cCounter++;
                }
            }
        }
    }
    os << std::endl;
}
std::vector<Cluster> D19cCic2Event::clusterize(uint8_t pFeId) const
{
    // even hits ---> bottom sensor : cSensorId ==0 [bottom], cSensorId == 1 [top]
    std::vector<Cluster>   cClusters(0);
    auto&                  cClusterWords = fEventHitList[getFeIndex(pFeId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    for(auto cClusterWord: cClusterWords)
    {
        uint8_t cChipId       = (cClusterWord & (0x3 << 11)) >> 11;
        auto    cChipIdMapped = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cChipId));

        Cluster cCluster;
        uint8_t cFirst         = ((cClusterWord & (0xFF << 3)) >> 3) & 0x7F;
        uint8_t cSensorId      = (((cClusterWord & (0xFF << 3)) >> 3) & (0x1 << 8)) >> 8;
        cCluster.fFirstStrip   = cChipIdMapped * 127 + std::floor(cFirst / 2.); // I think the MSB is the layer ...
        cCluster.fClusterWidth = 1 + (cClusterWord & 0x3);
        cCluster.fSensor       = cSensorId;
        cClusters.push_back(cCluster);
    }
    return cClusters;
}
// TO-DO : replace all get clusters with clusterize
std::vector<Cluster> D19cCic2Event::getClusters(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    std::vector<Cluster>   cClusters(0);
    auto&                  cClusterWords = fEventHitList[getFeIndex(pFeId)].second;
    std::bitset<NCHANNELS> cBitSet(0);
    for(auto cClusterWord: cClusterWords)
    {
        uint8_t cChipId       = (cClusterWord & (0x3 << 11)) >> 11;
        auto    cChipIdMapped = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cChipId));
        if(cChipIdMapped != pReadoutChipId) continue;

        Cluster cCluster;
        uint8_t cFirst         = ((cClusterWord & (0xFF << 3)) >> 3) & 0x7F;
        uint8_t cSensorId      = (((cClusterWord & (0xFF << 3)) >> 3) & (0x1 << 8)) >> 8;
        cCluster.fFirstStrip   = pReadoutChipId * 127 + std::floor(cFirst / 2.); // I think the MSB is the layer ...
        cCluster.fClusterWidth = 1 + (cClusterWord & 0x3);
        cCluster.fSensor       = cSensorId;
        cClusters.push_back(cCluster);
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
    std::set<uint8_t>    cEnabledFe;
    for(auto cOpticalGroup: *pBoard)
    {
        cEnabledFe.insert(cOpticalGroup->getId());
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
        // int cFeWord=0;
        // int cFirstBitFePayload = cPayload.get_current_write_position();
        uint16_t cCbcPresenceWord = 0;
        uint8_t  cFeStubCounter   = 0;
        auto     cPositionStubs   = cStubString.length();
        auto     cPositionHits    = cHitString.length();
        for(auto cHybrid: *cOpticalGroup)
        {
            uint8_t cHybridId = cHybrid->getId();
            for(auto cChip: *cHybrid)
            {
                uint8_t cChipId     = cChip->getId();
                auto    cDataBitset = getRawL1Word(cHybridId, cChipId);

                /*auto cIndex = 7 - std::distance( fFeMapping.begin() , std::find( fFeMapping.begin(), fFeMapping.end()
                , cChipId ) ) ; if( cIndex >= (int)fEventDataList[cFeId].second.size() ) continue; auto& cDataBitset =
                fEventDataList[cFeId].second[ cIndex ];*/

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
                    cFeStubCounter += 1;
                }
                cCbcCounter++;
            } // end of CBC loop
        }     // end of Fe loop

        // for the hit payload, I need to insert the word with the number of CBCs at the index I remembered before
        // cPayload.insert (cCbcPresenceWord, cFirstBitFePayload );
        std::bitset<16> cFeHeader(cCbcPresenceWord);
        cHitString.insert(cPositionHits, cFeHeader.to_string());
        // for the stub payload .. do the same
        std::bitset<6> cStubHeader(((cFeStubCounter << 1) | 0x00) & 0x3F); // 0 for 2S , 1 for PS
        LOG(DEBUG) << BOLDBLUE << "FE " << +cLinkId << " hit header " << cFeHeader << " - stub header  " << std::bitset<6>(cStubHeader) << " : " << +cFeStubCounter << " stub(s) found on this link."
                   << RESET;
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
    LOG(DEBUG) << BOLDBLUE << +cCbcCounter << " CBCs present in this event... " << +cEnabledFe.size() << " FEs enabled." << RESET;

    cEvent.generateTkHeader(cBeStatus, cCbcCounter, cEnabledFe, pBoard->getConditionDataSet()->getCondDataEnabled(),
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
//     std::set<uint8_t> cEnabledFe;
//     for(auto cOpticalGroup : *pBoard)
//     {
//         cEnabledFe.insert (cOpticalGroup->getId());
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
//         //int cFeWord=0;
//         //int cFirstBitFePayload = cPayload.get_current_write_position();
//         uint16_t cCbcPresenceWord = 0;
//         uint8_t cFeStubCounter = 0;
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
//                 fFeMapping.end() , cChipId ) ) ; if( cIndex >= (int)fEventDataList[cFeId].second.size() )
//                     continue;
//                 auto& cDataBitset = fEventDataList[cFeId].second[ cIndex ];*/

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
//                     std::bitset<4>(cStub.getBend()) << RESET; cFeStubCounter+=1;
//                 }
//                 cCbcCounter++;
//             } // end of CBC loop
//         }// end of Fe loop

//         //for the hit payload, I need to insert the word with the number of CBCs at the index I remembered before
//         //cPayload.insert (cCbcPresenceWord, cFirstBitFePayload );
//         std::bitset<16> cFeHeader(cCbcPresenceWord);
//         cHitString.insert( cPositionHits,  cFeHeader.to_string() );
//         // for the stub payload .. do the same
//         std::bitset<6> cStubHeader( (( cFeStubCounter << 1 ) | 0x00 ) & 0x3F); // 0 for 2S , 1 for PS
//         LOG (DEBUG) << BOLDBLUE << "FE " << +cLinkId << " hit header " << cFeHeader << " - stub header  " <<
//         std::bitset<6>(cStubHeader) << " : " << +cFeStubCounter << " stub(s) found on this link." << RESET;
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
//     LOG (DEBUG) << BOLDBLUE << +cCbcCounter << " CBCs present in this event... " << +cEnabledFe.size() << " FEs
//     enabled." << RESET;

//     cEvent.generateTkHeader (cBeStatus, cCbcCounter, cEnabledFe, pBoard->getConditionDataSet()->getCondDataEnabled(),
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
