/*

        \file                          BoardContainerStream.h
        \brief                         BoardContainerStream for DAQ
        \author                        Fabio Ravera, Lorenzo Uplegger
        \version                       1.0
        \date                          14/07/19
        Support :                      mail to : fabio.ravera@cern.ch

 */

#ifndef __BOARDCONTAINERSTREAM_H__
#define __BOARDCONTAINERSTREAM_H__
// pointers to base class
#include "HWDescription/OpticalGroup.h"
#include "HWDescription/ReadoutChip.h"
#include "NetworkUtils/TCPPublishServer.h"
#include "Utils/ContainerStream.h"
#include "Utils/ObjectStream.h"
#include "Utils/OpticalGroupContainerStream.h"
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

// ------------------------------------------- BoardContainerStream ------------------------------------------- //
template <typename T, typename C, typename H, typename O, typename B, typename... I>
class BoardContainerStream;

template <typename T, typename C, typename H, typename O, typename B>
using DataStreamBoardContainer = DataStreamContainer<4, B, O, DataStreamOpticalGroupContainer, T, C, H>;

template <typename T, typename C, typename H, typename O, typename B, typename... I>
class BoardContainerStream : public ObjectStream<HeaderStreamContainer<uint16_t, I...>, DataStreamBoardContainer<T, C, H, O, B>>
{
    enum HeaderId
    {
        BoardId
    };
    static constexpr size_t getEnumSize() { return BoardId + 1; }

  public:
    BoardContainerStream(const std::string& creatorName) : ObjectStream<HeaderStreamContainer<uint16_t, I...>, DataStreamBoardContainer<T, C, H, O, B>>(creatorName) {}
    BoardContainerStream(BoardContainerStream&& theContainerStream) : ObjectStream<HeaderStreamContainer<uint16_t, I...>, DataStreamBoardContainer<T, C, H, O, B>>(std::move(theContainerStream)) {}
    BoardContainerStream(const BoardContainerStream&) = delete;
    ~BoardContainerStream() {}

    void streamAndSendBoard(BoardDataContainer* board, TCPPublishServer* networkStreamer)
    {
        this->retrieveData(board);
        auto stream = this->encodeStream();
        this->incrementStreamPacketNumber();
        networkStreamer->broadcast(*stream.get());
    }

    void decodeData(DetectorDataContainer& detectorContainer)
    {
        uint16_t boardId = this->fHeaderStream.template getHeaderInfo<HeaderId::BoardId>();

        if(this->fDataStream.fContainerCarried.isBoardContainerCarried())
        {
            detectorContainer.getObject(boardId)->setSummaryContainer(this->fDataStream.fSummaryContainer);
            this->fDataStream.fSummaryContainer = nullptr;
        }

        for(auto opticalGroup: *detectorContainer.getObject(boardId))
        {
            OpticalGroupContainerStream<T, C, H, O> theOpticalGroupStreamer(this->fCreatorName);
            theOpticalGroupStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theOpticalGroupStreamer.fHeaderStream.template setHeaderInfo<OpticalGroupContainerStream<T, C, H, O>::HeaderId::BoardId>(boardId);
            theOpticalGroupStreamer.fHeaderStream.template setHeaderInfo<OpticalGroupContainerStream<T, C, H, O>::HeaderId::OpticalGroupId>(opticalGroup->getId());
            theOpticalGroupStreamer.fDataStream = std::move(this->fDataStream.fDataSteamSubContainerMap.at(opticalGroup->getId()));
            theOpticalGroupStreamer.decodeData(detectorContainer);
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

    void retrieveData(BoardDataContainer* board)
    {
        this->fHeaderStream.template setHeaderInfo<HeaderId::BoardId>(board->getId());
        this->fDataStream.fNumberOfSubContainers = board->size();
        this->fDataStream.fDataSteamSubContainerMap.clear();
        if(board->getSummaryContainer<B, O>() != nullptr)
        {
            this->fDataStream.fContainerCarried.carryBoardContainer();
            this->fDataStream.fSummaryContainer = board->getSummaryContainer<B, O>();
        }

        for(auto opticalGroup: *board)
        {
            OpticalGroupContainerStream<T, C, H, O> theOpticalGroupStreamer(this->fCreatorName);
            theOpticalGroupStreamer.setContainerCarried(this->fDataStream.fContainerCarried);
            theOpticalGroupStreamer.retrieveData(board->getId(), opticalGroup);
            this->fDataStream.fContainerCarried.fContainerCarried |= theOpticalGroupStreamer.getContainerCarried().fContainerCarried;
            this->fDataStream.fDataSteamSubContainerMap[opticalGroup->getId()] = std::move(theOpticalGroupStreamer.fDataStream);
        }
    }
};

#if defined(__GNUC__) && !defined(__INTEL_COMPILER) && (((__GNUC__ * 100) + __GNUC_MINOR__) >= 800)
#pragma GCC diagnostic pop
#endif
#endif
