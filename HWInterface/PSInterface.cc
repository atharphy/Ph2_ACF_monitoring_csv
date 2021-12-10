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
    theSSA2Interface = static_cast<SSA2Interface*>(new SSA2Interface(pBoardMap));
    theMPA2Interface = static_cast<MPA2Interface*>(new MPA2Interface(pBoardMap));
	const std::map<FrontEndType, ReadoutChipInterface*> CHIP_INTERFACE1 = {{FrontEndType::SSA,theSSAInterface},{FrontEndType::SSA2,theSSA2Interface},{FrontEndType::MPA,theMPAInterface},{FrontEndType::MPA2,theMPA2Interface}};
	CHIP_INTERFACE=CHIP_INTERFACE1;
}
PSInterface::~PSInterface() {}


ReadoutChipInterface* PSInterface::getInterface(Chip* pPS)
{

        if (pPS->getFrontEndType()==FrontEndType::SSA)
			return static_cast<SSAInterface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
        else if (pPS->getFrontEndType()==FrontEndType::SSA2)
			return static_cast<SSA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
        else if (pPS->getFrontEndType()==FrontEndType::MPA)
			return static_cast<MPAInterface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);
        else  
			return static_cast<MPA2Interface*>((CHIP_INTERFACE.find(pPS->getFrontEndType()))->second);

    //return (CHIP_INTERFACE.find(pPS->getFrontEndType()))->second;
}

std::vector<uint8_t> PSInterface::readLUT(ReadoutChip* pPS, uint8_t pMode)
{
    std::vector<uint8_t> cLUT(0);
    if(pPS->getFrontEndType() == FrontEndType::MPA) { cLUT = theMPAInterface->readLUT(pPS, pMode); }
    return cLUT;
}
bool PSInterface::setInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    return getInterface(pPS)->setInjectionSchema(pPS, group, pVerifLoop); 
}
bool PSInterface::maskChannelsAndSetInjectionSchema(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop)
{
    return getInterface(pPS)->maskChannelsAndSetInjectionSchema(pPS, group, mask, inject, pVerifLoop); 
}
bool PSInterface::maskChannelGroup(ReadoutChip* pPS, const std::shared_ptr<ChannelGroupBase> group, bool pVerifLoop)
{
    return getInterface(pPS)->maskChannelsGroup(pPS, group, pVerifLoop); 
}

bool PSInterface::ConfigureChipOriginalMask(ReadoutChip* pChip, bool pVerifLoop, uint32_t pBlockSize)
{

    return getInterface(pChip)->ConfigureChipOriginalMask(pChip, pVerifLoop, pBlockSize);
}

// To generalize
uint16_t PSInterface::ReadChipReg(Chip* pPS, const std::string& pRegName)
{


    return getInterface(pPS)->ReadChipReg(pPS, pRegName);
}

// To generalize
bool PSInterface::WriteChipReg(Chip* pPS, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    //LOG(INFO) << BOLDRED << "glorp! " << RESET;
    return getInterface(pPS)->WriteChipReg(pPS, pRegName, pValue, pVerifLoop);
}

bool PSInterface::WriteChipMultReg(Chip* pPS, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{

    return getInterface(pPS)->WriteChipMultReg(pPS, pVecReq, pVerifLoop);
}

// To generalize
bool PSInterface::WriteChipAllLocalReg(ReadoutChip* pPS, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{

    return getInterface(pPS)->WriteChipAllLocalReg(pPS, dacName, localRegValues, pVerifLoop); 

}

// To generalize
bool PSInterface::ConfigureChip(Chip* pPS, bool pVerifLoop, uint32_t pBlockSize)
{
    return getInterface(pPS)->ConfigureChip(pPS, pVerifLoop, pBlockSize);
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
void PSInterface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

void PSInterface::Pix_write(ReadoutChip* pChip, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Pix_write(pChip, cRegItem, row, pixel, data); 
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Pix_write(pChip, cRegItem, row, pixel, data); 
}

uint32_t PSInterface::Pix_read(ReadoutChip* pChip, ChipRegItem cRegItem, uint32_t row, uint32_t pixel) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Pix_read(pChip, cRegItem, row, pixel); 
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Pix_read(pChip, cRegItem, row, pixel); 
	return 0;
}

void PSInterface::Activate_async(Chip* pChip) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Activate_async(pChip); 
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Activate_async(pChip); 
}

void PSInterface::Activate_sync(Chip* pChip) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Activate_sync(pChip); 
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Activate_sync(pChip); 
}

void PSInterface::Activate_pp(Chip* pChip, uint8_t win) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Activate_pp(pChip, win); 
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Activate_pp(pChip, win); 
}

void PSInterface::Activate_ss(Chip* pChip, uint8_t win) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Activate_ss(pChip, win); 
    if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Activate_ss(pChip, win); 
}

void PSInterface::Activate_ps(Chip* pChip, uint8_t win) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA)
		return theMPAInterface->Activate_ps(pChip, win);
    else if(pChip->getFrontEndType() == FrontEndType::MPA2)
		return theMPA2Interface->Activate_ps(pChip, win);
}




void PSInterface::Set_calibration(Chip* pPS, uint32_t cal)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) 
	{ 
		theMPAInterface->Set_calibration(pPS, cal); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::MPA2) 
	{ 
		theMPA2Interface->Set_calibration(pPS, cal); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        theSSAInterface->Set_calibration(pPS, cal);
    }
    //else if(pPS->getFrontEndType() == FrontEndType::SSA2)
    //{
      //  theSSA2Interface->Set_calibration(pPS, cal);
    //}
    else
        LOG(ERROR) << "Bad chip for PS interface";
}

void PSInterface::Set_threshold(Chip* pPS, uint32_t th)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) 
	{ 
		theMPAInterface->Set_threshold(pPS, th); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::MPA2) 
	{ 
		theMPA2Interface->Set_threshold(pPS, th); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        theSSAInterface->Set_threshold(pPS, th);
    }
    //else if(pPS->getFrontEndType() == FrontEndType::SSA2)
    //{
      //  theSSA2Interface->Set_threshold(pPS, th);
    //}
    else
        LOG(ERROR) << "Bad chip for PS interface";
}

void PSInterface::ReadASEvent(ReadoutChip* pPS, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) 
	{ 
		theMPAInterface->ReadASEvent(pPS, pData, pSRange); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::MPA2) 
	{ 
		theMPA2Interface->ReadASEvent(pPS, pData, pSRange); 
	}
    else if(pPS->getFrontEndType() == FrontEndType::SSA)
    {
        theSSAInterface->ReadASEvent(pPS, pData, pSRange);
    }
    else if(pPS->getFrontEndType() == FrontEndType::SSA2)
    {
        theSSA2Interface->ReadASEvent(pPS, pData, pSRange);
    }
    else
        LOG(ERROR) << "Bad chip for PS interface";
}

bool PSInterface::enableInjection(ReadoutChip* pPS, bool inject, bool pVerifLoop)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) 
	{ 
		return theMPAInterface->enableInjection(pPS, inject, pVerifLoop); 
	}
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

void PSInterface::readAllBias(ReadoutChip* pPS)
{
    if(pPS->getFrontEndType() == FrontEndType::MPA) { theMPAInterface->readAllBias(pPS); }
    else if(pPS->getFrontEndType() == FrontEndType::MPA2) { theMPA2Interface->readAllBias(pPS); }
}

//
std::vector<int> PSInterface::decodeBendCode(ReadoutChip* pChip, uint8_t pBendCode) 
{ 
    if(pChip->getFrontEndType() == FrontEndType::MPA) 
	{ 
		return theMPAInterface->decodeBendCode(pChip, pBendCode);
	} 
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
