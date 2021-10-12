/*

        FileName :                     Event.cc
        Content :                      Event handling from DAQ
        Programmer :                   Nicolas PIERRE
        Version :                      1.0
        Date of creation :             10/07/14
        Support :                      mail to : nicolas.pierre@icloud.com

 */

#include "../Utils/D19cMPAEvent.h"
#include "../HWDescription/Definition.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Event implementation

D19cMPAEvent::D19cMPAEvent(const BeBoard* pBoard, uint32_t pNMPA, uint32_t pNFe, const std::vector<uint32_t>& list) : fEventDataVector(pNMPA * pNFe)
{
    fNMPA = pNMPA;
    SetEvent(pBoard, pNMPA, list);
}

void D19cMPAEvent::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    for(auto opticalGroup: *boardContainer)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                unsigned int i = 0;
                for(ChannelDataContainer<Occupancy>::iterator channel = chip->begin<Occupancy>(); channel != chip->end<Occupancy>(); channel++, i++)
                {
                    if(cTestChannelGroup->isChannelEnabled(i))
                    {
                        // TOFIX
                        channel->fOccupancy += 0.0;
                    }
                }
            }
        }
    }
}

void D19cMPAEvent::SetEvent(const BeBoard* pBoard, uint32_t pNMPA, const std::vector<uint32_t>& list)
{
    // for (auto L : list) LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;
    // Not sure about dsize...
    // fEventSize = 4*((0x0000FFFF & list.at (0)) - (0x000000FF & list.at (1)));
    fEventSize = 4 * (0x0000FFFF & list.at(0));
    // std::cout<<fEventSize<<","<<list.size()<<std::endl;
    if(fEventSize != list.size()) LOG(ERROR) << "Incorrect event size";
    uint16_t cLeading = ((0xFFFF0000 & list.at(0)) >> 16);
    if(cLeading != 0xFFFF) LOG(ERROR) << "Incorrect leading bits";

    // not iterate through hybrids
    uint32_t address_offset = D19C_EVENT_HEADER1_SIZE_32_MPA;
    uint32_t data_offset    = address_offset;
    // iterating through the first hybrid chips
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                uint8_t pMPAId = cChip->getId();

                if(cChip->getFrontEndType() != FrontEndType::MPA) continue;

                uint8_t cPLeadingMPA = ((0xF0000000 & list.at(data_offset)) >> 28);
                uint8_t cErrorMPA    = ((0x0F000000 & list.at(data_offset)) >> 24);
                uint8_t cFeId        = ((0x00FF0000 & list.at(data_offset)) >> 16);
                // uint8_t cCidMPA =         ((0x0000F000 & list.at (data_offset)) >> 16 );
                uint16_t cL1size_32_MPA = ((0x00000FFF & list.at(data_offset))) * 4;

                // uint16_t cFrameDelay =     (0x00000FFF & list.at (data_offset+1)) ;
                // uint8_t cChipType =        (0x0000F000 & list.at (data_offset+1)) ;
                uint8_t cSsize_32_MPA = 0;

                cSsize_32_MPA        = ((0x00000FFF & list.at(data_offset + cL1size_32_MPA))) * 4;
                uint8_t cSLeadingMPA = ((0xF0000000 & list.at(data_offset + cL1size_32_MPA)) >> 28);

                uint8_t cSyncBit1 = ((0x00008000 & list.at(data_offset + cL1size_32_MPA + 1)) >> 15);
                uint8_t cSyncBit2 = ((0x00004000 & list.at(data_offset + cL1size_32_MPA + 1)) >> 14);
                if(cPLeadingMPA != 0xA) LOG(ERROR) << "Incorrect L1A header for MPA " << unsigned(pMPAId) << "," << unsigned(cHybrid->getId()) << "," << unsigned(cOpticalGroup->getId());
                if(cSLeadingMPA != 0x5) LOG(ERROR) << "Incorrect stub header for MPA " << unsigned(pMPAId) << "," << unsigned(cHybrid->getId()) << "," << unsigned(cOpticalGroup->getId());
                if(cErrorMPA != 0) LOG(INFO) << BOLDRED << "Error code " << unsigned(cErrorMPA) << " for MPA " << unsigned(pMPAId);
                if(cSyncBit1 != 1) LOG(INFO) << BOLDRED << "Warning, sync bit 1 not 1, data frame probably misaligned!" << RESET;
                if(cSyncBit2 != 0) LOG(INFO) << BOLDRED << "Warning, sync bit 2 not 0, data frame probably misaligned!" << RESET;

                uint16_t cKey     = encodeVectorIndex(cFeId, pMPAId, fNMPA);
                uint32_t begin    = data_offset;
                uint16_t cFevSize = cL1size_32_MPA + cSsize_32_MPA;
                uint32_t end      = begin + cFevSize;
                // std::vector<uint32_t> cMPAData (std::next (std::begin (list), begin), std::next (std::begin (list), end)
                // );

                fEventDataVector[cKey] = std::vector<uint32_t>(std::next(std::begin(list), begin), std::next(std::begin(list), end));
                // LOG (INFO) << "Size "<<fEventDataVector[cKey].size()<< RESET;
                data_offset += cFevSize;
            }
        }
        address_offset = data_offset; // probably needs to be fixed
    }
}

bool D19cMPAEvent::Error(uint8_t pFeId, uint8_t pMPAId, uint32_t i) const
{
    uint32_t error = Error(pFeId, pMPAId);
    if(i == 0)
        return ((error & 0x1) >> 0);
    else if(i == 1)
        return ((error & 0x2) >> 1);
    else
    {
        LOG(ERROR) << "bit id must be less or equals 1";
        return true;
    }
}

uint32_t D19cMPAEvent::Error(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint32_t cError = ((lvec.at(0) & 0x00000003) >> 0);
        return cError;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint16_t D19cMPAEvent::GetMPAL1Counter(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint16_t L1cnt = ((lvec.at(2) & 0x01FF0000) >> 16);
        return L1cnt;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint8_t D19cMPAEvent::GetMPAChipType(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint8_t MPACT = ((lvec.at(1) & 0x0000F000) >> 12);
        return MPACT;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint8_t D19cMPAEvent::GetMPAChipID(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint8_t MPACID = ((lvec.at(0) & 0x0000F000) >> 12);
        return MPACID;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint16_t D19cMPAEvent::GetMPAHybridID(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint16_t MPAHID = ((lvec.at(0) & 0x00FF0000) >> 16);
        return MPAHID;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint8_t D19cMPAEvent::GetMPAError(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint8_t MPAERR = ((lvec.at(0) & 0xC0000000) >> 30);
        return MPAERR;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint8_t D19cMPAEvent::GetNStripClusters(uint8_t pFeId, uint8_t pMPAId) const
{
    // LOG (INFO) << fEventDataVector.size()<<" "<<encodeVectorIndex(pFeId, pMPAId, fNMPA) << RESET;
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint8_t Nstrip = ((lvec.at(2) & 0x00001F00) >> 8);
        return Nstrip;
    }
    else
    {
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

uint8_t D19cMPAEvent::GetNPixelClusters(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    if(lvec.size() > 1)
    {
        // buf overflow and lat error
        uint8_t Npix = (lvec.at(2) & 0x0000001F);
        return Npix;
    }
    else
    {
        return 0;
    }
}

uint32_t D19cMPAEvent::DivideBy2RoundUp(uint32_t value) const { return (value + value % 2) / 2; }

uint32_t D19cMPAEvent::GetCluster(std::vector<uint32_t> lvec, uint8_t nclus, uint8_t cClusterSize, uint32_t deltaword) const
{
    uint32_t mask = ((1 << cClusterSize) - 1) << (32 - cClusterSize);

    uint32_t startbit   = nclus * cClusterSize;
    uint32_t endbit     = (deltaword + startbit) + cClusterSize;
    uint8_t  nstartword = int((deltaword + startbit) / 32);
    uint8_t  nendword   = int((endbit) / 32);

    uint32_t startword = lvec.at(nstartword);
    // LOG(INFO) << BOLDRED << "startword "<<unsigned(nstartword) <<" nclus "<<unsigned(nclus) <<" cClusterSize "<<unsigned(cClusterSize)<< RESET;
    // LOG(INFO) << BOLDRED << "startbit "<<(startbit + deltaword) % 32 << RESET;

    uint8_t  displaceL = (startbit + deltaword) % 32;
    uint32_t curLmask  = mask >> displaceL;
    uint32_t curLword  = ((startword & curLmask) >> std::max(0, 32 - (displaceL + cClusterSize + 1)));

    uint8_t  displaceR = 0;
    uint32_t curRword  = 0;
    if(nstartword != nendword)
    {
        uint32_t endword = lvec.at(nendword);

        displaceR         = endbit % 32;
        uint32_t curRmask = mask << (cClusterSize - displaceR);
        curRword          = ((endword & curRmask) >> (31 - displaceR));
    }
    // LOG(INFO) << BOLDRED << "curLword "<<std::bitset<12>(curLword)<< RESET;
    // LOG(INFO) << BOLDRED << "curRword "<<std::bitset<12>(curRword)<< RESET;
    uint32_t word = ((curLword << abs(std::min(0, 31 - (displaceL + cClusterSize)))) + curRword) >> 1;
    return word;
}

std::vector<SCluster> D19cMPAEvent::GetStripClusters(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<SCluster> result;

    uint8_t cNstrip = GetNStripClusters(pFeId, pMPAId);
    if(cNstrip == 0) return result;

    SCluster aSCluster;

    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    uint8_t cSClusterSize = D19C_SCluster_SIZE_32_MPA;
    uint8_t deltaword     = 3 * 32;
    uint8_t nstrips       = 0;
    while(nstrips < cNstrip)
    {
        uint32_t word = GetCluster(lvec, nstrips, cSClusterSize, deltaword);

        aSCluster.fAddress = ((0x000007f0 & word) >> 4);
        aSCluster.fMip     = (0x0000000e & word) >> 1;
        aSCluster.fWidth   = 0x00000001 & word;

        // LOG(INFO) << BOLDRED << "STRIPS" << RESET;
        // LOG (INFO) << BOLDRED << std::bitset<12>(word) << RESET;
        // LOG(INFO) << BOLDRED << unsigned(aSCluster.fAddress)<<","<<unsigned(aSCluster.fWidth)<<","<< unsigned(aSCluster.fMip)<< RESET;

        result.push_back(aSCluster);
        nstrips += 1;
    }
    return result;
}

std::vector<PCluster> D19cMPAEvent::GetPixelClusters(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<PCluster> result;
    uint8_t               cNpix = GetNPixelClusters(pFeId, pMPAId);
    if(cNpix == 0) return result;
    PCluster aPCluster;
    uint8_t  cNstrip       = GetNStripClusters(pFeId, pMPAId);
    uint8_t  cSClusterSize = D19C_SCluster_SIZE_32_MPA;

    uint8_t cPClusterSize = D19C_PCluster_SIZE_32_MPA;

    std::vector<uint32_t> lvec      = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    uint8_t               deltaword = 3 * 32 + cSClusterSize * (cNstrip);
    uint8_t               npix      = 0;

    while(npix < cNpix)
    {
        uint32_t word      = GetCluster(lvec, npix, cPClusterSize, deltaword);
        aPCluster.fAddress = (0x00003f80 & word) >> 7;
        aPCluster.fWidth   = (0x00000070 & word) >> 4;
        aPCluster.fZpos    = 0x0000000F & word;
        // LOG(INFO) << BOLDRED << "PIX" << RESET;
        // LOG(INFO) << BOLDRED << std::bitset<15>(word) << RESET;
        // LOG(INFO) << BOLDRED << unsigned(aPCluster.fAddress)<<","<<unsigned(aPCluster.fWidth)<<","<< unsigned(aPCluster.fZpos)<< RESET;
        result.push_back(aPCluster);
        npix += 1;
    }

    return result;
}

/*uint32_t D19cMPAEvent::GetSync1( uint8_t pFeId, uint8_t pMPAId) const
{

    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    return (lvec.at(31) & 0x02000000) >> 25;
}


uint32_t D19cMPAEvent::GetSync2( uint8_t pFeId, uint8_t pMPAId) const
{

    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));

    return (lvec.at(31) & 0x01000000) >> 24;
}*/

uint32_t D19cMPAEvent::GetBX1_NStubs(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec           = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    uint16_t              cL1size_32_MPA = (0x00000FFF & lvec.at(0)) * 4;

    return (0x00000007 & lvec.at(cL1size_32_MPA + 1));
}

uint16_t D19cMPAEvent::GetStubDataDelay(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> lvec           = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    uint16_t              cL1size_32_MPA = (0x00000FFF & lvec.at(0)) * 4;

    return (0x00FFF000 & lvec.at(cL1size_32_MPA));
}

std::vector<Stub> D19cMPAEvent::StubVector(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<Stub> cStubVec;
    // here creavte stubs and return the vector
    std::vector<uint32_t> lvec           = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    uint16_t              cL1size_32_MPA = (0x00000FFF & lvec.at(0)) * 4;
    // Apparently something needs to be done with this?
    // uint16_t  =   (lvec.at (cL1size_32_MPA) & 0x00FFF000) >> 12 ;
    if(lvec.size() > 1)
    {
        uint8_t pos1 = (lvec.at(cL1size_32_MPA + 1) & 0x00FF0000) >> 16;
        uint8_t pos2 = (lvec.at(cL1size_32_MPA + 2) & 0x000000FF) >> 0;
        uint8_t pos3 = (lvec.at(cL1size_32_MPA + 2) & 0x00FF0000) >> 16;
        uint8_t pos4 = (lvec.at(cL1size_32_MPA + 3) & 0x000000FF) >> 0;
        uint8_t pos5 = (lvec.at(cL1size_32_MPA + 3) & 0x00FF0000) >> 16;

        uint8_t bend1 = (lvec.at(cL1size_32_MPA + 1) & 0xF0000000) >> 28;
        uint8_t bend2 = (lvec.at(cL1size_32_MPA + 2) & 0x0000F000) >> 12;
        uint8_t bend3 = (lvec.at(cL1size_32_MPA + 2) & 0xF0000000) >> 28;
        uint8_t bend4 = (lvec.at(cL1size_32_MPA + 3) & 0x0000F000) >> 12;
        uint8_t bend5 = (lvec.at(cL1size_32_MPA + 3) & 0xF0000000) >> 28;

        uint8_t row1 = (lvec.at(cL1size_32_MPA + 1) & 0x0F000000) >> 24;
        uint8_t row2 = (lvec.at(cL1size_32_MPA + 2) & 0x00000F00) >> 8;
        uint8_t row3 = (lvec.at(cL1size_32_MPA + 2) & 0x0F000000) >> 24;
        uint8_t row4 = (lvec.at(cL1size_32_MPA + 3) & 0x00000F00) >> 8;
        uint8_t row5 = (lvec.at(cL1size_32_MPA + 3) & 0x0F000000) >> 24;

        if(pos1 != 0) cStubVec.emplace_back(pos1, bend1, row1);
        if(pos2 != 0) cStubVec.emplace_back(pos2, bend2, row2);
        if(pos3 != 0) cStubVec.emplace_back(pos3, bend3, row3);
        if(pos4 != 0) cStubVec.emplace_back(pos4, bend4, row4);
        if(pos5 != 0) cStubVec.emplace_back(pos5, bend5, row5);
    }
    else
        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";

    return cStubVec;
}

uint8_t D19cMPAEvent::GetNStubs(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<Stub> cStubVec;
    // here create stubs and return the vector
    // std::cout<<"GetNStubs"<<std::endl;
    std::vector<uint32_t> lvec = fEventDataVector.at(encodeVectorIndex(pFeId, pMPAId, fNMPA));
    if(lvec.size() > 1)
    {
        // std::cout<<"GETEM"<<std::endl;
        // for (auto L : lvec) LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;
        uint16_t cL1size_32_MPA = (0x00000FFF & lvec.at(0)) * 4;
        // std::cout<<"GETEM"<<std::endl;
        // std::cout<<std::bitset<32> (lvec.at(cL1size_32_MPA + 1))<<std::endl;
        return (lvec.at(cL1size_32_MPA + 1) & 0x7);
    }
    else
    {
        // LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";        LOG(INFO) << "Event: FE " << +pFeId << " MPA " << +pMPAId << " is not found.";
        return 0;
    }
}

void D19cMPAEvent::print(std::ostream& os) const
{
    os << "MPA Event #" << std::endl;
    for(auto const& cKey: this->fEventDataMap)
    {
        uint8_t cFeId;
        uint8_t cMpaId;
        this->decodeId(cKey.first, cFeId, cMpaId);
        os << "Hybrid " << +cFeId << ", Chip " << +cMpaId << std::endl;
        os << "\t L1 Counter: " << GetMPAL1Counter(cFeId, cMpaId) << std::endl;
        os << "\t Error: " << Error(cFeId, cMpaId) << std::endl;
        os << "\t N Pixel Clusters: " << GetNPixelClusters(cFeId, cMpaId) << std::endl;
        for(auto pcluster: GetPixelClusters(cFeId, cMpaId)) os << "\t\t Cluster Address: " << +pcluster.fAddress << ", Width: " << +pcluster.fWidth << ", ZPos: " << +pcluster.fZpos << std::endl;
        os << "\t N Strip Clusters: " << GetNStripClusters(cFeId, cMpaId) << std::endl;
        for(auto scluster: GetStripClusters(cFeId, cMpaId)) os << "\t\t Cluster Address: " << +scluster.fAddress << ", Width: " << +scluster.fWidth << ", MIP: " << +scluster.fMip << std::endl;
        os << std::endl;
    }
}

uint32_t D19cMPAEvent::GetNHits(uint8_t pFeId, uint8_t pMPAId) const { return GetNPixelClusters(pFeId, pMPAId) + GetNStripClusters(pFeId, pMPAId); }

std::string D19cMPAEvent::StubBitString(uint8_t pFeId, uint8_t pCbcId) const
{
    std::ostringstream os;

    std::vector<Stub> cStubVector = this->StubVector(pFeId, pCbcId);

    for(auto cStub: cStubVector) os << std::bitset<8>(cStub.getPosition()) << " " << std::bitset<4>(cStub.getBend()) << " ";

    return os.str();
}

// These are unimplemented

uint32_t D19cMPAEvent::PipelineAddress(uint8_t pFeId, uint8_t pMPAId) const { return 0; }

std::string D19cMPAEvent::HexString() const { return ""; }

bool D19cMPAEvent::DataBit(uint8_t pFeId, uint8_t pMPAId, uint32_t i) const { return false; }

std::vector<uint32_t> D19cMPAEvent::GetHits(uint8_t pFeId, uint8_t pMPAId) const
{
    std::vector<uint32_t> none;
    return none;
}

std::string D19cMPAEvent::DataHexString(uint8_t pFeId, uint8_t pMPAId) const { return ""; }

bool D19cMPAEvent::StubBit(uint8_t pFeId, uint8_t pMPAId) const { return (GetNStubs(pFeId, pMPAId) > 0); }

SLinkEvent D19cMPAEvent::GetSLinkEvent(BeBoard* pBoard) const
{
    SLinkEvent none;
    return none;
}

std::string D19cMPAEvent::GlibFlagString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }

std::string D19cMPAEvent::DataBitString(uint8_t pFeId, uint8_t pCbcId) const { return ""; }

std::vector<bool> D19cMPAEvent::DataBitVector(uint8_t pFeId, uint8_t pCbcId) const
{
    std::vector<bool> none;
    return none;
}

std::vector<Cluster> D19cMPAEvent::getClusters(uint8_t pFeId, uint8_t pCbcId) const
{
    std::vector<Cluster> none;
    return none;
}

std::vector<bool> D19cMPAEvent::DataBitVector(uint8_t pFeId, uint8_t pCbcId, const std::vector<uint8_t>& channelList) const
{
    std::vector<bool> none;
    return none;
}

} // namespace Ph2_HwInterface
