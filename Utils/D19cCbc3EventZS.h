/*

        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Mykyta HARANKO
        \version                       1.0
        \date                                  28/07/17
        Support :                      mail to :

 */

#ifndef __D19cCbc3EventZS_H__
#define __D19cCbc3EventZS_H__

#include "Event.h"

namespace Ph2_HwInterface
{
/*!
 * \class Cbc3Event
 * \brief Event container to manipulate event flux from the Cbc2
 */
class D19cCbc3EventZS : public Event
{
  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pZSEventSize
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    D19cCbc3EventZS(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pZSEventSize, const std::vector<uint32_t>& list);
    /*!
     * \brief Copy Constructor of the Event Class
     */
    // Cbc3Event ( const Event& pEvent );
    /*!
     * \brief Destructor of the Event Class
     */
    ~D19cCbc3EventZS() {}
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pZSEventSize, const std::vector<uint32_t>& list) override;

    /*!
     * \brief Get the Cbc Event counter
     * \return Cbc Event counter
     */
    uint32_t GetEventCountCBC() const override { return fEventCountCBC; }

    // private members of cbc3 events only
    uint32_t GetBeId() const { return fBeId; }
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

    std::vector<Cluster> getClusters(uint8_t pFeId, uint8_t pCbcId) const override;

    void fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) override;

    void print(std::ostream& out) const override;

  private:
    uint8_t fNFe_software;
    uint8_t fNFe_event;
    uint8_t fFeMask_software;
    uint8_t fFeMask_event;

    void printCbcHeader(std::ostream& os, uint8_t pFeId, uint8_t pCbcId) const;

    SLinkEvent GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const override;
};
} // namespace Ph2_HwInterface
#endif
