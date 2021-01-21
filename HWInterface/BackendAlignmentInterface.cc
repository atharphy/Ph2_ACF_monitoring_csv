/*

        FileName :                     GbtInterface.cc
        Content :                      User Interface to the Cics
        Version :                      1.0
        Date of creation :             10/07/14

 */

#include "BackendAlignmentInterface.h"
// loggers + exceptions
#include "../Utils/ConsoleColor.h"
#include "../Utils/Exception.h"
#include "../Utils/easylogging++.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
BackendAlignmentInterface::BackendAlignmentInterface(const BeBoardFWMap& pBoardMap) : BeBoardInterface(pBoardMap)
{
    fObject.fHybrid  = 0;
    fObject.fChip    = 0;
    fObject.fLine    = 0;
    fObject.fType    = 0;
    fObject.fCommand = 0;
    //
    fLineConfig.fMode          = 0;
    fLineConfig.fDelay         = 0;
    fLineConfig.fBitslip       = 0;
    fLineConfig.fPattern       = 0;
    fLineConfig.fPatternPeriod = 0;
    //
    fStatus.fDone                   = 0;
    fStatus.fWordAlignmentFSMstate  = 0;
    fStatus.fPhaseAlignmentFSMstate = 0;
    fStatus.fFSMstate               = 0;
}
BackendAlignmentInterface::~BackendAlignmentInterface() {}

void BackendAlignmentInterface::SetLine(uint8_t pHybrid, uint8_t pChip, uint8_t pLine)
{
    this->SelectFrontEnd(pHybrid, pChip);
    this->SelectLine(pLine);
}
//
void BackendAlignmentInterface::ConfigurePattern(BeBoard* pBoard, uint8_t pPattern, uint16_t pPatternPeriod)
{
    fLineConfig.fPatternPeriod = pPatternPeriod;
    this->SetPatternLength();
    this->SendCommand(pBoard);
    fLineConfig.fPattern = pPattern;
    this->SetPattern();
    this->SendCommand(pBoard);
}

void BackendAlignmentInterface::PhaseAlign(BeBoard* pBoard)
{
    this->SetLineMode(0);
    this->SendCommand(pBoard);
    LOG(INFO) << BOLDBLUE << "Phase aligning line " << +fObject.fLine << RESET;
    // perform phase alignment
    this->SendControl(pBoard, "PhaseAlignment");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
void BackendAlignmentInterface::WordAlign(BeBoard* pBoard)
{
    this->SetLineMode(0);
    this->SendCommand(pBoard);
    LOG(INFO) << BOLDBLUE << "Phase aligning line " << +fObject.fLine << RESET;
    // perform phase alignment
    this->SendControl(pBoard, "WordAlignment");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
}
bool BackendAlignmentInterface::TuneLine(BeBoard* pBoard)
{
    this->SetLineMode(0);
    this->SendCommand(pBoard);
    LOG(INFO) << BOLDBLUE << "Tuning line " << +fObject.fLine << RESET;
    // perform phase alignment
    // LOG (INFO) << BOLDBLUE << "\t..... running phase alignment...." << RESET;
    this->SendControl(pBoard, "PhaseAlignment");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    // perform word alignment
    // LOG (INFO) << BOLDBLUE << "\t..... running word alignment...." << RESET;
    this->SendControl(pBoard, "WordAlignment");
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    uint8_t cLineStatus = GetLineStatus(pBoard);
    return (cLineStatus == 1);
}
void BackendAlignmentInterface::SendControl(BeBoard* pBoard, std::string pCommand)
{
    // control word
    SetControlWord(pCommand);
    LOG(DEBUG) << BOLDBLUE << pCommand << ": sending " << std::hex << encodeCommand() << std::dec << RESET;
    this->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", encodeCommand());
}
//
bool BackendAlignmentInterface::GetLineStatus(BeBoard* pBoard)
{
    LOG(INFO) << BOLDBLUE << "\t Hybrid: " << RESET << +fObject.fHybrid << BOLDBLUE << ", Chip: " << RESET << +fObject.fChip << BOLDBLUE << ", Line: " << RESET << +fObject.fLine;
    // select FE
    fObject.fType = 0;
    this->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", encodeCommand());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    bool cSuccess = GetStatus(pBoard);

    //
    fObject.fType = 1;
    this->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", encodeCommand());
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    cSuccess = cSuccess && GetStatus(pBoard);
    return cSuccess;
}
bool BackendAlignmentInterface::GetStatus(BeBoard* pBoard)
{
    // read status
    uint32_t cReply = this->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.phase_tuning_reply");
    this->ParseStatus(cReply);

    if(fObject.fType == 0)
    {
        LOG(INFO) << "\t\t Mode: " << +fLineConfig.fMode;
        LOG(INFO) << "\t\t Manual Delay: " << +fLineConfig.fDelay << ", Manual Bitslip: " << +fLineConfig.fBitslip;
        return true;
    }
    else if(fObject.fType == 1)
    {
        LOG(INFO) << "\t\t Done: " << +fStatus.fDone << ", PA FSM: " << BOLDGREEN << fPhaseFSMStateMap[fStatus.fPhaseAlignmentFSMstate] << RESET << ", WA FSM: " << BOLDGREEN
                  << fWordFSMStateMap[fStatus.fWordAlignmentFSMstate] << RESET;
        LOG(INFO) << "\t\t Delay: " << +fLineConfig.fDelay << ", Bitslip: " << +fLineConfig.fBitslip;
        return true;
    }
    else if(fObject.fType == 6)
    {
        LOG(INFO) << "\t\t Default FSM State: " << +fStatus.fFSMstate;
        return true;
    }
    else
        return false;
}
//
void BackendAlignmentInterface::SetControlWord(std::string pCommand)
{
    fObject.fType = 5;
    if(pCommand == "Apply")
        fObject.fCommand = 4;
    else if(pCommand == "WordAlignment")
        fObject.fCommand = 2;
    else if(pCommand == "PhaseAlignment")
        fObject.fCommand = 1;
}
void BackendAlignmentInterface::SetLineMode(uint8_t pMode, uint8_t pDelay, uint8_t pBitSlip, uint8_t pEnableL1, uint8_t pMasterLine)
{
    // command
    fObject.fType    = 2;
    fObject.fCommand = 0;

    // shift payload
    uint32_t mode_raw = (pMode & 0x3) << 12;
    // set defaults
    uint32_t l1a_en_raw         = (pMode == 0) ? ((pEnableL1 & 0x1) << 11) : 0;
    uint32_t master_line_id_raw = (pMode == 1) ? ((pMasterLine & 0xF) << 8) : 0;
    uint32_t delay_raw          = (pMode == 2) ? ((pDelay & 0x1F) << 3) : 0;
    uint32_t bitslip_raw        = (pMode == 2) ? ((pBitSlip & 0x7) << 0) : 0;
    // form command
    fObject.fCommand = mode_raw + l1a_en_raw + master_line_id_raw + delay_raw + bitslip_raw;
}
void BackendAlignmentInterface::SendCommand(BeBoard* pBoard) { this->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl", encodeCommand()); }

void BackendAlignmentInterface::SelectFrontEnd(uint8_t pHybrid, uint8_t pChip)
{
    fObject.fHybrid  = pHybrid;
    fObject.fChip    = pChip;
    fObject.fType    = 0;
    fObject.fCommand = 0;
}
void BackendAlignmentInterface::SelectLine(uint8_t pLine) { fObject.fLine = pLine; }
//
void BackendAlignmentInterface::SetPatternLength()
{
    // select FE
    fObject.fType          = 3;
    fObject.fCommand       = (0xFF & fLineConfig.fPatternPeriod) << 0;
    uint32_t command_final = encodeCommand();
    LOG(DEBUG) << BOLDBLUE << "Setting line pattern size to " << std::hex << command_final << std::dec << RESET;
}
void BackendAlignmentInterface::SetPattern()
{
    // set the pattern
    fObject.fType       = 4;
    uint8_t byte_id_raw = (0xFF & 0) << 8;
    uint8_t pattern_raw = (0xFF & fLineConfig.fPattern) << 0;
    fObject.fCommand    = byte_id_raw + pattern_raw;
}
//
void BackendAlignmentInterface::ParseStatus(uint32_t pReply)
{
    fObject.fType = (pReply >> 24) & 0xF;
    if(fObject.fType == 0)
    {
        fLineConfig.fMode    = (pReply & 0x00003000) >> 12;
        fLineConfig.fDelay   = (pReply & 0x000000F8) >> 3;
        fLineConfig.fBitslip = (pReply & 0x00000007) >> 0;
    }
    else if(fObject.fType == 1)
    {
        fLineConfig.fDelay              = (pReply & 0x00F80000) >> 19;
        fLineConfig.fBitslip            = (pReply & 0x00070000) >> 16;
        fStatus.fDone                   = (pReply & 0x00008000) >> 15;
        fStatus.fWordAlignmentFSMstate  = (pReply & 0x00000F00) >> 8;
        fStatus.fPhaseAlignmentFSMstate = (pReply & 0x0000000F) >> 0;
    }
    else if(fObject.fType == 6)
    {
        fStatus.fFSMstate = (pReply & 0x000000FF) >> 0;
    }
}

} // namespace Ph2_HwInterface
