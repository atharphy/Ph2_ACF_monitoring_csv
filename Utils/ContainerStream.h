/*

        \file                          ContainerStream.h
        \brief                         ContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __CONTAINERSTREAM_H__
#define __CONTAINERSTREAM_H__
// pointers to base class
#include "../HWDescription/ReadoutChip.h"
#include "../NetworkUtils/TCPPublishServer.h"
#include "../Utils/DataContainer.h"
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

// workaround missing "is_trivially_copyable" in g++ < 5.0
// #if __cplusplus < 201402
// namespace std
// {
// 	template<typename T>
// 	struct is_trivially_copyable
// 	{
// 		static const bool value = __has_trivial_copy(T);
// 	};
// }
// #endif

// Here I am using the The Curiously Recurring Template Pattern (CRTP)
// To avoid another inheritance I create a Header base class which takes as template the Child class
// (I need this to do the do sizeof(H), otheswise, since no virtual functions are present this will point to
// HeaderStreamContainerBase) In this base class I have all the datamember I need for a base container

template <typename T>
void constexpr check_if_retrivable()
{
    static_assert(!std::is_pointer<T>::value, "No pointers can be retreived from the stream");
    static_assert(!std::is_reference<T>::value, "No references can be retreived from the stream");
}

class EmptyContainer;

class ContainerCarried
{
  public:
    ContainerCarried() : fContainerCarried(0){};
    ~ContainerCarried(){};

    void reset() { fContainerCarried = 0; }

    void carryChannelContainer() { fContainerCarried |= (1 << 0); }
    void carryChipContainer() { fContainerCarried |= (1 << 1); }
    void carryHybridContainer() { fContainerCarried |= (1 << 2); }
    void carryOpticalGroupContainer() { fContainerCarried |= (1 << 3); }
    void carryBoardContainer() { fContainerCarried |= (1 << 4); }

    bool isChannelContainerCarried() { return (fContainerCarried >> 0) & 1; }
    bool isChipContainerCarried() { return (fContainerCarried >> 1) & 1; }
    bool isHybridContainerCarried() { return (fContainerCarried >> 2) & 1; }
    bool isOpticalGroupContainerCarried() { return (fContainerCarried >> 3) & 1; }
    bool isBoardContainerCarried() { return (fContainerCarried >> 4) & 1; }

    uint8_t fContainerCarried;
} __attribute__((packed));

template <typename H>
class HeaderStreamContainerBase : public DataStreamBase
{
  public:
    HeaderStreamContainerBase(){};
    ~HeaderStreamContainerBase(){};
    HeaderStreamContainerBase(HeaderStreamContainerBase &&) = default;
    HeaderStreamContainerBase(const HeaderStreamContainerBase &) = delete;

    uint32_t size(void) override
    {
        fDataSize = uint64_t(this) + sizeof(H) - uint64_t(&fDataSize);
        return fDataSize;
    }
} __attribute__((packed));

// Generic Header which allows to add other members to the header
// !!! IMPORTANT: the members that you add need to be continuos in memory or data want to shipped and you will get a
// crash !!! ContainerStream<Occupancy,int>         --> OK ContainerStream<Occupancy,char*>       --> ERROR
// ContainerStream<Occupancy,vector<int>> --> ERROR

template <typename... I>
class HeaderStreamContainer : public HeaderStreamContainerBase<HeaderStreamContainer<I...>>
{
    template <std::size_t N>
    using TupleElementType = typename std::tuple_element<N, std::tuple<I...>>::type;

  public:
    HeaderStreamContainer(){};
    ~HeaderStreamContainer(){};
    HeaderStreamContainer(HeaderStreamContainer<I...>&& theHeaderStreamContainer)
    : HeaderStreamContainerBase<HeaderStreamContainer<I...>>(std::move(theHeaderStreamContainer)) {}
    HeaderStreamContainer(const HeaderStreamContainer<I...>&) = delete;

    template <std::size_t N = 0>
    void setHeaderInfo(TupleElementType<N> theInfo)
    {
        std::get<N>(fInfo) = theInfo;
    }

    template <std::size_t N = 0>
    TupleElementType<N> getHeaderInfo() const
    {
        check_if_retrivable<TupleElementType<N>>();
        return std::get<N>(fInfo);
    }

    typename std::tuple<I...> fInfo;
};

// Specialized Header class when the parameter pack is empty
template <>
class HeaderStreamContainer<> : public HeaderStreamContainerBase<HeaderStreamContainer<>>
{
} __attribute__((packed));

// ------------------------------------------- v ------------------------------------------- //

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

template <typename T, typename C, typename... I>
class ChipContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChipContainer<T, C>>
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
    ChipContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, uint16_t, I...>, DataStreamChipContainer<T, C>>(creatorName) { ; }
    ~ChipContainerStream() { ; }

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

// ------------------------------------------- HybridContainerStream ------------------------------------------- //

template <typename T, typename C, typename M>
class DataStreamHybridContainer : public DataStreamBase
{
  public:
    DataStreamHybridContainer() : fHybridSummaryContainer(nullptr)
    {
        check_if_retrivable<C>();
        check_if_retrivable<M>();
    }

    DataStreamHybridContainer(const DataStreamHybridContainer<T, C, M>&) = delete;

    DataStreamHybridContainer(DataStreamHybridContainer<T, C, M>&& theOriginalStream)
    : DataStreamBase(std::move(theOriginalStream))
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        fNumberOfChips    = theOriginalStream.fNumberOfChips;
        for(auto& theChannelContainer : theOriginalStream.fChannelContainerVector)
        {
            fChannelContainerVector.emplace_back(theChannelContainer);
            theChannelContainer = nullptr;
        }
        for(auto& theChipSummaryContainer : theOriginalStream.fChipSummaryContainerVector)
        {
            fChipSummaryContainerVector.emplace_back(theChipSummaryContainer);
            theChipSummaryContainer = nullptr;
        }
        fHybridSummaryContainer = theOriginalStream.fHybridSummaryContainer;
        theOriginalStream.fHybridSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;
        fId = theOriginalStream.fId;
    }

    DataStreamHybridContainer<T,C,M>& operator=(const DataStreamHybridContainer<T,C,M>&) = delete;

    DataStreamHybridContainer<T,C,M>& operator=(DataStreamHybridContainer<T,C,M>&& theOriginalStream)
    {
        DataStreamBase::operator=(std::move(theOriginalStream));
        fContainerCarried = theOriginalStream.fContainerCarried;
        fNumberOfChips    = theOriginalStream.fNumberOfChips;
        for(auto& theChannelContainer : theOriginalStream.fChannelContainerVector)
        {
            fChannelContainerVector.emplace_back(theChannelContainer);
            theChannelContainer = nullptr;
        }
        for(auto& theChipSummaryContainer : theOriginalStream.fChipSummaryContainerVector)
        {
            fChipSummaryContainerVector.emplace_back(theChipSummaryContainer);
            theChipSummaryContainer = nullptr;
        }
        fHybridSummaryContainer = theOriginalStream.fHybridSummaryContainer;
        theOriginalStream.fHybridSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;
        fId = theOriginalStream.fId;
        return *this;
    }



    ~DataStreamHybridContainer()
    {
        if(fDeletePointers)
        {
            for(auto element: fChannelContainerVector)
            {
                if(element != nullptr)
                {
                    delete element;
                    element = nullptr;
                }
            }
            fChannelContainerVector.clear();

            for(auto element: fChipSummaryContainerVector)
            {
                if(element != nullptr)
                {
                    delete element;
                    element = nullptr;
                }
            }
            fChipSummaryContainerVector.clear();

            if(fHybridSummaryContainer != nullptr)
            {
                delete fHybridSummaryContainer;
                fHybridSummaryContainer = nullptr;
            }
        }
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried) + sizeof(fNumberOfChips);
        if(fHybridSummaryContainer != nullptr) { fDataSize += sizeof(M); }
        fDataSize += sizeOfChipContainer();
        fDataSize += sizeOfChannelContainer();

        return fDataSize;
    }

    size_t copyToStream(char* bufferBegin, size_t bufferWritingPosition = 0)
    {
        memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
        bufferWritingPosition += sizeof(fDataSize);

        memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
        bufferWritingPosition += sizeof(fContainerCarried);

        memcpy(&bufferBegin[bufferWritingPosition], &fNumberOfChips, sizeof(fNumberOfChips));
        bufferWritingPosition += sizeof(fNumberOfChips);

        if(fContainerCarried.isHybridContainerCarried())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(fHybridSummaryContainer->theSummary_), sizeof(M));
            bufferWritingPosition += sizeof(M);
            fHybridSummaryContainer = nullptr;
        }

        if(fContainerCarried.isChipContainerCarried())
        {

            for(auto& chipSummary: fChipSummaryContainerVector)
            {
                memcpy(&bufferBegin[bufferWritingPosition], &(chipSummary->theSummary_), sizeof(C));
                bufferWritingPosition += sizeof(C);
                chipSummary = nullptr;
            }
        }

        if(fContainerCarried.isChannelContainerCarried())
        {
            for(auto channelContainer: fChannelContainerVector)
            {
                memcpy(&bufferBegin[bufferWritingPosition], &(channelContainer->at(0)), channelContainer->size() * sizeof(T));
                bufferWritingPosition += channelContainer->size() * sizeof(T);
                channelContainer = nullptr;
            }
        }

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers = true;

        size_t originalBufferReadingPosition = bufferReadingPosition;
        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);

        memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
        bufferReadingPosition += sizeof(fContainerCarried);

        memcpy(&fNumberOfChips, &bufferBegin[bufferReadingPosition], sizeof(fNumberOfChips));
        bufferReadingPosition += sizeof(fNumberOfChips);

        if(fContainerCarried.isHybridContainerCarried())
        {
            fHybridSummaryContainer = new Summary<M, C>();
            memcpy(&(fHybridSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(M));
            bufferReadingPosition += sizeof(M);
        }
        if(fContainerCarried.isChipContainerCarried())
        {
            for(size_t chipIndex = 0; chipIndex < fNumberOfChips; ++chipIndex)
            {
                Summary<C, T>* chipSummaryContainer = new Summary<C, T>();
                memcpy(&(chipSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(C));
                fChipSummaryContainerVector.emplace_back(chipSummaryContainer);
                bufferReadingPosition += sizeof(C);
            }
        }
        if(fContainerCarried.isChannelContainerCarried())
        {
            size_t vectorSize = (fDataSize - bufferReadingPosition - originalBufferReadingPosition) / (sizeof(T) * fNumberOfChips);

            for(size_t chipIndex = 0; chipIndex < fNumberOfChips; ++chipIndex)
            {
                ChannelContainer<T>* channelContainer = new ChannelContainer<T>(vectorSize);
                memcpy(&channelContainer->at(0), &bufferBegin[bufferReadingPosition], vectorSize * sizeof(T));
                fChannelContainerVector.emplace_back(channelContainer);
                bufferReadingPosition += vectorSize * sizeof(T);
            }
        }

        return bufferReadingPosition;
    }

  private:
    uint32_t sizeOfChannelContainer()
    {
        if(fChannelContainerVector.size() == 0) return 0;
        uint32_t size = 0;
        for(auto element: fChannelContainerVector) size += (sizeof(T) * element->size());
        return size;
    }
    uint32_t sizeOfChipContainer()
    {
        if(fChipSummaryContainerVector.size() == 0) return 0;

        return fChipSummaryContainerVector.size() * sizeof(C);
    }

  public:
    ContainerCarried                  fContainerCarried;
    uint8_t                           fNumberOfChips;
    std::vector<ChannelContainer<T>*> fChannelContainerVector;
    std::vector<Summary<C, T>*>       fChipSummaryContainerVector;
    Summary<M, C>*                    fHybridSummaryContainer;
    bool                              fDeletePointers{false};
    uint16_t                          fId{65535};
};

template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream;

template <typename T, typename C, typename H, typename... I>
class HybridContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamHybridContainer<T, C, H>>
{
    template <typename T1, typename C1, typename H1, typename O, typename... I1>
    friend class OpticalGroupContainerStream;

    enum HeaderId
    {
        BoardId,
        OpticalGroupId,
        HybridId
    };
    static constexpr size_t getEnumSize() { return HybridId + 1; }

  public:
    HybridContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamHybridContainer<T, C, H>>(creatorName) {}
    HybridContainerStream(HybridContainerStream<T, C, H, I...>&& theHybridContainerStream)
    : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamHybridContainer<T, C, H>>(std::move(theHybridContainerStream))
    {

    }
    HybridContainerStream(const HybridContainerStream<T, C, H, I...>&) = delete;
    ~HybridContainerStream() { ; }

    void setContainerCarried(const ContainerCarried& theContainerCarried) { this->fDataStream.fContainerCarried = theContainerCarried; }
    ContainerCarried getContainerCarried() const { return this->fDataStream.fContainerCarried; }

    void streamAndSendBoard(const BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                retrieveHybridData(board->getId(), opticalGroup->getId(), hybrid);
                auto stream = this->encodeStream();
                this->incrementStreamPacketNumber();
                networkStreamer->broadcast(*stream.get());
            }
        }
    }

    void decodeHybridData(DetectorDataContainer& detectorContainer)
    {
        uint16_t boardId        = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();
        uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();
        uint16_t hybridId       = this->fHeaderStream.template getHeaderInfo<HeaderId::HybridId>();

        detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->setSummaryContainer(this->fDataStream.fHybridSummaryContainer);
        this->fDataStream.fHybridSummaryContainer = nullptr;

        for(auto chip: *detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId))
        {
            if(this->fDataStream.fContainerCarried.isChipContainerCarried())
            {
                chip->setSummaryContainer(this->fDataStream.fChipSummaryContainerVector.at(chip->getId()));
                this->fDataStream.fChipSummaryContainerVector.at(chip->getId()) = nullptr;
            }
            if(this->fDataStream.fContainerCarried.isChannelContainerCarried())
            {
                chip->setChannelContainer(this->fDataStream.fChannelContainerVector.at(chip->getId()));
                this->fDataStream.fChannelContainerVector.at(chip->getId()) = nullptr;
            }
        }
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

    void retrieveHybridData(uint16_t boardId, uint16_t opticalGroupId, HybridDataContainer* hybrid)
    {
        this->fHeaderStream.template setHeaderInfo<HeaderId::BoardId>(boardId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::OpticalGroupId>(opticalGroupId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::HybridId>(hybrid->getId());
        this->fDataStream.fNumberOfChips = hybrid->size();
        this->fDataStream.fChipSummaryContainerVector.clear();
        this->fDataStream.fChannelContainerVector.clear();
        if(hybrid->getSummaryContainer<H, C>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryHybridContainer();
            this->fDataStream.fHybridSummaryContainer = hybrid->getSummaryContainer<H, C>();
        }
        for(auto chip: *hybrid)
        {
            if(chip->getSummaryContainer<C, T>() != nullptr)
            {
                this->fDataStream.fContainerCarried.carryChipContainer();
                this->fDataStream.fChipSummaryContainerVector.emplace_back(chip->getSummaryContainer<C, T>());
            }
            if(chip->getChannelContainer<T>() != nullptr)
            {
                this->fDataStream.fContainerCarried.carryChannelContainer();
                this->fDataStream.fChannelContainerVector.emplace_back(chip->getChannelContainer<T>());
            }
        }
    }
};


#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
