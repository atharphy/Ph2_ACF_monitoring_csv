#include "HWDescription/lpGBT.h"
#include "HWInterface/lpGBTInterface.h"
#include "System/SystemController.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

INITIALIZE_EASYLOGGINGPP

using namespace Ph2_HwDescription;
using namespace Ph2_System;

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        std::cerr << "Usage: LpGBTefuseReader hardware.xml output.csv\n";
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
        output << "board,optical,lpgbt_efuse,portcard_id\n";

        for(auto* board: *controller.fDetectorContainer)
        {
            if(board->getBoardType() != BoardType::RD53) continue;
            controller.fBeBoardInterface->ConfigureBoard(board);
            controller.ConfigureIT(board);
            for(auto* optical: *board)
            {
                auto* lpgbt = optical->flpGBT;
                if(lpgbt == nullptr) continue;
                uint32_t efuse = controller.flpGBTInterface->ReadChipFuseID(lpgbt, lpgbt->getVersion());
                output << board->getId() << ',' << optical->getId() << ",0x" << std::uppercase << std::hex << std::setw(8) << std::setfill('0') << efuse << std::dec << ",\n";
            }
        }
        controller.Destroy();
    }
    catch(const std::exception& error)
    {
        controller.Destroy();
        std::cerr << "LpGBTefuseReader: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
