#ifndef __D19cBackendAlignmentFWInterface_H__
#define __D19cBackendAlignmentFWInterface_H__

#include <cstdint>
#include <map>
#include <string>

namespace Ph2_HwDescription
{
class Chip;
}
namespace Ph2_HwInterface
{

class PhaseTuningControl
{
  public:
    PhaseTuningControl(bool isOptical) : fIsOptical(isOptical) {};
    enum struct Command {ReturnConfig = 0, ReturnResult = 1, Configure = 2, SetPatternLength = 3, SetSyncPattern = 4, Align = 5};
    enum struct Mode    {Auto = 0, Slave = 1, Manual = 2};
    // setter
    void setBitSlip        (uint8_t theBitSlip)       {fBitSlip = theBitSlip;}
    void setDelay          (uint8_t theDelay)         {fDelay = theDelay;}
    void setPatternLenght  (uint8_t thePatternLenght) {fPatternLenght = thePatternLenght;}
    void setSyncPattern    (uint8_t theSyncPattern)   {fSyncPattern = theSyncPattern;}
    void setEnableSync     (bool enableSync)          {fEnableSync = enableSync;}
    void setDoWordAlignmen (bool doWordAlignment)     {fDoWordAlignment = doWordAlignment;}
    void setDoPhaseAlignmen(bool doPhaseAlignment)    {fDoPhaseAlignment = doPhaseAlignment;}
    void setApplyManual    (bool applyManual)         {fApplyManual = applyManual;}
    void setEnablePRBS     (bool enablePRBS)          {fEnablePRBS = enablePRBS;}
    void setMasterLineId   (uint8_t theMasterLineId)  {fMasterLineId = theMasterLineId;}
    void setEnableLFSR     (bool enableLFSR)          {fEnableLFSR = enableLFSR;}
    void setEnableL1A      (bool enableL1A)           {fEnableL1A = enableL1A;}
    void setEnableLCC      (bool enableLCC)           {fEnableLCC = enableLCC;}
    void setMode           (Mode theMode)             {fMode = theMode;}
    void setCommand        (Command theCommand)       {fCommand = theCommand;}
    void setLineId         (uint8_t theLineId)        {fLineId = theLineId;}
    void setChipId         (uint8_t theChipId)        {fChipId = theChipId;}
    void setHybridId       (uint8_t theHybridId)      {fHybridId = theHybridId;}

    void resetCommandBits();

    // getter
    bool    getIsOptical       () const {return fIsOptical;}
    uint8_t getBitSlip         () const {return fBitSlip;}
    uint8_t getDelay           () const {return fDelay;}
    uint8_t getPatternLenght   () const {return fPatternLenght;}
    uint8_t getSyncPattern     () const {return fSyncPattern;}
    bool    getDoWordAlignment () const {return fDoWordAlignment;}
    bool    getDoPhaseAlignment() const {return fDoPhaseAlignment;}
    bool    getApplyManual     () const {return fApplyManual;}
    uint8_t getMasterLineId    () const {return fMasterLineId;}
    bool    getEnableSync      () const {return fEnableSync;}
    bool    getEnablePRBS      () const {return fEnablePRBS;}
    bool    getEnableLFSR      () const {return fEnableLFSR;}
    bool    getEnableL1A       () const {return fEnableL1A;}
    bool    getEnableLCC       () const {return fEnableLCC;}
    Mode    getMode            () const {return fMode;}
    Command getCommand         () const {return fCommand;}
    uint8_t getLineId          () const {return fLineId;}
    uint8_t getChipId          () const {return fChipId;}
    uint8_t getHybridId        () const {return fHybridId;}

    uint32_t encodeCommand() const;

  private:
    bool    fIsOptical        {true};
    uint8_t fBitSlip          {0};
    uint8_t fDelay            {0};
    uint8_t fPatternLenght    {0};
    uint8_t fSyncPattern      {0};
    bool    fDoWordAlignment  {false};
    bool    fDoPhaseAlignment {false};
    bool    fApplyManual      {false};
    uint8_t fMasterLineId     {0};
    bool    fEnableSync       {false};
    bool    fEnablePRBS       {false};
    bool    fEnableLFSR       {false};
    bool    fEnableL1A        {false};
    bool    fEnableLCC        {false};
    Mode    fMode             {Mode::Auto};
    Command fCommand          {Command::ReturnConfig};
    uint8_t fLineId           {0};
    uint8_t fChipId           {0};
    uint8_t fHybridId         {0};

    uint32_t fWait_us {100};
};

class PhaseTuningReply
{
  public:
    PhaseTuningReply(const PhaseTuningControl& thePhaseTuningControl) : fPhaseTuningControl(thePhaseTuningControl) {};

  private:
    const PhaseTuningControl fPhaseTuningControl;
};

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
    LineConfiguration GetLineConfiguration() { return fLineConfiguration; };

    void AlignWord(uint8_t hybridId, uint8_t lineId, uint8_t chipId = 0);

    Reply                    TunePhase(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration);
    Reply                    AlignWord(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern);
    bool                     IsLineWordAligned();
    bool                     IsLinePhaseAligned();

    void SetIsOptical(bool isOptical) {fIsOptical = isOptical;}
    
  private:
    RegManager* fTheRegManager{nullptr};
    void              SetLineConfiguration(LineConfiguration pCnfg);
    void              SetAlignerObject(AlignerObject pAlignerObject);
    AlignerObject     fAlignerObject;
    LineConfiguration fLineConfiguration;
    Status            fStatus;
    uint8_t           fVerbose{3};
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

    std::map<std::string, int> fFlagBit = {{"SyncEn", 8}, {"PrbsEn", 9}, {"LfsrEn", 10}};

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
    std::map<std::string, int> fLineCnfg_Optical = {{"TunerMode", 13}, {"Bitslip", 0}};

    // map of alignment modes
    std::map<std::string, int> fAlignmentModes = {{"Manual", 2}, {"Auto", 0}};

    void SendCommand(std::string pCmdToTuner);

    void writeCommand(uint32_t phaseTunerCommand);

    bool fIsOptical {true};
};
} // namespace Ph2_HwInterface
#endif
