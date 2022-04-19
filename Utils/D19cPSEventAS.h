
#ifndef __D19cPSEventAS_H__
#define __D19cPSEventAS_H__

#include "Event.h"

namespace Ph2_HwInterface
{                                                      // Begin namespace
using ChipCounterData    = std::vector<uint32_t>;       // one per chip
using HybridCounterData = std::vector<ChipCounterData>; // vector per hybrid
using CounterData       = std::vector<HybridCounterData>;

using EventDataVector = std::vector<std::vector<uint32_t>>;
class D19cPSEventAS : public Event
{
  public:
    D19cPSEventAS(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, uint32_t pNHybrid, const std::vector<uint32_t>& list);
    D19cPSEventAS(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list);
    ~D19cPSEventAS() {}

    void Set(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list) override;
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list) override;

    uint32_t                GetNHits(uint8_t pHybridId, uint8_t pMPAId) const override;
    std::vector<uint32_t>   GetHits(uint8_t pHybridId, uint8_t pMPAId) const override;
    EventDataVector         fEventDataVector;
    static constexpr size_t encodeVectorIndex(const uint8_t pHybridId, const uint8_t pCbcId, const uint8_t numberOfCBCs) { return pCbcId + pHybridId * numberOfCBCs; }
    inline bool             privateDataBit(uint8_t pHybridId, uint8_t pMPAId, uint8_t i) const;

    void fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId) override;

    size_t getHybridIndex(const uint8_t pHybridId) const
    {
        // first find feIndex
        auto cHybridIterator = std::find(fHybridIds.begin(), fHybridIds.end(), pHybridId);
        if(cHybridIterator != fHybridIds.end()) { return std::distance(fHybridIds.begin(), cHybridIterator); }
        else
            throw std::runtime_error(std::string("HybridId not found in D19cCIC2Event .. check xml!"));
    }
    size_t getChipIndex(const uint8_t pHybridIndex, const uint8_t pChipId) const
    {
        // first find feIndex
        auto cChipIterator = std::find(fChipIds[pHybridIndex].begin(), fChipIds[pHybridIndex].end(), pChipId);
        if(cChipIterator != fChipIds[pHybridIndex].end()) { return std::distance(fChipIds[pHybridIndex].begin(), cChipIterator); }
        else
            throw std::runtime_error(std::string("ChipId not found in D19cCIC2Event .. check xml!"));
    }

  private:
    std::vector<uint8_t>              fHybridIds;
    std::vector<std::vector<uint8_t>> fChipIds;
    CounterData                       fCounterData;

    std::vector<uint8_t> fFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5}; //  Index Hybrid Fe Id , Value CIC Fe Id
    std::vector<uint8_t> fFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2}; // Index Hybrid Fe Id , Value CIC Fe Id
    // mapped id
    // takes chip id on the hybrid
    // returns chip id in the CIC
    uint8_t getChipIdMapped(uint8_t pHybridId, uint8_t pReadoutChipId) const
    {
        pReadoutChipId = pReadoutChipId % 8;
        // assign front-end mapping
        std::vector<uint8_t> cFeMapping = (pHybridId % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
        return cFeMapping[pReadoutChipId]; // std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), pReadoutChipId));
    }
};

} // namespace Ph2_HwInterface
#endif
