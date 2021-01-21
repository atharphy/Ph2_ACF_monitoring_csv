#include "../Utils/D19cSSAEvent.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cSSAEvent::D19cSSAEvent(const BeBoard* pBoard, uint32_t pNSSA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNSSA * pNFe)
{
    fNSSA = pNSSA;
    SetEvent(pBoard, pNSSA, list);
}
void D19cSSAEvent::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
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
                    if(cTestChannelGroup->isChannelEnabled(i)) { channel->fOccupancy += (float)privateDataBit(hybrid->getId(), chip->getId(), i); }
                }
            }
        }
    }
}
void D19cSSAEvent::SetEvent(const BeBoard* pBoard, uint32_t pNSSA, const std::vector<uint32_t>& list)
{
    // LOG(INFO) << BOLDBLUE << "NEW"<< RESET;
    // for (auto L : list) LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;

    // start reading here for first SSA
    std::vector<uint32_t> head;
    head.push_back(list.at(0));
    head.push_back(list.at(1));
    head.push_back(list.at(2));
    head.push_back(list.at(3));
    fEventHeader = head;
    for(uint32_t chip = 0; chip < pNSSA; chip++)
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

        uint32_t cSSAId = (list.at(4 + (chip * 12)) & 0xF000) >> 12;

        // LOG(INFO) << BOLDBLUE << "START" << RESET;

        // for (auto L : list)
        //	LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;

        // save it
        fEventDataVector[encodeVectorIndex(cFeId, cSSAId, pNSSA)] = lvec;
        // LOG(INFO) << BOLDBLUE <<"SETTING "<<encodeVectorIndex(cFeId, cSSAId, pNSSA)<< " " << cFeId<< " " <<cSSAId<< "
        // " <<pNSSA<< RESET;
    }
}
std::string D19cSSAEvent::HexString() const { return ""; }
std::string D19cSSAEvent::DataHexString(uint8_t pFeId, uint8_t pSSAId) const { return ""; }
bool        D19cSSAEvent::Error(uint8_t pFeId, uint8_t pSSAId, uint32_t i) const // FIXME NOT WORKING?!
{
    return Bit(pFeId, pSSAId, D19C_OFFSET_ERROR_CBC3);
}
uint32_t D19cSSAEvent::Error(uint8_t pFeId, uint8_t pSSAId) const
{
    try
    {
        const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId, fNCbc));
        uint32_t                     cError    = ((hitVector.at(4) & 0xF000000) >> 24);
        ;
        return cError;
    }
    catch(const std::out_of_range& outOfRange)
    {
        LOG(ERROR) << "Word 4 for FE " << +pFeId << " SSA " << +pSSAId << " is not found:";
        LOG(ERROR) << "Out of Range error: " << outOfRange.what();
        return 0;
    }
}
uint32_t D19cSSAEvent::PipelineAddress(uint8_t pFeId, uint8_t pCbcId) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return 0;
}
std::string D19cSSAEvent::DataBitString(uint8_t pFeId, uint8_t pSSAId) const
{
    std::ostringstream os;
    for(uint32_t i = 0; i < NSSACHANNELS; ++i) { os << privateDataBit(pFeId, pSSAId, i); }
    return os.str();
}
std::vector<bool> D19cSSAEvent::DataBitVector(uint8_t pFeId, uint8_t pSSAId) const
{
    std::vector<bool> blist;
    for(uint32_t i = 0; i < NSSACHANNELS; ++i) { blist.push_back(privateDataBit(pFeId, pSSAId, i)); }

    return blist;
}
std::vector<bool> D19cSSAEvent::DataBitVector(uint8_t pFeId, uint8_t pSSAId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> blist;

    for(auto i: channelList) { blist.push_back(privateDataBit(pFeId, pSSAId, i)); }
    return blist;
}
std::string D19cSSAEvent::GlibFlagString(uint8_t pFeId, uint8_t pSSAId) const { return ""; }
std::string D19cSSAEvent::StubBitString(uint8_t pFeId, uint8_t pSSAId) const // FIXME
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return "";
}
bool D19cSSAEvent::StubBit(uint8_t pFeId, uint8_t pSSAId) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return false;
}
std::vector<Stub> D19cSSAEvent::StubVector(uint8_t pFeId, uint8_t pSSAId) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return std::vector<Stub>();
}
uint32_t D19cSSAEvent::GetNHits(uint8_t pFeId, uint8_t pSSAId) const
{
    try
    {
        uint32_t                     cHits     = 0;
        const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId, fNCbc));
        cHits += __builtin_popcount(hitVector.at(8) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(9) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(10) & 0xFFFFFFFF);
        cHits += __builtin_popcount(hitVector.at(11) & 0xFFFFFF00);
        return cHits;
    }
    catch(const std::out_of_range& outOfRange)
    {
        LOG(ERROR) << "Hits for FE " << +pFeId << " SSA " << +pSSAId << " is not found:";
        LOG(ERROR) << "Out of Range error: " << outOfRange.what();
        return 0;
    }
}
uint32_t D19cSSAEvent::GetL1Number() const { return fEventHeader.at(2) & 0x00FFFFFF; }
uint32_t D19cSSAEvent::GetTrigID() const { return (fEventHeader.at(1) & 0xFFFF0000) >> 16; }
uint32_t D19cSSAEvent::GetSSAL1Counter(uint8_t pFeId, uint8_t pSSAId) const
{
    const std::vector<uint32_t>& hitVector = fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId, fNCbc));
    return (hitVector.at(6) & 0x000F0000) >> 16;
}
std::vector<uint32_t> D19cSSAEvent::GetHits(uint8_t pFeId, uint8_t pSSAId) const
{
    std::vector<uint32_t> CA(8);
    std::vector<uint32_t> stubs;

    // LOG(INFO) << BOLDBLUE <<"ACCESSING "<<encodeVectorIndex(pFeId, pSSAId, fNSSA)<< " " << uint32_t(pFeId)<< " "
    // <<uint32_t(pSSAId)<< " " <<uint32_t(fNSSA)<< RESET;

    std::vector<uint32_t> lvec = fEventDataVector[encodeVectorIndex(pFeId, pSSAId, fNSSA)];
    // LOG(INFO) << BOLDBLUE <<"lvec.size() "<<lvec.size()<< RESET;

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
        // LOG(INFO) << BOLDBLUE <<ca<< RESET;

        if(ca != 0) { stubs.push_back(ca); }
    }
    return stubs;
    // std::vector<uint32_t> cHits;
    // for ( uint32_t i = 0; i < NSSACHANNELS; ++i )
    //{
    //	cHits.push_back(privateDataBit(pFeId, pSSAId, i));
    //}
    // return cHits;
}
void D19cSSAEvent::print(std::ostream& os) const // TODO add info here as needed
{
    os << BOLDGREEN << "EventType: d19c SSA" << RESET << std::endl;
    size_t vectorIndex = 0;
    for(__attribute__((unused)) auto const& hitVector: fEventDataVector)
    {
        uint8_t cFeId  = getFeIdFromVectorIndex(vectorIndex, fNCbc);
        uint8_t cSSAId = getSSAIdFromVectorIndex(vectorIndex++, fNCbc);
        os << GREEN << "SSA Header:" << std::endl;
        os << " FeId: " << +cFeId << " SSAId: " << +cSSAId << RESET << std::endl;
    }
    os << std::endl;
}
std::vector<Cluster> D19cSSAEvent::getClusters(uint8_t pFeId, uint8_t pSSAId) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return std::vector<Cluster>();
}
SLinkEvent D19cSSAEvent::GetSLinkEvent(BeBoard* pBoard) const
{
    LOG(ERROR) << "This is not a CBC, this is an SSA!" << RESET;
    assert(false);
    return SLinkEvent();
}
} // namespace Ph2_HwInterface
