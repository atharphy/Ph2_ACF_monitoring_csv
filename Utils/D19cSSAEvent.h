
#ifndef __D19cSSAEvent_H__
#define __D19cSSAEvent_H__

#include "Event.h"

namespace Ph2_HwInterface
{ // Begin namespace

using EventDataVector = std::vector<std::vector<uint32_t>>;
using EventHeader     = std::vector<uint32_t>;

class D19cSSAEvent : public Event
{
  public:
    D19cSSAEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA, uint32_t pNFe, const std::vector<uint32_t>& list);
    ~D19cSSAEvent() {}
    void              SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA, const std::vector<uint32_t>& list) override;
    uint32_t          GetEventCountCBC() const override { return fEventCountCBC; }
    std::string       HexString() const override;
    std::string       DataHexString(uint8_t pFeId, uint8_t pSSAId) const override;
    bool              Error(uint8_t pFeId, uint8_t pSSAId, uint32_t i) const override;
    uint32_t          Error(uint8_t pFeId, uint8_t pSSAId) const override;
    uint32_t          PipelineAddress(uint8_t pFeId, uint8_t pSSAId) const override;
    std::string       DataBitString(uint8_t pFeId, uint8_t pSSAId) const override;
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pSSAId) const override;
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pSSAId, const std::vector<uint8_t>& channelList) const override;
    std::string       GlibFlagString(uint8_t pFeId, uint8_t pSSAId) const override;
    std::string       StubBitString(uint8_t pFeId, uint8_t pSSAId) const override;
    bool              StubBit(uint8_t pFeId, uint8_t pSSAId) const override;
    std::vector<Stub> StubVector(uint8_t pFeId, uint8_t pSSAId) const override;
    uint32_t          GetNHits(uint8_t pFeId, uint8_t pSSAId) const override;
    uint32_t          GetL1Number() const;
    uint32_t          GetTrigID() const;

    uint32_t              GetSSAL1Counter(uint8_t pFeId, uint8_t pSSAId) const;
    std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pSSAId) const override;
    std::vector<Cluster>  getClusters(uint8_t pFeId, uint8_t pSSAId) const override;
    void                  fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;
    void                  print(std::ostream& out) const override;
    bool                  DataBit(uint8_t pFeId, uint8_t pSSAId, uint32_t i) const override { return privateDataBit(pFeId, pSSAId, i); };
    inline bool           privateDataBit(uint8_t pFeId, uint8_t pSSAId, uint8_t i) const
    {
        try
        {
            return (fEventDataVector.at(encodeVectorIndex(pFeId, pSSAId, fNSSA)).at(calculateChannelWordPosition(119 - i)) >> (calculateChannelBitPosition(119 - i))) & 0x1;
        }
        catch(const std::out_of_range& outOfRange)
        {
            LOG(ERROR) << "Word " << +i << " for FE " << +pFeId << " SSA " << +pSSAId << " is not found:";
            LOG(ERROR) << "Out of Range error: " << outOfRange.what();
            return false;
        }
    }

  private:
    EventDataVector           fEventDataVector;
    EventHeader               fEventHeader;
    static constexpr size_t   encodeVectorIndex(const uint8_t pFeId, const uint8_t pSSAId, const uint8_t numberOfSSAs) { return pSSAId + pFeId * numberOfSSAs; }
    static constexpr uint32_t calculateChannelWordPosition(uint32_t channel) { return (channel - channel % 32) / 32; }
    static constexpr uint32_t calculateChannelBitPosition(uint32_t channel) { return 31 - channel % 32; }
    static constexpr uint8_t  getSSAIdFromVectorIndex(const size_t vectorIndex, const uint8_t numberOfSSAs) { return vectorIndex / numberOfSSAs; }
    static constexpr uint8_t  getFeIdFromVectorIndex(const size_t vectorIndex, const uint8_t numberOfSSAs) { return vectorIndex % numberOfSSAs; }
    SLinkEvent                GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override;
};

} // namespace Ph2_HwInterface
#endif
