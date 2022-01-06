/*!
 *
 * \file PSHybridTester_h__.h
 * \brief PSHybridTester_h__ class, PSHybridTester_h__ of the hardware
 *
 * \Support : inna.makarenko@cern.ch
 *
 */

#ifndef PSHybridTester_h__
#define PSHybridTester_h__

#include "LinkAlignmentOT.h"
#include "Tool.h"
#ifdef __TCUSB__
#include "USB_a.h"
#endif

#define PSHYBRIDMAXV 1.32

#include <map>
class PSHybridTester : public LinkAlignmentOT
{
  public:
    PSHybridTester();
    ~PSHybridTester();

    void Initialise();
    void CheckHybridCurrents();
    void CheckHybridVoltages();
    void CheckFastCommands(const std::string& pFastCommand, uint8_t pDuartion = 1);
    void CheckHybridInputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters);
    void CheckHybridOutputs(std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters);
    void ReadSSABias(const std::string& pBiasName);
    void SSATestStubOutput(const std::string& cSSAPairSel);
    void SSATestL1Output(const std::string& cSSAPairSel);
    void SetHybridVoltage();
    void MPATest(uint32_t pPattern);
    void SelectCIC(bool pSelect);
    void SelectAntennaPosition(const std::string& pPosition, uint16_t pPotentiometer);

    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

  private:
    void ReadSSABias(Ph2_HwDescription::BeBoard* pBoard, const std::string& pBiasName);
    void CheckHybridInputs(Ph2_HwDescription::BeBoard* pBoard, std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters);
    void CheckHybridOutputs(Ph2_HwDescription::BeBoard* pBoard, std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters);
    void CheckFastCommands(Ph2_HwDescription::BeBoard* pBoard, const std::string& pFastCommand, uint8_t pDuartion = 1);
    void ReadHybridVoltage(const std::string& pVoltageName);
    void ReadHybridCurrent(const std::string& pCurrentName);
    // functions to test SSA outputs (pogo)
    void SSAPairSelect(Ph2_HwDescription::BeBoard* pBoard, const std::string& SSAPairSel);
    void SSAOutputsPogoDebug(Ph2_HwDescription::BeBoard* pBoard, bool pTrigger = false);
    void SSATestStubOutput(Ph2_HwDescription::BeBoard* pBoard, const std::string& cSSAPairSel);
    void SSATestL1Output(Ph2_HwDescription::BeBoard* pBoard, const std::string& cSSAPairSel);
    void SSAOutputsPogoScope(Ph2_HwDescription::BeBoard* pBoard, bool pTrigger = false);
    void MPATest(Ph2_HwDescription::BeBoard* pBoard, uint32_t pPattern);

    std::map<std::string, uint8_t> fInputDebugMap = {{"sda_out", 0},      {"rtn_clk320", 1},   {"cic_out_6", 2},    {"cic_out_5", 3},    {"cic_out_4", 4},    {"cic_out_3", 5},    {"cic_out_2", 6},
                                                     {"cic_out_1", 7},    {"cic_out_0", 8},    {"sda_out", 9},      {"ssa1_clk320", 10}, {"ssa1_fcmd", 11},   {"ssa1_l1", 12},     {"ssa1_trig_7", 13},
                                                     {"ssa1_trig_6", 14}, {"ssa1_trig_5", 15}, {"ssa1_trig_4", 16}, {"ssa1_trig_3", 17}, {"ssa1_trig_2", 18}, {"ssa1_trig_1", 19}, {"ssa1_trig_0", 20},
                                                     {"ssa2_clk320", 21}, {"ssa2_fcmd", 22},   {"ssa2_l1", 23},     {"ssa2_trig_7", 24}, {"ssa2_trig_6", 25}, {"ssa2_trig_5", 26}, {"ssa2_trig_4", 27},
                                                     {"ssa2_trig_3", 28}, {"ssa2_trig_2", 29}, {"ssa2_trig_1", 30}, {"ssa2_trig_0", 31}, {"spare_out", 32},   {"antt_fb", 33},     {"cpg", 34},
                                                     {"bpg", 35},         {"na", 36}};

    std::map<std::string, uint8_t> fOutputDebugMap = {};

    std::map<std::string, uint8_t> fSSAPairSelMap = {
        {"01", 0x4},
        {"12", 0x5}, // 0b0101},
        {"23", 0x1}, // 0b0001},
        {"34", 0x2}, // 0b0010},
        {"45", 0xE}, // 0b1110},
        {"56", 0xF}, // 0b1111},
        {"67", 0xB}  // 0b1011}};
    };

#ifdef __TCUSB__
    std::map<std::string, TC_PSFE::measurement> fHybridVoltageMap = {{"TestCardGround", TC_PSFE::measurement::GROUND},
                                                                     {"PanasonicGround", TC_PSFE::measurement::ROH_GND},
                                                                     {"Hybrid1V00", TC_PSFE::measurement::_1V},
                                                                     {"Hybrid1V25", TC_PSFE::measurement::_1V25},
                                                                     {"Hybrid1V25", TC_PSFE::measurement::_1V25_OUT}, // end group 1V25O
                                                                     {"Hybrid3V3", TC_PSFE::measurement::_3V3},       // end group 1V25O
                                                                     {"ADC", TC_PSFE::measurement::AMUX}};
    std::map<std::string, TC_PSFE::measurement> fHybridCurrentMap = {{"Hybrid1V00", TC_PSFE::measurement::ISEN_1V},
                                                                     {"Hybrid1V25", TC_PSFE::measurement::ISEN_1V25},
                                                                     {"Hybrid3V30", TC_PSFE::measurement::ISEN_3V3}};
#endif

    int                     fVoltageMeasurementWait_ms = 100;
    int                     fNreadings                 = 3;
    std::pair<float, float> fVoltageMeasurement;
    std::pair<float, float> fCurrentMeasurement;
};

#endif
