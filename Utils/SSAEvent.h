/*

        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#ifndef __SSAEVENT_H__
#define __SSAEVENT_H__

#include "Event.h"

namespace Ph2_HwInterface
{
/*!
 * \class SSAEvent
 * \brief Event container to manipulate event flux from the SSA
 */
class SSAEvent : public Event
{
  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pNbCbc
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    SSAEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list);
    /*!
     * \brief Copy Constructor of the Event Class
     */
    // SSAEvent ( const Event& pEvent );
    /*!
     * \brief Destructor of the Event Class
     */
    ~SSAEvent() {}
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) override { ; }

    uint32_t GetEventCountCBC() const override { return 0; };

    /*!
     * \brief Convert Data to Hex string
     * \return Data string in hex
     */
    std::string HexString() const override { return ""; };
    /*!
     * \brief Function to get bit string in hexadecimal format for CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string in Hex
     */
    std::string DataHexString(uint8_t pFeId, uint8_t pCbcId) const override { return ""; };

    /*!
     * \brief Function to get Error bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : Error bit number i
     * \return Error bit
     */
    bool Error(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const override { return false; };
    /*!
     * \brief Function to get all Error bits
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Error bit
     */
    uint32_t Error(uint8_t pFeId, uint8_t pCbcId) const override { return 0; };
    /*!
     * \brief Function to get pipeline address
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Pipeline address
     */
    uint32_t PipelineAddress(uint8_t pFeId, uint8_t pCbcId) const override { return 0; };
    /*!
     * \brief Function to get a CBC pixel bit data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : pixel bit data number i
     * \return Data Bit
     */
    bool DataBit(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const override { return false; };
    /*!
     * \brief Function to get bit string of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string
     */
    std::string DataBitString(uint8_t pFeId, uint8_t pCbcId) const override { return ""; };
    /*!
     * \brief Function to get bit vector of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit vector
     */
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId) const override { return std::vector<bool>(); };
    std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId, const std::vector<uint8_t>& channelList) const override { return std::vector<bool>(); };
    /*!
     * \brief Function to get GLIB flag string
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Glib flag string
     */
    std::string GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const override { return ""; };
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    std::string StubBitString(uint8_t pFeId, uint8_t pCbcId) const override { return ""; };
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    bool StubBit(uint8_t pFeId, uint8_t pCbcId) const override { return false; };
    /*!
     * \brief Get a vector of Stubs - will be empty for Cbc2
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     */
    std::vector<Stub> StubVector(uint8_t pFeId, uint8_t pCbcId) const override { return std::vector<Stub>(); };

    /*!
     * \brief Function to count the Hits in this event
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return number of hits
     */
    uint32_t GetNHits(uint8_t pFeId, uint8_t pCbcId) const override { return false; };
    /*!
     * \brief Function to get a sparsified hit vector
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return vector with hit channels
     */
    std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pCbcId) const override { return std::vector<uint32_t>(); };

    std::vector<Cluster> getClusters(uint8_t pFeId, uint8_t pCbcId) const override { return std::vector<Cluster>(); };

    void print(std::ostream& out) const override{};

    SLinkEvent GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override { return SLinkEvent(); };
};
} // namespace Ph2_HwInterface

#endif
