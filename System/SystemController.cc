/*!
  \file                  SystemController.cc
  \brief                 Controller of the System, overall wrapper of the framework
  \author                Mauro DINARDO
  \version               2.0
  \date                  01/01/20
  Support:               email to mauro.dinardo@cern.ch
*/

#include "SystemController.h"
#include "../tools/CBCMonitor.h"
#include "../tools/DetectorMonitor.h"
#include "../tools/RD53Monitor.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace Ph2_System
{
SystemController::SystemController()
    : fBeBoardInterface(nullptr)
    , fReadoutChipInterface(nullptr)
    , fChipInterface(nullptr)
    , flpGBTInterface(nullptr)
    , fCicInterface(nullptr)
    , fDetectorContainer(nullptr)
    , fSettingsMap()
    , fFileHandler(nullptr)
    , fRawFileName("")
    , fWriteHandlerEnabled(false)
    , fStreamerEnabled(false)
    , fNetworkStreamer(nullptr)
    , fDetectorMonitor(nullptr)
{
}

SystemController::~SystemController() {}

void SystemController::Inherit(const SystemController* pController)
{
    fBeBoardInterface     = pController->fBeBoardInterface;
    fReadoutChipInterface = pController->fReadoutChipInterface;
    fChipInterface        = pController->fChipInterface;
    flpGBTInterface       = pController->flpGBTInterface;
    fBeBoardFWMap         = pController->fBeBoardFWMap;
    fSettingsMap          = pController->fSettingsMap;
    fFileHandler          = pController->fFileHandler;
    fStreamerEnabled      = pController->fStreamerEnabled;
    fNetworkStreamer      = pController->fNetworkStreamer;
    fDetectorContainer    = pController->fDetectorContainer;
    fCicInterface         = pController->fCicInterface;
    fPowerSupplyClient    = pController->fPowerSupplyClient;
}

void SystemController::Destroy()
{
    for(const auto cBoard: *fDetectorContainer)
        if(cBoard->getBoardType() == BoardType::RD53) static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->PrintErrorsLVDS(); // @TMP@

    this->closeFileHandler();

    LOG(INFO) << BOLDRED << ">>> Destroying interfaces <<<" << RESET;

    RD53Event::JoinDecodingThreads();

    delete fDetectorMonitor;
    fDetectorMonitor = nullptr;
    delete fBeBoardInterface;
    fBeBoardInterface = nullptr;
    delete fReadoutChipInterface;
    fReadoutChipInterface = nullptr;
    delete fChipInterface;
    fChipInterface = nullptr;
    delete flpGBTInterface;
    flpGBTInterface = nullptr;
    delete fDetectorContainer;
    fDetectorContainer = nullptr;

    delete fCicInterface;
    fCicInterface = nullptr;

    fBeBoardFWMap.clear();
    fSettingsMap.clear();

    delete fNetworkStreamer;
    fNetworkStreamer = nullptr;

    delete fPowerSupplyClient;
    fPowerSupplyClient = nullptr;

    LOG(INFO) << BOLDRED << ">>> Interfaces  destroyed <<<" << RESET;
}

void SystemController::addFileHandler(const std::string& pFilename, char pOption)
{
    if(pOption == 'r')
        fFileHandler = new FileHandler(pFilename, pOption);
    else if(pOption == 'w')
    {
        fRawFileName         = pFilename;
        fWriteHandlerEnabled = true;
    }
}

void SystemController::closeFileHandler()
{
    if(fFileHandler != nullptr)
    {
        if(fFileHandler->isFileOpen() == true) fFileHandler->closeFile();
        delete fFileHandler;
        fFileHandler = nullptr;
    }
}

void SystemController::readFile(std::vector<uint32_t>& pVec, uint32_t pNWords32)
{
    if(pNWords32 == 0)
        pVec = fFileHandler->readFile();
    else
        pVec = fFileHandler->readFileChunks(pNWords32);
}

void SystemController::InitializeHw(const std::string& pFilename, std::ostream& os, bool pIsFile, bool streamData)
{
    fStreamerEnabled = streamData;
    if(streamData == true)
    {
        fNetworkStreamer = new TCPPublishServer(6000, 1);
        fNetworkStreamer->startAccept();
    }

    fDetectorContainer = new DetectorContainer;
    this->fParser.parseHW(pFilename, fBeBoardFWMap, fDetectorContainer, os, pIsFile);
    fBeBoardInterface = new BeBoardInterface(fBeBoardFWMap);

    fPowerSupplyClient = new TCPClient("127.0.0.1", 7000);
    if(!fPowerSupplyClient->connect(1))
    {
        delete fPowerSupplyClient;
        fPowerSupplyClient = nullptr;
    }
    for(const auto board: *fDetectorContainer) fBeBoardInterface->setPowerSupplyClient(board, fPowerSupplyClient);

    if(fDetectorContainer->size() > 0)
    {
        const BeBoard* cFirstBoard = fDetectorContainer->at(0);

        if(cFirstBoard->getBoardType() != BoardType::RD53)
        {
            LOG(INFO) << BOLDBLUE << "Initializing HwInterfaces for OT BeBoards.." << RESET;
            if(cFirstBoard->size() > 0) // # of optical groups connected to Board0
            {
                auto cFirstOpticalGroup = cFirstBoard->at(0);
                LOG(INFO) << BOLDBLUE << "\t...Initializing HwInterfaces for OpticalGroups.." << +cFirstBoard->size() << " optical group(s) found ..." << RESET;

                if(cFirstOpticalGroup->flpGBT != nullptr)
                {
                    LOG(INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for lpGBT" << RESET;
                    flpGBTInterface = new D19clpGBTInterface(fBeBoardFWMap, cFirstBoard->ifUseOpticalLink(), cFirstBoard->ifUseCPB());
                }
                LOG(INFO) << BOLDBLUE << "Found " << +cFirstOpticalGroup->size() << " hybrids in this group..." << RESET;

                if(cFirstOpticalGroup->size() > 0) // # of hybrids connected to OpticalGroup0
                {
                    LOG(INFO) << BOLDBLUE << "\t\t...Initializing HwInterfaces for FrontEnd Hybrids.." << +cFirstOpticalGroup->size() << " hybrid(s) found ..." << RESET;
                    auto cFirstHybrid = cFirstOpticalGroup->at(0);
                    for(auto cROC: *cFirstHybrid)
                    {
                        auto cChipType = cROC->getFrontEndType();
                        if(cROC->getIndex() > 0) continue;

                        LOG(INFO) << BOLDBLUE << "\t\t\t...Assuming ROC#" << +cROC->getId() << " represents all ROCs on this hybrid" << RESET;
                        if(cChipType == FrontEndType::CBC3)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for CBC(s)" << RESET;
                            fReadoutChipInterface = new CbcInterface(fBeBoardFWMap);
                        }
                        else if(cChipType == FrontEndType::SSA)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for SSA(s)" << RESET;
                            fReadoutChipInterface = new SSAInterface(fBeBoardFWMap);
                        }
                        else if(cChipType == FrontEndType::MPA)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for MPA(s)" << RESET;
                            fReadoutChipInterface = new MPAInterface(fBeBoardFWMap);
                        }
                    }
                    LOG(INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for CIC" << RESET;
                    fCicInterface = new CicInterface(fBeBoardFWMap);
                }
            }
        }
        else
        {
            flpGBTInterface       = new RD53lpGBTInterface(fBeBoardFWMap);
            fReadoutChipInterface = new RD53Interface(fBeBoardFWMap);
        }
    }

    if(fWriteHandlerEnabled == true) this->initializeWriteFileHandler();

    DetectorMonitorConfig theDetectorMonitorConfig;
    std::string           monitoringType = fParser.parseMonitor(pFilename, theDetectorMonitorConfig, os, pIsFile);

    if(monitoringType != "None")
    {
        if(monitoringType == "2S")
            fDetectorMonitor = new CBCMonitor(*this, theDetectorMonitorConfig);
        else if(monitoringType == "RD53")
            fDetectorMonitor = new RD53Monitor(*this, theDetectorMonitorConfig);
        else
        {
            LOG(ERROR) << BOLDRED << "Unrecognized monitor type, Aborting" << RESET;
            abort();
        }
        fDetectorMonitor->forkMonitor();
    }
}

void SystemController::InitializeSettings(const std::string& pFilename, std::ostream& os, bool pIsFile) { this->fParser.parseSettings(pFilename, fSettingsMap, os, pIsFile); }

void SystemController::ReadSystemMonitor(BeBoard* pBoard, const std::vector<std::string>& args) const
{
    if(args.size() != 0)
        for(const auto cOpticalGroup: *pBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Monitor data for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << pBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    fBeBoardInterface->ReadChipMonitor(fReadoutChipInterface, cChip, args);
                    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
                }
}

void SystemController::ConfigureHw(bool bIgnoreI2c)
{
    if(fDetectorContainer == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Hardware not initialized: run SystemController::InitializeHw first" << RESET;
        return;
    }

    LOG(INFO) << BOLDMAGENTA << "@@@ Configuring HW parsed from xml file @@@" << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        if(cBoard->getBoardType() != BoardType::RD53)
        {
            uint8_t cAsync = (cBoard->getEventType() == EventType::SSAAS) ? 1 : 0;

            // setting up back-end board
            fBeBoardInterface->ConfigureBoard(cBoard);
            LOG(INFO) << GREEN << "Successfully configured Board " << int(cBoard->getId()) << RESET;
            LOG(INFO) << BOLDBLUE << "Now going to configure chips on Board " << int(cBoard->getId()) << RESET;

            // Link start-up
            // first configure lpGBT
            bool cIslpGBTI2C = false;
            for(auto cOpticalGroup: *cBoard)
            {
                if(cOpticalGroup->flpGBT == nullptr) continue;

                // are these needed?
                uint8_t cLinkId = cOpticalGroup->getId();
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(cLinkId);

                if(cOpticalGroup->flpGBT != nullptr)
                {
                    cIslpGBTI2C                         = !cBoard->ifUseOpticalLink();
                    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
                    if(cIslpGBTI2C)
                    {
#ifdef __TCUSB__
                        clpGBTInterface->InitialiseTCUSBHandler();
#endif
                    }
                    clpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT);
                }
            }
            // Check lpGBT Link Lock
            if(cIslpGBTI2C)
            {
                LOG(INFO) << BOLDBLUE << "Checking optical link link .." << RESET;
                bool clpGBTlock = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->LinkLock(cBoard);
                if(!clpGBTlock)
                {
                    LOG(INFO) << BOLDRED << "lpGBT link failed to LOCK!" << RESET;
                    exit(0);
                }
            }
            for(auto cOpticalGroup: *cBoard)
            {
                uint8_t cLinkId = cOpticalGroup->getId();
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(cLinkId);
                LOG(INFO) << BOLDMAGENTA << "CIC start-up seqeunce for hybrids on link " << +cLinkId << RESET;
                for(auto cHybrid: *cOpticalGroup)
                {
                    OuterTrackerHybrid* theOuterTrackerHybrid = static_cast<OuterTrackerHybrid*>(cHybrid);
                    if(theOuterTrackerHybrid->fCic != NULL)
                    {
                        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(cLinkId);
                        auto& cCic = theOuterTrackerHybrid->fCic;

                        // read CIC sparsification setting
                        bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
                        cBoard->setSparsification(cSparsified);

                        LOG(INFO) << BOLDBLUE << "Configuring CIC" << +(theOuterTrackerHybrid->getId() % 2) << " on link " << +theOuterTrackerHybrid->getLinkId() << " on hybrid "
                                  << +theOuterTrackerHybrid->getId() << RESET;
                        fCicInterface->ConfigureChip(cCic);

                        // CIC start-up
                        uint8_t cModeSelect = (static_cast<ReadoutChip*>(theOuterTrackerHybrid->at(0))->getFrontEndType() != FrontEndType::CBC3); // 0 --> CBC , 1 --> MPA
                        // select CIC mode
                        bool cSuccess = fCicInterface->SelectMode(cCic, cModeSelect);
                        if(!cSuccess)
                        {
                            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to configure CIC mode.." << RESET;
                            exit(EXIT_FAILURE);
                        }
                        LOG(INFO) << BOLDMAGENTA << "CIC configured for " << ((cModeSelect == 0) ? "2S" : "PS") << " readout." << RESET;
                        // CIC start-up sequence
                        uint8_t cDriveStrength = 5;
                        cSuccess               = fCicInterface->StartUp(cCic, cDriveStrength);
                        fBeBoardInterface->ChipReSync(cBoard);
                        LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " performed start-up sequence on CIC" << +(theOuterTrackerHybrid->getId() % 2) << " connected to link "
                                  << +theOuterTrackerHybrid->getLinkId() << RESET;
                        LOG(INFO) << BOLDGREEN << "####################################################################################" << RESET;
                    }
                    // Configure readout-chips [CBCs, MPAs, SSAs]
                    for(auto cReadoutChip: *cHybrid)
                    {
                        if(cReadoutChip != nullptr)
                        {
                            ReadoutChip* theReadoutChip = static_cast<ReadoutChip*>(cReadoutChip);
                            if(!bIgnoreI2c)
                            {
                                LOG(INFO) << BOLDBLUE << "Configuring readout chip [chip id " << +cReadoutChip->getId() << " ]" << RESET;
                                fReadoutChipInterface->ConfigureChip(theReadoutChip);
                            }
                            // if SSA + ASYNC
                            // make sure ROCs are configured for that
                            if(theReadoutChip->getFrontEndType() == FrontEndType::SSA) { fReadoutChipInterface->WriteChipReg(cReadoutChip, "AnalogueAsync", cAsync); }
                        }
                    }
                }
            }
        }
        else
        {
            // ######################################
            // # Configuring Inner Tracker hardware #
            // ######################################
            LOG(INFO) << BOLDBLUE << "\t--> Found an Inner Tracker board" << RESET;
            LOG(INFO) << GREEN << "Configuring Board: " << BOLDYELLOW << +cBoard->getId() << RESET;
            fBeBoardInterface->ConfigureBoard(cBoard);

            // ###################
            // # Configuring FSM #
            // ###################
            size_t nTRIGxEvent = SystemController::findValueInSettings<double>("nTRIGxEvent");
            size_t injType     = SystemController::findValueInSettings<double>("INJtype");
            size_t injLatency  = SystemController::findValueInSettings<double>("InjLatency");
            size_t nClkDelays  = SystemController::findValueInSettings<double>("nClkDelays");
            size_t colStart    = SystemController::findValueInSettings<double>("COLstart");
            bool   resetMask   = SystemController::findValueInSettings<double>("ResetMask");
            bool   resetTDAC   = SystemController::findValueInSettings<double>("ResetTDAC");
            LOG(INFO) << CYAN << "=== Configuring FSM fast command block ===" << RESET;
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->SetAndConfigureFastCommands(cBoard, nTRIGxEvent, injType, injLatency, nClkDelays, colStart < RD53::LIN.colStart);
            LOG(INFO) << CYAN << "================== Done ==================" << RESET;

            // ########################
            // # Configuring from XML #
            // ########################
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureFromXML(cBoard);

            // ########################
            // # Configure LpGBT chip #
            // ########################
            bool         LpGBTRxPhaseAllGood = true;
            unsigned int nAttempts           = 0;
            do
            {
                for(auto cOpticalGroup: *cBoard)
                    if(cOpticalGroup->flpGBT != nullptr)
                    {
                        LOG(INFO) << GREEN << "Initializing communication to Low-power Gigabit Transceiver (LpGBT): " << BOLDYELLOW << +cOpticalGroup->getId() << RESET;

                        if(flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT) == true)
                        {
                            LpGBTRxPhaseAllGood = flpGBTInterface->ExternalPhaseAlignRx(cOpticalGroup->flpGBT, cBoard, cOpticalGroup, this->fBeBoardFWMap[cBoard->getId()], fReadoutChipInterface);
                            nAttempts++;
                            LOG(INFO) << BOLDBLUE << ">>> LpGBT chip configured <<<" << RESET;
                        }
                        else
                            LOG(ERROR) << BOLDRED << ">>> LpGBT chip not configured, reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ") <<<" << RESET;
                    }

                // #######################
                // # Status optical link #
                // #######################
                uint32_t txStatus, rxStatus, mgtStatus;
                LOG(INFO) << GREEN << "Checking status of the optical links:" << RESET;
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->StatusOptoLink(txStatus, rxStatus, mgtStatus);

                // ######################################################
                // # Configure down and up-links to/from frontend chips #
                // ######################################################
                LOG(INFO) << CYAN << "=== Configuring frontend chip communication ===" << RESET;
                static_cast<RD53Interface*>(fReadoutChipInterface)->InitRD53Downlink(cBoard);
                for(auto cOpticalGroup: *cBoard)
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        LOG(INFO) << GREEN << "Initializing chip communication of hybrid: " << RESET << BOLDYELLOW << +cHybrid->getId() << RESET;
                        for(const auto cChip: *cHybrid)
                        {
                            LOG(INFO) << GREEN << "Initializing communicationng to/from RD53: " << RESET << BOLDYELLOW << +cChip->getId() << RESET;
                            static_cast<RD53Interface*>(fReadoutChipInterface)->InitRD53Uplinks(cChip);
                        }
                    }
                LOG(INFO) << CYAN << "==================== Done =====================" << RESET;

                // ####################################
                // # Check AURORA lock on data stream #
                // ####################################
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->CheckChipCommunication(cBoard);

            } while((LpGBTRxPhaseAllGood == false) && (nAttempts < RD53Shared::MAXATTEMPTS));

            // ############################
            // # Configure frontend chips #
            // ############################
            LOG(INFO) << CYAN << "===== Configuring frontend chip registers =====" << RESET;
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    LOG(INFO) << GREEN << "Configuring chip of hybrid: " << RESET << BOLDYELLOW << +cHybrid->getId() << RESET;
                    for(const auto cChip: *cHybrid)
                    {
                        LOG(INFO) << GREEN << "Configuring RD53: " << RESET << BOLDYELLOW << +cChip->getId() << RESET;
                        if(resetMask == true) static_cast<RD53*>(cChip)->enableAllPixels();
                        if(resetTDAC == true) static_cast<RD53*>(cChip)->resetTDAC();
                        static_cast<RD53*>(cChip)->copyMaskToDefault();
                        static_cast<RD53Interface*>(fReadoutChipInterface)->ConfigureChip(cChip);
                        LOG(INFO) << GREEN << "Number of masked pixels: " << RESET << BOLDYELLOW << static_cast<RD53*>(cChip)->getNbMaskedPixels() << RESET;
                        // static_cast<RD53Interface*>(fReadoutChipInterface)->CheckChipID(static_cast<RD53*>(cChip), 0); @TMP@
                    }
                }
            }

            LOG(INFO) << CYAN << "==================== Done =====================" << RESET;

            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->PrintFrequencyLVDS(); // @TMP@

            LOG(INFO) << GREEN << "Using " << BOLDYELLOW << RD53Shared::NTHREADS << RESET << GREEN << " threads for data decoding during running time" << RESET;
            RD53Event::ForkDecodingThreads();
        }
    }

    if(fDetectorMonitor != nullptr)
    {
        LOG(INFO) << GREEN << "Starting monitoring thread" << RESET;
        fDetectorMonitor->startMonitoring();
    }
}

void SystemController::initializeWriteFileHandler()
{
    for(const auto cBoard: *fDetectorContainer)
    {
        uint32_t cNChip        = 0;
        uint32_t cBeId         = cBoard->getId();
        uint32_t cNEventSize32 = this->computeEventSize32(cBoard);

        std::string cBoardTypeString;
        BoardType   cBoardType = cBoard->getBoardType();

        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup) cNChip += cHybrid->size();

        if(cBoardType == BoardType::D19C)
            cBoardTypeString = "D19C";
        else if(cBoardType == BoardType::RD53)
            cBoardTypeString = "RD53";

        uint32_t cFWWord  = fBeBoardInterface->getBoardInfo(cBoard);
        uint32_t cFWMajor = (cFWWord & 0xFFFF0000) >> 16;
        uint32_t cFWMinor = (cFWWord & 0x0000FFFF);

        FileHeader cHeader(cBoardTypeString, cFWMajor, cFWMinor, cBeId, cNChip, cNEventSize32, cBoard->getEventType());

        std::stringstream cBeBoardString;
        cBeBoardString << "_Board" << std::setw(3) << std::setfill('0') << cBeId;
        std::string cFilename = fRawFileName;
        if(fRawFileName.find(".raw") != std::string::npos) cFilename.insert(fRawFileName.find(".raw"), cBeBoardString.str());

        fFileHandler = new FileHandler(cFilename, 'w', cHeader);

        fBeBoardInterface->SetFileHandler(cBoard, fFileHandler);
        LOG(INFO) << GREEN << "Saving binary data into: " << RESET << BOLDYELLOW << cFilename << RESET;
    }
}

uint32_t SystemController::computeEventSize32(const BeBoard* pBoard)
{
    uint32_t cNEventSize32 = 0;
    uint32_t cNChip        = 0;

    for(const auto cOpticalGroup: *pBoard)
        for(const auto cHybrid: *cOpticalGroup) cNChip += cHybrid->size();

    if(pBoard->getBoardType() == BoardType::D19C) cNEventSize32 = D19C_EVENT_HEADER1_SIZE_32_CBC3 + cNChip * D19C_EVENT_SIZE_32_CBC3;

    return cNEventSize32;
}

void SystemController::Configure(std::string cHWFile, bool enableStream)
{
    std::stringstream outp;

    InitializeHw(cHWFile, outp, true, enableStream);
    InitializeSettings(cHWFile, outp);
    std::cout << outp.str() << std::endl;
    ConfigureHw();
}

void SystemController::Start(int runNumber)
{
    for(auto cBoard: *fDetectorContainer) fBeBoardInterface->Start(cBoard);
}

void SystemController::Stop()
{
    for(auto cBoard: *fDetectorContainer) fBeBoardInterface->Stop(cBoard);
}

void SystemController::Pause()
{
    for(auto cBoard: *fDetectorContainer) fBeBoardInterface->Pause(cBoard);
}

void SystemController::Resume()
{
    for(auto cBoard: *fDetectorContainer) fBeBoardInterface->Resume(cBoard);
}

void SystemController::StartBoard(BeBoard* pBoard) { fBeBoardInterface->Start(pBoard); }
void SystemController::StopBoard(BeBoard* pBoard) { fBeBoardInterface->Stop(pBoard); }
void SystemController::PauseBoard(BeBoard* pBoard) { fBeBoardInterface->Pause(pBoard); }
void SystemController::ResumeBoard(BeBoard* pBoard) { fBeBoardInterface->Resume(pBoard); }

void SystemController::Abort() { LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Abort not implemented" << RESET; }

uint32_t SystemController::ReadData(BeBoard* pBoard, bool pWait)
{
    std::vector<uint32_t> cData;
    return this->ReadData(pBoard, cData, pWait);
}

void SystemController::ReadData(bool pWait)
{
    for(auto cBoard: *fDetectorContainer) this->ReadData(cBoard, pWait);
}

uint32_t SystemController::ReadData(BeBoard* pBoard, std::vector<uint32_t>& pData, bool pWait)
{
    uint32_t cNPackets = fBeBoardInterface->ReadData(pBoard, false, pData, pWait);
    this->DecodeData(pBoard, pData, cNPackets, fBeBoardInterface->getBoardType(pBoard));
    return cNPackets;
}

void SystemController::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents)
{
    std::vector<uint32_t> cData;
    return this->ReadNEvents(pBoard, pNEvents, cData, true);
}

void SystemController::ReadNEvents(uint32_t pNEvents)
{
    for(auto cBoard: *fDetectorContainer) this->ReadNEvents(cBoard, pNEvents);
}

void SystemController::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
{
    fBeBoardInterface->ReadNEvents(pBoard, pNEvents, pData, pWait);

    uint32_t cMultiplicity = 0;
    if(fBeBoardInterface->getBoardType(pBoard) == BoardType::D19C) cMultiplicity = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    pNEvents = pNEvents * (cMultiplicity + 1);
    this->DecodeData(pBoard, pData, pNEvents, fBeBoardInterface->getBoardType(pBoard));
}

void SystemController::ReadASEvent(BeBoard* pBoard, uint32_t pNMsec, uint32_t pulses, bool fast, bool fsm)
{
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();

    std::vector<uint32_t> cData;
    if(fsm and (pulses > 0))
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Send_pulses(pulses);
    else
    {
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PS_Open_shutter(0);
        std::this_thread::sleep_for(std::chrono::microseconds(pNMsec));
        for(uint32_t i = 0; i < pulses; i++) { static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ChipTestPulse(); }
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PS_Close_shutter(0);
    }

    if(fast) { static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadASEvent(pBoard, cData); }
    else
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::MPA) static_cast<MPAInterface*>(fReadoutChipInterface)->ReadASEvent(cChip, cData);
                    if(cChip->getFrontEndType() == FrontEndType::SSA) static_cast<SSAInterface*>(fReadoutChipInterface)->ReadASEvent(cChip, cData);
                }
            }
        }
    }

    this->DecodeData(pBoard, cData, 1, fBeBoardInterface->getBoardType(pBoard));
}

// #################
// # Data decoding #
// #################
void SystemController::SetFuture(const BeBoard* pBoard, const std::vector<uint32_t>& pData, uint32_t pNevents, BoardType pType)
{
    if(pData.size() != 0) fFuture = std::async(&SystemController::DecodeData, this, pBoard, pData, pNevents, pType);
}

void SystemController::DecodeData(const BeBoard* pBoard, const std::vector<uint32_t>& pData, uint32_t pNevents, BoardType pType)
{
    // ####################
    // # Decoding IT data #
    // ####################
    if(pType == BoardType::RD53)
    {
        uint16_t status;
        fEventList.clear();
        if(RD53Event::decodedEvents.size() == 0) RD53Event::DecodeEventsMultiThreads(pData, RD53Event::decodedEvents, status);
        RD53Event::addBoardInfo2Events(pBoard, RD53Event::decodedEvents);
        for(auto& evt: RD53Event::decodedEvents) fEventList.push_back(&evt);
    }
    // ####################
    // # Decoding OT data #
    // ####################
    else if(pType == BoardType::D19C)
    {
        for(auto& pevt: fEventList) delete pevt;
        fEventList.clear();

        if(pNevents == 0) { LOG(INFO) << BOLDRED << "Asking to decode 0 events. . something might not be right here!!!" << RESET; }
        else
        {
            EventType fEventType = pBoard->getEventType();
            uint32_t  fNFe       = pBoard->getNFe();
            uint32_t  cBlockSize = 0x0000FFFF & pData.at(0);
            LOG(DEBUG) << BOLDBLUE << "Reading events from " << +fNFe << " FEs connected to uDTC...[ " << +cBlockSize * 4 << " 32 bit words to decode]" << RESET;
            fEventSize = static_cast<uint32_t>((pData.size()) / pNevents);
            // uint32_t nmpa = 0;
            uint32_t maxind = 0;

            // if(fEventType == EventType::SSAAS)
            //   {
            //   uint16_t nSSA = (fEventSize - D19C_EVENT_HEADER1_SIZE_32_SSA) / D19C_EVENT_SIZE_32_SSA / fNFe;
            //   nSSA = pData.size() / 120;
            //   }

            for(auto opticalGroup: *pBoard)
            {
                for(auto hybrid: *opticalGroup) { maxind = std::max(maxind, uint32_t(hybrid->size())); }
            }

            if(fEventType == EventType::SSAAS) { fEventList.push_back(new D19cSSAEventAS(pBoard, pData)); }
            else if(fEventType == EventType::MPAAS)
            {
                fEventList.push_back(new D19cMPAEventAS(pBoard, pData));
            }
            else if(fEventType != EventType::ZS)
            {
                size_t cEventIndex    = 0;
                auto   cEventIterator = pData.begin();
                do
                {
                    uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
                    auto     cEnd       = ((cEventIterator + cEventSize) > pData.end()) ? pData.end() : (cEventIterator + cEventSize);

                    // retrieve chunck of data vector belonging to this event
                    if(cEnd - cEventIterator == cEventSize)
                    {
                        std::vector<uint32_t> cEvent(cEventIterator, cEnd);
                        // some useful debug information
                        LOG(DEBUG) << BOLDGREEN << "Event" << +cEventIndex << " .. Data word that should be event header ..  " << std::bitset<32>(*cEventIterator) << ". Event is made up of "
                                   << +cEventSize << " 32 bit words..." << RESET;
                        if(pBoard->getFrontEndType() == FrontEndType::CBC3) { fEventList.push_back(new D19cCbc3Event(pBoard, cEvent)); }
                        else if(pBoard->getFrontEndType() == FrontEndType::CIC || pBoard->getFrontEndType() == FrontEndType::CIC2)
                        {
                            fEventList.push_back(new D19cCic2Event(pBoard, cEvent));
                        }
                        else if(pBoard->getFrontEndType() == FrontEndType::SSA)
                        {
                            fEventList.push_back(new D19cSSAEvent(pBoard, maxind, fNFe, cEvent));
                        }
                        else if(pBoard->getFrontEndType() == FrontEndType::MPA)
                        {
                            fEventList.push_back(new D19cMPAEvent(pBoard, maxind, fNFe, cEvent));
                        }
                        cEventIndex++;
                    }
                    cEventIterator += cEventSize;
                } while(cEventIterator < pData.end());
            }
        } // end zero check
    }
}

} // namespace Ph2_System
