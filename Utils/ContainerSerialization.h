#ifndef __CONTAINER_SERIALIZATION__
#define __CONTAINER_SERIALIZATION__

#include "HWDescription/Definition.h"
#include "HWDescription/RD53A.h"
#include "HWDescription/RD53B.h"
#include "NetworkUtils/TCPPublishServer.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/DataContainer.h"
#include "Utils/RD53Shared.h"
#include <boost/serialization/export.hpp>
#include <boost/utility/identity_type.hpp>
#include <boost/serialization/string.hpp>

class Occupancy;
class OccupancyAndPh;
class ThresholdAndNoise;
class EmptyContainer;
template <size_t size, typename T>
class GenericDataArray;
template <size_t size1, size_t size2, typename T>
class GenericDataArray_2D;
class GainFit;
class GenericDataVector;
template <typename T>
class ValueAndTime;

BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<uint8_t>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<uint32_t>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<float>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<Occupancy>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<OccupancyAndPh>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<GainFit>)))

BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<Occupancy, Occupancy>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<OccupancyAndPh, OccupancyAndPh>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataVector, OccupancyAndPh>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, Occupancy>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GainFit, GainFit>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, float>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, ValueAndTime<uint16_t>>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<ValueAndTime<uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, ValueAndTime<float>>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<ValueAndTime<float>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, std::pair<uint16_t, uint16_t>>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, double>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<ThresholdAndNoise, ThresholdAndNoise>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<uint16_t, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, uint8_t>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, uint16_t>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<EmptyContainer, std::string>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<std::string, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<std::string, std::string>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<uint32_t, uint32_t>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<VECSIZE, uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<TDCBINS, uint16_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<VECSIZE, GenericDataArray<VECSIZE, uint16_t>>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<NCHANNELS + 1, uint32_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>, GenericDataArray<NCHANNELS + 1, uint32_t>>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<TOTAL_CHANNELS_OT + 1, uint32_t>, GenericDataArray<HYBRID_CHANNELS_OT + 1, uint32_t>>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray_2D<TOTAL_CHANNELS_OT, TOTAL_CHANNELS_OT, uint32_t>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1, float>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID) + 1, float>, EmptyContainer>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<GenericDataArray<RD53Shared::setBits(RD53AEvtEncoder::NBIT_TRIGID) + 1, float>, EmptyContainer>)))

#include "Utils/Occupancy.h"
#include <arpa/inet.h>
#include <iostream>

class PacketHeader
{
  public:
    PacketHeader()
    {
        for(uint8_t i = 0; i < SIZE; ++i) fPacketSize[i] = 0u;
    };
    ~PacketHeader(){};

    uint8_t getPacketHeaderSize() { return SIZE; }

    void addPacketHeader(std::string& thePacket)
    {
        uint64_t thePacketSize = thePacket.size() + SIZE;
        setPacketSize(thePacketSize);
        std::string packetString(&fPacketSize[0], SIZE);
        thePacket.insert(0, packetString);
    }

    uint32_t getPacketSize(std::string& thePacket)
    {
        for(uint8_t i = 0; i < SIZE; ++i) fPacketSize[i] = thePacket[i];
        return getPacketSize();
    }

    uint32_t getPacketSize(std::vector<char>& thePacket)
    {
        std::string theStringPacket(thePacket.begin(), thePacket.begin() + SIZE);
        return getPacketSize(theStringPacket);
    }

  private:
    static const uint8_t SIZE = 4;

    void setPacketSize(uint64_t packetSize)
    {
        uint64_t maximumSize = 1 << (SIZE * 8 - 1);
        if(packetSize >= maximumSize)
        {
            std::string outputMessage =
                std::string(__PRETTY_FUNCTION__) + " ERROR: requested packet sizes = " + std::to_string(packetSize) + " is >= than " + std::to_string(maximumSize) + " are not allowed";
            throw std::runtime_error(outputMessage);
        }
        uint32_t localPacketSize = htonl(packetSize);
        for(uint8_t i = 0u; i < SIZE; ++i) fPacketSize[i] = ((localPacketSize >> (8u * i)) & 0xff);
    }

    uint32_t getPacketSize()
    {
        uint32_t localPacketSize = 0;
        for(uint8_t i = 0; i < SIZE; ++i) localPacketSize += ((fPacketSize[i] & 0xff) << (8u * i));
        return htonl(localPacketSize);
    }

    char fPacketSize[SIZE];
};

template <uint N>
struct Serialize
{
    template <class Archive, typename... Args>
    static void serialize(Archive& theArchive, std::tuple<Args...>& theTuple)
    {
        theArchive& std::get<N - 1>(theTuple);
        Serialize<N - 1>::serialize(theArchive, theTuple);
    }
};

template <>
struct Serialize<0>
{
    template <class Archive, typename... Args>
    static void serialize(Archive& theArchive, std::tuple<Args...>& theTuple)
    {
        (void)theArchive;
        (void)theTuple;
    }
};

template <class Archive, typename... Args>
void serialize(Archive& theArchive, std::tuple<Args...>& theTuple)
{
    Serialize<sizeof...(Args)>::serialize(theArchive, theTuple);
}

class ContainerSerialization
{
  public:
    ContainerSerialization(const std::string& calibrationName);
    ~ContainerSerialization();

    bool attachDeserializer(std::string& inputBuffer);

    // !!! ---------------------------------------------------------------------------- !!! //
    // Stream
    // !!! ---------------------------------------------------------------------------- !!! //
    template <typename... Args>
    void streamByDetectorContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        std::string  myStream = serializeDetectorContainer(theInputContainer, extraArguments...);
        PacketHeader thePacketHeader;
        thePacketHeader.addPacketHeader(myStream);
        networkStreamer->broadcast(myStream);
    }

    template <typename... Args>
    void streamByBoardContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board: theInputContainer)
        {
            std::string  myStream = serializeBoardContainer(board, extraArguments...);
            PacketHeader thePacketHeader;
            thePacketHeader.addPacketHeader(myStream);
            networkStreamer->broadcast(myStream);
        }
    }

    template <typename... Args>
    void streamByOpticalGroupContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board: theInputContainer)
        {
            for(auto opticalGroup: *board)
            {
                std::string  myStream = serializeOpticalGroupContainer(opticalGroup, board->getId(), extraArguments...);
                PacketHeader thePacketHeader;
                thePacketHeader.addPacketHeader(myStream);
                networkStreamer->broadcast(myStream);
            }
        }
    }

    template <typename... Args>
    void streamByHybridContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board: theInputContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    std::string  myStream = serializeHybridContainer(hybrid, board->getId(), opticalGroup->getId(), extraArguments...);
                    PacketHeader thePacketHeader;
                    thePacketHeader.addPacketHeader(myStream);
                    networkStreamer->broadcast(myStream);
                }
            }
        }
    }

    template <typename... Args>
    void streamByChipContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board: theInputContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        std::string  myStream = serializeChipContainer(chip, board->getId(), opticalGroup->getId(), hybrid->getId(), extraArguments...);
                        PacketHeader thePacketHeader;
                        thePacketHeader.addPacketHeader(myStream);
                        networkStreamer->broadcast(myStream);
                    }
                }
            }
        }
    }

    // !!! ---------------------------------------------------------------------------- !!! //
    // Serialize
    // !!! ---------------------------------------------------------------------------- !!! //

    template <typename... Args>
    std::string serializeDetectorContainer(DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        std::ostringstream            ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;

        theArchive << fCalibrationName;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;
        theArchive << theInputContainer;
std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]" << std::endl;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template <typename... Args>
    std::string serializeBoardContainer(BoardDataContainer* theInputContainer, Args&... extraArguments) const
    {
        std::ostringstream            ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t                      id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template <typename... Args>
    std::string serializeOpticalGroupContainer(OpticalGroupDataContainer* theInputContainer, uint16_t boardId, Args&... extraArguments) const
    {
        std::ostringstream            ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t                      id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template <typename... Args>
    std::string serializeHybridContainer(HybridDataContainer* theInputContainer, uint16_t boardId, uint16_t opticalGroupId, Args&... extraArguments) const
    {
        std::ostringstream            ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t                      id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << opticalGroupId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template <typename... Args>
    std::string serializeChipContainer(ChipDataContainer* theInputContainer, uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, Args&... extraArguments) const
    {
        std::ostringstream            ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t                      id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << opticalGroupId;
        theArchive << hybridId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    // !!! ---------------------------------------------------------------------------- !!! //
    // Deserialize
    // !!! ---------------------------------------------------------------------------- !!! //

    template <typename T, typename SC, typename SH, typename SO, typename SB, typename SD, typename... Args>
    DetectorDataContainer deserializeDetectorContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyAndInitStructure<T, SC, SH, SO, SB, SD>(*theDetectorContainer, theOutputContainer);
        std::istringstream            inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string                   inputCalibrationName;
        theArchive >> inputCalibrationName;
        theArchive >> theOutputContainer;
        serializeExtraArguments(theArchive, extraArguments...);

        theOutputContainer.remapIdtoPointer();
        return theOutputContainer;
    }

    template <typename T, typename SC, typename SH, typename SO, typename SB, typename... Args>
    DetectorDataContainer deserializeBoardContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream            inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string                   inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId = 65535;
        theArchive >> boardId;

        BoardDataContainer& board = *theOutputContainer.getObject(boardId);
        board.initialize<SB, SO>();
        for(const auto opticalGroup: board)
        {
            opticalGroup->initialize<SO, SH>();
            for(const auto hybrid: *opticalGroup)
            {
                hybrid->initialize<SH, SC>();
                for(const auto chip: *hybrid) { chip->initialize<SC, T>(); }
            }
        }
        theArchive >> board;
        serializeExtraArguments(theArchive, extraArguments...);

        theOutputContainer.remapIdtoPointer();
        return theOutputContainer;
    }

    template <typename T, typename SC, typename SH, typename SO, typename... Args>
    DetectorDataContainer deserializeOpticalGroupContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream            inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string                   inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId        = 65535;
        uint16_t opticalGroupId = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;

        OpticalGroupDataContainer& opticalGroup = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId);
        opticalGroup.initialize<SO, SH>();
        for(const auto hybrid: opticalGroup)
        {
            hybrid->initialize<SH, SC>();
            for(const auto chip: *hybrid) { chip->initialize<SC, T>(); }
        }

        theArchive >> opticalGroup;
        serializeExtraArguments(theArchive, extraArguments...);

        theOutputContainer.remapIdtoPointer();
        return theOutputContainer;
    }

    template <typename T, typename SC, typename SH, typename... Args>
    DetectorDataContainer deserializeHybridContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream            inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string                   inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId        = 65535;
        uint16_t opticalGroupId = 65535;
        uint16_t hybridId       = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;
        theArchive >> hybridId;

        HybridDataContainer& hybrid = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId);
        hybrid.initialize<SH, SC>();
        for(auto chip: hybrid) { chip->initialize<SC, T>(); }

        theArchive >> hybrid;
        serializeExtraArguments(theArchive, extraArguments...);

        theOutputContainer.remapIdtoPointer();
        return theOutputContainer;
    }

    template <typename T, typename SC, typename... Args>
    DetectorDataContainer deserializeChipContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream            inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string                   inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId        = 65535;
        uint16_t opticalGroupId = 65535;
        uint16_t hybridId       = 65535;
        uint16_t chipId         = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;
        theArchive >> hybridId;
        theArchive >> chipId;

        ChipDataContainer& chip = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId);
        chip.initialize<SC, T>();

        theArchive >> chip;
        serializeExtraArguments(theArchive, extraArguments...);

        theOutputContainer.remapIdtoPointer();
        return theOutputContainer;
    }

    // template <typename T, typename... Args>
    // DetectorDataContainer deserializeChannelContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    // {
    //     DetectorDataContainer theOutputContainer;
    //     ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
    //     std::istringstream inputStream(fStream);
    //     boost::archive::text_iarchive theArchive(inputStream);
    //     std::string inputCalibrationName;
    //     theArchive >> inputCalibrationName;
    //     uint16_t boardId = 65535;
    //     uint16_t opticalGroupId = 65535;
    //     uint16_t hybridId = 65535;
    //     uint16_t chipId = 65535;
    //     theArchive >> boardId;
    //     theArchive >> opticalGroupId;
    //     theArchive >> hybridId;
    //     theArchive >> chipId;

    //     ChannelDataContainer* channel = theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId);
    //     chip->initializeChannels<T>();

    //     theArchive >> channel;
    //     serializeExtraArguments(theArchive, extraArguments...);
    //     return theOutputContainer;
    // }

  private:
    template <typename T, typename... Args>
    void serializeExtraArguments(T& theArchive, Args&... extraArguments) const
    {
        auto extraArgumentTuple = std::tuple<Args&...>(extraArguments...);
        serialize(theArchive, extraArgumentTuple);
    }

    std::string fCalibrationName;
    std::string fStream;
};

#endif