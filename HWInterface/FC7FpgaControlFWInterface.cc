#include "FC7FpgaControlFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
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
    if(!fFpgaConfig) fFpgaConfig = new D19cFpgaConfig(fBeBoardFW);
}

const FpgaConfig* FC7FpgaControlFWInterface::GetConfiguringFpga() { return (const FpgaConfig*)fFpgaConfig; }

void FC7FpgaControlFWInterface::RebootBoard()
{
    if(!fFpgaConfig) fFpgaConfig = new D19cFpgaConfig(fBeBoardFW);
    fFpgaConfig->resetBoard();
}

} // namespace Ph2_HwInterface
