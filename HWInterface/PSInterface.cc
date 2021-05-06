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

void PSInterface::LinkLpGBT(D19clpGBTInterface* pLpGBTInterface, lpGBT* pLpGBT)
{
    theSSAInterface->LinkLpGBT(pLpGBTInterface, pLpGBT);
    theMPAInterface->LinkLpGBT(pLpGBTInterface, pLpGBT);
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

// To generalize
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

void PSInterface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

void PSInterface::Pix_write(ReadoutChip* cPS, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data) { return theMPAInterface->Pix_write(cPS, cRegItem, row, pixel, data); }

uint32_t PSInterface::Pix_read(ReadoutChip* cPS, ChipRegItem cRegItem, uint32_t row, uint32_t pixel) { return theMPAInterface->Pix_read(cPS, cRegItem, row, pixel); }

void PSInterface::Activate_async(Chip* pPS) { return theMPAInterface->Activate_async(pPS); }

void PSInterface::Activate_sync(Chip* pPS) { return theMPAInterface->Activate_sync(pPS); }

void PSInterface::Activate_pp(Chip* pPS, uint8_t win) { return theMPAInterface->Activate_pp(pPS, win); }

void PSInterface::Activate_ss(Chip* pPS, uint8_t win) { return theMPAInterface->Activate_ss(pPS, win); }

void PSInterface::Activate_ps(Chip* pPS, uint8_t win) { return theMPAInterface->Activate_ps(pPS, win); }

void PSInterface::Pix_Smode(ReadoutChip* pPS, uint32_t p, std::string smode = "edge") { return theMPAInterface->Pix_Smode(pPS, p, smode); }

void PSInterface::Enable_pix_BRcal(ReadoutChip* pPS, uint32_t p, std::string polarity, std::string smode) { return theMPAInterface->Enable_pix_BRcal(pPS, p, polarity, smode); }

void PSInterface::Enable_pix_counter(ReadoutChip* pPS, uint32_t p) { return theMPAInterface->Enable_pix_counter(pPS, p); }

void PSInterface::Enable_pix_sync(ReadoutChip* pPS, uint32_t p) { return theMPAInterface->Enable_pix_sync(pPS, p); }

void PSInterface::Disable_pixel(ReadoutChip* pPS, uint32_t p) { return theMPAInterface->Disable_pixel(pPS, p); }

void PSInterface::Enable_pix_digi(ReadoutChip* pPS, uint32_t p) { return theMPAInterface->Enable_pix_digi(pPS, p); }

void PSInterface::Pix_Set_enable(ReadoutChip* pPS,
                                 uint32_t     p,
                                 uint32_t     PixelMask = 1,
                                 uint32_t     Polarity  = 1,
                                 uint32_t     EnEdgeBR  = 1,
                                 uint32_t     EnLevelBR = 0,
                                 uint32_t     Encount   = 0,
                                 uint32_t     DigCal    = 0,
                                 uint32_t     AnCal     = 0,
                                 uint32_t     BRclk     = 0)
{
    return theMPAInterface->Pix_Set_enable(pPS, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void PSInterface::Set_calibration(Chip* pPS, uint32_t cal)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->Set_calibration(pPS, cal); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->Set_calibration(pPS, cal);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
}

void PSInterface::Set_threshold(Chip* pPS, uint32_t th)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->Set_threshold(pPS, th); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->Set_threshold(pPS, th);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
}

void PSInterface::ReadASEvent(ReadoutChip* pPS, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { return theMPAInterface->ReadASEvent(pPS, pData, pSRange); }
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        return theSSAInterface->ReadASEvent(pPS, pData, pSRange);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
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

void PSInterface::readAllBias(ReadoutChip* pPS)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->readAllBias(pPS); }
}

//
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

uint32_t PSInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    setBoard(0);
    return fBoardFW->ReadData(pBoard, pBreakTrigger, pData, pWait);
}

void PSInterface::Cleardata()
{
    setBoard(0);
    // fBoardFW->Cleardata( );
}

} // namespace Ph2_HwInterface
