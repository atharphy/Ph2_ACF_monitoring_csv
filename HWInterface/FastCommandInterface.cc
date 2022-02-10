#include "FastCommandInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
FastCommandInterface::FastCommandInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : RegManager(pId, pUri, pAddressTable) {
}
FastCommandInterface::FastCommandInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : RegManager(puHalConfigFileName, pBoardId) {
    
    LOG (INFO) << BOLDYELLOW << "FastCommandInterface::FastCommandInterface Constructor" << RESET;
}
FastCommandInterface::~FastCommandInterface() {}

}