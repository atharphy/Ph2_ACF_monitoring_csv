/*
        FileName :                     PSInterface.cc
        Content :                      User Interface to the PSs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "PSInterface.h"

#include "../Utils/ConsoleColor.h"
#include <typeinfo>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
PSInterface::PSInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap)
{
    theSSAInterface                                                     = static_cast<SSAInterface*>(new SSAInterface(pBoardMap));
    theMPAInterface                                                     = static_cast<MPAInterface*>(new MPAInterface(pBoardMap));
    theSSA2Interface                                                    = static_cast<SSA2Interface*>(new SSA2Interface(pBoardMap));
    theMPA2Interface                                                    = static_cast<MPA2Interface*>(new MPA2Interface(pBoardMap));
    const std::map<FrontEndType, ReadoutChipInterface*> CHIP_INTERFACE1 = {
        {FrontEndType::SSA, theSSAInterface}, {FrontEndType::SSA2, theSSA2Interface}, {FrontEndType::MPA, theMPAInterface}, {FrontEndType::MPA2, theMPA2Interface}};
    CHIP_INTERFACE = CHIP_INTERFACE1;
}
PSInterface::~PSInterface() {}

ReadoutChipInterface* PSInterface::getInterface(Chip* pPS)
{
    if(pPS->getFrontEndType() == FrontEndType::SSA)
        return static_cast<SSAInterface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else if(pPS->getFrontEndType() == FrontEndType::SSA2)
        return static_cast<SSA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else if(pPS->getFrontEndType() == FrontEndType::MPA)
        return static_cast<MPAInterface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
    else
        return static_cast<MPA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);

    // return (CHIP_INTERFACE.find(pPS->getFrontEndType()))->second;
}

std::vector<uint8_t> PSInterface::readLUT(ReadoutChip* pPS, uint8_t pMode)
{
    std::vector<uint8_t> cLUT(0);
    if(pPS->getFrontEndType() == FrontEndType::MPA) { cLUT = theMPAInterface->readLUT(pPS, pMode); }
    return cLUT;
}
bool PSInterface::setInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop) { return getInterface(pPS)->setInjectionSchema(pPS, group, pVerifLoop); }
bool PSInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    return getInterface(pPS)->maskChannelsAndSetInjectionSchema(pPS, group, mask, inject, pVerifLoop);
}
// bool PSInterface::maskChannelGroup(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
//{
//   return getInterface(pPS)->maskChannelsGroup(pPS, group, pVerifLoop);
//}

bool PSInterface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize) { return getInterface(pChip)->ConfigureChipOriginalMask(pChip, pVerifLoop, pBlockSize); }

// To generalize
uint16_t PSInterface::ReadChipReg(Chip* pPS, const std::string& pRegName) { return getInterface(pPS)->ReadChipReg(pPS, pRegName); }

// To generalize
bool PSInterface::WriteChipReg(Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    // LOG(INFO) << BOLDRED << "glorp! " << RESET;
    return getInterface(pPS)->WriteChipReg(pPS, pRegName, pValue, pVerifLoop);
}

bool PSInterface::WriteChipMultReg(Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop) { return getInterface(pPS)->WriteChipMultReg(pPS, pVecReq, pVerifLoop); }

// To generalize
bool PSInterface::WriteChipAllLocalReg(ReadoutChip* pPS, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    return getInterface(pPS)->WriteChipAllLocalReg(pPS, dacName, localRegValues, pVerifLoop);
}

// To generalize
bool PSInterface::ConfigureChip(Chip* pPS, bool pVerifLoop, uint32_t pBlockSize) { return getInterface(pPS)->ConfigureChip(pPS, pVerifLoop, pBlockSize); }

void PSInterface::producePhaseAlignmentPattern(ReadoutChip* pChip, uint8_t pWait_ms)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->producePhaseAlignmentPattern(pChip, pWait_ms); }
    else if(pChip->getFrontEndType() == FrontEndType::SSA)
    {
        LOG(INFO) << BOLDMAGENTA << "No need to generate phase alignment pattern on SSA#" << +pChip->getId() << " when on a PS module" << RESET;
    }
}
void PSInterface::produceWordAlignmentPattern(ReadoutChip* pChip)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->produceWordAlignmentPattern(pChip); }
    else if(pChip->getFrontEndType() == FrontEndType::SSA)
    {
        LOG(INFO) << BOLDMAGENTA << "No need to generate word alignment pattern on SSA#" << +pChip->getId() << " when on a PS module" << RESET;
    }
}

bool PSInterface::enableInjection(ReadoutChip* pPS, bool inject, bool pVerifLoop)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->enableInjection(pPS, inject, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::MPA2)
    {
        return theMPA2Interface->enableInjection(pPS, inject, pVerifLoop);
    }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->enableInjection(pPS, inject, pVerifLoop);
    }
    else if(pPS->getFrontEndType() == FrontEndType::SSA2)
    {
        return theSSA2Interface->enableInjection(pPS, inject, pVerifLoop);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

//
std::vector<int> PSInterface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->decodeBendCode(pChip, pBendCode); }
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
    {
        return theMPA2Interface->decodeBendCode(pChip, pBendCode);
    }
    return std::vector<int>(0);
}
//
void PSInterface::digiInjection(ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->digiInjection(pChip, pInjections, pPattern); }
    // if(pChip->getFrontEndType() == FrontEndType::SSA) { theSSAInterface->digiInjection(pChip, pInjections, pPattern); }
    if(pChip->getFrontEndType() == FrontEndType::MPA2) { theMPA2Interface->digiInjection(pChip, pInjections, pPattern); }
    // if(pChip->getFrontEndType() == FrontEndType::SSA2) { theSSA2Interface->digiInjection(pChip, pInjections, pPattern); }
}

} // namespace Ph2_HwInterface
