/*
  FileName :                    BeBoardInterface.cc
  Content :                     User Interface to the Boards
  Programmer :                  Lorenzo BIDEGAIN, Nicolas PIERRE
  Version :                     1.0
  Date of creation :            31/07/14
  Support :                     mail to : lorenzo.bidegain@gmail.com nico.pierre@icloud.com
*/

#include "HWInterface/BeBoardInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
BeBoardInterface::BeBoardInterface(const BeBoardFWMap& pBoardMap) : fBoardMap(pBoardMap), fBoardFW(nullptr), fPrevBoardIdentifier(65535) {}

BeBoardInterface::~BeBoardInterface() {}

void BeBoardInterface::setBoard(uint16_t pBoardIdentifier)
{
    if(fPrevBoardIdentifier != pBoardIdentifier)
    {
        BeBoardFWMap::iterator i = fBoardMap.find(pBoardIdentifier);
        if(i == fBoardMap.end())
            LOG(ERROR) << BOLDRED << "The Board: " << +pBoardIdentifier << "  doesn't exist" << RESET;
        else
        {
            fBoardFW             = i->second;
            fPrevBoardIdentifier = pBoardIdentifier;
        }
    }
}

void BeBoardInterface::SetFileHandler(const BeBoard* pBoard, FileHandler* pHandler)
{
    setBoard(pBoard->getId());
    fBoardFW->setFileHandler(pHandler);
}
void BeBoardInterface::enableFileHandler(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->enableFileHandler();
}

void BeBoardInterface::disableFileHandler(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->disableFileHandler();
}

void BeBoardInterface::WriteBoardReg(BeBoard* pBoard, const std::string& pRegNode, const uint32_t& pVal)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->WriteReg(pRegNode, pVal);
}

void BeBoardInterface::WriteBlockBoardReg(BeBoard* pBoard, const std::string& pRegNode, const std::vector<uint32_t>& pValVec)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->WriteBlockReg(pRegNode, pValVec);
}

void BeBoardInterface::WriteBoardMultReg(BeBoard* pBoard, const std::vector<std::pair<std::string, uint32_t>>& pRegVec)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->WriteStackReg(pRegVec);
}

uint32_t BeBoardInterface::ReadBoardReg(BeBoard* pBoard, const std::string& pRegNode, bool updateRegs)
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    setBoard(pBoard->getId());
    uint32_t cRegValue = static_cast<uint32_t>(fBoardFW->ReadReg(pRegNode));
    return cRegValue;
}

void BeBoardInterface::ReadBoardMultReg(BeBoard* pBoard, std::vector<std::pair<std::string, uint32_t>>& pRegVec)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    for(auto& cReg: pRegVec) try
        {
            cReg.second = static_cast<uint32_t>(fBoardFW->ReadReg(cReg.first));
        }
        catch(...)
        {
            std::cerr << "Error while reading: " + cReg.first;
            throw;
        }
}

std::vector<uint32_t> BeBoardInterface::ReadBlockBoardReg(BeBoard* pBoard, const std::string& pRegNode, uint32_t pSize)
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    setBoard(pBoard->getId());
    return fBoardFW->ReadBlockRegValue(pRegNode, pSize);
}

uint32_t BeBoardInterface::getBoardInfo(const BeBoard* pBoard)
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    setBoard(pBoard->getId());
    return fBoardFW->getBoardInfo();
}

uint32_t BeBoardInterface::getBoardFirmwareVersion(const BeBoard* pBoard)
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    setBoard(pBoard->getId());
    return fBoardFW->getBoardFirmwareVersion();
}

BoardType BeBoardInterface::getBoardType(const BeBoard* pBoard)
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    setBoard(pBoard->getId());
    return fBoardFW->getBoardType();
}

void BeBoardInterface::ConfigureBoard(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ConfigureBoard(pBoard);
}

void BeBoardInterface::Start(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Start(pBoard);
}
void BeBoardInterface::Stop(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Stop();
}

void BeBoardInterface::Pause(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Pause();
}

void BeBoardInterface::Resume(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Resume();
}

uint32_t BeBoardInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    uint32_t dataSize = 0;

    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);
    if(theGuard.try_lock() == true)
    {
        setBoard(pBoard->getId());
        dataSize = fBoardFW->ReadData(pBoard, pBreakTrigger, pData, pWait);
        theGuard.unlock();
    }

    return dataSize;
}

void BeBoardInterface::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ReadNEvents(pBoard, pNEvents, pData, pWait);
}

void BeBoardInterface::ChipReset(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ChipReset();
}

void BeBoardInterface::ChipTrigger(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ChipTrigger();
}

void BeBoardInterface::ChipTestPulse(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ChipTestPulse();
}

void BeBoardInterface::ChipReSync(const BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ChipReSync();
}

const uhal::Node& BeBoardInterface::getUhalNode(const BeBoard* pBoard, const std::string& pStrPath)
{
    setBoard(pBoard->getId());
    return fBoardFW->getUhalNode(pStrPath);
}

uhal::HwInterface* BeBoardInterface::getHardwareInterface(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    return fBoardFW->getHardwareInterface();
}

void BeBoardInterface::SetForceStart(BeBoard* pBoard, bool bStart)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->SetForceStart(bStart);
}

void BeBoardInterface::PowerOn(BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->PowerOn();
}

void BeBoardInterface::PowerOff(BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->PowerOff();
}

void BeBoardInterface::ReadVer(BeBoard* pBoard)
{
    std::lock_guard<std::recursive_mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ReadVer();
}
} // namespace Ph2_HwInterface
