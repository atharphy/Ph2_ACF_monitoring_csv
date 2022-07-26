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

#include "../HWDescription/ChipRegItem.h"
#include "../HWDescription/RD53.h"
#include "../HWDescription/RD53A.h"
#include "../HWDescription/RD53B.h"
#include "../Utils/RD53ChannelGroupHandler.h"
#include "BeBoardFWInterface.h"
#include "RD53FWInterface.h"
#include "ReadoutChipInterface.h"

// #############
// # CONSTANTS #
// #############
#define VCALSLEEP 50000 // [microseconds]

namespace Ph2_HwInterface
{
class RD53Interface : public ReadoutChipInterface
{
  public:
    RD53Interface(const BeBoardFWMap& pBoardMap);

    // #############################
    // # Override member functions #
    // #############################
    bool     WriteChipReg(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, bool pVerifLoop = true) override;
    void     WriteBoardBroadcastChipReg(const Ph2_HwDescription::BeBoard* pBoard, const std::string& regName, uint16_t data) override;
    bool     WriteChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue, bool pVerifLoop = true) override;
    void     ReadChipAllLocalReg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName, ChipContainer& pValue) override;
    uint16_t ReadChipReg(Ph2_HwDescription::Chip* pChip, const std::string& regName) override;
    bool     ConfigureChipOriginalMask(Ph2_HwDescription::ReadoutChip* pChip, bool pVerifLoop = true, uint32_t pBlockSize = 310) override;
    bool     MaskAllChannels(Ph2_HwDescription::ReadoutChip* pChip, bool mask, bool pVerifLoop = true) override;
    bool     maskChannelsAndSetInjectionSchema(Ph2_HwDescription::ReadoutChip* pChip, const std::shared_ptr<ChannelGroupBase> group, bool mask, bool inject, bool pVerifLoop = false) override;
    void     producePhaseAlignmentPattern(Ph2_HwDescription::ReadoutChip* pChip, uint8_t pWait_ms = 10) override;
    // ##################
    // # PRBS generator #
    // ##################
    void StartPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip);
    void StopPRBSpattern(Ph2_HwDescription::ReadoutChip* pChip);
    // #############################

    virtual void Reset(Ph2_HwDescription::ReadoutChip* pChip, const size_t resetType) = 0;
    virtual void ChipErrorReport(Ph2_HwDescription::ReadoutChip* pChip);

    virtual void InitRD53Downlink(const Ph2_HwDescription::BeBoard* pBoard)                                                                                                  = 0;
    virtual void InitRD53Uplinks(Ph2_HwDescription::ReadoutChip* pChip, int nActiveLanes = 1)                                                                                = 0;
    virtual void PackWriteCommand(Ph2_HwDescription::Chip* pChip, const std::string& regName, uint16_t data, std::vector<uint16_t>& chipCommandList, bool updateReg = false) = 0;

    void SendChipCommands(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId);
    void PackHybridCommands(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint16_t>& chipCommandList, int hybridId, std::vector<uint32_t>& hybridCommandList);
    void SendHybridCommands(const Ph2_HwDescription::BeBoard* pBoard, const std::vector<uint32_t>& hybridCommandList);

  protected:
    virtual void                             WriteRD53Mask(Ph2_HwDescription::RD53* pRD53, bool doSparse, bool doDefault)                           = 0;
    virtual std::pair<std::string, uint16_t> SplitSpecialRegisters(std::string regName, uint16_t value, Ph2_HwDescription::ChipRegMap& pRD53RegMap) = 0;

    std::vector<std::pair<uint16_t, uint16_t>> ReadRD53Reg(Ph2_HwDescription::ReadoutChip* pChip, const std::string& regName);

    template <typename T>
    void SendCommand(Ph2_HwDescription::ReadoutChip* pChip, const T& cmd)
    {
        static_cast<RD53FWInterface*>(fBoardFW)->WriteChipCommand(serialize(cmd), pChip->getHybridId());
    }

    template <typename T, size_t N>
    static size_t ArraySize(const T (&)[N])
    {
        return N;
    }

    uint16_t SetFieldValue(uint16_t regValue, uint16_t fieldValue, uint8_t start, uint8_t size);
    struct SpecialRegInfo
    {
        std::string regName;
        uint8_t     start; // Bit index at which the special register, i.e. field, starts
    };

    // ###########################
    // # Dedicated to monitoring #
    // ###########################
  public:
    void ReadChipMonitor(Ph2_HwDescription::ReadoutChip* pChip, const std::vector<std::string>& args)
    {
        for(const auto& arg: args) ReadChipMonitor(pChip, arg);
    }
    float    ReadChipMonitor(Ph2_HwDescription::ReadoutChip* pChip, const std::string& observableName);
    float    ReadHybridTemperature(Ph2_HwDescription::ReadoutChip* pChip);
    float    ReadHybridVoltage(Ph2_HwDescription::ReadoutChip* pChip);
    uint32_t ReadChipADC(Ph2_HwDescription::ReadoutChip* pChip, const std::string& observableName);

  private:
    uint32_t getADCobservable(const std::string& observableName, bool* isCurrentNotVoltage);
    uint32_t measureADC(Ph2_HwDescription::ReadoutChip* pChip, uint32_t data);
    float    measureVoltageCurrent(Ph2_HwDescription::ReadoutChip* pChip, uint32_t data, bool isCurrentNotVoltage);
    float    measureTemperature(Ph2_HwDescription::ReadoutChip* pChip, uint32_t data);
    float    convertADC2VorI(Ph2_HwDescription::ReadoutChip* pChip, uint32_t value, bool isCurrentNotVoltage = false);
};

} // namespace Ph2_HwInterface

#endif
