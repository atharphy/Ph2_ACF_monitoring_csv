/*!
  \file                  SystemController.cc
  \brief                 Controller of the System, overall wrapper of the framework
  \author                Mauro DINARDO
  \version               2.0
  \date                  01/01/20
  Support:               email to mauro.dinardo@cern.ch
*/

#include "SystemController.h"
#include "../MonitorUtils/CBCMonitor.h"
#include "../MonitorUtils/DetectorMonitor.h"
#include "../MonitorUtils/RD53Monitor.h"
#include "../MonitorUtils/SEHMonitor.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
bool cBrokenPS = false;

namespace Ph2_System
{
SystemController::SystemController()
    : fBeBoardInterface(nullptr)
    , fReadoutChipInterface(nullptr)
    , flpGBTInterface(nullptr)
    , fCicInterface(nullptr)
    , fDetectorContainer(nullptr)
    , fSettingsMap()
    , fFileHandler(nullptr)
    , fRawFileName("")
    , fWriteHandlerEnabled(false)
    , fDQMStreamerEnabled(false)
    , fMonitorDQMStreamerEnabled(false)
    , fDQMStreamer(nullptr)
    , fMonitorDQMStreamer(nullptr)
    , fDetectorMonitor(nullptr)
    , fChannelGroupHandlerContainer(nullptr)
{
}

SystemController::~SystemController() {}

void SystemController::Inherit(const SystemController* pController)
{
    fBeBoardInterface             = pController->fBeBoardInterface;
    fReadoutChipInterface         = pController->fReadoutChipInterface;
    flpGBTInterface               = pController->flpGBTInterface;
    fBeBoardFWMap                 = pController->fBeBoardFWMap;
    fSettingsMap                  = pController->fSettingsMap;
    fFileHandler                  = pController->fFileHandler;
    fDQMStreamerEnabled           = pController->fDQMStreamerEnabled;
    fMonitorDQMStreamerEnabled    = pController->fMonitorDQMStreamerEnabled;
    fDQMStreamer                  = pController->fDQMStreamer;
    fMonitorDQMStreamer           = pController->fMonitorDQMStreamer;
    fDetectorContainer            = pController->fDetectorContainer;
    fCicInterface                 = pController->fCicInterface;
    fPowerSupplyClient            = pController->fPowerSupplyClient;
    fChannelGroupHandlerContainer = pController->fChannelGroupHandlerContainer;

#ifdef __TCP_SERVER__
    fTestcardClient = pController->fTestcardClient;
#endif
}

void SystemController::Destroy()
{
    for(const auto cBoard: *fDetectorContainer)
        if(cBoard->getBoardType() == BoardType::RD53)
        {
            try
            {
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->PrintErrorsLVDS(); // @TMP@
            }
            catch(...)
            {
            }
        }

    this->closeFileHandler();

    LOG(INFO) << BOLDRED << ">>> Destroying interfaces <<<" << RESET;

    RD53Event::JoinDecodingThreads();

    delete fDetectorMonitor;
    fDetectorMonitor = nullptr;

    delete fBeBoardInterface;
    fBeBoardInterface = nullptr;

    delete fReadoutChipInterface;
    fReadoutChipInterface = nullptr;

    delete flpGBTInterface;
    flpGBTInterface = nullptr;

    delete fDetectorContainer;
    fDetectorContainer = nullptr;

    delete fCicInterface;
    fCicInterface = nullptr;

    fBeBoardFWMap.clear();
    fSettingsMap.clear();

    delete fDQMStreamer;
    fDQMStreamer = nullptr;

    delete fMonitorDQMStreamer;
    fMonitorDQMStreamer = nullptr;

    delete fPowerSupplyClient;
    fPowerSupplyClient = nullptr;

    delete fChannelGroupHandlerContainer;
    fChannelGroupHandlerContainer = nullptr;

#ifdef __TCP_SERVER__
    delete fTestcardClient;
    fTestcardClient = nullptr;
#endif

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

void SystemController::InitializeHw(const std::string& pFilename, std::ostream& os, bool pIsFile, bool streamData, uint16_t DQMportNumber, uint16_t monitorDQMportNumber)
{
    fDQMStreamerEnabled        = streamData;
    fMonitorDQMStreamerEnabled = streamData;

    if(streamData == true)
    {
        fDQMStreamer = new TCPPublishServer(DQMportNumber, 1);
        fDQMStreamer->startAccept();

        fMonitorDQMStreamer = new TCPPublishServer(monitorDQMportNumber, 1);
        fMonitorDQMStreamer->startAccept();
    }

    fDetectorContainer = new DetectorContainer;
    this->fParser.parseHW(pFilename, fBeBoardFWMap, fDetectorContainer, os, pIsFile);
    fBeBoardInterface = new BeBoardInterface(fBeBoardFWMap);
    fBeBoardInterface->setBoard(0);

    fChannelGroupHandlerContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitChip<std::shared_ptr<ChannelGroupHandler>>(*fDetectorContainer, *fChannelGroupHandlerContainer);

    LOG(INFO) << BOLDYELLOW << "Trying to connect to the Power Supply Server..." << RESET;
    fPowerSupplyClient = new TCPClient("127.0.0.1", 7000);
    if(!fPowerSupplyClient->connect(1))
    {
        LOG(INFO) << BOLDYELLOW << "Cannot connect to the Power Supply Server, power supplies will need to be controlled manually" << RESET;
        delete fPowerSupplyClient;
        fPowerSupplyClient = nullptr;
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Connected to the Power Supply Server!" << RESET;
    }

#ifdef __TCP_SERVER__
    fTestcardClient = new TCPClient("127.0.0.1", 8000);
    if(!fTestcardClient->connect(1))
    {
        std::cerr << "Cannot connect to the Testcard Server" << '\n';
        delete fTestcardClient;
        fTestcardClient = nullptr;
    }
#endif

    if(fDetectorContainer->size() > 0)
    {
        const BeBoard* cFirstBoard = fDetectorContainer->at(0);
        if(cFirstBoard->getBoardType() != BoardType::RD53 && fInitializeInterfaces == 1)
        {
            LOG(INFO) << BOLDBLUE << "Initializing HwInterfaces for OT BeBoards.." << RESET;
            if(cFirstBoard->size() > 0) // # of optical groups connected to Board0
            {
                auto cFirstOpticalGroup = cFirstBoard->at(0);
                LOG(INFO) << BOLDBLUE << "\t...Initializing HwInterfaces for OpticalGroups.." << +cFirstBoard->size() << " optical group(s) found ..." << RESET;
                bool cWithLpGBT = (cFirstOpticalGroup->flpGBT != nullptr);
                if(cWithLpGBT)
                {
                    LOG(INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for lpGBT" << RESET;
                    flpGBTInterface = new D19clpGBTInterface(fBeBoardFWMap, cFirstBoard->isOptical(), cFirstBoard->ifUseCPB());
// check link to external interface
#ifdef __TCUSB__
#if defined(__SEH_USB__) || defined(__ROH_USB__)
                    if(flpGBTInterface->getExternalController() != nullptr)
                    {
                        LOG(INFO) << BOLDBLUE << "TC interface should be initialized... type is " << flpGBTInterface->getExternalController()->getName() << RESET;
#ifdef __ROH_USB__
                        // check reading of ADC from PSROH TC
                        float cOutput;
                        flpGBTInterface->getExternalController()->getInterface().adc_get(TC_PSROH::measurement::_1V25_REF, cOutput);
                        LOG(INFO) << BOLDBLUE << "Checking communication with test card by reading 1V25_Ref : " << cOutput << RESET;
#endif
                    }
#endif
#endif
                }

                LOG(INFO) << BOLDBLUE << "Found " << +cFirstOpticalGroup->size() << " hybrids in this group..." << RESET;
                if(cFirstOpticalGroup->size() > 0) // # of hybrids connected to OpticalGroup0
                {
                    LOG(INFO) << BOLDBLUE << "\t\t...Initializing HwInterfaces for FrontEnd Hybrids.." << +cFirstOpticalGroup->size() << " hybrid(s) found ..." << RESET;
                    auto cFirstHybrid = cFirstOpticalGroup->at(0);
                    auto cType        = FrontEndType::CBC3;
                    bool cWithCBC  = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType          = FrontEndType::SSA;
                    bool cWithSSA  = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType          = FrontEndType::SSA2;
                    bool cWithSSA2 = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType          = FrontEndType::MPA;
                    bool cWithMPA  = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType          = FrontEndType::MPA2;
                    bool cWithMPA2 = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    bool cMPAtype  = cWithMPA2 | cWithMPA;
                    bool cSSAtype  = cWithSSA2 | cWithSSA;

                    if(cWithCBC)
                    {
                        LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for CBC(s)" << RESET;
                        fReadoutChipInterface = new CbcInterface(fBeBoardFWMap);
                    }

                    if(cSSAtype && !cMPAtype) // SSA boards?
                    {
                        if(cWithSSA)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for SSA(s)" << RESET;
                            fReadoutChipInterface = new SSAInterface(fBeBoardFWMap);
                        }
                        if(cWithSSA2)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for SSA2(s)" << RESET;
                            fReadoutChipInterface = new SSA2Interface(fBeBoardFWMap);
                        }
                    }

                    if(cMPAtype && !cSSAtype) // MPA boards?
                    {
                        if(cWithMPA)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for MPA(s)" << RESET;
                            fReadoutChipInterface = new MPAInterface(fBeBoardFWMap);
                        }
                        if(cWithMPA2)
                        {
                            LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for MPA(s)" << RESET;
                            fReadoutChipInterface = new MPA2Interface(fBeBoardFWMap);
                        }
                    }
                    if((cMPAtype && cSSAtype) && cWithLpGBT)
                    {
                        LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for PS module(s)" << RESET;
                        fReadoutChipInterface = new PSInterface(fBeBoardFWMap);
                    }
                    if(fReadoutChipInterface != nullptr)
                    {
                        bool cFoundLpgbt = fReadoutChipInterface->lpGBTCheck(cFirstBoard);
                        if(cFoundLpgbt) LOG(INFO) << BOLDGREEN << "\t\t\t\t\t.. Readout chip interface aware of the lpGBT connected to this board ... " << RESET;
                        if(cWithMPA && cWithSSA) static_cast<PSInterface*>(fReadoutChipInterface)->SetOptical();
                    }
                } // creat ROC interfaces

                LOG(INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for CIC" << RESET;
                fCicInterface = new CicInterface(fBeBoardFWMap);
                // check event type
                bool cWithCBC3 = !(cFirstBoard->getEventType() == EventType::VR2S);
                fCicInterface->setWith8CBC3(cWithCBC3);
                if(cFirstOpticalGroup->flpGBT != nullptr)
                {
                    bool cFoundLpgbt = fCicInterface->lpGBTCheck(cFirstBoard);
                    if(cFoundLpgbt) LOG(INFO) << BOLDGREEN << "\t\t\t\t\t.. CIC interface aware of the lpGBT connected to this board ... " << RESET;
                }
                else
                    fCicInterface->setWithLpGBT(false);
            }
        }
        else
        {
            flpGBTInterface       = new RD53lpGBTInterface(fBeBoardFWMap);
            fReadoutChipInterface = new RD53Interface(fBeBoardFWMap);
        }
    } // if there is something to create an interface for

    if(fWriteHandlerEnabled == true) this->initializeWriteFileHandler();

    DetectorMonitorConfig theDetectorMonitorConfig;
    std::string           monitoringType = fParser.parseMonitor(pFilename, theDetectorMonitorConfig, os, pIsFile);

    if(monitoringType != "None")
    {
        if(monitoringType == "2S")
            fDetectorMonitor = new CBCMonitor(this, theDetectorMonitorConfig);
        else if(monitoringType == "RD53")
            fDetectorMonitor = new RD53Monitor(this, theDetectorMonitorConfig);
        else if(monitoringType == "2SSEH")
            fDetectorMonitor = new SEHMonitor(this, theDetectorMonitorConfig);
        else
        {
            LOG(ERROR) << BOLDRED << "Unrecognized monitor type, Aborting" << RESET;
            abort();
        }
        fDetectorMonitor->forkMonitor();
    }

    // ###################
    // # Set module type #
    // ###################
    for(const auto cBoard: *fDetectorContainer)
    {
        if(cBoard->getBoardType() != BoardType::D19C) continue;

        auto cConnectedFeTypes = cBoard->connectedFrontEndTypes();
        bool cMPAfound =
            (std::find_if(cConnectedFeTypes.begin(), cConnectedFeTypes.end(), [](FrontEndType x) { return x == FrontEndType::MPA || x == FrontEndType::MPA2; }) != cConnectedFeTypes.end());
        bool cSSAfound =
            (std::find_if(cConnectedFeTypes.begin(), cConnectedFeTypes.end(), [](FrontEndType x) { return x == FrontEndType::SSA || x == FrontEndType::SSA2; }) != cConnectedFeTypes.end());
        bool cCBCfound = (std::find_if(cConnectedFeTypes.begin(), cConnectedFeTypes.end(), [](FrontEndType x) { return x == FrontEndType::CBC3; }) != cConnectedFeTypes.end());
        for(auto cOpticalGroup: *cBoard)
        {
            bool cWithLpGBT    = (cOpticalGroup->flpGBT != nullptr);
            bool cWithPSmodule = (cMPAfound || cSSAfound) && cWithLpGBT;
            bool cWith2Smodule = cCBCfound && cWithLpGBT;
            bool cWithPSHybrid = (cSSAfound && !cWithLpGBT);
            bool cWith2SHybrid = (cCBCfound && !cWithLpGBT);

            if(cWithPSmodule) { cOpticalGroup->setFrontEndType(FrontEndType::OuterTrackerPS); }
            else if(cWith2Smodule)
            {
                cOpticalGroup->setFrontEndType(FrontEndType::OuterTracker2S);
            }
            else if(cWithPSHybrid)
            {
                LOG(INFO) << BOLDYELLOW << "HYBRIDPS" << RESET;
                cOpticalGroup->setFrontEndType(FrontEndType::HYBRIDPS);
            }
            else if(cWith2SHybrid)
            {
                cOpticalGroup->setFrontEndType(FrontEndType::HYBRID2S);
            }
            else if(cWithLpGBT && flpGBTInterface != nullptr)
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->setFrontEndType(cOpticalGroup->getFrontEndType());
            else
                LOG(INFO) << BOLDMAGENTA << "UN-KNOWN MODULE TYPE" << RESET;
        }
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
                    fBeBoardInterface->ReadHybridVoltageMonitor(fReadoutChipInterface, cChip);
                    fBeBoardInterface->ReadHybridTemperatureMonitor(fReadoutChipInterface, cChip);
                    fBeBoardInterface->ReadChipMonitor(fReadoutChipInterface, cChip, args);
                    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
                }
}

// ######################################
// # Configuring Inner Tracker hardware #
// ######################################
void SystemController::ConfigureIT(BeBoard* pBoard)
{
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
    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->SetAndConfigureFastCommands(pBoard, nTRIGxEvent, injType, injLatency, nClkDelays, colStart < RD53::LIN.colStart);
    LOG(INFO) << CYAN << "================== Done ==================" << RESET;

    // ########################
    // # Configuring from XML #
    // ########################
    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->ConfigureFromXML(pBoard);

    // ########################
    // # Configure LpGBT chip #
    // ########################
    for(auto cOpticalGroup: *pBoard)
    {
        if(cOpticalGroup->flpGBT != nullptr)
        {
            LOG(INFO) << GREEN << "Initializing communication to Low-power Gigabit Transceiver (LpGBT): " << BOLDYELLOW << +cOpticalGroup->getId() << RESET;

            if(flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT) == true)
            {
                static_cast<RD53lpGBTInterface*>(flpGBTInterface)->InternalPhaseAlignRx(cOpticalGroup->flpGBT, pBoard, cOpticalGroup, fReadoutChipInterface);
                LOG(INFO) << BOLDBLUE << ">>> LpGBT chip configured <<<" << RESET;
            }
            else
                LOG(ERROR) << BOLDRED << ">>> LpGBT chip not configured, reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ") <<<" << RESET;
        }
    }

    // #######################
    // # Status optical link #
    // #######################
    uint32_t txStatus, rxStatus, mgtStatus;
    LOG(INFO) << GREEN << "Checking status of the optical links:" << RESET;
    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->StatusOptoLink(txStatus, rxStatus, mgtStatus);

    // ######################################################
    // # Configure down and up links to/from frontend chips #
    // ######################################################
    LOG(INFO) << CYAN << "=== Configuring frontend chip communication ===" << RESET;
    static_cast<RD53Interface*>(fReadoutChipInterface)->InitRD53Downlink(pBoard);
    for(auto cOpticalGroup: *pBoard)
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
    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->CheckChipCommunication(pBoard);

    // ############################
    // # Configure frontend chips #
    // ############################
    LOG(INFO) << CYAN << "===== Configuring frontend chip registers =====" << RESET;
    for(auto cOpticalGroup: *pBoard)
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

    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->PrintFrequencyLVDS(); // @TMP@

    LOG(INFO) << GREEN << "Using " << BOLDYELLOW << RD53Shared::NTHREADS << RESET << GREEN << " threads for data decoding during running time" << RESET;
    RD53Event::ForkDecodingThreads();
}

// ######################################
// # Configuring Outer Tracker hardware #
// ######################################
void SystemController::InitializeOT(BeBoard* pBoard)
{
    LOG(INFO) << BOLDMAGENTA << "Initializing OT hardware.." << RESET;

    for(auto cOpticalGroup: *pBoard)
    {
        if(cOpticalGroup->flpGBT == nullptr) continue;

        LOG(INFO) << BOLDBLUE << "Now going to configuring lpGBTs#" << +cOpticalGroup->getId() << " on Board " << int(pBoard->getId()) << RESET;
        D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
        if(cOpticalGroup->getReset() == 0)
        {
            LOG(INFO) << BOLDYELLOW << "Will not re-configure lpGBT on Link#" << +cOpticalGroup->getId() << RESET;
            continue;
        }

        if(!clpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT))
        {
            LOG(INFO) << BOLDRED << "SOMETHING FUNNY" << RESET;
            continue;
        }
    }

    // module start-up
    // depends on module type
    for(auto cOpticalGroup: *pBoard)
    {
        if(cOpticalGroup->getReset() == 0)
        {
            LOG(INFO) << BOLDYELLOW << "Will not re-configure lpGBT for specific module type.." << RESET;
            continue;
        }

        if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
        {
            LOG(INFO) << BOLDMAGENTA << "Configuring an OuterTracker2S module " << RESET;
            ModuleStartUp2S(cOpticalGroup);
        }
        if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
        {
            LOG(INFO) << BOLDMAGENTA << "Configuring an OuterTrackerPS module " << RESET;
            ModuleStartUpPS(cOpticalGroup);
        }
    }

    // CIC reset
    for(auto cOpticalGroup: *pBoard)
    {
        auto& clpGBT = cOpticalGroup->flpGBT;
        if(clpGBT == nullptr) continue;

        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic == NULL) continue;
            uint8_t cSide = cHybrid->getId() % 2;
            if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S) { static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide); }
            else if(!cBrokenPS)
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
            }
            else
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, 0);
            }
        }
    }

    // CIC start-up
    for(auto cOpticalGroup: *pBoard)
    {
        // CIC configuration part .. first configure
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic == NULL) continue;

            LOG(INFO) << BOLDBLUE << "Configuring CIC" << +(cHybrid->getId() % 2) << " on link " << +cHybrid->getOpticalId() << " on hybrid " << +cHybrid->getId() << RESET;
            fCicInterface->ConfigureChip(cCic);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false); // make sure all FEs are disabled by default
        }
        bool cSuccess = CicStartUp(cOpticalGroup, true);
        if(!cSuccess)
        {
            LOG(INFO) << BOLDRED << "Failed start-up sequence on OG" << +cOpticalGroup->getId() << RESET;
            throw std::runtime_error(std::string("FAILED to start-up CIC... something is wrong... .. STOPPING"));
        }
    }

    // check if a resync is needed
    LOG(INFO) << BOLDBLUE << "Checking if a ReSync is needed for Board" << +pBoard->getId() << RESET;
    bool cReSyncNeeded = false;
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic == NULL) continue;
            if(cReSyncNeeded) continue;

            bool cReSync = fCicInterface->GetResyncRequest(cCic);
            if(cReSync) LOG(INFO) << BOLDBLUE << "\t... CIC" << +cHybrid->getId() << " requires a ReSync" << RESET;
            cReSyncNeeded = cReSync;
        }
    }

    if(cReSyncNeeded)
    {
        LOG(INFO) << BOLDMAGENTA << "Sending a ReSync at the end of the OT-module configuration step" << RESET;
        // send a ReSync to all chips before starting
        fBeBoardInterface->ChipReSync(pBoard);
        // check resync request has been cleared
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                if(cCic == NULL) continue;

                if(fCicInterface->GetResyncRequest(cCic))
                {
                    LOG(INFO) << BOLDRED << "ReSync request ofrom CIC" << +cHybrid->getId() << RESET;
                    throw std::runtime_error(std::string("FAILED to clear CIC ReSync request"));
                }
            }
        }
    }
    else
        LOG(INFO) << BOLDMAGENTA << "No ReSync needed after OT-module configuration step" << RESET;
}

void SystemController::ConfigureOT(BeBoard* pBoard)
{
    // hard reset ROCs on hybrid if lpGBT is there; if no lpGBT this
    // is already taken care of by ConfigureBoard
    for(auto cOpticalGroup: *pBoard)
    {
        auto& clpGBT = cOpticalGroup->flpGBT;
        for(auto cHybrid: *cOpticalGroup)
        {
            if(clpGBT == nullptr) continue;

            uint8_t cSide = cHybrid->getId() % 2;
            LOG(DEBUG) << BOLDBLUE << "Configuring ReadoutOutChips on Hybrid" << +cHybrid->getId() << RESET;

            if(cHybrid->getReset() == 0)
            {
                LOG(INFO) << BOLDYELLOW << "Will not send a hard-reset to ROCs on Hybrid#" << +cHybrid->getId() << RESET;
                continue;
            }
            // cWithLpGBT=true;
            // no SSA because I don't want to reset it here. . already done earlier
            if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
            {
                LOG(DEBUG) << BOLDBLUE << "\t... Applying hard reset to MPAs" << RESET;
                if(!cBrokenPS) { static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, cSide); }
                else
                {
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, 1);
                }
            }
            if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
            {
                LOG(DEBUG) << BOLDBLUE << "\t... Applying hard reset to CBCs" << RESET;
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cSide);
            }
        } // hybrid
    }     // OG

    // configure chips
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid) { fReadoutChipInterface->ConfigureChip(cChip); } // ROC config
        }                                                                              // hybrid
    }                                                                                  // OG
    LOG(INFO) << BOLDMAGENTA << "Configured OT module" << RESET;
}

void SystemController::ModuleStartUpPS(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    LOG(INFO) << BOLDBLUE << "SystemController::ModuleStartUpPS for BeBoard#" << +(*cBoardIter)->getId() << " OpticalGroup#" << +pOpticalGroup->getId() << RESET;

    auto& clpGBT = pOpticalGroup->flpGBT;
    // configure PS ROHs
    if(clpGBT != nullptr)
    {
        const uint8_t cSsaClockDrive = 7;
        const uint8_t cCicClockDrive = 7;

        static_cast<D19clpGBTInterface*>(flpGBTInterface)->ConfigurePSROH(clpGBT);
        const std::vector<uint8_t> cGroupsExamples = {0, 1};
        for(auto cHybrid: *pOpticalGroup)
        {
            // first .. send clock to the SSAs on this hybrid
            uint8_t  cSide        = cHybrid->getId() % 2;
            uint16_t cReadoutRate = static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetRxDataRate(clpGBT, cGroupsExamples[cSide]);
            LOG(INFO) << BOLDMAGENTA << "Readout rate on PS-module (Hybrid# " << +cHybrid->getId() << ") is " << +cReadoutRate << " Mbps" << RESET;

            lpGBTClockConfig cClkCnfg;
            cClkCnfg.fClkFreq         = (cReadoutRate == 320) ? 4 : 5;
            cClkCnfg.fClkDriveStr     = cSsaClockDrive;
            cClkCnfg.fClkInvert       = 1;
            cClkCnfg.fClkPreEmphWidth = 0;
            cClkCnfg.fClkPreEmphMode  = 0; // 3;
            cClkCnfg.fClkPreEmphStr   = 0; // 7;

            LOG(INFO) << BOLDBLUE << "Enabling SSA clock [Side == " << +cSide << "]" << RESET;
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);

            // enable clock to CIC
            cClkCnfg.fClkFreq     = (cReadoutRate == 320) ? 4 : 5;
            cClkCnfg.fClkInvert   = 0;
            cClkCnfg.fClkDriveStr = cCicClockDrive;
            LOG(INFO) << BOLDBLUE << "Enabling CIC clock [Side == " << +cSide << "]" << RESET;
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicClock(clpGBT, cClkCnfg, cSide);

            // hold resets
            if(!cBrokenPS)
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->ssaReset(clpGBT, true, cSide);
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->mpaReset(clpGBT, true, cSide);
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicReset(clpGBT, true, cSide);
            }
            else
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->mpaReset(clpGBT, true, 1);
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicReset(clpGBT, true, 0);
            }

            // make sure all SSAs on a module are configured to produce a clock
            // regardless of how many are enabled on this hybrid
            LOG(INFO) << BOLDBLUE << "Resetting SSA" << RESET;
            if(!cBrokenPS) { static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetSSA(clpGBT, cSide); }
            else
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicReset(clpGBT, 0);
            }

            bool     cSkipSSA3            = true; // eventually this needs to be set in the xml somewhere
            for(uint8_t cSSAId = 0; cSSAId < 8; cSSAId++)
            {
                if(cSkipSSA3 && cSSAId == 3) continue;

                SSA*    cSSA          = new SSA(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getId(), cSSAId, 0, 0, "./settings/SSAFiles/SSA.txt");
                uint8_t cSLVSdriveSSA = cSSA->getReg("SLVS_pad_current");
                cSSA->setOpticalId(cHybrid->getOpticalId());
                cSSA->setOptical(cHybrid->isOptical());
                cSSA->setMasterId(cHybrid->getMasterId());
                LOG(INFO) << BOLDMAGENTA << "SSA " << +cSSAId << " current set to " << +cSLVSdriveSSA << "" << RESET;
                auto cRegItem = cSSA->getRegItem("SLVS_pad_current");
                (fBeBoardInterface->getFirmwareInterface())->SingleRegisterWrite(cSSA, cRegItem, false);
            }

        } // hybrid
    }     // lpGBT part ... resets + clocks
}

void SystemController::ModuleStartUp2S(const OpticalGroup* pOpticalGroup)
{
    auto cBoardId   = pOpticalGroup->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    LOG(INFO) << BOLDBLUE << "SystemController::ModuleStartUp2S for BeBoard#" << +(*cBoardIter)->getId() << " OpticalGroup#" << +pOpticalGroup->getId() << RESET;

    // configure 2S SEHs
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT != nullptr)
    {
        uint8_t                    cHybridClockDrive = 7;
        uint8_t                    cPreEmphMode      = 0; // 3
        uint8_t                    cPreEmphStr       = 0; // 7
        const std::vector<uint8_t> cGroupsExamples   = {0, 1};
        static_cast<D19clpGBTInterface*>(flpGBTInterface)->Configure2SSEH(clpGBT);
        for(auto cHybrid: *pOpticalGroup)
        {
            uint8_t cSide = cHybrid->getId() % 2;
            // work out readout rate
            uint16_t cReadoutRate = flpGBTInterface->GetRxDataRate(clpGBT, cGroupsExamples[cSide]);
            LOG(INFO) << BOLDMAGENTA << "Readout rate on 2S-module (Hybrid# " << +cHybrid->getId() << ") is " << +cReadoutRate << " Mbps" << RESET;

            // first .. send clock to the CBCs on this hybrid
            lpGBTClockConfig cClkCnfg;
            cClkCnfg.fClkFreq         = 4;
            cClkCnfg.fClkDriveStr     = cHybridClockDrive;
            cClkCnfg.fClkInvert       = (cSide == 0) ? 1 : 0;
            cClkCnfg.fClkPreEmphWidth = 0;
            cClkCnfg.fClkPreEmphMode  = cPreEmphMode;
            cClkCnfg.fClkPreEmphStr   = cPreEmphStr;
            LOG(INFO) << BOLDBLUE << "Enabling Hybrid clock [Side == " << +cSide << "]" << RESET;
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->hybridClock(clpGBT, cClkCnfg, cSide);

            // hold CBC reset
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->cbcReset(clpGBT, true, cSide);
            // auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            // if(cCic == NULL) continue;
            // // Configure CICs on this hybrid
            // // release CIC reset
            // LOG(INFO) << BOLDBLUE << "Resetting CIC" << RESET;
            // static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
        }
    } // lpGBT part ... resets + clocks
}

bool SystemController::CicStartUp(const OpticalGroup* pOpticalGroup, bool cStartUpSequence)
{
    auto cBoardId    = pOpticalGroup->getBeBoardId();
    auto cBoardIter  = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    bool cWith2SFEH  = (*cBoardIter)->getEventType() == EventType::VR2S;
    auto cSparsified = (*cBoardIter)->getSparsification();

    auto& clpGBT   = pOpticalGroup->flpGBT;
    bool  cSuccess = true;
    LOG(INFO) << BOLDGREEN << "####################################################################################" << RESET;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == NULL) continue;

        LOG(INFO) << BOLDMAGENTA << "SystemController::CicStartUp for OpticalGroup#" << +pOpticalGroup->getId() << " CIC#" << +cCic->getHybridId() << RESET;

        // if there is an lpGBT .
        // its configuration overwrites whatever is in the xml
        if(clpGBT != nullptr)
        {
            auto     cChipRate     = static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(clpGBT);
            uint16_t cClkFrequency = (cChipRate == 5) ? 320 : 640;
            cCic->setClockFrequency(cClkFrequency);
        }

        // CIC start-up
        auto cType       = FrontEndType::CBC3;
        auto cHybridIter = std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; });
        bool cIs2S       = cHybridIter != cHybrid->end();
        // 0 --> CBC , 1 --> MPA
        uint8_t cModeSelect = (cIs2S) ? 0 : 1;
        uint8_t cBx0Delay   = (cIs2S) ? 8 : 22;
        // select CIC mode
        cSuccess = fCicInterface->SelectMode(cCic, cModeSelect);
        if(!cSuccess)
        {
            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to configure CIC mode.." << RESET;
            throw std::runtime_error(std::string("FAILED to set CIC mode ... something is wrong... .. STOPPING"));
        }
        LOG(INFO) << BOLDMAGENTA << "CIC configured for " << (cIs2S ? "2S" : "PS") << " readout." << RESET;

        // configure CIC FE enable register
        // first make sure it is set to 0x00
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        // figure out which ROCs are enabled
        std::vector<uint8_t> cFeIds(0);
        for(auto cReadoutChip: *cHybrid)
        {
            // only consider MPAs and CBCs
            if(cReadoutChip->getFrontEndType() == FrontEndType::SSA || cReadoutChip->getFrontEndType() == FrontEndType::SSA2) continue;
            cFeIds.push_back(cReadoutChip->getId() % 8);
        }
        fCicInterface->EnableFEs(cCic, cFeIds, true);

        // make sure data rate is correctly configured
        // only works for CIC2
        if(cCic->getFrontEndType() == FrontEndType::CIC2)
        {
            uint8_t cFeConfigReg  = fCicInterface->ReadChipReg(cCic, "FE_CONFIG");
            auto    cClkFrequency = cCic->getClockFrequency();
            uint8_t cNewValue     = (cFeConfigReg & 0xFD) | ((uint8_t)(cClkFrequency == 640) << 1);
            cSuccess              = fCicInterface->WriteChipReg(cCic, "FE_CONFIG", cNewValue);
        }

        // 2S-FEHs
        // CIC start-up sequence
        uint8_t cClkTerm = 1;
        uint8_t cRxTerm  = 1;
        if(cWith2SFEH)
        {
            cClkTerm = 0;
            cRxTerm  = 1;
        }
        cSuccess = fCicInterface->ConfigureTermination(cCic, cClkTerm, cRxTerm);
        if(cSuccess)
        {
            if(cStartUpSequence)
            {
                LOG(INFO) << BOLDYELLOW << "Launching CIC start-up sequence.." << RESET;
                cSuccess = fCicInterface->StartUp(cCic, cCic->getDriveStrength(), cCic->getEdgeSelect());
            }
            else
            {
                LOG(INFO) << BOLDYELLOW << "Not launching CIC start-up sequence.. but will configure drive strength and FCMD edge from xml.." << RESET;
                if(fCicInterface->ConfigureDriveStrength(cCic, cCic->getDriveStrength()))
                    cSuccess = fCicInterface->ConfigureFCMDEdge(cCic, cCic->getEdgeSelect());
                else
                    cSuccess = false;
            }
        }
        else
            throw std::runtime_error(std::string("FAILED to start-up CIC ... something is wrong... .. STOPPING"));

        if(cSuccess)
            cSuccess = fCicInterface->SetSparsification(cCic, cSparsified);
        else
            throw std::runtime_error(std::string("FAILED to set CIC sparsification... .. STOPPING"));

        if(cSuccess)
            cSuccess = fCicInterface->ConfigureStubOutput(cCic);
        else
            throw std::runtime_error(std::string("FAILED to configure CIC stub output... .. STOPPING"));

        if(cSuccess)
            cSuccess = fCicInterface->ManualBx0Alignment(cCic, cBx0Delay);
        else
            throw std::runtime_error(std::string("FAILED to configure CIC Bx0 delay... .. STOPPING"));

    } // all hybrids connected to this OG
    LOG(INFO) << BOLDGREEN << "####################################################################################" << RESET;
    return cSuccess;
}

void SystemController::ConfigureHw(bool bIgnoreI2c, bool pReInitialize)
{
    if(fDetectorContainer == nullptr)
    {
        LOG(ERROR) << BOLDRED << "Hardware not initialized: run SystemController::InitializeHw first" << RESET;
        return;
    }

    LOG(INFO) << BOLDMAGENTA << "@@@ Configuring HW parsed from xml file @@@" << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        cBoard->printBoardType();
        fBeBoardInterface->setBoard(0);
        fBeBoardInterface->ConfigureBoard(cBoard);
        if(cBoard->getBoardType() == BoardType::D19C)
        {
            // set board sparisificatio
            // based on what is configured in the fw register
            // read CIC sparsification setting from fW register
            // make sure board is also set to the same thing
            bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
            cBoard->setSparsification(cSparsified);
            if(pReInitialize)
                InitializeOT(cBoard);
            else // lpGBT + CIC will need to be configured  (and also maybe reset)
            {
                // lpGBT config
                for(auto cOpticalGroup: *cBoard)
                {
                    if(cOpticalGroup->flpGBT == nullptr) continue;

                    LOG(INFO) << BOLDBLUE << "Now going to configuring lpGBTs#" << +cOpticalGroup->getId() << " on Board " << +cBoard->getId() << RESET;
                    D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
                    if(cOpticalGroup->getReset() == 0)
                    {
                        LOG(INFO) << BOLDYELLOW << "Will not re-configure lpGBT on Link#" << +cOpticalGroup->getId() << RESET;
                        continue;
                    }

                    if(!clpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT))
                    {
                        LOG(INFO) << BOLDRED << "SOMETHING FUNNY" << RESET;
                        continue;
                    }
                }

                // CIC hard reset
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& clpGBT = cOpticalGroup->flpGBT;
                    if(clpGBT == nullptr) continue;

                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                        if(cCic == NULL) continue;
                        uint8_t cSide = cHybrid->getId() % 2;
                        if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S) { static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide); }
                        else if(!cBrokenPS)
                        {
                            static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
                        }
                        else
                        {
                            static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, 0);
                        }
                    }
                }

                // CIC configure
                for(auto cOpticalGroup: *cBoard)
                {
                    // auto& clpGBT = cOpticalGroup->flpGBT;
                    // if(clpGBT == nullptr) continue;
                    // CIC configuration part .. first configure
                    LOG(INFO) << BOLDYELLOW << "Configuring CIC connected to OG#" << +cOpticalGroup->getId() << RESET;
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                        if(cCic == NULL) continue;

                        LOG(INFO) << BOLDBLUE << "Configuring CIC" << +(cHybrid->getId() % 2) << " on link " << +cHybrid->getOpticalId() << " on hybrid " << +cHybrid->getId() << RESET;
                        fCicInterface->ConfigureChip(cCic);
                        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false); // make sure all FEs are disabled by default
                    }
                    bool cSuccess = CicStartUp(cOpticalGroup, false);
                    if(!cSuccess)
                    {
                        LOG(INFO) << BOLDRED << "Failed start-up sequence on OG" << +cOpticalGroup->getId() << RESET;
                        throw std::runtime_error(std::string("FAILED to start-up CIC... something is wrong... .. STOPPING"));
                    }
                }
            }
            if(!cBoard->isOptical() && cBoard->at(0)->flpGBT != nullptr)
            {
                LOG(INFO) << YELLOW << "Checking LinkLock after USB configuration of lpGBT" << RESET;
                LOG(INFO) << BOLDMAGENTA << "Resetting lpGBT-FPGA core on BeBoard#" << +cBoard->getId() << RESET;
                // reset lpGBT core
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->WriteReg("fc7_daq_ctrl.optical_block.general", 0x1);
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->WriteReg("fc7_daq_ctrl.optical_block.general", 0x0);
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                bool cLinkLock = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->LinkLock(cBoard);
                if(!cLinkLock)
                {
                    LOG(INFO) << BOLDRED << "lpGBT link failed to LOCK!" << RESET;
                    exit(0);
                }
            }
            ConfigureOT(cBoard);

            LOG(INFO) << CYAN << "==================== Done =====================" << RESET;
        }
        else if(cBoard->getBoardType() == BoardType::RD53)
            ConfigureIT(cBoard);
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

void SystemController::Configure(std::string cHWFile, bool enableStream, uint16_t DQMportNumber)
{
    std::stringstream outp;

    InitializeHw(cHWFile, outp, true, enableStream, DQMportNumber);
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
    if(cNPackets == 0) return cNPackets;

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
    else if(pType == BoardType::D19C && pBoard->getEventType() != EventType::PSAS)
    {
        bool cTLUconfig = 2;
        // bool cTLUconfig = (fBeBoardInterface->ReadBoardReg(fDetectorContainer->at(pBoard->getIndex()), "fc7_daq_cnfg.tlu_block.handshake_mode") == 2 &&
        //                    fBeBoardInterface->ReadBoardReg(fDetectorContainer->at(pBoard->getIndex()), "fc7_daq_cnfg.tlu_block.tlu_enabled") == 1);
        // for (auto L : pData) LOG(INFO) << BOLDBLUE << std::bitset<32>(L) << RESET;
        for(auto& pevt: fEventList) delete pevt;
        fEventList.clear();

        if(pNevents == 0)
        {
            // LOG(INFO) << BOLDRED << "Asking to decode 0 events. . something might not be right here!!!" << RESET;
        }
        else
        {
            EventType fEventType = pBoard->getEventType();
            uint32_t  fNFe       = pBoard->getNFe();
            // uint32_t  cBlockSize = 0x0000FFFF & pData.at(0);
            // LOG(INFO) << BOLDBLUE << "Reading events from " << +fNFe << " FEs connected to uDTC...[ " << +cBlockSize * 4 << " 32 bit words to decode]" << RESET;
            fEventSize = static_cast<uint32_t>((pData.size()) / pNevents);
            // uint32_t nmpa = 0;
            uint32_t maxind = 0;
            for(auto opticalGroup: *pBoard)
            {
                for(auto hybrid: *opticalGroup) { maxind = std::max(maxind, uint32_t(hybrid->size())); }
            }

            if(fEventType != EventType::ZS)
            {
                // check data words because I'm desperate
                // for( auto cWord : pData )
                //     LOG (INFO) << BOLDYELLOW << "SystemController \t..." << std::bitset<32>(cWord) << RESET;

                size_t cEventIndex    = 0;
                auto   cEventIterator = pData.begin();
                do
                {
                    uint32_t cHeader = (0xFFFF0000 & (*cEventIterator)) >> 16;
                    if(cHeader != 0xFFFF)
                    {
                        int cPositionInData = (int)std::distance(pData.begin(), cEventIterator);
                        // first part of data header
                        if(fFileHandler != nullptr && cPositionInData > 12)
                            LOG(INFO) << BOLDRED << "SystemController::DecodeData Invalid header from the FW in position#" << cPositionInData << RESET;
                        else if(fFileHandler == nullptr)
                            LOG(INFO) << BOLDRED << "SystemController::DecodeData Invalid header from the FW in position#" << cPositionInData << RESET;
                        cEventIterator++;
                    }
                    else // valid event  // decode
                    {
                        uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
                        // LOG (INFO) << BOLDMAGENTA << "SystemController::DecodeData Decoding event made of up " << +cEventSize << " 32 bit words. " << RESET;
                        auto cEnd = ((cEventIterator + cEventSize) > pData.end()) ? pData.end() : (cEventIterator + cEventSize);
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
                                bool cWithCBC3 = !(fEventType == EventType::VR2S);
                                if(cWithCBC3)
                                    LOG(DEBUG) << BOLDBLUE << "Decoding CIC data : with 8CBC3 " << RESET;
                                else
                                    LOG(DEBUG) << BOLDBLUE << "Decoding CIC data : with 2S-FEH  " << RESET;
                                fEventList.push_back(new D19cCic2Event(pBoard, cEvent, cWithCBC3, cTLUconfig));
                            }
                            else if(pBoard->getFrontEndType() == FrontEndType::SSA)
                            {
                                fEventList.push_back(new D19cSSAEvent(pBoard, maxind, fNFe, cEvent));
                            }
                            else if(pBoard->getFrontEndType() == FrontEndType::SSA2)
                            {
                                fEventList.push_back(new D19cSSA2Event(pBoard, maxind, fNFe, cEvent));
                            }
                            else if(pBoard->getFrontEndType() == FrontEndType::MPA)
                            {
                                fEventList.push_back(new D19cMPAEvent(pBoard, maxind, fNFe, cEvent));
                                LOG(INFO) << BOLDBLUE << "Decoding SSA data " << RESET;
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
                                LOG(INFO) << BOLDBLUE << "Decoding MPA data " << RESET;
                                // fEventList.push_back(new D19cCic2Event(pBoard, cEvent));
                                fEventList.push_back(new D19cMPAEvent(pBoard, maxind + 1, fNFe, cEvent));
                            }
                            cEventIndex++;
                        }
                        cEventIterator += cEventSize;
                    }
                } while(cEventIterator < pData.end());
            }
        } // end zero check
    }
    else if(pType == BoardType::D19C && pBoard->getEventType() == EventType::PSAS)
    {
        fEventList.clear();
        fEventList.push_back(new D19cPSEventAS(pBoard, pData));
    }
}

void SystemController::setChannelGroupHandler(ChannelGroupHandler& theChannelGroupHandler, std::vector<FrontEndType> cFrontEndTypes)
{
    auto selectChipFlavourFunction = [cFrontEndTypes](const ChipContainer* theChip) {
        return (std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), static_cast<const ReadoutChip*>(theChip)->getFrontEndType()) != cFrontEndTypes.end());
    };
    setChannelGroupHandler(theChannelGroupHandler, selectChipFlavourFunction);
}

void SystemController::setChannelGroupHandler(ChannelGroupHandler& theChannelGroupHandler, FrontEndType theFrontEndType)
{
    auto selectChipFlavourFunction = [theFrontEndType](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == theFrontEndType); };
    setChannelGroupHandler(theChannelGroupHandler, selectChipFlavourFunction);
}

void SystemController::setChannelGroupHandler(ChannelGroupHandler& theChannelGroupHandler, std::function<bool(const ChipContainer*)> theQueryFunction)
{
    auto theChannelGroupHandlerPointer = std::make_shared<ChannelGroupHandler>(std::move(theChannelGroupHandler));
    setChannelGroupHandler(theChannelGroupHandlerPointer, theQueryFunction);
}

void SystemController::setChannelGroupHandler(std::shared_ptr<ChannelGroupHandler> theChannelGroupHandlerPointer, std::function<bool(const ChipContainer*)> theQueryFunction)
{
    uint16_t totalNumberOfChips = 0;
    for(const auto board: *fDetectorContainer)
    {
        for(const auto opticalGroup: *board)
        {
            for(const auto hybrid: *opticalGroup) { totalNumberOfChips += hybrid->size(); }
        }
    }

    uint16_t totalNumberOfQueriedChips = 0;
    fDetectorContainer->setReadoutChipQueryFunction(theQueryFunction);
    for(const auto board: *fDetectorContainer)
    {
        for(const auto opticalGroup: *board)
        {
            for(const auto hybrid: *opticalGroup)
            {
                totalNumberOfQueriedChips += hybrid->size();
                for(const auto chip: *hybrid)
                {
                    fChannelGroupHandlerContainer->getObject(board->getId())
                        ->getObject(opticalGroup->getId())
                        ->getObject(hybrid->getId())
                        ->getObject(chip->getId())
                        ->getSummary<std::shared_ptr<ChannelGroupHandler>>() = theChannelGroupHandlerPointer;
                }
            }
        }
    }

    fDetectorContainer->resetReadoutChipQueryFunction();
    fSameChannelGroupForAllChannels = (totalNumberOfQueriedChips == totalNumberOfChips);
}

void SystemController::setChannelGroupHandler(std::shared_ptr<ChannelGroupHandler> theChannelGroupHandlerPointer, uint16_t boardId, uint16_t opticalGroupId, uint16_t hybridId, uint16_t chipId)
{
    fChannelGroupHandlerContainer->getObject(boardId)->getObject(opticalGroupId)->getObject(hybridId)->getObject(chipId)->getSummary<std::shared_ptr<ChannelGroupHandler>>() =
        theChannelGroupHandlerPointer;
}

void SystemController::disableAllChannels()
{
    // ###########################################
    // # Disable channels of the entire detector #
    // ###########################################
    if(SystemController::findValueInSettings<double>("DisableChannelsAtExit", false) == true)
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid) fReadoutChipInterface->MaskAllChannels(cChip, true);
}

} // namespace Ph2_System
