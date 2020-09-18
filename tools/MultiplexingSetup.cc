#include "MultiplexingSetup.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

// initialize the static member

MultiplexingSetup::MultiplexingSetup() : Tool()
{
    fAvailableCards = 0;
    fAvailable.clear();
}

MultiplexingSetup::~MultiplexingSetup() {}

void MultiplexingSetup::Initialise()
{
    // If I do this here.. DLL does not lock
    for(auto cBoard: *fDetectorContainer)
    {
        auto cBeBoard = static_cast<BeBoard*>(cBoard);
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        fBeBoardInterface->setBoard(theBoardId);
        bool cSetupScanned = (fBeBoardInterface->ReadBoardReg(cBeBoard, "fc7_daq_stat.physical_interface_block.multiplexing_bp.setup_scanned") == 1);
        // if its not been scanned.. then send a reset 
        if( cSetupScanned )
        {
            LOG (INFO) << BOLDBLUE << "Set-up has already been scanned..." << RESET;
        }
        else
        {
            LOG (INFO) << BOLDBLUE << "Set-up has not been scanned..." << RESET;
            LOG(INFO) << BOLDBLUE << "Sending a global reset to the FC7 ..... " << RESET;
            fBeBoardInterface->WriteBoardReg(cBeBoard, "fc7_daq_ctrl.command_processor_block.global.reset", 0x1);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

// Scan multiplexing set-up
void MultiplexingSetup::Scan()
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        LOG(INFO) << BOLDBLUE << "Scanning all available backplanes and cards on BeBoard " << +theBoardId << RESET;
        fBeBoardInterface->setBoard(theBoardId);
        fBeBoardInterface->getBoardInfo(static_cast<BeBoard*>(cBoard));
        fAvailableCards = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ScanMultiplexingSetup();
        parseAvailable(false);
        printAvailableCards();
    }
}

// Disconnect multiplexing set-up
void MultiplexingSetup::Disconnect()
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        LOG(INFO) << BOLDBLUE << "Disconnecting all backplanes and cards on BeBoard " << +theBoardId << RESET;
        fBeBoardInterface->setBoard(theBoardId);
        fBeBoardInterface->getBoardInfo(static_cast<BeBoard*>(cBoard));
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->DisconnectMultiplexingSetup();
    }
}
void MultiplexingSetup::ConfigureSingleCard(uint8_t pBackPlaneId, uint8_t pCardId)
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        LOG(INFO) << BOLDBLUE << "Configuring backplane " << +pBackPlaneId << " card " << +pCardId << " on BeBoard " << +theBoardId << RESET;
        fBeBoardInterface->setBoard(theBoardId);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureMultiplexingSetup(pBackPlaneId, pCardId);
        parseAvailable();
        printAvailableCards();
    }
}
void MultiplexingSetup::ConfigureAll()
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        LOG(INFO) << BOLDBLUE << "Configuring all cards on BeBoard " << +theBoardId << RESET;
        fBeBoardInterface->setBoard(theBoardId);
        fAvailableCards = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ScanMultiplexingSetup();
        parseAvailable(false);
        printAvailableCards();
        for(const auto& el: fAvailable)
        {
            int         cBackPlaneId = el.first;
            const auto& cCardIds     = el.second;
            for(auto cCardId: cCardIds) this->ConfigureSingleCard(cBackPlaneId, cCardId);
        }
    }
}
void MultiplexingSetup::Power(bool pEnable)
{
    for(auto cBoard: *fDetectorContainer)
    {
        uint16_t theBoardId = static_cast<BeBoard*>(cBoard)->getBeBoardId();
        LOG(INFO) << BOLDBLUE << "Powering FMCs on " << +theBoardId << RESET;
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->InitFMCPower();
    }
}

void MultiplexingSetup::printAvailableCards()
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
std::map<int, std::vector<int>> MultiplexingSetup::getAvailableCards(bool filterBoardsWithoutCards)
{
    parseAvailable(filterBoardsWithoutCards);
    return fAvailable;
}

// State machine control functions
void MultiplexingSetup::Start(int currentRun)
{
    LOG(INFO) << "Starting Multiplexing set-up";
    Initialise();
}

void MultiplexingSetup::Stop()
{
    LOG(INFO) << "Stopping  Multiplexing set-up";
    // writeObjects();
    dumpConfigFiles();
    Destroy();
}

void MultiplexingSetup::Pause() {}

void MultiplexingSetup::Resume() {}
