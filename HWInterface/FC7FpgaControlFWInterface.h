#ifndef _FC7FpgaControlFWInterface_H__
#define __FC7FpgaControlFWInterface_H__

#include "BeBoardFWInterface.h"
#include <string>

namespace Ph2_HwInterface
{
class D19cFpgaConfig;
class FC7FpgaControlFWInterface : public BeBoardFWInterface
{
  public:
    FC7FpgaControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId);
    FC7FpgaControlFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler);

    FC7FpgaControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable);
    FC7FpgaControlFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler);
    ~FC7FpgaControlFWInterface();

    void                     Initialize();
    void                     FlashProm(const std::string& strConfig, const char* pstrFile);
    void                     JumpToFpgaConfig(const std::string& strConfig);
    void                     DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest);
    std::vector<std::string> getFpgaConfigList();
    void                     DeleteFpgaConfig(const std::string& strId);
    const FpgaConfig*        GetConfiguringFpga();
    void                     checkIfUploading();
    void                     RebootBoard();

  private:
    D19cFpgaConfig* fFpgaConfig{nullptr};

}; // namespace Ph2_HwInterface
} // namespace Ph2_HwInterface
#endif
