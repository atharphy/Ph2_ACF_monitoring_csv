/*!
  \file                  RD53Interface.h
  \brief                 User interface to the RD53 readout chip
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Interface_H
#define RD53Interface_H

#include "BeBoardFWInterface.h"
#include "RD53FWInterface.h"
#include "ReadoutChipInterface.h"

// #############
// # CONSTANTS #
// #############
#define VCALSLEEP 50000 // [microseconds]
#define NPIXCMD 100     // Number of possible pixel commands to stack
#define MONITORSLEEP 10 // [seconds]

namespace Ph2_HwInterface
{
class RD53Interface : public ReadoutChipInterface
{
  public:
    RD53Interface(const BeBoardFWMap& pBoardMap);

    bool     ConfigureChip(Ph2_HwDescription::Chip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode, uint16_t data, bool pVerifLoop = true) override;
    void     WriteBoardBroadcastChipReg(const Ph2_HwDescription::BeBoard* pBoard, const std::string& pRegNode, uint16_t data) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue, bool pVerifLoop = true) override;
    void     ReadChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode) override;
    bool     ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     MaskAllChannels(Ph2_HwDescription::ReadoutChip* pChip, bool mask, bool pVerifLoop = true) override;
    bool     maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const ChannelGroupBase* group, bool mask, bool inject, bool pVerifLoop = false) override;

  private:
    std::vector<std::pair<uint16_t, uint16_t>> ReadRD53Reg(Ph2_HwDescription::Chip* pChip, const std::string& pRegNode);
    void                                       WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault, bool pVerifLoop = false);
    void                                       InitRD53Aurora(Ph2_HwDescription::Chip* pChip, int nActiveLanes = 1);

    template <typename T>
    void sendCommand(Ph2_HwDescription::Chip* pChip, const T& cmd)
    {
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(cmd.getFrames(), pChip->getFeId());
    }

    template <typename T, size_t N>
    static size_t arraySize(const T (&)[N])
    {
        return N;
    }

    // ###########################
    // # Dedicated to minitoring #
    // ###########################
  public:
    template <typename T, typename... Ts>
    void ReadChipMonitor(Ph2_HwDescription::Chip* pChip, const T& observableName, const Ts&... observableNames)
    {
        ReadChipMonitor(pChip, observableName);
        ReadChipMonitor(pChip, observableNames...);
    }

    float ReadChipMonitor(Ph2_HwDescription::Chip* pChip, const char* observableName);
    float ReadHybridTemperature(Ph2_HwDescription::Chip* pChip);
    float ReadHybridVoltage(Ph2_HwDescription::Chip* pChip);

  private:
    uint32_t measureADC(Ph2_HwDescription::Chip* pChip, uint32_t data);
    float    measureVoltageCurrent(Ph2_HwDescription::Chip* pChip, uint32_t data, bool isCurrentNotVoltage);
    float    measureTemperature(Ph2_HwDescription::Chip* pChip, uint32_t data);
    float    convertADC2VorI(Ph2_HwDescription::Chip* pChip, uint32_t value, bool isCurrentNotVoltage = false);
};
} // namespace Ph2_HwInterface

#endif
