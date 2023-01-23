/*

        \file                          HybridContainerStream.h
        \brief                         HybridContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __HybridCONTAINERSTREAM_H__
#define __HybridCONTAINERSTREAM_H__
// pointers to base class
#include "HWDescription/Hybrid.h"
#include "HWDescription/ReadoutChip.h"
#include "NetworkUtils/TCPPublishServer.h"
#include "Utils/ChipContainerStream.h"
#include "Utils/ContainerStream.h"
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

// ------------------------------------------- HybridContainerStream ------------------------------------------- //

template <typename T, typename C, typename H, typename O, typename... I>
class OpticalGroupContainerStream;

template <typename T, typename C, typename H>
using DataStreamHybridContainer = DataStreamContainer<2, H, C, DataStreamChipContainer, T>;

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
    HybridContainerStream(HybridContainerStream&& theHybridContainerStream)
        : ObjectStream<HeaderStreamContainer<uint16_t, uint16_t, uint16_t, I...>, DataStreamHybridContainer<T, C, H>>(std::move(theHybridContainerStream))
    {
    }
    HybridContainerStream(const HybridContainerStream&) = delete;
    ~HybridContainerStream() { ; }

    void             setContainerCarried(const ContainerCarried& theContainerCarried) { this->fDataStream.fContainerCarried = theContainerCarried; }
    ContainerCarried getContainerCarried() const { return this->fDataStream.fContainerCarried; }

    void streamAndSendBoard(const BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                retrieveData(board->getId(), opticalGroup->getId(), hybrid);
                auto stream = this->encodeStream();
                this->incrementStreamPacketNumber();
                networkStreamer->broadcast(*stream.get());
            }
        }
    }

    void decodeData(DetectorDataContainer& detectorContainer)
    {
        uint16_t boardId        = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();
        uint16_t opticalGroupId = this->fHeaderStream.template getHeaderInfo<HeaderId::OpticalGroupId>();
        uint16_t hybridId       = this->fHeaderStream.template getHeaderInfo<HeaderId::HybridId>();

        if(this->fDataStream.fContainerCarried.isHybridContainerCarried())
        {
            detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->setSummaryContainer(this->fDataStream.fSummaryContainer);
            this->fDataStream.fSummaryContainer = nullptr;
        }

        for(auto chip: *detectorContainer.getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId))
        {
            ChipContainerStream<T, C> theChipStreamer(this->fCreatorName);
            theChipStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theChipStreamer.fHeaderStream.template setHeaderInfo<ChipContainerStream<T, C>::HeaderId::BoardId>(boardId);
            theChipStreamer.fHeaderStream.template setHeaderInfo<ChipContainerStream<T, C>::HeaderId::OpticalGroupId>(opticalGroupId);
            theChipStreamer.fHeaderStream.template setHeaderInfo<ChipContainerStream<T, C>::HeaderId::HybridId>(hybridId);
            theChipStreamer.fHeaderStream.template setHeaderInfo<ChipContainerStream<T, C>::HeaderId::ChipId>(chip->getId());
            theChipStreamer.fDataStream = std::move(this->fDataStream.fDataSteamSubContainerMap.at(chip->getId()));
            theChipStreamer.decodeChipData(detectorContainer);
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

    void retrieveData(uint16_t boardId, uint16_t opticalGroupId, HybridDataContainer* hybrid)
    {
        this->fHeaderStream.template setHeaderInfo<HeaderId::BoardId>(boardId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::OpticalGroupId>(opticalGroupId);
        this->fHeaderStream.template setHeaderInfo<HeaderId::HybridId>(hybrid->getId());
        this->fDataStream.fNumberOfSubContainers = hybrid->size();
        if(hybrid->getSummaryContainer<H, C>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryHybridContainer();
            this->fDataStream.fSummaryContainer = hybrid->getSummaryContainer<H, C>();
        }
        for(auto chip: *hybrid)
        {
            ChipContainerStream<T, C, H> theChipStreamer(this->fCreatorName);
            theChipStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theChipStreamer.retrieveChipData(boardId, opticalGroupId, hybrid->getId(), chip);
            this->fDataStream.fContainerCarried.fContainerCarried |= theChipStreamer.getContainerCarried().fContainerCarried;
            this->fDataStream.fDataSteamSubContainerMap[chip->getId()] = std::move(theChipStreamer.fDataStream);
        }
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
