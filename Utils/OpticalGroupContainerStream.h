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
#include "../Utils/HybridContainerStream.h"
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

// ------------------------------------------- OpticalGroupContainerStream ------------------------------------------- //
template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream;

template<typename T, typename C, typename H, typename O>
using DataStreamOpticalGroupContainer = DataStreamContainer<3, O, H, DataStreamHybridContainer, T, C>;

template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T,C,H,O>>
{
    template <typename T1, typename C1, typename H1, typename O1, typename B, typename... I1>
    friend class BoardContainerStream;

    enum HeaderId
    {
        BoardId,
        OpticalGroupId
    };
    static constexpr size_t getEnumSize() { return OpticalGroupId + 1; }

  public:
    OpticalGroupContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T,C,H,O>>(creatorName) {}
    OpticalGroupContainerStream(OpticalGroupContainerStream<T, C, H, O, I...>&& theContainerStream)
    : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, I...>, DataStreamOpticalGroupContainer<T,C,H,O>>(std::move(theContainerStream))
    {}
    OpticalGroupContainerStream(const OpticalGroupContainerStream<T, C, H, O, I...>&) = delete;
    ~OpticalGroupContainerStream() 
    { 
    }

    void setContainerCarried(const ContainerCarried& theContainerCarried) { this->fDataStream.fContainerCarried = theContainerCarried; }
    ContainerCarried getContainerCarried() const { return this->fDataStream.fContainerCarried; }

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
            theHybridStreamer.decodeData(detectorContainer);
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
        
        for(auto hybrid: *opticalGroup)
        {
            HybridContainerStream<T, C, H> theHybridStreamer(this->fCreatorName);
            theHybridStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theHybridStreamer.retrieveData(boardId, opticalGroup->getId(), hybrid);
            this->fDataStream.fContainerCarried.fContainerCarried |= theHybridStreamer.getContainerCarried().fContainerCarried;
            this->fDataStream.fDataSteamSubContainerMap[hybrid->getId()] = std::move(theHybridStreamer.fDataStream);
        }
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
