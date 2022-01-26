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

    void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    retrieveChipData(board->getId(), opticalGroup->getId(), hybrid->getId(), chip);
                    const std::vector<char>& stream = this->encodeStream();
                    this->incrementStreamPacketNumber();
                    /* std::cout << __PRETTY_FUNCTION__ << "SENDING STREAM!" << std::endl; */
                    networkStreamer->broadcast(stream);
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

    void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    retrieveChipData(board->getId(), opticalGroup->getId(), hybrid->getId(), chip);
                    const std::vector<char>& stream = this->encodeStream();
                    this->incrementStreamPacketNumber();
                    networkStreamer->broadcast(stream);
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

    DataStreamHybridContainer(const DataStreamHybridContainer&) = delete;

    DataStreamHybridContainer(DataStreamHybridContainer&& theOriginalStream)
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
        std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

        memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
        bufferWritingPosition += sizeof(fContainerCarried);
        std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

        memcpy(&bufferBegin[bufferWritingPosition], &fNumberOfChips, sizeof(fNumberOfChips));
        bufferWritingPosition += sizeof(fNumberOfChips);
        std::cout << __PRETTY_FUNCTION__ << "fNumberOfChips = " << +fNumberOfChips << std::endl;

        if(fContainerCarried.isHybridContainerCarried())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(fHybridSummaryContainer->theSummary_), sizeof(M));
            bufferWritingPosition += sizeof(M);
            fHybridSummaryContainer = nullptr;
        }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;

        if(fContainerCarried.isChipContainerCarried())
        {
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            for(auto chipSummary: fChipSummaryContainerVector)
            {
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
                memcpy(&bufferBegin[bufferWritingPosition], &(chipSummary->theSummary_), sizeof(C));
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
                bufferWritingPosition += sizeof(C);
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
                chipSummary = nullptr;
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;

        if(fContainerCarried.isChannelContainerCarried())
        {
            for(auto channelContainer: fChannelContainerVector)
            {
                memcpy(&bufferBegin[bufferWritingPosition], &(channelContainer->at(0)), channelContainer->size() * sizeof(T));
                bufferWritingPosition += channelContainer->size() * sizeof(T);
                std::cout << __PRETTY_FUNCTION__ << "vectorSize = " << +channelContainer->size() << std::endl;
                channelContainer = nullptr;
            }
        }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers = true;

        size_t originalBufferReadingPosition = bufferReadingPosition;
        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);
        std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

        memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
        bufferReadingPosition += sizeof(fContainerCarried);
        std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

        memcpy(&fNumberOfChips, &bufferBegin[bufferReadingPosition], sizeof(fNumberOfChips));
        bufferReadingPosition += sizeof(fNumberOfChips);
        std::cout << __PRETTY_FUNCTION__ << "fNumberOfChips = " << +fNumberOfChips << std::endl;

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
                std::cout << __PRETTY_FUNCTION__ << " chipIndex = " << chipIndex << std::endl;
                Summary<C, T>* chipSummaryContainer = new Summary<C, T>();
                memcpy(&(chipSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(C));
                fChipSummaryContainerVector.emplace_back(chipSummaryContainer);
                bufferReadingPosition += sizeof(C);
            }
        }
        if(fContainerCarried.isChannelContainerCarried())
        {
            size_t vectorSize = (fDataSize - bufferReadingPosition - originalBufferReadingPosition) / (sizeof(T) * fNumberOfChips);
            std::cout << __PRETTY_FUNCTION__ << "vectorSize = " << +vectorSize << std::endl;

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
        std::cout << __PRETTY_FUNCTION__ << +size << std::endl;
        return size;
    }
    uint32_t sizeOfChipContainer()
    {
        if(fChipSummaryContainerVector.size() == 0) return 0;
        std::cout << __PRETTY_FUNCTION__ << " Chip vector size = " << +fChipSummaryContainerVector.size() << std::endl;
        std::cout << __PRETTY_FUNCTION__ << " Chip summary size = " << +sizeof(C) << std::endl;
        std::cout << __PRETTY_FUNCTION__ << " Total size = " << +fChipSummaryContainerVector.size() * sizeof(C) << std::endl;

        return fChipSummaryContainerVector.size() * sizeof(C);
    }

  public:
    ContainerCarried                  fContainerCarried;
    uint8_t                           fNumberOfChips;
    std::vector<ChannelContainer<T>*> fChannelContainerVector;
    std::vector<Summary<C, T>*>       fChipSummaryContainerVector;
    Summary<M, C>*                    fHybridSummaryContainer;
    bool                              fDeletePointers{false};
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
    HybridContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamHybridContainer<T, C, H>>(creatorName) { ; }
    ~HybridContainerStream() { ; }

    void setContainerCarried(const ContainerCarried& theContainerCarried) { this->fDataStream.fContainerCarried = theContainerCarried; }
    ContainerCarried getContainerCarried() const { return this->fDataStream.fContainerCarried; }

    void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                retrieveHybridData(board->getId(), opticalGroup->getId(), hybrid);
                const std::vector<char>& stream = this->encodeStream();
                this->incrementStreamPacketNumber();
                networkStreamer->broadcast(stream);
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
            std::cout << __PRETTY_FUNCTION__<< " ChipId = " << chip->getId() << std::endl;
            if(this->fDataStream.fContainerCarried.isChipContainerCarried())
            {
                std::cout << __PRETTY_FUNCTION__<< " Container carried " << std::endl;
                chip->setSummaryContainer(this->fDataStream.fChipSummaryContainerVector.at(chip->getId()));
                std::cout << __PRETTY_FUNCTION__<< __LINE__ << std::endl;
                this->fDataStream.fChipSummaryContainerVector.at(chip->getId()) = nullptr;
                std::cout << __PRETTY_FUNCTION__<< __LINE__ << std::endl;
            }
            if(this->fDataStream.fContainerCarried.isChannelContainerCarried())
            {
                chip->setChannelContainer(this->fDataStream.fChannelContainerVector.at(chip->getId()));
                this->fDataStream.fChannelContainerVector.at(chip->getId()) = nullptr;
            }
            std::cout << __PRETTY_FUNCTION__<< __LINE__ << std::endl;
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

// ------------------------------------------- OpticalGroupContainerStream ------------------------------------------- //

template <typename T, typename C, typename H, typename O>
class DataStreamOpticalGroupContainer : public DataStreamBase
{
  public:
    DataStreamOpticalGroupContainer() : fOpticalGroupSummaryContainer(nullptr) { check_if_retrivable<O>(); }
    ~DataStreamOpticalGroupContainer()
    {
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        if(fDeletePointers)
        {
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        //     for(auto element: fDataSteamHybridContainerVector)
        //     {
        // std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        //         if(element != nullptr)
        //         {
        // std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        //             delete element;
        // std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        //             element = nullptr;
        // std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        //         }
        //     }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            fDataSteamHybridContainerVector.clear();
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;

            if(fOpticalGroupSummaryContainer == nullptr)
            {
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
                delete fOpticalGroupSummaryContainer;
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
                fOpticalGroupSummaryContainer = nullptr;
            }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        }
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried) + sizeof(fNumberOfSubContainers);
        for(auto& dataSteamHybridContainer: fDataSteamHybridContainerVector) { fDataSize += dataSteamHybridContainer.size(); }
        if(fOpticalGroupSummaryContainer != nullptr) { fDataSize += sizeof(O); }
        return fDataSize;
    }

    size_t copyToStream(char* bufferBegin, size_t bufferWritingPosition = 0)
    {
        memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
        bufferWritingPosition += sizeof(fDataSize);
        std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

        memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
        bufferWritingPosition += sizeof(fContainerCarried);
        std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

        memcpy(&bufferBegin[bufferWritingPosition], &fNumberOfSubContainers, sizeof(fNumberOfSubContainers));
        bufferWritingPosition += sizeof(fNumberOfSubContainers);
        std::cout << __PRETTY_FUNCTION__ << "fNumberOfSubContainers = " << +fNumberOfSubContainers << std::endl;

        if(fContainerCarried.isOpticalGroupContainerCarried())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(fOpticalGroupSummaryContainer->theSummary_), sizeof(O));
            bufferWritingPosition += sizeof(O);
            fOpticalGroupSummaryContainer = nullptr;
        }

        for(auto& subContainer: fDataSteamHybridContainerVector) 
        {
            std::cout << __PRETTY_FUNCTION__ << "bufferWritingPosition = " << +bufferWritingPosition << std::endl;
            bufferWritingPosition = subContainer.copyToStream(&bufferBegin[bufferWritingPosition], bufferWritingPosition);
            std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        }

        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers = true;

        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);
        std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

        memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
        bufferReadingPosition += sizeof(fContainerCarried);
        std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

        memcpy(&fNumberOfSubContainers, &bufferBegin[bufferReadingPosition], sizeof(fNumberOfSubContainers));
        bufferReadingPosition += sizeof(fNumberOfSubContainers);
        std::cout << __PRETTY_FUNCTION__ << "fNumberOfSubContainers = " << +fNumberOfSubContainers << std::endl;

        if(fContainerCarried.isOpticalGroupContainerCarried())
        {
            fOpticalGroupSummaryContainer = new Summary<O, H>();
            memcpy(&(fOpticalGroupSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(O));
            bufferReadingPosition += sizeof(O);
        }

        for(uint8_t subContainerIndex = 0; subContainerIndex<fNumberOfSubContainers; ++subContainerIndex)
        {
            fDataSteamHybridContainerVector.emplace_back(DataStreamHybridContainer<T, C, H>());
            std::cout << __PRETTY_FUNCTION__ << "bufferReadingPosition = " << +bufferReadingPosition << std::endl;
            bufferReadingPosition = fDataSteamHybridContainerVector.back().copyFromStream(bufferBegin, bufferReadingPosition);
        }
        // for(auto dataSteamHybridContainerVector: fDataSteamHybridContainerVector) { bufferReadingPosition = dataSteamHybridContainerVector->copyFromStream(bufferBegin, bufferReadingPosition); }

        return bufferReadingPosition;
    }

  public:
    ContainerCarried                                fContainerCarried{};
    std::vector<DataStreamHybridContainer<T, C, H>> fDataSteamHybridContainerVector{};
    uint8_t                                         fNumberOfSubContainers{0};
    Summary<O, H>*                                  fOpticalGroupSummaryContainer{nullptr};
    bool                                            fDeletePointers{false};
};

template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T, C, H, O>>
{
    enum HeaderId
    {
        BoardId,
        OpticalGroupId
    };
    static constexpr size_t getEnumSize() { return OpticalGroupId + 1; }

  public:
    OpticalGroupContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T, C, H, O>>(creatorName) 
    { 
        std::cout << "Creating OpticalGroupContainerStream with pointer = " << this << std::endl;
    }
    ~OpticalGroupContainerStream() 
    { 
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << " pointer = " << this << std::endl;
    }

    void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            retrieveData(board->getId(), opticalGroup);
            std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            const std::vector<char>& stream = this->encodeStream();
            std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            this->incrementStreamPacketNumber();
            std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
            networkStreamer->broadcast(stream);
            std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
        }
        std::cout << __PRETTY_FUNCTION__ << __LINE__ << std::endl;
    }

    void decodeData(DetectorDataContainer& detectorContainer)
    {
        uint16_t boardId        = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();
        uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();

        if(this->fDataStream.fContainerCarried.isOpticalGroupContainerCarried())
        {
            detectorContainer.getObject(boardId)->getObject(opticalGroupId)->setSummaryContainer(this->fDataStream.fOpticalGroupSummaryContainer);
            this->fDataStream.fOpticalGroupSummaryContainer = nullptr;
        }

        for(auto hybrid: *detectorContainer.getObject(boardId)->getObject(opticalGroupId))
        {
            std::cout << __PRETTY_FUNCTION__<< " boardId = " << boardId << " opticalGroupId = " << opticalGroupId << " hybridId = " << hybrid->getId() << std::endl;
            HybridContainerStream<T, C, H> theHybridStreamer(this->fCreatorName);
            std::cout << __PRETTY_FUNCTION__<< " Container carried = " << +this->fDataStream.fContainerCarried.fContainerCarried << std::endl;
            theHybridStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::BoardId>(boardId);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::OpticalGroupId>(boardId);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::HybridId>(hybrid->getId());
            theHybridStreamer.decodeHybridData(detectorContainer);
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

    void retrieveData(uint16_t boardId, OpticalGroupDataContainer* opticalGroup)
    {
        this->fHeaderStream.template setHeaderInfo<HeaderId::BoardId>(boardId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::OpticalGroupId>(opticalGroup->getId());
        this->fDataStream.fNumberOfSubContainers = opticalGroup->size();
        this->fDataStream.fDataSteamHybridContainerVector.clear();
        if(opticalGroup->getSummaryContainer<O, H>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryOpticalGroupContainer();
            this->fDataStream.fOpticalGroupSummaryContainer = opticalGroup->getSummaryContainer<O, H>();
        }
        for(auto hybrid: *opticalGroup)
        {
            HybridContainerStream<T, C, H> theHybridStreamer(this->fCreatorName);
            theHybridStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            std::cout << __PRETTY_FUNCTION__<< " Container carried = " << +this->fDataStream.fContainerCarried.fContainerCarried << std::endl;
            theHybridStreamer.retrieveHybridData(boardId, opticalGroup->getId(), hybrid);
            this->fDataStream.fContainerCarried.fContainerCarried |= theHybridStreamer.getContainerCarried().fContainerCarried;
            std::cout << __PRETTY_FUNCTION__<< " Container carried = " << +this->fDataStream.fContainerCarried.fContainerCarried << std::endl;
            this->fDataStream.fDataSteamHybridContainerVector.emplace_back(std::move(theHybridStreamer.fDataStream));
        }
    }
};

// // ------------------------------------------- OpticalGroupContainerStream ------------------------------------------- //

// template <typename T, typename C, typename M, typename O>
// class DataStreamOpticalGroupContainer : public DataStreamBase
// {
//   public:
//     DataStreamOpticalGroupContainer() : fOpticalGroupSummaryContainer(nullptr)
//     {
//         check_if_retrivable<C>();
//         check_if_retrivable<M>();
//         check_if_retrivable<O>();
//     }
//     ~DataStreamOpticalGroupContainer()
//     {
//         for(auto element: fChannelContainerVector)
//         {
//             if(element != nullptr)
//             {
//                 delete element;
//                 element = nullptr;
//             }
//         }
//         fChannelContainerVector.clear();

//         for(auto element: fChipSummaryContainerVector)
//         {
//             if(element != nullptr)
//             {
//                 delete element;
//                 element = nullptr;
//             }
//         }
//         fChipSummaryContainerVector.clear();

//         if(fHybridSummaryContainer == nullptr)
//         {
//             delete fHybridSummaryContainer;
//             fHybridSummaryContainer = nullptr;
//         }
//     }

//     uint32_t size(void) override
//     {
//         fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried) + sizeof(fNumberOfChips);
//         if(fHybridSummaryContainer != nullptr) { fDataSize += sizeof(M); }
//         fDataSize += sizeOfChipContainer();
//         fDataSize += sizeOfChannelContainer();

//         return fDataSize;
//     }

//     size_t copyToStream(char* bufferBegin)
//     {
//         size_t bufferWritingPosition = 0;

//         memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
//         bufferWritingPosition += sizeof(fDataSize);
//         std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

//         memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
//         bufferWritingPosition += sizeof(fContainerCarried);
//         std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

//         memcpy(&bufferBegin[bufferWritingPosition], &fNumberOfChips, sizeof(fNumberOfChips));
//         bufferWritingPosition += sizeof(fNumberOfChips);
//         std::cout << __PRETTY_FUNCTION__ << "fNumberOfChips = " << +fNumberOfChips << std::endl;

//         if(fContainerCarried.isHybridContainerCarried())
//         {
//             memcpy(&bufferBegin[bufferWritingPosition], &(fHybridSummaryContainer->theSummary_), sizeof(M));
//             bufferWritingPosition += sizeof(M);
//             fHybridSummaryContainer = nullptr;
//         }

//         if(fContainerCarried.isChipContainerCarried())
//         {
//             for(auto chipSummary: fChipSummaryContainerVector)
//             {
//                 memcpy(&bufferBegin[bufferWritingPosition], &(chipSummary->theSummary_), sizeof(C));
//                 bufferWritingPosition += sizeof(C);
//                 chipSummary = nullptr;
//             }
//         }

//         if(fContainerCarried.isChannelContainerCarried())
//         {
//             for(auto channelContainer: fChannelContainerVector)
//             {
//                 memcpy(&bufferBegin[bufferWritingPosition], &(channelContainer->at(0)), channelContainer->size() * sizeof(T));
//                 bufferWritingPosition += channelContainer->size() * sizeof(T);
//                 std::cout << __PRETTY_FUNCTION__ << "vectorSize = " << +channelContainer->size() << std::endl;
//                 channelContainer = nullptr;
//             }
//         }
//     }

//     size_t copyFromStream(const char* bufferBegin)
//     {
//         size_t bufferReadingPosition = 0;

//         memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
//         bufferReadingPosition += sizeof(fDataSize);
//         std::cout << __PRETTY_FUNCTION__ << "fDataSize = " << +fDataSize << std::endl;

//         memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
//         bufferReadingPosition += sizeof(fContainerCarried);
//         std::cout << __PRETTY_FUNCTION__ << "fContainerCarried = " << +fContainerCarried.fContainerCarried << std::endl;

//         memcpy(&fNumberOfChips, &bufferBegin[bufferReadingPosition], sizeof(fNumberOfChips));
//         bufferReadingPosition += sizeof(fNumberOfChips);
//         std::cout << __PRETTY_FUNCTION__ << "fNumberOfChips = " << +fNumberOfChips << std::endl;

//         if(fContainerCarried.isHybridContainerCarried())
//         {
//             fHybridSummaryContainer = new Summary<M, C>();
//             memcpy(&(fHybridSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(M));
//             bufferReadingPosition += sizeof(M);
//         }
//         if(fContainerCarried.isChipContainerCarried())
//         {
//             for(size_t chipIndex = 0; chipIndex < fNumberOfChips; ++chipIndex)
//             {
//                 Summary<C, T>* chipSummaryContainer = new Summary<C, T>();
//                 memcpy(&(chipSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(C));
//                 fChipSummaryContainerVector.emplace_back(chipSummaryContainer);
//                 bufferReadingPosition += sizeof(C);
//             }
//         }
//         if(fContainerCarried.isChannelContainerCarried())
//         {
//             size_t vectorSize = (fDataSize - bufferReadingPosition) / (sizeof(T) * fNumberOfChips);
//             std::cout << __PRETTY_FUNCTION__ << "vectorSize = " << +vectorSize << std::endl;

//             for(size_t chipIndex = 0; chipIndex < fNumberOfChips; ++chipIndex)
//             {
//                 ChannelContainer<T>* channelContainer = new ChannelContainer<T>(vectorSize);
//                 memcpy(&channelContainer->at(0), &bufferBegin[bufferReadingPosition], vectorSize * sizeof(T));
//                 fChannelContainerVector.emplace_back(channelContainer);
//                 bufferReadingPosition += vectorSize * sizeof(T);
//             }
//         }
//     }

//   private:
//     uint32_t sizeOfChannelContainer()
//     {
//         if(fChannelContainerVectorVector.size() == 0) return 0;

//         uint32_t sizeOfChannelContainer = 0;
//         for(auto& channelContainerVector : fChannelContainerVectorVector)
//             for(auto& channelContainer : channelContainerVector)
//                 sizeOfChannelContainer += channelContainer->size() * sizeof(T));
//         return sizeOfChannelContainer;
//     }

//     uint32_t sizeOfChipContainer()
//     {
//         if(fChipSummaryContainerVector.size() == 0) return 0;

//         uint32_t sizeOfChipContainer = 0;
//         for(auto chipContainerVector : fChipSummaryContainerVectorVector) sizeOfChipContainer += chipContainerVector.size() * sizeof(C);

//         std::cout << __PRETTY_FUNCTION__ << " Chip vector size = " << +fChipSummaryContainerVector.size() << std::endl;
//         std::cout << __PRETTY_FUNCTION__ << " Chip summary size = " << +sizeof(C) << std::endl;
//         std::cout << __PRETTY_FUNCTION__ << " Total size = " << +sizeOfChipContainer.size() * sizeof(C) << std::endl;

//         return sizeOfChipContainer;
//     }

//     uint32_t sizeOfHybridContainer()
//     {
//         if(fChipSummaryContainerVector.size() == 0) return 0;
//         return fChipSummaryContainerVector.size() * sizeof(H);
//     }

//     uint32_t sizeOfOpticalContainer()
//     {
//         if(fChipSummaryContainerVector.size() == 0) return 0;
//         return fChipSummaryContainerVector.size() * sizeof(H);
//     }

//   public:
//     ContainerCarried                               fContainerCarried;
//     std::vector<uint8_t>                           fNumberOfChipVector;
//     uint8_t                                        fNumberOfHybrids;
//     std::vector<std::vector<ChannelContainer<T>*>> fChannelContainerVectorVector;
//     std::vector<std::vector<Summary<C, T>*>>       fChipSummaryContainerVectorVector;
//     std::vector<Summary<M, C>*>                    fHybridSummaryContainerVector;
//     Summary<O, M>                                  fOpticalGroupSummaryContainer;
// };

// template <typename T, typename C, typename M, typename O, typename... I>
// class OpticalGroupContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T, C, M, O>>
// {
//     enum HeaderId
//     {
//         OpticalGroupId,
//         HybridId
//     };
//     static constexpr size_t getEnumSize() { return HybridId + 1; }

//   public:
//     OpticalGroupContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T, C, M, O>>(creatorName) { ; }
//     ~OpticalGroupContainerStream() { ; }

//     void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
//     {
//         for(auto opticalGroup: *board)
//         {
//             for(auto hybrid: *opticalGroup)
//             {
//                 retrieveHybridData(board->getId(), opticalGroup->getId(), hybrid);
//                 const std::vector<char>& stream = this->encodeStream();
//                 this->incrementStreamPacketNumber();
//                 networkStreamer->broadcast(stream);
//             }
//         }
//     }

//     void decodeHybridData(DetectorDataContainer& detectorContainer)
//     {
//         uint16_t boardId        = this->fHeaderStream.fBoardId;
//         uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();
//         uint16_t hybridId       = this->fHeaderStream.template getHeaderInfo<HeaderId::HybridId>();

//         detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->setSummaryContainer(this->fDataStream.fHybridSummaryContainer);
//         this->fDataStream.fHybridSummaryContainer = nullptr;

//         for(auto chip: *detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId))
//         {
//             if(this->fDataStream.fContainerCarried.isChipContainerCarried())
//             {
//                 chip->setSummaryContainer(this->fDataStream.fChipSummaryContainerVector.getObject(chip->getId()));
//                 this->fDataStream.fChipSummaryContainerVector.getObject(chip->getId()) = nullptr;
//             }
//             if(this->fDataStream.fContainerCarried.isChannelContainerCarried())
//             {
//                 chip->setChannelContainer(this->fDataStream.fChannelContainerVector.getObject(chip->getId()));
//                 this->fDataStream.fChannelContainerVector.getObject(chip->getId()) = nullptr;
//             }
//         }
//     }

//     template <std::size_t N>
//     using TupleElementType = typename std::tuple_element<N, std::tuple<I...>>::type;

//     template <std::size_t N = 0>
//     void setHeaderElement(TupleElementType<N> theInfo)
//     {
//         this->fHeaderStream.template setHeaderInfo<N + getEnumSize()>(theInfo);
//     }

//     template <std::size_t N = 0>
//     TupleElementType<N> getHeaderElement() const
//     {
//         return this->fHeaderStream.template getHeaderInfo<N + getEnumSize()>();
//     }

//   protected:
//     void retrieveHybridData(uint16_t boardId, uint16_t opticalGroupId, HybridDataContainer* hybrid)
//     {
//         this->fHeaderStream.fBoardId = boardId;
//         this->fHeaderStream.template setHeaderInfo<HeaderId::OpticalGroupId>(opticalGroupId);
//         this->fHeaderStream.template setHeaderInfo<HeaderId::HybridId>(hybrid->getId());
//         this->fDataStream.fNumberOfChips = hybrid->size();
//         this->fDataStream.fChipSummaryContainerVector.clear();
//         this->fDataStream.fChannelContainerVector.clear();
//         if(hybrid->getSummaryContainer<M, C>() != nullptr)
//         {
//             this->fDataStream.fContainerCarried.carryHybridContainer();
//             this->fDataStream.fHybridSummaryContainer = hybrid->getSummaryContainer<M, C>();
//         }
//         for(auto chip: *hybrid)
//         {
//             if(chip->getSummaryContainer<C, T>() != nullptr)
//             {
//                 this->fDataStream.fContainerCarried.carryChipContainer();
//                 this->fDataStream.fChipSummaryContainerVector.emplace_back(chip->getSummaryContainer<C, T>());
//             }
//             if(chip->getChannelContainer<T>() != nullptr)
//             {
//                 this->fDataStream.fContainerCarried.carryChannelContainer();
//                 this->fDataStream.fChannelContainerVector.emplace_back(chip->getChannelContainer<T>());
//             }
//         }
//     }
// };

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
