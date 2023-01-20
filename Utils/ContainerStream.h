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
#include "HWDescription/ReadoutChip.h"
#include "NetworkUtils/TCPPublishServer.h"
#include "Utils/DataContainer.h"
#include "Utils/ObjectStream.h"
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

    void carryChannelContainer() { carryCarried<0>(); }
    void carryChipContainer() { carryCarried<1>(); }
    void carryHybridContainer() { carryCarried<2>(); }
    void carryOpticalGroupContainer() { carryCarried<3>(); }
    void carryBoardContainer() { carryCarried<4>(); }

    bool isChannelContainerCarried() { return isContainerCarried<0>(); }
    bool isChipContainerCarried() { return isContainerCarried<1>(); }
    bool isHybridContainerCarried() { return isContainerCarried<2>(); }
    bool isOpticalGroupContainerCarried() { return isContainerCarried<3>(); }
    bool isBoardContainerCarried() { return isContainerCarried<4>(); }

    template <uint8_t N>
    bool isContainerCarried()
    {
        return (fContainerCarried >> N) & 1;
    }

    template <uint8_t N>
    void carryCarried()
    {
        fContainerCarried |= (1 << N);
    }

    uint8_t fContainerCarried;
} __attribute__((packed));

template <typename H>
class HeaderStreamContainerBase : public DataStreamBase
{
  public:
    HeaderStreamContainerBase(){};
    ~HeaderStreamContainerBase(){};
    HeaderStreamContainerBase(HeaderStreamContainerBase&&)      = default;
    HeaderStreamContainerBase(const HeaderStreamContainerBase&) = delete;

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
    HeaderStreamContainer(HeaderStreamContainer<I...>&& theHeaderStreamContainer) : HeaderStreamContainerBase<HeaderStreamContainer<I...>>(std::move(theHeaderStreamContainer)) {}
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

// ------------------------------------------- DataStreamContainer ------------------------------------------- //

template <uint8_t Layer, typename This, typename Sub, template <typename, typename...> class SubStream, typename... Args>
class DataStreamContainer : public DataStreamBase
{
  public:
    DataStreamContainer() : fSummaryContainer(nullptr) { check_if_retrivable<This>(); }
    ~DataStreamContainer()
    {
        if(fDeletePointers)
        {
            fDataSteamSubContainerMap.clear();
            if(fSummaryContainer == nullptr)
            {
                delete fSummaryContainer;
                fSummaryContainer = nullptr;
            }
        }
    }

    DataStreamContainer(const DataStreamContainer&) = delete;

    DataStreamContainer(DataStreamContainer&& theOriginalStream) : DataStreamBase(std::move(theOriginalStream))
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        for(auto& fDataSteamSubContainer: theOriginalStream.fDataSteamSubContainerMap) { fDataSteamSubContainerMap[fDataSteamSubContainer.first] = std::move(fDataSteamSubContainer.second); }
        fNumberOfSubContainers              = theOriginalStream.fNumberOfSubContainers;
        fSummaryContainer                   = theOriginalStream.fSummaryContainer;
        theOriginalStream.fSummaryContainer = nullptr;
        fDeletePointers                     = theOriginalStream.fDeletePointers;
    }

    DataStreamContainer& operator=(const DataStreamContainer&) = delete;

    DataStreamContainer& operator=(DataStreamContainer&& theOriginalStream)
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        fDataSteamSubContainerMap.clear();
        for(auto& fDataSteamSubContainer: theOriginalStream.fDataSteamSubContainerMap) { fDataSteamSubContainerMap[fDataSteamSubContainer.first] = std::move(fDataSteamSubContainer.second); }
        fNumberOfSubContainers = theOriginalStream.fNumberOfSubContainers;
        delete fSummaryContainer;
        fSummaryContainer                   = theOriginalStream.fSummaryContainer;
        theOriginalStream.fSummaryContainer = nullptr;
        fDeletePointers                     = theOriginalStream.fDeletePointers;

        return *this;
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried) + sizeof(fNumberOfSubContainers);
        for(auto& dataSteamSubContainer: fDataSteamSubContainerMap) { fDataSize += (sizeof(uint16_t) + dataSteamSubContainer.second.size()); }
        if(fSummaryContainer != nullptr) { fDataSize += sizeof(This); }
        return fDataSize;
    }

    size_t copyToStream(char* bufferBegin, size_t bufferWritingPosition = 0)
    {
        memcpy(&bufferBegin[bufferWritingPosition], &fDataSize, sizeof(fDataSize));
        bufferWritingPosition += sizeof(fDataSize);

        memcpy(&bufferBegin[bufferWritingPosition], &fContainerCarried, sizeof(fContainerCarried));
        bufferWritingPosition += sizeof(fContainerCarried);

        memcpy(&bufferBegin[bufferWritingPosition], &fNumberOfSubContainers, sizeof(fNumberOfSubContainers));
        bufferWritingPosition += sizeof(fNumberOfSubContainers);

        if(fContainerCarried.isContainerCarried<Layer>())
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(fSummaryContainer->theSummary_), sizeof(This));
            bufferWritingPosition += sizeof(This);
            fSummaryContainer = nullptr;
        }

        for(auto& subContainer: fDataSteamSubContainerMap)
        {
            memcpy(&bufferBegin[bufferWritingPosition], &(subContainer.first), sizeof(uint16_t));
            bufferWritingPosition += sizeof(uint16_t);

            bufferWritingPosition = subContainer.second.copyToStream(bufferBegin, bufferWritingPosition);
        }

        return bufferWritingPosition;
    }

    size_t copyFromStream(const char* bufferBegin, size_t bufferReadingPosition = 0)
    {
        fDeletePointers = true;

        memcpy(&fDataSize, &bufferBegin[bufferReadingPosition], sizeof(fDataSize));
        bufferReadingPosition += sizeof(fDataSize);

        memcpy(&fContainerCarried, &bufferBegin[bufferReadingPosition], sizeof(fContainerCarried));
        bufferReadingPosition += sizeof(fContainerCarried);

        memcpy(&fNumberOfSubContainers, &bufferBegin[bufferReadingPosition], sizeof(fNumberOfSubContainers));
        bufferReadingPosition += sizeof(fNumberOfSubContainers);

        if(fContainerCarried.isContainerCarried<Layer>())
        {
            fSummaryContainer = new Summary<This, Sub>();
            memcpy(&(fSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(This));
            bufferReadingPosition += sizeof(This);
        }

        for(uint8_t subContainerIndex = 0; subContainerIndex < fNumberOfSubContainers; ++subContainerIndex)
        {
            uint16_t subContainerId = 65535;
            memcpy(&subContainerId, &bufferBegin[bufferReadingPosition], sizeof(uint16_t));
            bufferReadingPosition += sizeof(uint16_t);

            SubStream<Args..., Sub> theDataSteamSubContainer;
            theDataSteamSubContainer.fContainerCarried = fContainerCarried;
            bufferReadingPosition                      = theDataSteamSubContainer.copyFromStream(bufferBegin, bufferReadingPosition);
            fDataSteamSubContainerMap[subContainerId]  = std::move(theDataSteamSubContainer);
        }

        return bufferReadingPosition;
    }

  public:
    ContainerCarried                            fContainerCarried{};
    std::map<uint16_t, SubStream<Args..., Sub>> fDataSteamSubContainerMap{};
    uint8_t                                     fNumberOfSubContainers{0};
    Summary<This, Sub>*                         fSummaryContainer{nullptr};
    bool                                        fDeletePointers{false};
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
