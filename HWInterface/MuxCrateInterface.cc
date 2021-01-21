/*

        FileName :                     GbtInterface.cc
        Content :                      User Interface to the Cics
        Version :                      1.0
        Date of creation :             10/07/14

 */

#include "MuxCrateInterface.h"
// loggers + exceptions
#include "../Utils/ConsoleColor.h"
#include "../Utils/Exception.h"
#include "../Utils/easylogging++.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
// Default C'tor
MuxCrateInterface::MuxCrateInterface(const BeBoardFWMap& pBoardMap) : BeBoardInterface(pBoardMap), fWait_ms(0), fAvailableCards(0), fBackplaneNum(0), fCardNum(0) { fAvailable.clear(); }

MuxCrateInterface::~MuxCrateInterface() {}

void MuxCrateInterface::ScanCrate(BeBoard* pBoard)
{
    LOG(INFO) << BOLDBLUE << "Scanning all available backplanes and cards on BeBoard " << +pBoard->getBeBoardId() << RESET;
    fAvailableCards = this->ScanMultiplexingSetup(pBoard);
    this->parseAvailable(false);
}
void MuxCrateInterface::DisconnectCrate(BeBoard* pBoard)
{
    LOG(INFO) << BOLDBLUE << "Disconnecting all backplanes and cards on BeBoard " << +pBoard->getBeBoardId() << RESET;
    this->DisconnectMultiplexingSetup(pBoard);
    fAvailableCards = this->ScanMultiplexingSetup(pBoard);
    this->parseAvailable(false);
}
void MuxCrateInterface::SelectCard(BeBoard* pBoard, int pBackplaneNum, int pCardNum)
{
    LOG(INFO) << BOLDBLUE << "Configuring backplane " << +pBackplaneNum << " card " << +pCardNum << " on BeBoard " << +pBoard->getBeBoardId() << RESET;
    fBackplaneNum = pBackplaneNum;
    fCardNum      = pCardNum;
    this->ConfigureMultiplexingSetup(pBoard);
    fAvailableCards = this->ScanMultiplexingSetup(pBoard);
    this->parseAvailable(false);
}

// disconnect setup with multiplexing backplane
void MuxCrateInterface::DisconnectMultiplexingSetup(BeBoard* pBoard)
{
    LOG(INFO) << BOLDBLUE << "Disconnect multiplexing set-up" << RESET;

    bool L12Power = (ReadBoardReg(pBoard, "sysreg.fmc_pwr.l12_pwr_en") == 1);
    bool L8Power  = (ReadBoardReg(pBoard, "sysreg.fmc_pwr.l8_pwr_en") == 1);
    bool PGC2M    = (ReadBoardReg(pBoard, "sysreg.fmc_pwr.pg_c2m") == 1);
    if(!L12Power)
    {
        LOG(ERROR) << RED << "Power on L12 is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }
    if(!L8Power)
    {
        LOG(ERROR) << RED << "Power on L8 is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }
    if(!PGC2M)
    {
        LOG(ERROR) << RED << "PG C2M is not enabled" << RESET;
        throw std::runtime_error("FC7 power is not enabled!");
    }

    bool BackplanePG   = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.backplane_powergood") == 1);
    bool CardPG        = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.card_powergood") == 1);
    bool SystemPowered = false;
    if(BackplanePG && CardPG)
    {
        LOG(INFO) << BOLDBLUE << "Back-plane power good and card power good." << RESET;
        WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_disconnect", 0x1);
        SystemPowered = true;
    }
    else
    {
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Setup is disconnected" << RESET;
    }
    if(SystemPowered)
    {
        bool CardsDisconnected      = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.cards_disconnected") == 1);
        bool c                      = false;
        bool BackplanesDisconnected = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.backplanes_disconnected") == 1);
        bool b                      = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Disconnecting setup" << RESET;

        while(!CardsDisconnected)
        {
            if(c == false) LOG(INFO) << "Disconnecting cards";
            c = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
            CardsDisconnected = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.cards_disconnected") == 1);
        }

        while(!BackplanesDisconnected)
        {
            if(b == false) LOG(INFO) << "Disconnecting backplanes";
            b = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
            BackplanesDisconnected = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.backplanes_disconnected") == 1);
        }

        if(CardsDisconnected && BackplanesDisconnected)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Setup is disconnected" << RESET;
        }
    }
}

// scan setup with multiplexing backplane
uint32_t MuxCrateInterface::ScanMultiplexingSetup(BeBoard* pBoard)
{
    int AvailableBackplanesCards = 0;
    this->DisconnectMultiplexingSetup(pBoard);
    WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.backplane_num", 0xF);
    WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.card_num", 0xF);
    std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
    bool ConfigurationRequired = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.configuration_required") == 1);
    bool SystemNotConfigured   = false;
    if(ConfigurationRequired)
    {
        SystemNotConfigured = true;
        WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_configure", 0x1);
    }

    if(SystemNotConfigured == true)
    {
        bool SetupScanned = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        bool s            = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Scan setup" << RESET;
        while(!SetupScanned)
        {
            if(s == false) LOG(INFO) << "Scanning setup";
            s = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(fWait_ms));
            SetupScanned = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        }

        if(SetupScanned)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Setup is scanned" << RESET;
            AvailableBackplanesCards = ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.available_backplanes_cards");
        }
    }
    return AvailableBackplanesCards;
}

// configure setup with multiplexing backplane
uint32_t MuxCrateInterface::ConfigureMultiplexingSetup(BeBoard* pBoard)
{
    uint32_t cAvailableCards = 0;
    this->DisconnectMultiplexingSetup(pBoard);
    WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.backplane_num", 0xF & ~(1 << (3 - fBackplaneNum)));
    WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.card_num", 0xF & ~(1 << (3 - fCardNum)));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    bool ConfigurationRequired = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.configuration_required") == 1);
    bool SystemNotConfigured   = false;
    if(ConfigurationRequired)
    {
        SystemNotConfigured = true;
        WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.multiplexing_bp.setup_configure", 0x1);
        cAvailableCards = ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.available_backplanes_cards");
    }

    if(SystemNotConfigured == true)
    {
        bool SetupScanned = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        bool s            = false;
        LOG(INFO) << GREEN << "============================" << RESET;
        LOG(INFO) << BOLDGREEN << "Scan setup" << RESET;
        while(!SetupScanned)
        {
            if(s == false) LOG(INFO) << "Scanning setup";
            s = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            SetupScanned = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        }

        bool BackplaneValid = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.backplane_valid") == 1);
        bool CardValid      = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.card_valid") == 1);
        if(SetupScanned)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Setup is scanned" << RESET;
            if(BackplaneValid) { LOG(INFO) << BLUE << "Backplane configuration VALID" << RESET; }
            else
            {
                LOG(ERROR) << RED << "Backplane configuration is NOT VALID" << RESET;
                exit(0);
            }
            if(CardValid) { LOG(INFO) << BLUE << "Card configuration VALID" << RESET; }
            else
            {
                LOG(ERROR) << RED << "Card configuration is NOT VALID" << RESET;
                exit(0);
            }
            // LOG (INFO) << BLUE << AvailableBackplanesCards << RESET;
            // printAvailableBackplanesCards(parseAvailableBackplanesCards(AvailableBackplanesCards,false));
        }

        bool SetupConfigured = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_configured") == 1);
        bool c               = false;
        if(BackplaneValid && CardValid)
        {
            LOG(INFO) << GREEN << "============================" << RESET;
            LOG(INFO) << BOLDGREEN << "Configure setup" << RESET;
            while(!SetupConfigured)
            {
                if(c == false) LOG(INFO) << "Configuring setup";
                c = true;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                SetupConfigured = (ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_configured") == 1);
            }

            if(SetupConfigured)
            {
                LOG(INFO) << GREEN << "============================" << RESET;
                LOG(INFO) << BOLDGREEN << "Setup with backplane " << fBackplaneNum << " and card " << fCardNum << " is configured" << RESET;
                cAvailableCards = ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.available_backplanes_cards");
            }
        }
    }
    return cAvailableCards;
}

// parse
void MuxCrateInterface::parseAvailable(bool filterBoardsWithoutCards)
{
    fAvailable.clear();
    // copy "numbits" from "buf" starting at position "at"
    auto copybits = [](int buf, int at, int numbits) {
        int mask = ((~0u) >> (sizeof(int) * 8 - numbits)) << at; // 2nd aproach
        return ((buf & mask) >> at);
    };

    // split original integer into chunks of 5 bits
    std::vector<int> list_of_bp_and_cards;
    list_of_bp_and_cards.push_back(copybits(fAvailableCards, 15, 5)); // 0th board
    list_of_bp_and_cards.push_back(copybits(fAvailableCards, 10, 5)); // 1st board
    list_of_bp_and_cards.push_back(copybits(fAvailableCards, 5, 5));  // 2nd board
    list_of_bp_and_cards.push_back(copybits(fAvailableCards, 0, 5));  // 3rd board

    // iterate over
    for(unsigned int iBP = 0; iBP < list_of_bp_and_cards.size(); ++iBP)
    {
        // test leftmost bit corresponding to availability of the board
        if(list_of_bp_and_cards[iBP] & (1 << 4))
        {
            // if board is available
            fAvailable[iBP] = std::vector<int>{};
            // test cards state for given board
            for(unsigned int iCard = 0; iCard <= 3; iCard++)
            {
                if(list_of_bp_and_cards[iBP] & (1 << (3 - iCard)))
                { // if card is ON
                    fAvailable.at(iBP).push_back(iCard);
                }
            }
        }
    }

    if(filterBoardsWithoutCards)
    {
        for(auto itBPCard = fAvailable.cbegin(); itBPCard != fAvailable.cend();)
        {
            if(itBPCard->second.empty())
                fAvailable.erase(itBPCard++);
            else
                ++itBPCard;
        }
    }
}
// print
void MuxCrateInterface::printAvailableCards()
{
    for(auto const& itBPCard: fAvailable)
    {
        std::stringstream sstr;
        if(itBPCard.second.empty())
            sstr << "No cards";
        else
            for(auto const& itCard: itBPCard.second) sstr << itCard << " ";
        LOG(INFO) << BLUE << "Available cards for bp " << itBPCard.first << ":"
                  << "[ " << sstr.str() << "]" << RESET;
    }
}

} // namespace Ph2_HwInterface
