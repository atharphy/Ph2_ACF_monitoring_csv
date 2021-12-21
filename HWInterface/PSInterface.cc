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
    theSSAInterface = static_cast<SSAInterface*>(new SSAInterface(pBoardMap));
    theMPAInterface = static_cast<MPAInterface*>(new MPAInterface(pBoardMap));
}
PSInterface::~PSInterface() {}

std::vector<uint8_t> PSInterface::readLUT(ReadoutChip* pPS, uint8_t pMode)
{
    std::vector<uint8_t> cLUT(0);
    if(pPS->getFrontEndType() == FrontEndType::MPA) { cLUT = theMPAInterface->readLUT(pPS, pMode); }
    return cLUT;
}
bool PSInterface::setInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    bool toreturn = false;
    if(pPS->getFrontEndType() == FrontEndType::MPA) { toreturn = theMPAInterface->setInjectionSchema(pPS, group, pVerifLoop); }
    if(pPS->getFrontEndType() == FrontEndType::SSA) { toreturn = theSSAInterface->setInjectionSchema(pPS, group, pVerifLoop); }
    return toreturn;
}
bool PSInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    bool toreturn = false;
    if(pPS->getFrontEndType() == FrontEndType::MPA) { toreturn = theMPAInterface->maskChannelsAndSetInjectionSchema(pPS, group, mask, inject, pVerifLoop); }
    if(pPS->getFrontEndType() == FrontEndType::SSA) { toreturn = theSSAInterface->maskChannelsAndSetInjectionSchema(pPS, group, mask, inject, pVerifLoop); }
    return toreturn;
}
bool PSInterface::maskChannelGroup(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    bool toreturn = false;
    if(pPS->getFrontEndType() == FrontEndType::MPA) { toreturn = theMPAInterface->maskChannelGroup(pPS, group, pVerifLoop); }
    if(pPS->getFrontEndType() == FrontEndType::SSA) { toreturn = theSSAInterface->maskChannelGroup(pPS, group, pVerifLoop); }
    return toreturn;
}

bool PSInterface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA)
    {
        LOG(INFO) << BOLDMAGENTA << "ConfigureChipOriginalMask MPA PS Interface" << RESET;
        return theMPAInterface->ConfigureChipOriginalMask(pChip, pVerifLoop, pBlockSize);
    }
    else
    {
        return theSSAInterface->ConfigureChipOriginalMask(pChip, pVerifLoop, pBlockSize);
    }
}

// To generalize
uint16_t PSInterface::ReadChipReg(Chip* pPS, const std::string& pRegName)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->ReadChipReg(pPS, pRegName); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->ReadChipReg(pPS, pRegName);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

// To generalize
bool PSInterface::WriteChipReg(Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->WriteChipReg(pPS, pRegName, pValue, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->WriteChipReg(pPS, pRegName, pValue, pVerifLoop);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

bool PSInterface::WriteChipMultReg(Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    // need to or success
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->WriteChipMultReg(pPS, pVecReq, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->WriteChipMultReg(pPS, pVecReq, pVerifLoop);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

// To generalize
bool PSInterface::WriteChipAllLocalReg(ReadoutChip* pPS, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->WriteChipAllLocalReg(pPS, dacName, localRegValues, pVerifLoop); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->WriteChipAllLocalReg(pPS, dacName, localRegValues, pVerifLoop);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

// To generalize
bool PSInterface::ConfigureChip(Chip* pPS, bool pVerifLoop, uint32_t pBlockSize)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->ConfigureChip(pPS, pVerifLoop, pBlockSize); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->ConfigureChip(pPS, pVerifLoop, pBlockSize);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

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
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->enableInjection(pPS, inject, pVerifLoop);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
    return false;
}

std::vector<int> PSInterface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode) { return theMPAInterface->decodeBendCode(pChip, pBendCode); }
//
void PSInterface::digiInjection(ReadoutChip* pChip, std::vector<Injection> pInjections, uint8_t pPattern)
{
    if(pChip->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->digiInjection(pChip, pInjections, pPattern); }
    // add SSA here
    // if( pChip->getFrontEndType() == FrontEndType::SSA )
    // {
    //     theSSAInterface->WriteChipReg(pChip, "ENFLAGS_ALL", 0x0);
    //     theSSAInterface->WriteChipReg(pChip, "DigCalibPattern_L_ALL", pPattern);
    //     theSSAInterface->WriteChipReg(pChip, "CalPulse_duration", 0x01);
    //     for( auto cInj : pInjections )
    //     {
    //         theSSAInterface->WriteChipReg(pChip, "ENFLAGS_S" + std::to_string(cInj.fRow), 0x9);
    //     }

    // }
}

} // namespace Ph2_HwInterface
