/*
        \file                          Event.h
        \brief                         Event handling from DAQ
        \author                        Nicolas PIERRE
        \version                       1.0
        \date                                  10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com
 */

#ifndef __EVENT_H__
#define __EVENT_H__

#include "../HWDescription/BeBoard.h"
#include "../HWDescription/Definition.h"
#include "../Utils/easylogging++.h"
#include "ConsoleColor.h"
#include "SLinkEvent.h"
#include <bitset>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

class BoardDataContainer;
class ChannelGroupBase;

namespace Ph2_HwInterface
{
using EventDataMap = std::map<uint16_t, std::vector<uint32_t>>;

/*!
 * \class Cluster
 * \brief Cluster object for the Event
 */
class Cluster
{
  public:
    uint8_t fSensor;
    uint8_t fFirstStrip;
    uint8_t fClusterWidth;
    float   getBaricentre();
};

class PCluster
{
  public:
    uint8_t fAddress;
    uint8_t fWidth;
    uint8_t fZpos;
    float   getBaricentre();
};

class SCluster
{
  public:
    uint8_t fAddress;
    uint8_t fMip;
    uint8_t fWidth;
    float   getBaricentre();
};

class Stub
{
  public:
    Stub(uint8_t pPosition, uint8_t pBend, uint8_t pRow = 0) : fPosition(pPosition), fBend(pBend), fRow(pRow)
    {
        // with Strips starting at 0
        fCenter = static_cast<float>((pPosition / 2.) - 1);
    }
    uint8_t getPosition() { return fPosition; }
    uint8_t getBend() { return fBend; }
    uint8_t getRow() { return fRow; }
    float   getCenter() { return fCenter; }

  private:
    uint8_t fPosition;
    uint8_t fBend;
    uint8_t fRow;
    float   fCenter;
};
/*!
 * \class Event
 * \brief Event container to manipulate event flux from the Cbc
 */

class Event
{
    /*
       id of FeEvent should be the order of FeEvents in data stream starting from 0
       id of CbcEvent also should be the order of CBCEvents in data stream starting from 0
     */
  public:
    EventDataMap fEventDataMap;

  protected:
    uint32_t fEventCount;        /*!< Event Counter */
    uint32_t fTDC;               /*!< TDC value*/
    uint32_t fExternalTriggerID; /*!< TLU Trigger ID*/
    // for CBC2 use
    uint32_t fBunch;         /*!< Bunch value */
    uint32_t fOrbit;         /*!< Orbit value */
    uint32_t fLumi;          /*!< LuminositySection value */
    uint32_t fEventCountCBC; /*!< Cbc Event Counter */
    uint32_t fEventSize;

    // for CBC3 use
    uint8_t  fBeId;
    uint8_t  fBeFWType;
    uint8_t  fCBCDataType;
    uint8_t  fNCbc;
    uint8_t  fNSSA;
    uint8_t  fNMPA;
    uint16_t fEventDataSize;
    uint32_t fBeStatus;

    uint16_t encodeId(const uint8_t& pFeId, const uint8_t& pCbcId) const { return (pFeId << 8 | pCbcId); }

    void decodeId(const uint16_t& pKey, uint8_t& pFeId, uint8_t& pCbcId) const
    {
        pFeId  = (pKey >> 8) & 0x00FF;
        pCbcId = pKey & 0xFF;
    }

  public:
    /*!
     * \brief Constructor of the Event Class
     * \param pBoard : Board to work with
     * \param pNbCbc
     * \param pEventBuf : the pointer to the raw Event buffer of this Event
     */
    Event();
    /*!
     * \brief Copy Constructor of the Event Class
     */
    Event(const Event& pEvent);
    /*!
     * \brief Destructor of the Event Class
     */
    virtual ~Event() {}
    /*!
     * \brief Clear the Event Map
     */
    void Clear() { fEventDataMap.clear(); }
    /*! \brief Get raw data */
    // const std::vector<uint32_t>& GetEventData() const
    //{
    // return fEventData;
    //}
    /*! \brief Get the event size in bytes */
    uint32_t GetSize() const { return fEventSize; }
    /*!
     * \brief Get the bunch value
     * \return Bunch value
     */
    uint32_t GetBunch() const { return fBunch; }
    /*!
     * \brief Get the orbit value
     * \return Orbit value
     */
    uint32_t GetOrbit() const { return fOrbit; }
    /*!
     * \brief Get the luminence value
     * \return Luminence value
     */
    uint32_t GetLumi() const { return fLumi; }
    /*!
     * \brief Get the Event counter
     * \return Event counter
     */
    uint32_t GetEventCount() const { return fEventCount; }
    /*!
     * \brief Get TDC value ??
     * \return TDC value
     */
    uint32_t GetTDC() const { return fTDC; }
    /*!
     * \brief Get External trigger  id ??
     * \return external trigger value
     */
    uint32_t GetExternalTriggerId() const { return fExternalTriggerID; }
    /*!
     * \brief Get an event contained in a Cbc
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Event buffer
     */
    void GetCbcEvent(const uint8_t& pFeId, const uint8_t& pCbcId, std::vector<uint32_t>& cbcData) const;
    /*!
     * \brief Get an event contained in a Cbc
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Event buffer
     */
    void GetCbcEvent(const uint8_t& pFeId, const uint8_t& pCbcId, std::vector<uint8_t>& cbcData) const;
    /*!
     * \brief Function to get the bit at the global data string position
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param pPosition : Position in the data buffer
     * \return Bit
     */
    bool Bit(uint8_t pFeId, uint8_t pCbcId, uint32_t pPosition) const;
    /*!
     * \brief Function to get bit string from the data offset and width
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param pOffset : position Offset
     * \param pWidth : string width
     * \return Bit string
     */
    std::string BitString(uint8_t pFeId, uint8_t pCbcId, uint32_t pOffset, uint32_t pWidth) const;
    /*!
     * \brief Function to get bit vector from the data offset and width
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param pOffset : position Offset
     * \param pWidth : string width
     * \return Boolean/Bit vector
     */
    std::vector<bool> BitVector(uint8_t pFeId, uint8_t pCbcId, uint32_t pOffset, uint32_t pWidth) const;
    /*!
     * \brief Function to get char at the global data string at position 8*i
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param pBytePosition : Position of the byte
     * \return Char in given position
     */
    unsigned char Char(uint8_t pFeId, uint8_t pCbcId, uint32_t pBytePosition);

    const EventDataMap& GetEventDataMap() const { return fEventDataMap; }

    bool operator==(const Event& pEvent) const;

    ///////////////////////////////////////
    // VIRTUAL METHODS                    //
    ///////////////////////////////////////
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    virtual void Set(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& list) {}
    /*!
     * \brief Set an Event to the Event map
     * \param pEvent : Event to set
     * \return Aknowledgement of the Event setting (1/0)
     */
    virtual void SetEvent(const Ph2_HwDescription::BeBoard* pBoard, uint32_t pNbCbc, const std::vector<uint32_t>& list) {}
    /*!
     * \brief Convert Data to Hex string
     * \return Data string in hex
     */
    virtual std::string HexString() const { return ""; }

    // user interface
    /*!
     * \brief Get the Cbc Event counter
     * \return Cbc Event counter
     */
    virtual uint32_t GetEventCountCBC() const { return 0; }
    /*!
     * \brief Function to get bit string in hexadecimal format for CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string in Hex
     */
    virtual std::string DataHexString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }
    /*!
     * \brief Function to get bit string of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit string
     */
    virtual std::string DataBitString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }
    /*!
     * \brief Function to get bit vector of CBC data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Data Bit vector
     */
    virtual std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId) const { return {}; }
    /*!
     * \brief Function to get Error bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : Error bit number i
     * \return Error bit
     */
    virtual bool Error(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const { return false; }
    /*!
     * \brief Function to get all Error bits
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Error bit
     */
    virtual uint32_t Error(uint8_t pFeId, uint8_t pCbcId) const { return 0; }
    /*!
     * \brief Function to get pipeline address
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Pipeline address
     */
    virtual uint32_t PipelineAddress(uint8_t pFeId, uint8_t pCbcId) const { return 0; }
    /*!
     * \brief Function to get pipeline address
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Pipeline address
     */
    virtual uint32_t L1Id(uint8_t pFeId, uint8_t pCbcId) const { return 0; }

    /*!
     * \brief Function to get pipeline address
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Pipeline address
     */
    virtual uint32_t BxId(uint8_t pFeId) const { return 0; }
    /*!
     * \brief Function to get a CBC pixel bit data
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \param i : pixel bit data number i
     * \return Data Bit
     */
    virtual bool              DataBit(uint8_t pFeId, uint8_t pCbcId, uint32_t i) const { return true; }
    virtual std::vector<bool> DataBitVector(uint8_t pFeId, uint8_t pCbcId, const std::vector<uint8_t>& channelList) const { return {}; }
    /*!
     * \brief Function to get GLIB flag string
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return Glib flag string
     */
    virtual std::string GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    virtual std::string StubBitString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }
    /*!
     * \brief Function to get Stub bit
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return stub bit?
     */
    virtual bool StubBit(uint8_t pFeId, uint8_t pCbcId) const { return true; }
    /*!
     * \brief Get a vector of Stubs - will be empty for Cbc2
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     */
    virtual std::vector<Stub> StubVector(uint8_t pFeId, uint8_t pCbcId) const { return {}; }

    /*!
     * \brief Function to count the Hits in this event
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return number of hits
     */
    virtual uint32_t GetNHits(uint8_t pFeId, uint8_t pCbcId) const { return 0; }
    /*!
     * \brief Function to get a sparsified hit vector
     * \param pFeId : FE Id
     * \param pCbcId : Cbc Id
     * \return vector with hit channels
     */
    virtual std::vector<uint32_t> GetHits(uint8_t pFeId, uint8_t pCbcId) const { return {}; }
    /*!
     * \brief Function to get an encoded SLinkEvent object
     * \param pBoard : pointer to Ph2_HwDescription::BeBoard
     * \param pSet : set of condition data parsed from config file
     * \return SlinkEvent object
     */
    virtual SLinkEvent GetSLinkEvent(Ph2_HwDescription::BeBoard* pBoard) const
    {
        SLinkEvent e;
        return e;
    }

    friend std::ostream& operator<<(std::ostream& out, const Event& ev)
    {
        ev.print(out);
        return out;
    }

    virtual std::vector<Cluster> getClusters(uint8_t pFeId, uint8_t pCbcId) const { return {}; }

    virtual void fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup) = 0;

    // split stream of data
    template <std::size_t N>
    void splitStream(const std::vector<uint32_t> pData, std::vector<std::bitset<N>>& pBitSet, size_t pOffset, size_t pSize)
    {
        uint32_t cBitCounter  = 0;
        uint32_t cId          = 0;
        auto     cIterator    = pData.begin() + pOffset;
        size_t   cWordCounter = 0;
        do
        {
            auto cWord = std::bitset<32>(*cIterator);
            LOG(DEBUG) << BOLDBLUE << "Word " << +cWordCounter << " : " << cWord << RESET;
            for(size_t cIndex = 0; cIndex < 32; cIndex++)
            {
                if(cId >= pSize) continue;

                pBitSet[cId][N - 1 - cBitCounter] = cWord[31 - cIndex];
                // LOG (INFO) << "\t..Bit index " << +(31  - cIndex) << " bit counter in hit word at index " <<
                // +cBitCounter << RESET;
                cId += (cBitCounter == (N - 1));
                cBitCounter = (cBitCounter + 1) % N;
            }
            cIterator++;
            cWordCounter++;
        } while(cIterator < pData.end() && cId < pSize);
    }

  protected:
    virtual void print(std::ostream& out) const {}
};
} // namespace Ph2_HwInterface
#endif
