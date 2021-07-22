#include "../Utils/D19cSSA2Event.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cSSA2Event::D19cSSA2Event(const BeBoard* pBoard, uint32_t pNSSA2, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA2 * pNFe)
{
    fNSSA2 = pNSSA2;
    SetEvent(pBoard, pNSSA2, list);
}
void D19cSSA2Event::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                unsigned int i = 0;
                for(ChannelDataContainer<Occupancy>::iterator channel = chip->begin<Occupancy>(); channel != chip->end<Occupancy>(); channel++, i++)
                {
                    if(cTestChannelGroup->isChannelEnabled(i)) {
                        channel->fOccupancy += (float)privateDataBit(hybrid->getId(), chip->getId(), i);
                        //if (privateDataBit(hybrid->getId(), chip->getId(), i)) LOG (INFO) << "!";
                        }
                }
            }
        }
    }
}
void D19cSSA2Event::SetEvent(const BeBoard* pBoard, uint32_t pNSSA2, const std::vector<uint32_t>& list)
{
    // LOG(INFO) << BOLDBLUE << "NEW"<< RESET;
    //for (auto L : list) LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;

    //start reading here for first SSA2
    auto cIter = list.begin() + 4;
    size_t cIndx=0;
    do
    {
        if( cIndx == 2 ) 
        {
            fL1Id = ((*cIter) >> 16) & 0x1FF;
            LOG (DEBUG) << BOLDGREEN << std::bitset<32>(*cIter) << ".....L1 Id is " << +fL1Id << RESET;
        }
        // if( cIndx >= 4 && cIndx <= 7 )
        // {
        //     LOG (INFO) << BOLDBLUE << "Hit data is " <<  std::bitset<32>(*cIter)  << RESET;   
        // }
        cIter++;
        cIndx++;
    }while( cIter < list.end() );

    std::vector<uint32_t> head;
    head.push_back(list.at(0));
    head.push_back(list.at(1));
    head.push_back(list.at(2));
    head.push_back(list.at(3));
    fEventHeader = head;
    for(uint32_t chip = 0; chip < pNSSA2; chip++)
    {
        uint32_t              cFeId = (list.at(4 + (chip * 12)) & 0xFF0000) >> 16;
        std::vector<uint32_t> lvec;

        // L1 hits
        lvec.push_back(list.at(8 + (chip * 12)));
        lvec.push_back(list.at(9 + (chip * 12)));
        lvec.push_back(list.at(10 + (chip * 12)));
        lvec.push_back(list.at(11 + (chip * 12)));

        // stubs
        lvec.push_back(list.at(13 + (chip * 12)));
        lvec.push_back(list.at(14 + (chip * 12)));
        lvec.push_back(list.at(6 + (chip * 12)));

	    // errors
        lvec.push_back(list.at(4 + (chip * 12)));

        uint32_t cSSA2Id = (list.at(4 + (chip * 12)) & 0xF000) >> 12;

        // LOG(INFO) << BOLDBLUE << "START" << RESET;

        // for (auto L : list)
        //	LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;

        // save it
        fEventDataVector[encodeVectorIndex(cFeId, cSSA2Id, pNSSA2)] = lvec;
        // LOG(INFO) << BOLDBLUE <<"SETTING "<<encodeVectorIndex(cFeId, cSSA2Id, pNSSA2)<< " " << cFeId<< " " <<cSSA2Id<< "
        // " <<pNSSA2<< RESET;
    }
}
uint32_t D19cSSA2Event::L1Id(uint8_t pFeId, uint8_t pCbcId) const
{
   return fL1Id;
}
std::string D19cSSA2Event::HexString() const { return ""; }
std::string D19cSSA2Event::DataHexString(uint8_t pFeId, uint8_t pSSA2Id) const { return ""; }
bool        D19cSSA2Event::Error(uint8_t pFeId, uint8_t pSSA2Id, uint32_t i) const // FIXME NOT WORKING?!
{
    return Bit(pFeId, pSSA2Id, D19C_OFFSET_ERROR_CBC3);
}
uint32_t D19cSSA2Event::Error(uint8_t pFeId, uint8_t pSSA2Id) const
{
    try
    {
        const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id, fNCbc));
        uint32_t                     cError    = ((hitVector.at(7) & 0xF000000) >> 24);
	LOG (INFO) << BOLDBLUE << std::bitset<32>(hitVector.at(7)) << RESET;
        return cError;
    }
    catch(const std::out_of_range& outOfRange)
    {
        LOG(ERROR) << "Word 4 for FE " << +pFeId << " SSA2 " << +pSSA2Id << " is not found:";
        LOG(ERROR) << "Out of Range error: " << outOfRange.what();
        return 0;
    }
}
uint32_t D19cSSA2Event::PipelineAddress(uint8_t pFeId, uint8_t pCbcId) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA2!" << RESET;
    assert(false);
    return 0;
}
std::string D19cSSA2Event::DataBitString(uint8_t pFeId, uint8_t pSSA2Id) const
{
    std::ostringstream os;
    for(uint32_t i = 0; i < NSSACHANNELS; ++i) { os << privateDataBit(pFeId, pSSA2Id, i); }
    return os.str();
}
std::vector<bool> D19cSSA2Event::DataBitVector(uint8_t pFeId, uint8_t pSSA2Id) const
{
    std::vector<bool> blist;
    for(uint32_t i = 0; i < NSSACHANNELS; ++i) { blist.push_back(privateDataBit(pFeId, pSSA2Id, i)); }

    return blist;
}
std::vector<bool> D19cSSA2Event::DataBitVector(uint8_t pFeId, uint8_t pSSA2Id, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> blist;

    for(auto i: channelList) { blist.push_back(privateDataBit(pFeId, pSSA2Id, i)); }
    return blist;
}
std::string D19cSSA2Event::GlibFlagString(uint8_t pFeId, uint8_t pSSA2Id) const { return ""; }
std::string D19cSSA2Event::StubBitString(uint8_t pFeId, uint8_t pSSA2Id) const // FIXME
{
    LOG(ERROR) << "This is not a CBC, this is an SSA2!" << RESET;
    assert(false);
    return "";
}
bool D19cSSA2Event::StubBit(uint8_t pFeId, uint8_t pSSA2Id) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA2!" << RESET;
    assert(false);
    return false;
}
std::vector<Stub> D19cSSA2Event::StubVector(uint8_t pFeId, uint8_t pSSA2Id) const
{
    std::vector<uint32_t> CA(8);
    std::vector<Stub> stubs;
    std::vector<uint32_t> lvec = fEventDataVector[encodeVectorIndex(pFeId, pSSA2Id, fNSSA2)];

    CA.at(0) = (lvec.at(4) & 0xFF);
    CA.at(1) = (lvec.at(4) & 0xFF00) >> 8;
    CA.at(2) = (lvec.at(4) & 0xFF0000) >> 16;
    CA.at(3) = (lvec.at(4) & 0xFF000000) >> 24;
    CA.at(4) = (lvec.at(5) & 0xFF);
    CA.at(5) = (lvec.at(5) & 0xFF00) >> 8;
    CA.at(6) = (lvec.at(5) & 0xFF0000) >> 16;
    CA.at(7) = (lvec.at(5) & 0xFF000000) >> 24;
    for(auto ca: CA)
    {
        if(ca != 0) { stubs.push_back(Stub(ca,0)); }
    }
    return stubs;
}
uint32_t D19cSSA2Event::GetNHits(uint8_t pFeId, uint8_t pSSA2Id) const
{
    try
    {
        uint32_t                     cHits     = 0;
        const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id, fNCbc));
        cHits += __builtin_popcount(hitVector.at(0) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(1) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(2) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(3) & 0xFFFFFF00);
        return cHits;
    }
    catch(const std::out_of_range& outOfRange)
    {
        LOG(ERROR) << "Hits for FE " << +pFeId << " SSA2 " << +pSSA2Id << " is not found:";
        LOG(ERROR) << "Out of Range error: " << outOfRange.what();
        return 0;
    }
}
uint32_t D19cSSA2Event::GetL1Number() const { return fEventHeader.at(2) & 0x00FFFFFF; }
uint32_t D19cSSA2Event::GetTrigID() const { return (fEventHeader.at(1) & 0xFFFF0000) >> 16; }
uint32_t D19cSSA2Event::GetSSA2L1Counter(uint8_t pFeId, uint8_t pSSA2Id) const
{
    const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id, fNCbc));
    return (hitVector.at(6) & 0x01FF0000) >> 16;
}
std::vector<uint32_t> D19cSSA2Event::GetHits(uint8_t pFeId, uint8_t pSSA2Id) const
{
    std::vector<uint32_t> cHits;
    for ( uint32_t i = 0; i < NSSACHANNELS; ++i )
    {
        if (privateDataBit(pFeId, pSSA2Id, i) > 0) cHits.push_back(i);
    }
    return cHits;
}
void D19cSSA2Event::print(std::ostream& os) const // TODO add info here as needed
{
    os << BOLDGREEN << "EventType: d19c SSA2" << RESET << std::endl;
    size_t vectorIndex = 0;
    for(__attribute__((unused)) auto const& hitVector: fEventDataVector)
    {
        uint8_t cFeId  = getHybridIdFromVectorIndex(vectorIndex, fNCbc);
        uint8_t cSSA2Id = getSSA2IdFromVectorIndex(vectorIndex++, fNCbc);
        os << GREEN << "SSA2 Header:" << std::endl;
        os << " FeId: " << +cFeId << " SSA2Id: " << +cSSA2Id << RESET << std::endl;
    }
    os << std::endl;
}
std::vector<Cluster> D19cSSA2Event::getClusters(uint8_t pFeId, uint8_t pSSA2Id) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA2!" << RESET;
    assert(false);
    return std::vector<Cluster>();
}
SLinkEvent D19cSSA2Event::GetSLinkEvent(BeBoard* pBoard) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA2!" << RESET;
    assert(false);
    return SLinkEvent();
}
} // namespace Ph2_HwInterface
