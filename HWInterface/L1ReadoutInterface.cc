#include "HWInterface/L1ReadoutInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
L1ReadoutInterface::L1ReadoutInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable)
{
    fData.clear();
    fNEvents              = 100;
    fFastCommandInterface = nullptr;
    fTriggerInterface     = nullptr;
}
L1ReadoutInterface::L1ReadoutInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId)
{
    LOG(INFO) << BOLDYELLOW << "L1ReadoutInterface::L1ReadoutInterface Constructor" << RESET;
    fData.clear();
    fNEvents              = 100;
    fFastCommandInterface = nullptr;
    fTriggerInterface     = nullptr;
}
L1ReadoutInterface::~L1ReadoutInterface() {}

} // namespace Ph2_HwInterface