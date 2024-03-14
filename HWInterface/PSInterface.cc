/*
        FileName :                     PSInterface.cc
        Content :                      User Interface to the PSs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "HWInterface/PSInterface.h"

#include "Utils/ConsoleColor.h"
#include <typeinfo>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
PSInterface::PSInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap)
{
    theSSAInterface                                                     = static_cast<SSAInterface*>(new SSAInterface(pBoardMap));
    fTheSSA2Interface                                                    = static_cast<SSA2Interface*>(new SSA2Interface(pBoardMap));
    fTheMPA2Interface                                                    = static_cast<MPA2Interface*>(new MPA2Interface(pBoardMap));
    const std::map<FrontEndType, ReadoutChipInterface*> CHIP_INTERFACE1 = {
        {FrontEndType::SSA, theSSAInterface}, {FrontEndType::SSA2, fTheSSA2Interface}, {FrontEndType::MPA2, fTheMPA2Interface}};
    CHIP_INTERFACE = CHIP_INTERFACE1;
}
PSInterface::~PSInterface() {}

ReadoutChipInterface* PSInterface::getInterface(Chip* pPS)
{
    if(pPS->getFrontEndType() == FrontEndType::SSA)
        return static_cast<SSAInterface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else if(pPS->getFrontEndType() == FrontEndType::SSA2)
        return static_cast<SSA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else if(pPS->getFrontEndType() == FrontEndType::MPA2)
        return static_cast<MPA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else
    {
        std::string errorstring = "Unknown Interface Type " + std::to_string((int)pPS->getFrontEndType());
        throw Exception(errorstring.c_str());
        exit(EXIT_FAILURE);
    }

    // return (CHIP_INTERFACE.find(pPS->getFrontEndType()))->second;
}

std::vector<uint8_t> PSInterface::readLUT(ReadoutChip* pPS, uint8_t pMode)
{
    std::vector<uint8_t> cLUT(0);
    if(pPS->getFrontEndType() == FrontEndType::MPA2) { cLUT = fTheMPA2Interface->readLUT(pPS, pMode); }
    return cLUT;
}
bool PSInterface::setInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop) { return getInterface(pPS)->setInjectionSchema(pPS, group, pVerifLoop); }
bool PSInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    return getInterface(pPS)->maskChannelsAndSetInjectionSchema(pPS, group, mask, inject, pVerifLoop);
}
bool PSInterface::maskChannelGroup(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop) { return getInterface(pPS)->maskChannelGroup(pPS, group, pVerifLoop); }

bool PSInterface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize) { return getInterface(pChip)->ConfigureChipOriginalMask(pChip, pVerifLoop, pBlockSize); }

// To generalize
uint16_t PSInterface::ReadChipReg(Chip* pPS, const std::string& pRegName) { return getInterface(pPS)->ReadChipReg(pPS, pRegName); }

std::vector<std::pair<std::string, uint16_t>> PSInterface::ReadChipMultReg(Ph2_HwDescription::Chip* pChip, const std::vector<std::string>& theRegisterList)
{
    return getInterface(pChip)->ReadChipMultReg(pChip, theRegisterList);
}

// To generalize
bool PSInterface::WriteChipReg(Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    // LOG(DEBUG) << BOLDMAGENTA << " PSInterface::WriteChipReg writing to " << pRegName << RESET;
    return getInterface(pPS)->WriteChipReg(pPS, pRegName, pValue, pVerifLoop);
}

bool PSInterface::WriteChipMultReg(Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop) { return getInterface(pPS)->WriteChipMultReg(pPS, pVecReq, pVerifLoop); }

// To generalize
bool PSInterface::WriteChipAllLocalReg(ReadoutChip* pPS, const std::string& dacName, const ChipContainer& localRegValues, bool pVerifLoop)
{
    return getInterface(pPS)->WriteChipAllLocalReg(pPS, dacName, localRegValues, pVerifLoop);
}

// To generalize
bool PSInterface::ConfigureChip(Chip* pPS, bool pVerifLoop, uint32_t pBlockSize) { return getInterface(pPS)->ConfigureChip(pPS, pVerifLoop, pBlockSize); }

void PSInterface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA2) { fTheMPA2Interface->producePhaseAlignmentPattern(pChip, pWait_ms); }
    else if(pChip->getFrontEndType() == FrontEndType::SSA or pChip->getFrontEndType() == FrontEndType::SSA2)
    {
        LOG(INFO) << BOLDMAGENTA << "No need to generate phase alignment pattern on SSA#" << +pChip->getId() << " when on a PS module" << RESET;
    }
}
void PSInterface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA2) { fTheMPA2Interface->produceWordAlignmentPattern(pChip); }
    else if(pChip->getFrontEndType() == FrontEndType::SSA or pChip->getFrontEndType() == FrontEndType::SSA2)
    {
        LOG(INFO) << BOLDMAGENTA << "No need to generate word alignment pattern on SSA#" << +pChip->getId() << " when on a PS module" << RESET;
    }
}

bool PSInterface::enableInjection(ReadoutChip* pPS, bool inject, bool pVerifLoop)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA2) { return fTheMPA2Interface->enableInjection(pPS, inject, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA) { return theSSAInterface->enableInjection(pPS, inject, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA2) { return fTheSSA2Interface->enableInjection(pPS, inject, pVerifLoop); }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

//
std::vector<int> PSInterface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA2) { return fTheMPA2Interface->decodeBendCode(pChip, pBendCode); }
    return std::vector<int>(0);
}
//
void PSInterface::digiInjection(ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA2) { fTheMPA2Interface->digiInjection(pChip, pInjections, pPattern); }
    else if(pChip->getFrontEndType() == FrontEndType::SSA2 or pChip->getFrontEndType() == FrontEndType::SSA)
    {
        LOG(ERROR) << BOLDRED << "No digiInjection implemented for SSA for some reason " << RESET;
        throw std::runtime_error(std::string("No digiInjection implemented for SSA for some reason "));
    }
}

bool PSInterface::injectNoiseClusters(ReadoutChip* pPS, std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> theClusterList)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA2) { return fTheMPA2Interface->injectNoiseClusters(pPS, theClusterList); }
    else { return fTheSSA2Interface->injectNoiseClusters(pPS, theClusterList); }
}

bool PSInterface::injectNoiseStubs(Ph2_HwDescription::ReadoutChip* pMPA, Ph2_HwDescription::ReadoutChip* pSSA, std::vector<std::tuple<uint8_t, uint8_t, int>> theStubVector)
{
    if(pMPA->getFrontEndType() != FrontEndType::MPA2 || pSSA->getFrontEndType() != FrontEndType::SSA2)
    {
        std::cerr << __PRETTY_FUNCTION__ << " MPA2 and SSA2 must be provided in the correct order! Aborting..." << std::endl;
        abort();
    }
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> pixelClusterList;
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> stripClusterList;

    for(const auto& theStub: theStubVector)
    {
        uint8_t seedRow         = std::get<0>(theStub);
        uint8_t seedCol         = std::get<1>(theStub) / 2;
        uint8_t seedClusterSize = 1 + std::get<1>(theStub) % 2;

        uint8_t correlationHit         = std::get<1>(theStub) + std::get<2>(theStub);
        uint8_t correlationCol         = correlationHit / 2;
        uint8_t correlationClusterSize = 1 + correlationHit % 2;

        pixelClusterList.push_back({seedRow, seedCol, seedClusterSize});
        stripClusterList.push_back({0, correlationCol, correlationClusterSize});
    }

    return fTheMPA2Interface->injectNoiseClusters(pMPA, pixelClusterList) && fTheSSA2Interface->injectNoiseClusters(pSSA, stripClusterList);
}

} // namespace Ph2_HwInterface
