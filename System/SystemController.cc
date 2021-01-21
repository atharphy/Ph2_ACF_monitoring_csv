/*!
  \file                  SystemController.cc
  \brief                 Controller of the System, overall wrapper of the framework
  \author                Mauro DINARDO
  \version               2.0
  \date                  01/01/20
  Support:               email to mauro.dinardo@cern.ch
*/

#include "SystemController.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

namespace Ph2_System
{
SystemController::SystemController()
    : fBeBoardInterface(nullptr)
    , fReadoutChipInterface(nullptr)
    , fChipInterface(nullptr)
    , flpGBTInterface(nullptr)
    , fDetectorContainer(nullptr)
    , fSettingsMap()
    , fFileHandler(nullptr)
    , fRawFileName("")
    , fWriteHandlerEnabled(false)
    , fStreamerEnabled(false)
    , fNetworkStreamer(nullptr) // This is the server listening port
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
}

void SystemController::Destroy()
{
    this->closeFileHandler();

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
    if(streamData == true) fNetworkStreamer = new TCPPublishServer(6000, 1);

    fDetectorContainer = new DetectorContainer;
    this->fParser.parseHW(pFilename, fBeBoardFWMap, fDetectorContainer, os, pIsFile);
    fBeBoardInterface = new BeBoardInterface(fBeBoardFWMap);

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
                    // OuterTrackerModule* cFirstHybrid = static_cast<OuterTrackerModule*>(cFirstOpticalGroup->at(0));
                    // if( cFirstHybrid->size() > 0 )//# of ROCs connected to Hybrid0
                    // {
                    //   LOG (INFO) << BOLDBLUE << "\t\t...Initializing HwInterfaces for ROCs .."
                    //     << +cFirstHybrid->size()
                    //     << " ROCs found ..."
                    //     << RESET;
                    //   auto cFirstROC = cFirstHybrid->at(0);
                    //   auto cChipType = cFirstROC->getFrontEndType();
                    //   if (cChipType == FrontEndType::CBC3)
                    //   {
                    //     LOG (INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface(s) for CBC(s)" << RESET;
                    //     fReadoutChipInterface = new CbcInterface(fBeBoardFWMap);
                    //   }
                    //   else if(cChipType == FrontEndType::SSA)
                    //   {
                    //     LOG (INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface(s) for SSA(s)" << RESET;
                    //     fReadoutChipInterface = new SSAInterface(fBeBoardFWMap);
                    //   }
                    //   else if(cChipType == FrontEndType::MPA)
                    //   {
                    //     LOG (INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface(s) for MPA(s)" << RESET;
                    //     fReadoutChipInterface = new MPAInterface(fBeBoardFWMap);
                    //   }
                    //   LOG (INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for CIC" << RESET;
                    //   fCicInterface = new CicInterface(fBeBoardFWMap);
                    // }
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
}

void SystemController::InitializeSettings(const std::string& pFilename, std::ostream& os, bool pIsFile) { this->fParser.parseSettings(pFilename, fSettingsMap, os, pIsFile); }

void SystemController::ConfigureHw(bool bIgnoreI2c)
{
    if(fDetectorContainer == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Hardware not initialized: run SystemController::InitializeHw first" << RESET;
        return;
    }

    LOG(INFO) << BOLDMAGENTA << "@@@ Configuring HW parsed from .xml file @@@" << RESET;

    for(const auto cBoard: *fDetectorContainer)
    {
        uint8_t cAsync = (cBoard->getEventType() == EventType::SSAAS) ? 1 : 0;

        if(cBoard->getBoardType() != BoardType::RD53)
        {
            // setting up back-end board
            fBeBoardInterface->ConfigureBoard(cBoard);
            LOG(INFO) << GREEN << "Successfully configured Board " << int(cBoard->getId()) << RESET;
            LOG(INFO) << BOLDBLUE << "Now going to configure chips on Board " << int(cBoard->getId()) << RESET;

            // CIC start-up
            for(auto cOpticalGroup: *cBoard)
            {
                if(cOpticalGroup->flpGBT != nullptr) flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT);
                uint8_t cLinkId = cOpticalGroup->getId();
                LOG(INFO) << BOLDMAGENTA << "CIC start-up seqeunce for hybrids on link " << +cLinkId << RESET;
                for(auto cHybrid: *cOpticalGroup)
                {
                    OuterTrackerModule* theOuterTrackerModule = static_cast<OuterTrackerModule*>(cHybrid);
                    if(theOuterTrackerModule->fCic != NULL)
                    {
                        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->selectLink(cLinkId);
                        auto& cCic = theOuterTrackerModule->fCic;

                        // read CIC sparsification setting
                        bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
                        cBoard->setSparsification(cSparsified);

                        LOG(INFO) << BOLDBLUE << "Configuring CIC" << +(theOuterTrackerModule->getFeId() % 2) << " on link " << +theOuterTrackerModule->getLinkId() << " on hybrid "
                                  << +theOuterTrackerModule->getFeId() << RESET;
                        fCicInterface->ConfigureChip(cCic);

                        // CIC start-up
                        uint8_t cModeSelect = (static_cast<ReadoutChip*>(theOuterTrackerModule->at(0))->getFrontEndType() != FrontEndType::CBC3); // 0 --> CBC , 1 --> MPA
                        // select CIC mode
                        bool cSuccess = fCicInterface->SelectMode(cCic, cModeSelect);
                        if(!cSuccess)
                        {
                            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to configure CIC mode.." << RESET;
                            exit(0);
                        }
                        LOG(INFO) << BOLDMAGENTA << "CIC configured for " << ((cModeSelect == 0) ? "2S" : "PS") << " readout." << RESET;
                        // CIC start-up sequence
                        uint8_t cDriveStrength = 5;
                        cSuccess               = fCicInterface->StartUp(cCic, cDriveStrength);
                        fBeBoardInterface->ChipReSync(cBoard);
                        LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " performed start-up sequence on CIC" << +(theOuterTrackerModule->getId() % 2) << " connected to link "
                                  << +theOuterTrackerModule->getLinkId() << RESET;
                        LOG(INFO) << BOLDGREEN << "####################################################################################" << RESET;
                    }
                    // Configure readout-chips [CBCs, MPAs, SSAs]
                    for(auto cReadoutChip: *cHybrid)
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
        else
        {
            // ######################################
            // # Configuring Inner Tracker hardware #
            // ######################################
            LOG(INFO) << BOLDBLUE << "\t--> Found an Inner Tracker board" << RESET;
            LOG(INFO) << GREEN << "Configuring Board: " << RESET << BOLDYELLOW << +cBoard->getId() << RESET;
            fBeBoardInterface->ConfigureBoard(cBoard);

            // ###################
            // # Configuring FSM #
            // ###################
            size_t nTRIGxEvent = SystemController::findValueInSettings("nTRIGxEvent");
            size_t injType     = SystemController::findValueInSettings("INJtype");
            size_t nClkDelays  = SystemController::findValueInSettings("nClkDelays");
            size_t colStart    = SystemController::findValueInSettings("COLstart");
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->SetAndConfigureFastCommands(cBoard, nTRIGxEvent, injType, nClkDelays, colStart < RD53::LIN.colStart);
            LOG(INFO) << GREEN << "Configured FSM fast command block" << RESET;

            // ########################
            // # Configuring from XML #
            // ########################
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureFromXML(cBoard);

            // ###################
            // # Configure chips #
            // ###################
            for(auto cOpticalGroup: *cBoard)
            {
                if(cOpticalGroup->flpGBT != nullptr) flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT);
                for(auto cHybrid: *cOpticalGroup)
                {
                    LOG(INFO) << GREEN << "Initializing communication to Module: " << RESET << BOLDYELLOW << +cHybrid->getId() << RESET;
                    for(const auto cRD53: *cHybrid)
                    {
                        LOG(INFO) << GREEN << "Configuring RD53: " << RESET << BOLDYELLOW << +cRD53->getId() << RESET;
                        static_cast<RD53Interface*>(fReadoutChipInterface)->ConfigureChip(static_cast<RD53*>(cRD53));
                        LOG(INFO) << GREEN << "Number of masked pixels: " << RESET << BOLDYELLOW << static_cast<RD53*>(cRD53)->getNbMaskedPixels() << RESET;
                    }
                }
            }

            LOG(INFO) << GREEN << "Using " << BOLDYELLOW << RD53Shared::NTHREADS << RESET << GREEN << " threads for data decoding during running time" << RESET;
        }
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

void SystemController::Start(int currentRun)
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

void SystemController::ConfigureHardware(std::string cHWFile, bool enableStream)
{
    std::stringstream outp;

    InitializeHw(cHWFile, outp, true, enableStream);
    InitializeSettings(cHWFile, outp);
    std::cout << outp.str() << std::endl;
    outp.str("");
    ConfigureHw();
}

void SystemController::ConfigureCalibration() {}

void SystemController::Configure(std::string cHWFile, bool enableStream)
{
    ConfigureHardware(cHWFile, enableStream);
    ConfigureCalibration();
}

void SystemController::Start(BeBoard* pBoard) { fBeBoardInterface->Start(pBoard); }

void SystemController::Stop(BeBoard* pBoard) { fBeBoardInterface->Stop(pBoard); }
void SystemController::Pause(BeBoard* pBoard) { fBeBoardInterface->Pause(pBoard); }
void SystemController::Resume(BeBoard* pBoard) { fBeBoardInterface->Resume(pBoard); }

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
    // LOG (INFO) << BOLDGREEN << "TEST"<< fsm<< RESET;

    std::vector<uint32_t> cData;
    if(fsm and (pulses > 0))
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->Send_pulses(pulses);
    else
    {
        // LOG (INFO) << BOLDGREEN << "go "<< pulses<< RESET;

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
                    if(pBoard->getFrontEndType() == FrontEndType::MPA) static_cast<MPAInterface*>(fReadoutChipInterface)->ReadASEvent(cChip, cData);
                    if(pBoard->getFrontEndType() == FrontEndType::SSA) static_cast<SSAInterface*>(fReadoutChipInterface)->ReadASEvent(cChip, cData);
                }
            }
        }
    }

    this->DecodeData(pBoard, cData, 1, fBeBoardInterface->getBoardType(pBoard));
}

double SystemController::findValueInSettings(const std::string name, double defaultValue) const
{
    auto setting = fSettingsMap.find(name);
    return (setting != std::end(fSettingsMap) ? setting->second : defaultValue);
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
    if(pType == BoardType::RD53)
    {
        fEventList.clear();
        if(RD53FWInterface::decodedEvents.size() == 0) RD53FWInterface::DecodeEventsMultiThreads(pData, RD53FWInterface::decodedEvents);
        RD53FWInterface::Event::addBoardInfo2Events(pBoard, RD53FWInterface::decodedEvents);
        for(auto i = 0u; i < RD53FWInterface::decodedEvents.size(); i++) fEventList.push_back(&RD53FWInterface::decodedEvents[i]);
    }
    else if(pType == BoardType::D19C)
    {
        for(auto& pevt: fEventList) delete pevt;
        fEventList.clear();
        fCurrentEvent = 0;

        if(pNevents == 0) { LOG(INFO) << BOLDRED << "Asking to decode 0 events. . something might not be right here!!!" << RESET; }
        else
        {
            EventType fEventType = pBoard->getEventType();
            uint32_t  fNFe       = pBoard->getNFe();
            uint32_t  cBlockSize = 0x0000FFFF & pData.at(0);
            LOG(DEBUG) << BOLDBLUE << "Reading events from " << +fNFe << " FEs connected to uDTC...[ " << +cBlockSize * 4 << " 32 bit words to decode]" << RESET;
            fEventSize      = static_cast<uint32_t>((pData.size()) / pNevents);
            uint32_t maxind = 0;

            if(pBoard->getFrontEndType() == FrontEndType::SSA)
            {
                uint16_t nSSA = (fEventSize - D19C_EVENT_HEADER1_SIZE_32_SSA) / D19C_EVENT_SIZE_32_SSA / fNFe;
                if(fEventType == EventType::SSAAS) nSSA = pData.size() / 120;

                for(auto opticalGroup: *pBoard)
                {
                    for(auto hybrid: *opticalGroup)
                    {
                        for(auto chip: *hybrid)
                        {
                            // LOG (INFO) << BOLDBLUE <<chip->getId()+hybrid->getId()*nSSA <<RESET;
                            maxind = std::max(maxind, uint32_t(chip->getId() + hybrid->getId() * nSSA));
                        }
                    }
                }
                // LOG (INFO) << BOLDBLUE << "maxind " << maxind << RESET;
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
                            // LOG (INFO) << BOLDBLUE << "Decoding SSA data " << RESET;
                            // auto cL1Counter0 = (cEvent[4+2] & (0xF<<16)) >> 16;
                            // auto cL1Counter1 = (cEvent[4+8+4+2] & (0xF<<16)) >> 16;
                            // LOG (INFO) << BOLDBLUE << "L1A counter chip0 : " << cL1Counter0 << RESET;
                            // LOG (INFO) << BOLDBLUE << "L1A counter chip1 : " << cL1Counter1 << RESET;
                            // for(auto cWord : cEvent )
                            //   LOG (INFO) << BOLDMAGENTA << std::bitset<32>(cWord) << RESET;
                            fEventList.push_back(new D19cSSAEvent(pBoard, maxind + 1, fNFe, cEvent));
                        }
                        else if(pBoard->getFrontEndType() == FrontEndType::MPA)
                        {
                            fEventList.push_back(new D19cMPAEvent(pBoard, maxind + 1, fNFe, cEvent));
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
