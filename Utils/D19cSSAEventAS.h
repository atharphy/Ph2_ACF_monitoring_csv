
#ifndef __D19cSSAEventAS_H__
#define __D19cSSAEventAS_H__

#include "Event.h"

namespace Ph2_HwInterface
{                                                      // Begin namespace
using RocCounterData    = std::vector<uint32_t>;       // one per chip
using HybridCounterData = std::vector<RocCounterData>; // vector per hybrid
using CounterData       = std::vector<HybridCounterData>;

using EventDataVector = std::vector<std::vector<uint32_t>>;
class D19cSSAEventAS : public Event
{
  public:
    D19cSSAEventAS(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list);
    D19cSSAEventAS(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA, uint32_t pNFe, const std::vector<uint32_t>& list);
    ~D19cSSAEventAS() {}
    void Set(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list) override;
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNSSA, const std::vector<uint32_t>& list) override;

    uint32_t                GetNHits(uint8_t pFeId, uint8_t pSSAId) const override;
    std::vector<uint32_t>   GetHits(uint8_t pFeId, uint8_t pSSAId) const override;
    EventDataVector         fEventDataVector;
    static constexpr size_t encodeVectorIndex(const uint8_t pFeId, const uint8_t pCbcId, const uint8_t numberOfCBCs) { return pCbcId + pFeId * numberOfCBCs; }
    void                    fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;
    inline bool             privateDataBit(uint8_t pFeId, uint8_t pSSAId, uint8_t i) const;

    size_t getFeIndex(const uint8_t pFeId) const
    {
        // first find feIndex
        auto cFeIterator = std::find(fFeIds.begin(), fFeIds.end(), pFeId);
        if(cFeIterator != fFeIds.end()) { return std::distance(fFeIds.begin(), cFeIterator); }
        else
            throw std::runtime_error(std::string("FeId not found in D19cCIC2Event .. check xml!"));
    }
    size_t getROCIndex(const uint8_t pFeIndex, const uint8_t pROCId) const
    {
        // first find feIndex
        auto cROCIterator = std::find(fROCIds[pFeIndex].begin(), fROCIds[pFeIndex].end(), pROCId);
        if(cROCIterator != fROCIds[pFeIndex].end()) { return std::distance(fROCIds[pFeIndex].begin(), cROCIterator); }
        else
            throw std::runtime_error(std::string("ROCId not found in D19cCIC2Event .. check xml!"));
    }

  private:
    std::vector<uint8_t>              fFeIds;
    std::vector<std::vector<uint8_t>> fROCIds;
    CounterData                       fCounterData;
};

} // namespace Ph2_HwInterface
#endif
