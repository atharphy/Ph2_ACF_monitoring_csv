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

#include "../Utils/ConsoleColor.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/bit_packing.h"
#include "../Utils/easylogging++.h"
#include "ReadoutChip.h"

#include <iomanip>

// #############
// # CONSTANTS #
// #############
#define NROWS 192 // Total number of rows
#define NCOLS 400 // Total number of columns

// #########################
// # Chip useful constants #
// #########################
namespace RD53Constants
{
const uint8_t  BROADCAST_CHIPID  = 0x08; // Broadcast chip ID used to send the command to multiple chips
const uint8_t  NREGIONS_LONGCMD  = 6;    // Number of regions to program with long write commands
const uint8_t  FIELDS_SHORTCMD   = 8;    // Number of fields for the short write command
const uint8_t  FIELDS_LONGCMD    = 24;   // Number of fields for the long write command
const uint8_t  NBIT_TDAC         = 4;    // Number of TDAC bits
const uint8_t  NBIT_MAXREG       = 16;   // Maximum number of bits for a chip register
const uint8_t  NPIX_REGION       = 4;    // Number of pixels in a region (1x4)
const uint8_t  NROW_CORE         = 8;    // Number of rows in a core
const uint8_t  NBIT_ADDR         = 9;    // Number of address bits
const uint8_t  NSYNC_WORS        = 64;   // Number of Sync words for synchronization
const uint16_t CDRCONFIG_1Gbit   = 1048; // Value for 1.28 Gbit/s
const uint16_t CDRCONFIG_640Mbit = 1049; // Value for 640 Mbit/s
const uint8_t  PATTERN_PRBS      = 0x02; // Start PRBS pattern
const uint8_t  PATTERN_AURORA    = 0x01; // Start AURORA pattern
const uint8_t  PATTERN_CLOCK     = 0x00; // Start clock pattern
const uint16_t GLOBAL_PULSE_ADDR = 0x2C; // Global Pulse Route regiser address
const uint16_t SET_SEL_OUT_ADDR  = 0x44; // SET_SEL_OUT regiser address
} // namespace RD53Constants

// ############
// # Commands #
// ############
namespace RD53CmdEncoder
{
const uint16_t RESET_ECR  = 0x5A5A; // Event Counter Reset word
const uint16_t RESET_BCR  = 0x5959; // Bunch Counter Reset word
const uint16_t GLOB_PULSE = 0x5C5C; // Global pulse word
const uint16_t CAL        = 0x6363; // Calibration word
const uint16_t WRITE      = 0x6666; // Write command word
const uint16_t READ       = 0x6565; // Read command word
const uint16_t NOOP       = 0x6969; // No operation word
const uint16_t SYNC       = 0x817E; // Synchronization word
} // namespace RD53CmdEncoder

namespace RD53EvtEncoder
{
// #######################
// # Event configuration #
// #######################
const uint8_t HEADER      = 0x1; // Data header word
const uint8_t NBIT_HEADER = 7;   // Number of data header bits
const uint8_t NBIT_TRIGID = 5;   // Number of trigger ID bits
const uint8_t NBIT_TRGTAG = 5;   // Number of trigger tag bits
const uint8_t NBIT_BCID   = 15;  // Number of bunch crossing ID bits
const uint8_t NBIT_TOT    = 16;  // Number of ToT bits
const uint8_t NBIT_SIDE   = 1;   // Number of "side" bits
const uint8_t NBIT_ROW    = 9;   // Number of row bits
const uint8_t NBIT_CCOL   = 6;   // Number of core column bits

// ################
// # Event status #
// ################
const uint16_t CHIPGOOD  = 0x0000; // Chip event status Good
const uint16_t CHIPHEAD  = 0x0100; // Chip event status Bad chip header
const uint16_t CHIPPIX   = 0x0200; // Chip event status Bad pixel row or column
const uint16_t CHIPNOHIT = 0x0400; // Chip event status Hit data are missing
} // namespace RD53EvtEncoder

// #####################################################################
// # Formula: par0/par1 * VCal / electron_charge [C] * capacitance [C] #
// #####################################################################
namespace RD53chargeConverter
{
constexpr float par0   = 0.9;    // Vref [V]
constexpr float par1   = 4096.0; // VCal total range
constexpr float cap    = 8.5;    // [fF]
constexpr float ele    = 1.6;    // [e-19]
constexpr float offset = 64;     // Due to VCal_High vs VCal_Med offset difference [e-]

constexpr float VCal2Charge(float VCal, bool isNoise = false) { return (par0 / par1) * VCal / ele * cap * 1e4 + (isNoise == false ? offset : 0); }

constexpr float Charge2VCal(float Charge) { return (Charge - offset) / (cap * 1e4) * ele / (par0 / par1); }
} // namespace RD53chargeConverter

namespace Ph2_HwDescription
{
struct perColumnPixelData
{
    std::array<uint8_t, NROWS> Enable;
    std::array<uint8_t, NROWS> HitBus;
    std::array<uint8_t, NROWS> InjEn;
    std::array<uint8_t, NROWS> TDAC;
};

class RD53 : public ReadoutChip
{
  public:
    static constexpr size_t nRows = NROWS;
    static constexpr size_t nCols = NCOLS;

    // ########################################
    // # Support for different FrontEnd types #
    // ########################################
    struct FrontEnd
    {
        const char* name;
        const char* thresholdReg;
        const char* gainReg;
        size_t      nTDACvalues;
        size_t      colStart;
        size_t      colStop;
    };

    static constexpr FrontEnd SYNC = {"SYNC", "VTH_SYNC", "IBIAS_KRUM_SYNC", 0, 0, 127};
    static constexpr FrontEnd LIN  = {"LIN", "Vthreshold_LIN", "KRUM_CURR_LIN", 16, 128, 263};
    static constexpr FrontEnd DIFF = {"DIFF", "VTH1_DIFF", "VFF_DIFF", 31, 264, 399};
    static const FrontEnd*    frontEnds[];
    static const FrontEnd*    getMajorityFE(size_t colStart, size_t colStop);

    RD53(uint8_t pBeId, uint8_t pFMCId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName);
    RD53(const RD53& chipObj);

    void     loadfRegMap(const std::string& fileName) override;
    void     saveRegMap(const std::string& fName2Add) override;
    uint32_t getNumberOfChannels() const override;
    bool     isDACLocal(const std::string& regName) override;
    uint8_t  getNumberOfBits(const std::string& regName) override;

    std::string                      getFileName(const std::string& fName2Add) { return RD53Shared::composeFileName(configFileName, fName2Add); }
    std::vector<perColumnPixelData>* getPixelsMask() { return &fPixelsMask; }
    std::vector<perColumnPixelData>* getPixelsMaskDefault() { return &fPixelsMaskDefault; }

    void    copyMaskFromDefault();
    void    copyMaskToDefault();
    void    resetMask();
    void    enableAllPixels();
    void    disableAllPixels();
    size_t  getNbMaskedPixels();
    void    enablePixel(unsigned int row, unsigned int col, bool enable);
    void    injectPixel(unsigned int row, unsigned int col, bool inject);
    void    setTDAC(unsigned int row, unsigned int col, uint8_t TDAC);
    void    resetTDAC();
    uint8_t getTDAC(unsigned int row, unsigned int col);
    uint8_t getChipLane() const { return myChipLane; }

    struct HitData
    {
        HitData(uint16_t row, uint16_t col, uint8_t tot) : row(row), col(col), tot(tot) {}

        uint16_t row;
        uint16_t col;
        uint8_t  tot;
    };

    struct Event
    {
        Event(const uint32_t* data, size_t n);

        uint16_t             trigger_id;
        uint16_t             trigger_tag;
        uint16_t             bc_id;
        std::vector<HitData> hit_data;

        uint16_t eventStatus;

      private:
        void DecodeQuad(uint32_t data);
    };

    struct CalCmd
    {
        CalCmd(const uint8_t& _cal_edge_mode, const uint8_t& _cal_edge_delay, const uint8_t& _cal_edge_width, const uint8_t& _cal_aux_mode, const uint8_t& _cal_aux_delay);

        void setCalCmd(const uint8_t& _cal_edge_mode, const uint8_t& _cal_edge_delay, const uint8_t& _cal_edge_width, const uint8_t& _cal_aux_mode, const uint8_t& _cal_aux_delay);

        uint32_t getCalCmd(const uint8_t& chipId);

        uint8_t cal_edge_mode;
        uint8_t cal_edge_delay;
        uint8_t cal_edge_width;
        uint8_t cal_aux_mode;
        uint8_t cal_aux_delay;
    };

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
    std::vector<perColumnPixelData> fPixelsMask;
    std::vector<perColumnPixelData> fPixelsMaskDefault;
    std::string                     configFileName;
    uint8_t                         myChipLane;
};
} // namespace Ph2_HwDescription

// ###############################
// # RD53 command base functions #
// ###############################
namespace RD53Cmd
{
// Map 5-bit to 8-bit fields
constexpr uint8_t map5to8bit[] = {
    0x6A, // 00: 0b01101010,
    0x6C, // 01: 0b01101100,
    0x71, // 02: 0b01110001,
    0x72, // 03: 0b01110010,
    0x74, // 04: 0b01110100,
    0x8B, // 05: 0b10001011,
    0x8D, // 06: 0b10001101,
    0x8E, // 07: 0b10001110,
    0x93, // 08: 0b10010011,
    0x95, // 09: 0b10010101,
    0x96, // 10: 0b10010110,
    0x99, // 11: 0b10011001,
    0x9A, // 12: 0b10011010,
    0x9C, // 13: 0b10011100,
    0xA3, // 14: 0b10100011,
    0xA5, // 15: 0b10100101,
    0xA6, // 16: 0b10100110,
    0xA9, // 17: 0b10101001,
    0xAA, // 18: 0b10101010,
    0xAC, // 19: 0b10101100,
    0xB1, // 20: 0b10110001,
    0xB2, // 21: 0b10110010,
    0xB4, // 22: 0b10110100,
    0xC3, // 23: 0b11000011,
    0xC5, // 24: 0b11000101,
    0xC6, // 25: 0b11000110,
    0xC9, // 26: 0b11001001,
    0xCA, // 27: 0b11001010,
    0xCC, // 28: 0b11001100,
    0xD1, // 29: 0b11010001,
    0xD2, // 30: 0b11010010,
    0xD4  // 31: 0b11010100
};

template <uint16_t cmdCode, size_t nFields>
class Command
{
    static_assert(nFields % 2 == 0, "RD53Cmd::Command: a command must have an even number of fields");

  public:
    void appendTo(std::vector<uint16_t>& frameVector) const
    {
        // Insert command code
        frameVector.push_back(cmdCode);

        // Insert: chip id, address and data
        for(auto i = 1; i < static_cast<int>(nFields); i += 2) frameVector.push_back(bits::pack<8, 8>(fields[i - 1], fields[i]));
    }

    std::vector<uint16_t> getFrames() const
    {
        std::vector<uint16_t> frameVector;

        frameVector.reserve(1 + nFields / 2);
        Command::appendTo(frameVector);

        return frameVector;
    }

  protected:
    template <int... Sizes, class... Args>
    uint8_t packAndEncode(Args&&... args)
    {
        return map5to8bit[bits::pack<Sizes...>(std::forward<Args>(args)...)];
    }

    std::array<uint8_t, nFields> fields;
};

struct ECR : public Command<RD53CmdEncoder::RESET_ECR, 0>
{
};
struct BCR : public Command<RD53CmdEncoder::RESET_BCR, 0>
{
};
struct NoOp : public Command<RD53CmdEncoder::NOOP, 0>
{
};
struct Sync : public Command<RD53CmdEncoder::SYNC, 0>
{
};
struct GlobalPulse : public Command<RD53CmdEncoder::GLOB_PULSE, 2>
{
    GlobalPulse(uint8_t chip_id, uint8_t data);
};
struct Cal : public Command<RD53CmdEncoder::CAL, 4>
{
    Cal(uint8_t chip_id, bool cal_edge_mode, uint8_t cal_edge_delay, uint8_t cal_edge_width, bool cal_aux_mode, uint8_t cal_aux_delay);
};
struct WrReg : public Command<RD53CmdEncoder::WRITE, 6>
{
    WrReg(uint8_t chip_id, uint16_t address, uint16_t value);
};
struct WrRegLong : public Command<RD53CmdEncoder::WRITE, 22>
{
    WrRegLong(uint8_t chip_id, uint16_t address, const std::vector<uint16_t>& values);
};
struct RdReg : public Command<RD53CmdEncoder::READ, 4>
{
    RdReg(uint8_t chip_id, uint16_t address);
};

} // namespace RD53Cmd

#endif
