#ifndef __D19cBackendAlignmentFWInterface_H__
#define __D19cBackendAlignmentFWInterface_H__

#include <string>
#include <map>

namespace Ph2_HwDescription
{
class Chip;
}
namespace Ph2_HwInterface
{
struct AlignerObject
{
    // inputs
    uint8_t fHybrid = 0;
    uint8_t fChip   = 0;
    uint8_t fLine   = 0;
    uint8_t fType   = 0;
    // internal
    uint32_t fCommand = 0;
    // received
    uint32_t fReply = 0;
    // config
    uint32_t fWait_us = 100; // 1000
    uint8_t  fOptical = 0;
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
struct Reply
{
    LineConfiguration fCnfg;
    bool              fSuccess;
};

class RegManager;
class D19cBackendAlignmentFWInterface
{
  public:
    D19cBackendAlignmentFWInterface(RegManager* theRegManager);
    ~D19cBackendAlignmentFWInterface();

    void              InitializeConfiguration();
    void              InitializeAlignerObject();
    void              SetAlignerObject(AlignerObject pAlignerObject);
    void              SetLineConfiguration(LineConfiguration pCnfg);
    LineConfiguration GetLineConfiguration() { return fLineConfiguration; };

    Reply                    TunePhase(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    Reply                    AlignWord(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern);
    Reply                    ManuallyConfigureLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    Reply                    RetrieveConfig(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    bool                     TuneLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern);
    bool                     IsLineWordAligned();
    bool                     IsLinePhaseAligned();
    void                     EnablePrintout(bool pPrint = false) { fVerbose = (pPrint) ? 3 : 0; }
    std::pair<bool, uint8_t> PhaseTuneLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pOptical);
    std::pair<bool, uint8_t> WordAlignLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod, uint8_t pSamplingDelay, uint8_t pOptical);
    void                     ManuallyConfigureLine(const Ph2_HwDescription::Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip, uint8_t pOptical);

    RegManager* fTheRegManager {nullptr};

  private:
    AlignerObject     fAlignerObject;
    LineConfiguration fLineConfiguration;
    Status            fStatus;
    uint8_t           fVerbose{3};

  private:
    void ClearConfig();
    void ClearStatus();
    void GetReply(std::string pCommand);
    void Print();

    // maps needed to decode + ctrl phase tuner
    std::map<std::string, int> fAutoTunerCommands = {{"PhaseAlign", 0}, {"WordAlign", 1}, {"ApplyManual", 2}};

    // description + command type
    std::map<std::string, int> fTunerControl =
        {{"ReturnConfig", 0}, {"ReturnResult", 1}, {"Configure", 2}, {"SetPatternLength", 3}, {"SetSyncPattern", 4}, {"RunTuner", 5}, {"TunePhase", 5}, {"AlignLine", 5}};

    // map of bits for decoding configuration of electrical tuning
    std::map<std::string, int> fTunerCnfgBitMap_Electrical = {{"LineId", 28}, {"CmdCode", 24}, {"TunerMode", 13}, {"Delay", 4}, {"Bitslip", 0}};

    // map of bits for decoding configuration of optical tuning
    std::map<std::string, int> fTunerCnfgBitMap_Optical = {{"LineId", 28}, {"CmdCode", 24}, {"TunerMode", 12}, {"Bitslip", 0}};

    // map of bits for decoding status electrical tuning
    std::map<std::string, int> fTunerStatusBitMap_Electrical = {
        {"LineId", 28},
        {"CmdCode", 24},
        {"Delay", 19},
        {"Bitslip", 15},
        {"Done", 14},
        {"WordAlignerFSM", 7},
        {"PhaseAlignerFSM", 0},
    };

    // map of bits for decoding status optical tuning
    std::map<std::string, int> fTunerStatusBitMap_Optical = {{"LineId", 28}, {"CmdCode", 24}, {"Bitslip", 16}, {"Done", 15}, {"WordAlignerFSM", 8}};

    // map of bits for setting configuration of line [electrical]
    std::map<std::string, int> fLineCnfg_Electrical = {{"TunerMode", 13}, {"EnableL1A", 11}, {"MasterLine", 8}, {"Delay", 4}, {"Bitslip", 0}};

    // map of bits for setting configuration of line [optical]
    std::map<std::string, int> fLineCnfg_Optical = {{"TunerMode", 12}, {"Bitslip", 0}};

    // map of alignment modes
    std::map<std::string, int> fAlignmentModes = {{"Manual", 2}, {"Auto", 0}};

    void SendCommand(std::string pCmdToTuner);
};
} // namespace Ph2_HwInterface
#endif
