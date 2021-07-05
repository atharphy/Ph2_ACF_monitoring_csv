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

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
bool cBrokenPS=true;
            
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

void SystemController::InitializeHw(const std::string& pFilename, std::ostream& os, bool pIsFile, bool streamData, uint16_t DQMportNumber)
{
    fStreamerEnabled = streamData;
    if(streamData == true)
    {
        fNetworkStreamer = new TCPPublishServer(DQMportNumber, 1);
        fNetworkStreamer->startAccept();
    }

    fDetectorContainer = new DetectorContainer;
    this->fParser.parseHW(pFilename, fBeBoardFWMap, fDetectorContainer, os, pIsFile);
    fBeBoardInterface = new BeBoardInterface(fBeBoardFWMap);

    /*
    fPowerSupplyClient = new TCPClient("192.168.122.123", 7000);
    if(!fPowerSupplyClient->connect(1))
    {
        delete fPowerSupplyClient;
        fPowerSupplyClient = nullptr;
    }
    for(const auto board: *fDetectorContainer) fBeBoardInterface->setPowerSupplyClient(board, fPowerSupplyClient);
    */

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
                bool cWithLpGBT = (cFirstOpticalGroup->flpGBT != nullptr);
                if(cWithLpGBT)
                {
                    LOG(INFO) << BOLDBLUE << "\t\t\t.. Initializing HwInterface for lpGBT" << RESET;
                    flpGBTInterface = new D19clpGBTInterface(fBeBoardFWMap, cFirstBoard->isOptical(), cFirstBoard->ifUseCPB());
// link to external interface
#ifdef __TCUSB__
                    flpGBTInterface->LinkExternalInterface<TestCardInterface>(fTCInterface);
#endif
                }

                LOG(INFO) << BOLDBLUE << "Found " << +cFirstOpticalGroup->size() << " hybrids in this group..." << RESET;
                if(cFirstOpticalGroup->size() > 0) // # of hybrids connected to OpticalGroup0
                {
                    LOG(INFO) << BOLDBLUE << "\t\t...Initializing HwInterfaces for FrontEnd Hybrids.." << +cFirstOpticalGroup->size() << " hybrid(s) found ..." << RESET;
                    auto cFirstHybrid = cFirstOpticalGroup->at(0);
                    auto cType        = FrontEndType::CBC3;
                    bool cWithCBC = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType         = FrontEndType::SSA;
                    bool cWithSSA = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());
                    cType         = FrontEndType::MPA;
                    bool cWithMPA = (std::find_if(cFirstHybrid->begin(), cFirstHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cFirstHybrid->end());

                    if(cWithCBC)
                    {
                        LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for CBC(s)" << RESET;
                        fReadoutChipInterface = new CbcInterface(fBeBoardFWMap);
                    }
                    if(cWithSSA && !cWithMPA)
                    {
                        LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for SSA(s)" << RESET;
                        fReadoutChipInterface = new SSAInterface(fBeBoardFWMap);
                    }
                    if(cWithMPA && !cWithSSA)
                    {
                        LOG(INFO) << BOLDBLUE << "\t\t\t\t.. Initializing HwInterface(s) for MPA(s)" << RESET;
                        fReadoutChipInterface = new MPAInterface(fBeBoardFWMap);
                    }
                    if((cWithMPA && cWithSSA) && cWithLpGBT)
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
        else
        {
            LOG(ERROR) << BOLDRED << "Unrecognized monitor type, Aborting" << RESET;
            abort();
        }
        fDetectorMonitor->forkMonitor();
    }

// turn on the SEH here - moved from the lpGBT interface
// I think it makes more sense to have it in the initialization step
#ifdef __SEH_USB__
    fTCInterface.getInterface().set_SehSupply(TC_2SSEH::sehSupplyState::sehSupply_On);
    LOG(INFO) << BOLDRED << "Intitally switching on SEH for configuration" << RESET;
// move this to the TC library .. I shouldn't have to wait here - you should wait for me
// std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
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

// ######################################
// # Configuring Inner Tracker hardware #
// ######################################
void SystemController::ConfigureIT(BeBoard* pBoard)
{
    // ###################
    // # Configuring FSM #
    // ###################
    size_t nTRIGxEvent = SystemController::findValueInSettings("nTRIGxEvent");
    size_t injType     = SystemController::findValueInSettings("INJtype");
    size_t injLatency  = SystemController::findValueInSettings("InjLatency");
    size_t nClkDelays  = SystemController::findValueInSettings("nClkDelays");
    size_t colStart    = SystemController::findValueInSettings("COLstart");
    bool   resetMask   = SystemController::findValueInSettings("ResetMask");
    bool   resetTDAC   = SystemController::findValueInSettings("ResetTDAC");
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
                flpGBTInterface->ExternalPhaseAlignRx(cOpticalGroup->flpGBT, pBoard, cOpticalGroup, this->fBeBoardFWMap[pBoard->getId()], fReadoutChipInterface);
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

    LOG(INFO) << GREEN << "Using " << BOLDYELLOW << RD53Shared::NTHREADS << RESET << GREEN << " threads for data decoding during running time" << RESET;
    RD53Event::ForkDecodingThreads();
}
// ######################################
// # Configuring Outer Tracker hardware #
// ######################################
void SystemController::ConfigureOT(BeBoard* pBoard)
{
    const uint8_t cCicDriveStrength = 4;
    // set board sparisification
    // based on what is configured in the fw register
    // read CIC sparsification setting from fW register
    // make sure board is also set to the same thing
    bool cSparsified = (fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
    pBoard->setSparsification(cSparsified);

    // Configure CPB
    // Optical link start-up
    // first configure lpGBT
    bool cNonModule = true;
    for(auto cOpticalGroup: *pBoard)
    {
        if(cOpticalGroup->flpGBT == nullptr) continue;

        LOG(INFO) << BOLDBLUE << "Now going to configuring lpGBTs#" << +cOpticalGroup->getId() << " on Board " << int(pBoard->getId()) << RESET;
        D19clpGBTInterface* clpGBTInterface = static_cast<D19clpGBTInterface*>(flpGBTInterface);
        if(!clpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT))
        {
            LOG(INFO) << BOLDRED << "SOMETHING FUNNY" << RESET;
            continue;
        }
        
        bool cWithPSmodule = false;
        bool cWith2Smodule = false;
        for(auto cHybrid: *cOpticalGroup)
        {
            auto cType     = FrontEndType::MPA;
            auto cMPAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
            cType          = FrontEndType::SSA;
            auto cSSAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
            cType          = FrontEndType::CBC3;
            auto cCBCfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());

            cWith2Smodule = cWith2Smodule || cCBCfound;
            cWithPSmodule = cWithPSmodule || cMPAfound || cSSAfound;
        }
        cNonModule = cWithPSmodule || cWith2Smodule;
        if(cWithPSmodule) { cOpticalGroup->setFrontEndType(FrontEndType::OuterTrackerPS); }
        else if(cWith2Smodule)
        {
            cOpticalGroup->setFrontEndType(FrontEndType::OuterTracker2S);
        }
        else
            LOG(INFO) << BOLDMAGENTA << "UN-KNOWN MODULE TYPE" << RESET;

        static_cast<D19clpGBTInterface*>(flpGBTInterface)->setFrontEndType(cOpticalGroup->getFrontEndType());
    }

    // module start-up
    // depends on module type
    bool cReSyncNeeded=false;
    for(auto cOpticalGroup: *pBoard)
    {
        uint8_t pCICUseNegEdge = 0;
        if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
        {
            LOG(INFO) << BOLDMAGENTA << "Configuring an OuterTracker2S module " << RESET;
            ModuleStartUp2S(cOpticalGroup);
            pCICUseNegEdge = 0;
        }
        if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
        {
            LOG(INFO) << BOLDMAGENTA << "Configuring an OuterTrackerPS module " << RESET;
            ModuleStartUpPS(cOpticalGroup);
            pCICUseNegEdge = 0;
        }

        auto& clpGBT = cOpticalGroup->flpGBT;
        // CIC configuration part .. first configure
        for(auto cHybrid: *cOpticalGroup)
        {
            auto&   cCic  = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
            if(cCic == NULL) continue;
            uint8_t cSide = cHybrid->getId() % 2;
            //if(clpGBT != nullptr) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
            if(clpGBT != nullptr)
            { 
                if( cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S ) 
                {
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
                }    
                else if(!cBrokenPS)
                {
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, cSide);
                }
                else
                {
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCIC(clpGBT, 0);
                }
            }

            LOG(INFO) << BOLDBLUE << "Configuring CIC" << +(cHybrid->getId() % 2) << " on link " << +cHybrid->getOpticalId() << " on hybrid " << +cHybrid->getId() << RESET;
            fCicInterface->ConfigureChip(cCic);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false); // make sure all FEs are disabled by default
        }                                                                    // CIC part - configure and make sure all FE blocks are disabled
        // then start-up CIC
        cReSyncNeeded = cReSyncNeeded || CicStartUp(cOpticalGroup, cCicDriveStrength, pCICUseNegEdge);
    }
    if( cReSyncNeeded )
    {
        LOG(INFO) << BOLDMAGENTA << "Sending a ReSync at the end of the OT-module configuration step" << RESET;
        // send a ReSync to all chips before starting
        fBeBoardInterface->ChipReSync(pBoard);
    }
    else 
        LOG(INFO) << BOLDMAGENTA << "No ReSync needed after OT-module configuration step" << RESET;

    // align BE for CIC 
    bool cSuccess = true;
    for(auto cOpticalGroup: *pBoard)
    {
        bool cBeAlignSuccess =  CicBeAlignment( cOpticalGroup );      
        if (cBeAlignSuccess) 
        {
                LOG(INFO) << BOLDGREEN << "Successful BE alignment for CIC data [hits+stubs]..." << RESET;
        }
        else
            LOG(INFO) << BOLDRED << "FAILED  BE alignment for CIC data [hits+stubs]..." << RESET;
        cSuccess = cSuccess && cBeAlignSuccess;
    }
    if( cSuccess )
    {
        uint16_t cOriginalTriggerSrc = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
        uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
        fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
        for(auto cOpticalGroup: *pBoard)
        {
            bool cDelayFoundSuccess =  CicPackageDelay( cOpticalGroup );   
            if (cDelayFoundSuccess) 
            {
                    LOG(INFO) << BOLDGREEN << "Successful BE alignment for CIC stub pkg..." << RESET;
            }
            else
                LOG(INFO) << BOLDRED << "FAILED  BE alignment CIC stub pkg..." << RESET;
            cSuccess = cSuccess && cDelayFoundSuccess;
        }
        cRegVec.clear();
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
        fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    }
    // finally
    // configure ROCs on hybrid .. can have SSAs or MPAs
    for(auto cOpticalGroup: *pBoard)
    {
        auto& clpGBT = cOpticalGroup->flpGBT;
           
        std::vector<FrontEndType> cFrontEndTypesAll{FrontEndType::SSA, FrontEndType::MPA, FrontEndType::CBC3};
        std::vector<FrontEndType> cFrontEndTypesPS{FrontEndType::SSA, FrontEndType::MPA};
        std::vector<FrontEndType> cFrontEndTypes2S{FrontEndType::CBC3};
        auto&                     cFrontEndTypes = (cOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S) ? cFrontEndTypes2S : cFrontEndTypesPS;
        if(cNonModule) cFrontEndTypes = cFrontEndTypesAll;
        for(auto cType: cFrontEndTypes)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                uint8_t cSide       = cHybrid->getId() % 2;
                auto    cHybridIter = std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; });
                if(clpGBT != nullptr && cHybridIter != cHybrid->end())
                {
                    // no SSA because I don't want to reset it here. . already done earlier
                    if(cType == FrontEndType::MPA)
                    {
                        LOG(INFO) << BOLDBLUE << "Resetting MPA" << RESET;
                        if( !cBrokenPS ) 
                        {
                            static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, cSide);
                        }
                        else
                        {
                            static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, 1);
                        }
                    }
                    if(cType == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Resetting CBC" << RESET;
                        static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cSide);
                    }
                }
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != cType) continue;
                    fReadoutChipInterface->ConfigureChip(cChip);
                } // ROC config
            }     // hybrid
        }         // configure all FE types
    }
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
        const uint8_t cSsaClockDrive = 4; // 4;
        const uint8_t cCicClockDrive = 4; // 4;

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
            if(!cBrokenPS )
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
            if(!cBrokenPS)
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetSSA(clpGBT, cSide);
            }
            else 
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->cicReset(clpGBT, 0);
            }
             
            bool     cSkipSSA3            = true; // eventually this needs to be set in the xml somewhere
            uint16_t cRegisterPadStrength = 0x1018;
            uint8_t  cSLVSdriveSSA        = 7;
            for(uint8_t cSSAId = 0; cSSAId < 8; cSSAId++)
            {
                if(cSkipSSA3 && cSSAId == 3) continue;

                LOG(INFO) << BOLDMAGENTA << "SSA " << +cSSAId << " current set to " << +cSLVSdriveSSA << "" << RESET;
                SSA* cSSA = new SSA(cHybrid->getBeBoardId(), cHybrid->getFMCId(), cHybrid->getId(), cSSAId, 0, 0, "./settings/SSAFiles/SSAPreCalibSYNC.txt");
                cSSA->setOpticalId(cHybrid->getOpticalId());
                cSSA->setOptical(cHybrid->isOptical());
                (fBeBoardInterface->getFirmwareInterface())->WriteFERegister(cSSA, cRegisterPadStrength, cSLVSdriveSSA);
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
        uint8_t                    cHybridClockDrive = 4;
        uint8_t                    cPreEmphMode      = 3; // 3
        uint8_t                    cPreEmphStr       = 7; // 7
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
bool SystemController::CicStartUp(const OpticalGroup* pOpticalGroup, uint8_t pDriveStrength, uint8_t pCICUseNegEdge)
{
    auto cBoardId    = pOpticalGroup->getBeBoardId();
    auto cBoardIter  = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    bool cWith2SFEH = (*cBoardIter)->getEventType() == EventType::VR2S;                
    auto cSparsified = (*cBoardIter)->getSparsification();

    auto& clpGBT = pOpticalGroup->flpGBT;
    bool cReSyncNeeded = false; 
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        if(cCic == NULL) continue;

        LOG(INFO) << BOLDMAGENTA << "SystemController::CicStartUp for OpticalGroup#" << +pOpticalGroup->getId() << " CIC#" << +cCic->getId() << RESET;

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
        bool cSuccess = fCicInterface->SelectMode(cCic, cModeSelect);
        if(!cSuccess)
        {
            LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to configure CIC mode.." << RESET;
            throw std::runtime_error(std::string("FAILED to set CIC mode ... something is wrong... .. STOPPING"));
        }
        LOG(INFO) << BOLDMAGENTA << "CIC configured for " << (cIs2S ? "2S" : "PS") << " readout." << RESET;

        // configure CIC FE enable register
        // first make sure it is set to 0x00 
        fCicInterface->EnableFEs(cCic,{0,1,2,3,4,5,6,7},false);
        // figure out which ROCs are enabled
        std::vector<uint8_t> cFeIds(0);
        for(auto cReadoutChip: *cHybrid)
        {
            // only consider MPAs and CBCs
            if(cReadoutChip->getFrontEndType() == FrontEndType::SSA) continue;
            cFeIds.push_back(cReadoutChip->getId());
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
        fCicInterface->ConfigureTermination(cCic, cClkTerm, cRxTerm);
        if(cSuccess) cSuccess = fCicInterface->StartUp(cCic, pDriveStrength);
        if(cSuccess) cSuccess = fCicInterface->SetSparsification(cCic, cSparsified);
        if(cSuccess) cSuccess = fCicInterface->ConfigureStubOutput(cCic);
        if(cSuccess) cSuccess = fCicInterface->ManualBx0Alignment(cCic, cBx0Delay);
        if(cSuccess)
        {
            LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " performed start-up sequence on CIC" << +cHybrid->getId() % 2 << " connected to link " << +cHybrid->getOpticalId() << RESET;
            cReSyncNeeded = cReSyncNeeded || fCicInterface->GetResyncRequest(cCic);
        }
        LOG(INFO) << BOLDGREEN << "####################################################################################" << RESET;
    } // all hybrids connected to this OG

    return cReSyncNeeded;
}
bool SystemController::CicBeAlignment(const OpticalGroup* pOpticalGroup )
{
    // make sure you're only sending one trigger at a time here
    auto cBoardId    = pOpticalGroup->getBeBoardId();
    auto cBoardIter  = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0);

    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        cFeEnableRegs.push_back( fCicInterface->ReadChipReg(cCic,"FE_ENABLE") );
    }

    // force CIC to output repeating 101010 pattern on L1 line
    // needed for phase alignment in back-end
    // force CIC to output empty L1A frames [by disabling all FEs]
    // needed for word alingment in the back-end
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // disable alignment output
        fCicInterface->SelectOutput(cCic, false);
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    bool cAligned = true;
    bool cL1Debug      = true;
    if(!(*cBoardIter)->isOptical()) cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1PhaseTuning((*cBoardIter), cL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A phase alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }
    
    // fL1Debug = false;
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1WordAlignment((*cBoardIter), cL1Debug);
    if(!cAligned)
    {
        LOG(INFO) << BOLDBLUE << "L1A word alignment in the back-end " << BOLDRED << " FAILED ..." << RESET;
        return false;
    }
    
    // enable CIC output of alignmnent pattern on stub lines
    // .. and enable all FEs again
    bool cIsPS = false;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        // enable alignment output for stubs
        fCicInterface->SelectOutput(cCic, true);
        // check for an MPA
        auto cType     = FrontEndType::MPA;
        auto cMPAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
        cIsPS          = cIsPS || cMPAfound;
        // check for an SSA
        cType          = FrontEndType::SSA;
        auto cSSAfound = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
        cIsPS          = cIsPS || cSSAfound;
    }
    bool cStubDebug     = true;
    size_t cNlines = cIsPS ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "SystemController::CicBeAlignment ... stub alignment on " << +cNlines << "/6 lines from CIC.." << RESET;
    cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubTuning((*cBoardIter), cStubDebug, cNlines);
    LOG(INFO) << BOLDMAGENTA << "Now looking at output of all hybrids " << RESET;
    for(auto cHybrid: *pOpticalGroup)
    {
        LOG(INFO) << BOLDMAGENTA << "Hybrid#" << +cHybrid->getId() << RESET;
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", cHybrid->getId());
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->StubDebug(true, cNlines);
    } // hybrids
    
    // disable CIC output of pattern on stub + l1 lines
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->SelectOutput(cCic, false);
    } // hybrids [CICs]
    
    // reconfigure FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "SystemController::CicBeAlignment Resetting FE_ENABLE" << RESET;
    size_t cIndx=0;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto&                cCic           = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->WriteChipReg(cCic,"FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;    
    } // hybrids
    
    // re-load configuration of fast command block from register map loaded from xml file
    LOG(INFO) << BOLDBLUE << "Re-loading original coonfiguration of fast command block from hardware description file [.xml] " << RESET;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureFastCommandBlock((*cBoardIter));
    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMultiplicity);
    return cAligned;
}
bool SystemController::CicPackageDelay(const OpticalGroup* pOpticalGroup )
{
    // only perform for first link 
    if(pOpticalGroup->getIndex() > 0) return true;

    // make sure you're only sending one trigger at a time here
    auto cBoardId    = pOpticalGroup->getBeBoardId();
    auto cBoardIter  = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    
    LOG(INFO) << GREEN << "Trying CIC un-packer alignment in the back-end" << RESET;
    uint32_t cNevents      = 10;
    uint16_t cMaxBxCounter = 3564;

    // sparsification of
    bool cSparsified = (*cBoardIter)->getSparsification();
    if(cSparsified)
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Sparsification on " << RESET;
    else
        LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Sparsification off " << RESET;

    std::vector<uint8_t> cFeEnableRegs(0);
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        cFeEnableRegs.push_back( fCicInterface->ReadChipReg(cCic,"FE_ENABLE") );
        // disable all FEs. . not needed here 
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
    }
    
    // check trigger source
    // and reload
    uint16_t cTriggerSrc         = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.trigger_source");
    uint16_t cOriginalTriggerSrc = cTriggerSrc;
    uint16_t cOrignalTriggerMult = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    uint8_t  cOriginalTLUconfig  = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.tlu_block.tlu_enabled");
    cTriggerSrc                  = (cTriggerSrc == 6) ? cTriggerSrc : 6;
    LOG(INFO) << BOLDBLUE << "Trigger source is set to " << +cTriggerSrc << RESET;
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    fBeBoardInterface->WriteBoardMultReg((*cBoardIter), cRegVec);

    // now try and find correct package delay
    auto cOriginalDelay = fBeBoardInterface->ReadBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");
    LOG(INFO) << BOLDBLUE << "Original package delay is " << +cOriginalDelay << RESET;
    bool    cCorrectDelay = false;
    uint8_t cPackageDelay = 7;
    uint8_t cFinalDelay   = cPackageDelay;

    for(cPackageDelay = 0; cPackageDelay < 8; cPackageDelay++)
    {
        if(cCorrectDelay) continue;

        LOG(INFO) << BOLDMAGENTA << "Package delay set to " << +cPackageDelay << RESET;
        fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay", cPackageDelay);
        (static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface()))->Bx0Alignment();

        // check stubs
        // 2 events should be enough
        // LOG(DEBUG) << BOLDMAGENTA << "Requesting " << +cNevents << " events from the board " << RESET;
        ReadNEvents((*cBoardIter), cNevents);
        const std::vector<Event*>& cEventsWithStubs = this->GetEvents();
        LOG(INFO) << BOLDBLUE << "Read back " << +cEventsWithStubs.size() << " events from the FC7 ..." << RESET;

        // now ... check for incrementing BxIds
        int              cNRollOvers = 0;
        std::vector<int> cBxIds(0);
        std::vector<int> cBxDifferences(0); // I think by injecting this way this number should always be the same ..
        for(auto& cEvent: cEventsWithStubs)
        {
            for(auto cHybrid: *pOpticalGroup)
            {
                if(cHybrid->getIndex() > 0) continue;

                auto cBx = (int)cEvent->BxId(cHybrid->getId());
                if(cBxIds.size() > 0)
                {
                    int cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBxIds[cBxIds.size() - 1] % cMaxBxCounter);
                    cNRollOvers += ((cBxIds[cBxIds.size() - 1] >= 2500) && (cBxIds[cBxIds.size() - 1] < cMaxBxCounter)) && (cBx < cBxIds[cBxIds.size() - 1]) ? 1 : 0;
                    cBxDifference = (cNRollOvers)*cMaxBxCounter + (cBx % cMaxBxCounter) - cBxDifference;
                    cBxDifferences.push_back(cBxDifference);
                    //LOG(INFO) << BOLDBLUE << "\t.....BxDifference is " << +cBxDifference << RESET;
                }
                cBxIds.push_back(cBx);
                //LOG(INFO) << BOLDBLUE << "Hybrid " << +cHybrid->getId() << " BxID " << +cBx << RESET;

            } // hybrids or CICs
        }         // events
        // figure out the differences between the bxIds
        auto cFirstDifference = cBxDifferences[0];
        std::adjacent_difference(cBxDifferences.begin(), cBxDifferences.end(), cBxDifferences.begin());
        cBxDifferences.erase(cBxDifferences.begin()); // erase the first element
        for(auto cDifference: cBxDifferences) LOG(DEBUG) << BOLDBLUE << "\t..." << +cDifference << RESET;
        // all elements are equal
        if(cFirstDifference != 0 && std::equal(cBxDifferences.begin() + 1, cBxDifferences.end(), cBxDifferences.begin()))
        {
            LOG(INFO) << BOLDGREEN << "Found differences between bxIds to always be the same : " << +cFirstDifference << RESET;
            LOG(INFO) << BOLDGREEN << "Going to fix the manual package delay to " << +cPackageDelay << RESET;
            cFinalDelay   = cPackageDelay;
            cCorrectDelay = true;
        }
        else
            LOG(INFO) << BOLDRED << "Found differences between bxIds to be different from one another." << RESET;

    } // pkg delay
    if(!cCorrectDelay) return cCorrectDelay;

    // set everything back to original values .. like I wasn't here
    // reset fast command registers
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Resetting BeBoards regs back to their original values" << RESET;
    cRegVec.clear();
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", cOriginalTriggerSrc});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cOrignalTriggerMult});
    cRegVec.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", cOriginalTLUconfig});
    fBeBoardInterface->WriteBoardMultReg((*cBoardIter), cRegVec);

    // reconfigure sparsification + FEs enabled in this CIC
    LOG(INFO) << BOLDMAGENTA << "BackEndAlignment::FindPackageDelay Resetting Sparsification" << RESET;
    fBeBoardInterface->WriteBoardReg((*cBoardIter), "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable", (int)cSparsified);
    size_t cIndx=0;
    for(auto cHybrid: *pOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
        fCicInterface->SetSparsification(cCic, cSparsified);
        fCicInterface->WriteChipReg(cCic,"FE_ENABLE", cFeEnableRegs[cIndx]);
        cIndx++;    
    }

    LOG(INFO) << BOLDMAGENTA << "Found package delay to be " << +cFinalDelay << RESET;
    return cCorrectDelay;
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
        cBoard->printBoardType();
        fBeBoardInterface->ConfigureBoard(cBoard);

        if(cBoard->getBoardType() == BoardType::D19C)
            ConfigureOT(cBoard);
        else if(cBoard->getBoardType() == BoardType::RD53)
            ConfigureIT(cBoard);
    }
    if(fDetectorMonitor != nullptr)
    {
        LOG(INFO) << GREEN << "Starting monitoring thread" << RESET;
        fDetectorMonitor->startMonitoring();
    }
    std::cout << __LINE__ << std::endl;
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
        bool  cTLUconfig  = (fBeBoardInterface->ReadBoardReg( fDetectorContainer->at(pBoard->getIndex()), "fc7_daq_cnfg.tlu_block.tlu_enabled") == 1 );
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
            //uint32_t  cBlockSize = 0x0000FFFF & pData.at(0);
            //LOG(INFO) << BOLDBLUE << "Reading events from " << +fNFe << " FEs connected to uDTC...[ " << +cBlockSize * 4 << " 32 bit words to decode]" << RESET;
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

            if(fEventType == EventType::PSAS) { fEventList.push_back(new D19cPSEventAS(pBoard, pData)); }
            else if(fEventType == EventType::SSAAS)
            {
                fEventList.push_back(new D19cSSAEventAS(pBoard, pData));
            }
            else if(fEventType == EventType::MPAAS)
            {
                fEventList.push_back(new D19cMPAEventAS(pBoard, pData));
            }
            else if(fEventType != EventType::ZS)
            {
                // check data words because I'm desperate
                // for( auto cWord : pData )
                //     LOG (INFO) << BOLDYELLOW << "SystemController \t..." << std::bitset<32>(cWord) << RESET;

                size_t cEventIndex    = 0;
                auto   cEventIterator = pData.begin();
                do {
                    uint32_t cHeader = (0xFFFF0000 & (*cEventIterator)) >> 16;
                    if(cHeader != 0xFFFF)
                    {
                        LOG (INFO) << BOLDRED << "SystemController::DecodeData Invalid header from the FW" << RESET;
                        cEventIterator++;
                    }
                    else // valid event  // decode
                    {
                        uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
                        //LOG (INFO) << BOLDMAGENTA << "SystemController::DecodeData Decoding event made of up " << +cEventSize << " 32 bit words. " << RESET;
                        auto     cEnd       = ((cEventIterator + cEventSize) > pData.end()) ? pData.end() : (cEventIterator + cEventSize);
                        // retrieve chunck of data vector belonging to this event
                        if(cEnd - cEventIterator == cEventSize)
                        {
                            std::vector<uint32_t> cEvent(cEventIterator, cEnd);
                            // some useful debug information
                            // LOG(INFO) << BOLDGREEN << "Event" << +cEventIndex << " .. Data word that should be event header ..  " << std::bitset<32>(*cEventIterator) << ". Event is made up of "
                            //            << +cEventSize << " 32 bit words..." << RESET;
                            if(pBoard->getFrontEndType() == FrontEndType::CBC3) { fEventList.push_back(new D19cCbc3Event(pBoard, cEvent)); }
                            else if(pBoard->getFrontEndType() == FrontEndType::CIC || pBoard->getFrontEndType() == FrontEndType::CIC2)
                            {
                                bool cWithCBC3 = !(fEventType == EventType::VR2S);
                                if(cWithCBC3)
                                    LOG(DEBUG) << BOLDBLUE << "Decoding CIC data : with 8CBC3 " << RESET;
                                else
                                    LOG(DEBUG) << BOLDBLUE << "Decoding CIC data : with 2S-FEH  " << RESET;
                                fEventList.push_back(new D19cCic2Event(pBoard, cEvent, cWithCBC3,cTLUconfig));
                            }
                            else if(pBoard->getFrontEndType() == FrontEndType::SSA)
                            {
                                fEventList.push_back(new D19cSSAEvent(pBoard, maxind, fNFe, cEvent));
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
}
} // namespace Ph2_System
