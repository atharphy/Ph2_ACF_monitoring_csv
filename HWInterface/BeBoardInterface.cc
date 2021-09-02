/*
  FileName :                    BeBoardInterface.cc
  Content :                     User Interface to the Boards
  Programmer :                  Lorenzo BIDEGAIN, Nicolas PIERRE
  Version :                     1.0
  Date of creation :            31/07/14
  Support :                     mail to : lorenzo.bidegain@gmail.com nico.pierre@icloud.com
*/

#include "BeBoardInterface.h"

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
            LOG(INFO) << "The Board: " << +pBoardIdentifier << "  doesn't exist";
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

void BeBoardInterface::setPowerSupplyClient(const Ph2_HwDescription::BeBoard* pBoard, TCPClient* fPowerSupplyClient)
{
    setBoard(pBoard->getId());
    fBoardFW->setPowerSupplyClient(fPowerSupplyClient);
}
#ifdef __TCP_SERVER__
void BeBoardInterface::setTestcardClient(const Ph2_HwDescription::BeBoard* pBoard, TCPClient* fTestcardClient)
{
    setBoard(pBoard->getId());
    fBoardFW->setTestcardClient(fTestcardClient);
}
#endif

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
    setBoard(pBoard->getId());
    fBoardFW->WriteReg(pRegNode, pVal);
    pBoard->setReg(pRegNode, pVal);
}

void BeBoardInterface::WriteBlockBoardReg(BeBoard* pBoard, const std::string& pRegNode, const std::vector<uint32_t>& pValVec)
{
    setBoard(pBoard->getId());
    fBoardFW->WriteBlockReg(pRegNode, pValVec);
}

void BeBoardInterface::WriteBoardMultReg(BeBoard* pBoard, const std::vector<std::pair<std::string, uint32_t>>& pRegVec)
{
    setBoard(pBoard->getId());
    fBoardFW->WriteStackReg(pRegVec);
    for(const auto& cReg: pRegVec) pBoard->setReg(cReg.first, cReg.second);
}

uint32_t BeBoardInterface::ReadBoardReg(BeBoard* pBoard, const std::string& pRegNode)
{
    setBoard(pBoard->getId());
    uint32_t cRegValue = static_cast<uint32_t>(fBoardFW->ReadReg(pRegNode));
    pBoard->setReg(pRegNode, cRegValue);
    return cRegValue;
}

void BeBoardInterface::ReadBoardMultReg(BeBoard* pBoard, std::vector<std::pair<std::string, uint32_t>>& pRegVec)
{
    setBoard(pBoard->getId());
    for(auto& cReg: pRegVec) try
        {
            cReg.second = static_cast<uint32_t>(fBoardFW->ReadReg(cReg.first));
            pBoard->setReg(cReg.first, cReg.second);
        }
        catch(...)
        {
            std::cerr << "Error while reading: " + cReg.first;
            throw;
        }
}

void BeBoardInterface::selectLink(BeBoard* pBoard, uint8_t pLinkId, uint32_t pWait_ms)
{
    setBoard(pBoard->getId());
    return fBoardFW->selectLink(pLinkId, pWait_ms);
}
// uint16_t BeBoardInterface::ParseEvents(const BeBoard* pBoard, const std::vector<uint32_t>& pData)
// {
//   setBoard(pBoard->getId());
//   return fBoardFW->ParseEvents (pData);
// }

std::vector<uint32_t> BeBoardInterface::ReadBlockBoardReg(BeBoard* pBoard, const std::string& pRegNode, uint32_t pSize)
{
    setBoard(pBoard->getId());
    return fBoardFW->ReadBlockRegValue(pRegNode, pSize);
}

uint32_t BeBoardInterface::getBoardInfo(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    return fBoardFW->getBoardInfo();
}

BoardType BeBoardInterface::getBoardType(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    return fBoardFW->getBoardType();
}

void BeBoardInterface::ConfigureBoard(const BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ConfigureBoard(pBoard);
}

void BeBoardInterface::Start(BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Start();
}

void BeBoardInterface::Stop(BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Stop();
}

void BeBoardInterface::Pause(BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Pause();
}

void BeBoardInterface::Resume(BeBoard* pBoard)
{
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->Resume();
}

uint32_t BeBoardInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    uint32_t dataSize = 0;

    std::unique_lock<std::mutex> theGuard(theMtx, std::defer_lock);
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
    std::lock_guard<std::mutex> theGuard(theMtx);

    setBoard(pBoard->getId());
    fBoardFW->ReadNEvents(pBoard, pNEvents, pData, pWait);
}

void BeBoardInterface::ChipReset(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->ChipReset();
}

void BeBoardInterface::ChipTrigger(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->ChipTrigger();
}

void BeBoardInterface::ChipTestPulse(const BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->ChipTestPulse();
}

void BeBoardInterface::ChipReSync(const BeBoard* pBoard)
{
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

void BeBoardInterface::FlashProm(BeBoard* pBoard, const std::string& strConfig, const char* pstrFile)
{
    setBoard(pBoard->getId());
    fBoardFW->FlashProm(strConfig, pstrFile);
}

void BeBoardInterface::JumpToFpgaConfig(BeBoard* pBoard, const std::string& strConfig)
{
    setBoard(pBoard->getId());
    fBoardFW->JumpToFpgaConfig(strConfig);
}

void BeBoardInterface::DownloadFpgaConfig(BeBoard* pBoard, const std::string& strConfig, const std::string& strDest)
{
    setBoard(pBoard->getId());
    fBoardFW->DownloadFpgaConfig(strConfig, strDest);
}

const FpgaConfig* BeBoardInterface::GetConfiguringFpga(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    return fBoardFW->GetConfiguringFpga();
}

std::vector<std::string> BeBoardInterface::getFpgaConfigList(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    return fBoardFW->getFpgaConfigList();
}

void BeBoardInterface::DeleteFpgaConfig(BeBoard* pBoard, const std::string& strId)
{
    setBoard(pBoard->getId());
    fBoardFW->DeleteFpgaConfig(strId);
}

void BeBoardInterface::RebootBoard(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->RebootBoard();
}

void BeBoardInterface::SetForceStart(BeBoard* pBoard, bool bStart)
{
    setBoard(pBoard->getId());
    fBoardFW->SetForceStart(bStart);
}

void BeBoardInterface::PowerOn(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->PowerOn();
}

void BeBoardInterface::PowerOff(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->PowerOff();
}

void BeBoardInterface::ReadVer(BeBoard* pBoard)
{
    setBoard(pBoard->getId());
    fBoardFW->ReadVer();
}
} // namespace Ph2_HwInterface
