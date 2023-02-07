#ifndef __CONTAINER_SERIALIZATION__
#define __CONTAINER_SERIALIZATION__


#include <boost/utility/identity_type.hpp>
#include <boost/serialization/export.hpp>
#include "Utils/DataContainer.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "NetworkUtils/TCPPublishServer.h"

class Occupancy;
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((Summary<Occupancy,Occupancy>)))
BOOST_CLASS_EXPORT_KEY(BOOST_IDENTITY_TYPE((ChannelDataContainer<Occupancy>)))

#include "Utils/Occupancy.h"
#include <iostream>
#include <arpa/inet.h>

class PacketHeader
{
  public:
    PacketHeader(){};
    ~PacketHeader(){};

    uint8_t getPacketHeaderSize() {return SIZE;}

    void addPacketHeader(std::string& thePacket)
    {
        uint64_t thePacketSize = thePacket.size() + SIZE;
        setPacketSize(thePacketSize);
        std::string packetString(&fPacketSize[0], SIZE);
        thePacket.insert(0, packetString);
    }

    uint32_t getPacketSize(std::string& thePacket)
    {
        for(uint8_t i=0; i<SIZE; ++i) fPacketSize[i] = thePacket[i];
        return getPacketSize(); 
    }

    uint32_t getPacketSize(std::vector<char>& thePacket)
    {
        std::string theStringPacket(thePacket.begin(), thePacket.begin()+SIZE);
        return getPacketSize(theStringPacket);
    }


  private:
    static const uint8_t SIZE = 4;

    void setPacketSize(uint64_t packetSize)
    {
        uint64_t maximumSize = 1 << (SIZE*8-1);
        if(packetSize >= maximumSize)
        {
            std::string outputMessage = std::string(__PRETTY_FUNCTION__) + " ERROR: requested packet sizes = " + std::to_string(packetSize) + " is >= than " 
                                        + std::to_string(maximumSize) + " are not allowed";
            throw std::runtime_error(outputMessage);
        }
        uint32_t    localPacketSize = htonl(packetSize);
        for(uint8_t i=0; i<SIZE; ++i) fPacketSize[i] = (localPacketSize >> (8*i)) & 0xff;
    }

    uint32_t getPacketSize()
    {
        uint32_t localPacketSize = 0;
        for(uint8_t i=0; i<SIZE; ++i) localPacketSize += (fPacketSize[i] << (8*i));
        return htonl(localPacketSize);
    }


    char fPacketSize[SIZE];
};

template<uint N>
struct Serialize
{
    template<class Archive, typename... Args>
    static void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
    {
        theArchive & std::get<N-1>(theTuple);
        Serialize<N-1>::serialize(theArchive, theTuple);
    }
};

template<>
struct Serialize<0>
{
    template<class Archive, typename... Args>
    static void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
    {
        (void) theArchive;
        (void) theTuple;
    }
};

template<class Archive, typename... Args>
void serialize(Archive & theArchive, std::tuple<Args...> & theTuple)
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
    template<typename... Args>
    void streamByBoardContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board : theInputContainer)
        {
            std::string myStream = serializeBoardContainer(board, extraArguments...);
            PacketHeader thePacketHeader;
            thePacketHeader.addPacketHeader(myStream);
            networkStreamer->broadcast(myStream);
        }
    }

    template<typename... Args>
    void streamByOpticalGroupContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board : theInputContainer)
        {
            for(auto opticalGroup : *board)
            {
                std::string myStream = serializeOpticalGroupContainer(opticalGroup, board->getId(), extraArguments...);
                PacketHeader thePacketHeader;
                thePacketHeader.addPacketHeader(myStream);
                networkStreamer->broadcast(myStream);
            }
        }
    }

    template<typename... Args>
    void streamByHybridContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board : theInputContainer)
        {
            for(auto opticalGroup : *board)
            {
                for(auto hybrid : *opticalGroup)
                {
                    std::string myStream = serializeHybridContainer(hybrid, board->getId(), opticalGroup->getId(), extraArguments...);
                    PacketHeader thePacketHeader;
                    thePacketHeader.addPacketHeader(myStream);
                    networkStreamer->broadcast(myStream);
                }
            }
        }
    }

    template<typename... Args>
    void streamByChipContainer(TCPPublishServer* networkStreamer, DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        for(auto board : theInputContainer)
        {
            for(auto opticalGroup : *board)
            {
                for(auto hybrid : *opticalGroup)
                {
                    for(auto chip : *hybrid)
                    {
                        std::string myStream = serializeChipContainer(chip, board->getId(), opticalGroup->getId(), hybrid->getId(), extraArguments...);
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
    
    template<typename... Args>
    std::string serializeDetectorContainer(DetectorDataContainer& theInputContainer, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);

        theArchive << fCalibrationName;
        theArchive << theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template<typename... Args>
    std::string serializeBoardContainer(BoardDataContainer* theInputContainer, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template<typename... Args>
    std::string serializeOpticalGroupContainer(OpticalGroupDataContainer* theInputContainer, uint16_t boardId, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template<typename... Args>
    std::string serializeHybridContainer(HybridDataContainer* theInputContainer, uint16_t boardId, uint16_t opticalGroupId, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << opticalGroupId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    template<typename... Args>
    std::string serializeChipContainer(ChipDataContainer* theInputContainer, uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, Args&... extraArguments) const
    {
        std::ostringstream ouputStream;
        boost::archive::text_oarchive theArchive(ouputStream);
        uint16_t id = theInputContainer->getId();

        theArchive << fCalibrationName;
        theArchive << boardId;
        theArchive << opticalGroupId;
        theArchive << hybridId;
        theArchive << id;
        theArchive << *theInputContainer;

        serializeExtraArguments(theArchive, extraArguments...);
        return ouputStream.str();
    }

    // template<typename... Args>
    // std::string serializeChannelContainer(ChipDataContainer* theInputContainer, uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, Args&... extraArguments) const
    // {
    //     std::ostringstream ouputStream;
    //     boost::archive::text_oarchive theArchive(ouputStream);
    //     uint16_t id = theInputContainer->getId();

    //     theArchive << fCalibrationName;
    //     theArchive << boardId;
    //     theArchive << opticalGroupId;
    //     theArchive << hybridId;
    //     theArchive << id;
    //     theArchive << theInputContainer->getChannelContainer();

    //     serializeExtraArguments(theArchive, extraArguments...);
    //     return ouputStream.str();
    // }

    // !!! ---------------------------------------------------------------------------- !!! //
    // Deserialize
    // !!! ---------------------------------------------------------------------------- !!! //

    template <typename T, typename SC, typename SH, typename SO, typename SB, typename SD, typename... Args>
    DetectorDataContainer deserializeDetectorContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyAndInitStructure<T, SC, SH, SO, SB, SD>(*theDetectorContainer, theOutputContainer);
        std::istringstream inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
        theArchive >> inputCalibrationName;
        theArchive >> theOutputContainer;
        serializeExtraArguments(theArchive, extraArguments...);
        return theOutputContainer;
    }

    template <typename T, typename SC, typename SH, typename SO, typename SB, typename... Args>
    DetectorDataContainer deserializeBoardContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
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
                for(const auto chip: *hybrid)
                {
                    chip->initialize<SC, T>();
                }
            }
        }
        theArchive >> board;
        serializeExtraArguments(theArchive, extraArguments...);
        return theOutputContainer;
    }


    template <typename T, typename SC, typename SH, typename SO, typename... Args>
    DetectorDataContainer deserializeOpticalGroupContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId = 65535;
        uint16_t opticalGroupId = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;

        OpticalGroupDataContainer& opticalGroup = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId);
        opticalGroup.initialize<SO, SH>();
        for(const auto hybrid: opticalGroup)
        {
            hybrid->initialize<SH, SC>();
            for(const auto chip: *hybrid)
            {
                chip->initialize<SC, T>();
            }
        }

        theArchive >> opticalGroup;
        serializeExtraArguments(theArchive, extraArguments...);
        return theOutputContainer;
    }

    template <typename T, typename SC, typename SH, typename... Args>
    DetectorDataContainer deserializeHybridContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId = 65535;
        uint16_t opticalGroupId = 65535;
        uint16_t hybridId = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;
        theArchive >> hybridId;

        // HybridDataContainer& hybrid = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId);
        HybridDataContainer hybrid;
        // hybrid.initialize<SH, SC>();
        std::cout<<__PRETTY_FUNCTION__<<__LINE__ << " hybrid pointer " << &hybrid <<std::endl;
        // for(auto chip : hybrid)
        // {
        //     chip->initialize<SC, T>();
        //     std::cout<<__PRETTY_FUNCTION__<<__LINE__ << " chip pointer " << chip <<std::endl;
        // }
        std::cout << __PRETTY_FUNCTION__ << "number of chips before = " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->size()<< std::endl;

        theArchive >> hybrid;
        serializeExtraArguments(theArchive, extraArguments...);


        std::cout << __PRETTY_FUNCTION__ << "number of chips after = " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->size()<< std::endl;
        // std::cout << __PRETTY_FUNCTION__ << std::endl;
        // std::cout << __PRETTY_FUNCTION__ << std::endl;
        // std::cout << boardId << " | " << opticalGroupId << " | " << hybridId << " | " << std::endl;
        // std::cout << "theOutputContainer content" << std::endl;
        // std::cout << "Hybrid occupancy " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getSummary<Occupancy>().fOccupancy << std::endl;
        // std::cout << "Chip occupancy " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(0)->getSummary<Occupancy>().fOccupancy << std::endl;
        // for(auto channel : *theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(0)->getChannelContainer<Occupancy>()) std::cout << channel.fOccupancy << " ";
        // std::cout << std::endl;
        // std::cout << "chip content" << std::endl;
        // std::cout << "Hybrid occupancy " << hybrid.getSummary<Occupancy>().fOccupancy << std::endl;
        std::cout << "Chip occupancy " << hybrid.getObject(0)->getSummary<Occupancy>().fOccupancy << std::endl;
        // for(auto channel : *hybrid.getObject(0)->getChannelContainer<Occupancy>()) std::cout << channel.fOccupancy << " ";
        // std::cout << std::endl;
        // std::cout<<__PRETTY_FUNCTION__<<__LINE__ << " hybrid pointer " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId) <<std::endl;
        // std::cout<<__PRETTY_FUNCTION__<<__LINE__ << " chip pointer " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(0) <<std::endl;
        // std::cout<<__PRETTY_FUNCTION__<<__LINE__ << " chip pointer " << theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(1) <<std::endl;


        return theOutputContainer;
    }

    template <typename T, typename SC, typename... Args>
    DetectorDataContainer deserializeChipContainer(const DetectorContainer* theDetectorContainer, Args&... extraArguments)
    {
        DetectorDataContainer theOutputContainer;
        ContainerFactory::copyStructure(*theDetectorContainer, theOutputContainer);
        std::istringstream inputStream(fStream);
        boost::archive::text_iarchive theArchive(inputStream);
        std::string inputCalibrationName;
        theArchive >> inputCalibrationName;
        uint16_t boardId = 65535;
        uint16_t opticalGroupId = 65535;
        uint16_t hybridId = 65535;
        uint16_t chipId = 65535;
        theArchive >> boardId;
        theArchive >> opticalGroupId;
        theArchive >> hybridId;
        theArchive >> chipId;

        ChipDataContainer& chip = *theOutputContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId);
        chip.initialize<SC, T>();

        theArchive >> chip;
        serializeExtraArguments(theArchive, extraArguments...);

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
    template<typename T, typename... Args>
    void serializeExtraArguments(T& theArchive, Args&... extraArguments) const
    {
        auto extraArgumentTuple = std::tuple<Args&...>(extraArguments...);
        serialize(theArchive, extraArgumentTuple);
    }

    std::string fCalibrationName;
    std::string fStream;

};


#endif