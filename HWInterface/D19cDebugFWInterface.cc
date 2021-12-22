#include "D19cDebugFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
D19cDebugFWInterface::D19cDebugFWInterface(const char* puHalConfigFileName, uint32_t pBoardId, FileHandler* pFileHandler) : BeBoardFWInterface(puHalConfigFileName, pBoardId) {}
D19cDebugFWInterface::D19cDebugFWInterface(const char* pId, const char* pUri, const char* pAddressTable, FileHandler* pFileHandler) : BeBoardFWInterface(pId, pUri, pAddressTable) {}
D19cDebugFWInterface::~D19cDebugFWInterface() {}

void D19cDebugFWInterface::ResetReadout()
{
    // LOG (INFO) << BOLDBLUE << "Resetting readout..." << RESET;
    auto cPkgDelay = this->ReadReg("fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    LOG(DEBUG) << "Package delay is set to " << +cPkgDelay << RESET;
    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    if(ReadReg("fc7_daq_stat.ddr3_block.is_ddr3_type"))
    {
        LOG(DEBUG) << BOLDBLUE << "Reseting DDR3 " << RESET;
        auto cDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        bool i               = false;
        while(!cDDR3Calibrated)
        {
            if(i == false) LOG(DEBUG) << "Waiting for DDR3 to finish initial calibration";
            i = true;
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
            cDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
        }
    }
}

std::string D19cDebugFWInterface::L1ADebug(uint8_t pWait_ms, bool pPrint)
{
    // disable back-pressure
    this->WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0);
    WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // reset trigger
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    // load new trigger configuration
    this->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);

    LOG(DEBUG) << BOLDMAGENTA << "First header found after " << this->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.first_header_delay") << " clock cycles." << RESET;
    auto cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.l1a_debug", 50);
    LOG(DEBUG) << BOLDBLUE << "Hits debug ...." << RESET;
    std::string cBuffer   = "";
    size_t      cLineIndx = 0;
    for(auto cWord: cWords)
    {
        auto                     cString = std::bitset<32>(cWord).to_string();
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < 4; cIndex++) { cOutputWords.push_back(cString.substr(cIndex * 8, 8)); }
        std::string cOutput = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput += *cIt + " ";
            cBuffer += *cIt;
        }
        if(pPrint) LOG(INFO) << BOLDBLUE << "#" << +cLineIndx << ":" << cOutput << RESET;
        cLineIndx++;
    }
    return cBuffer;
}
std::vector<std::string> D19cDebugFWInterface::StubDebug(bool pWithTestPulse, uint8_t pNlines)
{
    this->ResetReadout();
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 0;
    uint8_t cDuration = 0;
    if(pWithTestPulse) { cCalPulse = 1; }
    else
    {
        cL1A = 1;
    }
    uint32_t encode_resync    = cReSync << 16;
    uint32_t encode_cal_pulse = cCalPulse << 17;
    uint32_t encode_l1a       = cL1A << 18;
    uint32_t encode_bc0       = cBC0 << 19;
    uint32_t encode_duration  = cDuration << 28;
    uint32_t final_command    = encode_resync + encode_l1a + encode_cal_pulse + encode_bc0 + encode_duration;
    WriteReg("fc7_daq_ctrl.fast_command_block.control", final_command);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    auto                     cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.stub_debug", 80);
    std::vector<std::string> cLines(0);
    size_t                   cLine = 0;
    do
    {
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < 5; cIndex++)
        {
            auto cWord   = cWords[cLine * 10 + cIndex];
            auto cString = std::bitset<32>(cWord).to_string();
            for(size_t cOffset = 0; cOffset < 4; cOffset++) { cOutputWords.push_back(cString.substr(cOffset * 8, 8)); }
        }

        std::string cOutput_wSpace = "";
        std::string cOutput        = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput_wSpace += *cIt + " ";
            cOutput += *cIt;
        }
        LOG(INFO) << BOLDBLUE << "Line " << +cLine << " : " << cOutput_wSpace << RESET;
        cLines.push_back(cOutput);
        // cStrLength = cOutput.length();
        cLine++;
    } while(cLine < pNlines);
    this->ResetReadout();
    return cLines;
}
std::vector<std::string> D19cDebugFWInterface::ScopeStubLines(bool pWithTestPulse)
{
    this->ResetReadout();
    uint8_t cReSync   = 0;
    uint8_t cCalPulse = 0;
    uint8_t cL1A      = 0;
    uint8_t cBC0      = 0;
    uint8_t cDuration = 0;
    if(pWithTestPulse) { cCalPulse = 1; }
    else
    {
        cL1A = 1;
    }
    uint32_t encode_resync    = cReSync << 16;
    uint32_t encode_cal_pulse = cCalPulse << 17;
    uint32_t encode_l1a       = cL1A << 18;
    uint32_t encode_bc0       = cBC0 << 19;
    uint32_t encode_duration  = cDuration << 28;
    uint32_t final_command    = encode_resync + encode_l1a + encode_cal_pulse + encode_bc0 + encode_duration;
    WriteReg("fc7_daq_ctrl.fast_command_block.control", final_command);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    auto                     cWords = ReadBlockReg("fc7_daq_stat.physical_interface_block.stub_debug", 80);
    std::vector<std::string> cLines(0);
    size_t                   cLine   = 0;
    size_t                   cNlines = 6;
    // int cStrLength=0;
    do
    {
        std::vector<std::string> cOutputWords(0);
        for(size_t cIndex = 0; cIndex < cNlines; cIndex++)
        {
            auto cWord   = cWords[cLine * 10 + cIndex];
            auto cString = std::bitset<32>(cWord).to_string();
            for(size_t cOffset = 0; cOffset < 4; cOffset++) { cOutputWords.push_back(cString.substr(cOffset * 8, 8)); }
        }

        std::string cOutput_wSpace = "";
        std::string cOutput        = "";
        for(auto cIt = cOutputWords.end() - 1; cIt >= cOutputWords.begin(); cIt--)
        {
            cOutput_wSpace += *cIt + " ";
            cOutput += *cIt;
        }
        LOG(DEBUG) << BOLDBLUE << "Line " << +cLine << " : " << cOutput_wSpace << RESET;
        cLines.push_back(cOutput);
        cLine++;
    } while(cLine < cNlines);
    this->ResetReadout();
    return cLines;
}
} // namespace Ph2_HwInterface