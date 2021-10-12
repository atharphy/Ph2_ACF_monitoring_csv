/*

        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#ifndef __D19cMPAEvent_H__
#define __D19cMPAEvent_H__

#include "Event.h"

namespace Ph2_HwInterface
{ // Begin namespace

using EventDataVector = std::vector<std::vector<uint32_t>>;
using EventHeader     = std::vector<uint32_t>;

class D19cMPAEvent : public Event
{
  public:
    D19cMPAEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, uint32_t pNFe, const std::vector<uint32_t>& list);

    ~D19cMPAEvent() {}
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list) override;

    /*!
     * \brief Get the MPA Event counter
     * \return MPA Event counter
     */
    uint32_t GetEventCountCBC() const override { return fEventCount; }

    // private members of MPA events only
    uint32_t getBeBoardId() const { return fBeId; }
    uint8_t  GetFWType() const { return fBeFWType; }
    uint32_t GetCBCDataType() const { return fCBCDataType; }
    uint32_t GetNC() const { return fNCbc; }
    uint32_t GetEventDataSize() const { return fEventDataSize; }
    uint32_t GetBeStatus() const { return fBeStatus; }
    /*!
     * \brief Convert Data to Hex string
     * \return Data string in hex
     */
    std::string HexString() const override;
    /*!
     * \brief Function to get bit string in hexadecimal format for MPA data
     * \param pFeId : FE Id
     * \param pMPAId : MPA Id
     * \return Data Bit string in Hex
     */
    std::string DataHexString(uint8_t pFeId, uint8_t pMPAId) const override;

    /*!
     * \brief Function to get Error bit
     * \param pFeId : FE Id
     * \param pMPAId : Cbc Id
     * \param i : Error bit number i
     * \return Error bit
     */
    bool Error(uint8_t pFeId, uint8_t pMPAId, uint32_t i) const override;
    /*!
     * \brief Function to get all Error bits
     * \param pFeId : FE Id
     * \param pMPAId : MPA Id
     * \return Error bit
     */
    uint32_t Error(uint8_t pFeId, uint8_t pMPAId) const override;

    /*!
     * \brief Function to count the Hits in this event
     * \param pFeId : FE Id
     * \param pMPAId : MPA Id
     * \return number of hits
     */
    uint32_t GetNHits(uint8_t pFeId, uint8_t pMPAId) const override;
    /*!
     * \brief Function to get a sparsified hit vector
     * \param pFeId : FE Id
     * \param pMPAId : MPA Id
     * \return vector with hit channels
     */
    uint8_t GetNStubs(uint8_t pFeId, uint8_t pMPAId) const;

    std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pMPAId) const override;
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
    std::vector<Cluster> getClusters(uint8_t pFeId, uint8_t pCbcId) const override;
    void                 fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;
    std::string          GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const override;
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

    uint16_t GetMPAL1Counter(uint8_t pFeId, uint8_t pMPAId) const;

    uint8_t GetNStripClusters(uint8_t pFeId, uint8_t pMPAId) const;

    std::vector<SCluster> GetStripClusters(uint8_t pFeId, uint8_t pMPAId) const;

    uint8_t GetNPixelClusters(uint8_t pFeId, uint8_t pMPAId) const;

    std::vector<PCluster> GetPixelClusters(uint8_t pFeId, uint8_t pMPAId) const;

    uint8_t GetMPAChipType(uint8_t pFeId, uint8_t pMPAId) const;

    uint8_t GetMPAChipID(uint8_t pFeId, uint8_t pMPAId) const;

    uint16_t GetMPAHybridID(uint8_t pFeId, uint8_t pMPAId) const;

    uint8_t GetMPAError(uint8_t pFeId, uint8_t pMPAId) const;

    // uint32_t GetSync1( uint8_t pFeId, uint8_t pMPAId) const;

    // uint32_t GetSync2( uint8_t pFeId, uint8_t pMPAId) const;

    uint32_t GetBX1_NStubs(uint8_t pFeId, uint8_t pMPAId) const;

    uint16_t GetStubDataDelay(uint8_t pFeId, uint8_t pMPAId) const;

    uint32_t DivideBy2RoundUp(uint32_t value) const;

    uint32_t GetCluster(std::vector<uint32_t> lvec, uint8_t nclus, uint8_t cClusterSize, uint32_t nbits) const;

    void                    print(std::ostream& out) const override;
    static constexpr size_t encodeVectorIndex(const uint8_t pFeId, const uint8_t pSSAId, const uint8_t numberOfSSAs) { return pSSAId + pFeId * numberOfSSAs; }

  private:
    EventDataVector fEventDataVector;
    EventHeader     fEventHeader;

    uint32_t reverse_bits(uint32_t n) const
    {
        n = ((n >> 1) & 0x55555555) | ((n << 1) & 0xaaaaaaaa);
        n = ((n >> 2) & 0x33333333) | ((n << 2) & 0xcccccccc);
        n = ((n >> 4) & 0x0f0f0f0f) | ((n << 4) & 0xf0f0f0f0);
        n = ((n >> 8) & 0x00ff00ff) | ((n << 8) & 0xff00ff00);
        n = ((n >> 16) & 0x0000ffff) | ((n << 16) & 0xffff0000);
        return n;
    }

    void printMPAHeader(std::ostream& os, uint8_t pFeId, uint8_t pMPAId) const;

    SLinkEvent GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override;
};
} // namespace Ph2_HwInterface
#endif
