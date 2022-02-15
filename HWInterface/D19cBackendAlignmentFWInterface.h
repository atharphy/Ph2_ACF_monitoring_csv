#ifndef __D19cBackendAlignmentFWInterface_H__
#define __D19cBackendAlignmentFWInterface_H__

#include "../Utils/Utilities.h"
#include "../Utils/easylogging++.h"
#include "BeBoard.h"
#include "RegManager.h"
#include <string>

namespace Ph2_HwInterface
{
struct AlignerObject
{
    uint8_t  fHybrid  = 0;
    uint8_t  fChip    = 0;
    uint8_t  fLine    = 0;
    uint8_t  fType    = 0;
    uint32_t fCommand = 0;
    uint32_t fWait_us = 10000;
};
struct LineConfiguration
{
    uint8_t fMode          = 0;
    uint8_t fDelay         = 0;
    uint8_t fBitslip       = 0;
    uint8_t fPattern       = 0;
    uint8_t fPatternPeriod = 0;
    uint8_t fEnableL1      = 0;
    uint8_t fMasterLine    = 0;
};
struct Status
{
    uint8_t fDone                   = 0;
    uint8_t fWordAlignmentFSMstate  = 0;
    uint8_t fPhaseAlignmentFSMstate = 0;
    uint8_t fFSMstate               = 0;
};

class D19cBackendAlignmentFWInterface : public RegManager
{
  public:
    D19cBackendAlignmentFWInterface(const std::string& puHalConfigFileName, uint32_t pBoardId);
    D19cBackendAlignmentFWInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable);

    ~D19cBackendAlignmentFWInterface();

    void              InitializeConfiguration();
    void              InitializeAlignerObject();
    void              SetAlignerObject(AlignerObject pAlignerObject);
    void              SetLineConfiguration(LineConfiguration pCnfg);
    LineConfiguration GetLineConfiguration() { return fLineConfiguration; };

    void    SetLineMode(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    void    SetLinePattern(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    uint8_t GetLineStatus(AlignerObject pAlignerObject);
    void    TunePhase(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    void    AlignWord(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern);
    bool    TuneLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern);
    bool    IsLineWordAligned(AlignerObject pAlignerObject);
    bool    IsLinePhaseAligned(AlignerObject pAlignerObject);

    void    setNbits(uint8_t pNbits){ fNbits = pNbits; }
    uint8_t getNbits(){ return fNbits; }

  private:
    AlignerObject     fAlignerObject;
    LineConfiguration fLineConfiguration;
    Status            fStatus;
    uint8_t           fVerbose{0};
    uint8_t           fNbits{3};

  private:
    void    ConfigureInput();
    void    ConfigureAligner(LineConfiguration pCnfg);
    void    ConfigureCommandType(uint8_t pType);
    void    ParseResult(uint32_t pReply);
    uint8_t ParseStatus();
    void    SendControl(std::string pCommand);
};
} // namespace Ph2_HwInterface
#endif
