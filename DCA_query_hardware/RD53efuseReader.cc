#include "HWInterface/RD53Interface.h"
#include "System/SystemController.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

uint32_t readEfuse(RD53Interface* interface, Chip* chip)
{
    if(!interface->WriteChipReg(chip, "EfusesConfig", 0x0F0F)) throw std::runtime_error("failed to configure eFuse readout");
    const int32_t low = interface->ReadChipReg(chip, "EfusesReadData0");
    const int32_t high = interface->ReadChipReg(chip, "EfusesReadData1");
    if(low < 0 || high < 0) throw std::runtime_error("failed to read eFuse registers");
    return static_cast<uint32_t>(low) | (static_cast<uint32_t>(high) << chip->getNumberOfBits("EfusesReadData0"));
}

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "Usage: RD53efuseReader hardware.xml output.csv\n";
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
        output << "board,optical,hybrid,chip,efuse\n";

        auto* interface = static_cast<RD53Interface*>(controller.fReadoutChipInterface);
        for(auto* board: *controller.fDetectorContainer)
        {
            if(board->getBoardType() != BoardType::RD53) continue;
            controller.fBeBoardInterface->ConfigureBoard(board);
            controller.ConfigureIT(board);
            for(auto* optical: *board)
                for(auto* hybrid: *optical)
                    for(auto* chip: *hybrid)
                    {
                        interface->ConfigureChip(chip);
                        output << board->getId() << ',' << optical->getId() << ',' << hybrid->getId() << ',' << +chip->getId() << ',' << readEfuse(interface, chip) << '\n';
                    }
        }
        controller.Destroy();
    }
    catch(const std::exception& error)
    {
        controller.Destroy();
        std::cerr << "RD53efuseReader: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
