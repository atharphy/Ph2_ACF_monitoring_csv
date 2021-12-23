
#ifndef __D19cPSEventAS_H__
#define __D19cPSEventAS_H__

#include "Event.h"

namespace Ph2_HwInterface
{                                                      // Begin namespace
using RocCounterData    = std::vector<uint32_t>;       // one per chip
using HybridCounterData = std::vector<RocCounterData>; // vector per hybrid
using CounterData       = std::vector<HybridCounterData>;

using EventDataVector = std::vector<std::vector<uint32_t>>;
class D19cPSEventAS : public Event
{
  public:
    D19cPSEventAS(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, uint32_t pNFe, const std::vector<uint32_t>& list);
    D19cPSEventAS(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list);
    ~D19cPSEventAS() {}

    void Set(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list) override;
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list) override;

    uint32_t                GetNHits(uint8_t pFeId, uint8_t pMPAId) const override;
    std::vector<uint32_t>   GetHits(uint8_t pFeId, uint8_t pMPAId) const override;
    EventDataVector         fEventDataVector;
    static constexpr size_t encodeVectorIndex(const uint8_t pFeId, const uint8_t pCbcId, const uint8_t numberOfCBCs) { return pCbcId + pFeId * numberOfCBCs; }
    inline bool             privateDataBit(uint8_t pFeId, uint8_t pMPAId, uint8_t i) const;

    void fillDataContainer(BoardDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup) override;
    void fillChipDataContainer(ChipDataContainer* boardContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId) override;

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

    std::vector<uint8_t> fFeMappingPSR{6, 7, 3, 2, 1, 0, 4, 5}; //  Index Hybrid FE Id , Value CIC FE Id
    std::vector<uint8_t> fFeMappingPSL{1, 0, 4, 5, 6, 7, 3, 2}; // Index hybrid FE Id , Value CIC FE Id
    // mapped id
    // takes chip id on the hybrid
    // returns chip id in the CIC
    uint8_t getChipIdMapped(uint8_t pFeId, uint8_t pReadoutChipId) const
    {
        pReadoutChipId = pReadoutChipId % 8;
        // assign front-end mapping
        std::vector<uint8_t> cFeMapping = (pFeId % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
        return cFeMapping[pReadoutChipId]; // std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), pReadoutChipId));
    }
};

} // namespace Ph2_HwInterface
#endif
