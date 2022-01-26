#ifndef _FC7FpgaControlFWInterface_H__
#define __FC7FpgaControlFWInterface_H__

#include "../HWInterface/D19cFpgaConfig.h"
#include "BeBoardFWInterface.h"
#include <string>

namespace Ph2_HwInterface
{
class FC7FpgaControlFWInterface
{
  public:
    FC7FpgaControlFWInterface(BeBoardFWInterface* fBeBoardFW) : fFpgaConfig(nullptr), fBeBoardFW(fBeBoardFW){};

    void                     FlashProm(const std::string& strConfig, const char* pstrFile);
    void                     JumpToFpgaConfig(const std::string& strConfig);
    void                     DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest);
    std::vector<std::string> getFpgaConfigList();
    void                     DeleteFpgaConfig(const std::string& strId);
    const FpgaConfig*        GetConfiguringFpga();
    void                     checkIfUploading();
    void                     RebootBoard();

  private:
    D19cFpgaConfig*     fFpgaConfig;
    BeBoardFWInterface* fBeBoardFW;
};
} // namespace Ph2_HwInterface
#endif
