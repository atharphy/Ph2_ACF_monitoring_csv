/*

        FileName :                     CicInterface.cc
        Content :                      User Interface to the Cics
        Version :                      1.0
        Date of creation :             10/07/14

 */

#include "CicInterface.h"
#include "BeBoardFWInterface.h"
#include "D19cFWInterface.h"
#include "ReadoutChipInterface.h"

#define DEV_FLAG 0
// #define COUNT_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
CicInterface::CicInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap)
{
    fFeStates.clear(); // 8 FEs
    for(size_t cIndex = 0; cIndex < 8; cIndex++) fFeStates.push_back(0);

    fPortStates.clear(); // 12 ports
    for(size_t cIndex = 0; cIndex < 12; cIndex++) fPortStates.push_back(0);
}

CicInterface::~CicInterface() {}

bool CicInterface::ConfigureChip(Chip* pCic, bool pVerifLoop, uint32_t pBlockSize)
{
    LOG(INFO) << BOLDBLUE << "Configuring CIC" << +pCic->getId() << " on FE" << +pCic->getHybridId() << RESET;
    // first, identify the correct BeBoardFWInterface
    setBoard(pCic->getBeBoardId());

    // vector to encode all the registers into
    std::vector<uint32_t> cVec;

    // Deal with the RegItems and encode them
    ChipRegMap cCicRegMap = pCic->getRegMap();

    // encode the FW registers
    for(auto& cRegItem: cCicRegMap)
    {
        LOG(DEBUG) << BOLDBLUE << cRegItem.first << " , Value: 0x" << std::hex << +cRegItem.second.fValue << std::dec << RESET;
        fBoardFW->EncodeReg(cRegItem.second, pCic->getHybridId(), pCic->getId(), cVec, pVerifLoop, true);
#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }
    // now write the registers
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);

#ifdef COUNT_FLAG
    fTransactionCount++;
#endif
    LOG(INFO) << BOLDGREEN << "--- Done configuring one CIC " << RESET;
    return cSuccess;
}
bool CicInterface::WriteChipReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    std::vector<uint32_t> cVec;
    ChipRegItem           cRegItem = pChip->getRegItem(pRegNode);
    cRegItem.fValue                = pValue;
    LOG(DEBUG) << BOLDBLUE << pRegNode << " , Value: 0x" << std::hex << cRegItem.fValue << std::dec << RESET;
    fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId(), cVec, pVerifLoop, true);
    // now write the registers
    uint8_t cWriteAttempts = 0;
    bool    cSuccess       = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    return cSuccess;
}

bool     CicInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop) { return true; }
uint16_t CicInterface::ReadChipReg(Chip* pChip, const std::string& pRegNode)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem = pChip->getRegItem(pRegNode);

    std::vector<uint32_t> cVecReq;

    fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId(), cVecReq, true, false);
    fBoardFW->ReadChipBlockReg(cVecReq);

    // bools to find the values of failed and read
    bool    cFailed = false;
    bool    cRead;
    uint8_t cChipId;
    fBoardFW->DecodeReg(cRegItem, cChipId, cVecReq[0], cRead, cFailed);
    if(!cFailed) pChip->setReg(pRegNode, cRegItem.fValue);

    return cRegItem.fValue & 0xFF;
}
std::pair<bool, uint16_t> CicInterface::ReadChipReg(Chip* pChip, ChipRegItem pRegItem)
{
    setBoard(pChip->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    fBoardFW->EncodeReg(pRegItem, pChip->getHybridId(), pChip->getId(), cVecReq, true, false);
    fBoardFW->ReadChipBlockReg(cVecReq);
    // bools to find the values of failed and read
    bool    cFailed = false;
    bool    cRead;
    uint8_t cChipId;
    fBoardFW->DecodeReg(pRegItem, cChipId, cVecReq[0], cRead, cFailed);
    return std::make_pair(!cFailed, pRegItem.fValue);
}
bool CicInterface::CheckReSync(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAD : 0xA6;

    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC requires a ReSync." << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = cRegAddress;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
    LOG(DEBUG) << BOLDBLUE << "Read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    if(!cReadBack.first) return false;

    bool cResyncNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x8) >> 3);
    if(!cResyncNeeded)
    {
        LOG(INFO) << BOLDBLUE << "....... No ReSync needed" << RESET;
        return true;
    }

    LOG(INFO) << BOLDBLUE << "....... ReSync needed - sending one now ...... " << RESET;
    // if readback worked and the CIC says it needs a Resync then send
    // a resync
    fBoardFW->ChipReSync();
    // check if CIC still needs one
    cReadBack = this->ReadChipReg(pChip, cRegItem);
    LOG(DEBUG) << BOLDBLUE << "After ReSync... read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    cResyncNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x8) >> 3);
    if(!cReadBack.first || cResyncNeeded)
    {
        LOG(INFO) << BOLDRED << "..............FAILED" << BOLDBLUE << " cleared RESYNC request" << RESET;
        return false;
    }
    else
    {
        LOG(INFO) << BOLDGREEN << "..............SUCCESSFULLY" << BOLDBLUE << " cleared  RESYNC request" << RESET;
        return true;
    }
}
bool CicInterface::CheckFastCommandLock(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAE : 0xA6;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC fast command decoder locked" << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = cRegAddress;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
    if(!cReadBack.first) return false;
    LOG(DEBUG) << BOLDBLUE << "Read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    bool cLocked = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x10) >> 4);
    if(cLocked)
        return this->CheckReSync(pChip);
    else
        return cLocked;
}
// configure alignment patterns on CIC
bool CicInterface::ConfigureAlignmentPatterns(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns)
{
    bool cSuccess = true;
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    for(uint8_t cIndex = 0; cIndex < (uint8_t)pAlignmentPatterns.size(); cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "CALIB_PATTERN%d", cIndex);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, pAlignmentPatterns[cIndex]);
        if(cSuccess) { LOG(DEBUG) << BOLDBLUE << "Calibration pattern [for word alignment] on stub line " << +cIndex << " set to " << std::bitset<8>(pAlignmentPatterns[cIndex]) << RESET; }
    }
    return cSuccess;
}
// manually set Bx0 alignment
bool CicInterface::ManualBx0Alignment(Chip* pChip, uint8_t pBx0delay)
{
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_BX0_DELAY" : "BX0_ALIGN_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x7F) | (0x01 << 7));
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Manually settomg BX0 delay value in CIC on FE" << +pChip->getHybridId() << " to " << +pBx0delay << " clock cycles." << RESET;
    bool cSuccess = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;
    cSuccess = cSuccess && this->WriteChipReg(pChip, "EXT_BX0_DELAY", pBx0delay);
    return cSuccess;
}
// run automated Bx0 alignment
bool CicInterface::ConfigureBx0Alignment(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns, uint8_t pFEId, uint8_t pLineId)
{
    std::vector<uint8_t> cFeMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Running automated word alignment in CIC on FE" << +pChip->getHybridId() << RESET;
    LOG(DEBUG) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    bool cSuccess = ConfigureAlignmentPatterns(pChip, pAlignmentPatterns);

    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_BX0_DELAY" : "BX0_ALIGN_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x7F) | (0x00 << 7));
    cSuccess              = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "BX0_ALIGNMENT_FE" : "BX0_ALIGN_CONFIG";
    cRegValue = this->ReadChipReg(pChip, cRegName);
    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pFEId : ((cRegValue & 0xC7) | (cFeMapping[pFEId] << 3));
    cSuccess  = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "BX0_ALIGNMENT_LINE" : "BX0_ALIGN_CONFIG";
    cRegValue = this->ReadChipReg(pChip, cRegName);
    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pLineId : ((cRegValue & 0xF8) | (pLineId << 0));
    cSuccess  = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    fBoardFW->ChipReSync();
    return cSuccess;
}
bool CicInterface::AutoBx0Alignment(Chip* pChip, uint8_t pStatus)
{
    // make sure auto WA request is 0
    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_WA_REQUEST" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 0));
    bool        cSuccess   = this->WriteChipReg(pChip, cRegName, cToggleOff);
    if(!cSuccess) return cSuccess;

    cRegName        = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_BX0_ALIGNMENT_REQUEST" : "BX0_ALIGN_CONFIG";
    cRegValue       = this->ReadChipReg(pChip, cRegName);
    uint16_t cValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0xF8) | (pStatus << 6));
    cSuccess        = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    return cSuccess;
}
std::pair<bool, uint8_t> CicInterface::CheckBx0Alignment(Chip* pChip)
{
    uint8_t cDelay = 0;
    setBoard(pChip->getBeBoardId());

    bool cSuccess = this->AutoBx0Alignment(pChip, 0);
    if(!cSuccess) return std::make_pair(cSuccess, cDelay);
    // remember to send a resync
    fBoardFW->ChipReSync();

    // check status
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x84 : 0xA6;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
    if(!cReadBack.first)
    {
        LOG(ERROR) << BOLDRED << "Read-back failed!" << RESET;
        return std::make_pair(false, cDelay);
    }
    uint16_t cReadBackValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second) : ((cReadBack.second & 0x02) >> 1);
    cSuccess                = (cReadBackValue == 1);

    // read back delay
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAF : 0xA7;
    cReadBack         = this->ReadChipReg(pChip, cRegItem);
    if(cReadBack.first)
        cDelay = cReadBack.second;
    else
        cSuccess = false;
    return std::make_pair(cSuccess, cDelay);
}
// run automated word alignment
// assumes FEs have been configured to output alignment pattern
bool CicInterface::AutomatedWordAlignment(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns, int pWait_ms)
{
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Running automated word alignment in CIC on FE" << +pChip->getHybridId() << RESET;
    LOG(INFO) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    bool cSuccess = ConfigureAlignmentPatterns(pChip, pAlignmentPatterns);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot configure patterns on CIC.." << RESET;
        exit(0);
    }

    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_WA_DELAY" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 1));
    uint16_t    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 1));

    cSuccess = this->WriteChipReg(pChip, cRegName, cToggleOn);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot disable external word alignment value on CIC.." << RESET;
        exit(0);
    }

    cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_WA_REQUEST" : "MISC_CTRL";
    cRegValue  = this->ReadChipReg(pChip, cRegName);
    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 0));
    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 0));
    cSuccess   = this->WriteChipReg(pChip, cRegName, cToggleOn);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot send external word alignment request to CIC.." << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDBLUE << "Running automated word alignment .... " << RESET;
    // check if word alingment is done
    bool    cDone          = false;
    uint8_t cMaxIterations = (pWait_ms / 100);
    uint8_t cIteration     = 0;
    bool    cStop          = false;
    do
    {
        // check status
        ChipRegItem cRegItem;
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x83 : 0xA6;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
        if(!cReadBack.first)
        {
            LOG(INFO) << BOLDBLUE << "Readback failed.." << RESET;
            cDone = false;
            exit(0);
        }
        cDone = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x01) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(cIteration % 10 == 0) LOG(INFO) << BOLDBLUE << "\t....Iteration " << +cIteration << " ... : " << cDone << RESET;
        // stop either if done or if the maximum number of iterations
        // has been exceeded
        cStop = cDone || (cIteration > cMaxIterations);
        cIteration += 1;
    } while(!cStop);
    if(!cDone) { return cDone; }
    LOG(INFO) << BOLDBLUE << "Requesting CIC to stop automated word alignment..." << RESET;
    cSuccess = this->WriteChipReg(pChip, cRegName, cToggleOff);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot disable automated Word alignment request.." << RESET;
        exit(0);
    }

    return cSuccess;
}
bool CicInterface::ResetDLL(Chip* pChip, uint16_t pWait_ms)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Resetting DLL in CIC" << RESET;
    // apply a channel reset
    LOG(INFO) << BOLDBLUE << "\t.... Enabling RESET on DLL" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "scDllResetReq%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0xFF);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC DLL reset" << RESET;
            exit(0);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    // release channel reset
    LOG(INFO) << BOLDBLUE << "\t... Disabling RESET on DLL" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "scDllResetReq%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC DLL reset" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
// check DLL lock in CIC
bool CicInterface::CheckDLL(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x5A : 0x9A;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking DLL lock in CIC" << RESET;
    ChipRegItem           cRegItem;
    std::vector<uint16_t> cValues(2);
    for(int cIndex = 0; cIndex < (int)cValues.size(); cIndex += 1)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cRegAddress + cIndex;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
        if(cReadBack.first) cValues[cIndex] = cReadBack.second & 0xFF;
        LOG(DEBUG) << BOLDBLUE << "Lock" << cIndex << " -- " << cValues[cIndex] << RESET;
    }
    uint16_t cLock   = (cValues[1] << 8) | cValues[0];
    bool     cLocked = (cLock == 0xFFF);
    return cLocked;
}
bool CicInterface::SetAutomaticPhaseAlignment(Chip* pChip, bool pAuto)
{
    setBoard(pChip->getBeBoardId());
    if(pAuto)
        LOG(INFO) << BOLDBLUE << "Configuring CIC to use automatic phase aligner..." << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Configuring CIC to use static phase aligner..." << RESET;
    // set phase aligner in static mode
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "scTrackMode" : "PHY_PORT_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pAuto : ((cRegValue & 0x07) | (pAuto << 3));
    bool        cSuccess  = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess)
    {
        LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
        exit(0);
    }
    return cSuccess;
}
bool CicInterface::PhaseAlignerPorts(Chip* pChip, uint8_t pState)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    if(pState == 1)
        LOG(INFO) << BOLDGREEN << "Enabling " << BOLDBLUE << " all CIC phase aligner input..." << RESET;
    else
        LOG(INFO) << BOLDRED << "Disabling " << BOLDBLUE << " all CIC phase aligner input..." << RESET;
    for(uint8_t cIndex = 0; cIndex < 6; cIndex += 1)
    {
        char cBuffer[13];
        sprintf(cBuffer, "scEnableLine%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, (pState == 1) ? 0xFF : 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error conifguring CIC" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
bool CicInterface::ResetPhaseAligner(Chip* pChip, uint16_t pWait_ms)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Resetting CIC phase aligner..." << RESET;
    // apply a channel reset
    LOG(INFO) << BOLDBLUE << "\t.... Enabling RESET on all phase aligner inputs" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[16];
        sprintf(cBuffer, "scResetChannels%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0xFF);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC phase aligner reset" << RESET;
            exit(0);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    this->CheckPhaseAlignerLock(pChip, 0x00);
    // release channel reset
    LOG(INFO) << BOLDBLUE << "\t... Disabling RESET on all phase aligner inputs" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[16];
        sprintf(cBuffer, "scResetChannels%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC phase aligner reset" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
bool CicInterface::SetStaticPhaseAlignment(Chip* pChip, uint8_t pReadoutChipId, uint8_t pLineId, uint8_t pPhase)
{
    std::vector<uint8_t> cFeMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC
    bool                 cL1Selected = (pLineId == 0);
    uint8_t              cPort       = 0;
    uint8_t              cInput      = 0;
    if(cL1Selected)
    {
        uint8_t cChipId_forCic = std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), pReadoutChipId));
        LOG(DEBUG) << BOLDBLUE << "Modifying phase alignment value for readout chip " << +pReadoutChipId << " [ in CIC mapping this is FE" << +cChipId_forCic << " ]" << RESET;
        cInput = cChipId_forCic & 0x3;
        cPort  = 10 + ((cChipId_forCic & 0x4) >> 2);
        LOG(DEBUG) << BOLDBLUE << "Will modifiy phase aligner value for phy port " << +cPort << " input " << +cInput << RESET;
    }
    // I assume this has already been done
    // bool cSuccess = SetAutomaticPhaseAlignment(pChip, false);
    bool cSuccess = true;
    setBoard(pChip->getBeBoardId());
    if(cSuccess)
    {
        ChipRegItem cRegItem;
        uint8_t     cIndex = std::floor(cPort / 2.);
        char        cBuffer[17];
        sprintf(cBuffer, "scPhaseSelectB%di%d", cInput, cIndex);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
        uint8_t     cMask     = ~(0xF << ((cPort & 0x1))) & 0xFF;
        uint16_t    cValue    = (cRegValue & cMask) | pPhase;
        LOG(DEBUG) << BOLDBLUE << "Will modifiy register " << cBuffer << " which was set to " << std::bitset<8>(cRegValue) << " -- new value is " << std::bitset<8>(cValue) << RESET;
        cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    }
    if(!cSuccess)
    {
        LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
        exit(0);
    }
    return cSuccess;
}
bool CicInterface::SetStaticPhaseAlignment(Chip* pChip, std::vector<std::vector<uint8_t>> pPhaseTaps)
{
    bool cSuccess = SetAutomaticPhaseAlignment(pChip, false);
    setBoard(pChip->getBeBoardId());
    if(cSuccess)
    {
        ChipRegItem cRegItem;
        for(uint8_t cInput = 0; cInput < 4; cInput += 1)
        {
            uint8_t cPhyPort = 0;
            for(uint8_t cIndex = 0; cIndex < 6; cIndex += 1)
            {
                char cBuffer[17];
                sprintf(cBuffer, "scPhaseSelectB%di%d", cInput, cIndex);
                uint8_t     cValue = (pPhaseTaps[cInput][cPhyPort + 1] << 4) | (pPhaseTaps[cInput][cPhyPort]);
                std::string cRegName(cBuffer, sizeof(cBuffer));
                LOG(DEBUG) << BOLDBLUE << "Input" << +cInput << " : " << +pPhaseTaps[cInput][cPhyPort + 1] << " " << +pPhaseTaps[cInput][cPhyPort] << " " << cRegName << "  ---> "
                           << std::bitset<8>(cValue) << RESET;
                cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
                cPhyPort += 2;
            }
        }
    }
    if(!cSuccess)
    {
        LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
        exit(0);
    }
    return cSuccess;
}
bool CicInterface::SetStaticWordAlignment(Chip* pChip, uint8_t pValue)
{
    LOG(INFO) << BOLDBLUE << "Setting word alignment value of CIC on FE" << +pChip->getHybridId() << " to " << +pValue << RESET;

    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_WA_DELAY" : "MISC_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOn = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 1));

    bool cSuccess = this->WriteChipReg(pChip, cRegName, cToggleOn);
    // check status
    for(uint8_t cIndex = 0; cIndex < 20; cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "EXT_WA_DELAY%.2d", cIndex);
        uint8_t     cValue = (pValue << 4) | (pValue);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        LOG(DEBUG) << BOLDBLUE << "Setting static word alignment to " << +pValue << RESET;
        cSuccess = this->WriteChipReg(pChip, cRegName, cValue);
    }
    return cSuccess;
}
std::vector<std::vector<uint8_t>> CicInterface::ReadWordAlignmentValues(Chip* pChip)
{
    setBoard(pChip->getBeBoardId());
    // 5 lines per FE ... 8 FEs per CIC
    std::vector<std::vector<uint8_t>> cWordAlignmentValues(8, std::vector<uint8_t>(5, 0));
    uint8_t                           cLineCounter = 0;
    uint8_t                           cFECounter   = 0;
    ChipRegItem                       cRegItem;
    bool                              cSuccess     = true;
    uint16_t                          cBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x85 : 0xA8;
    for(uint8_t cIndex = 0; cIndex < 20; cIndex += 1)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cBaseAddress + cIndex;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
        cSuccess                            = cSuccess && cReadBack.first;
        if(cSuccess)
        {
            LOG(DEBUG) << BOLDBLUE << "Word alignment value found to be " << std::bitset<8>(cReadBack.second) << RESET;
            for(uint8_t cNibble = 0; cNibble < 2; cNibble += 1)
            {
                uint8_t cWordAlignment                         = (cReadBack.second & (0xF << cNibble * 4)) >> 4 * cNibble;
                cWordAlignmentValues[cFECounter][cLineCounter] = cWordAlignment;
                LOG(DEBUG) << BOLDBLUE << "Word alignment for FE" << +cFECounter << " Line" << +cLineCounter << " value found to be " << +cWordAlignment << RESET;
                cLineCounter += 1;
                if(cLineCounter > 4)
                {
                    cLineCounter = 0;
                    cFECounter += 1;
                }
            }
        }
    }
    return cWordAlignmentValues;
}
bool CicInterface::ReadOptimalTap(Chip* pChip, uint8_t pPhyPortChannel, std::vector<std::vector<uint8_t>>& pPhaseTaps)
{
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Reading optimal tap found by Auto phase aligner lock in CIC for channel number " << +pPhyPortChannel << RESET;
    ChipRegItem cRegItem;
    bool        cSuccess     = true;
    uint16_t    cBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x40 : 0x80;
    for(int cIndex = 0; cIndex < 6; cIndex++)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cBaseAddress + pPhyPortChannel * 6 + cIndex;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
        if(cReadBack.first)
        {
            cSuccess = cSuccess && cReadBack.first;
            LOG(DEBUG) << BOLDBLUE << "Phy-port channel " << +pPhyPortChannel << " : [ " << cIndex << " ]: " << std::bitset<8>(cReadBack.second) << RESET;
            for(uint8_t cChannelIndex = 0; cChannelIndex < 2; cChannelIndex += 1)
            {
                uint8_t cInput = cChannelIndex + cIndex * 2;
                uint8_t cPhase = (cReadBack.second & (0xF << (cChannelIndex * 4))) >> (cChannelIndex * 4);
                LOG(DEBUG) << BOLDBLUE << "Input " << +cInput << " -- phase found to be " << +(cPhase) << RESET;
                pPhaseTaps[pPhyPortChannel][cInput] = cPhase;
            }
        }
    }
    return cSuccess;
}
std::vector<std::vector<uint8_t>> CicInterface::GetOptimalTaps(Chip* pChip)
{
    // 4 channels per phyPort ... 12 phyPorts per CIC
    std::vector<std::vector<uint8_t>> cPhaseTaps(4, std::vector<uint8_t>(12, 0));
    for(uint8_t cPhyPortChannel = 0; cPhyPortChannel < 4; cPhyPortChannel += 1) { this->ReadOptimalTap(pChip, cPhyPortChannel, cPhaseTaps); }
    return cPhaseTaps;
}
bool CicInterface::CheckPhaseAlignerLock(Chip* pChip, uint8_t pCheckValue)
{
    uint16_t cRegBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x5C : 0xA0;
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Checking Auto phase aligner lock in CIC." << RESET;
    ChipRegItem cRegItem;
    bool        cLocked = true;

    // std::vector<std::bitset<4>> cPortStates(12);
    size_t cPortCounter = 0;
    // read back phase alignment on stub lines
    for(int cIndex = 0; cIndex < 6; cIndex++)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cRegBaseAddress + cIndex;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
        if(cReadBack.first) cLocked = cLocked & (cReadBack.second == pCheckValue);
        LOG(DEBUG) << BOLDBLUE << "Lock on input " << cIndex << " -- " << std::bitset<8>(cReadBack.second) << RESET;
        for(size_t cBitIndex = 0; cBitIndex < 8; cBitIndex++)
        {
            fPortStates[cPortCounter][cBitIndex & 0x3] = std::bitset<8>(cReadBack.second)[cBitIndex];
            cPortCounter += (cBitIndex == 3) || (cBitIndex == 7);
        }
    }

    size_t cInputLineCounter = 0;
    // std::vector<std::bitset<6>> cFeStates(8,0);
    cPortCounter = 0;
    for(size_t cFeCounter = 0; cFeCounter < 8; cFeCounter++)
    {
        for(size_t cStubLineCounter = 0; cStubLineCounter < 5; cStubLineCounter++)
        {
            fFeStates[cFeCounter][cStubLineCounter] = fPortStates[cPortCounter][cInputLineCounter];
            cInputLineCounter++;
            if(cInputLineCounter > 3)
            {
                cPortCounter++;
                cInputLineCounter = 0;
            }
        }
        // L1 line
        fFeStates[cFeCounter][5] = fPortStates[(10 + (cFeCounter > 4))][(cFeCounter & 0x3)];
        LOG(INFO) << BOLDBLUE << "PhyPort lock FE" << +cFeCounter << " : " << BOLDYELLOW << +fFeStates[cFeCounter][5] << " [L1 lines] " << BOLDMAGENTA
                  << std::bitset<5>(fFeStates[cFeCounter].to_ulong() & 0x1F) << " [Stub lines 0 -- 4]" << RESET;
    }
    if(cLocked)
        LOG(INFO) << BOLDGREEN << "SUCCESSFULL " << BOLDBLUE << " lock on all phase aligner lines.." << RESET;
    else
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to lock on all phase aligner lines.." << RESET;
    return cLocked;
}
bool CicInterface::SoftReset(Chip* pChip, uint32_t cWait_ms)
{
    setBoard(pChip->getBeBoardId());

    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "SOFT_RESET" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : (cRegValue & 0x0F) | (0x1 << 4);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : (cRegValue & 0x0F) | (0x0 << 4);

    LOG(INFO) << BOLDBLUE << "Setting register " << cRegName << " to " << std::bitset<5>(cToggleOn) << " to toggle ON soft reset." << RESET;
    if(!this->WriteChipReg(pChip, cRegName, cToggleOn)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(cWait_ms));

    LOG(INFO) << BOLDBLUE << "Setting register " << cRegName << " to " << std::bitset<5>(cToggleOff) << " to toggle OFF soft reset." << RESET;
    if(!this->WriteChipReg(pChip, cRegName, cToggleOff)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(cWait_ms));
    return true;
}
bool CicInterface::SelectOutput(Chip* pChip, bool pFixedPattern)
{
    setBoard(pChip->getBeBoardId());
    if(pFixedPattern)
        LOG(INFO) << BOLDBLUE << "Want to configure CIC to output fixed pattern on all lines... " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Want to configure CIC to output patterns from readout chips... " << RESET;

    // enable output pattern from CIC
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "OUTPUT_PATTERN_ENABLE" : "MISC_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? static_cast<uint8_t>(pFixedPattern) : ((cRegValue & 0x1B) | (static_cast<uint8_t>(pFixedPattern) << 2));

    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    cRegValue = this->ReadChipReg(pChip, cRegName);
    LOG(INFO) << BOLDBLUE << "CIC output pattern configured by setting " << cRegName << " to " << std::bitset<8>(cRegValue) << RESET;
    return true;
}
bool CicInterface::SetSparsification(Chip* pChip, uint8_t pEnable)
{
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBC_SPARSIFICATION_SEL" : "FE_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pEnable : (cRegValue & 0x2F) | (pEnable << 4);
    return this->WriteChipReg(pChip, cRegName, cValue);
}
bool CicInterface::EnableFEs(Chip* pChip, std::vector<uint8_t> pFEs, bool pEnable)
{
    setBoard(pChip->getBeBoardId());
    std::string cRegName = "FE_ENABLE";
    uint16_t    cValue   = this->ReadChipReg(pChip, cRegName);
    for(auto pFe: pFEs)
    {
        uint8_t cMask = ~(0x1 << pFe) & 0xFF;
        cValue        = (cValue & cMask) | (static_cast<uint8_t>(pEnable) << pFe);
    }
    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    LOG(INFO) << BOLDBLUE << "Setting FE enable register [" << cRegName << "] to " << std::bitset<8>(cValue) << RESET;
    return true;
}
bool CicInterface::SelectMode(Chip* pChip, uint8_t pMode)
{
    setBoard(pChip->getBeBoardId());

    LOG(INFO) << BOLDBLUE << "Want to confiure CIC mode : " << +pMode << RESET;
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pMode : (cRegValue & 0x3E) | pMode;
    if(pMode == 0) // for CBC mode .. always320 MHz and without last line
        cValue = (cValue & 0x35) | (pMode << 1) | (pMode << 3);

    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    cRegValue = this->ReadChipReg(pChip, cRegName);
    LOG(INFO) << BOLDBLUE << "Register " << cRegName << " set to " << std::bitset<6>(cRegValue) << " to select CIC Mode " << +pMode << RESET;
    return true;
}
// bool CicInterface::PhaseAlignerStatus(Chip* pChip, std::vector<std::vector<uint8_t>>& pPhaseTaps )
// {
//     bool cLocked=this->CheckPhaseAlignerLock(pChip);
//     // 4 channels per phyPort ... 12 phyPorts per CIC
//     // 8 FEs per CIC .... 6 SLVS lines per FE
//     // read back phase aligner values
//     pPhaseTaps = fCicInterface->GetOptimalTaps( pChip);
//     return cLocked;
// }
bool CicInterface::CheckSoftReset(Chip* pChip)
{
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC requires a Soft reset." << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAC : 0xA6;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipReg(pChip, cRegItem);
    if(!cReadBack.first)
    {
        LOG(ERROR) << BOLDRED << "Read back failed!" << RESET;
        exit(0);
    }
    bool cSoftResetNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : (((cReadBack.second & 0x04) >> 2) == 1);
    // if readback worked and the CIC says it needs a Resync then send
    // a resync
    if(cSoftResetNeeded)
    {
        LOG(INFO) << BOLDBLUE << "....... Reset  needed - sending one now ...... " << RESET;
        SoftReset(pChip);
    }
    // check if CIC still needs one
    cReadBack        = this->ReadChipReg(pChip, cRegItem);
    cSoftResetNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : (((cReadBack.second & 0x04) >> 2) == 1);
    if(!cReadBack.first || cSoftResetNeeded)
        return false;
    else
        return true;
}
bool CicInterface::SelectMux(Chip* pChip, uint8_t pPhyPort)
{
    // first .. enable bypass of logic
    if(!this->ControlMux(pChip, 1)) return false;

    // then select phy port
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Selecting phyPort [0-11]: " << +pPhyPort << RESET;
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "ctrlTestMux" : "MUX_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pPhyPort : (cRegValue & 0x8) | pPhyPort;
    return this->WriteChipReg(pChip, cRegName, cValue);
}
bool CicInterface::ControlMux(Chip* pChip, uint8_t pEnable)
{
    setBoard(pChip->getBeBoardId());
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "enableMux" : "MUX_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pEnable : (cRegValue & 0x7) | (pEnable << 3);
    if(pEnable == 1)
        LOG(INFO) << BOLDBLUE << " Enabling CIC MUX .. so bypassing CIC logic " << RESET;
    else
        LOG(INFO) << BOLDBLUE << " Disabling CIC MUX .. so activating CIC logic  " << RESET;

    return this->WriteChipReg(pChip, cRegName, cValue);
}
// start-up sequence for CIC [everything that does not require interaction
// with the BE or the other readout ASICs on the chip
bool CicInterface::StartUp(Chip* pChip, uint8_t pDriveStrength)
{
    std::string cOut = ".... Starting CIC start-up ........ on hybrid " + std::to_string(pChip->getHybridId());
    if(pChip->getFrontEndType() == FrontEndType::CIC)
        cOut += " for CIC1.";
    else
        cOut += " for CIC2.";
    LOG(INFO) << BOLDBLUE << cOut << RESET;

    bool cSuccess = this->CheckSoftReset(pChip);
    // if( !cSuccess )
    // {
    //     LOG (INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " clear SOFT reset request in CIC...
    //     " << RESET; exit(0);
    // }
    // bool cSuccess = this->SoftReset(pChip);

    bool cClkTermination = true;
    bool cRxTermination  = true;
    //(pChip->getFrontEndType() == FrontEndType::CIC ) ? true : false ;// true, false -- this needs to be false for the
    // crate set-up .. how to fix this?!?!
    std::string cRegName  = "SLVS_PADS_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    auto        cIterator = fTxDriveStrength.find(pDriveStrength);
    if(cIterator != fTxDriveStrength.end())
    {
        auto cValue = (cRxTermination << 4) | (cClkTermination << 3) | cIterator->second;
        cSuccess    = this->WriteChipReg(pChip, "SLVS_PADS_CONFIG", cValue);
        LOG(INFO) << BOLDBLUE << "Configuring drive strength on CIC output pads: 0x" << std::hex << +cValue << std::dec << RESET;
        if(!cSuccess)
        {
            LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " configure drive strength on CIC output pads." << RESET;
            exit(0);
        }
        cRegValue = this->ReadChipReg(pChip, cRegName);
        LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " configured drive strength on CIC output pads: 0x" << std::hex << +cRegValue << std::dec << RESET;
    }

    // reset DLL for each of the 12 phy ports
    cSuccess = this->ResetDLL(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " Reset DLL in CIC " << RESET;
        exit(0);
    }
    // checking DLL lock
    cSuccess = this->CheckDLL(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " LOCK DLL in CIC  " << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDBLUE << "DLL in CIC " << BOLDGREEN << " LOCKED." << RESET;

    cSuccess = this->SetAutomaticPhaseAlignment(pChip, true);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " set automatic phase aligner in CIC... " << RESET;
        exit(0);
    }
    this->EnableFEs(pChip, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    this->ResetPhaseAligner(pChip, 200);
    this->EnableFEs(pChip, {0, 1, 2, 3, 4, 5, 6, 7}, true);

    // select fast command edge
    bool cNegEdge = true;
    if(cNegEdge)
        LOG(INFO) << BOLDBLUE << "Configuring fast command block in CIC to lock on falling edge." << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Configuring fast command block in CIC to lock on rising edge." << RESET;
    cRegName        = (pChip->getFrontEndType() == FrontEndType::CIC) ? "FC_ON_NEG_EDGE" : "MISC_CTRL";
    cRegValue       = this->ReadChipReg(pChip, cRegName);
    uint16_t cValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? cNegEdge : (cRegValue & 0x17) | (cNegEdge << 3);
    cSuccess        = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " select FC edge in CIC  " << RESET;
        exit(0);
    }

    // check fast command lock
    cSuccess = this->CheckFastCommandLock(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " lock FC decoder in CIC  " << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " configured fast command block in CIC." << RESET;

    cSuccess = this->CheckReSync(pChip);
    LOG(INFO) << BOLDGREEN << ".... Completed CIC start-up ........ " << RESET;
    return cSuccess;
}
} // namespace Ph2_HwInterface
