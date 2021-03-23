/*!
  \file                  RD53Latency.cc
  \brief                 Implementaion of Latency scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Latency.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void Latency::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::ConfigureCalibration();
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    nTRIGxEvent    = this->findValueInSettings("nTRIGxEvent");
    startValue     = this->findValueInSettings("LatencyStart");
    stopValue      = this->findValueInSettings("LatencyStop");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const size_t nSteps = ((stopValue - startValue) / nTRIGxEvent + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1 ? (stopValue - startValue) / nTRIGxEvent + 1
                                                                                                                            : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    const size_t step   = nTRIGxEvent;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + step * i);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += Latency::getNumberIterations();
}

void Latency::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[Latency::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Latency.raw", 'w');
        this->initializeWriteFileHandler();
    }

    Latency::run();
    Latency::analyze();
    Latency::saveChipRegisters(theCurrentRun);
    Latency::sendData();
}

void Latency::sendData()
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theStream        = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<LatencySize>>("Occ");
    auto theLatencyStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>("Latency");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theLatencyContainer) theLatencyStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void Latency::Stop()
{
    LOG(INFO) << GREEN << "[Latency::Stop] Stopping" << RESET;

    Tool::Stop();

    Latency::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void Latency::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[Latency::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    Latency::ConfigureCalibration();
    Latency::initializeFiles(fileRes_, currentRun);
}

void Latency::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_Latency.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new LatencyHistograms;
#endif
}

void Latency::run()
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<GenericDataArray<LatencySize>>(*fDetectorContainer, theOccContainer);
    Latency::scanDac("LATENCY_CONFIG", dacList, nEvents, &theOccContainer);

    // ################
    // # Error report #
    // ################
    Latency::chipErrorReport();
}

void Latency::draw(bool saveData)
{
    if(saveData == true) Latency::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> Latency saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    Latency::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    this->CloseResultFile();
#endif
}

void Latency::analyze()
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theLatencyContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto best   = 0.;
                    int  regVal = 0;

                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        auto current =
                            theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<LatencySize>>().data[i];
                        if(current > best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    if(nTRIGxEvent > 1)
                        LOG(INFO) << BOLDMAGENTA << ">>> Best latency for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is within [" << BOLDYELLOW
                                  << (regVal - (int)nTRIGxEvent + 1 >= 0 ? std::to_string(regVal - (int)nTRIGxEvent + 1) : "N.A.") << "," << regVal << BOLDMAGENTA << "] (n.bx) <<<" << RESET;
                    else
                        LOG(INFO) << BOLDMAGENTA << ">>> Best latency for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " (n.bx) <<<" << RESET;

                    // ######################################################
                    // # Fill latency container and download new DAC values #
                    // ######################################################
                    theLatencyContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", regVal, true);
                }
}

void Latency::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillLatency(theLatencyContainer);
#endif
}

void Latency::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer)
{
    const size_t LatencySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(auto i = 0u; i < dacList.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regName, dacList[i]);

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();
        output->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

        // ###############
        // # Save output #
        // ###############
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        float occ = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;
                        theContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<LatencySize>>().data[i] = occ;
                    }

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        Latency::sendData();
    }
}

void Latency::chipErrorReport()
{
    auto RD53ChipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    LOG(INFO) << BOLDBLUE << "LOCKLOSS_CNT        = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LOCKLOSS_CNT") << std::setfill(' ') << std::setw(8)
                              << "" << RESET;
                    LOG(INFO) << BOLDBLUE << "BITFLIP_WNG_CNT     = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "BITFLIP_WNG_CNT") << std::setfill(' ') << std::setw(8)
                              << "" << RESET;
                    LOG(INFO) << BOLDBLUE << "BITFLIP_ERR_CNT     = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "BITFLIP_ERR_CNT") << std::setfill(' ') << std::setw(8)
                              << "" << RESET;
                    LOG(INFO) << BOLDBLUE << "CMDERR_CNT          = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "CMDERR_CNT") << std::setfill(' ') << std::setw(8)
                              << "" << RESET;
                    LOG(INFO) << BOLDBLUE << "SKIPPED_TRIGGER_CNT = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "SKIPPED_TRIGGER_CNT") << std::setfill(' ')
                              << std::setw(8) << "" << RESET;
                    LOG(INFO) << BOLDBLUE << "BCID_CNT            = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "BCID_CNT") << std::setfill(' ') << std::setw(8) << ""
                              << RESET;
                    LOG(INFO) << BOLDBLUE << "TRIG_CNT            = " << BOLDYELLOW << RD53ChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "TRIG_CNT") << std::setfill(' ') << std::setw(8) << ""
                              << RESET;
                }
}

void Latency::saveChipRegisters(int currentRun)
{
    const std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    static_cast<RD53*>(cChip)->copyMaskFromDefault();
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + RD53Shared::RESULTDIR);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> Latency saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
