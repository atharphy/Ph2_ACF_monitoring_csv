/*

        \file                          OpticalGroupContainerStream.h
        \brief                         OpticalGroupContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __OPTICALGROUPCONTAINERSTREAM_H__
#define __OPTICALGROUPCONTAINERSTREAM_H__
// pointers to base class
#include "../HWDescription/ReadoutChip.h"
#include "../HWDescription/OpticalGroup.h"
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

    DataStreamContainer(DataStreamContainer&& theOriginalStream)
    : DataStreamBase(std::move(theOriginalStream))
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        for(auto& fDataSteamSubContainer : theOriginalStream.fDataSteamSubContainerMap)
        {
            fDataSteamSubContainerMap[fDataSteamSubContainer.first] = std::move(fDataSteamSubContainer.second);
        }
        fNumberOfSubContainers = theOriginalStream.fNumberOfSubContainers;
        fSummaryContainer = theOriginalStream.fSummaryContainer;
        theOriginalStream.fSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;
    }

    DataStreamContainer& operator=(const DataStreamContainer&) = delete;

    DataStreamContainer& operator=(DataStreamContainer&& theOriginalStream)
    {
        fContainerCarried = theOriginalStream.fContainerCarried;
        fDataSteamSubContainerMap.clear();
        for(auto& fDataSteamSubContainer : theOriginalStream.fDataSteamSubContainerMap)
        {
            fDataSteamSubContainerMap[fDataSteamSubContainer.first] = std::move(fDataSteamSubContainer.second);
        }
        fNumberOfSubContainers = theOriginalStream.fNumberOfSubContainers;
        delete fSummaryContainer;
        fSummaryContainer = theOriginalStream.fSummaryContainer;
        theOriginalStream.fSummaryContainer = nullptr;
        fDeletePointers = theOriginalStream.fDeletePointers;

        return *this;
    }

    uint32_t size(void) override
    {
        fDataSize = sizeof(fDataSize) + sizeof(fContainerCarried) + sizeof(fNumberOfSubContainers);
        for(auto& dataSteamSubContainer: fDataSteamSubContainerMap) 
        {
            fDataSize += (sizeof(uint16_t) + dataSteamSubContainer.second.size());
        }
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
            fSummaryContainer = new Summary<This,Sub>();
            memcpy(&(fSummaryContainer->theSummary_), &bufferBegin[bufferReadingPosition], sizeof(This));
            bufferReadingPosition += sizeof(This);
        }

        for(uint8_t subContainerIndex = 0; subContainerIndex<fNumberOfSubContainers; ++subContainerIndex)
        {
            uint16_t subContainerId = 65535;
            memcpy(&subContainerId, &bufferBegin[bufferReadingPosition], sizeof(uint16_t));
            bufferReadingPosition += sizeof(uint16_t);

            SubStream<Args..., Sub> theDataSteamSubContainer;
            theDataSteamSubContainer.fContainerCarried = fContainerCarried;
            bufferReadingPosition = theDataSteamSubContainer.copyFromStream(bufferBegin, bufferReadingPosition);
            fDataSteamSubContainerMap[subContainerId] = std::move(theDataSteamSubContainer);
        }
        
        return bufferReadingPosition;
    }

  public:
    ContainerCarried                            fContainerCarried{};
    std::map<uint16_t, SubStream<Args..., Sub>> fDataSteamSubContainerMap{};
    uint8_t                                     fNumberOfSubContainers{0};
    Summary<This,Sub>*                          fSummaryContainer{nullptr};
    bool                                        fDeletePointers{false};
};

// ------------------------------------------- OpticalGroupContainerStream ------------------------------------------- //

template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamContainer<3, O, H, DataStreamHybridContainer, T, C>>
{
    enum HeaderId
    {
        BoardId,
        OpticalGroupId
    };
    static constexpr size_t getEnumSize() { return OpticalGroupId + 1; }

  public:
    OpticalGroupContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamContainer<3, O, H, DataStreamHybridContainer, T, C>>(creatorName) {}
    OpticalGroupContainerStream(OpticalGroupContainerStream<T, C, H, O, I...>&& theContainerStream)
    : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamContainer<3, O, H, DataStreamHybridContainer, T, C, H>>(std::move(theContainerStream))
    {}
    OpticalGroupContainerStream(const OpticalGroupContainerStream<T, C, H, O, I...>&) = delete;
    ~OpticalGroupContainerStream() 
    { 
    }

    void streamAndSendBoard(const BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {

            retrieveData(board->getId(), opticalGroup);
            auto stream = this->encodeStream();
            this->incrementStreamPacketNumber();
            networkStreamer->broadcast(*stream.get());
        }
    }

    void decodeData(DetectorDataContainer& detectorContainer)
    {
        uint16_t boardId        = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();
        uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();

        if(this->fDataStream.fContainerCarried.isOpticalGroupContainerCarried())
        {
            detectorContainer.getObject(boardId)->getObject(opticalGroupId)->setSummaryContainer(this->fDataStream.fSummaryContainer);
            this->fDataStream.fSummaryContainer = nullptr;
        }

        for(auto hybrid: *detectorContainer.getObject(boardId)->getObject(opticalGroupId))
        {
            HybridContainerStream<T, C, H> theHybridStreamer(this->fCreatorName);
            theHybridStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::BoardId>(boardId);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::OpticalGroupId>(boardId);
            theHybridStreamer.fHeaderStream.template setHeaderInfo<HybridContainerStream<T, C, H>::HeaderId::HybridId>(hybrid->getId());
            theHybridStreamer.fDataStream = std::move(this->fDataStream.fDataSteamSubContainerMap.at(hybrid->getId()));
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
        this->fDataStream.fDataSteamSubContainerMap.clear();
        if(opticalGroup->getSummaryContainer<O, H>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryOpticalGroupContainer();
            this->fDataStream.fSummaryContainer = opticalGroup->getSummaryContainer<O, H>();
        }
        // std::vector<HybridContainerStream<T, C, H>> theHybridStreamerVector; 
        for(auto hybrid: *opticalGroup)
        {
            // theHybridStreamerVector.emplace_back(HybridContainerStream<T, C, H>(this->fCreatorName));
            // HybridContainerStream<T, C, H> &theHybridStreamer = theHybridStreamerVector.back();

            HybridContainerStream<T, C, H> theHybridStreamer(this->fCreatorName);
            theHybridStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theHybridStreamer.retrieveHybridData(boardId, opticalGroup->getId(), hybrid);
            this->fDataStream.fContainerCarried.fContainerCarried |= theHybridStreamer.getContainerCarried().fContainerCarried;
            this->fDataStream.fDataSteamSubContainerMap[hybrid->getId()] = std::move(theHybridStreamer.fDataStream);
        }
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
