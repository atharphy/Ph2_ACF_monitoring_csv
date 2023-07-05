/*!
  \file                  RD53lpGBTInterface.cc
  \brief                 Interface to access and control the Low-power Gigabit Transceiver chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  03/03/20
  Support:               email to mauro.dinardo@cern.ch
*/

#include "HWInterface/RD53lpGBTInterface.h"
#include "HWInterface/RD53Interface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// ##################################
// # Read and Write LpGBT registers #
// ##################################

bool RD53lpGBTInterface::WriteChipReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerify)
{
    bool writeGood = RD53lpGBTInterface::WriteReg(pChip, pChip->getRegItem(pRegNode).fAddress, pValue, pVerify);
    pChip->setReg(pRegNode, pValue);
    return writeGood;
}

bool RD53lpGBTInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pRegVec, bool pVerify)
{
    bool writeGood = true;
    for(const auto& cReg: pRegVec) writeGood &= RD53lpGBTInterface::WriteChipReg(pChip, cReg.first, cReg.second);
    return writeGood;
}

uint16_t RD53lpGBTInterface::ReadChipReg(Chip* pChip, const std::string& pRegNode) { return RD53lpGBTInterface::ReadReg(pChip, pChip->getRegItem(pRegNode).fAddress); }

bool RD53lpGBTInterface::WriteReg(Chip* pChip, uint16_t pAddress, uint16_t pValue, bool pVerify)
{
    const uint16_t maxRegValue      = 0xFF;                                                            // @CONST@
    const uint16_t cMaxWriteAddress = (static_cast<lpGBT*>(pChip)->getVersion() == 0) ? 0x13C : 0x14F; // Setting highest write address possible (lpGBT version dependent)

    this->setBoard(pChip->getBeBoardId());

    if(pValue > maxRegValue)
    {
        LOG(ERROR) << BOLDRED << "LpGBT registers are 8 bits, impossible to write " << BOLDYELLOW << pValue << BOLDRED << " to address " << BOLDYELLOW << pAddress << RESET;
        return false;
    }

    if(pAddress >= cMaxWriteAddress)
    {
        LOG(ERROR) << "LpGBT read-write registers end at " << cMaxWriteAddress << " ... impossible to write to address " << BOLDYELLOW << pAddress << RESET;
        return false;
    }

    int  nAttempts = 0;
    bool status;
    do
    {
        status = fBoardFW->WriteOptoLinkRegister(pChip, pAddress, pValue, pVerify);
        nAttempts++;
    } while((pVerify == true) && (status == false) && (nAttempts < RD53Shared::MAXATTEMPTS));

    if((pVerify == true) && (status == false)) throw Exception("[RD53lpGBTInterface::WriteReg] LpGBT register writing issue");

    return true;
}

uint16_t RD53lpGBTInterface::ReadReg(Chip* pChip, uint16_t pAddress)
{
    this->setBoard(pChip->getBeBoardId());
    return fBoardFW->ReadOptoLinkRegister(pChip, pAddress);
}

// ######################
// # Main configurarion #
// ######################

bool RD53lpGBTInterface::ConfigureChip(Chip* pChip, bool pVerify, uint32_t pBlockSize)
{
    this->setBoard(pChip->getBeBoardId());

    // #####################
    // # Make reverted map #
    // #####################
    uint8_t cChipVersion = static_cast<lpGBT*>(pChip)->getVersion();
    for(auto& ele: fPUSMStatusMap[cChipVersion]) revertedPUSMStatusMap[ele.second] = ele.first;
    fBoardFW->SetOptoLinkVersion(cChipVersion);
    LOG(INFO) << GREEN << "LpGBT version: " << BOLDYELLOW << (cChipVersion == 0 ? "LpGBT-v0" : "LpGBT-v1") << RESET;

    // #########################
    // # Configure PLL and DLL #
    // #########################
    this->WriteChipReg(pChip, "LDConfigH", 1 << 5, false);
    this->WriteChipReg(pChip, "EPRXLOCKFILTER", 0x55, false);
    this->WriteChipReg(pChip, "EPRXDllConfig", 1 << 6 | 1 << 4 | 1 << 2, false);
    this->WriteChipReg(pChip, "PSDllConfig", 5 << 4 | 1 << 2 | 1, false);
    this->WriteChipReg(pChip, "POWERUP2", 1 << 2 | 1 << 1, false);

    // #####################
    // # Check PUSM status #
    // #####################
    uint8_t      PUSMStatus = this->GetPUSMStatus(pChip);
    unsigned int nAttempts  = 0;
    while((PUSMStatus != revertedPUSMStatusMap["READY"]) && (nAttempts < RD53Shared::MAXATTEMPTS))
    {
        PUSMStatus = this->GetPUSMStatus(pChip);
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
        nAttempts++;
    }

    if(PUSMStatus != revertedPUSMStatusMap["READY"])
    {
        LOG(ERROR) << BOLDRED << "LpGBT PUSM status: " << BOLDYELLOW << fPUSMStatusMap[cChipVersion][PUSMStatus] << RESET;
        return false;
    }
    LOG(INFO) << GREEN << "LpGBT PUSM status: " << BOLDYELLOW << fPUSMStatusMap[cChipVersion][PUSMStatus] << RESET;

    // ######################
    // # Configure Up links #
    // ######################
    this->ConfigureRxGroups(
        pChip, static_cast<lpGBT*>(pChip)->getRxGroups(), static_cast<lpGBT*>(pChip)->getRxChannels(), f10GRxDataRateMap[static_cast<lpGBT*>(pChip)->getRxDataRate()], lpGBTconstants::rxPhaseTracking);
    this->ConfigureRxChannels(pChip, static_cast<lpGBT*>(pChip)->getRxGroups(), static_cast<lpGBT*>(pChip)->getRxChannels(), 1, 1, 1, static_cast<lpGBT*>(pChip)->getRxHSLPolarity(), 12);

    // ########################
    // # Configure Down links #
    // ########################
    this->ConfigureTxGroups(pChip, static_cast<lpGBT*>(pChip)->getTxGroups(), static_cast<lpGBT*>(pChip)->getTxChannels(), fTxDataRateMap[static_cast<lpGBT*>(pChip)->getTxDataRate()]);
    this->ConfigureTxChannels(pChip, static_cast<lpGBT*>(pChip)->getTxGroups(), static_cast<lpGBT*>(pChip)->getTxChannels(), 3, 3, 0, 0, static_cast<lpGBT*>(pChip)->getTxHSLPolarity());

    // ####################################################
    // # Programming registers as from configuration file #
    // ####################################################
    LOG(INFO) << GREEN << "Initializing registers of LpGBT: " << BOLDYELLOW << pChip->getId() << RESET;
    const auto& lpGBTRegMap = pChip->getRegMap();
    for(const auto& cRegItem: lpGBTRegMap)
        if(cRegItem.second.fPrmptCfg == true)
        {
            LOG(INFO) << BOLDBLUE << "\t--> " << BOLDYELLOW << cRegItem.first << BOLDBLUE << " = " << BOLDYELLOW << cRegItem.second.fValue << RESET;

            if(cRegItem.first.find("_phase"))
            {
                lpGBTInterface::ConfigureRxPhase(
                    pChip, {static_cast<uint8_t>(std::stoi(cRegItem.first.substr(4, 1)))}, {static_cast<uint8_t>(std::stoi(cRegItem.first.substr(5, 1)))}, cRegItem.second.fValue);
                static_cast<lpGBT*>(pChip)->setPhaseRxAligned(true); // @TMP@
            }
            else
                RD53lpGBTInterface::WriteReg(pChip, cRegItem.second.fAddress, cRegItem.second.fValue);
        }
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    this->PrintChipMode(pChip);

    // #######################
    // # Checking DLL status #
    // #######################
    LOG(INFO) << GREEN << "Checking DLL status of LpGBT: " << BOLDYELLOW << pChip->getId() << RESET;
    for(const auto& cGroup: static_cast<lpGBT*>(pChip)->getRxGroups())
        LOG(INFO) << BOLDBLUE << "\t--> DLL status of Rx Group " << BOLDYELLOW << +cGroup << BOLDBLUE << " is 0x" << BOLDYELLOW << std::hex << +lpGBTInterface::GetRxDllStatus(pChip, cGroup)
                  << std::dec << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    return true;
}

// ###################################
// # RD53 specific routine functions #
// ###################################

void RD53lpGBTInterface::SetDownLinkMapping(const OpticalGroup* pOpticalGroup)
{
    this->setBoard(pOpticalGroup->getBeBoardId());

    for(const auto cHybrid: *pOpticalGroup)
        for(const auto cChip: *cHybrid)
        {
            auto pChip = static_cast<RD53*>(cChip);
            auto fwGr  = mapLpGBTGrCh2fwGr[pChip->getTxGroup() * 10 + pChip->getTxChannel()];
            static_cast<RD53FWInterface*>(fBoardFW)->SetDownLinkMapping(pOpticalGroup->getOpticalGroupId(), fwGr, cHybrid->getId());
        }
}

void RD53lpGBTInterface::SetUpLinkMapping(const OpticalGroup* pOpticalGroup)
{
    this->setBoard(pOpticalGroup->getBeBoardId());

    for(const auto cHybrid: *pOpticalGroup)
        for(const auto cChip: *cHybrid)
        {
            auto pChip = static_cast<RD53*>(cChip);
            static_cast<RD53FWInterface*>(fBoardFW)->SetUpLinkMapping(pOpticalGroup->getOpticalGroupId(), pChip->getRxGroup(), cHybrid->getId(), pChip->getChipLane());
        }
}

void RD53lpGBTInterface::PhaseAlignRx(Chip* pChip, const BeBoard* pBoard, const OpticalGroup* pOpticalGroup, ReadoutChipInterface* pReadoutChipInterface)
{
    const uint8_t              cChipRate = this->GetChipRate(pChip);
    const std::vector<uint8_t> pGroups   = static_cast<lpGBT*>(pChip)->getRxGroups();
    const std::vector<uint8_t> pChannels = static_cast<lpGBT*>(pChip)->getRxChannels();

    // @TMP@
    if(static_cast<lpGBT*>(pChip)->getPhaseRxAligned() == true)
    {
        LOG(INFO) << BOLDBLUE << "\t--> The phase for this LpGBT chip was already aligned (maybe from configuration file)" << RESET;
        return;
    }

    // ##############################
    // # Configure Rx Phase Shifter #
    // ##############################
    uint16_t cDelay = 0x0;
    uint8_t  cFreq = (cChipRate == 5) ? 4 : 5, cEnFTune = 0, cDriveStr = 0; // 4 --> 320 MHz || 5 --> 640 MHz
    this->ConfigurePhShifter(pChip, {0, 1, 2, 3}, cFreq, cDriveStr, cEnFTune, cDelay);

    static_cast<RD53Interface*>(pReadoutChipInterface)->InitRD53Downlink(pBoard);
    for(const auto cHybrid: *pOpticalGroup)
        for(const auto cChip: *cHybrid) { static_cast<RD53Interface*>(pReadoutChipInterface)->StartPRBSpattern(cChip); }

    this->PhaseTrainRx(pChip, pGroups);

    for(const auto& cGroup: pGroups)
    {
        // ############################
        // # Wait until channels lock #
        // ############################
        LOG(INFO) << GREEN << "Phase aligning Rx Group: " << BOLDYELLOW << +cGroup << RESET;
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
        } while(lpGBTInterface::IsRxLocked(pChip, cGroup, pChannels) == false);
        LOG(INFO) << BOLDBLUE << "\t--> Group " << BOLDYELLOW << +cGroup << BOLDBLUE << " LOCKED" << RESET;

        // #################
        // # Set new phase #
        // #################
        for(const auto& cChannel: pChannels)
        {
            uint8_t cCurrPhase = this->GetRxPhase(pChip, cGroup, cChannel);
            LOG(INFO) << BOLDBLUE << "\t\t--> Channel " << BOLDYELLOW << +cChannel << BOLDBLUE << " has phase " << BOLDYELLOW << +cCurrPhase << RESET;
            this->ConfigureRxPhase(pChip, cGroup, cChannel, cCurrPhase);
        }
    }
    this->PhaseTrainRx(pChip, pGroups);

    for(const auto cHybrid: *pOpticalGroup)
        for(const auto cChip: *cHybrid) static_cast<RD53Interface*>(pReadoutChipInterface)->StopPRBSpattern(cChip);

    // #####################################
    // # Set back Rx groups to fixed phase #
    // #####################################
    this->ConfigureRxGroups(pChip, pGroups, pChannels, f10GRxDataRateMap[static_cast<lpGBT*>(pChip)->getRxDataRate()], lpGBTconstants::rxPhaseTracking);

    static_cast<lpGBT*>(pChip)->setPhaseRxAligned(true); // @TMP@
}

bool RD53lpGBTInterface::ExternalPhaseAlignRx(Chip*                 pChip,
                                              const BeBoard*        pBoard,
                                              const OpticalGroup*   pOpticalGroup,
                                              BeBoardFWInterface*   pBeBoardFWInterface,
                                              ReadoutChipInterface* pReadoutChipInterface)
{
    const double frames_or_time = 1; // @CONST@
    const bool   given_time     = true;
    bool         allGood        = true;
    auto         frontendSpeed  = static_cast<RD53FWInterface*>(pBeBoardFWInterface)->ReadoutSpeed();

    LOG(INFO) << GREEN << "Phase alignment ongoing for LpGBT chip: " << BOLDYELLOW << pChip->getId() << RESET;

    // @TMP@
    if(static_cast<lpGBT*>(pChip)->getPhaseRxAligned() == true)
    {
        LOG(INFO) << BOLDBLUE << "\t--> The phase for this LpGBT chip was already aligned (maybe from configuration file)" << RESET;
        return true;
    }

    for(const auto cHybrid: *pOpticalGroup)
        for(const auto cChip: *cHybrid)
        {
            uint8_t cGroup   = static_cast<RD53*>(cChip)->getRxGroup();
            uint8_t cChannel = static_cast<RD53*>(cChip)->getRxChannel();

            uint8_t bestPhase      = 0;
            uint8_t bestPhaseStart = 0;
            uint8_t bestPhaseEnd   = 0;
            uint8_t phaseGap       = 0;
            double  bestBERtest    = -1;

            for(uint8_t phase = 0; phase < 16; phase++)
            {
                LOG(INFO) << BOLDMAGENTA << ">>> Phase value = " << BOLDYELLOW << +phase << BOLDMAGENTA << " of (0-15) <<<" << RESET;
                this->ConfigureRxPhase(pChip, cGroup, cChannel, phase);

                static_cast<RD53Interface*>(pReadoutChipInterface)->InitRD53Downlink(pBoard);
                static_cast<RD53Interface*>(pReadoutChipInterface)->StartPRBSpattern(cChip);

                const double result = this->RunBERtest(pChip, cGroup, cChannel, given_time, frames_or_time, (uint8_t)frontendSpeed);

                // #########################################################
                // # Search for largest interval and set into middle point #
                // #########################################################
                if(bestBERtest == -1)
                {
                    bestPhaseStart = phase;
                    bestBERtest    = result;
                }
                else if(result < bestBERtest)
                {
                    bestPhaseStart = phase;
                    bestBERtest    = result;
                }
                else if(result == bestBERtest)
                    bestPhaseEnd = phase;
                else if((result > bestBERtest) && (bestPhaseEnd >= bestPhaseStart))
                    bestBERtest = result;

                if((bestPhaseEnd >= bestPhaseStart) && (bestPhaseEnd - bestPhaseStart > phaseGap))
                {
                    bestPhase = (bestPhaseStart + bestPhaseEnd) / 2;
                    phaseGap  = bestPhaseEnd - bestPhaseStart;
                }

                static_cast<RD53Interface*>(pReadoutChipInterface)->StopPRBSpattern(cChip);
            }

            if(bestBERtest == 0)
                LOG(INFO) << BOLDBLUE << "\t--> Rx Group " << BOLDYELLOW << +cGroup << BOLDBLUE << " Channel " << BOLDYELLOW << +cChannel << BOLDBLUE << " has phase " << BOLDYELLOW << +bestPhase
                          << RESET;
            else
            {
                LOG(INFO) << BOLDBLUE << "\t--> Rx Group " << BOLDYELLOW << +cGroup << BOLDBLUE << " Channel " << BOLDYELLOW << +cChannel << BOLDRED << " has no good phase" << RESET;
                allGood = false;
            }

            this->ConfigureRxPhase(pChip, cGroup, cChannel, bestPhase);
        }

    static_cast<lpGBT*>(pChip)->setPhaseRxAligned(allGood); // @TMP@

    return allGood;
}

} // namespace Ph2_HwInterface
