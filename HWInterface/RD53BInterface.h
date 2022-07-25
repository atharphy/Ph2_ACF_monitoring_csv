/*!
  \file                  RD53BInterface.h
  \brief                 User interface to the RD53B readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53BInterface_H
#define RD53BInterface_H

#include "../HWDescription/RD53B.h"
#include "../HWDescription/RD53BCommands.h"
#include "RD53Interface.h"

namespace Ph2_HwInterface
{
class RD53BInterface : public RD53Interface
{
  public:
    using RD53Interface::RD53Interface;

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    void     Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType) override;
    void     ChipErrorReport(Ph2_HwDescription::ReadoutChip* pChip) override;
    void     InitRD53Downlink(const Ph2_HwDescription::BeBoard* pBoard) override;
    void     InitRD53Uplinks(Ph2_HwDescription::ReadoutChip* pChip, int nActiveLanes = 1) override;
    void     PackWriteCommand(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = false) override;
    uint32_t ReadChipFuseID(Ph2_HwDescription::Chip* pChip) override;

  private:
    void                             InitRD53UplinkSpeed(Ph2_HwDescription::ReadoutChip* pChip) override;
    void                             WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault) override;
    std::pair<std::string, uint16_t> SplitSpecialRegisters(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;

    void SendGlobalPulse(Ph2_HwDescription::Chip* pChip, uint16_t route, uint16_t pulseDuration);
    void SendGlobalPulseBroadcast(const Ph2_HwDescription::BeBoard* pBoard, uint16_t route, uint16_t pulseDuration);
};

} // namespace Ph2_HwInterface

#endif
