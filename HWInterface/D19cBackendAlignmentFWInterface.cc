#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWDescription/Chip.h"
#include "HWInterface/RegManager.h"
#include "Utils/ConsoleColor.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cBackendAlignmentFWInterface::D19cBackendAlignmentFWInterface(RegManager* theRegManager) : fTheRegManager(theRegManager) {}
D19cBackendAlignmentFWInterface::~D19cBackendAlignmentFWInterface() {}

void D19cBackendAlignmentFWInterface::SetAlignerObject(AlignerObject pAlignerObject)
{
    fAlignerObject.fHybrid  = (pAlignerObject.fHybrid);
    fAlignerObject.fChip    = (pAlignerObject.fChip);
    fAlignerObject.fLine    = (pAlignerObject.fLine);
    fAlignerObject.fOptical = (pAlignerObject.fOptical);
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
    fAlignerObject.fHybrid  = 0;
    fAlignerObject.fChip    = 0;
    fAlignerObject.fLine    = 0;
    fAlignerObject.fType    = 0;
    fAlignerObject.fCommand = 0;
    fAlignerObject.fWait_us = 10000;
    // fAlignerObject.fWordAlignmentFSMstate="Unknown";
    // fAlignerObject.fPhaseAlignmentFSMstate="Unknown";
}

void D19cBackendAlignmentFWInterface::Print()
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

    if(fAlignerObject.fType == 0 && fVerbose == 3)
    {
        LOG(INFO) << "\tHybrid:" << +fAlignerObject.fHybrid << " Chip:" << +fAlignerObject.fChip << " Line: " << +fAlignerObject.fLine;
        LOG(INFO) << "\t\t Mode: " << +fLineConfiguration.fMode;
        LOG(INFO) << "\t\t Manual Delay: " << +fLineConfiguration.fDelay << ", Manual Bitslip: " << +fLineConfiguration.fBitslip;
    }
    else if(fAlignerObject.fType == 1 && fVerbose == 3)
    {
        LOG(INFO) << "\tHybrid:" << +fAlignerObject.fHybrid << " Chip:" << +fAlignerObject.fChip << " Line: " << +fAlignerObject.fLine;
        LOG(INFO) << "\t\t Done: " << +fStatus.fDone << ", PA FSM: " << BOLDGREEN << cPhaseFSMStateMap[fStatus.fPhaseAlignmentFSMstate] << RESET << ", WA FSM: " << BOLDGREEN
                  << cWordFSMStateMap[fStatus.fWordAlignmentFSMstate] << RESET;
        LOG(INFO) << "\t\t Delay: " << +fLineConfiguration.fDelay << ", Bitslip: " << +fLineConfiguration.fBitslip;
    }
}

void D19cBackendAlignmentFWInterface::ClearConfig()
{
    fLineConfiguration.fMode          = 0xFF;
    fLineConfiguration.fBitslip       = 0xFF;
    fLineConfiguration.fDelay         = 0xFF;
    fLineConfiguration.fEnableL1      = 0xFF;
    fLineConfiguration.fMasterLine    = 0xFF;
    fLineConfiguration.fMode          = 0xFF;
    fLineConfiguration.fPattern       = 0xFF;
    fLineConfiguration.fPatternPeriod = 0xFF;
}
void D19cBackendAlignmentFWInterface::ClearStatus()
{
    fStatus.fDone                   = 0xFF;
    fStatus.fFSMstate               = 0xFF;
    fStatus.fPhaseAlignmentFSMstate = 0xFF;
    fStatus.fWordAlignmentFSMstate  = 0xFF;
}

void D19cBackendAlignmentFWInterface::SendCommand(std::string pCmdToTuner)
{
    fAlignerObject.fCommand = 0x00;
    fAlignerObject.fType    = fTunerControl[pCmdToTuner];
    // build command
    std::map<std::string, int> cMap = (fAlignerObject.fOptical == 1) ? fLineCnfg_Optical : fLineCnfg_Electrical;
    fAlignerObject.fCommand += fAlignerObject.fHybrid << 28;
    fAlignerObject.fCommand += fAlignerObject.fChip << 24;
    fAlignerObject.fCommand += fAlignerObject.fLine << 20;
    fAlignerObject.fCommand += fAlignerObject.fType << 16;
    if(pCmdToTuner == "Configure")
    {
        fAlignerObject.fCommand += fLineConfiguration.fMode << cMap["TunerMode"];
        if(fLineConfiguration.fMode == 0 && fAlignerObject.fOptical == 0) fAlignerObject.fCommand += fLineConfiguration.fEnableL1 << cMap["EnableL1A"];
        if(fLineConfiguration.fMode == 1 && fAlignerObject.fOptical == 0) fAlignerObject.fCommand += fLineConfiguration.fMasterLine << cMap["MasterLine"];
        if(fLineConfiguration.fMode == 2 && fAlignerObject.fOptical == 0) fAlignerObject.fCommand += fLineConfiguration.fDelay << cMap["Delay"];
        if(fLineConfiguration.fMode == 2) fAlignerObject.fCommand += fLineConfiguration.fBitslip << cMap["Bitslip"];
    }
    if(pCmdToTuner == "SetPatternLength")
    {
        if(fAlignerObject.fOptical == 0) fAlignerObject.fCommand += fLineConfiguration.fPatternPeriod;
    }
    if(pCmdToTuner == "SetSyncPattern")
    {
        if(fAlignerObject.fOptical == 0) fAlignerObject.fCommand += fLineConfiguration.fPattern;
    }
    if(pCmdToTuner == "RunTuner")
    {
        fAlignerObject.fCommand += 1 << fAutoTunerCommands["ApplyManual"];
        fAlignerObject.fCommand += 1 << fAutoTunerCommands["PhaseAlign"];
        fAlignerObject.fCommand += 1 << fAutoTunerCommands["WordAlign"];
    }
    if(pCmdToTuner == "Apply")
    {
        fAlignerObject.fCommand += 1 << fAutoTunerCommands["ApplyManual"];
        fAlignerObject.fCommand += 1 << fAutoTunerCommands["PhaseAlign"];
    }
    if(pCmdToTuner == "TunePhase") { fAlignerObject.fCommand += 1 << fAutoTunerCommands["PhaseAlign"]; }
    if(pCmdToTuner == "AlignLine") { fAlignerObject.fCommand += 1 << fAutoTunerCommands["WordAlign"]; }
    if(fVerbose == 1)
        LOG(INFO) << BOLDYELLOW << "D19cBackendAlignmentFWInterface::SendCommand " << pCmdToTuner << " 0x" << std::hex << fAlignerObject.fCommand << std::dec << " for Line#" << +fAlignerObject.fLine
                  << " on Hybrid#" << +fAlignerObject.fHybrid << " for Chip#" << +fAlignerObject.fChip << " Optical set to " << +fAlignerObject.fOptical << " Cmd type set to " << +fAlignerObject.fType
                  << " Tuner mode set to " << +fLineConfiguration.fMode << " Line bitslip set to " << +fLineConfiguration.fBitslip << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDYELLOW << "D19cBackendAlignmentFWInterface::SendCommand " << pCmdToTuner << " 0x" << std::hex << fAlignerObject.fCommand << std::dec << " for Line#" << +fAlignerObject.fLine
                   << " on Hybrid#" << +fAlignerObject.fHybrid << " for Chip#" << +fAlignerObject.fChip << " Optical set to " << +fAlignerObject.fOptical << " Cmd type set to "
                   << +fAlignerObject.fType << " Tuner mode set to " << +fLineConfiguration.fMode << RESET;

    fTheRegManager->WriteReg("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", fAlignerObject.fCommand);
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Command " << pCmdToTuner << " -> phase_tuning_ctrl = " << std::hex << fAlignerObject.fCommand << std::dec << std::endl;

    std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
}
void D19cBackendAlignmentFWInterface::GetReply(std::string pCmdToTuner)
{
    fAlignerObject.fReply = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.phase_tuning_reply");
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] phase_tuning_reply = " << std::hex << fAlignerObject.fReply << std::dec << std::endl;
    
    if(pCmdToTuner == "ReturnConfig")
    {
        std::map<std::string, int> cCnfgMap = (fAlignerObject.fOptical == 1) ? fTunerCnfgBitMap_Optical : fTunerCnfgBitMap_Electrical;
        fAlignerObject.fLine                = (fAlignerObject.fReply >> cCnfgMap["LineId"]) & 0xF;                        // 4 bits
        fAlignerObject.fType                = (fAlignerObject.fReply >> cCnfgMap["CmdCode"]) & 0xF;                       // 4 bits
        fLineConfiguration.fMode            = (fAlignerObject.fReply >> cCnfgMap["TunerMode"]) & 0x3;                     // 2 bits
        if(fAlignerObject.fOptical == 0) fLineConfiguration.fDelay = (fAlignerObject.fReply >> cCnfgMap["Delay"]) & 0x1F; // 5 bits
        fLineConfiguration.fBitslip = (fAlignerObject.fReply >> cCnfgMap["Bitslip"]) & 0xF;                               // 4 bits
    }
    else
    {
        std::map<std::string, int> cStatusMap = (fAlignerObject.fOptical == 1) ? fTunerStatusBitMap_Optical : fTunerStatusBitMap_Electrical;
        // line configuration
        fAlignerObject.fLine     = (fAlignerObject.fReply >> cStatusMap["LineId"]) & 0xF;
        fAlignerObject.fType     = (fAlignerObject.fReply >> cStatusMap["CmdCode"]) & 0xF;
        fLineConfiguration.fMode = (fAlignerObject.fReply >> cStatusMap["TunerMode"]) & 0x3; // 2 bits
        // status bits
        fStatus.fDone = (fAlignerObject.fReply >> cStatusMap["Done"]) & 0x1;
        if(fAlignerObject.fOptical == 0) fLineConfiguration.fDelay = (fAlignerObject.fReply >> cStatusMap["Delay"]) & 0x1F;
        fLineConfiguration.fBitslip    = (fAlignerObject.fReply >> cStatusMap["Bitslip"]) & 0xF;
        fStatus.fWordAlignmentFSMstate = (fAlignerObject.fReply >> cStatusMap["WordAlignerFSM"]) & 0xF;
        if(fAlignerObject.fOptical == 0) fStatus.fPhaseAlignmentFSMstate = (fAlignerObject.fReply >> cStatusMap["PhaseAlignerFSM"]) & 0xF;
    }
    if(fVerbose == 1)
        LOG(INFO) << BOLDYELLOW << "D19cBackendAlignmentFWInterface::GetReply " << pCmdToTuner << " 0x" << std::hex << fAlignerObject.fReply << std::dec << " for Line#" << +fAlignerObject.fLine
                  << " on Hybrid#" << +fAlignerObject.fHybrid << " for Chip#" << +fAlignerObject.fChip << " Optical set to " << +fAlignerObject.fOptical << " Command code set to "
                  << +fAlignerObject.fType << " Mode set to " << +fLineConfiguration.fMode << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDYELLOW << "D19cBackendAlignmentFWInterface::GetReply " << pCmdToTuner << " 0x" << std::hex << fAlignerObject.fReply << std::dec << " for Line#" << +fAlignerObject.fLine
                   << " on Hybrid#" << +fAlignerObject.fHybrid << " for Chip#" << +fAlignerObject.fChip << " Optical set to " << +fAlignerObject.fOptical << " Command code set to "
                   << +fAlignerObject.fType << " Mode set to " << +fLineConfiguration.fMode << RESET;
}
Reply D19cBackendAlignmentFWInterface::TunePhase(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    Reply cReply;
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    fLineConfiguration.fMode = fAlignmentModes["Auto"];
    SendCommand("Configure");
    if(fAlignerObject.fOptical == 0) // only applies for electrical
    {
        SendCommand("SetPatternLength");
    }
    SendCommand("TunePhase");
    ClearStatus();
    SendCommand("ReturnResult");
    GetReply("ReturnResult");
    cReply.fCnfg    = fLineConfiguration;
    cReply.fSuccess = IsLinePhaseAligned(); //(fStatus.fDone == 1 && IsLinePhaseAligned());
    Print();
    return cReply;
}
Reply D19cBackendAlignmentFWInterface::AlignWord(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern)
{
    Reply cReply;
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    fLineConfiguration.fMode = fAlignmentModes["Auto"];
    SendCommand("Configure");
    if(pChangePattern && fAlignerObject.fOptical == 0) // only applies for electrical
    {
        SendCommand("SetPatternLength");
        SendCommand("SetSyncPattern");
    }
    SendCommand("AlignLine");

    bool isDone                  = false;
    int  maxNumberOfIterations   = 10;
    int  currentInterationNumber = 0;
    while(!isDone && currentInterationNumber < maxNumberOfIterations)
    {
        ++currentInterationNumber;
        std::this_thread::sleep_for(std::chrono::microseconds(fAlignerObject.fWait_us));
        ClearStatus();
        SendCommand("ReturnResult");
        GetReply("ReturnResult");
        isDone = (fStatus.fDone == 1);
    }
    if(currentInterationNumber == maxNumberOfIterations) LOG(ERROR) << BOLDRED << "D19cBackendAlignmentFWInterface::AlignWord - Align Line procedure timed out" << RESET;
    cReply.fCnfg    = fLineConfiguration;
    cReply.fSuccess = IsLineWordAligned(); //(fStatus.fDone == 1 && IsLineWordAligned());
    Print();
    return cReply;
}
Reply D19cBackendAlignmentFWInterface::ManuallyConfigureLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    // fVerbose=3;
    Reply cReply;
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    fLineConfiguration.fMode = fAlignmentModes["Manual"];
    SendCommand("Configure");
    SendCommand("Apply"); // was "AlignLine"
    // SendCommand("ReturnConfig"); GetReply("ReturnConfig");

    // ClearStatus();
    SendCommand("ReturnConfig");
    GetReply("ReturnConfig");
    cReply.fCnfg    = fLineConfiguration;
    cReply.fSuccess = (fStatus.fDone == 1);
    Print();
    return cReply;
}
Reply D19cBackendAlignmentFWInterface::RetrieveConfig(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration)
{
    Reply cReply;
    // select FE
    SetAlignerObject(pAlignerObject);
    // configure aligner
    SetLineConfiguration(pLineConfiguration);
    SendCommand("ReturnConfig");
    GetReply("ReturnConfig");
    cReply.fCnfg    = fLineConfiguration;
    cReply.fSuccess = true;
    Print();
    return cReply;
}
bool D19cBackendAlignmentFWInterface::TuneLine(AlignerObject pAlignerObject, LineConfiguration pLineConfiguration, bool pChangePattern)
{
    if(fVerbose == 1)
        LOG(INFO) << BOLDBLUE << "Tuning line " << +pAlignerObject.fLine << RESET;
    else if(fVerbose == 2)
        LOG(DEBUG) << BOLDBLUE << "Tuning line " << +pAlignerObject.fLine << RESET;
    if(TunePhase(pAlignerObject, pLineConfiguration).fSuccess) { return AlignWord(fAlignerObject, fLineConfiguration, pChangePattern).fSuccess; }
    return false;
}
bool D19cBackendAlignmentFWInterface::IsLineWordAligned() { return (fStatus.fWordAlignmentFSMstate == 14); }
bool D19cBackendAlignmentFWInterface::IsLinePhaseAligned() { return (fStatus.fPhaseAlignmentFSMstate == 14); }

std::pair<bool, uint8_t> D19cBackendAlignmentFWInterface::PhaseTuneLine(const Chip* pChip, uint8_t pLineId, uint8_t pPattern, uint8_t pOptical)
{
    // EnablePrintout(true);
    std::pair<bool, uint8_t> cLineStatus;
    cLineStatus.first  = false;
    cLineStatus.second = 0;

    fAlignerObject.fHybrid  = pChip->getHybridId();
    fAlignerObject.fChip    = (pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId() % 8;
    fAlignerObject.fLine    = pLineId;
    fAlignerObject.fOptical = pOptical;

    fLineConfiguration.fPattern       = pPattern;
    fLineConfiguration.fPatternPeriod = 8;
    fLineConfiguration.fBitslip       = 0;
    fLineConfiguration.fDelay         = 0;
    auto cReply                       = this->TunePhase(fAlignerObject, fLineConfiguration);
    cLineStatus.first                 = cReply.fSuccess;
    cLineStatus.second                = cReply.fCnfg.fDelay;
    if(!cLineStatus.first)
    {
        LOG(INFO) << BOLDRED << "Could not phase align-BE data for BeBoard#" << +pChip->getBeBoardId() << " Hybrid#" << +pChip->getHybridId() << " Chip#" << +pChip->getId() << " line# " << +pLineId
                  << RESET;
    }
    return cLineStatus;
}
std::pair<bool, uint8_t> D19cBackendAlignmentFWInterface::WordAlignLine(const Chip* pChip, uint8_t pLineId, uint8_t pAlignmentPattern, uint8_t pPeriod, uint8_t pSamplingDelay, uint8_t pOptical)
{
    // EnablePrintout(true);
    std::pair<bool, uint8_t> cLineStatus;
    cLineStatus.first  = false;
    cLineStatus.second = 0;

    fAlignerObject.fHybrid  = pChip->getHybridId();
    fAlignerObject.fChip    = (pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId() % 8;
    fAlignerObject.fLine    = pLineId;
    fAlignerObject.fOptical = pOptical;

    fLineConfiguration.fPattern       = pAlignmentPattern;
    fLineConfiguration.fPatternPeriod = pPeriod;
    fLineConfiguration.fBitslip       = 0;
    fLineConfiguration.fDelay         = pSamplingDelay;
    auto cReply                       = this->AlignWord(fAlignerObject, fLineConfiguration, true);
    cLineStatus.first                 = cReply.fSuccess;
    cLineStatus.second                = cReply.fCnfg.fBitslip;
    return cLineStatus;
}
void D19cBackendAlignmentFWInterface::ManuallyConfigureLine(const Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip, uint8_t pOptical)
{
    EnablePrintout(true);
    fAlignerObject.fHybrid = pChip->getHybridId();
    fAlignerObject.fChip   = (pChip->getFrontEndType() == FrontEndType::CIC2) ? 0 : pChip->getId() % 8;
    fAlignerObject.fLine   = pLineId;

    fLineConfiguration.fDelay   = pPhase;
    fLineConfiguration.fBitslip = pBitslip;
    this->ManuallyConfigureLine(fAlignerObject, fLineConfiguration);
}
} // namespace Ph2_HwInterface
