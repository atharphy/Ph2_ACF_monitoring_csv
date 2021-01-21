/*
  FileName :                     Event.cc
  Content :                      Event handling from DAQ
  Programmer :                   Nicolas PIERRE
  Version :                      1.0
  Date of creation :             10/07/14
  Support :                      mail to : nicolas.pierre@icloud.com
*/

#include "../Utils/Event.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
float Cluster::getBaricentre() { return fFirstStrip + float(fClusterWidth) / 2. - 0.5; }

Event::Event() {}

Event::Event(const Event& pEvent)
    : fEventDataMap(pEvent.fEventDataMap)
    , fEventCount(pEvent.fEventCount)
    , fTDC(pEvent.fTDC)
    , fBunch(pEvent.fBunch)
    , fOrbit(pEvent.fOrbit)
    , fLumi(pEvent.fLumi)
    , fEventCountCBC(pEvent.fEventCountCBC)
    , fEventSize(pEvent.fEventSize)
    , fBeId(pEvent.fBeId)
    , fBeFWType(pEvent.fBeFWType)
    , fCBCDataType(pEvent.fCBCDataType)
    , fNCbc(pEvent.fNCbc)
    , fNSSA(pEvent.fNSSA)
    , fNMPA(pEvent.fNMPA)
    , fEventDataSize(pEvent.fEventDataSize)
    , fBeStatus(pEvent.fBeStatus)
{
}
// Event implementation
bool Event::operator==(const Event& pEvent) const { return fEventDataMap == pEvent.fEventDataMap; }

void Event::GetCbcEvent(const uint8_t& pFeId, const uint8_t& pCbcId, std::vector<uint32_t>& cbcData) const
{
    cbcData.clear();

    uint16_t                     cKey  = encodeId(pFeId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        cbcData.reserve(cData->second.size());
        cbcData.assign(cData->second.begin(), cData->second.end());
    }
    else
        LOG(INFO) << "Event: FE " << +pFeId << " CBC " << +pCbcId << " is not found.";
}

void Event::GetCbcEvent(const uint8_t& pFeId, const uint8_t& pCbcId, std::vector<uint8_t>& cbcData) const
{
    cbcData.clear();

    uint16_t                     cKey  = encodeId(pFeId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        for(const auto& cWord: cData->second)
        {
            cbcData.push_back((cWord >> 24) & 0xFF);
            cbcData.push_back((cWord >> 16) & 0xFF);
            cbcData.push_back((cWord >> 8) & 0xFF);
            cbcData.push_back((cWord)&0xFF);
        }
    }
    else
        LOG(INFO) << "Event: FE " << +pFeId << " CBC " << +pCbcId << " is not found.";
}

bool Event::Bit(uint8_t pFeId, uint8_t pCbcId, uint32_t pPosition) const
{
    uint32_t cWordP = pPosition / 32;
    uint32_t cBitP  = pPosition % 32;

    uint16_t                     cKey  = encodeId(pFeId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        if(cWordP >= cData->second.size()) return false;

        return ((cData->second.at(cWordP) >> (31 - cBitP)) & 0x1);
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " CBC " << +pCbcId << " is not found.";
        return false;
    }
}

std::string Event::BitString(uint8_t pFeId, uint8_t pCbcId, uint32_t pOffset, uint32_t pWidth) const
{
    uint16_t                     cKey  = encodeId(pFeId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        std::ostringstream os;

        for(uint32_t i = 0; i < pWidth; ++i)
        {
            uint32_t pos    = i + pOffset;
            uint32_t cWordP = pos / 32;
            uint32_t cBitP  = pos % 32;

            if(cWordP >= cData->second.size()) break;

            // os << ((cbcData[cByteP] & ( 1 << ( 7 - cBitP ) ))?"1":"0");
            os << ((cData->second[cWordP] >> (31 - cBitP)) & 0x1);
        }

        return os.str();
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " CBC " << +pCbcId << " is not found.";
        return "";
    }
}

std::vector<bool> Event::BitVector(uint8_t pFeId, uint8_t pCbcId, uint32_t pOffset, uint32_t pWidth) const
{
    std::vector<bool>            blist;
    uint16_t                     cKey  = encodeId(pFeId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        std::ostringstream os;

        for(uint32_t i = 0; i < pWidth; ++i)
        {
            uint32_t pos    = i + pOffset;
            uint32_t cWordP = pos / 32;
            uint32_t cBitP  = pos % 32;

            if(cWordP >= cData->second.size()) break;

            blist.push_back((cData->second[cWordP] >> (31 - cBitP)) & 0x1);
        }
    }
    else
        LOG(INFO) << "Event: FE " << +pFeId << " CBC " << +pCbcId << " is not found.";

    return blist;
}
} // namespace Ph2_HwInterface
