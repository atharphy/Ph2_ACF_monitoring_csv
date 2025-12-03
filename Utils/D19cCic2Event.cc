/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "Utils/D19cCic2Event.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "Utils/ChannelGroupHandler.h"
#include "Utils/ContainerFactory.h"
#include "Utils/DataContainer.h"
#include "Utils/EmptyContainer.h"
#include "Utils/Occupancy.h"
#include "Utils/Utilities.h"
#include <cstdint>

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{

bool Cluster2SHandler::parseData(uint32_t data)
{
    fCluster2S.fAddress = ((data >> 3) & 0xFF) - 1;
    fCluster2S.fWidth   = (data & 0x7) + 1;
    return fCluster2S.fAddress < NCHANNELS;
}

bool Cluster2SHandler::isChannelHit(uint8_t channel) const { return channel >= fCluster2S.fAddress && channel < (fCluster2S.fAddress + fCluster2S.fWidth); }

void Cluster2SHandler::print() const { std::cout << "First strip = " << +fCluster2S.fAddress << " cluster width = " << +fCluster2S.fWidth << std::endl; }

bool PixelClusterPSHandler::parseData(uint32_t data)
{
    fPixelClusterPS.fAddress = ((data >> 7) & 0x7F) - 1;
    fPixelClusterPS.fWidth   = ((data >> 4) & 0x7) + 1;
    fPixelClusterPS.fZpos    = data & 0xF;
    return fPixelClusterPS.fAddress < NSSACHANNELS;
}

bool PixelClusterPSHandler::isChannelHit(uint8_t row, uint8_t col) const
{
    return col >= fPixelClusterPS.fAddress && col < (fPixelClusterPS.fAddress + fPixelClusterPS.fWidth) && row == fPixelClusterPS.fZpos;
}

void PixelClusterPSHandler::print() const
{
    std::cout << "First pixel row = " << +fPixelClusterPS.fZpos << " col = " << +fPixelClusterPS.fAddress << " cluster width = " << +fPixelClusterPS.fWidth << std::endl;
}

bool StripClusterPSHandler::parseData(uint32_t data)
{
    fStripClusterPS.fAddress = ((data >> 4) & 0x7F) - 1;
    fStripClusterPS.fWidth   = ((data >> 1) & 0x7) + 1;
    fStripClusterPS.fIsHIP   = (data & 0x1) == 0x1;
    return fStripClusterPS.fAddress < NSSACHANNELS;
}

bool StripClusterPSHandler::isChannelHit(uint8_t col) const { return col >= fStripClusterPS.fAddress && col < (fStripClusterPS.fAddress + fStripClusterPS.fWidth); }

void StripClusterPSHandler::print() const { std::cout << "First strip = " << +fStripClusterPS.fAddress << " cluster width = " << +fStripClusterPS.fWidth << std::endl; }

void HybridL1EventInfoHandler::parseData(std::vector<uint32_t>::const_iterator dataStart, bool is2S)
{
    fHybridL1EventInfo.fErrorCode  = (*(dataStart) >> 24) & 0xF;
    fHybridL1EventInfo.fHybridId   = (*(dataStart) >> 16) & 0xFF;
    fHybridL1EventInfo.fChipId     = (*(dataStart) >> 12) & 0xF;
    fHybridL1EventInfo.fChipType   = (*(dataStart + 1) >> 12) & 0xF;
    fHybridL1EventInfo.fFrameDelay = *(dataStart + 1) & 0xFFF;
    fHybridL1EventInfo.fStatusBits = *(dataStart + 2) >> 23;
    fHybridL1EventInfo.fL1counter  = (*(dataStart + 2) >> 14) & 0x1FF;
    if(is2S)
    {
        fHybridL1EventInfo.fNumberOfStripClusters = (*(dataStart + 2)) & 0x7F;
        fHybridL1EventInfo.fNumberOfPixelClusters = 0;
    }
    else
    {
        fHybridL1EventInfo.fNumberOfStripClusters = (*(dataStart + 2) >> 7) & 0x7F;
        fHybridL1EventInfo.fNumberOfPixelClusters = *(dataStart + 2) & 0x7F;
    }
    fHybridL1EventInfo.fIsCICErrorFlagSet = ((fHybridL1EventInfo.fStatusBits & 0x1) == 0x1);
}

void HybridL1EventInfoHandler::print() const
{
    std::cout << "ErrorCode             = " << +fHybridL1EventInfo.fErrorCode << std::endl;
    std::cout << "HybridId              = " << +fHybridL1EventInfo.fHybridId << std::endl;
    std::cout << "ChipId                = " << +fHybridL1EventInfo.fChipId << std::endl;
    std::cout << "ChipType              = " << +fHybridL1EventInfo.fChipType << std::endl;
    std::cout << "FrameDelay            = " << +fHybridL1EventInfo.fFrameDelay << std::endl;
    std::cout << "StatusBits            = " << +fHybridL1EventInfo.fStatusBits << std::endl;
    std::cout << "L1counter             = " << +fHybridL1EventInfo.fL1counter << std::endl;
    std::cout << "NumberOfStripClusters = " << +fHybridL1EventInfo.fNumberOfStripClusters << std::endl;
    std::cout << "NumberOfPixelClusters = " << +fHybridL1EventInfo.fNumberOfPixelClusters << std::endl;
}

void CBCL1EventInfoHandler::parseData(std::array<uint32_t, NUMBER_OF_CIC_PORTS * 9>::const_iterator dataStart, size_t bitStart)
{
    fCBCL1EventInfo.fErrorCode       = getWord<2, 0x3>(dataStart, bitStart);
    fCBCL1EventInfo.fPipelineAddress = getWord<9, 0x1FF>(dataStart, bitStart + 2);
    fCBCL1EventInfo.fL1id            = getWord<9, 0x1FF>(dataStart, bitStart + 11);
    for(size_t wordIndex = 0; wordIndex < 7; ++wordIndex) { fCBCL1EventInfo.fRawData[wordIndex] = getWord<32, 0xFFFFFFFF>(dataStart, bitStart + 20 + (32 * wordIndex)); }
    fCBCL1EventInfo.fRawData[7] = getWord<30, 0x3FFFFFFF>(dataStart, bitStart + 20 + 32 * 7) << 2;
}

bool CBCL1EventInfoHandler::isChannelHit(uint8_t channel) const { return (fCBCL1EventInfo.fRawData.at(channel / 32) >> (31 - channel % 32)) & 0x1; }

void CBCL1EventInfoHandler::print() const
{
    std::cout << "ErrorCode             = " << +fCBCL1EventInfo.fErrorCode << std::endl;
    std::cout << "PipelineAddress       = " << +fCBCL1EventInfo.fPipelineAddress << std::endl;
    std::cout << "L1id                  = " << +fCBCL1EventInfo.fL1id << std::endl;
    std::vector<uint32_t> rawDataVector(fCBCL1EventInfo.fRawData.begin(), fCBCL1EventInfo.fRawData.end());
    std::cout << "RawData               = " << getPatternPrintout(rawDataVector, 1) << std::endl;
}

std::vector<uint8_t> CBCL1EventInfoHandler::getChannelHitList() const
{
    std::vector<uint8_t> theHitList;
    theHitList.reserve(NCHANNELS);

    for(size_t channel = 0; channel < NCHANNELS; ++channel)
    {
        if(isChannelHit(channel)) theHitList.emplace_back(channel);
    }
    return theHitList;
}

uint8_t CBCL1EventInfoHandler::countNumberOfHits() const
{
    uint8_t numberOfHits = 0;
    for(auto hitWord: fCBCL1EventInfo.fRawData) numberOfHits += __builtin_popcount(hitWord);
    return numberOfHits;
}

void HybridStubEventInfo::parseData(std::vector<uint32_t>::const_iterator dataStart)
{
    fStubDataDelay   = (*(dataStart) >> 12) & 0xFFF;
    fStatusBits      = (*(dataStart + 1) >> 22) & 0x1FF;
    fNumberOfStubs   = (*(dataStart + 1) >> 16) & 0x3F;
    fBunchCrossingId = *(dataStart + 1) & 0xFFF;
}

void HybridStubEventInfo::print() const
{
    std::cout << "StubDataDelay   = " << +fStubDataDelay << std::endl;
    std::cout << "StatusBits      = " << +fStatusBits << std::endl;
    std::cout << "NumberOfStubs   = " << +fNumberOfStubs << std::endl;
    std::cout << "BunchCrossingId = " << +fBunchCrossingId << std::endl;
}

bool StubHandler::parseData(uint32_t data, bool is2S)
{
    if(is2S)
    {
        fStub.fPosition = ((data >> 4) & 0xFF);
        fStub.fBend     = (data & 0xF);
    }
    else
    {
        fStub.fPosition = ((data >> 7) & 0xFF);
        fStub.fBend     = ((data >> 4) & 0x7);
        fStub.fRow      = (data & 0xF);
    }
    return true;
}

void StubHandler::print() const
{
    std::cout << "Seed = " << +fStub.fPosition;
    if(fStub.fRow != 0xFF) std::cout << " Z = " << +fStub.fRow;
    std::cout << " Bend = " << +fStub.fBend << std::endl;
}

bool      D19cCic2Event::fAreDecodedEventContainersReady = false;
bool      D19cCic2Event::fIs2S                            = true;
bool      D19cCic2Event::fIsSparsified                    = true;
uintptr_t D19cCic2Event::fLastEventDecodedPointer         = reinterpret_cast<uintptr_t>(nullptr);

BoardDataContainer                            D19cCic2Event::fDecodedL1Event    = BoardDataContainer();
BoardDataContainer                            D19cCic2Event::fDecodedStubEvent  = BoardDataContainer();
std::array<uint32_t, NUMBER_OF_CIC_PORTS * 9> D19cCic2Event::fTheChipDataVector = std::array<uint32_t, NUMBER_OF_CIC_PORTS * 9>();

// Event implementation
D19cCic2Event::D19cCic2Event(const BeBoard* pBoard, std::vector<uint32_t>& list, bool isSparsified) : fBoard(pBoard)
{
    bool localIs2S         = pBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S;
    bool localIsSparsified = localIs2S ? isSparsified : true;
    if(fIs2S != localIs2S || fIsSparsified != localIsSparsified)
    {
        fIsSparsified                    = localIsSparsified;
        fIs2S                            = localIs2S;
        fAreDecodedEventContainersReady = false;
        fDecodedL1Event.reset();
        fDecodedStubEvent.reset();
    }

    if(!fAreDecodedEventContainersReady)
    {
        if(fIs2S)
        {
            if(fIsSparsified)
            {
                ContainerFactory::copyAndInitStructure<EmptyContainer, ClusterCollection<Cluster2SHandler, 31>, HybridL1EventInfoHandler, EmptyContainer, EmptyContainer>(*pBoard, fDecodedL1Event);
            }
            else { ContainerFactory::copyAndInitStructure<EmptyContainer, CBCL1EventInfoHandler, HybridL1EventInfoHandler, EmptyContainer, EmptyContainer>(*pBoard, fDecodedL1Event); }
        }
        else
        {
            fDecodedL1Event.setId(pBoard->getId());
            fDecodedL1Event.initialize<EmptyContainer, EmptyContainer>();

            for(auto theOpticalGroup: *pBoard)
            {
                auto decodedEventOpticalGroup = fDecodedL1Event.addOpticalGroupDataContainer(theOpticalGroup->getId());
                decodedEventOpticalGroup->initialize<EmptyContainer, HybridL1EventInfoHandler>();

                for(auto theHybrid: *theOpticalGroup)
                {
                    auto decodedEventHybrid = decodedEventOpticalGroup->addHybridDataContainer(theHybrid->getId());
                    decodedEventHybrid->initialize<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();

                    for(auto theChip: *theHybrid)
                    {
                        auto decodedEventChip = decodedEventHybrid->addChipDataContainer(theChip->getId(), theChip->getNumberOfRows(), theChip->getNumberOfCols());

                        if(theChip->getId() < 8) { ContainerFactory::copyAndInitChip<ClusterCollection<StripClusterPSHandler, 32>>(*static_cast<ChipContainer*>(theChip), *decodedEventChip); }
                        else { ContainerFactory::copyAndInitChip<ClusterCollection<PixelClusterPS, 32>>(*static_cast<ChipContainer*>(theChip), *decodedEventChip); }
                    }
                }
            }
        }

        if(fIs2S) { ContainerFactory::copyAndInitStructure<EmptyContainer, ClusterCollection<StubHandler, 3>, HybridStubEventInfo, EmptyContainer, EmptyContainer>(*pBoard, fDecodedStubEvent); }
        else { ContainerFactory::copyAndInitStructure<EmptyContainer, ClusterCollection<StubHandler, 5>, HybridStubEventInfo, EmptyContainer, EmptyContainer>(*pBoard, fDecodedStubEvent); }

        fAreDecodedEventContainersReady = true;
    }

    fLocalData = std::move(list);
}

D19cCic2Event::D19cCic2Event(const BeBoard* pBoard, std::vector<uint32_t>& list) : D19cCic2Event(pBoard, list, pBoard->getSparsification()) {}

void D19cCic2Event::decodeEvent()
{
    if(fLastEventDecodedPointer == reinterpret_cast<uintptr_t>(this)) return;

    // decode Header
    if(fLocalData.size() < 4)
    {
        LOG(ERROR) << ERROR_FORMAT << "No event header received" << RESET;
        return;
    }

    if(fLocalData[0] >> 16 != 0xFFFF)
    {
        LOG(ERROR) << ERROR_FORMAT << "Invalid Header D19cCic2Event" << RESET;
        return;
    }

    size_t totalEventSize = fLocalData[0] & 0xFFFF;

    if(fLocalData.size() < totalEventSize)
    {
        LOG(ERROR) << ERROR_FORMAT << "Event incomplete" << RESET;
        return;
    }

    fBoardEventInfo.fExternalTriggerID = fLocalData[1] >> 16;
    fBoardEventInfo.fTDC               = ((fLocalData[2] >> 24) - 1) & 0x7;
    fBoardEventInfo.fEventCount        = fLocalData[2] & 0xFFFFFF;
    fBoardEventInfo.fBunch             = fLocalData[3];

    std::vector<uint32_t>::const_iterator theCurrentHybridDataPointer = fLocalData.begin() + 4;
    for(auto theOpticalGroup: *fBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theHybridL1EventContainer = fDecodedL1Event.getHybrid(theOpticalGroup->getId(), theHybrid->getId());
            if(fIsSparsified)
            {
                for(auto theChip: *theHybridL1EventContainer)
                {
                    if(fIs2S)
                        theChip->getSummary<ClusterCollection<Cluster2SHandler, 31>>().fNumberOfClusters = 0;
                    else
                    {
                        if(theChip->getId() < 8)
                            theChip->getSummary<ClusterCollection<StripClusterPSHandler, 32>>().fNumberOfClusters = 0;
                        else
                            theChip->getSummary<ClusterCollection<PixelClusterPS, 32>>().fNumberOfClusters = 0;
                    }
                }
            }
            uint16_t theL1EventDataSize = decodeHybridL1Event(theHybridL1EventContainer, theCurrentHybridDataPointer);

            theCurrentHybridDataPointer += theL1EventDataSize;

            auto& theHybridStubEventContainer = fDecodedStubEvent.getHybrid(theOpticalGroup->getId(), theHybrid->getId());
            for(auto theChip: *theHybridStubEventContainer)
            {
                if(fIs2S) { theChip->getSummary<ClusterCollection<StubHandler, 3>>().fNumberOfClusters = 0; }
                else { theChip->getSummary<ClusterCollection<StubHandler, 5>>().fNumberOfClusters = 0; }
            }
            uint16_t theStubEventDataSize = decodeHybridStubEvent(theHybridStubEventContainer, theCurrentHybridDataPointer);

            theCurrentHybridDataPointer += theStubEventDataSize;
        }
    }

    fLastEventDecodedPointer = reinterpret_cast<uintptr_t>(this);
}

uint16_t D19cCic2Event::decodeHybridL1Event(HybridDataContainer* theHybridEventContainer, std::vector<uint32_t>::const_iterator dataStartIterator)
{
    auto theCicToChipMapping = getCicToChipMapping(theHybridEventContainer->getId());

    HybridL1EventInfoHandler* theHybridL1EventInfoHandlerPointer;

    if(fIs2S)
    {
        if(fIsSparsified) { theHybridL1EventInfoHandlerPointer = &theHybridEventContainer->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>(); }
        else { theHybridL1EventInfoHandlerPointer = &theHybridEventContainer->getSummary<HybridL1EventInfoHandler, CBCL1EventInfoHandler>(); }
    }
    else { theHybridL1EventInfoHandlerPointer = &theHybridEventContainer->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>(); }

    theHybridL1EventInfoHandlerPointer->parseData(dataStartIterator, fIs2S);

    size_t currentBitCount = 32 * 3;
    if(fIs2S)
    {
        if(fIsSparsified)
        {
            for(size_t clusterNumber = 0; clusterNumber < theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fNumberOfStripClusters; ++clusterNumber)
            {
                uint32_t         theClusterWord = getWord<CLUSTER_2S_DATA_SIZE, CLUSTER_2S_DATA_MASK>(dataStartIterator + currentBitCount / 32, currentBitCount % 32);
                Cluster2SHandler theCluster2S;
                if(theCluster2S.parseData(theClusterWord))
                {
                    theHybridEventContainer->getObject((*theCicToChipMapping)[theClusterWord >> (CLUSTER_2S_DATA_SIZE - 3)])
                        ->getSummary<ClusterCollection<Cluster2SHandler, 31>>()
                        .addCluster(theCluster2S);
                }
                currentBitCount += CLUSTER_2S_DATA_SIZE;
            }
        }
        else
        {
            fTheChipDataVector.fill(0);

            uint32_t currentCBCwordOffset = 0;

            for(size_t numberOfCBCblock = 0; numberOfCBCblock < 25; ++numberOfCBCblock)
            {
                for(int chipIdForCIC = NUMBER_OF_CIC_PORTS - 1; chipIdForCIC >= 0; --chipIdForCIC)
                {
                    uint32_t currentWordOffsetQuotient  = currentCBCwordOffset / 32;
                    uint32_t currentWordOffsetRemainder = currentCBCwordOffset % 32;
                    uint32_t theWord                    = getWord<L1_UNSPARSFIED_BLOCK_SIZE_2S, 0x7FF>(dataStartIterator, currentBitCount);

                    currentBitCount += L1_UNSPARSFIED_BLOCK_SIZE_2S;

                    if(32 - L1_UNSPARSFIED_BLOCK_SIZE_2S >= currentWordOffsetRemainder)
                    {
                        fTheChipDataVector[chipIdForCIC * 9 + currentWordOffsetQuotient] |= (theWord << (32 - L1_UNSPARSFIED_BLOCK_SIZE_2S - currentWordOffsetRemainder));
                    }
                    else
                    {
                        fTheChipDataVector[chipIdForCIC * 9 + currentWordOffsetQuotient] |= (theWord >> (L1_UNSPARSFIED_BLOCK_SIZE_2S - 32 + currentWordOffsetRemainder));
                        fTheChipDataVector[chipIdForCIC * 9 + currentWordOffsetQuotient + 1] |= (theWord << (32 - (L1_UNSPARSFIED_BLOCK_SIZE_2S - 32 + currentWordOffsetRemainder)));
                    }
                }
                currentCBCwordOffset += L1_UNSPARSFIED_BLOCK_SIZE_2S;
            }

            for(auto* theChipEventContainer: *theHybridEventContainer)
            {
                theChipEventContainer->getSummary<CBCL1EventInfoHandler>().parseData(fTheChipDataVector.begin() + 9 * getIdForCic(theHybridEventContainer->getId(), theChipEventContainer->getId()));
            }
        }
    }
    else
    {
        for(size_t stripClusterNumber = 0; stripClusterNumber < theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fNumberOfStripClusters; ++stripClusterNumber)
        {
            uint32_t              theClusterWord = getWord<STRIP_CLUSTER_PS_DATA_SIZE, STRIP_CLUSTER_PS_DATA_MASK>(dataStartIterator + currentBitCount / 32, currentBitCount % 32);
            StripClusterPSHandler theStripClusterPS;
            if(theStripClusterPS.parseData(theClusterWord))
            {
                theHybridEventContainer->getObject((*theCicToChipMapping)[theClusterWord >> (STRIP_CLUSTER_PS_DATA_SIZE - 3)])
                    ->getSummary<ClusterCollection<StripClusterPSHandler, 32>>()
                    .addCluster(theStripClusterPS);
            }
            currentBitCount += STRIP_CLUSTER_PS_DATA_SIZE;
        }
        for(size_t pixelClusterNumber = 0; pixelClusterNumber < theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fNumberOfPixelClusters; ++pixelClusterNumber)
        {
            uint32_t              theClusterWord = getWord<PIXEL_CLUSTER_PS_DATA_SIZE, PIXEL_CLUSTER_PS_DATA_MASK>(dataStartIterator + currentBitCount / 32, currentBitCount % 32);
            PixelClusterPSHandler thePixelClusterPSHandler;
            if(thePixelClusterPSHandler.parseData(theClusterWord))
            {
                theHybridEventContainer->getObject((*theCicToChipMapping)[theClusterWord >> (PIXEL_CLUSTER_PS_DATA_SIZE - 3)] + 8)
                    ->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>()
                    .addCluster(thePixelClusterPSHandler);
            }
            currentBitCount += PIXEL_CLUSTER_PS_DATA_SIZE;
        }
    }

    uint16_t hybridL1DataSize = ((*dataStartIterator) & 0xFFF) << 2; // << 2 is equal to * 4
    return hybridL1DataSize;
}

uint16_t D19cCic2Event::decodeHybridStubEvent(HybridDataContainer* theHybridStubEventContainer, std::vector<uint32_t>::const_iterator dataStartIterator)
{
    HybridStubEventInfo* theHybridStubEventInfo;
    if(fIs2S) { theHybridStubEventInfo = &theHybridStubEventContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 3>>(); }
    else { theHybridStubEventInfo = &theHybridStubEventContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 5>>(); }
    theHybridStubEventInfo->parseData(dataStartIterator);

    auto theCicToChipMapping = getCicToChipMapping(theHybridStubEventContainer->getId());

    size_t currentBitCount = 32 * 2;
    for(size_t stubNumber = 0; stubNumber < theHybridStubEventInfo->fNumberOfStubs; ++stubNumber)
    {
        if(fIs2S)
        {
            uint32_t    theClusterWord = getWord<STUB_2S_DATA_SIZE, STUB_2S_DATA_MASK>(dataStartIterator + currentBitCount / 32, currentBitCount % 32);
            StubHandler theStubHandler;
            if(theStubHandler.parseData(theClusterWord, true))
            {
                try
                {
                    theHybridStubEventContainer->getObject((*theCicToChipMapping)[theClusterWord >> (STUB_2S_DATA_SIZE - 3)])
                        ->getSummary<ClusterCollection<StubHandler, 3>>()
                        .addCluster(theStubHandler);
                }
                catch(const std::exception& e)
                {
                    LOG(DEBUG) << WARNING_FORMAT << "Number of stubs for OpticalGroup " << theHybridStubEventContainer->getId() / 2 << " Hybrid " << theHybridStubEventContainer->getId() << " Chip "
                               << (*theCicToChipMapping)[theClusterWord >> (STUB_2S_DATA_SIZE - 3)] << RESET;
                }
            }
            currentBitCount += STUB_2S_DATA_SIZE;
        }
        else
        {
            uint32_t    theClusterWord = getWord<STUB_PS_DATA_SIZE, STUB_PS_DATA_MASK>(dataStartIterator + currentBitCount / 32, currentBitCount % 32);
            StubHandler theStubHandler;
            if(theStubHandler.parseData(theClusterWord, false))
            {
                try
                {
                    theHybridStubEventContainer->getObject((*theCicToChipMapping)[theClusterWord >> (STUB_PS_DATA_SIZE - 3)] + 8)
                        ->getSummary<ClusterCollection<StubHandler, 5>>()
                        .addCluster(theStubHandler);
                }
                catch(const std::exception& e)
                {
                    LOG(DEBUG) << WARNING_FORMAT << "Number of stubs for OpticalGroup " << theHybridStubEventContainer->getId() / 2 << " Hybrid " << theHybridStubEventContainer->getId() << " Chip "
                               << (*theCicToChipMapping)[theClusterWord >> (STUB_PS_DATA_SIZE - 3)] + 8 << RESET;
                }
            }
            currentBitCount += STUB_PS_DATA_SIZE;
        }
    }

    uint16_t hybridStubDataSize = ((*dataStartIterator) & 0xFFF) << 2; // << 2 is equal to * 4
    return hybridStubDataSize;
}

const std::vector<uint8_t>* D19cCic2Event::getChipToCicMapping(uint16_t theHybridId) const
{
    const std::vector<uint8_t>* theChipToCicMapping;
    if(fIs2S)
        theChipToCicMapping = &fChipToCicMapping2S;
    else
        theChipToCicMapping = theHybridId % 2 == 0 ? &fChipToCicMappingPSR : &fChipToCicMappingPSL;
    return theChipToCicMapping;
}

const std::vector<uint8_t>* D19cCic2Event::getCicToChipMapping(uint16_t theHybridId) const
{
    const std::vector<uint8_t>* theCicToChipMapping;
    if(fIs2S)
        theCicToChipMapping = &fCicToChipMapping2S;
    else
        theCicToChipMapping = theHybridId % 2 == 0 ? &fCicToChipMappingPSR : &fCicToChipMappingPSL;
    return theCicToChipMapping;
}

void D19cCic2Event::fillChipDataContainer(ChipDataContainer* chipContainer, const std::shared_ptr<ChannelGroupBase> testChannelGroup, uint8_t hybridId)
{
    decodeEvent();
    auto               readoutChipId = chipContainer->getId();
    ChipDataContainer* theChipL1Container;
    try
    {
        theChipL1Container = fDecodedL1Event.getChip(hybridId / 2, hybridId, readoutChipId);
    }
    catch(const std::exception& e)
    {
        // hybrid was disabled
        return;
    }
    if(theChipL1Container == nullptr) return;

    auto updateIfUnmasked = [chipContainer, &testChannelGroup](uint16_t row, uint16_t col)
    {
        if(testChannelGroup->isChannelEnabled(row, col)) { chipContainer->getChannel<Occupancy>(row, col).fOccupancy += 1.; }
    };

    if(fIsSparsified)
    {
        if(fIs2S)
        {
            for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<Cluster2SHandler, 31>>())
            {
                for(size_t channel = theCluster.fCluster2S.fAddress; channel < theCluster.fCluster2S.fAddress + theCluster.fCluster2S.fWidth; ++channel) { updateIfUnmasked(0, channel); }
            }
        }
        else
        {
            if(readoutChipId < 8)
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<StripClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fStripClusterPS.fAddress; channel < theCluster.fStripClusterPS.fAddress + theCluster.fStripClusterPS.fWidth; ++channel)
                    {
                        updateIfUnmasked(0, channel);
                    }
                }
            }
            else
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fPixelClusterPS.fAddress; channel < theCluster.fPixelClusterPS.fAddress + theCluster.fPixelClusterPS.fWidth; ++channel)
                    {
                        updateIfUnmasked(theCluster.fPixelClusterPS.fZpos, channel);
                    }
                }
            }
        }
    }
    else
    {
        for(auto theHitChannel: theChipL1Container->getSummary<CBCL1EventInfoHandler>().getChannelHitList()) { updateIfUnmasked(0, theHitChannel); }
    }
}

ClusterCollection<PixelClusterPSHandler, 32> D19cCic2Event::GetPixelClusters(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    if(fIs2S || pReadoutChipId < 8)
    {
        std::cerr << "D19cCic2Event::GetPixelClusters can be called only for MPA, aborting" << std::endl;
        abort();
    }
    return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>();
}

ClusterCollection<StripClusterPSHandler, 32> D19cCic2Event::GetStripClusters(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();

    if(fIs2S || pReadoutChipId >= 8)
    {
        std::cerr << "D19cCic2Event::GetStripClusters can be called only for SSA, aborting" << std::endl;
        abort();
    }
    return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<ClusterCollection<StripClusterPSHandler, 32>>();
}

uint16_t D19cCic2Event::L1Status(uint8_t pHybridId)
{
    decodeEvent();

    auto                            theHybridL1Event = fDecodedL1Event.getHybrid(pHybridId / 2, pHybridId);
    const HybridL1EventInfoHandler* theHybridL1EventInfoHandlerPointer;
    if(fIsSparsified)
    {
        if(fIs2S)
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>();
        else
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();
    }
    else { theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, CBCL1EventInfoHandler>(); }
    return theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fStatusBits;
}

uint32_t D19cCic2Event::IsL1ErrorSet(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();

    if(fIsSparsified)
    {
        auto                            theHybridL1Event = fDecodedL1Event.getHybrid(pHybridId / 2, pHybridId);
        const HybridL1EventInfoHandler* theHybridL1EventInfoHandlerPointer;
        if(fIs2S)
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>();
        else
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();
        auto theCicStatusBits = theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fStatusBits;
        return theCicStatusBits >> (getIdForCic(pHybridId, pReadoutChipId) + 1);
    }
    else { return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<CBCL1EventInfoHandler>().fCBCL1EventInfo.fErrorCode; }
}

uint32_t D19cCic2Event::IsStubErrorSet(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    auto theHybridStubEventContainer = fDecodedL1Event.getHybrid(pHybridId / 2, pHybridId);

    HybridStubEventInfo* theHybridStubEventInfo;
    if(fIs2S) { theHybridStubEventInfo = &theHybridStubEventContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 3>>(); }
    else { theHybridStubEventInfo = &theHybridStubEventContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 5>>(); }

    auto theCicStatusBits = theHybridStubEventInfo->fStatusBits;

    return theCicStatusBits >> (getIdForCic(pHybridId, pReadoutChipId) + 1);
}

uint32_t D19cCic2Event::BxId(uint8_t pHybridId)
{
    decodeEvent();
    auto theHybridStubContainer = fDecodedStubEvent.getHybrid(pHybridId / 2, pHybridId);
    if(fIs2S) { return theHybridStubContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 3>>().fBunchCrossingId; }
    else { return theHybridStubContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 5>>().fBunchCrossingId; }
}

uint16_t D19cCic2Event::StubStatus(uint8_t pHybridId)
{
    decodeEvent();
    auto theHybridStubContainer = fDecodedStubEvent.getHybrid(pHybridId / 2, pHybridId);
    if(fIs2S) { return theHybridStubContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 3>>().fStatusBits; }
    else { return theHybridStubContainer->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 5>>().fStatusBits; }
}

uint32_t D19cCic2Event::L1Id(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    if(fIsSparsified)
    {
        auto                            theHybridL1Event = fDecodedL1Event.getHybrid(pHybridId / 2, pHybridId);
        const HybridL1EventInfoHandler* theHybridL1EventInfoHandlerPointer;
        if(fIs2S)
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>();
        else
            theHybridL1EventInfoHandlerPointer = &theHybridL1Event->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();
        return theHybridL1EventInfoHandlerPointer->fHybridL1EventInfo.fL1counter;
    }
    else { return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<CBCL1EventInfoHandler>().fCBCL1EventInfo.fL1id; }
}

// does not apply for sparsified event
uint32_t D19cCic2Event::PipelineAddress(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    if(fIsSparsified)
    {
        std::cerr << "D19cCic2Event::PipelineAddress can be called only for 2S read in unsparsified mode, aborting" << std::endl;
        abort();
    }
    else
        return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<CBCL1EventInfoHandler>().fCBCL1EventInfo.fPipelineAddress;
}

bool D19cCic2Event::DataBit(uint8_t pHybridId, uint8_t pReadoutChipId, uint8_t row, uint8_t col)
{
    decodeEvent();
    auto theChipL1Container = fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    if(fIsSparsified)
    {
        if(fIs2S)
        {
            for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<Cluster2SHandler, 31>>())
            {
                if(theCluster.isChannelHit(col)) return true;
            }
        }
        else
        {
            if(pReadoutChipId < 8)
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<StripClusterPSHandler, 32>>())
                {
                    if(theCluster.isChannelHit(col)) return true;
                }
            }
            else
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>())
                {
                    if(theCluster.isChannelHit(row, col)) return true;
                }
            }
        }
        return false;
    }
    else { return theChipL1Container->getSummary<CBCL1EventInfoHandler>().isChannelHit(col); }
}

std::vector<bool> D19cCic2Event::DataBitVector(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    auto theChipL1Container = fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    if(fIsSparsified)
    {
        if(fIs2S)
        {
            std::vector<bool> bitList(NCHANNELS, false);
            for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<Cluster2SHandler, 31>>())
            {
                for(size_t channel = theCluster.fCluster2S.fAddress; channel < theCluster.fCluster2S.fAddress + theCluster.fCluster2S.fWidth; ++channel) { bitList[channel] = true; }
            }
            return bitList;
        }
        else
        {
            if(pReadoutChipId < 8)
            {
                std::vector<bool> bitList(NSSACHANNELS, false);
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<StripClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fStripClusterPS.fAddress; channel < theCluster.fStripClusterPS.fAddress + theCluster.fStripClusterPS.fWidth; ++channel) { bitList[channel] = true; }
                }
                return bitList;
            }
            else
            {
                std::vector<bool> bitList(NSSACHANNELS * NMPAROWS, false);
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fPixelClusterPS.fAddress; channel < theCluster.fPixelClusterPS.fAddress + theCluster.fPixelClusterPS.fWidth; ++channel)
                    {
                        bitList[channel + NSSACHANNELS * theCluster.fPixelClusterPS.fZpos] = true;
                    }
                }
                return bitList;
            }
        }
    }
    else
    {
        std::vector<bool> bitList(NCHANNELS, false);
        for(auto theHitChannel: theChipL1Container->getSummary<CBCL1EventInfoHandler>().getChannelHitList()) { bitList[theHitChannel] = true; }
        return bitList;
    }
}

std::vector<StubHandler> D19cCic2Event::StubVector(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    std::vector<StubHandler> theStubVector;
    auto                     theChipStubContainer = fDecodedStubEvent.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    if(fIs2S)
    {
        for(auto& StubHandler: theChipStubContainer->getSummary<ClusterCollection<StubHandler, 3>>()) { theStubVector.push_back(StubHandler); }
    }
    else
    {
        for(auto& StubHandler: theChipStubContainer->getSummary<ClusterCollection<StubHandler, 5>>()) { theStubVector.push_back(StubHandler); }
    }
    return theStubVector;
}

bool D19cCic2Event::StubBit(uint8_t pHybridId, uint8_t pCbcId)
{
    decodeEvent();
    auto theChipStubContainer = fDecodedStubEvent.getChip(pHybridId / 2, pHybridId, pCbcId);
    if(fIs2S) { return theChipStubContainer->getSummary<ClusterCollection<StubHandler, 3>>().size() > 0; }
    else { return theChipStubContainer->getSummary<ClusterCollection<StubHandler, 5>>().size() > 0; }
}

uint32_t D19cCic2Event::GetNHits(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    auto     theChipL1Container = fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    uint32_t numberOfHits       = 0;
    if(fIsSparsified)
    {
        if(fIs2S)
        {
            for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<Cluster2SHandler, 31>>()) { numberOfHits += theCluster.fCluster2S.fWidth; }
        }
        else
        {
            if(pReadoutChipId < 8)
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<StripClusterPSHandler, 32>>()) { numberOfHits += theCluster.fStripClusterPS.fWidth; }
            }
            else
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>()) { numberOfHits += theCluster.fPixelClusterPS.fWidth; }
            }
        }
    }
    else { numberOfHits = theChipL1Container->getSummary<CBCL1EventInfoHandler>().countNumberOfHits(); }

    return numberOfHits;
}

std::vector<std::pair<uint16_t, uint16_t>> D19cCic2Event::GetHits(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    std::vector<std::pair<uint16_t, uint16_t>> theHitList;
    auto                                       theChipL1Container = fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    if(fIsSparsified)
    {
        theHitList.reserve(MAXCICCHANNELS);
        if(fIs2S)
        {
            for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<Cluster2SHandler, 31>>())
            {
                for(size_t channel = theCluster.fCluster2S.fAddress; channel < theCluster.fCluster2S.fAddress + theCluster.fCluster2S.fWidth; ++channel) { theHitList.emplace_back(0, channel); }
            }
        }
        else
        {
            if(pReadoutChipId < 8)
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<StripClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fStripClusterPS.fAddress; channel < theCluster.fStripClusterPS.fAddress + theCluster.fStripClusterPS.fWidth; ++channel)
                    {
                        theHitList.emplace_back(0, channel);
                    }
                }
            }
            else
            {
                for(auto theCluster: theChipL1Container->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>())
                {
                    for(size_t channel = theCluster.fPixelClusterPS.fAddress; channel < theCluster.fPixelClusterPS.fAddress + theCluster.fPixelClusterPS.fWidth; ++channel)
                    {
                        theHitList.emplace_back(theCluster.fPixelClusterPS.fZpos, channel);
                    }
                }
            }
        }
    }
    else
    {
        theHitList.reserve(NCHANNELS);
        for(auto theHitChannel: theChipL1Container->getSummary<CBCL1EventInfoHandler>().getChannelHitList()) { theHitList.emplace_back(0, theHitChannel); }
    }

    return theHitList;
}

ClusterCollection<Cluster2SHandler, 31> D19cCic2Event::getClusters(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    decodeEvent();
    if(!fIsSparsified || !fIs2S)
    {
        std::cerr << "D19cCic2Event::getClusters can be called only for 2S read in sparsified mode, aborting" << std::endl;
        abort();
    }
    return fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId)->getSummary<ClusterCollection<Cluster2SHandler, 31>>();
}

void D19cCic2Event::print()
{
    decodeEvent();
    std::cout << "D19cCic2Event event printout" << std::endl;

    std::cout << "ExternalTriggerID = " << fBoardEventInfo.fExternalTriggerID << std::endl;
    std::cout << "TDC               = " << fBoardEventInfo.fTDC << std::endl;
    std::cout << "EventCount        = " << fBoardEventInfo.fEventCount << std::endl;
    std::cout << "BxId              = " << fBoardEventInfo.fBunch << std::endl;

    for(auto theOpticalGroup: fDecodedL1Event)
    {
        std::cout << "OpticalGroup id = " << theOpticalGroup->getId() << std::endl;
        for(auto theHybrid: *theOpticalGroup)
        {
            std::cout << "Hybrid id = " << theHybrid->getId() << std::endl;

            const HybridL1EventInfoHandler* theHybridL1EventInfoHandlerPointer;
            if(fIsSparsified)
            {
                if(fIs2S)
                    theHybridL1EventInfoHandlerPointer = &theHybrid->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>();
                else
                    theHybridL1EventInfoHandlerPointer = &theHybrid->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();
            }
            else { theHybridL1EventInfoHandlerPointer = &theHybrid->getSummary<HybridL1EventInfoHandler, CBCL1EventInfoHandler>(); }
            std::cout << "L1 header" << std::endl;
            theHybridL1EventInfoHandlerPointer->print();
            for(auto theChip: *theHybrid)
            {
                std::cout << "Chip id = " << theChip->getId() << std::endl;
                if(fIsSparsified)
                {
                    if(fIs2S)
                    {
                        for(auto theCluster: theChip->getSummary<ClusterCollection<Cluster2SHandler, 31>>()) { theCluster.print(); }
                    }
                    else
                    {
                        if(theChip->getId() < 8)
                        {
                            for(auto theCluster: theChip->getSummary<ClusterCollection<StripClusterPSHandler, 32>>()) { theCluster.print(); }
                        }
                        else
                        {
                            for(auto theCluster: theChip->getSummary<ClusterCollection<PixelClusterPSHandler, 32>>()) { theCluster.print(); }
                        }
                    }
                }
                else { theChip->getSummary<CBCL1EventInfoHandler>().print(); }
            }

            auto theStubHybrid = fDecodedStubEvent.getHybrid(theOpticalGroup->getId(), theHybrid->getId());
            std::cout << "Stub header" << std::endl;
            if(fIs2S) { theStubHybrid->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 3>>().print(); }
            else { theStubHybrid->getSummary<HybridStubEventInfo, ClusterCollection<StubHandler, 5>>().print(); }
            for(auto theChip: *theStubHybrid)
            {
                std::cout << "Chip id = " << theChip->getId() << std::endl;
                if(fIs2S)
                {
                    for(auto theStub: theChip->getSummary<ClusterCollection<StubHandler, 3>>()) { theStub.print(); }
                }
                else
                {
                    for(auto theStub: theChip->getSummary<ClusterCollection<StubHandler, 5>>()) { theStub.print(); }
                }
            }
        }
    }
}

HybridL1EventInfoHandler D19cCic2Event::getHybridL1EventInfoHandler(uint8_t pHybridId)
{
    decodeEvent();
    auto theHybrid = fDecodedL1Event.getHybrid(pHybridId / 2, pHybridId);
    if(fIsSparsified)
    {
        if(fIs2S)
            return theHybrid->getSummary<HybridL1EventInfoHandler, ClusterCollection<Cluster2SHandler, 31>>();
        else
            return theHybrid->getSummary<HybridL1EventInfoHandler, ClusterCollection<StripClusterPSHandler, 32>>();
    }
    else { return theHybrid->getSummary<HybridL1EventInfoHandler, CBCL1EventInfoHandler>(); }
}

CBCL1EventInfoHandler D19cCic2Event::getCBCL1EventInfoHandler(uint8_t pHybridId, uint8_t pReadoutChipId)
{
    if(fIsSparsified || !fIs2S)
    {
        std::cerr << "D19cCic2Event::getCBCL1EventInfoHandler can be called only for unsparsified  2S events, aborting" << std::endl;
        abort();
    }
    decodeEvent();
    auto theChip = fDecodedL1Event.getChip(pHybridId / 2, pHybridId, pReadoutChipId);
    return theChip->getSummary<CBCL1EventInfoHandler>();
}

BoardEventInfo D19cCic2Event::getBoardEventInfo()
{
    decodeEvent();
    return fBoardEventInfo;
}

uint32_t D19cCic2Event::GetBunch()
{
    decodeEvent();
    return fBoardEventInfo.fBunch;
}

uint32_t D19cCic2Event::GetEventCount()
{
    decodeEvent();
    return fBoardEventInfo.fEventCount;
}

uint32_t D19cCic2Event::GetTDC()
{
    decodeEvent();
    return fBoardEventInfo.fTDC;
}

uint32_t D19cCic2Event::GetExternalTriggerId()
{
    decodeEvent();
    return fBoardEventInfo.fExternalTriggerID;
}

} // namespace Ph2_HwInterface
