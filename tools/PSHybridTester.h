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
#if defined(__TCUSB__)
#include "USB_a.h"
#endif

#include "../Utils/SSAChannelGroupHandler.h"
#include "DPInterface.h"
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
    void RunHybridETest();
    void CheckI2C();
    void CheckCounters();
    void CheckFastCommands(const std::string& pFastCommand, uint8_t pDuartion = 1);
    void CheckHybridInputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters);
    void CheckHybridOutputs(std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters);
    void ReadSSABias(const std::string& pBiasName);
    void ReadAntennaVoltage();
    void CalibrateSSABias();
    void CalibrateGainTrim();
    void SetTrim(std::string pTrimRegister, uint16_t pTrimValue);
    void SSATestStubOutput(const std::string& cSSAPairSel);
    void SSATestL1Output(const std::string& cSSAPairSel);
    void SSATestLateralCommunication(const std::string& cSSAPairSel, bool pSweepPhaseSelector = false);
    void SSAOutputsPogoScope(std::vector<std::vector<std::string>>&, std::string pSSAPairSel, bool pTrigger = false, bool pPrintScoped = true);
    void SetHybridVoltage(uint32_t pUsbBus, uint8_t pUsbDev);
    void SSAPairSelect(const std::string& SSAPairSel);
    void MPATest();
    void SelectCIC(bool pSelect);
    void AlignCICout(uint8_t pPattern = 0xAA);

    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    void SweepPhaseAlignment(uint8_t);

  private:
    void                     CheckI2C(Ph2_HwDescription::BeBoard* pBoard);
    void                     CheckCounters(Ph2_HwDescription::BeBoard* pBoard);
    void                     ReadSSABias(Ph2_HwDescription::BeBoard* pBoard, const std::string& pBiasName);
    void                     TrimSSABias(Ph2_HwDescription::BeBoard* pBoard, const std::string& pBiasName);
    void                     CalibrateSSABias(Ph2_HwDescription::BeBoard* pBoard);
    void                     CalibrateGainTrim(Ph2_HwDescription::BeBoard* pBoard);
    std::vector<double>      DecodeSSACentroids(std::vector<std::string>);
    std::vector<std::string> DecodeSSAL1Packet(int, std::string);
    void                     SetTrim(Ph2_HwDescription::BeBoard* pBoard, std::string pTrimRegister, uint16_t pTrimValue);
    void                     CheckHybridInputs(Ph2_HwDescription::BeBoard* pBoard, std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters);
    void                     CheckHybridOutputs(Ph2_HwDescription::BeBoard* pBoard, std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters);
    void                     CheckFastCommands(Ph2_HwDescription::BeBoard* pBoard, const std::string& pFastCommand, uint8_t pDuartion = 1);
    void                     ReadHybridVoltage(const std::string& pVoltageName);
    void                     ReadHybridCurrent(const std::string& pCurrentName);
    // functions to test SSA outputs (pogo)
    void SSAPairSelect(Ph2_HwDescription::BeBoard* pBoard, const std::string& SSAPairSel);
    void SSAOutputsPogoDebug(Ph2_HwDescription::BeBoard* pBoard, bool pTrigger = false);
    void SSATestStubOutput(Ph2_HwDescription::BeBoard* pBoard, const std::string& cSSAPairSel);
    void SSATestL1Output(Ph2_HwDescription::BeBoard* pBoard, const std::string& cSSAPairSel);
    void SSATestLateralCommunication(Ph2_HwDescription::BeBoard* pBoard, const std::string& pSSAPairSel, bool pSweepPhaseSelector);
    void SSAOutputsPogoScope(Ph2_HwDescription::BeBoard* pBoard, bool pTrigger = false);
    void SSAOutputsPogoScope(std::vector<std::vector<std::string>>&, std::string pSSAPairSel, Ph2_HwDescription::BeBoard* pBoard, bool pTrigger = false, bool pPrintScoped = true);
    void FillSSATree(std::string, std::string);
    void MPATest(Ph2_HwDescription::BeBoard* pBoard);
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

#if defined(__TCUSB__)
    // Maps for electrical measurements on the hybrid:
    std::map<std::string, TC_PSFE::measurement> fHybridVoltageMap = {
        {"TC_GND", TC_PSFE::measurement::GROUND},           {"ROH_GND", TC_PSFE::measurement::ROH_GND},          {"Hybrid1V00", TC_PSFE::measurement::_1V},
        {"Hybrid1V25", TC_PSFE::measurement::_1V25},        {"Hybrid1V25_out", TC_PSFE::measurement::_1V25_OUT}, {"Hybrid3V3", TC_PSFE::measurement::_3V3},
        {"HybridLoadV", TC_PSFE::measurement::_3V3_OUT},    {"VDrop2V55", TC_PSFE::measurement::_3V3_AMP},       {"ADC", TC_PSFE::measurement::AMUX},
        {"ROH_1V", TC_PSFE::measurement::_1V_OUT},          {"VDrop1V00", TC_PSFE::measurement::_1V_AMP},        {"VDrop1V25", TC_PSFE::measurement::_1V25_AMP},
        {"MPA1V_3", TC_PSFE::measurement::MPA_1V_3},        {"MPA_1V25A_3", TC_PSFE::measurement::MPA_1V25A_3},  {"ROH_GND_AMP", TC_PSFE::measurement::ROH_GND_AMP},
        {"MPA_1V25A_4", TC_PSFE::measurement::MPA_1V25A_4}, {"MPA_1V_4", TC_PSFE::measurement::MPA_1V_4},        {"Hybrid2V5", TC_PSFE::measurement::_2V5},
        {"Ref1V25", TC_PSFE::measurement::_1V25_REF},       {"RefV625", TC_PSFE::measurement::_625mV_REF},       {"AntennaPullUp", TC_PSFE::measurement::ANT_PULL}};
    std::map<std::string, TC_PSFE::measurement> fHybridCurrentMap = {{"Hybrid1V00_current", TC_PSFE::measurement::ISEN_1V},
                                                                     {"Hybrid1V25_current", TC_PSFE::measurement::ISEN_1V25},
                                                                     {"Hybrid3V30_current", TC_PSFE::measurement::ISEN_3V3}};
    std::map<std::string, TC_PSFE::measurement> fHybridOtherMap   = {{"Temperature", TC_PSFE::measurement::THERM_SENSE},
                                                                   {"PGLineContinuity", TC_PSFE::measurement::C_TEST_PG},
                                                                   {"12VLineContinuity", TC_PSFE::measurement::C_TEST_P12},
                                                                   {"MPAContinuity", TC_PSFE::measurement::MPA_RST_TEST}};
#endif
    // 0 means there is no nominal value and it does not have to be checked
    std::map<std::string, int> fHybridNominalValues = {{"TC_GND", 0},
                                                       {"ROH_GND", 0},
                                                       {"Hybrid1V00", 1000},
                                                       {"Hybrid1V25", 1250},
                                                       {"Hybrid1V25_out", 1250},
                                                       {"Hybrid3V3", 3300},
                                                       {"HybridLoadV", 0},
                                                       {"VDrop2V55", 0},
                                                       {"ADC", 0},
                                                       {"ROH_1V", 1000},
                                                       {"VDrop1V00", 0},
                                                       {"VDrop1V25", 0},
                                                       {"MPA1V_3", 1000},
                                                       {"MPA_1V25A_3", 1250},
                                                       {"ROH_GND_AMP", 0},
                                                       {"MPA_1V25A_4", 1250},
                                                       {"MPA_1V_4", 1000},
                                                       {"Hybrid2V5", 2500},
                                                       {"Ref1V25", 1250},
                                                       {"RefV625", 625},
                                                       {"Hybrid1V00_current", 0},
                                                       {"Hybrid1V25_current", 0},
                                                       {"Hybrid3V30_current", 0},
                                                       {"Temperature", 0},
                                                       {"PGLineContinuity", 0},
                                                       {"12VLineContinuity", 1},
                                                       {"MPAContinuity", 1},
                                                       {"AntennaPullUp", 0}};

    // Maps for the calibration of the SSA bias DACs accesible through the AMUX
    // On version 1 of the SSA they need to be read via the ADC on the test card
    // fDACsCalibrationMap = {<DAC to be calibrated>, <Adjustment register> }
    std::map<std::string, std::string> fDACsCalibrationMap = {{"BoosterFeedback", "Bias_D5BFEED"},
                                                              {"PreampBias", "Bias_D5PREAMP"},
                                                              {"VoltageBias", "Bias_D5ALLV"},
                                                              {"CurrentBias", "Bias_D5ALLI"},
                                                              {"DAC", "Bias_D5DAC8"}};
    // fDACsCalibrationTargetMap = {<DAC to be calibrated>, <Target ADC value>}
    std::map<std::string, float> fDACsCalibrationTargetMap = {{"BoosterFeedback", 82.0}, {"PreampBias", 82.0}, {"VoltageBias", 82.0}, {"CurrentBias", 82.0}, {"DAC", 86.0}};
#if defined(__USE_ROOT__)
    TTree* fSSATree = nullptr;
#endif
    std::string             fSSATreeParameter          = "";
    std::string             fSSATreeValue              = "";
    int                     fVoltageMeasurementWait_ms = 100;
    int                     fNreadings                 = 3;
    std::pair<float, float> fVoltageMeasurement;
    std::pair<float, float> fCurrentMeasurement;
};

#endif
