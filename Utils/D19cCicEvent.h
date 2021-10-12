/*

        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#ifndef __D19cCicEvent_H__
#define __D19cCicEvent_H__

#include "Event.h"
#include <iterator>
#include <numeric>

namespace Ph2_HwInterface
{
using EventDataVector = std::vector<std::vector<uint32_t>>;
using EventMap        = std::map<uint16_t, std::bitset<274>>;
using StubData        = std::pair<std::pair<uint16_t, uint16_t>, std::vector<uint16_t>>;
using EventData       = std::vector<std::pair<StubData, std::vector<std::bitset<274>>>>;
// using Event2S = std::vector<std::pair< std::vector<std::bitset<15>>, std::vector<std::bitset<274>> >>;
/*!
 * \class CicEvent
 * \brief Event container to manipulate event flux from the Cbc2
 */
class D19cCicEvent : public Event
{
  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pNbCbc
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    D19cCicEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCic, uint32_t pNFe, const std::vector<uint32_t>& list);
    /*!
     * \brief Copy Constructor of the Event Class
     */
    // CicEvent ( const Event& pEvent );
    /*!
     * \brief Destructor of the Event Class
     */
    ~D19cCicEvent()
    {
        fEventMap.clear();
        fEventDataList.clear();
    }
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) override;

    /*!
     * \brief Get the Cbc Event counter
     * \return Cbc Event counter
     */
    uint32_t GetEventCountCBC() const override { return fEventCountCBC; }

    // private members of cbc3 events only
    uint32_t getBeBoardId() const { return fBeId; }
    uint8_t  GetFWType() const { return fBeFWType; }
    uint32_t GetCbcDataType() const { return fCBCDataType; }
    uint32_t GetNCbc() const { return fNCbc; }
    uint32_t GetEventDataSize() const { return fEventDataSize; }
    uint32_t GetBeStatus() const { return fBeStatus; }
    /*!
     * \brief Convert Data to Hex string
     * \return Data string in hex
     */
    std::string HexString() const override;
    /*!
     * \brief Function to get bit string in hexadecimal format for CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string in Hex
     */
    std::string DataHexString(uint8_t pFeId, uint8_t pCbcId) const override;

    /*!
     * \brief Function to get Error bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : Error bit number i
     * \return Error bit
     */
    bool Error(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const override;
    /*!
     * \brief Function to get all Error bits
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Error bit
     */
    uint32_t Error(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get pipeline address
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Pipeline address
     */
    uint32_t PipelineAddress(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get a CBC pixel bit data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : pixel bit data number i
     * \return Data Bit
     */
    bool DataBit(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const override;
    /*!
     * \brief Function to get bit string of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string
     */
    std::string DataBitString(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get bit vector of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit vector
     */
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId) const override;
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId, const std::vector<uint8_t>& channelList) const override;
    /*!
     * \brief Function to get GLIB flag string
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Glib flag string
     */
    std::string GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    std::string StubBitString(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    bool StubBit(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Get a vector of Stubs - will be empty for Cbc2
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     */
    std::vector<Stub> StubVector(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to count the Hits in this event
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return number of hits
     */
    uint32_t GetNHits(uint8_t pFeId, uint8_t pCbcId) const override;
    /*!
     * \brief Function to get a sparsified hit vector
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return vector with hit channels
     */
    std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pCbcId) const override;
    std::vector<Cluster>  clusterize(uint8_t pFeId) const;

    std::vector<Cluster> getClusters(uint8_t pFeId, uint8_t pCbcId) const override;

    void fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;

    void     print(std::ostream& out) const override;
    uint32_t L1Id(uint8_t pFeId, uint8_t pReadoutChipId) const;
    uint32_t BxId(uint8_t pFeId) const override;
    uint16_t Status(uint8_t pFeId) const;

  private:
    std::vector<uint8_t> fFeMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC
    EventDataVector      fEventDataVector;
    EventData            fEventDataList;
    EventMap             fEventMap;
    // Event2S fEventList;
    // StubDataVector fStubDataVector;

    std::vector<Cluster> formClusters(std::vector<uint32_t> pHits, int pSensorId) const
    {
        std::vector<Cluster> cClusters;
        if(pHits.size() != 0)
        {
            auto cFirstHit = pHits[0];
            std::transform(pHits.begin(), pHits.end(), pHits.begin(), [cFirstHit](int c) { return c -= cFirstHit; });
            std::vector<int> cDifference(pHits.size());
            std::adjacent_difference(pHits.begin(), pHits.end(), cDifference.begin()); // difference between consecutive elements
            auto cIter  = cDifference.begin();
            auto cStart = cDifference.begin();
            do
            {
                cIter = std::find_if(cIter, cDifference.end(), [](int i) { return (i > 1); });
                Cluster cCluster;
                cCluster.fSensor       = pSensorId;
                cCluster.fFirstStrip   = pHits[std::distance(cDifference.begin(), cStart)];
                cCluster.fClusterWidth = pHits[std::distance(cDifference.begin(), cIter - 1)] - cCluster.fFirstStrip + 1;
                cCluster.fFirstStrip += cFirstHit;
                cClusters.push_back(cCluster);
                cStart = cIter;
                cIter += 1;
            } while(cStart < cDifference.end());
        }
        return cClusters;
    }
    // split stream of data
    // void splitStream(std::vector<uint32_t> pData, const size_t pLength);
    // L1 Id from chip
    void       printL1Header(std::ostream& os, uint8_t pFeId, uint8_t pCbcId) const;
    SLinkEvent GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override;
};
} // namespace Ph2_HwInterface
#endif
