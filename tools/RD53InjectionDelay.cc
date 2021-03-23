/*!
  \file                  RD53InjectionDelay.cc
  \brief                 Implementaion of Injection Delay scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53InjectionDelay.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void InjectionDelay::ConfigureCalibration()
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
    startValue     = 0;
    stopValue      = RD53Shared::NLATENCYBINS * (RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY")) + 1) - 1;
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const size_t nSteps = (stopValue - startValue + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1 ? stopValue - startValue + 1 : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    const float  step   = (stopValue - startValue + 1) / nSteps;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + step * i);

    // ######################
    // # Initialize Latency #
    // ######################
    la.Inherit(this);
    la.localConfigure("", -1);

    // ##############################
    // # Injection register masking #
    // ##############################
    saveInjection = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT")) -
                    RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY"));
    maxDelay = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY"));

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += InjectionDelay::getNumberIterations();
}

void InjectionDelay::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[InjectionDelay::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_InjectionDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

    InjectionDelay::run();
    InjectionDelay::analyze();
    InjectionDelay::saveChipRegisters(theCurrentRun);
    InjectionDelay::sendData();

    la.sendData();
}

void InjectionDelay::sendData()
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theStream               = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<InjDelaySize>>("Occ");
    auto theInjectionDelayStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>("InjDelay");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theInjectionDelayContainer) theInjectionDelayStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void InjectionDelay::Stop()
{
    LOG(INFO) << GREEN << "[InjectionDelay::Stop] Stopping" << RESET;

    Tool::Stop();

    InjectionDelay::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void InjectionDelay::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[InjectionDelay::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    InjectionDelay::ConfigureCalibration();
    InjectionDelay::initializeFiles(fileRes_, currentRun);
}

void InjectionDelay::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_InjectionDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new InjectionDelayHistograms;
#endif

    // ######################
    // # Initialize Latency #
    // ######################
    std::string fileName = fileRes;
    fileName.replace(fileRes.find("_InjectionDelay"), 15, "_Latency");
    la.initializeFiles(fileName, -1);
}

void InjectionDelay::run()
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    // ###############
    // # Run Latency #
    // ###############
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT", val & saveInjection, true);
                }
    la.run();
    la.analyze();

    ContainerFactory::copyAndInitChip<GenericDataArray<InjDelaySize>>(*fDetectorContainer, theOccContainer);

    // #######################
    // # Set Initial latency #
    // #######################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency - 1, true);

                    for(auto i = 0u; i < InjDelaySize; i++)
                        theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<InjDelaySize>>().data[i] = 0;
                }

    // ###############################
    // # Scan two adjacent latencies #
    // ###############################
    for(auto i = 0; i < 2; i++)
    {
        std::vector<uint16_t> halfDacList(dacList.begin() + i * (dacList.end() - dacList.begin()) / 2, dacList.begin() + (i + 1) * (dacList.end() - dacList.begin()) / 2);

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency + i, true);
                    }

        InjectionDelay::scanDac("INJECTION_SELECT", halfDacList, nEvents, &theOccContainer);
    }

    // ################
    // # Error report #
    // ################
    InjectionDelay::chipErrorReport();
}

void InjectionDelay::draw()
{
    InjectionDelay::saveChipRegisters(theCurrentRun);
    la.draw(false);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> InjectionDelay saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    InjectionDelay::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    this->CloseResultFile();
#endif
}

void InjectionDelay::analyze()
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theInjectionDelayContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto best   = 0.;
                    auto regVal = 0;

                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        auto current =
                            theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<InjDelaySize>>().data[i];
                        if(current > best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best injection delay for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " (1.5625 ns) computed over two bx <<<" << RESET;
                    LOG(INFO) << BOLDMAGENTA << ">>> New injection delay dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << (regVal & maxDelay) << BOLDMAGENTA << " <<<" << RESET;

                    // ####################################################
                    // # Fill delay container and download new DAC values #
                    // ####################################################
                    theInjectionDelayContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT", (val & saveInjection) | (regVal & maxDelay), true);

                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    if(regVal / (maxDelay + 1) == 0) latency--;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency, true);
                    LOG(INFO) << BOLDMAGENTA << ">>> New latency dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << latency << BOLDMAGENTA << " <<<" << RESET;
                }
}

void InjectionDelay::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillInjectionDelay(theInjectionDelayContainer);
#endif
}

void InjectionDelay::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer)
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(auto i = 0u; i < dacList.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), regName);
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), regName, (val & saveInjection) | (dacList[i] & maxDelay), true);
                    }

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
                        theContainer->at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<InjDelaySize>>()
                            .data[dacList[i]] = occ;
                    }

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        InjectionDelay::sendData();
    }
}

void InjectionDelay::chipErrorReport()
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

void InjectionDelay::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> InjectionDelay saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
