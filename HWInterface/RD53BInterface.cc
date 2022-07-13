/*!
  \file                  RD53BInterface.h
  \brief                 User interface to the RD53B readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#include "RD53BInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
void RD53BInterface::SendGlobalPulse(Chip* pChip, uint16_t route, uint16_t pulseDuration)
{
    std::vector<uint16_t> cmdStream;
    PackWriteCommand(pChip, "GlobalPulseConf", route, cmdStream);
    PackWriteCommand(pChip, "GlobalPulseWidth", pulseDuration, cmdStream);
    serialize(RD53BCmd::GlobalPulse{pChip->getId()}, cmdStream);
    static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmdStream, pChip->getHybridId());
}

void RD53BInterface::SendGlobalPulseBroadcast(const BeBoard* pBoard, uint16_t route, uint16_t pulseDuration)
{
    std::vector<uint16_t> cmdStream;
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, 61, route}, cmdStream);
    serialize(RD53BCmd::WrReg{RD53BConstants::BROADCAST_CHIPID, 62, pulseDuration}, cmdStream);
    serialize(RD53BCmd::GlobalPulse{RD53BConstants::BROADCAST_CHIPID}, cmdStream);
    SendChipCommands(pBoard, cmdStream, -1);
}

void RD53BInterface::InitRD53Downlink(const BeBoard* pBoard)
{
    this->setBoard(pBoard->getId());

    LOG(INFO) << GREEN << "Down-link phase initialization..." << RESET;

    WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG", 0xAC75);
    WriteBoardBroadcastChipReg(pBoard, "GCR_DEFAULT_CONFIG_B", 0x538A);
    WriteBoardBroadcastChipReg(pBoard, "CmdErrCnt", 0);
    WriteBoardBroadcastChipReg(pBoard, "CdrConf", static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed() == RD53FWconstants::ReadoutSpeed::x1280 ? 0 : 1);
    SendGlobalPulseBroadcast(pBoard, 7, 0xFF); // ResetChannelSynchronizer, ResetCommandDecoder, ResetGlobalConfiguration
    WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x7FFF);
    WriteBoardBroadcastChipReg(pBoard, "RingOscConfig", 0x7FFF);
    SendGlobalPulseBroadcast(pBoard, 1 << 8, 0xFF); // ResetEfuses

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53BInterface::InitRD53UplinkSpeed(ReadoutChip* pChip)
{
    this->setBoard(pChip->getBeBoardId());

    auto auroraSpeed = static_cast<RD53FWInterface*>(fBoardFW)->ReadoutSpeed();
    WriteChipReg(pChip, "CdrConf", (auroraSpeed == RD53FWconstants::ReadoutSpeed::x1280 ? RD53Constants::CDRCONFIG_1Gbit : RD53Constants::CDRCONFIG_640Mbit), false);
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{});

    LOG(INFO) << GREEN << "Up-link speed set to: " << BOLDYELLOW << (auroraSpeed == RD53FWconstants::ReadoutSpeed::x1280 ? "1.28 Gbit/s" : "640 Mbit/s") << RESET;
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
}

void RD53BInterface::InitRD53Uplinks(ReadoutChip* pChip, int nActiveLanes)
{
    LOG(INFO) << GREEN << "Configuring up-link lanes and monitoring..." << RESET;
    WriteChipReg(pChip, "SER_SEL_OUT", 0x0055);
    size_t hybridId = pChip->getHybridId();
    // @TMP@ : what is this?
    if(hybridId >= 2)
        WriteChipReg(pChip, "CML_CONFIG", 15);
    else
        WriteChipReg(pChip, "CML_CONFIG", 1);
    WriteChipReg(pChip, "AuroraConfig", bits::pack<4, 6, 2>(1, 25, 3));
    uint16_t val;
    // @TMP@ : what is this?
    if(hybridId >= 2)
        val = bits::pack<2, 2, 2, 2, 2, 2, 2, 2>(0, 1, 2, 3, 0, 1, 2, 3);
    else
        val = bits::pack<2, 2, 2, 2, 2, 2, 2, 2>(3, 2, 1, 0, 3, 2, 1, 0);
    WriteChipReg(pChip, "DataMergingMux", val);
    WriteChipReg(pChip, "ServiceDataConf", (1 << 8) | 50);
    WriteChipReg(pChip, "AURORA_CB_CONFIG0", 0x0FF1);
    WriteChipReg(pChip, "AURORA_CB_CONFIG1", 0x0000);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    SendGlobalPulse(pChip, 0b110000, 0xFF);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{pChip->getId()});
    RD53Interface::SendCommand(pChip, RD53BCmd::Clear{pChip->getId()});
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53BInterface::PackWriteCommand(Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg)
{
    RD53BCmd::serialize(RD53BCmd::WrReg{pChip->getId(), pChip->getRegItem(regName).fAddress, data}, chipCommandList);
    if(updateReg == true) pChip->setReg(regName, data);
}

uint32_t RD53BInterface::ReadChipFuseID(Ph2_HwDescription::Chip* pChip)
{
    uint16_t low  = RD53Interface::ReadChipReg(pChip, "EfusesReadData0");
    uint16_t high = RD53Interface::ReadChipReg(pChip, "EfusesReadData1");
    return low | (high << 16);
}

} // namespace Ph2_HwInterface
