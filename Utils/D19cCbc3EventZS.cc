/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "Utils/D19cCbc3EventZS.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/DataContainer.h"
#include "Utils/Occupancy.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
void D19cCbc3EventZS::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint16_t hybridId)
{
    unsigned int i = 0;
    for(ChannelDataContainer<Occupancy>::iterator channel = chipContainer->begin<Occupancy>(); channel != chipContainer->end<Occupancy>(); channel++, i++)
    {
        if(testChannelGroup->isChannelEnabled(i)) { channel->fOccupancy += (float)DataBit(hybridId, chipContainer->getId(), i); }
    }
}

// Event implementation
D19cCbc3EventZS::D19cCbc3EventZS(const BeBoard* pBoard, uint32_t pZSEventSize, const std::vector<uint32_t>& list) { SetEvent(pBoard, pZSEventSize, list); }

void D19cCbc3EventZS::SetEvent(const BeBoard* pBoard, uint32_t pZSEventSize, const std::vector<uint32_t>& list)
{
    // LOG(INFO) << "event size: " << +pZSEventSize;

    // these two values come from width of the hybrid/cbc enabled mask
    uint8_t fMaxHybrids = 8;

    fEventSize = 0x0000FFFF & list.at(0);
    fEventSize *= 4; // block size is in 128 bit words
    // LOG (INFO) << "Block size: " << fEventSize;

    if(fEventSize != list.size() || fEventSize != pZSEventSize) LOG(ERROR) << "Vector size doesnt match the BLOCK_SIZE in Header1";

    uint8_t header1_size = (0xFF000000 & list.at(0)) >> 24;

    if(header1_size != D19C_EVENT_HEADER1_SIZE_32_CBC3) LOG(ERROR) << "Misaligned data: Header1 size doesnt correspond to the one sent from firmware";

    fNHybrid_software = static_cast<uint8_t>(pBoard->getNHybrid());
    fNHybrid_event    = 0;
    fFeMask_software  = 0;
    fFeMask_event     = static_cast<uint8_t>((0x00FF0000 & list.at(0)) >> 16);

    for(uint8_t bit = 0; bit < fMaxHybrids; bit++)
    {
        if((fFeMask_event >> bit) & 1) fNHybrid_event++;
    }

    for(uint8_t cHybrid = 0; cHybrid < fNHybrid_software; cHybrid++)
    {
        uint8_t cHybridId = pBoard->getFirstObject()->getObject(cHybrid)->getId();
        fFeMask_software |= 1 << cHybridId;
    }

    fEventCount = 0x00FFFFFF & list.at(2);
    fBunch      = 0xFFFFFFFF & list.at(3);
    fTDC        = 0x000000FF & list.at(4);

    fBeId          = pBoard->getId();
    fBeFWType      = 0;
    fCBCDataType   = (0x0000FF00 & list.at(1)) >> 8;
    fBeStatus      = 0;
    fNCbc          = 0;
    fEventDataSize = fEventSize;

    // now iterate through hybrids
    uint32_t address_offset = D19C_EVENT_HEADER1_SIZE_32_CBC3;

    for(uint8_t cHybrid = 0; cHybrid < fNHybrid_software; cHybrid++)
    {
        uint8_t cHybridId = pBoard->getFirstObject()->getObject(cHybrid)->getId();
        if(((fFeMask_software >> cHybridId) & 1) && ((fFeMask_event >> cHybridId) & 1))
        {
            // uint8_t chip_data_mask = static_cast<uint8_t> ( ( (0xFF000000) & list.at (address_offset + 0) ) >> 24);
            // LOG(INFO) << "Chip data mask: " << std::hex << +chip_data_mask << std::dec;

            uint16_t fe_data_size = static_cast<uint16_t>(((0x0000FFFF) & list.at(address_offset + 0)) >> 0);
            // LOG(INFO) << "Hybrid Data Size: " << +fe_data_size;

            uint8_t  cChipIDPrev = 255; // to be sure that we are starting from new chip everytime
            uint32_t word_id     = address_offset;

            while(word_id < (address_offset + (fe_data_size - 1)))
            {
                uint8_t cChipID = (0xE0000000 & list.at(word_id)) >> 29;
                // LOG(INFO) << "ChipId: " << +cChipID << ", Word ID: " << +word_id;

                if(cChipID != cChipIDPrev)
                {
                    // means that this word is a header
                    uint32_t cNclusters       = (0x1FC00000 & list.at(word_id)) >> 22;
                    uint32_t cNstripDataWords = (cNclusters + cNclusters % 2) / 2;
                    // LOG(INFO) << "N Clusters for chip " << +cChipID << " is " << cNclusters << " (" << std::hex <<
                    // list.at(word_id) << std::dec << ")";
                    uint32_t cChipDataSize = 1 + cNstripDataWords + 2; // 1 for header, 2 for stubs

                    // assign the data to the map
                    uint16_t              cKey = encodeId(cHybridId, cChipID);
                    std::vector<uint32_t> cCbcDataZS(std::next(std::begin(list), word_id), std::next(std::begin(list), word_id + cChipDataSize));
                    fEventDataMap[cKey] = cCbcDataZS;
                    word_id += cChipDataSize;

                    // check the data for sync and cluster overflow
                    uint8_t cSyncBit = (0x00000008 & cCbcDataZS.at(cCbcDataZS.size() - 1)) >> 3;
                    if(!cSyncBit) LOG(INFO) << BOLDRED << "Warning, sync bit not 1, data frame probably misaligned!" << RESET;
                    int8_t cClusterOverflow = (0x00000004 & cCbcDataZS.at(0)) >> 2;
                    if(cClusterOverflow) LOG(INFO) << BOLDRED << "Warning, cluster overflow happened!" << RESET;

                    // now update the chip id
                    cChipIDPrev = cChipID;
                }
                else
                {
                    LOG(ERROR) << "Error, the same chip id - should never happen";
                    exit(1);
                }
            }

            address_offset = address_offset + (fe_data_size - 1);
        }
    }
}

std::string D19cCbc3EventZS::HexString() const { return ""; }

std::string D19cCbc3EventZS::DataHexString(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::stringbuf tmp;
    std::ostream   os(&tmp);
    std::ios       oldState(nullptr);
    oldState.copyfmt(os);
    os << std::hex << std::setfill('0');

    // get the CBC event for pHybridId and pCbcId into vector<32bit> cbcData
    std::vector<uint32_t> cbcData;
    GetCbcEvent(pHybridId, pCbcId, cbcData);

    // trigdata
    os << std::endl;

    for(auto word: cbcData) os << std::setw(8) << word << std::endl;

    os.copyfmt(oldState);

    return tmp.str();
}

bool D19cCbc3EventZS::Error(uint8_t pHybridId, uint8_t pCbcId, uint32_t i) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // get word
        uint32_t word = cData->second.at(0);

        // buf overflow and lat error
        if(i == 0) return ((0x00000001 & word) >> 0); // lat err
        if(i == 1) return ((0x00000002 & word) >> 1); // buf ovf

        return true;
    }
    else
    {
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
        return true;
    }
}

uint32_t D19cCbc3EventZS::Error(uint8_t pHybridId, uint8_t pCbcId) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // get word
        uint32_t word = cData->second.at(0);
        // buf overflow and lat error
        uint32_t cError = (0x00000003 & word) >> 0;
        return cError;
    }
    else
    {
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
        return 0;
    }
}

uint32_t D19cCbc3EventZS::PipelineAddress(uint8_t pHybridId, uint8_t pCbcId) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // get word
        uint32_t word = cData->second.at(0);
        // pipe address
        uint32_t cPipelineAddress = (0x00001ff0 & word) >> 4;
        return cPipelineAddress;
    }
    else
    {
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
        return 0;
    }
}

bool D19cCbc3EventZS::DataBit(uint8_t pHybridId, uint8_t pCbcId, uint32_t i) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    // check parity
                    if((cClusterAddress - i) % 2 == 0)
                    {
                        // check if it's in cluster
                        if((i >= cClusterAddress) && (i < (cClusterAddress + 2u * cClusterWidth))) return 1;
                    }

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    // check parity
                    if((cClusterAddress - i) % 2 == 0)
                    {
                        // check if it's in cluster
                        if((i >= cClusterAddress) && (i < (cClusterAddress + 2u * cClusterWidth))) return 1;
                    }

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }
        // not found
        return 0;
    }
    else
    {
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
        return 0;
    }
}

std::string D19cCbc3EventZS::DataBitString(uint8_t pHybridId, uint8_t pCbcId) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // i didn't find way better than this one, because of the fact that clusters separated for sensors, and also
        // randomly stored in the event
        bool* hit_bits = new bool[NCHANNELS];
        for(uint32_t i = 0; i < NCHANNELS; i++) hit_bits[i] = false;

        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) hit_bits[cClusterAddress + 2 * i] = true;

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) hit_bits[cClusterAddress + 2 * i] = true;

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }

        std::ostringstream os;
        for(uint32_t i = 0; i < NCHANNELS; i++) os << hit_bits[i];
        delete[] hit_bits;

        return os.str();
    }
    else
    {
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
        return "";
    }
}

std::vector<bool> D19cCbc3EventZS::DataBitVector(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::vector<bool>            blist;
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // i didn't find way better than this one, because of the fact that clusters separated for sensors, and also
        // randomly stored in the event
        bool* hit_bits = new bool[NCHANNELS];
        for(uint32_t i = 0; i < NCHANNELS; i++) hit_bits[i] = false;

        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) hit_bits[cClusterAddress + 2 * i] = true;

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) hit_bits[cClusterAddress + 2 * i] = true;

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }

        for(uint32_t i = 0; i < NCHANNELS; i++) blist.push_back(hit_bits[i]);
        delete[] hit_bits;
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return blist;
}

std::vector<bool> D19cCbc3EventZS::DataBitVector(uint8_t pHybridId, uint8_t pCbcId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool>            blist;
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        std::vector<uint32_t> cHitsVector;

        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) cHitsVector.push_back(cClusterAddress + 2 * i);

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) cHitsVector.push_back(cClusterAddress + 2 * i);

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }

        // checks if channel is in the hits vector.
        for(auto cChannel: channelList) blist.push_back(std::find(cHitsVector.begin(), cHitsVector.end(), cChannel) != cHitsVector.end());
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return blist;
}

std::string D19cCbc3EventZS::GlibFlagString(uint8_t pHybridId, uint8_t pCbcId) const { return ""; }

std::string D19cCbc3EventZS::StubBitString(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::ostringstream os;

    std::vector<Stub> cStubVector = this->StubVector(pHybridId, pCbcId);

    for(auto cStub: cStubVector) os << std::bitset<8>(cStub.getPosition()) << " " << std::bitset<4>(cStub.getBend()) << " ";

    return os.str();

    // return BitString ( pHybridId, pCbcId, OFFSET_CBCSTUBDATA, WIDTH_CBCSTUBDATA );
}

bool D19cCbc3EventZS::StubBit(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::vector<Stub> cStubVector = this->StubVector(pHybridId, pCbcId);
    return !cStubVector.empty();
}

std::vector<Stub> D19cCbc3EventZS::StubVector(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::vector<Stub> cStubVec;
    // here create stubs and return the vector
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        uint8_t pos1 = (cData->second.at(cData->second.size() - 2) & 0x000000FF);
        uint8_t pos2 = (cData->second.at(cData->second.size() - 2) & 0x0000FF00) >> 8;
        uint8_t pos3 = (cData->second.at(cData->second.size() - 2) & 0x00FF0000) >> 16;
        // LOG (DEBUG) << std::bitset<8> (pos1);
        // LOG (DEBUG) << std::bitset<8> (pos2);
        // LOG (DEBUG) << std::bitset<8> (pos3);
        uint8_t bend1 = (cData->second.at(cData->second.size() - 1) & 0x00000F00) >> 8;
        uint8_t bend2 = (cData->second.at(cData->second.size() - 1) & 0x000F0000) >> 16;
        uint8_t bend3 = (cData->second.at(cData->second.size() - 1) & 0x0F000000) >> 24;

        if(pos1 != 0) cStubVec.emplace_back(pos1, bend1);
        if(pos2 != 0) cStubVec.emplace_back(pos2, bend2);
        if(pos3 != 0) cStubVec.emplace_back(pos3, bend3);
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return cStubVec;
}

uint32_t D19cCbc3EventZS::GetNHits(uint8_t pHybridId, uint8_t pCbcId) const
{
    uint32_t                     cNHits = 0;
    uint16_t                     cKey   = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData  = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    // uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth = ((0x00000007 & word) >> 0) + 1;

                    cNHits += cClusterWidth;

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    // uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth = ((0x00070000 & word) >> 16) + 1;

                    cNHits += cClusterWidth;

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return cNHits;
}

std::vector<uint32_t> D19cCbc3EventZS::GetHits(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::vector<uint32_t>        cHits;
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) cHits.push_back(cClusterAddress + 2 * i);

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    for(uint8_t i = 0; i < cClusterWidth; i++) cHits.push_back(cClusterAddress + 2 * i);

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return cHits;
}

void D19cCbc3EventZS::printCbcHeader(std::ostream& os, uint8_t pHybridId, uint8_t pCbcId) const
{
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        uint8_t  cBeId        = 0;
        uint8_t  cHybridId    = pHybridId;
        uint8_t  cCbcId       = pCbcId;
        uint16_t cCbcDataSize = 0xFF;
        os << GREEN << "CBC Header:" << std::endl;
        os << "BeId: " << +cBeId << " HybridId: " << +cHybridId << " CbcId: " << +cCbcId << " DataSize: " << cCbcDataSize << RESET << std::endl;
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";
}

void D19cCbc3EventZS::print(std::ostream& os) const
{
    os << BOLDGREEN << "EventType: d19c CBC3" << RESET << std::endl;
    os << BOLDBLUE << "L1A Counter: " << this->GetEventCount() << RESET << std::endl;
    os << "          Be Id: " << +this->getBeBoardId() << std::endl;
    // os << "          Be FW: " << +this->GetFWType() << std::endl;
    // os << "      Be Status: " << +this->GetBeStatus() << std::endl;
    // os << "  Cbc Data type: " << +this->GetCbcDataType() << std::endl;
    // os << "          N Cbc: " << +this->GetNCbc() << std::endl;
    os << "Event Data size: " << +this->GetEventDataSize() << std::endl;
    // os << "  CBC Counter: " << this->GetEventCountCBC() << RESET << std::endl;
    os << "Bunch Counter: " << this->GetBunch() << std::endl;
    // os << "Orbit Counter: " << this->GetOrbit() << std::endl;
    // os << " Lumi Section: " << this->GetLumi() << std::endl;
    os << BOLDRED << "    TDC Counter: " << +this->GetTDC() << RESET << std::endl;

    const int FIRST_LINE_WIDTH = 22;
    const int LINE_WIDTH       = 32;
    const int LAST_LINE_WIDTH  = 8;

    for(auto const& cKey: this->fEventDataMap)
    {
        uint8_t cHybridId;
        uint8_t cCbcId;
        this->decodeId(cKey.first, cHybridId, cCbcId);

        // here display the Cbc Header manually
        this->printCbcHeader(os, cHybridId, cCbcId);

        // here print a list of stubs
        uint8_t cCounter = 1;

        if(this->StubBit(cHybridId, cCbcId))
        {
            os << BOLDCYAN << "List of Stubs: " << RESET << std::endl;

            for(auto& cStub: this->StubVector(cHybridId, cCbcId))
            {
                os << CYAN << "Stub: " << +cCounter << " Position: " << +cStub.getPosition() << " Bend: " << +cStub.getBend() << " Strip: " << cStub.getCenter() << RESET << std::endl;
                cCounter++;
            }
        }

        // here list other bits in the stub stream

        std::string data(this->DataBitString(cHybridId, cCbcId));
        os << GREEN << "HybridId = " << +cHybridId << " CBCId = " << +cCbcId << RESET << " len(data) = " << data.size() << std::endl;
        os << YELLOW << "PipelineAddress: " << this->PipelineAddress(cHybridId, cCbcId) << RESET << std::endl;
        os << RED << "Error: " << static_cast<std::bitset<2>>(this->Error(cHybridId, cCbcId)) << RESET << std::endl;
        os << CYAN << "Total number of hits: " << this->GetNHits(cHybridId, cCbcId) << RESET << std::endl;
        os << BLUE << "List of hits: " << RESET << std::endl;
        std::vector<uint32_t> cHits = this->GetHits(cHybridId, cCbcId);

        if(cHits.size() == 254)
            os << "All channels firing!" << std::endl;
        else
        {
            int cCounter = 0;

            for(auto& cHit: cHits)
            {
                os << std::setw(3) << cHit << " ";
                cCounter++;

                if(cCounter == 10)
                {
                    os << std::endl;
                    cCounter = 0;
                }
            }

            os << RESET << std::endl;
        }

        os << "Ch. Data:      ";

        for(int i = 0; i < FIRST_LINE_WIDTH; i += 2) os << data.substr(i, 2) << " ";

        os << std::endl;

        for(int i = 0; i < 7; ++i)
        {
            for(int j = 0; j < LINE_WIDTH; j += 2)
                // os << data.substr ( FIRST_LINE_WIDTH + LINE_WIDTH * i, LINE_WIDTH ) << std::endl;
                os << data.substr(FIRST_LINE_WIDTH + LINE_WIDTH * i + j, 2) << " ";

            os << std::endl;
        }

        for(int i = 0; i < LAST_LINE_WIDTH; i += 2) os << data.substr(FIRST_LINE_WIDTH + LINE_WIDTH * 7 + i, 2) << " ";

        os << std::endl;

        os << BLUE << "Stubs: " << this->StubBitString(cHybridId, cCbcId).c_str() << RESET << std::endl;
    }

    os << std::endl;
}

std::vector<Cluster> D19cCbc3EventZS::getClusters(uint8_t pHybridId, uint8_t pCbcId) const
{
    std::vector<Cluster>         cClusterVec;
    uint16_t                     cKey  = encodeId(pHybridId, pCbcId);
    EventDataMap::const_iterator cData = fEventDataMap.find(cKey);

    if(cData != std::end(fEventDataMap))
    {
        // data vector
        std::vector<uint32_t> cDataVector = cData->second;
        // nclusters
        uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
        uint32_t cGotClusters = 0;
        if(cNclusters > 0)
        {
            for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
            {
                uint32_t word = cDataVector.at(word_id);

                if(cGotClusters < cNclusters)
                {
                    // low part of the word
                    uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                    uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                    Cluster aCluster;
                    aCluster.fSensor       = cClusterAddress % 2;
                    aCluster.fFirstStrip   = cClusterAddress;
                    aCluster.fClusterWidth = cClusterWidth;
                    cClusterVec.push_back(aCluster);

                    // increment got clusters
                    cGotClusters++;
                }

                if(cGotClusters < cNclusters)
                {
                    // top part of the word
                    uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                    uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                    Cluster aCluster;
                    aCluster.fSensor       = cClusterAddress % 2;
                    aCluster.fFirstStrip   = cClusterAddress;
                    aCluster.fClusterWidth = cClusterWidth;
                    cClusterVec.push_back(aCluster);

                    // increment got clusters
                    cGotClusters++;
                }

                // break if got all clusters
                if(cGotClusters >= cNclusters) break;
            }
        }
    }
    else
        LOG(INFO) << "Event: Hybrid " << +pHybridId << " CBC " << +pCbcId << " is not found.";

    return cClusterVec;
}

SLinkEvent D19cCbc3EventZS::GetSLinkEvent(BeBoard* pBoard) const
{
    uint16_t          cCbcCounter = 0;
    std::set<uint8_t> cEnabledHybrids;

    // payload for the status bits
    GenericPayload cStatusPayload;
    // for the payload and the stubs
    GenericPayload cPayload;
    GenericPayload cStubPayload;

    for(auto cHybrid: *pBoard->getFirstObject())
    {
        uint8_t cHybridId = cHybrid->getId();

        // firt get the list of enabled front ends
        if(cEnabledHybrids.find(cHybridId) == std::end(cEnabledHybrids)) cEnabledHybrids.insert(cHybridId);

        // now on to the payload
        int cFirstBitFePayload = cPayload.get_current_write_position();
        int cFirstBitFeStub    = cStubPayload.get_current_write_position();
        // cluster counter per Hybrid
        uint8_t cHybridCluCounter = 0;
        // stub counter per Hybrid
        uint8_t cHybridStubCounter = 0;

        // in ZS mode and FULL DEBUG mode, we have one error bit per CBC and 9 bits of L1A counter per CIC
        // the way I am going to implement this here is for each Hybrid I put 9 bits of L1A Ctr followed by 1 Error bit per
        // CBC
        // TODO
        cStatusPayload.append(this->GetEventCount(), 9);

        for(auto cCbc: *cHybrid)
        {
            uint8_t                      cCbcId = cCbc->getId();
            uint16_t                     cKey   = encodeId(cHybridId, cCbcId);
            EventDataMap::const_iterator cData  = fEventDataMap.find(cKey);

            if(cData != std::end(fEventDataMap))
            {
                // init
                bool cStatusBit = false;

                // data vector
                std::vector<uint32_t> cDataVector = cData->second;

                // !!! put clusters
                // nclusters
                uint32_t cNclusters   = (0x1FC00000 & cDataVector.at(0)) >> 22;
                uint32_t cGotClusters = 0;
                if(cNclusters > 0)
                {
                    for(size_t word_id = 1; word_id < cDataVector.size(); word_id++)
                    {
                        uint32_t word = cDataVector.at(word_id);

                        if(cGotClusters < cNclusters)
                        {
                            // low part of the word
                            uint8_t cClusterAddress = (0x000007f8 & word) >> 3;
                            uint8_t cClusterWidth   = ((0x00000007 & word) >> 0) + 1;

                            // i don't do cClusterWidth-1 here because from the fw it comes in a proper way
                            cPayload.append(uint16_t((cCbcId & 0x0F) << 11 | cClusterAddress << 3 | ((cClusterWidth)&0x07)));
                            cHybridCluCounter++;

                            // increment got clusters
                            cGotClusters++;
                        }

                        if(cGotClusters < cNclusters)
                        {
                            // top part of the word
                            uint8_t cClusterAddress = (0x07f80000 & word) >> 19;
                            uint8_t cClusterWidth   = ((0x00070000 & word) >> 16) + 1;

                            // i don't do cClusterWidth-1 here because from the fw it comes in a proper way
                            cPayload.append(uint16_t((cCbcId & 0x0F) << 11 | cClusterAddress << 3 | ((cClusterWidth)&0x07)));
                            cHybridCluCounter++;

                            // increment got clusters
                            cGotClusters++;
                        }

                        // break if got all clusters
                        if(cGotClusters >= cNclusters) break;
                    }
                }

                // !!! stubs
                uint8_t pos1  = (cDataVector.at(cDataVector.size() - 2) & 0x000000FF);
                uint8_t pos2  = (cDataVector.at(cDataVector.size() - 2) & 0x0000FF00) >> 8;
                uint8_t pos3  = (cDataVector.at(cDataVector.size() - 2) & 0x00FF0000) >> 16;
                uint8_t bend1 = (cDataVector.at(cDataVector.size() - 1) & 0x00000F00) >> 8;
                uint8_t bend2 = (cDataVector.at(cDataVector.size() - 1) & 0x000F0000) >> 16;
                uint8_t bend3 = (cDataVector.at(cDataVector.size() - 1) & 0x0F000000) >> 24;

                if(pos1 != 0)
                {
                    cStubPayload.append(uint16_t((cCbcId & 0x0F) << 12 | pos1 << 4 | (bend1 & 0xF)));
                    cHybridStubCounter++;
                }

                if(pos2 != 0)
                {
                    cStubPayload.append(uint16_t((cCbcId & 0x0F) << 12 | pos2 << 4 | (bend2 & 0xF)));
                    cHybridStubCounter++;
                }

                if(pos3 != 0)
                {
                    cStubPayload.append(uint16_t((cCbcId & 0x0F) << 12 | pos3 << 4 | (bend3 & 0xF)));
                    cHybridStubCounter++;
                }

                // !!! error
                // we are in sparsified mode so in full debug we get 1 error bit per chip + 1 L1A counter per CIC
                cStatusBit = ((cDataVector.at(0) & 0x00000003) != 0);

                // just append a status bit to the status payload, since the format is the same for full and Error mode
                // for the moment so if we are in summary mode, we just don't insert the staus payload at all a bit
                // further down
                cStatusPayload.append(cStatusBit);
            }

            cCbcCounter++;
        } // end of CBC loop

        // for the payload for this Hybrid, I need to prepend a status bit(6) and 6 bit Cluster counter
        cPayload.insert((cHybridCluCounter & 0x3F), cFirstBitFePayload, 7);

        // for the stubs for this Hybrid, I need to prepend a 5 bit counter shifted by 1 to the right (to account for the 0
        // bit)
        cStubPayload.insert((cHybridStubCounter & 0x1F) << 1, cFirstBitFeStub, 6);

    } // end of Hybrid loop

    uint32_t   cEvtCount = this->GetEventCount();
    uint16_t   cBunch    = static_cast<uint16_t>(this->GetBunch());
    uint32_t   cBeStatus = this->fBeStatus;
    SLinkEvent cEvent(EventType::ZS, pBoard->getConditionDataSet()->getDebugMode(), FrontEndType::CBC3, cEvtCount, cBunch, SOURCE_ID);
    cEvent.generateTkHeader(cBeStatus, cCbcCounter, cEnabledHybrids, pBoard->getConditionDataSet()->getCondDataEnabled(),
                            false); // Be Status, total number CBC, condition data?, fake data?

    // generate a vector of uint64_t with the chip status
    if(pBoard->getConditionDataSet()->getDebugMode() != SLinkDebugMode::SUMMARY) // do nothing
        cEvent.generateStatus(cStatusPayload.Data<uint64_t>());

    // PAYLOAD
    cEvent.generatePayload(cPayload.Data<uint64_t>());

    // STUBS
    cEvent.generateStubs(cStubPayload.Data<uint64_t>());

    // condition data, first update the values in the vector for I2C values
    uint32_t cTDC = this->GetTDC();
    pBoard->updateCondData(cTDC);
    cEvent.generateConditionData(pBoard->getConditionDataSet());

    cEvent.generateDAQTrailer();

    return cEvent;
}
} // namespace Ph2_HwInterface
