#include "HWInterface/RD53Interface.h"
#include "HWInterface/lpGBTInterface.h"
#include "System/SystemController.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

uint32_t readRD53Efuse(RD53Interface* interface, Chip* chip)
{
    if(!interface->WriteChipReg(chip, "EfusesConfig", 0x0F0F)) throw std::runtime_error("failed to configure RD53 eFuse readout");
    const int32_t low = interface->ReadChipReg(chip, "EfusesReadData0");
    const int32_t high = interface->ReadChipReg(chip, "EfusesReadData1");
    if(low < 0 || high < 0) throw std::runtime_error("failed to read RD53 eFuse registers");
    return static_cast<uint32_t>(low) | (static_cast<uint32_t>(high) << chip->getNumberOfBits("EfusesReadData0"));
}

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "Usage: CombinedEfuseReader hardware.xml output.csv\n";
        return 1;
    }

    SystemController controller;
    try
    {
        std::stringstream parsed;
        controller.InitializeHw(argv[1], parsed);
        controller.InitializeSettings(argv[1], parsed);
        std::ofstream output(argv[2]);
        if(!output) throw std::runtime_error("cannot open output CSV");
        output << "board,optical,portcard_id,lpgbt_efuse,hybrid,chip,module_id,chip_efuse\n";

        auto* rd53 = static_cast<RD53Interface*>(controller.fReadoutChipInterface);
        for(auto* board: *controller.fDetectorContainer)
        {
            if(board->getBoardType() != BoardType::RD53) continue;
            controller.fBeBoardInterface->ConfigureBoard(board);
            controller.ConfigureIT(board);
            for(auto* optical: *board)
            {
                std::string lpgbtEfuse = "-1";
                if(optical->flpGBT != nullptr)
                {
                    std::ostringstream value;
                    value << "0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0')
                          << controller.flpGBTInterface->ReadChipFuseID(optical->flpGBT, optical->flpGBT->getVersion());
                    lpgbtEfuse = value.str();
                }
                for(auto* hybrid: *optical)
                    for(auto* chip: *hybrid)
                    {
                        rd53->ConfigureChip(chip);
                        output << board->getId() << ',' << optical->getId() << ",-1," << lpgbtEfuse << ','
                               << hybrid->getId() << ',' << +chip->getId() << ",," << readRD53Efuse(rd53, chip) << '\n';
                    }
            }
        }
        controller.Destroy();
    }
    catch(const std::exception& error)
    {
        controller.Destroy();
        std::cerr << "CombinedEfuseReader: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
