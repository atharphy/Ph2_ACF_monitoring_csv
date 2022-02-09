/*

        \file                          ChipContainerStream.h
        \brief                         ChipContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __CHIPCONTAINERSTREAM_H__
#define __CHIPCONTAINERSTREAM_H__
// pointers to base class
#include "../HWDescription/ReadoutChip.h"
#include "../NetworkUtils/TCPPublishServer.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/ObjectStream.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cxxabi.h>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <type_traits>
#include <vector>

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wclass-memaccess"
#endif

// ------------------------------------------- ChipContainerStream ------------------------------------------- //

template <typename T, typename C>
class DataStreamChipContainer : public DataStreamBase
{
  public:
    DataStreamChipContainer() : fChannelContainer(nullptr), fChipSummaryContainer(nullptr) { check_if_retrivable<C>(); }
    ~DataStreamChipContainer()
    {
        if(fDeletePointers)
        {
            if(fChannelContainer != nullptr) delete fChannelContainer;
            fChannelContainer = nullptr;

            if(fChipSummaryContainer != nullptr) delete fChipSummaryContainer;
            fChipSummaryContainer = nullptr;
        }
    }

    DataStreamChipContainer(const DataStreamChipContainer& theOriginalStream) = delete;

    DataStreamChipContainer(DataStreamChipContainer&& theOriginalStream)
    : DataStreamBase(std::move(theOriginalStream))
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        fChannelContainer = theOriginalStream.fChannelContainer;
        theOriginalStream.fChannelContainer = nullptr;
        fChipSummaryContainer = theOriginalStream.fChipSummaryContainer;
        theOriginalStream.fChipSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;
    }

    DataStreamChipContainer& operator=(const DataStreamChipContainer&) = delete;

    DataStreamChipContainer& operator=(DataStreamChipContainer&& theOriginalStream)
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        delete fChannelContainer;
        fChannelContainer = theOriginalStream.fChannelContainer;
        theOriginalStream.fChannelContainer = nullptr;
        delete fChipSummaryContainer;
        fChipSummaryContainer = theOriginalStream.fChipSummaryContainer;
        theOriginalStream.fChipSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;

        return *this;
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried);
        if(fChipSummaryContainer != nullptr) { fDataSize += sizeof(C); }
        if(fChannelContainer != nullptr) { fDataSize += fChannelContainer->size() * sizeof(T); }
        return fDataSize;
    }

    size_t copyToStream(char* bufferBegin, size_t bufferWritingPosition = 0)
    {
        memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
        bufferWritingPosition += sizeof(fDataSize);

        memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
        bufferWritingPosition += sizeof(fContainerCarried);

        if(fContainerCarried.isChipContainerCarried())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(fChipSummaryContainer->theSummary_), sizeof(C));
            bufferWritingPosition += sizeof(C);
            fChipSummaryContainer = nullptr;
        }
        if(fContainerCarried.isChannelContainerCarried())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &fChannelContainer->at(0), fChannelContainer->size() * sizeof(T));
            bufferWritingPosition += (fChannelContainer->size() * sizeof(T));
            fChannelContainer = nullptr;
        }

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers                      = true;
        size_t originalBufferReadingPosition = bufferReadingPosition;
        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);

        memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
        bufferReadingPosition += sizeof(fContainerCarried);

        if(fContainerCarried.isChipContainerCarried())
        {
            fChipSummaryContainer = new Summary<C, T>();
            memcpy(&(fChipSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(C));
            bufferReadingPosition += sizeof(C);
        }
        if(fContainerCarried.isChannelContainerCarried())
        {
            fChannelContainer = new ChannelContainer<T>((fDataSize - bufferReadingPosition - originalBufferReadingPosition) / sizeof(T));
            memcpy(&fChannelContainer->at(0), &bufferBegin[bufferReadingPosition], fChannelContainer->size() * sizeof(T));
            bufferReadingPosition += (fDataSize - bufferReadingPosition - originalBufferReadingPosition);
        }
        return bufferReadingPosition;
    }

  public:
    ContainerCarried     fContainerCarried;
    ChannelContainer<T>* fChannelContainer;
    Summary<C, T>*       fChipSummaryContainer;
    bool                 fDeletePointers{false};

} __attribute__((packed));

template <typename T, typename C, typename H, typename... I>
class HybridContainerStream;

template <typename T, typename C, typename... I>
class ChipContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChipContainer<T, C>>
{
    template <typename T1, typename C1, typename H, typename... I1>
    friend class HybridContainerStream;
 
    enum HeaderId
    {
        BoardId,
        OpticalGroupId,
        HybridId,
        ChipId
    };
    static constexpr size_t getEnumSize() { return ChipId + 1; }

  public:
    ChipContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChipContainer<T, C>>(creatorName) { ; }
    ~ChipContainerStream() { ; }

    void setContainerCarried(const ContainerCarried& theContainerCarried) { this->fDataStream.fContainerCarried = theContainerCarried; }
    ContainerCarried getContainerCarried() const { return this->fDataStream.fContainerCarried; }

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
        uint16_t boardId        = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();
        uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();
        uint16_t hybridId       = this->fHeaderStream.template getHeaderInfo<HeaderId::HybridId>();
        uint16_t chipId         = this->fHeaderStream.template getHeaderInfo<HeaderId::ChipId>();

        detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId)->setChannelContainer(this->fDataStream.fChannelContainer);
        detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId)->setSummaryContainer(this->fDataStream.fChipSummaryContainer);
        this->fDataStream.fChannelContainer     = nullptr;
        this->fDataStream.fChipSummaryContainer = nullptr;
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
        if(chip->getChannelContainer<T>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryChannelContainer();
            this->fDataStream.fChannelContainer = chip->getChannelContainer<T>();
        }
        if(chip->getSummaryContainer<C, T>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryChipContainer();
            this->fDataStream.fChipSummaryContainer = chip->getSummaryContainer<C, T>();
        }
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
