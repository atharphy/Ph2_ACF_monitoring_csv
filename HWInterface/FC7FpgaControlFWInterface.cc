#include "FC7FpgaControlFWInterface.h"
#include "D19cFpgaConfig.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{


FC7FpgaControlFWInterface::FC7FpgaControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId) : BeBoardFWInterface(puHalConfigFileName, pBoardId)
{
    std::cout << "Constr#0\n";
    fFpgaConfig = nullptr;
}
FC7FpgaControlFWInterface::FC7FpgaControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler) : BeBoardFWInterface(puHalConfigFileName, pBoardId)
{

    std::cout << "Constr#1\n";
    fFpgaConfig = nullptr;
}
FC7FpgaControlFWInterface::FC7FpgaControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable) : BeBoardFWInterface(pId, pUri, pAddressTable)
{

    std::cout << "Constr#2\n";
    fFpgaConfig = nullptr;
}
FC7FpgaControlFWInterface::FC7FpgaControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler) : BeBoardFWInterface(pId, pUri, pAddressTable)
{

    std::cout << "Constr#3\n";
    fFpgaConfig = nullptr;
}
FC7FpgaControlFWInterface::~FC7FpgaControlFWInterface() {delete fFpgaConfig; fFpgaConfig = nullptr;}

void FC7FpgaControlFWInterface::Initialize()
{
    if(fFpgaConfig){ std::cout << "On Initialize fFpgaConfig exists... deleting\n"; fFpgaConfig = nullptr; }
    std::cout << "Creating FPGAconfig object..\n"; 
    fFpgaConfig = new D19cFpgaConfig(this);
}
void FC7FpgaControlFWInterface::FlashProm(const std::string& strConfig, const char* pstrFile)
{
    checkIfUploading();

    fFpgaConfig->runUpload(strConfig, pstrFile);
}

void FC7FpgaControlFWInterface::JumpToFpgaConfig(const std::string& strConfig)
{
    checkIfUploading();

    fFpgaConfig->jumpToImage(strConfig);
}

void FC7FpgaControlFWInterface::DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest)
{
    checkIfUploading();
    fFpgaConfig->runDownload(strConfig, strDest.c_str());
}

std::vector<std::string> FC7FpgaControlFWInterface::getFpgaConfigList()
{
    checkIfUploading();
    return fFpgaConfig->getFirmwareImageNames();
}

void FC7FpgaControlFWInterface::DeleteFpgaConfig(const std::string& strId)
{
    checkIfUploading();
    fFpgaConfig->deleteFirmwareImage(strId);
}

void FC7FpgaControlFWInterface::checkIfUploading()
{
    if(fFpgaConfig && fFpgaConfig->getUploadingFpga() > 0) throw Exception("This board is uploading an FPGA configuration"); 
}

const FpgaConfig* FC7FpgaControlFWInterface::GetConfiguringFpga() { return (const FpgaConfig*)fFpgaConfig; }

void FC7FpgaControlFWInterface::RebootBoard()
{
    fFpgaConfig->resetBoard();
}
} // namespace Ph2_HwInterface