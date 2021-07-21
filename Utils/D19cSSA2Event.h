
#ifndef __D19cSSA2Event_H__
#define __D19cSSA2Event_H__

#include "Event.h"

namespace Ph2_HwInterface
{ // Begin namespace

using EventDataVector = std::vector<std::vector<uint32_t>>;
using EventHeader     = std::vector<uint32_t>;

class D19cSSA2Event : public Event
{
  public:
    D19cSSA2Event(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA2, uint32_t pNFe, const std::vector<uint32_t>& list);
    ~D19cSSA2Event() {}
    void              SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA2, const std::vector<uint32_t>& list) override;
    uint32_t          GetEventCountCBC() const override { return fEventCountCBC; }
    std::string       HexString() const override;
    std::string       DataHexString(uint8_t pFeId, uint8_t pSSA2Id) const override;
    bool              Error(uint8_t pFeId, uint8_t pSSA2Id, uint32_t i) const override;
    uint32_t          Error(uint8_t pFeId, uint8_t pSSA2Id) const override;
    uint32_t          PipelineAddress(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::string       DataBitString(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pSSA2Id, const std::vector<uint8_t>& channelList) const override;
    std::string       GlibFlagString(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::string       StubBitString(uint8_t pFeId, uint8_t pSSA2Id) const override;
    bool              StubBit(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::vector<Stub> StubVector(uint8_t pFeId, uint8_t pSSA2Id) const override;
    uint32_t          GetNHits(uint8_t pFeId, uint8_t pSSA2Id) const override;
    uint32_t          GetL1Number() const;
    uint32_t          GetTrigID() const;
    uint32_t              GetSSA2L1Counter(uint8_t pFeId, uint8_t pSSA2Id) const;
    std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pSSA2Id) const override;
    std::vector<Cluster>  getClusters(uint8_t pFeId, uint8_t pSSA2Id) const override;
    void                  fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;
    void                  print(std::ostream& out) const override;
    bool                  DataBit(uint8_t pFeId, uint8_t pSSA2Id, uint32_t i) const override { return privateDataBit(pFeId, pSSA2Id, i); };
    inline bool           privateDataBit(uint8_t pFeId, uint8_t pSSA2Id, uint8_t i) const
    {
        try
        {
            return (fEventDataVector.at(encodeVectorIndex(pFeId, pSSA2Id, fNSSA2)).at(calculateChannelWordPosition(119 - i)) >> (calculateChannelBitPosition(119 - i))) & 0x1;
        }
        catch(const std::out_of_range& outOfRange)
        {
            LOG(ERROR) << "Word " << +i << " for FE " << +pFeId << " SSA2 " << +pSSA2Id << " is not found:";
            LOG(ERROR) << "Out of Range error: " << outOfRange.what();
            return false;
        }
    }
  private:
    uint8_t fNSSA2=1;
    EventDataVector           fEventDataVector;
    EventHeader               fEventHeader;
    static constexpr size_t   encodeVectorIndex(const uint8_t pFeId, const uint8_t pSSA2Id, const uint8_t numberOfSSA2s) { return pSSA2Id + pFeId * numberOfSSA2s; }
    static constexpr uint32_t calculateChannelWordPosition(uint32_t channel) { return (channel - channel % 32) / 32; }
    static constexpr uint32_t calculateChannelBitPosition(uint32_t channel) { return 31 - channel % 32; }
    static constexpr uint8_t  getSSA2IdFromVectorIndex(const size_t vectorIndex, const uint8_t numberOfSSA2s) { return vectorIndex / numberOfSSA2s; }
    static constexpr uint8_t  getHybridIdFromVectorIndex(const size_t vectorIndex, const uint8_t numberOfSSA2s) { return vectorIndex % numberOfSSA2s; }
    SLinkEvent                GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override;
};

} // namespace Ph2_HwInterface
#endif
