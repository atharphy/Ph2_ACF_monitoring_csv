/*!
  \file                  RD53.h
  \brief                 RD53 description class, config of the RD53
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53_H
#define RD53_H

#include "../Utils/BitMaster/bit_packing.h"
#include "../Utils/ConsoleColor.h"
#include "../Utils/Container.h"
#include "../Utils/RD53Event.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/easylogging++.h"
#include "BeBoard.h"
#include "ReadoutChip.h"

#include <iomanip>

// #########################
// # Chip useful constants #
// #########################
namespace RD53Constants
{
const uint8_t ACCELERATOR_CLK = 40;   // Accelerator clock frequency [MHz]
const uint8_t NBIT_MAXREG     = 16;   // Maximum number of bits for a chip register
const uint8_t NPIX_REGION     = 4;    // Number of pixels in a region (1x4)
const uint8_t NROW_CORE       = 8;    // Number of rows in a core
const uint8_t NBIT_ADDR       = 9;    // Number of address bits
const uint8_t NBIT_TOT        = 4;    // Number of ToT bits
const uint8_t NSYNC_WORDS     = 64;   // Number of Sync words for synchronization
const uint8_t NWORDS_TO_SYNC  = 30;   // Number of words beforse send a Sync
const uint8_t PATTERN_PRBS    = 0xAA; // Start PRBS pattern
const uint8_t PATTERN_AURORA  = 0x55; // Start AURORA pattern
const uint8_t PATTERN_CLOCK   = 0x00; // Start clock pattern
} // namespace RD53Constants

// #####################
// # Chip event status #
// #####################
namespace RD53EvtEncoder
{
const uint32_t CHIPGOOD    = 0x00000000; // Chip event status Good
const uint32_t CHIPHEAD    = 0x00010000; // Chip event status Bad chip header
const uint32_t CHIPID      = 0x00020000; // Chip event status Found conflicting chip ID
const uint32_t CHIPPIX     = 0x00040000; // Chip event status Bad pixel row or column
const uint32_t CHIPTOT     = 0x00080000; // Chip event status Invalid TOT value
const uint32_t CHIPNOHIT   = 0x00100000; // Chip event status Hit data are missing
const uint32_t CHIPFWERR   = 0x00200000; // Chip event status Firmware error
const uint32_t CHIPNS_WAS0 = 0x00400000; // Chip event status new-stream bit was 0 in the first word of the event stream
const uint32_t CHIPNS_WAS1 = 0x00800000; // Chip event status new-stream bit was 1 before the last word of the event stream
const uint32_t CHIP_QROW   = 0x01000000; // Chip event status neighbor bit set for the first qrow
} // namespace RD53EvtEncoder

namespace Ph2_HwDescription
{
// ############################################
// # Data structure describing the chip masks #
// ############################################
struct pixelMask
{
    pixelMask() = default;
    pixelMask(size_t size, bool en, bool hb, bool ie, uint8_t tdac) : Enable(size, en), HitBus(size, hb), InjEn(size, ie), TDAC(size, tdac) {}

    std::vector<bool>    Enable;
    std::vector<bool>    HitBus;
    std::vector<bool>    InjEn;
    std::vector<uint8_t> TDAC;
};

// ###################################################
// # Data structure describing the chip lane mapping #
// ###################################################
struct LaneConfig
{
    LaneConfig() : outputLaneMapping({0, 1, 2, 3}), inputLaneMapping({0, 1, 2, 3}), internalLanesEnabled({0, 0, 0, 0, 0}), nOutputLanes(1), isPrimary(true) {}
    LaneConfig(bool isPrimary, const std::array<uint8_t, 4>& outputLanes, const std::array<bool, 4>& signleChannelInputLanes, const std::array<bool, 4>& dualChannelInputLanes);

    template <typename T, size_t N, size_t S>
    auto serializeBits(std::array<T, N> arr)
    {
        uint16_t val = 0;
        for(auto i = 0u; i < N; i++) val |= arr[i] << (S * i);
        return val;
    }

    std::array<uint8_t, 4> outputLaneMapping;
    std::array<uint8_t, 4> inputLaneMapping;
    std::array<bool, 5>    internalLanesEnabled;
    uint8_t                nOutputLanes;
    bool                   isPrimary;
};

class RD53 : public ReadoutChip
{
  public:
    // ########################################
    // # Support for different FrontEnd types #
    // ########################################
    struct FrontEnd
    {
        const char* name;
        const char* thresholdReg;
        const char* gainReg;
        const char* latencyReg;
        const char* TDACGainReg;
        const char* VDDDreadReg;
        const char* VDDAreadReg;
        size_t      nLatencyBins2Span;
        size_t      nTDACvalues;
        size_t      maxToTvalue;
        size_t      splitToTvalue;
        size_t      maxBCIDvalue;
        size_t      maxTRIGIDvalue;
        size_t      nBitTrimDig;
        size_t      nBitTrimAna;
        size_t      colStart;
        size_t      colStop;
        size_t      VCalSleepTime; // [microseconds]
        size_t      AutoIncrementMask;
    };

    // ####################################
    // # Input&Output lanes configuration #
    // ####################################
    LaneConfig laneConfig;

    virtual size_t                getNRows() const                                                                                                           = 0;
    virtual size_t                getNCols() const                                                                                                           = 0;
    virtual std::vector<uint16_t> getLaneUpInitSequence() const                                                                                              = 0;
    virtual const FrontEnd*       getFEtype(const size_t colStart, const size_t colStop) const                                                               = 0;
    virtual uint32_t              getCalCmd(bool cal_edge_mode, size_t cal_edge_delay, size_t cal_edge_width, bool cal_aux_mode, size_t cal_aux_delay) const = 0;
    virtual float                 VCal2Charge(float VCal, bool isNoise = false) const                                                                        = 0;
    virtual float                 Charge2VCal(float Charge) const                                                                                            = 0;
    virtual bool                  getUseGainDualSlope() const                                                                                                = 0;

    RD53() : ReadoutChip(0, 0, 0, 0, 0) {}
    RD53(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);
    RD53(const RD53& chipObj);

    void              loadfRegMap(const std::string& fileName) override;
    std::stringstream saveRegMap(const std::string& fName2Add = "") override;
    uint32_t          getNumberOfChannels() const override;
    bool              isDACLocal(const std::string& regName) override;
    uint8_t           getNumberOfBits(const std::string& regName) override;

    pixelMask& getPixelsMask() { return fPixelsMask; }
    pixelMask& getPixelsMaskDefault() { return fPixelsMaskDefault; }

    void        copyMaskFromDefault(const std::string& which = "all");
    void        copyMaskToDefault(const std::string& which = "all");
    void        resetMask();
    void        enableAllPixels();
    void        disableAllPixels();
    size_t      getNbMaskedPixels();
    void        enablePixel(unsigned int row, unsigned int col, bool enable);
    void        injectPixel(unsigned int row, unsigned int col, bool inject);
    void        setTDAC(unsigned int row, unsigned int col, uint8_t TDAC);
    void        resetTDAC(uint8_t TDAC);
    uint8_t     getTDAC(unsigned int row, unsigned int col);
    uint8_t     getChipLane() const { return myChipLane; }
    std::string getComment() const { return myComment; }

    // #################
    // # LpGBT mapping #
    // #################
    void    setRxGroup(uint8_t pRxGroup) { fLpGBTmap.RxGroup = pRxGroup; }
    void    setRxChannel(uint8_t pRxChannel) { fLpGBTmap.RxChannel = pRxChannel; }
    void    setTxGroup(uint8_t pTxGroup) { fLpGBTmap.TxGroup = pTxGroup; }
    void    setTxChannel(uint8_t pTxChannel) { fLpGBTmap.TxChannel = pTxChannel; }
    uint8_t getRxGroup() { return fLpGBTmap.RxGroup; }
    uint8_t getRxChannel() { return fLpGBTmap.RxChannel; }
    uint8_t getTxGroup() { return fLpGBTmap.TxGroup; }
    uint8_t getTxChannel() { return fLpGBTmap.TxChannel; }

  private:
    struct LpGBTmap
    {
        uint8_t RxGroup;
        uint8_t RxChannel;
        uint8_t TxGroup;
        uint8_t TxChannel;
    } fLpGBTmap;
    pixelMask   fPixelsMask;
    pixelMask   fPixelsMaskDefault;
    std::string myComment;
    uint8_t     myChipLane;
};
} // namespace Ph2_HwDescription

#endif
