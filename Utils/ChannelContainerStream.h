/*

        \file                          ChannelContainerStream.h
        \brief                         ChannelContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __CHANNELCONTAINERSTREAM_H__
#define __CHANNELCONTAINERSTREAM_H__
// pointers to base class
#include "../HWDescription/ReadoutChip.h"
#include "../NetworkUtils/TCPPublishServer.h"
#include "../Utils/DataContainer.h"
#include "../Utils/ObjectStream.h"
#include "../Utils/ContainerStream.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

template <typename C>
class DataStreamChannelContainer : public DataStreamBase
{
  public:
    DataStreamChannelContainer() : fChannelContainer(nullptr) {}
    ~DataStreamChannelContainer()
    {
        if(fDeletePointers)
        {
            if(fChannelContainer != nullptr) delete fChannelContainer;
            fChannelContainer = nullptr;
        }
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + fChannelContainer->size() * sizeof(C);
        return fDataSize;
    }

    size_t copyToStream(char* bufferBegin, size_t bufferWritingPosition = 0)
    {
        memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
        bufferWritingPosition += sizeof(fDataSize);

        memcpy(&bufferBegin[bufferWritingPosition], &fChannelContainer->at(0), fChannelContainer->size() * sizeof(C));
        fChannelContainer = nullptr;
        bufferWritingPosition += fChannelContainer->size() * sizeof(C);

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers = true;

        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);

        fChannelContainer = new ChannelContainer<C>((fDataSize - sizeof(fDataSize)) / sizeof(C));
        memcpy(&fChannelContainer->at(0), &bufferBegin[bufferReadingPosition], fDataSize - sizeof(fDataSize));
        bufferReadingPosition += (fChannelContainer->size() * sizeof(C));

        return bufferReadingPosition;
    }

  public:
    ChannelContainer<C>* fChannelContainer;
    bool                 fDeletePointers{false};
} __attribute__((packed));

template <typename C, typename... I>
class ChannelContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChannelContainer<C>>
{
    enum HeaderId
    {
        BoardId,
        OpticalGroupId,
        HybridId,
        ChipId
    };
    static constexpr size_t getEnumSize() { return ChipId + 1; }

  public:
    ChannelContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChannelContainer<C>>(creatorName) { ; }
    ~ChannelContainerStream() { ; }

    void streamAndSendBoard(const BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    retrieveChipData(board->getId(), opticalGroup->getId(), hybrid->getId(), chip);
                    auto stream = this->encodeStream();
                    this->incrementStreamPacketNumber();
                    networkStreamer->broadcast(*stream.get());
                }
            }
        }
    }

    void decodeChipData(DetectorDataContainer& detectorContainer)
    {
        detectorContainer.getObject(this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>())
            ->getObject(this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>())
            ->getObject(this->fHeaderStream.template getHeaderInfo<HeaderId::HybridId>())
            ->getObject(this->fHeaderStream.template getHeaderInfo<HeaderId::ChipId>())
            ->setChannelContainer(this->fDataStream.fChannelContainer);
        this->fDataStream.fChannelContainer = nullptr;
    }

    template <std::size_t N>
    using TupleElementType = typename std::tuple_element<N, std::tuple<I...>>::type;

    template <std::size_t N = 0>
    void setHeaderElement(TupleElementType<N> theInfo)
    {
        this->fHeaderStream.template setHeaderInfo<N + getEnumSize()>(theInfo);
    }

    template <std::size_t N = 0>
    TupleElementType<N> getHeaderElement() const
    {
        return this->fHeaderStream.template getHeaderInfo<N + getEnumSize()>();
    }

  protected:
    void retrieveChipData(uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, ChipDataContainer* chip)
    {
        this->fHeaderStream.template setHeaderInfo<HeaderId::BoardId>(boardId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::OpticalGroupId>(opticalGroupId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::HybridId>(hybridId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::ChipId>(chip->getId());
        this->fDataStream.fChannelContainer = chip->getChannelContainer<C>();
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
