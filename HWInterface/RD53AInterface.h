/*!
  \file                  RD53AInterface.h
  \brief                 User interface to the RD53A readout chip
  \author                Mauro DINARDO and Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
  Support:               email to alkiviadis.papadopoulos@cern.ch
*/

#ifndef RD53AInterface_H
#define RD53AInterface_H

#include "../HWDescription/RD53A.h"
#include "../HWDescription/RD53ACommands.h"
#include "RD53Interface.h"

namespace Ph2_HwInterface
{
class RD53AInterface : public RD53Interface
{
  public:
    using RD53Interface::RD53Interface;

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    void     Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType) override;
    void     ChipErrorReport(Ph2_HwDescription::ReadoutChip* pChip) override;
    void     InitRD53Downlink(const Ph2_HwDescription::BeBoard* pBoard) override;
    void     InitRD53Uplinks(Ph2_HwDescription::ReadoutChip* pChip, int nActiveLanes = 1) override;
    void     PackWriteCommand(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = false) override;
    uint32_t ReadChipFuseID(Ph2_HwDescription::Chip* pChip) override { return pChip->getId(); }

  private:
    void                             InitRD53UplinkSpeed(Ph2_HwDescription::ReadoutChip* pChip) override;
    void                             WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault) override;
    std::pair<std::string, uint16_t> SplitSpecialRegisters(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) override;

    uint16_t GetPixelConfig(const Ph2_HwDescription::pixelMask& mask, uint16_t NRows, uint16_t row, uint16_t col, bool highGain);
    uint16_t SetFieldValue(uint16_t regValue, uint16_t fieldValue, uint8_t start, uint8_t size);
    struct SpecialRegInfo
    {
        std::string regName;
        uint8_t     start; // Bit index at which the special register, i.e. field, starts
    };
};

} // namespace Ph2_HwInterface

#endif
