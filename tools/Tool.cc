#include "Tool.h"
#include <numeric>
#ifdef __USE_ROOT__
#include "TH1.h"
#endif

#include "../HWDescription/Chip.h"
#include "../Utils/ChannelGroupHandler.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/DataContainer.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/Occupancy.h"
#include <future>

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

Tool::Tool()
    : SystemController()
    ,
#ifdef __USE_ROOT__
    fCanvasMap()
    , fChipHistMap()
    , fHybridHistMap()
    , fSummaryTree(nullptr)
    ,
#endif
    fType()
    , fTestGroupChannelMap()
    , fDirectoryName("")
    ,
#ifdef __USE_ROOT__
    fResultFile(nullptr)
    ,
#endif
    fSkipMaskedChannels(false)
    , fAllChan(false)
    , fMaskChannelsFromOtherGroups(false)
    , fTestPulse(false)
    , fDoBoardBroadcast(false)
    , fDoHybridBroadcast(false)
    , fChannelGroupHandler(nullptr)
{
#ifdef __HTTP__
    fHttpServer = nullptr;
#endif
}

#ifdef __USE_ROOT__
#ifdef __HTTP__
Tool::Tool(THttpServer* pHttpServer)
    : SystemController()
    , fCanvasMap()
    , fChipHistMap()
    , fHybridHistMap()
    , fType()
    , fTestGroupChannelMap()
    , fDirectoryName("")
    , fResultFile(nullptr)
    , fHttpServer(pHttpServer)
    , fSkipMaskedChannels(false)
    , fAllChan(false)
    , fMaskChannelsFromOtherGroups(false)
    , fTestPulse(false)
    , fDoBoardBroadcast(false)
    , fDoHybridBroadcast(false)
    , fChannelGroupHandler(nullptr)
{
}
#endif
#endif

Tool::Tool(const Tool& pTool) { this->Inherit(&pTool); }

Tool::~Tool() {}

bool Tool::GetRunningStatus() { return (fRunningFuture.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready); }

void Tool::waitForRunToBeCompleted()
{
    while(!GetRunningStatus()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
}

void Tool::Configure(std::string cHWFile, bool enableStream)
{
    SystemController::Configure(cHWFile, enableStream);
    ConfigureCalibration();
}

void Tool::Start(int runNumber)
{
    fKeepRunning   = true;
    fRunNumber     = runNumber;
    fRunningFuture = std::async(std::launch::async, &Tool::Running, this);
}

void Tool::Stop()
{
    fKeepRunning = false;
    waitForRunToBeCompleted();
    SystemController::Stop();
}

void Tool::Inherit(const Tool* pTool)
{
    // WE SHOULD ONLY KEEP IN HERE ONLY THINGS THAT ARE NOT CALIBRATION SPECIFIC

    SystemController::Inherit(pTool);

#ifdef __USE_ROOT__
    fResultFile = pTool->fResultFile;
#endif
    fType          = pTool->fType;
    fDirectoryName = pTool->fDirectoryName;
#ifdef __USE_ROOT__
    fSummaryTree    = pTool->fSummaryTree;
    fCanvasMap      = pTool->fCanvasMap;
    fChipHistMap    = pTool->fChipHistMap;
    fHybridHistMap  = pTool->fHybridHistMap;
    fBeBoardHistMap = pTool->fBeBoardHistMap;
#endif
    fTestGroupChannelMap         = pTool->fTestGroupChannelMap;
    fSkipMaskedChannels          = pTool->fSkipMaskedChannels;
    fAllChan                     = pTool->fAllChan;
    fMaskChannelsFromOtherGroups = pTool->fMaskChannelsFromOtherGroups;
    fTestPulse                   = pTool->fTestPulse;
    fDoBoardBroadcast            = pTool->fDoBoardBroadcast;
    fDoHybridBroadcast           = pTool->fDoHybridBroadcast;

#ifdef __HTTP__
    fHttpServer = pTool->fHttpServer;
#endif
}

void Tool::Inherit(const SystemController* pSystemController) { SystemController::Inherit(pSystemController); }

void Tool::resetPointers()
{
    if(fChannelGroupHandler != nullptr)
    {
        // delete fChannelGroupHandler;
        fChannelGroupHandler = nullptr;
    }
}

void Tool::Destroy()
{
    LOG(INFO) << BOLDRED << "Destroying memory objects" << RESET;
    SystemController::Destroy();
#ifdef __HTTP__
    LOG(INFO) << BOLDRED << "Destroying HttpServer" << RESET;
    if(fHttpServer)
    {
        delete fHttpServer;
        fHttpServer = nullptr;
    }
    LOG(INFO) << BOLDRED << "HttpServer Destroyed" << RESET;
#endif

    SoftDestroy();
    LOG(INFO) << BOLDRED << "Memory objects destroyed" << RESET;
}

void Tool::SoftDestroy()
{
#ifdef __USE_ROOT__
    if(fResultFile != nullptr)
    {
        if(fResultFile->IsOpen()) fResultFile->Close();

        if(fResultFile)
        {
            delete fResultFile;
            fResultFile = nullptr;
        }
    }

    for(auto canvas: fCanvasMap)
    {
        delete canvas.second;
        canvas.second = nullptr;
    }
    fCanvasMap.clear();
    for(auto chip: fChipHistMap)
    {
        for(auto hist: chip.second)
        {
            delete hist.second;
            hist.second = nullptr;
        }
    }
    fChipHistMap.clear();
    for(auto chip: fHybridHistMap)
    {
        for(auto hist: chip.second)
        {
            delete hist.second;
            hist.second = nullptr;
        }
    }
    fHybridHistMap.clear();
    for(auto chip: fBeBoardHistMap)
    {
        for(auto hist: chip.second)
        {
            delete hist.second;
            hist.second = nullptr;
        }
    }
    fBeBoardHistMap.clear();
#endif
    fTestGroupChannelMap.clear();
}

#ifdef __USE_ROOT__
TString  Tool::fSummaryTreeParameter = ""; // Is this ok here?
Double_t Tool::fSummaryTreeValue     = 0.0;

/*!
 * \brief Initialize a 'summary' TTree in the ROOT File, with branches 'parameter'(string) and 'value'(double)
 */
void Tool::bookSummaryTree() // MINE
{
    fResultFile->cd();
    fSummaryTreeParameter = "";
    fSummaryTreeValue     = 0;
    fSummaryTree          = new TTree("summaryTree", "Most relevant results");
    fSummaryTree->Branch("Parameter", &fSummaryTreeParameter);
    fSummaryTree->Branch("Value", &fSummaryTreeValue);
}

/*!
 * \brief Insert data into the summary tree
 * \param cParameter : Name of the measurement to be stored
 * \param cValue: Value of the measurement to be stored
 */
void Tool::fillSummaryTree(TString cParameter, Double_t cValue) // MINE
{
    fResultFile->cd();
    fSummaryTreeParameter.Clear();
    fSummaryTreeParameter = cParameter;
    fSummaryTreeValue     = cValue;
    if(fSummaryTree) fSummaryTree->Fill();
}

TString Tool::getDirectoryName() { return fDirectoryName.c_str(); }

void Tool::bookHistogram(ChipContainer* pChip, std::string pName, TObject* pObject)
{
    TH1* tmpHistogramPointer = dynamic_cast<TH1*>(pObject);
    if(tmpHistogramPointer != nullptr) tmpHistogramPointer->SetDirectory(0);

    // find or create map<string,TOBject> for specific CBC
    auto cChipHistMap = fChipHistMap.find(pChip);

    if(cChipHistMap == std::end(fChipHistMap))
    {
        // Fabio: CBC specific -> to be moved out from Tool
        LOG(INFO) << "Histo Map for CBC " << int(pChip->getId()) << " (FE " << int(static_cast<ReadoutChip*>(pChip)->getHybridId()) << ") does not exist - creating ";
        std::map<std::string, TObject*> cTempChipMap;

        fChipHistMap[pChip] = cTempChipMap;
        cChipHistMap        = fChipHistMap.find(pChip);
    }

    // find histogram with given name: if it exists, delete the object, if not create
    auto cHisto = cChipHistMap->second.find(pName);

    if(cHisto != std::end(cChipHistMap->second)) cChipHistMap->second.erase(cHisto);

    cChipHistMap->second[pName] = pObject;

#ifdef __HTTP__
    if(fHttpServer) fHttpServer->Register("/Histograms", pObject);
#endif
}

void Tool::bookHistogram(HybridContainer* pHybrid, std::string pName, TObject* pObject)
{
    TH1* tmpHistogramPointer = dynamic_cast<TH1*>(pObject);
    if(tmpHistogramPointer != nullptr) tmpHistogramPointer->SetDirectory(0);

    // find or create map<string,TOBject> for specific CBC
    auto cHybridHistMap = fHybridHistMap.find(pHybrid);

    if(cHybridHistMap == std::end(fHybridHistMap))
    {
        LOG(INFO) << "Histo Map for Hybrid " << int(pHybrid->getId()) << " does not exist - creating ";
        std::map<std::string, TObject*> cTempHybridMap;

        fHybridHistMap[pHybrid] = cTempHybridMap;
        cHybridHistMap          = fHybridHistMap.find(pHybrid);
    }

    // find histogram with given name: if it exists, delete the object, if not create
    auto cHisto = cHybridHistMap->second.find(pName);

    if(cHisto != std::end(cHybridHistMap->second)) cHybridHistMap->second.erase(cHisto);

    cHybridHistMap->second[pName] = pObject;
#ifdef __HTTP__
    if(fHttpServer) fHttpServer->Register("/Histograms", pObject);
#endif
}

void Tool::bookHistogram(BoardContainer* pBeBoard, std::string pName, TObject* pObject)
{
    TH1* tmpHistogramPointer = dynamic_cast<TH1*>(pObject);
    if(tmpHistogramPointer != nullptr) tmpHistogramPointer->SetDirectory(0);

    // find or create map<string,TOBject> for specific CBC
    auto cBeBoardHistMap = fBeBoardHistMap.find(pBeBoard);

    if(cBeBoardHistMap == std::end(fBeBoardHistMap))
    {
        LOG(INFO) << "Histo Map for Hybrid " << int(pBeBoard->getId()) << " does not exist - creating ";
        std::map<std::string, TObject*> cTempHybridMap;

        fBeBoardHistMap[pBeBoard] = cTempHybridMap;
        cBeBoardHistMap           = fBeBoardHistMap.find(pBeBoard);
    }

    // find histogram with given name: if it exists, delete the object, if not create
    auto cHisto = cBeBoardHistMap->second.find(pName);

    if(cHisto != std::end(cBeBoardHistMap->second)) cBeBoardHistMap->second.erase(cHisto);

    cBeBoardHistMap->second[pName] = pObject;
#ifdef __HTTP__
    if(fHttpServer) fHttpServer->Register("/Histograms", pObject);
#endif
}

TObject* Tool::getHist(ChipContainer* pChip, std::string pName)
{
    auto cChipHistMap = fChipHistMap.find(pChip);

    if(cChipHistMap == std::end(fChipHistMap))
    {
        // Fabio: CBC specific -> to be moved out from Tool
        LOG(ERROR) << RED << "Error: could not find the Histograms for CBC " << int(pChip->getId()) << " (FE " << int(static_cast<ReadoutChip*>(pChip)->getHybridId()) << ")" << RESET;
        return nullptr;
    }
    else
    {
        auto cHisto = cChipHistMap->second.find(pName);

        if(cHisto == std::end(cChipHistMap->second))
        {
            LOG(ERROR) << RED << "Error: could not find the Histogram with the name " << pName << RESET;
            return nullptr;
        }
        else
            return cHisto->second;
    }
}

TObject* Tool::getHist(HybridContainer* pHybrid, std::string pName)
{
    auto cHybridHistMap = fHybridHistMap.find(pHybrid);

    if(cHybridHistMap == std::end(fHybridHistMap))
    {
        LOG(ERROR) << RED << "Error: could not find the Histograms for Hybrid " << int(pHybrid->getId()) << RESET;
        return nullptr;
    }
    else
    {
        auto cHisto = cHybridHistMap->second.find(pName);

        if(cHisto == std::end(cHybridHistMap->second))
        {
            LOG(ERROR) << RED << "Error: could not find the Histogram with the name " << pName << RESET;
            return nullptr;
        }
        else
            return cHisto->second;
    }
}

TObject* Tool::getHist(BoardContainer* pBeBoard, std::string pName)
{
    auto cBeBoardHistMap = fBeBoardHistMap.find(pBeBoard);

    if(cBeBoardHistMap == std::end(fBeBoardHistMap))
    {
        LOG(ERROR) << RED << "Error: could not find the Histograms for Hybrid " << int(pBeBoard->getId()) << RESET;
        return nullptr;
    }
    else
    {
        auto cHisto = cBeBoardHistMap->second.find(pName);

        if(cHisto == std::end(cBeBoardHistMap->second))
        {
            LOG(ERROR) << RED << "Error: could not find the Histogram with the name " << pName << RESET;
            return nullptr;
        }
        else
            return cHisto->second;
    }
}

void Tool::WriteRootFile()
{
    if((fResultFile != nullptr) && (fResultFile->IsOpen() == true)) fResultFile->Write();
}
#endif

void Tool::SaveResults()
{
#ifdef __USE_ROOT__
    for(const auto& cBeBoard: fBeBoardHistMap)
    {
        fResultFile->cd();

        for(const auto& cHist: cBeBoard.second) cHist.second->Write(cHist.second->GetName(), TObject::kOverwrite);

        fResultFile->cd();
    }

    // Now per FE
    for(const auto& cHybrid: fHybridHistMap)
    {
        TString  cDirName = Form("FE%d", cHybrid.first->getId());
        TObject* cObj     = gROOT->FindObject(cDirName);

        // if ( cObj ) delete cObj;

        if(!cObj) fResultFile->mkdir(cDirName);

        fResultFile->cd(cDirName);

        for(const auto& cHist: cHybrid.second) cHist.second->Write(cHist.second->GetName(), TObject::kOverwrite);

        fResultFile->cd();
    }

    for(const auto& cChip: fChipHistMap)
    {
        // Fabio: CBC specific -> to be moved out from Tool
        TString  cDirName = Form("FE%dCBC%d", static_cast<ReadoutChip*>(cChip.first)->getHybridId(), cChip.first->getId());
        TObject* cObj     = gROOT->FindObject(cDirName);

        // if ( cObj ) delete cObj;

        if(!cObj) fResultFile->mkdir(cDirName);

        fResultFile->cd(cDirName);

        for(const auto& cHist: cChip.second) cHist.second->Write(cHist.second->GetName(), TObject::kOverwrite);

        fResultFile->cd();
    }

    // Save Canvasses too
    for(const auto& cCanvas: fCanvasMap)
    {
        cCanvas.second->Write(cCanvas.second->GetName(), TObject::kOverwrite);
        std::string cPdfName = fDirectoryName + "/" + cCanvas.second->GetName() + ".pdf";
        cCanvas.second->SaveAs(cPdfName.c_str());
    }
    // Save summary TTree
    if(fSummaryTree != nullptr) fSummaryTree->Write(); // Seems to be needed with ROOT6, seems to break with ROOT5...

#endif

    // fResultFile->Write();
    // fResultFile->Close();

    LOG(INFO) << "Results saved!";
}

void Tool::CreateResultDirectory(const std::string& pDirname, bool pMode, bool pDate)
{
    // Fabio: CBC specific -> to be moved out from Tool - BEGIN
    bool cCheck = false;
    bool cHoleMode;
    auto cSetting = fSettingsMap.find("HoleMode");

    if(cSetting != std::end(fSettingsMap))
    {
        cCheck    = true;
        cHoleMode = (boost::any_cast<double>(cSetting->second) == 1) ? true : false;
    }

    std::string cMode;

    if(cCheck)
    {
        if(cHoleMode)
            cMode = "_Hole";
        else
            cMode = "_Electron";
    }
    // Fabio: CBC specific -> to be moved out from Tool - END

    std::string nDirname = pDirname;

    if(cCheck && pMode) nDirname += cMode;

    if(pDate) nDirname += currentDateTime();

    LOG(INFO) << GREEN << "Creating directory: " << BOLDYELLOW << nDirname << RESET;
    std::string cCommand = "mkdir -p " + nDirname;

    try
    {
        system(cCommand.c_str());
    }
    catch(std::exception& e)
    {
        LOG(ERROR) << "Exceptin when trying to create Result Directory: " << e.what();
    }

    fDirectoryName = nDirname;
}

/*!
 * \brief Initialize the result Root file
 * \param pFilename : Root filename
 */
#ifdef __USE_ROOT__
void Tool::InitResultFile(const std::string& pFilename)
{
    if(!fDirectoryName.empty())
    {
        std::string cFilename = fDirectoryName + "/" + pFilename + ".root";

        try
        {
            fResultFile     = TFile::Open(cFilename.c_str(), "RECREATE");
            fResultFileName = cFilename;
        }
        catch(std::exception& e)
        {
            LOG(ERROR) << "Exceptin when trying to create Result File: " << e.what();
        }
    }
    else
        LOG(INFO) << RED << "ERROR: " << RESET << "No Result Directory initialized - not saving results!";
}

void Tool::CloseResultFile()
{
    if(fResultFile != nullptr)
    {
        LOG(INFO) << GREEN << "Closing result file" << RESET;
        fResultFile->Close();
        delete fResultFile;
        fResultFile = nullptr;
    }
}

void Tool::StartHttpServer(const int pPort, bool pReadonly)
{
#ifdef __HTTP__

    if(fHttpServer)
    {
        delete fHttpServer;
        fHttpServer = nullptr;
    }

    char hostname[HOST_NAME_MAX];

    try
    {
        fHttpServer = new THttpServer(Form("http:%d", pPort));
        fHttpServer->SetReadOnly(pReadonly);
        // fHttpServer->SetTimer ( pRefreshTime, kTRUE );
        fHttpServer->SetTimer(0, kTRUE);
        fHttpServer->SetJSROOT("https://root.cern.ch/js/latest/");

        // configure the server
        // see: https://root.cern.ch/gitweb/?p=root.git;a=blob_plain;f=tutorials/http/httpcontrol.C;hb=HEAD
        fHttpServer->SetItemField("/", "_monitoring", "5000");
        fHttpServer->SetItemField("/", "_layout", "grid2x2");

        gethostname(hostname, HOST_NAME_MAX);
    }
    catch(std::exception& e)
    {
        LOG(ERROR) << "Exception when trying to start THttpServer: " << e.what();
    }

    LOG(INFO) << "Opening THttpServer on port " << pPort << ". Point your browser to: " << GREEN << hostname << ":" << pPort << RESET;
#else
    LOG(INFO) << "Error, ROOT version < 5.34 detected or not compiled with Http Server support!"
              << " No THttpServer available! - The webgui will fail to show plots!";
    LOG(INFO) << "ROOT must be built with '--enable-http' flag to use this feature.";
#endif
}

void Tool::HttpServerProcess()
{
#ifdef __HTTP__

    if(fHttpServer)
    {
        gSystem->ProcessEvents();
        fHttpServer->ProcessRequests();
    }

#endif
}
#endif

void Tool::dumpConfigFiles()
{
    if(!fDirectoryName.empty())
    {
        // Fabio: CBC specific -> to be moved out from Tool
        for(auto board: *fDetectorContainer)
        {
            if(board->getBoardType() == BoardType::RD53) break;

            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        std::string cFilename = fDirectoryName + "/BE" + std::to_string(board->getId()) + "_OG" + std::to_string(opticalGroup->getId()) + "_FE" + std::to_string(hybrid->getId()) +
                                                "_Chip" + std::to_string(chip->getId()) + ".txt";
                        LOG(DEBUG) << BOLDBLUE << "Dumping readout chip configuration to " << cFilename << RESET;
                        chip->saveRegMap(cFilename.data());
                    }
                    auto& cCic = static_cast<OuterTrackerHybrid*>(hybrid)->fCic;
                    if(cCic != NULL)
                    {
                        std::string cFilename =
                            fDirectoryName + "/BE" + std::to_string(board->getId()) + "_OG" + std::to_string(opticalGroup->getId()) + "_FE" + std::to_string(hybrid->getId()) + ".txt";
                        LOG(INFO) << BOLDBLUE << "Dumping CIC configuration to " << cFilename << RESET;
                        cCic->saveRegMap(cFilename.data());
                    }
                }
            }
        }
        LOG(INFO) << BOLDBLUE << "Configfiles for all Chips written to " << fDirectoryName << RESET;
    }
    else
        LOG(ERROR) << "Error: no results Directory initialized" << RESET;
}

void Tool::setSystemTestPulse(uint8_t pTPAmplitude, uint8_t pTestGroup, bool pTPState, bool pHoleMode)
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // Fabio: CBC specific but not used by common scans - BEGIN
                    // first, get the Amux Value
                    uint8_t cOriginalAmuxValue;
                    cOriginalAmuxValue = cChip->getReg("MiscTestPulseCtrl&AnalogMux");
                    // uint8_t cOriginalHitDetectSLVSValue = cChip->getReg ("HitDetectSLVS" );

                    std::vector<std::pair<std::string, uint16_t>> cRegVec;
                    uint8_t                                       cRegValue = to_reg(0, pTestGroup);

                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        uint8_t cTPRegValue;

                        if(pTPState)
                            cTPRegValue = (cOriginalAmuxValue | 0x1 << 6);
                        else if(!pTPState)
                            cTPRegValue = (cOriginalAmuxValue & ~(0x1 << 6));

                        // uint8_t cHitDetectSLVSValue = (cOriginalHitDetectSLVSValue & ~(0x1 << 6));

                        // cRegVec.push_back ( std::make_pair ( "HitDetectSLVS", cHitDetectSLVSValue ) );
                        cRegVec.push_back(std::make_pair("MiscTestPulseCtrl&AnalogMux", cTPRegValue));
                        cRegVec.push_back(std::make_pair("TestPulseDel&ChanGroup", cRegValue));
                        cRegVec.push_back(std::make_pair("TestPulsePotNodeSel", pTPAmplitude));
                        LOG(DEBUG) << BOLDBLUE << "Read original Amux Value to be: " << std::bitset<8>(cOriginalAmuxValue) << " and changed to " << std::bitset<8>(cTPRegValue)
                                   << " - the TP is bit 6!" RESET;
                    }

                    this->fReadoutChipInterface->WriteChipMultReg(cChip, cRegVec);
                    // Fabio: CBC specific but not used by common scans - END
                }
            }
        }
    }
}

void Tool::enableTestPulse(bool enableTP)
{
    fTestPulse = enableTP;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->enableInjection(cChip, enableTP); }
            }
        }
    }

    return;
}

void Tool::selectGroupTestPulse(Chip* cChip, uint8_t pTestGroup)
{
    switch(cChip->getFrontEndType())
    {
    case FrontEndType::CBC3:
    {
        uint8_t cRegValue = to_reg(0, pTestGroup);
        this->fReadoutChipInterface->WriteChipReg(cChip, "TestPulseDel&ChanGroup", cRegValue);
        break;
    }

    default:
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " FrontEnd type not recognized for Bebord " << cChip->getBeBoardId() << " Hybrid " << cChip->getHybridId() << " Chip " << +cChip->getId()
                   << ", aborting" << RESET;
        throw("[Tool::selectGroupTestPulse]\tError, FrontEnd type not found");
        break;
    }
    }

    return;
}

void Tool::setFWTestPulse()
{
    for(auto cBoard: *fDetectorContainer)
    {
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        // uint8_t cAsync = ( cBoard->getEventType() == EventType::SSAAS || cBoard->getEventType() == EventType::MPAAS )
        // ? 1 : 0;
        switch(cBoard->getBoardType())
        {
        case BoardType::D19C:
        {
            EventType cEventType = cBoard->getEventType();
            bool      cAsync     = (cEventType == EventType::SSAAS || cEventType == EventType::MPAAS);

            if(!cAsync)
            {
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 6});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            }
            else
            {
                LOG(INFO) << BOLDBLUE << "Since I'm in ASYNC mode .. set trigger source to 10" << RESET;
                // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
                cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 6});
                cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
            }
            break;
        }

        default:
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " BeBoard type not recognized for Bebord " << cBoard->getId() << ", aborting" << RESET;
            throw("[Tool::setFWTestPulse]\tError, BeBoard type not found");
            break;
        }
        }

        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
    }
}

void Tool::CreateReport()
{
    std::ofstream report;
    report.open(fDirectoryName + "/TestReport.txt", std::ofstream::out | std::ofstream::app);
    report.close();
}

void Tool::AmmendReport(std::string pString)
{
    std::ofstream report;
    report.open(fDirectoryName + "/TestReport.txt", std::ofstream::out | std::ofstream::app);
    report << pString << std::endl;
    report.close();
}

std::pair<float, float> Tool::getStats(std::vector<float> pData)
{
    float              cMean = std::accumulate(pData.begin(), pData.end(), 0.) / pData.size();
    std::vector<float> cTmp(pData.size(), 0);
    std::transform(pData.begin(), pData.end(), cTmp.begin(), [&](float el) { return (el - cMean) * (el - cMean); });
    float cStandardDeviation = std::sqrt(std::accumulate(cTmp.begin(), cTmp.end(), 0.) / (cTmp.size() - 1.));
    return std::make_pair(cMean, cStandardDeviation);
}

std::pair<std::vector<float>, std::vector<float>> Tool::getDerivative(std::vector<float> pData, std::vector<float> pValues, bool pIgnoreNegative)
{
    std::vector<float> cWeights(pData.size());
    std::adjacent_difference(pData.begin(), pData.end(), cWeights.begin());
    // replace negative entries with 0s
    if(pIgnoreNegative) std::replace_if(cWeights.begin(), cWeights.end(), [](float i) { return std::signbit(i); }, 0);
    cWeights.erase(cWeights.begin(), cWeights.begin() + 1);
    pValues.erase(pValues.begin(), pValues.begin() + 1);
    return std::make_pair(cWeights, pValues);
}

std::pair<float, float> Tool::evalNoise(std::vector<float> pData, std::vector<float> pValues, bool pIgnoreNegative)
{
    std::vector<float> cWeights(pData.size());
    std::adjacent_difference(pData.begin(), pData.end(), cWeights.begin());
    cWeights.erase(cWeights.begin(), cWeights.begin() + 1);
    if(pIgnoreNegative) std::replace_if(cWeights.begin(), cWeights.end(), [](float i) { return std::signbit(i); }, 0);
    float cN            = static_cast<float>(cWeights.size() - std::count(cWeights.begin(), cWeights.end(), 0.));
    float cSumOfWeights = std::accumulate(cWeights.begin(), cWeights.end(), 0.);
    // weighted sum of scan values to get pedestal
    pData.erase(pData.begin(), pData.begin() + 1);
    std::fill(pData.begin(), pData.end(), 0.);
    std::transform(cWeights.begin(), cWeights.end(), pValues.begin(), pData.begin(), std::multiplies<float>());
    float cMean = std::accumulate(pData.begin(), pData.end(), 0.);
    cMean /= cSumOfWeights;
    // weighted sample variance of scan values to get noise
    std::transform(pValues.begin(), pValues.end(), pData.begin(), [&](float el) { return (el - cMean) * (el - cMean); });
    std::transform(cWeights.begin(), cWeights.end(), pData.begin(), pData.begin(), std::multiplies<float>());
    float cCorrection = std::sqrt(cN / (cN - 1.));
    float cNoise      = (std::accumulate(pData.begin(), pData.end(), 0.) / (cSumOfWeights));
    cNoise            = std::sqrt(cNoise);
    LOG(DEBUG) << BOLDBLUE << "Noise is " << cNoise << " -- sum of weights  " << cSumOfWeights << " " << cN << " non zero-elements : correction of " << cCorrection << RESET;
    return std::make_pair(cMean, cNoise * cCorrection);
}

// then a method to un-mask pairs of channels on a given CBC
void Tool::unmaskPair(Chip* cChip, std::pair<uint8_t, uint8_t> pPair)
{
    // Fabio: CBC specific but not used by common scans - BEGIN

    // get ready to mask/un-mask channels in pairs...
    MaskedChannelsList cMaskedList;
    MaskedChannels     cMaskedChannels;
    cMaskedChannels.clear();
    cMaskedChannels.push_back(pPair.first);

    uint8_t     cRegisterIndex = pPair.first >> 3;
    std::string cMaskRegName   = fChannelMaskMapCBC3[cRegisterIndex];
    cMaskedList.insert(std::pair<std::string, MaskedChannels>(cMaskRegName.c_str(), cMaskedChannels));

    cRegisterIndex = pPair.second >> 3;
    cMaskRegName   = fChannelMaskMapCBC3[cRegisterIndex];
    auto it        = cMaskedList.find(cMaskRegName.c_str());
    if(it != cMaskedList.end()) { (it->second).push_back(pPair.second); }
    else
    {
        cMaskedChannels.clear();
        cMaskedChannels.push_back(pPair.second);
        cMaskedList.insert(std::pair<std::string, MaskedChannels>(cMaskRegName.c_str(), cMaskedChannels));
    }

    // Do the actual channel un-masking
    for(auto cMasked: cMaskedList)
    {
        uint8_t     cRegValue = 0; // cChip->getReg (cMasked.first);
        std::string cOutput   = "";
        for(auto cMaskedChannel: cMasked.second)
        {
            uint8_t cBitShift = (cMaskedChannel)&0x7;
            cRegValue |= (1 << cBitShift);
            std::string cChType = ((+cMaskedChannel & 0x1) == 0) ? "seed" : "correlation";
            std::string cOut    = "Channel " + std::to_string((int)cMaskedChannel) + " in the " + cChType.c_str() + " layer\t";
            cOutput += cOut.data();
        }
        // Channels for stub sweep : " << cOutput.c_str() << RESET ;
        fReadoutChipInterface->WriteChipReg(cChip, cMasked.first, cRegValue);
    }
    // Fabio: CBC specific but not used by common scans - END
}

// Two dimensional dac scan
void Tool::scanDacDac(const std::string&                               dac1Name,
                      const std::vector<uint16_t>&                     dac1List,
                      const std::string&                               dac2Name,
                      const std::vector<uint16_t>&                     dac2List,
                      uint32_t                                         numberOfEvents,
                      std::vector<std::vector<DetectorDataContainer*>> detectorContainerVectorOfVector,
                      int32_t                                          numberOfEventsPerBurst)
{
    for(unsigned int boardIndex = 0; boardIndex < fDetectorContainer->size(); boardIndex++)
    { scanBeBoardDacDac(boardIndex, dac1Name, dac1List, dac2Name, dac2List, numberOfEvents, detectorContainerVectorOfVector, numberOfEventsPerBurst); }

    return;
}

// Two dimensional dac scan per BeBoard
void Tool::scanBeBoardDacDac(uint16_t                                         boardIndex,
                             const std::string&                               dac1Name,
                             const std::vector<uint16_t>&                     dac1List,
                             const std::string&                               dac2Name,
                             const std::vector<uint16_t>&                     dac2List,
                             uint32_t                                         numberOfEvents,
                             std::vector<std::vector<DetectorDataContainer*>> detectorContainerVectorOfVector,
                             int32_t                                          numberOfEventsPerBurst)
{
    if(dac1List.size() != detectorContainerVectorOfVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    for(size_t dacIt = 0; dacIt < dac1List.size(); ++dacIt)
    {
        // el::LoggingFlag::NewLineForContainer (0);
        if(boardIndex == 0) LOG(INFO) << BOLDBLUE << " Scanning dac1 " << dac1Name << ", value = " << dac1List[dacIt] << " vs " << dac2Name << RESET;
        // el::LoggingFlag::NewLineForContainer (1);
        setSameDacBeBoard(fDetectorContainer->at(boardIndex), dac1Name, dac1List[dacIt]);
        scanBeBoardDac(boardIndex, dac2Name, dac2List, numberOfEvents, detectorContainerVectorOfVector[dacIt], numberOfEventsPerBurst);
    }

    return;
}

// One dimensional dac scan
void Tool::scanDac(const std::string&                  dacName,
                   const std::vector<uint16_t>&        dacList,
                   uint32_t                            numberOfEvents,
                   std::vector<DetectorDataContainer*> detectorContainerVector,
                   int32_t                             numberOfEventsPerBurst)
{
    for(unsigned int boardIndex = 0; boardIndex < fDetectorContainer->size(); boardIndex++)
    { scanBeBoardDac(boardIndex, dacName, dacList, numberOfEvents, detectorContainerVector, numberOfEventsPerBurst); }

    return;
}

// bit wise scan
void Tool::bitWiseScan(const std::string& dacName, uint32_t numberOfEvents, const float& targetOccupancy, int32_t numberOfEventsPerBurst)
{
    for(unsigned int boardIndex = 0; boardIndex < fDetectorContainer->size(); boardIndex++) { bitWiseScanBeBoard(boardIndex, dacName, numberOfEvents, targetOccupancy, numberOfEventsPerBurst); }
    return;
}

// bit wise scan per BeBoard
void Tool::bitWiseScanBeBoard(uint16_t boardIndex, const std::string& dacName, uint32_t numberOfEvents, const float& targetOccupancy, int32_t numberOfEventsPerBurst)
{
    // int minDAC = 0x0;
    DetectorDataContainer* outputDataContainer = fDetectorDataContainer;
    ReadoutChip*           cChip               = fDetectorContainer->at(boardIndex)->at(0)->at(0)->at(0); // assumption: one BeBoard has only one type of chip;
    bool                   localDAC            = cChip->isDACLocal(dacName);
    uint8_t                numberOfBits        = cChip->getNumberOfBits(dacName);
    LOG(INFO) << BOLDBLUE << "Number of bits in this DAC is " << +numberOfBits << RESET;
    bool                   occupanyDirectlyProportionalToDAC;
    DetectorDataContainer* previousStepOccupancyContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *previousStepOccupancyContainer);
    DetectorDataContainer* currentStepOccupancyContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *currentStepOccupancyContainer);

    DetectorDataContainer* previousDacList = new DetectorDataContainer();
    DetectorDataContainer* currentDacList  = new DetectorDataContainer();

    uint16_t allZeroRegister = 0;
    uint16_t allOneRegister  = (0xFFFF >> (16 - numberOfBits));
    if(localDAC)
    {
        ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, *previousDacList, allZeroRegister);
        ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, *currentDacList, allOneRegister);
    }
    else
    {
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *previousDacList, allZeroRegister);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *currentDacList, allOneRegister);
    }
    LOG(INFO) << BOLDBLUE << "Setting all bits of registers " << dacName << "  to  " << +allZeroRegister << RESET;
    if(localDAC)
        setAllLocalDacBeBoard(boardIndex, dacName, *previousDacList);
    else
        setAllGlobalDacBeBoard(boardIndex, dacName, *previousDacList);

    fDetectorDataContainer = previousStepOccupancyContainer;
    LOG(INFO) << BOLDBLUE << "\t\t... measuring occupancy...." << RESET;
    measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);

    LOG(INFO) << BOLDBLUE << "Setting all bits of registers " << dacName << "  to  " << +allOneRegister << RESET;
    if(localDAC)
        setAllLocalDacBeBoard(boardIndex, dacName, *currentDacList);
    else
        setAllGlobalDacBeBoard(boardIndex, dacName, *currentDacList);

    fDetectorDataContainer = currentStepOccupancyContainer;
    LOG(INFO) << BOLDBLUE << "\t\t... measuring occupancy...." << RESET;
    measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);

    occupanyDirectlyProportionalToDAC =
        currentStepOccupancyContainer->at(boardIndex)->getSummary<Occupancy, Occupancy>().fOccupancy > previousStepOccupancyContainer->at(boardIndex)->getSummary<Occupancy, Occupancy>().fOccupancy;

    if(!occupanyDirectlyProportionalToDAC)
    {
        DetectorDataContainer* tmpPointer = previousDacList;
        previousDacList                   = currentDacList;
        currentDacList                    = tmpPointer;
    }
    // LOG (INFO) << BOLDBLUE << "START " << RESET;

    for(int iBit = numberOfBits - 1; iBit >= 0; --iBit)
    {
        LOG(INFO) << BOLDBLUE << "Bit number " << +iBit << " of " << dacName << RESET;
        for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(localDAC)
                    {
                        for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
                        {
                            if(occupanyDirectlyProportionalToDAC)
                                currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel) =
                                    previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel) + (1 << iBit);
                            else
                                currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel) =
                                    previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel) &
                                    (0xFFFF - (1 << iBit));
                        }
                    }
                    else
                    {
                        if(occupanyDirectlyProportionalToDAC)
                            currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() + (1 << iBit);
                        else
                            currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() & (0xFFFF - (1 << iBit));
                        // LOG (INFO) << BOLDBLUE << minDAC<<"
                        // "<<previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()
                        // << RESET; LOG (INFO) << BOLDBLUE <<
                        // std::max(minDAC,previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()
                        // & (0xFFFF - (1<<iBit))) << RESET;
                    }
                }
            }
        }

        if(localDAC)
            setAllLocalDacBeBoard(boardIndex, dacName, *currentDacList);
        else
            setAllGlobalDacBeBoard(boardIndex, dacName, *currentDacList);

        fDetectorDataContainer = currentStepOccupancyContainer;
        measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);

        // Determine if it is better or not
        for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(localDAC)
                    {
                        for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
                        {
                            // LOG (INFO) << BOLDBLUE << "localocc "<<
                            // currentStepOccupancyContainer->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(iChannel).fOccupancy<<
                            // RESET;

                            if(currentStepOccupancyContainer->at(boardIndex)
                                   ->at(cOpticalGroup->getIndex())
                                   ->at(cHybrid->getIndex())
                                   ->at(cChip->getIndex())
                                   ->getChannel<Occupancy>(iChannel)
                                   .fOccupancy <= targetOccupancy)
                            {
                                previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel) =
                                    currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(iChannel);
                                previousStepOccupancyContainer->at(boardIndex)
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getChannel<Occupancy>(iChannel)
                                    .fOccupancy = currentStepOccupancyContainer->at(boardIndex)
                                                      ->at(cOpticalGroup->getIndex())
                                                      ->at(cHybrid->getIndex())
                                                      ->at(cChip->getIndex())
                                                      ->getChannel<Occupancy>(iChannel)
                                                      .fOccupancy;
                            }
                        }
                    }
                    else
                    {
                        // LOG (INFO) << BOLDBLUE << "globalocc
                        // "<<currentStepOccupancyContainer->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Occupancy,Occupancy>().fOccupancy<<
                        // RESET;

                        if(currentStepOccupancyContainer->at(boardIndex)
                               ->at(cOpticalGroup->getIndex())
                               ->at(cHybrid->getIndex())
                               ->at(cChip->getIndex())
                               ->getSummary<Occupancy, Occupancy>()
                               .fOccupancy <= targetOccupancy)
                        {
                            previousDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                currentDacList->at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                            previousStepOccupancyContainer->at(boardIndex)
                                ->at(cOpticalGroup->getIndex())
                                ->at(cHybrid->getIndex())
                                ->at(cChip->getIndex())
                                ->getSummary<Occupancy, Occupancy>()
                                .fOccupancy = currentStepOccupancyContainer->at(boardIndex)
                                                  ->at(cOpticalGroup->getIndex())
                                                  ->at(cHybrid->getIndex())
                                                  ->at(cChip->getIndex())
                                                  ->getSummary<Occupancy, Occupancy>()
                                                  .fOccupancy;
                        }
                    }
                }
            }
        }
    }

    if(localDAC) { setAllLocalDacBeBoard(boardIndex, dacName, *previousDacList); }
    else
        setAllGlobalDacBeBoard(boardIndex, dacName, *previousDacList);

    fDetectorDataContainer = outputDataContainer;
    measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);

    delete previousStepOccupancyContainer;
    delete currentStepOccupancyContainer;
    delete previousDacList;
    delete currentDacList;

    return;
}

// set dac and measure occupancy
void Tool::setDacAndMeasureData(const std::string& dacName, const uint16_t dacValue, uint32_t numberOfEvents, int32_t numberOfEventsPerBurst)
{
    for(uint16_t boardIndex = 0; boardIndex < fDetectorContainer->size(); boardIndex++) { setDacAndMeasureBeBoardData(boardIndex, dacName, dacValue, numberOfEvents, numberOfEventsPerBurst); }

    return;
}

// set dac and measure occupancy per BeBoard
void Tool::setDacAndMeasureBeBoardData(uint16_t boardIndex, const std::string& dacName, const uint16_t dacValue, uint32_t numberOfEvents, int32_t numberOfEventsPerBurst)
{
    setSameDacBeBoard(fDetectorContainer->at(boardIndex), dacName, dacValue);
    measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);
    return;
}

// measure occupancy
void Tool::measureData(uint32_t numberOfEvents, int32_t numberOfEventsPerBurst)
{
    for(unsigned int boardIndex = 0; boardIndex < fDetectorContainer->size(); boardIndex++) measureBeBoardData(boardIndex, numberOfEvents, numberOfEventsPerBurst);
}

class ScanBase
{
  public:
    ScanBase(Tool* theTool) : fTool(theTool) { ; }
    virtual ~ScanBase() { ; }

    virtual void operator()() = 0;
    void         setGroup(const ChannelGroupBase* cTestChannelGroup) { fTestChannelGroup = cTestChannelGroup; }
    void         setBoardId(uint16_t boardIndex) { fBoardIndex = boardIndex; }
    void         setNumberOfEvents(uint32_t numberOfEvents) { fNumberOfEvents = numberOfEvents; }
    void         setNumberOfEventsPerBurst(int32_t numberOfEventsPerBurst) { fNumberOfEventsPerBurst = numberOfEventsPerBurst; }

    void setDetectorContainer(DetectorContainer* detectorContainer) { fDetectorContainer = detectorContainer; }

  protected:
    uint32_t                fNumberOfEvents;
    int32_t                 fNumberOfEventsPerBurst{-1};
    uint32_t                fNumberOfMSec;
    uint32_t                fBoardIndex;
    const ChannelGroupBase* fTestChannelGroup;
    Tool*                   fTool;
    DetectorContainer*      fDetectorContainer;
};

void Tool::doScanOnAllGroupsBeBoard(uint16_t boardIndex, uint32_t numberOfEvents, int32_t numberOfEventsPerBurst, ScanBase* groupScan)
{
    groupScan->setBoardId(boardIndex);
    groupScan->setNumberOfEvents(numberOfEvents);
    groupScan->setDetectorContainer(fDetectorContainer);
    groupScan->setNumberOfEventsPerBurst(numberOfEventsPerBurst);
    // std::cout<<"groupScan "<<std::endl;
    if(fChannelGroupHandler == nullptr)
    {
        std::cout << __PRETTY_FUNCTION__ << " fChannelGroupHandler was not initialized!!! Aborting..." << std::endl;
        abort();
    }
    if(!fAllChan)
    {
        for(auto group: *fChannelGroupHandler)
        {
            if(fMaskChannelsFromOtherGroups || fTestPulse)
            {
                for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid) { fReadoutChipInterface->maskChannelsAndSetInjectionSchema(cChip, group, fMaskChannelsFromOtherGroups, fTestPulse); }
                    }
                }
            }
            groupScan->setGroup(group);
            (*groupScan)();
            // this->sendData();
        }

        if(fMaskChannelsFromOtherGroups) // re-enable all the channels and evaluate
        {
            for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid) { fReadoutChipInterface->ConfigureChipOriginalMask(cChip); }
                }
            }
        }
    }
    else
    {
        groupScan->setGroup(fChannelGroupHandler->allChannelGroup());
        (*groupScan)();
    }
    // It need to be moved into the place the loop on boards is done
    // fDetectorDataContainer->at(boardIndex)->normalizeAndAverageContainers(fDetectorContainer->at(boardIndex),
    // fChannelGroupHandler->allChannelGroup(), numberOfEvents);
}

class MeasureBeBoardDataPerGroup : public ScanBase
{
  public:
    MeasureBeBoardDataPerGroup(Tool* theTool) : ScanBase(theTool) { ; }
    ~MeasureBeBoardDataPerGroup() { ; }

    void operator()() override
    {
        uint32_t burstNumbers;
        uint32_t lastBurstNumberOfEvents;
        if(fNumberOfEventsPerBurst <= 0)
        {
            burstNumbers            = 1;
            lastBurstNumberOfEvents = fNumberOfEvents;
        }
        else
        {
            burstNumbers            = fNumberOfEvents / fNumberOfEventsPerBurst;
            lastBurstNumberOfEvents = fNumberOfEventsPerBurst;
            if(fNumberOfEvents % fNumberOfEventsPerBurst > 0)
            {
                ++burstNumbers;
                lastBurstNumberOfEvents = fNumberOfEvents % fNumberOfEventsPerBurst;
            }
        }

        while(burstNumbers > 0)
        {
            uint32_t currentNumberOfEvents = uint32_t(fNumberOfEventsPerBurst);
            if(burstNumbers == 1) currentNumberOfEvents = lastBurstNumberOfEvents;
            fTool->ReadNEvents(fDetectorContainer->at(fBoardIndex), currentNumberOfEvents);
            // Loop over Events from this Acquisition
            const std::vector<Event*>& events = fTool->GetEvents();
            for(auto& event: events) event->fillDataContainer((fDetectorDataContainer->at(fBoardIndex)), fTestChannelGroup);
            --burstNumbers;
        }
    }

    void setDataContainer(DetectorDataContainer* detectorDataContainer) { fDetectorDataContainer = detectorDataContainer; }

  private:
    DetectorDataContainer* fDetectorDataContainer;
};

void Tool::measureBeBoardData(uint16_t boardIndex, uint32_t numberOfEvents, int32_t numberOfEventsPerBurst)
{
    MeasureBeBoardDataPerGroup theScan(this);
    theScan.setDataContainer(fDetectorDataContainer);

    doScanOnAllGroupsBeBoard(boardIndex, numberOfEvents, numberOfEventsPerBurst, &theScan);
    // if in async mode normalization is a little different ..
    // normalize by the number of triggers to accept
    if(fDetectorContainer->at(boardIndex)->getEventType() == EventType::SSAAS || fDetectorContainer->at(boardIndex)->getEventType() == EventType::MPAAS)
    { numberOfEvents = fBeBoardInterface->ReadBoardReg(fDetectorContainer->at(boardIndex), "fc7_daq_cnfg.fast_command_block.triggers_to_accept"); }
    fDetectorDataContainer->normalizeAndAverageContainers(fDetectorContainer, fChannelGroupHandler->allChannelGroup(), numberOfEvents);
}

class ScanBeBoardDacPerGroup : public MeasureBeBoardDataPerGroup
{
  public:
    ScanBeBoardDacPerGroup(Tool* theTool) : MeasureBeBoardDataPerGroup(theTool) { ; }
    ~ScanBeBoardDacPerGroup() { ; }

    void operator()() override
    {
        for(size_t dacIt = 0; dacIt < fDacList->size(); ++dacIt)
        {
            fTool->setSameDacBeBoard(static_cast<BeBoard*>(fDetectorContainer->at(fBoardIndex)), fDacName, fDacList->at(dacIt));
            setDataContainer(fDetectorDataContainerVector->at(dacIt));
            MeasureBeBoardDataPerGroup::operator()();
        }
    }

    void setDataContainerVector(std::vector<DetectorDataContainer*>* detectorDataContainerVector) { fDetectorDataContainerVector = detectorDataContainerVector; }
    void setDacName(const std::string& dacName) { fDacName = dacName; }
    void setDacList(const std::vector<uint16_t>* dacList) { fDacList = dacList; }

  private:
    std::vector<DetectorDataContainer*>* fDetectorDataContainerVector;
    const std::vector<uint16_t>*         fDacList;
    std::string                          fDacName;
};

#define USE_OLD_GROUP_SCAN

#ifndef USE_OLD_GROUP_SCAN
// One dimensional dac scan per BeBoard
void Tool::scanBeBoardDac(uint16_t                             boardIndex,
                          const std::string&                   dacName,
                          const std::vector<uint16_t>&         dacList,
                          uint32_t                             numberOfEvents,
                          std::vector<DetectorDataContainer*>& detectorContainerVector,
                          int32_t                              numberOfEventsPerBurst)
{
    if(dacList.size() != detectorContainerVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    ScanBeBoardDacPerGroup theScan(this);
    theScan.setDataContainerVector(&detectorContainerVector);
    theScan.setDacName(dacName);
    theScan.setDacList(&dacList);

    doScanOnAllGroupsBeBoard(boardIndex, numberOfEvents, numberOfEventsPerBurst, &theScan);

    for(auto container: detectorContainerVector) container->normalizeAndAverageContainers(fDetectorContainer, fChannelGroupHandler->allChannelGroup(), numberOfEvents);

    return;
}

#else
// One dimensional dac scan per BeBoard
void Tool::scanBeBoardDac(uint16_t                             boardIndex,
                          const std::string&                   dacName,
                          const std::vector<uint16_t>&         dacList,
                          uint32_t                             numberOfEvents,
                          std::vector<DetectorDataContainer*>& detectorContainerVector,
                          int32_t                              numberOfEventsPerBurst)
{
    if(dacList.size() != detectorContainerVector.size())
    {
        LOG(ERROR) << __PRETTY_FUNCTION__ << " dacList and detector container vector have different sizes, aborting";
        abort();
    }

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        fDetectorDataContainer = detectorContainerVector[dacIt];
        setDacAndMeasureBeBoardData(boardIndex, dacName, dacList[dacIt], numberOfEvents, numberOfEventsPerBurst);
        this->sendData();
    }

    return;
}
#endif

// Set global DAC for all CBCs in the BeBoard
void Tool::setAllGlobalDacBeBoard(uint16_t boardIndex, const std::string& dacName, DetectorDataContainer& globalDACContainer)
{
    for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                fReadoutChipInterface->WriteChipReg(
                    cChip, dacName, globalDACContainer.at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>());
            }
        }
    }
    return;
}

// set local dac per BeBoard
void Tool::setAllLocalDacBeBoard(uint16_t boardIndex, const std::string& dacName, DetectorDataContainer& globalDACContainer)
{
    for(auto cOpticalGroup: *(fDetectorContainer->at(boardIndex)))
        for(auto cHybrid: *cOpticalGroup)
            for(auto cChip: *cHybrid)
                fReadoutChipInterface->WriteChipAllLocalReg(cChip, dacName, *globalDACContainer.at(boardIndex)->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
}

// Set same global DAC for all chips
void Tool::setSameGlobalDac(const std::string& dacName, const uint16_t dacValue)
{
    for(auto cBoard: *fDetectorContainer) setSameGlobalDacBeBoard(static_cast<BeBoard*>(cBoard), dacName, dacValue);
}

// Set same global DAC for all chips in the BeBoard
void Tool::setSameGlobalDacBeBoard(BeBoard* pBoard, const std::string& dacName, const uint16_t dacValue)
{
    if(fDoBoardBroadcast == false)
        for(auto cOpticalGroup: *pBoard)
            for(auto cHybrid: *cOpticalGroup)
                if(fDoHybridBroadcast == false)
                    for(auto cChip: *cHybrid) fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), dacName, dacValue);
                else
                    fReadoutChipInterface->WriteHybridBroadcastChipReg(static_cast<Hybrid*>(cHybrid), dacName, dacValue);
    else
        fReadoutChipInterface->WriteBoardBroadcastChipReg(pBoard, dacName, dacValue);
}

// set same local dac for all BeBoard
void Tool::setSameLocalDac(const std::string& dacName, const uint16_t dacValue)
{
    for(auto cBoard: *fDetectorContainer) { setSameLocalDacBeBoard(static_cast<BeBoard*>(cBoard), dacName, dacValue); }

    return;
}

// set same local dac per BeBoard
void Tool::setSameLocalDacBeBoard(BeBoard* pBoard, const std::string& dacName, const uint16_t dacValue)
{
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            for(auto cChip: *cHybrid)
            {
                ChannelContainer<uint16_t>* dacVector = new ChannelContainer<uint16_t>(cChip->getNumberOfChannels(), dacValue);
                ChipContainer               theChipContainer(cChip->getIndex(), cChip->getNumberOfRows(), cChip->getNumberOfCols());
                theChipContainer.setChannelContainer(dacVector);

                fReadoutChipInterface->WriteChipAllLocalReg(cChip, dacName, theChipContainer);
            }
        }
    }
    return;
}

void Tool::setSameDacBeBoard(BeBoard* pBoard, const std::string& dacName, const uint16_t dacValue)
{
    // Assumption: 1 BeBoard has only 1 chip flavor:
    if(static_cast<ReadoutChip*>(pBoard->at(0)->at(0)->at(0))->isDACLocal(dacName)) { setSameLocalDacBeBoard(pBoard, dacName, dacValue); }
    else
    {
        setSameGlobalDacBeBoard(pBoard, dacName, dacValue);
    }
}

void Tool::setSameDac(const std::string& dacName, const uint16_t dacValue)
{
    for(auto cBoard: *fDetectorContainer) { setSameDacBeBoard(static_cast<BeBoard*>(cBoard), dacName, dacValue); }

    return;
}

std::string Tool::getCalibrationName(void)
{
    int32_t     status;
    std::string className     = abi::__cxa_demangle(typeid(*this).name(), 0, 0, &status);
    std::string emptyTemplate = "<> ";
    size_t      found         = className.find(emptyTemplate);
    while(found != std::string::npos)
    {
        className.erase(found, emptyTemplate.length());
        found = className.find(emptyTemplate);
    }
    return className;
}
