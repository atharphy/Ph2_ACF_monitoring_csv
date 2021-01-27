
#include "DPInterface.h"
#include "BeBoardFWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
DPInterface::DPInterface()
{
    fEmulatorRunning = false;
    fWait_us         = 1000;
}

DPInterface::~DPInterface() {}

void DPInterface::Configure(BeBoardFWInterface* pInterface, uint32_t pPattern)
{
    LOG(INFO) << BOLDBLUE << std::bitset<8>(pPattern) << " pattern to use in PS ROH data player." << RESET;

    pInterface->WriteReg("fc7_daq_cnfg.physical_interface_block.fe_data_player.pattern_320MHz", pPattern);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
}
void DPInterface::CheckNPatterns(BeBoardFWInterface* pInterface)
{
    auto cInput = pInterface->ReadReg("fc7_daq_stat.physical_interface_block.mpa_to_cic_cntr");
    LOG(INFO) << BOLDGREEN << " Data Player sent : " << +cInput << " patterns." << RESET;
}
void DPInterface::Start(BeBoardFWInterface* pInterface, uint8_t pType)
{
    pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.fe_data_player.start_data_player", 0x01);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    bool cIsRunning = this->IsRunning(pInterface, pType);
    if(cIsRunning) { LOG(INFO) << BOLDGREEN << " Data Player [STARTED]" << RESET; }
    else
    {
        LOG(INFO) << BOLDRED << " Data Player [START ERROR]" << RESET;
    }
}

bool DPInterface::IsRunning(BeBoardFWInterface* pInterface, uint8_t pType)
{
    std::string cRegName = (pType == 0) ? "fc7_daq_stat.physical_interface_block.fe_data_player.stat_feh_data_player" : "fc7_daq_stat.physical_interface_block.fe_data_player.stat_roh_data_player";
    fEmulatorRunning     = (pInterface->ReadReg(cRegName) == 1);
    return fEmulatorRunning;
}
void DPInterface::Stop(BeBoardFWInterface* pInterface)
{
    pInterface->WriteReg("fc7_daq_ctrl.physical_interface_block.fe_data_player.stop_data_player", 0x01);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    LOG(INFO) << BOLDBLUE << "FE data player for PS ROH stopped." << RESET;
}

} // namespace Ph2_HwInterface
