#include "D19cBackendAlignmentFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cBackendAlignmentFWInterface::D19cBackendAlignmentFWInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {
}
D19cBackendAlignmentFWInterface::D19cBackendAlignmentFWInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId) {
    
    LOG (INFO) << BOLDYELLOW << "D19cBackendAlignmentFWInterface::D19cBackendAlignmentFWInterface Constructor" << RESET;
}
D19cBackendAlignmentFWInterface::~D19cBackendAlignmentFWInterface() {}

void D19cBackendAlignmentFWInterface::ParseResult(uint32_t pReply)
{
    fAlignerObject.fType = (pReply >> 24) & 0xF;
    if(fAlignerObject.fType == 0)
    {
        // bit slip of 3 bits
        fLineConfiguration.fMode    = (pReply & 0x00003000) >> 12;
        fLineConfiguration.fDelay   = (pReply & 0x000000F8) >> 3;
        fLineConfiguration.fBitslip = (pReply & 0x00000007) >> 0;

        // // bit slip of 4 bits electrical
        // fLineConfiguration.fMode    = (pReply & (0x3)) >> 13;
        // fLineConfiguration.fDelay   = (pReply & (0x1F)) >> 4;
        // fLineConfiguration.fBitslip = (pReply & (0xF<<0)) >> 0;
    }
    else if(fAlignerObject.fType == 1)
    {
        // bit slip of 4 bits electrical
        // fLineConfiguration.fDelay                  = (pReply & (0x1F<<19)) >> 19;
        // fLineConfiguration.fBitslip                = (pReply & (0xF<<15)) >> 15;
        // fStatus.fDone                   = (pReply & (0x1<<14)) >> 14;
        // fStatus.fWordAlignmentFSMstate  = (pReply & (0xF<<7)) >> 7;
        // fStatus.fPhaseAlignmentFSMstate = (pReply & (0xF<<0)) >> 0;

        // bit slip of 3 bits
        fLineConfiguration.fDelay       = (pReply & 0x00F80000) >> 19;
        fLineConfiguration.fBitslip     = (pReply & 0x00070000) >> 16;
        fStatus.fDone                   = (pReply & 0x00008000) >> 15;
        fStatus.fWordAlignmentFSMstate  = (pReply & 0x00000F00) >> 8;
        fStatus.fPhaseAlignmentFSMstate = (pReply & 0x0000000F) >> 0;
    }
}
uint8_t D19cBackendAlignmentFWInterface::ParseStatus()
{
    // maps to decode status of word and phase alignment FSM
    std::map<int, std::string> cPhaseFSMStateMap = {{0, "IdlePHASE"},
                                                    {1, "ResetIDELAYE"},
                                                    {2, "WaitResetIDELAYE"},
                                                    {3, "ApplyInitialDelay"},
                                                    {4, "CheckInitialDelay"},
                                                    {5, "InitialSampling"},
                                                    {6, "ProcessInitialSampling"},
                                                    {7, "ApplyDelay"},
                                                    {8, "CheckDelay"},
                                                    {9, "Sampling"},
                                                    {10, "ProcessSampling"},
                                                    {11, "WaitGoodDelay"},
                                                    {12, "FailedInitial"},
                                                    {13, "FailedToApplyDelay"},
                                                    {14, "TunedPHASE"},
                                                    {15, "Unknown"}};
    std::map<int, std::string> cWordFSMStateMap  = {{0, "IdleWORD or WaitIserdese"},
                                                   {1, "WaitFrame"},
                                                   {2, "ApplyBitslip"},
                                                   {3, "WaitBitslip"},
                                                   {4, "PatternVerification"},
                                                   {5, "Not Defined"},
                                                   {6, "Not Defined"},
                                                   {7, "Not Defined"},
                                                   {8, "Not Defined"},
                                                   {9, "Not Defined"},
                                                   {10, "Not Defined"},
                                                   {11, "Not Defined"},
                                                   {12, "FailedFrame"},
                                                   {13, "FailedVerification"},
                                                   {14, "TunedWORD"},
                                                   {15, "Unknown"}};

    uint8_t cStatus = 0;
    // read status
    uint32_t cReply = ReadReg("fc7_daq_stat.physical_interface_block.phase_tuning_reply");
    ParseResult(cReply);

    if(fAlignerObject.fType == 0)
    {
        LOG(INFO) << "\t\t Mode: " << +fLineConfiguration.fMode;
        LOG(INFO) << "\t\t Manual Delay: " << +fLineConfiguration.fDelay << ", Manual Bitslip: " << +fLineConfiguration.fBitslip;
        cStatus = 1;
    }
    else if(fAlignerObject.fType == 1)
    {
        LOG(INFO) << "\t\t Done: " << +fStatus.fDone << ", PA FSM: " << BOLDGREEN << cPhaseFSMStateMap[fStatus.fPhaseAlignmentFSMstate] << RESET << ", WA FSM: " << BOLDGREEN
                  << cWordFSMStateMap[fStatus.fWordAlignmentFSMstate] << RESET;
        LOG(INFO) << "\t\t Delay: " << +fLineConfiguration.fDelay << ", Bitslip: " << +fLineConfiguration.fBitslip;
        cStatus = 1;
    }
    else
        cStatus = 0;
    return cStatus;
}
void D19cBackendAlignmentFWInterface::SetAlignerObject(AlignerObject pAlignerObject)
{
    fAlignerObject.fHybrid = (pAlignerObject.fHybrid);
    fAlignerObject.fChip   = (pAlignerObject.fChip);
    fAlignerObject.fLine   = (pAlignerObject.fLine);
}
void D19cBackendAlignmentFWInterface::SetLineConfiguration(LineConfiguration pCnfg)
{
    fLineConfiguration.fMode          = pCnfg.fMode;
    fLineConfiguration.fDelay         = pCnfg.fDelay;
    fLineConfiguration.fBitslip       = pCnfg.fBitslip;
    fLineConfiguration.fPattern       = pCnfg.fPattern;
    fLineConfiguration.fPatternPeriod = pCnfg.fPatternPeriod;
    fLineConfiguration.fEnableL1      = pCnfg.fEnableL1;
    fLineConfiguration.fMasterLine    = pCnfg.fMasterLine;
}
void D19cBackendAlignmentFWInterface::ConfigureInput()
{
    fAlignerObject.fCommand = 0;
    fAlignerObject.fCommand += (fAlignerObject.fHybrid & 0xF) << 28;
    fAlignerObject.fCommand += (fAlignerObject.fChip & 0xF) << 24;
    fAlignerObject.fCommand += (fAlignerObject.fLine & 0xF) << 20;
}
void D19cBackendAlignmentFWInterface::InitializeConfiguration()
{
    fLineConfiguration.fMode          = 0;
    fLineConfiguration.fDelay         = 0;
    fLineConfiguration.fBitslip       = 0;
    fLineConfiguration.fPattern       = 0;
    fLineConfiguration.fPatternPeriod = 0;
    fLineConfiguration.fEnableL1      = 0;
    fLineConfiguration.fMasterLine    = 0;
}
void D19cBackendAlignmentFWInterface::InitializeAlignerObject()
{
    fAlignerObject.fHybrid  = (0 & 0xF) << 28;
    fAlignerObject.fChip    = (0 & 0xF) << 24;
    fAlignerObject.fLine    = (0 & 0xF) << 20;
    fAlignerObject.fType    = 0;
    fAlignerObject.fCommand = 0;
    fAlignerObject.fWait_us = 10000;
    // fAlignerObject.fWordAlignmentFSMstate="Unknown";
    // fAlignerObject.fPhaseAlignmentFSMstate="Unknown";
}
void D19cBackendAlignmentFWInterface::ConfigureAligner(LineConfiguration pCnfg)
{
    fLineConfiguration.fMode          = pCnfg.fMode;
    fLineConfiguration.fDelay         = pCnfg.fDelay;
    fLineConfiguration.fBitslip       = pCnfg.fBitslip;
    fLineConfiguration.fPattern       = pCnfg.fPattern;
    fLineConfiguration.fPatternPeriod = pCnfg.fPatternPeriod;
    fLineConfiguration.fEnableL1      = pCnfg.fEnableL1;
    fLineConfiguration.fMasterLine    = pCnfg.fMasterLine;
}

void D19cBackendAlignmentFWInterface::ConfigureCommandType(uint8_t pType)
{
    fAlignerObject.fType = pType;
    ConfigureInput();
    fAlignerObject.fCommand += (fAlignerObject.fType & 0xF) << 16;
}
void D19cBackendAlignmentFWInterface::SendControl(std::string pCommand)
{
    // set the pattern size
    ConfigureCommandType(5);
    uint32_t command_final = fAlignerObject.fCommand;
    if(pCommand == "Apply")
        command_final += 4;
    else if(pCommand == "WordAlignment")
        command_final += 2;
    else if(pCommand == "PhaseAlignment")
        command_final += 1;
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << pCommand << ": sending " << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << pCommand << ": sending " << std::hex << command_final << std::dec << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
}
void D19cBackendAlignmentFWInterface::SetLineMode(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    // command
    ConfigureCommandType(2);
    // set defaults - bit slip of 3 bits
    uint32_t mode_raw           = (fLineConfiguration.fMode & 0x3) << 12;
    uint32_t l1a_en_raw         = (fLineConfiguration.fMode == 0) ? ((fLineConfiguration.fEnableL1 & 0x1) << 11) : 0;
    uint32_t master_line_id_raw = (fLineConfiguration.fMode == 1) ? ((fLineConfiguration.fMasterLine & 0xF) << 8) : 0;
    uint32_t delay_raw          = (fLineConfiguration.fMode == 2) ? ((fLineConfiguration.fDelay & 0x1F) << 3) : 0;
    uint32_t bitslip_raw        = (fLineConfiguration.fMode == 2) ? ((fLineConfiguration.fBitslip & 0x7) << 0) : 0;

    // set defaults - bit slip of 4 bits..
    // uint32_t mode_raw           = (pMode & 0x3) << 13 ;
    // uint32_t l1a_en_raw         = (pMode == 0) ? ((pEnableL1 & 0x1) << 11) : 0;
    // uint32_t master_line_id_raw = (pMode == 1) ? ((pMasterLine & 0xF) << 8) : 0;
    // uint32_t delay_raw          = (pMode == 2) ? ((pDelay & 0x1F) << 4) : 0;
    // uint32_t bitslip_raw        = (pMode == 2) ? ((pBitSlip & 0xF) << 0) : 0;

    // form command
    uint32_t command_final = fAlignerObject.fCommand + mode_raw + l1a_en_raw + master_line_id_raw + delay_raw + bitslip_raw;
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface Line " << +fAlignerObject.fLine << " setting line mode to 0x" << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "D19cBackendAlignmentFWInterface Line " << +fAlignerObject.fLine << " setting line mode to 0x" << std::hex << command_final << std::dec << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
}
void D19cBackendAlignmentFWInterface::SetLinePattern(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    LOG(DEBUG) << BOLDBLUE << "Setting line pattern..." << RESET;
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    // command
    ConfigureCommandType(3);
    uint32_t len_raw       = (0xFF & fLineConfiguration.fPatternPeriod) << 0;
    uint32_t command_final = fAlignerObject.fCommand + len_raw;
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface Setting line pattern size to 0x" << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "D19cBackendAlignmentFWInterface Setting line pattern size to 0x" << std::hex << command_final << std::dec << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    // set the pattern
    ConfigureCommandType(4);
    uint8_t byte_id_raw = (0xFF & 0) << 8;
    uint8_t pattern_raw = (0xFF & fLineConfiguration.fPattern) << 0;
    command_final       = fAlignerObject.fCommand + byte_id_raw + pattern_raw;
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface Setting line pattern  to 0x" << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "D19cBackendAlignmentFWInterface Setting line pattern  to 0x" << std::hex << command_final << std::dec << RESET;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
}
uint8_t D19cBackendAlignmentFWInterface::GetLineStatus(AlignerObject pAlignerObject)
{
    // select FE
    SetAlignerObject(pAlignerObject);
    // print header
    LOG(DEBUG) << BOLDBLUE << "D19cBackendAlignmentFWInterface \t Hybrid: " << RESET << +fAlignerObject.fHybrid << BOLDBLUE << ", Chip: " << RESET << +fAlignerObject.fChip << BOLDBLUE
               << ", Line: " << RESET << +fAlignerObject.fLine;
    ConfigureCommandType(0);
    uint32_t command_final = fAlignerObject.fCommand;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface::PhaseTuner Get line status  0x" << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface::PhaseTuner Get line status  0x" << std::hex << command_final << std::dec << RESET;

    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
    uint8_t cStatus = ParseStatus();
    //
    ConfigureCommandType(1);
    command_final = fAlignerObject.fCommand;
    WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", command_final);
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "D19cBackendAlignmentFWInterface::PhaseTuner Get line status  0x" << std::hex << command_final << std::dec << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "D19cBackendAlignmentFWInterface::PhaseTuner Get line status  0x" << std::hex << command_final << std::dec << RESET;
    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
    cStatus = ParseStatus();
    return cStatus;
}
void D19cBackendAlignmentFWInterface::TunePhase(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    SetLineMode(pAlignerObject, pLineConfiguration);
    // perform phase alignment
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "\t..... running phase alignment...." << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "\t..... running phase alignment...." << RESET;
    SendControl("PhaseAlignment");
}
void D19cBackendAlignmentFWInterface::AlignWord(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern)
{
    SetLineMode(pAlignerObject, pLineConfiguration);
    if(pChangePattern) { SetLinePattern(fAlignerObject, fLineConfiguration); }
    // perform word alignment
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "\t..... running word alignment...." << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "\t..... running word alignment...." << RESET;
    SendControl("WordAlignment");
}
bool D19cBackendAlignmentFWInterface::TuneLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern)
{
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "Tuning line " << +pAlignerObject.fLine << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "Tuning line " << +pAlignerObject.fLine << RESET;
    TunePhase(pAlignerObject, pLineConfiguration);
    AlignWord(fAlignerObject, fLineConfiguration, pChangePattern);
    uint8_t cLineStatus = GetLineStatus(fAlignerObject);
    return (cLineStatus == 1);
}
bool D19cBackendAlignmentFWInterface::IsLineWordAligned(AlignerObject pAlignerObject) { return (fStatus.fWordAlignmentFSMstate == 14); }
bool D19cBackendAlignmentFWInterface::IsLinePhaseAligned(AlignerObject pAlignerObject) { return (fStatus.fPhaseAlignmentFSMstate == 14); }

} // namespace Ph2_HwInterface
