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
#include "../Utils/Container.h"
#include "../Utils/RD53Event.h"
#include "../Utils/RD53Shared.h"
#include "../Utils/bit_packing.h"
#include "../Utils/easylogging++.h"
#include "BeBoard.h"
#include "ReadoutChip.h"

#include <iomanip>

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
    perColumnPixelData(size_t size) : Enable(size), HitBus(size), InjEn(size), TDAC(size) {}

    std::vector<uint8_t> Enable;
    std::vector<uint8_t> HitBus;
    std::vector<uint8_t> InjEn;
    std::vector<uint8_t> TDAC;
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
        size_t      nTDACvalues;
        size_t      colStart;
        size_t      colStop;
    };

    virtual size_t            getNRows() const                                                                                   = 0;
    virtual size_t            getNCols() const                                                                                   = 0;
    virtual const FrontEnd*   getMajorityFE(size_t colStart, size_t colStop) const                                               = 0;
    virtual void              decodeChipData(const uint32_t* data, size_t size, Ph2_HwInterface::RD53ChipEvent& chipEvent) const = 0;

    RD53(uint8_t pBeId, uint8_t pFMCId, uint8_t pOpticalGroupId, uint8_t pHybridId, uint8_t pRD53Id, uint8_t pRD53Lane, const std::string& fileName, const std::string& cfgComment);
    RD53(const RD53& chipObj);

    void     loadfRegMap(const std::string& fileName) override;
    void     saveRegMap(const std::string& fName2Add) override;
    uint32_t getNumberOfChannels() const override;
    bool     isDACLocal(const std::string& regName) override;
    uint8_t  getNumberOfBits(const std::string& regName) override;

    std::string                      getFileName(const std::string& fName2Add) { return RD53Shared::composeFileName(configFileName, fName2Add); }
    std::vector<perColumnPixelData>* getPixelsMask() { return &fPixelsMask; }
    std::vector<perColumnPixelData>* getPixelsMaskDefault() { return &fPixelsMaskDefault; }

    void        copyMaskFromDefault();
    void        copyMaskToDefault(const std::string& which = "all");
    void        resetMask();
    void        enableAllPixels();
    void        disableAllPixels();
    size_t      getNbMaskedPixels();
    void        enablePixel(unsigned int row, unsigned int col, bool enable);
    void        injectPixel(unsigned int row, unsigned int col, bool inject);
    void        setTDAC(unsigned int row, unsigned int col, uint8_t TDAC);
    void        resetTDAC();
    uint8_t     getTDAC(unsigned int row, unsigned int col);
    uint8_t     getChipLane() const { return myChipLane; }
    std::string getComment() const { return myComment; }

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
    std::string                     myComment;
    uint8_t                         myChipLane;
};
} // namespace Ph2_HwDescription

#endif
