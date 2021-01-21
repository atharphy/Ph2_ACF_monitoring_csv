/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "../Utils/D19cCicEvent.h"
#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OuterTrackerModule.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Event implementation
D19cCicEvent::D19cCicEvent(const BeBoard* pBoard, uint32_t pNbCic, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNbCic * pNFe), fEventDataList(pNbCic * pNFe)
{
    LOG(DEBUG) << BOLDBLUE << "Event constructor ... event data list has  " << +fEventDataList.size() << " items." << RESET;
    SetEvent(pBoard, pNbCic, list);
}

void D19cCicEvent::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            LOG(DEBUG) << BOLDBLUE << "Filling data container for hybrid " << hybrid->getId() << RESET;
            for(auto chip: *hybrid)
            {
                std::vector<uint32_t> cHits = this->GetHits(hybrid->getId(), chip->getId());
                LOG(DEBUG) << "\t.... " << +cHits.size() << " hits in chip." << RESET;
                for(auto cHit: cHits)
                {
                    if(cTestChannelGroup->isChannelEnabled(cHit)) { chip->getChannelContainer<Occupancy>()->at(cHit).fOccupancy += 1.; }
                }
            }
        }
    }
}
// void D19cCicEvent::splitStream( std::vector<uint32_t> pData, const size_t pLength )
// {
//     std::string sPacket ="";
//     for( auto cWord : pData )
//     {
//         std::bitset<32> cBitset( cWord ) ;
//         sPacket += cBitset.to_string();
//         if( sPacket.size() > pLength )
//         {
//             std::string cPacket = sPacket.substr(0, pLength);
//             LOG (INFO) << BOLDBLUE << "\t\t..." << cPacket << RESET;
//             if( sPacket.size() - pLength > pLength )
//             {
//                 std::string cRemainder = sPacket.substr(pLength, sPacket.size() - pLength ) ;
//                 sPacket = cRemainder;
//             }
//         }
//     }
// }
void D19cCicEvent::SetEvent(const BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list)
{
    // mapping of FEs for CIC
    std::vector<uint8_t> cFeMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC

    fBeId        = pBoard->getBeId();
    fBeFWType    = 0;
    fCBCDataType = 0;
    fBeStatus    = 0;
    fNCbc        = pNbCbc;

    uint32_t cEventHeader = list.at(0);
    // block size
    fEventSize = 0x0000FFFF & cEventHeader;
    fEventSize *= 4; // block size is in 128 bit words
    fEventDataSize = fEventSize;
    LOG(DEBUG) << BOLDGREEN << std::bitset<32>(cEventHeader) << " : " << +fEventSize << " 32 bit words." << RESET;
    LOG(DEBUG) << BOLDBLUE << "Event header is " << std::bitset<32>(cEventHeader) << RESET;
    LOG(DEBUG) << BOLDBLUE << "Block size is " << +fEventSize << " 32 bit words." << RESET;
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

    // L1
    uint32_t cL1Header = list.at(4);
    uint8_t  cHeader   = (cL1Header & 0xF0000000) >> 28;
    if(cHeader != 0xa)
    {
        LOG(ERROR) << BOLDRED << "Invalid header found in L1 packet." << RESET;
        exit(1);
    }
    uint8_t cErrorCode = (cL1Header & 0xF000000) >> 24;
    if(cErrorCode != 0)
    {
        LOG(ERROR) << BOLDRED << "Error Code " << +cErrorCode << RESET;
        exit(1);
    }
    auto cIterator = list.begin() + 0;
    LOG(DEBUG) << BOLDBLUE << "Event" << +fEventCount << " has " << +list.size() << " 32 bit words [ of which " << +fDummySize << " words are dummy]" << RESET;

    cIterator         = list.begin() + 4;
    uint8_t cHybridId = 0;
    size_t  cOffset   = 4 + 3;
    do
    {
        uint32_t cL1Header      = (*cIterator);
        uint8_t  cFeId          = (*cIterator & 0xFF0000) >> 16;
        uint32_t cL1DataSize    = (*cIterator & 0xFFF) * 4;
        uint8_t  cHeader        = (*cIterator & 0xF0000000) >> 28;
        bool     cValidL1Header = (cHeader == 10);
        if(!cValidL1Header) // cL1DataSize == 0 || cHeader != 10 || (cIterator +cL1DataSize) > list.end() )
        { LOG(INFO) << BOLDRED << "Weird L1 header for hybrid" << +cFeId << RESET; }
        std::vector<uint32_t> cL1data(cIterator + 2, cIterator + cL1DataSize);

        // const size_t L1_BLOCK_SIZE=274;
        // auto& cHybrid = pBoard->fModuleVector[cHybridId];
        // auto& cReadoutChips = cHybrid->fReadoutChipVector;
        // const size_t cNblocks = cReadoutChips.size(); // 275 bits per chip ... 8chips... blocks of 11 bits
        // std::vector<std::bitset<L1_BLOCK_SIZE>> cL1Words(cNblocks , 0);
        // this->splitStream(cL1data , cL1Words , 0 , cNblocks ); // split 32 bit words in std::vector of L1_BLOCK_SIZE
        // bits for(auto cWord : cL1data)
        //     LOG (INFO) << BOLDMAGENTA << "L1 Word is " << std::bitset<32>(cWord) << RESET;
        // for(auto cWord : cL1Words )
        //     LOG (INFO) << BOLDBLUE << "L1 Word is " << std::bitset<L1_BLOCK_SIZE>(cWord) << RESET;

        // std::vector<std::bitset<274>> cL1(8, 0);
        // this->splitStream(cL1data , cL1 , 0 , 8 ); // split 32 bit words in std::vector of 274 bits
        // for(auto cL1Word : cL1 )
        //     LOG (INFO) << BOLDBLUE << std::bitset<274>(cL1Word) << RESET;
        cIterator += cL1DataSize;

        // uint32_t cStubHeaderWord = (*cIterator);
        uint32_t cStubDataSize    = (*(cIterator)&0xFFF) * 4;
        uint8_t  cStubHeader      = (*(cIterator)&0xF0000000) >> 28;
        bool     cValidStubHeader = (cStubHeader == 5);
        if(!cValidStubHeader) // cStubDataSize == 0 || cStubHeader != 5 || (cIterator + cStubDataSize) > list.end() )
        {
            LOG(INFO) << BOLDRED << "Weird Stub header for hybrid" << +cFeId << " - stub : " << std::bitset<32>(*(cIterator)) << " - header is " << std::bitset<4>(cStubHeader) << " -- expect  "
                      << +cStubDataSize << " 128 bits words." << RESET;
        }
        uint16_t              cStubBxId    = (*(cIterator + 1) & 0x00000FFF);
        uint16_t              cStubStatus  = (*(cIterator + 1) & (0x1FF << 22)) >> 22;
        uint8_t               cStubCounter = (*(cIterator + 1) & (0x3F << 16)) >> 16;
        std::vector<uint32_t> cStubDataComplete(cIterator, cIterator + cStubDataSize);
        std::vector<uint32_t> cStubData(cIterator + 2, cIterator + cStubDataSize);

        LOG(DEBUG) << BOLDGREEN << "Hybrid " << +cFeId << "- header : " << std::bitset<32>(cL1Header) << " --- " << cL1DataSize << " 32 bit words" << RESET;
        LOG(DEBUG) << BOLDGREEN << "Stub Header " << std::bitset<4>(cStubHeader) << RESET;
        LOG(DEBUG) << BOLDGREEN << "Stub info : " << std::bitset<32>(*(cIterator + 1)) << " --- " << cStubDataSize << " 32 bit words\t\t .... " << +cStubCounter << " stubs in event ... BxId "
                   << +cStubBxId << RESET;
        cIterator += cStubDataSize;

        // const size_t STUB_BLOCK_SIZE=15;
        // std::vector<std::bitset<STUB_BLOCK_SIZE>> cStubWords(cStubCounter , 0);
        // this->splitStream(list , cStubWords , cOffset+cL1DataSize , cStubCounter ); // split 32 bit words in
        // std::vector of STUB_BLOCK_SIZE bits for(auto cWord : cStubWords )
        //     LOG (INFO) << BOLDMAGENTA << "Stub Word is " << std::bitset<STUB_BLOCK_SIZE>(cWord) << RESET;

        // now store stub information
        // auto cStubIterator = cStubData.begin();
        std::string sStubPacket = "";
        // size_t cStubIndex=0;
        size_t cPacketLength                     = 15;
        fEventDataList[cFeId].first.first.first  = cStubBxId;
        fEventDataList[cFeId].first.first.second = cStubStatus;
        fEventDataList[cFeId].first.second.clear();
        fEventDataList[cFeId].second.clear();
        for(auto cWord: cStubDataComplete) { LOG(DEBUG) << BOLDBLUE << std::bitset<32>(cWord) << RESET; }
        for(auto cWord: cStubData)
        {
            std::bitset<32> cBitset(cWord);
            sStubPacket += cBitset.to_string();
            std::string cRemainder = "";
            for(size_t cIndex = 0; cIndex < std::floor(sStubPacket.length() / cPacketLength); cIndex++)
            {
                std::bitset<15> cDataBitset(sStubPacket.substr(cPacketLength * cIndex, cPacketLength));
                if(sStubPacket.length() - cPacketLength * (cIndex + 1) > 0) cRemainder = sStubPacket.substr(cPacketLength * (cIndex + 1), sStubPacket.length() - cPacketLength * (cIndex + 1));
                LOG(DEBUG) << "\t\t..." << std::bitset<15>(cDataBitset) << " --- left with " << +(sStubPacket.length() - cPacketLength * (cIndex + 1)) << " bits." << RESET;
                if(cDataBitset.to_ulong() != 0) // if all 0s then do not count this stub...
                {
                    uint16_t cStub = cDataBitset.to_ulong();
                    LOG(DEBUG) << BOLDBLUE << "\t.... " << std::bitset<15>(cDataBitset.to_ulong()) << RESET;
                    uint8_t cChipId      = static_cast<uint8_t>((cStub & (0x7 << 12)) >> 12);
                    uint8_t cStubAddress = static_cast<uint8_t>((cStub & (0xFF << 4)) >> 4);
                    uint8_t cStubBend    = static_cast<uint8_t>((cStub & (0xF << 0)) >> 0);
                    LOG(DEBUG) << BOLDBLUE << "\t\t... chip id " << std::bitset<3>(cChipId) << " address " << std::bitset<8>(cStubAddress) << " bend " << std::bitset<4>(cStubBend) << RESET;
                    fEventDataList[cFeId].first.second.push_back(static_cast<uint16_t>(cDataBitset.to_ulong()));
                }
            }
            sStubPacket = cRemainder;
        }
        // and L1 hit information
        std::string sL1packet      = "";
        uint8_t     cReadoutChipId = 0;
        for(auto cWord: cL1data)
        {
            LOG(DEBUG) << BOLDBLUE << "\t....." << std::bitset<32>(cWord) << RESET;
            if(cReadoutChipId >= 8) continue;
            std::bitset<32> cBitset(cWord);
            sL1packet += cBitset.to_string();
            if(sL1packet.size() > 274)
            {
                if(cReadoutChipId < 8)
                {
                    // get complete l1 packet -- 274 bits
                    std::string      cPacket = sL1packet.substr(0, 274);
                    std::bitset<274> cDataBitset(cPacket);
                    fEventDataList[cFeId].second.push_back(cDataBitset);
                    LOG(DEBUG) << BOLDBLUE << "L1 packet " << std::bitset<274>(cDataBitset) << RESET;
                    // get remaining l1 packet for the next readoutchip
                    std::string cRemainder = sL1packet.substr(274, sL1packet.size() - 274);
                    sL1packet              = cRemainder;
                    // LOG (DEBUG) << BOLDBLUE << "\t... remaining word to go into next packet is : " << cRemainder <<
                    // RESET;
                    cReadoutChipId += 1;
                }
            }
        }
        cHybridId++;
        cOffset += cL1DataSize + cStubDataSize;
    } while(cIterator < list.end() - fDummySize);
}

std::string D19cCicEvent::HexString() const
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

std::string D19cCicEvent::DataHexString(uint8_t pFeId, uint8_t pCbcId) const
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
bool D19cCicEvent::Error(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const { return Bit(pFeId, pCbcId, D19C_OFFSET_ERROR_CBC3); }

uint32_t D19cCicEvent::Error(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto             cIndex      = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto&            cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::bitset<274> cMask;
    // L1 data is : 2 error bits + 9 pipeline address + 9 L1A + 254 hit
    // data
    for(uint16_t cPos = 273; cPos > 273 - (2); cPos--) { cMask.set(cPos); }
    uint32_t cError = static_cast<uint32_t>(std::bitset<274>(((cDataBitset & cMask)) >> (NCHANNELS + 9 + 9)).to_ulong());
    return cError;
}
uint32_t D19cCicEvent::BxId(uint8_t pFeId) const { return fEventDataList[pFeId].first.first.first; }
uint16_t D19cCicEvent::Status(uint8_t pFeId) const { return fEventDataList[pFeId].first.first.second; }
uint32_t D19cCicEvent::L1Id(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto             cIndex      = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto&            cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::bitset<274> cMask;
    // L1 data is : 2 error bits + 9 pipeline address + 9 L1A + 254 hit
    // data
    for(uint16_t cPos = 273 - (2 + 9); cPos > 273 - (2 + 9 + 9); cPos--) { cMask.set(cPos); }
    uint32_t cL1Id = static_cast<uint32_t>(std::bitset<274>(((cDataBitset & cMask)) >> (NCHANNELS)).to_ulong());
    return cL1Id;
}
uint32_t D19cCicEvent::PipelineAddress(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto cIndex = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    LOG(DEBUG) << BOLDBLUE << "Decoding pipeline address from chip " << +pReadoutChipId << " on FeHybrid" << +pFeId << " this is stored in " << cIndex << " , event list has "
               << fEventDataList[pFeId].second.size() << " entries." << RESET;
    auto&            cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::bitset<274> cMask;
    // L1 data is : 2 error bits + 9 pipeline address + 9 L1A + 254 hit
    // data
    for(uint16_t cPos = 273 - 2; cPos > 273 - (2 + 9); cPos--) { cMask.set(cPos); }
    uint32_t cPipelineAddress = static_cast<uint32_t>(std::bitset<274>(((cDataBitset & cMask)) >> (NCHANNELS + 9)).to_ulong());
    return cPipelineAddress;
}

bool D19cCicEvent::DataBit(uint8_t pFeId, uint8_t pReadoutChipId, uint32_t i) const
{
    auto  cIndex      = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto& cDataBitset = fEventDataList[pFeId].second[cIndex];
    if(i >= NCHANNELS) return 0;
    std::bitset<274> cMask;
    cMask.set(NCHANNELS - 1 - i);
    uint32_t cNhits = std::bitset<274>(cDataBitset & cMask).count();
    return (cNhits > 0);
}

std::string D19cCicEvent::DataBitString(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto cIndex = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    if(cIndex >= (int)fEventDataList[pFeId].second.size()) return "";
    auto&            cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::bitset<274> cMask;
    for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { cMask.set(NCHANNELS - 1 - cPos); }
    std::string cBitStream = std::bitset<274>(cDataBitset & cMask).to_string();
    cBitStream             = cBitStream.substr(cBitStream.size() - NCHANNELS, NCHANNELS);
    // not sure I need this....
    LOG(DEBUG) << BOLDBLUE << "Original bit stream was : " << cBitStream << RESET;
    std::reverse(cBitStream.begin(), cBitStream.end());
    LOG(DEBUG) << BOLDBLUE << "Now it is : " << cBitStream << RESET;
    return cBitStream;
}

std::vector<bool> D19cCicEvent::DataBitVector(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto              cIndex      = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto&             cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::vector<bool> blist;
    std::bitset<274>  cMask;
    for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++)
    {
        cMask.set(NCHANNELS - 1 - cPos);
        blist.push_back(std::bitset<274>(cDataBitset & cMask).count() > 0);
        cMask.flip(NCHANNELS - 1 - cPos);
    }
    return blist;
}

std::vector<bool> D19cCicEvent::DataBitVector(uint8_t pFeId, uint8_t pReadoutChipId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> blist;
    auto              cIndex      = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    auto&             cDataBitset = fEventDataList[pFeId].second[cIndex];
    std::bitset<274>  cMask;
    for(auto i: channelList)
    {
        cMask.set(NCHANNELS - 1 - i);
        blist.push_back(std::bitset<274>(cDataBitset & cMask).count() > 0);
        cMask.flip(NCHANNELS - 1 - i);
    }
    return blist;
}

std::string D19cCicEvent::GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }

std::string D19cCicEvent::StubBitString(uint8_t pFeId, uint8_t pCbcId) const
{
    std::ostringstream os;

    std::vector<Stub> cStubVector = this->StubVector(pFeId, pCbcId);

    for(auto cStub: cStubVector) os << std::bitset<8>(cStub.getPosition()) << " " << std::bitset<4>(cStub.getBend()) << " ";

    return os.str();
    // return BitString ( pFeId, pCbcId, OFFSET_CBCSTUBDATA, WIDTH_CBCSTUBDATA );
}
std::vector<Stub> D19cCicEvent::StubVector(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    auto              cChipId_CIC = std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    std::vector<Stub> cStubVec;
    auto&             cStubs = fEventDataList[pFeId].first.second;
    for(auto cStub: cStubs)
    {
        uint8_t cChipId      = static_cast<uint8_t>((cStub & (0x7 << 12)) >> 12);
        uint8_t cStubAddress = static_cast<uint8_t>((cStub & (0xFF << 4)) >> 4);
        uint8_t cStubBend    = static_cast<uint8_t>((cStub & (0xF << 0)) >> 0);
        // uint8_t cStubAddress  = static_cast<uint8_t>( (cStub  & (0xFF << 4) ) >> 4);
        // uint8_t cStubBend  = static_cast<uint8_t>( (cStub  & (0xF << 0) ) >> 0);
        if(cChipId == cChipId_CIC)
        {
            LOG(DEBUG) << BOLDBLUE << "Stub package ..... " << std::bitset<15>(cStub) << " --  chip id from package " << +cChipId << " [ chip id on hybrid " << +pReadoutChipId << "]" << RESET;
            cStubVec.emplace_back(cStubAddress, cStubBend);
        }
    }
    return cStubVec;
}
bool D19cCicEvent::StubBit(uint8_t pFeId, uint8_t pCbcId) const
{
    // here just OR the stub positions
    std::vector<Stub> cStubVector = this->StubVector(pFeId, pCbcId);
    return (cStubVector.size() > 0);
}

uint32_t D19cCicEvent::GetNHits(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    uint32_t cNHits = 0;
    if(pFeId > fEventDataList.size())
    {
        LOG(INFO) << BOLDBLUE << "Event data list not big enough..." << RESET;
        return cNHits;
    }
    auto cIndex = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    if(cIndex > (int)fEventDataList[pFeId].second.size())
    {
        LOG(INFO) << BOLDBLUE << "Need information from CBC " << +pReadoutChipId << " at location " << +cIndex << "...event list only has " << +fEventDataList[pFeId].second.size() << RESET;
        return cNHits;
    }
    auto& cDataBitset = fEventDataList[pFeId].second[cIndex];
    LOG(DEBUG) << BOLDBLUE << "L1 data from CIC : " << std::bitset<274>(cDataBitset) << RESET;
    std::bitset<274> cMask;
    for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { cMask.set(NCHANNELS - 1 - cPos); }
    cNHits = std::bitset<274>(cDataBitset & cMask).count();
    return cNHits;
}

std::vector<uint32_t> D19cCicEvent::GetHits(uint8_t pFeId, uint8_t pReadoutChipId) const
{
    std::vector<uint32_t> cHits(0);
    auto                  cIndex = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pReadoutChipId));
    if(cIndex >= 0 && static_cast<size_t>(cIndex) < fEventDataList[pFeId].second.size())
    {
        auto& cDataBitset = fEventDataList[pFeId].second[cIndex];
        LOG(DEBUG) << BOLDBLUE << "Hits for FE" << +pFeId << " Chip" << +cIndex << " are : " << std::bitset<274>(cDataBitset) << RESET;
        std::bitset<274> cMask;
        for(uint32_t i = 0; i < NCHANNELS; ++i)
        {
            cMask.set(NCHANNELS - 1 - i);
            if(std::bitset<274>(cDataBitset & cMask).count() > 0) cHits.push_back(i);
            cMask.flip(NCHANNELS - 1 - i);
        }
    }
    return cHits;
}

void D19cCicEvent::printL1Header(std::ostream& os, uint8_t pFeId, uint8_t pReadoutChipId) const
{
    LOG(INFO) << GREEN << "FEId = " << +pFeId << " CBCId = " << +pReadoutChipId << RESET;
    LOG(INFO) << YELLOW << "L1 Id: " << this->L1Id(pFeId, pReadoutChipId) << RESET;
    LOG(INFO) << YELLOW << "PipelineAddress: " << this->PipelineAddress(pFeId, pReadoutChipId) << RESET;
    LOG(INFO) << RED << "Error: " << static_cast<std::bitset<2>>(this->Error(pFeId, pReadoutChipId)) << RESET;
    LOG(INFO) << CYAN << "Total number of hits: " << this->GetNHits(pFeId, pReadoutChipId) << RESET;
}

void D19cCicEvent::print(std::ostream& os) const
{
    os << BOLDGREEN << "EventType: d19c CIC" << RESET << std::endl;
    os << BOLDBLUE << "L1A Counter: " << this->GetEventCount() << RESET << std::endl;
    os << "          Be Id: " << +this->GetBeId() << std::endl;
    os << "Event Data size: " << +this->GetEventDataSize() << std::endl;
    os << "Bunch Counter: " << this->GetBunch() << std::endl;
    os << BOLDRED << "    TDC Counter: " << +this->GetTDC() << RESET << std::endl;
    os << BOLDRED << "    TLU Trigger ID: " << +this->GetExternalTriggerId() << RESET << std::endl;

    const int FIRST_LINE_WIDTH = 22;
    const int LINE_WIDTH       = 32;
    const int LAST_LINE_WIDTH  = 8;
    uint8_t   cFeId            = 0;
    for(auto& cHybridEvent: fEventDataList)
    {
        // uint8_t cReadoutChipId=0;
        // for( auto cReadoutChipData : cHybridEvent.second )
        for(size_t cReadoutChipId = 0; cReadoutChipId < cHybridEvent.second.size(); cReadoutChipId++)
        {
            // print out information
            os << GREEN << "FEId = " << +cFeId << " CBCId = " << +cReadoutChipId << RESET << std::endl;
            os << GREEN << "TDC: " << this->GetTDC() << RESET << std::endl;
            os << YELLOW << "L1 Id: " << this->L1Id(cFeId, cReadoutChipId) << RESET << std::endl;
            os << YELLOW << "PipelineAddress: " << this->PipelineAddress(cFeId, cReadoutChipId) << RESET << std::endl;
            os << RED << "Error: " << static_cast<std::bitset<2>>(this->Error(cFeId, cReadoutChipId)) << RESET << std::endl;
            os << CYAN << "Total number of hits: " << this->GetNHits(cFeId, cReadoutChipId) << RESET << std::endl;
            //
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
            // cReadoutChipId++;
        }
        cFeId++;
    }
    os << std::endl;
}
std::vector<Cluster> D19cCicEvent::clusterize(uint8_t pFeId) const
{
    // even hits ---> bottom sensor : cSensorId ==0 [bottom], cSensorId == 1 [top]
    std::vector<uint32_t> cHitsTopSensor(0);
    std::vector<uint32_t> cHitsBottomSensor(0);
    for(int cCbcId = 0; cCbcId < 8; cCbcId++)
    {
        auto cIndex = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cCbcId));
        if(cIndex < (int)fEventDataList[pFeId].second.size())
        {
            // get hits
            std::vector<uint32_t> cHits = this->GetHits(pFeId, cCbcId);
            LOG(DEBUG) << BOLDBLUE << "Clusters for CBC" << +cCbcId << " : " << +cHits.size() << " hits found." << RESET;
            std::vector<uint32_t> cTmp(cHits.size());

            // convert to strip number and append to hits in bottom sensor
            auto it = std::copy_if(cHits.begin(), cHits.end(), cTmp.begin(), [](int i) { return (i % 2 == 0); });
            cTmp.resize(std::distance(cTmp.begin(), it));
            std::transform(cTmp.begin(), cTmp.end(), cTmp.begin(), [cCbcId](int c) { return cCbcId * 127 + std::floor(c / 2.); });
            cHitsBottomSensor.insert(cHitsBottomSensor.end(), cTmp.begin(), cTmp.end());

            // resize vector ..
            cTmp.resize(std::distance(cHits.begin(), cHits.end()));

            // convert to strip number and append to hits in bottom sensor
            it = std::copy_if(cHits.begin(), cHits.end(), cTmp.begin(), [](int i) { return (i % 2 != 0); });
            cTmp.resize(std::distance(cTmp.begin(), it));
            std::transform(cTmp.begin(), cTmp.end(), cTmp.begin(), [cCbcId](int c) { return cCbcId * 127 + std::floor(c / 2.); });
            cHitsTopSensor.insert(cHitsTopSensor.end(), cTmp.begin(), cTmp.end());
        }
    }
    LOG(DEBUG) << BOLDBLUE << "Found " << +cHitsBottomSensor.size() << " hits in the bottom sensor" << RESET;
    std::vector<Cluster> cClusters    = formClusters(cHitsBottomSensor, 0);
    std::vector<Cluster> cClustersTop = formClusters(cHitsTopSensor, 1);

    // merge two lists and unmask
    cClusters.insert(cClusters.end(), cClustersTop.begin(), cClustersTop.end());
    return cClusters;
}
// TO-DO : replace all get clusters with clusterize
std::vector<Cluster> D19cCicEvent::getClusters(uint8_t pFeId, uint8_t pCbcId) const
{
    std::vector<Cluster> result;
    // Use the bool vector method (SLOW!) TODO: improve this
    std::vector<bool> stripBits = DataBitVector(pFeId, pCbcId);

    // Cluster finding
    Cluster aCluster;
    for(int iSensor = 0; iSensor < 2; ++iSensor)
    {
        aCluster.fSensor = iSensor;
        bool inCluster   = false;

        for(size_t iStrip = iSensor; iStrip < stripBits.size(); iStrip += 2)
        {
            if(stripBits.at(iStrip))
            {
                // The strip is on
                if(!inCluster)
                {
                    // New cluster
                    aCluster.fFirstStrip   = iStrip / 2;
                    aCluster.fClusterWidth = 1;
                    inCluster              = true;
                }
                else
                {
                    // Increase cluster
                    aCluster.fClusterWidth++;
                }
            }
            else
            {
                // The strip is off
                if(inCluster)
                {
                    inCluster = false;
                    result.push_back(aCluster);
                }
            }
        }

        // Fix clusters at the end of the sensor
        if(inCluster) result.push_back(aCluster);
    }

    return result;
}

SLinkEvent D19cCicEvent::GetSLinkEvent(BeBoard* pBoard) const
{
    // get link Ids
    std::vector<uint8_t> cLinkIds(0);
    std::set<uint8_t>    cEnabledFe;
    for(auto cFe: *pBoard->at(0))
    {
        uint8_t linkId = static_cast<OuterTrackerModule*>(cFe)->getLinkId();
        if(std::find(cLinkIds.begin(), cLinkIds.end(), linkId) == cLinkIds.end())
        {
            cEnabledFe.insert(linkId);
            cLinkIds.push_back(linkId);
        }
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
    for(auto cLinkId: cLinkIds)
    {
        // int cFeWord=0;
        // int cFirstBitFePayload = cPayload.get_current_write_position();
        uint16_t cCbcPresenceWord = 0;
        uint8_t  cFeStubCounter   = 0;
        auto     cPositionStubs   = cStubString.length();
        auto     cPositionHits    = cHitString.length();
        for(auto cFe: *pBoard->at(0))
        {
            uint8_t linkId = static_cast<OuterTrackerModule*>(cFe)->getLinkId();
            if(linkId != cLinkId) continue;

            uint8_t cFeId = cFe->getId();
            for(auto cChip: *cFe)
            {
                uint8_t cChipId = cChip->getId();
                auto    cIndex  = 7 - std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), cChipId));
                if(cIndex >= (int)fEventDataList[cFeId].second.size()) continue;
                auto&    cDataBitset  = fEventDataList[cFeId].second[cIndex];
                uint32_t cError       = this->Error(cFeId, cChipId);
                uint32_t cPipeAddress = this->PipelineAddress(cFeId, cChipId);
                uint32_t cL1ACounter  = this->L1Id(cFeId, cChipId);
                uint32_t cStatusWord  = cError << 18 | cPipeAddress << 9 | cL1ACounter;

                auto cNHits = this->GetNHits(cFe->getId(), cChip->getId());
                // now get the CBC status summary
                if(pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::ERROR)
                    cStatusPayload.append((cError != 0) ? 1 : 0);
                else if(pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::FULL)
                    // assemble the error bits (63, 62, pipeline address and L1A counter) into a status word
                    cStatusPayload.append(cStatusWord, 20);

                // generate the payload
                // the first line sets the cbc presence bits
                cCbcPresenceWord |= 1 << (cChipId + 8 * (cFe->getId() % 2));

                // 254 channels + 2 padding bits at the end
                std::bitset<256> cBitsetHitData;
                for(uint8_t cPos = 0; cPos < NCHANNELS; cPos++) { cBitsetHitData[NCHANNELS - 1 - cPos] = cDataBitset[NCHANNELS - 1 - cPos]; }
                // convert bitset to string.. this is useful in case I need to reverse this later
                cHitString += cBitsetHitData.to_string();
                if(cNHits > 0)
                {
                    LOG(DEBUG) << BOLDBLUE << "Readout chip " << +cChip->getId() << " on link " << +linkId << RESET;
                    //" : " << cBitsetHitData.to_string() << RESET;
                    auto cHits = this->GetHits(cFe->getId(), cChip->getId());
                    for(auto cHit: this->GetHits(cFe->getId(), cChip->getId())) { LOG(DEBUG) << BOLDBLUE << "\t... Hit in channel " << +cHit << RESET; }
                }
                // now stubs
                for(auto cStub: this->StubVector(cFe->getId(), cChip->getId()))
                {
                    std::bitset<16> cBitsetStubs(((cChip->getId() + 8 * (cFe->getId() % 2)) << 12) | (cStub.getPosition() << 4) | cStub.getBend());
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
    uint32_t   cEvtCount = this->GetEventCount();
    uint16_t   cBunch    = static_cast<uint16_t>(this->GetBunch());
    uint32_t   cBeStatus = this->fBeStatus;
    SLinkEvent cEvent(EventType::VR, pBoard->getConditionDataSet()->getDebugMode(), FrontEndType::CBC3, cEvtCount, cBunch, SOURCE_ID);
    cEvent.generateTkHeader(cBeStatus, cCbcCounter, cEnabledFe, pBoard->getConditionDataSet()->getCondDataEnabled(),
                            false); // Be Status, total number CBC, condition data?, fake data?

    // generate a vector of uint64_t with the chip status
    if(pBoard->getConditionDataSet()->getDebugMode() != SLinkDebugMode::SUMMARY) // do nothing
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

// to be checked!!!!
// SLinkEvent D19cCicEvent::GetSLinkEvent (  BeBoard* pBoard) const
// {
//     uint16_t cCbcCounter = 0;
//     std::set<uint8_t> cEnabledFe;

//     //payload for the status bits
//     GenericPayload cStatusPayload;
//     //for the payload and the stubs
//     GenericPayload cPayload;
//     GenericPayload cStubPayload;

//     for (auto cFe : pBoard->fModuleVector)
//     {
//         uint8_t cFeId = cFe->getId();

//         // firt get the list of enabled front ends
//         if (cEnabledFe.find (cFeId) == std::end (cEnabledFe) )
//             cEnabledFe.insert (cFeId);

//         //now on to the payload
//         uint16_t cCbcPresenceWord = 0;
//         int cFirstBitFePayload = cPayload.get_current_write_position();
//         int cFirstBitFeStub = cStubPayload.get_current_write_position();
//         //stub counter per FE
//         uint8_t cFeStubCounter = 0;

//         for (auto cCbc : cFe->fReadoutChipVector)
//         {
//             uint8_t cCbcId = cCbc->getId();
//             uint16_t cKey = encodeId (cFeId, cCbcId);
//             EventDataMap::const_iterator cData = fEventDataMap.find (cKey);

//             if (cData != std::end (fEventDataMap) )
//             {
//                 uint16_t cError = ( cData->second.at (2) >> 30 ) & 0x3;

//                 //now get the CBC status summary
//                 if (pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::ERROR)
//                     cStatusPayload.append ( (cError != 0) ? 1 : 0);

//                 else if (pBoard->getConditionDataSet()->getDebugMode() == SLinkDebugMode::FULL)
//                 {
//                     //assemble the error bits (63, 62, pipeline address and L1A counter) into a status word
//                     uint16_t cPipeAddress = (cData->second.at (2) & 0x000001FF) >> 0;
//                     uint16_t cL1ACounter = (cData->second.at (2) &  0x01FF0000) >> 16;
//                     uint32_t cStatusWord = cError << 18 | cPipeAddress << 9 | cL1ACounter;
//                     cStatusPayload.append (cStatusWord, 20);
//                 }

//                 //generate the payload
//                 //the first line sets the cbc presence bits
//                 cCbcPresenceWord |= 1 << cCbcId;

//                 //first CBC3 channel data word
//                 // i guess we do not need to reverse bits any more
//                 // channels 0-223
//                 for (size_t i = 3; i < 10; i++)
//                 {
//                     uint32_t cWord = (cData->second.at (i));
//                     cPayload.append (cWord);
//                 }
//                 //last channel word (last two bits are empty)
//                 uint32_t cLastChanWord = (cData->second.at (10) & 0xFFFFFFFC) >> 2;
//                 cPayload.append (cLastChanWord, 30);

//                 //don't forget the two padding 0s
//                 cPayload.padZero (2);

//                 //stubs
//                 uint8_t pos1 =  (cData->second.at (13) &  0x000000FF) ;
//                 uint8_t pos2 =   (cData->second.at (13) & 0x0000FF00) >> 8;
//                 uint8_t pos3 =   (cData->second.at (13) & 0x00FF0000) >> 16;
//                 uint8_t bend1 = (cData->second.at (14) & 0x00000F00) >> 8;
//                 uint8_t bend2 = (cData->second.at (14) & 0x000F0000) >> 16;
//                 uint8_t bend3 = (cData->second.at (14) & 0x0F000000) >> 24;

//                 if (pos1 != 0)
//                 {
//                     cStubPayload.append ( uint16_t ( (cCbcId & 0x0F) << 12 | pos1 << 4 | (bend1 & 0xF)) );
//                     cFeStubCounter++;
//                 }

//                 if (pos2 != 0)
//                 {
//                     cStubPayload.append ( uint16_t ( (cCbcId & 0x0F) << 12 | pos2 << 4 | (bend2 & 0xF)) );
//                     cFeStubCounter++;
//                 }

//                 if (pos3 != 0)
//                 {
//                     cStubPayload.append ( uint16_t ( (cCbcId & 0x0F) << 12 | pos3 << 4 | (bend3 & 0xF)) );
//                     cFeStubCounter++;
//                 }
//             }

//             cCbcCounter++;
//         } // end of CBC loop

//         //for the payload, I need to insert the status word at the index I remembered before
//         cPayload.insert (cCbcPresenceWord, cFirstBitFePayload );

//         //for the stubs for this FE, I need to prepend a 5 bit counter shifted by 1 to the right (to account for the
//         0 bit) cStubPayload.insert ( (cFeStubCounter & 0x1F) << 1, cFirstBitFeStub, 6);

//     } // end of Fe loop
//     uint32_t cEvtCount = this->GetEventCount();
//     uint16_t cBunch = static_cast<uint16_t> (this->GetBunch() );
//     uint32_t cBeStatus = this->fBeStatus;
//     SLinkEvent cEvent (EventType::VR, pBoard->getConditionDataSet()->getDebugMode(), FrontEndType::CBC3, cEvtCount,
//     cBunch, SOURCE_ID ); cEvent.generateTkHeader (cBeStatus, cCbcCounter, cEnabledFe,
//     pBoard->getConditionDataSet()->getCondDataEnabled(), false);  // Be Status, total number CBC, condition data?,
//     fake data?

//     //generate a vector of uint64_t with the chip status
//     if (pBoard->getConditionDataSet()->getDebugMode() != SLinkDebugMode::SUMMARY) // do nothing
//         cEvent.generateStatus (cStatusPayload.Data<uint64_t>() );

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
