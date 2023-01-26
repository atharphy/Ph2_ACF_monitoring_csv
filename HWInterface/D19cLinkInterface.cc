#include "HWInterface/D19cLinkInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// ##########################################
// # Constructors #
// #########################################

D19cLinkInterface::D19cLinkInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : LinkInterface(pId, pUri, pAddressTable)
{
    fConfiguration.fResetWait_ms = 2000;
    fConfiguration.fReTry        = 1;
    fConfiguration.fMaxAttempts  = 10;
}

D19cLinkInterface::D19cLinkInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : LinkInterface(puHalConfigFileName, pBoardId)
{
    fConfiguration.fResetWait_ms = 2000;
    fConfiguration.fReTry        = 1;
    fConfiguration.fMaxAttempts  = 10;
}

D19cLinkInterface::~D19cLinkInterface() {}

void D19cLinkInterface::ResetLinks()
{
    // reset lpGBT core - for now there is one reset signal for all links
    this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
    std::this_thread::sleep_for(std::chrono::milliseconds(fConfiguration.fResetWait_ms));
    this->WriteReg("fc7_daq_ctrl.optical_block.general", 0x0);
    std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
}
bool D19cLinkInterface::GetLinkStatus(uint8_t pLinkId)
{
    bool     cLocked    = true;
    uint8_t  cCommandId = 1; // command id for  link status request is 1
    uint32_t cCommand   = ((pLinkId & 0x3f) << 26) | (cCommandId << 22);
    this->WriteReg("fc7_daq_ctrl.optical_block.general", cCommand);
    std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
    // read back status register
    LOG(INFO) << BOLDBLUE << "lpGBT Link Status..." << RESET;
    uint32_t cLinkStatus = this->ReadReg("fc7_daq_stat.optical_block");
    LOG(INFO) << BOLDBLUE << "lpGBT Link" << +pLinkId << " status " << std::bitset<32>(cLinkStatus) << RESET;
    std::vector<std::string> cStates = {"lpGBT TX Ready", "MGT Ready", "lpGBT RX Ready"};
    uint8_t                  cIndex  = 1;
    for(auto cState: cStates)
    {
        uint8_t cStatus = (cLinkStatus >> (3 - cIndex)) & 0x1;
        cLocked &= (cStatus == 1);
        if(cStatus == 1)
            LOG(INFO) << BOLDBLUE << "D19cLinkInterface:GetLinkStatus\t... " << cState << BOLDGREEN << "\t : LOCKED" << RESET;
        else
            LOG(INFO) << BOLDBLUE << "D19cLinkInterface:GetLinkStatus\t... " << cState << BOLDRED << "\t : FAILED" << RESET;
        cIndex++;
    }
    return cLocked;
}
void D19cLinkInterface::GeneralLinkReset(const BeBoard* pBoard)
{
    bool   cAllLocked   = false;
    size_t cMaxAttempts = fConfiguration.fReTry ? fConfiguration.fMaxAttempts : 1;
    size_t cAttempts    = 0;
    do
    {
        cAllLocked = true;
        LOG(INFO) << BOLDMAGENTA << "D19cLinkInterface::GeneralLinkReset Resetting lpGBT-FPGA core on BeBoard#" << +pBoard->getId() << " [Attempt#" << cAttempts++ << "]" << RESET;
        ResetLinks();
        for(auto cOpticalReadout: *pBoard) { cAllLocked = cAllLocked && GetLinkStatus(cOpticalReadout->getId()); }
    } while(cAttempts < cMaxAttempts && !cAllLocked);

    if(!cAllLocked)
    {
        LOG(ERROR) << BOLDRED << "Failed to lock all links after a general reset" << RESET;
        throw Exception("Failed to lock all links after a general reset");
    }
}

} // namespace Ph2_HwInterface