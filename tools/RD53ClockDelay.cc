/*!
  \file                  RD53ClockDelay.cc
  \brief                 Implementaion of Clock Delay scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ClockDelay.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ClockDelay::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::ConfigureCalibration();
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings<double>("ROWstart");
    rowStop        = this->findValueInSettings<double>("ROWstop");
    colStart       = this->findValueInSettings<double>("COLstart");
    colStop        = this->findValueInSettings<double>("COLstop");
    nEvents        = this->findValueInSettings<double>("nEvents");
    doFast         = this->findValueInSettings<double>("DoFast");
    startValue     = 0;
    stopValue      = RD53Shared::NLATENCYBINS * (RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CLK_DELAY")) + 1) - 1;
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip   = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings<double>("SaveBinaryData");

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
    la.localConfigure();

    // ##################
    // # Register masks #
    // ##################
    maxClkDelay = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CLK_DELAY"));
    maxCmdDelay = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CMD_DELAY"));

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ClockDelay::getNumberIterations();
}

void ClockDelay::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ClockDelay::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_ClockDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ClockDelay::run();
    ClockDelay::analyze();
    ClockDelay::saveChipRegisters(theCurrentRun);
    ClockDelay::sendData();

    la.sendData();
}

void ClockDelay::sendData()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theStream           = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<ClkDelaySize>>("Occ");
    auto theClockDelayStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>("ClkDelay");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theClockDelayContainer) theClockDelayStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void ClockDelay::Stop()
{
    LOG(INFO) << GREEN << "[ClockDelay::Stop] Stopping" << RESET;

    Tool::Stop();

    ClockDelay::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void ClockDelay::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[ClockDelay::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    ClockDelay::ConfigureCalibration();
    ClockDelay::initializeFiles(fileRes_, currentRun);
}

void ClockDelay::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ClockDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new ClockDelayHistograms;
#endif

    // ######################
    // # Initialize Latency #
    // ######################
    std::string fileName = fileRes;
    fileName.replace(fileRes.find("_ClockDelay"), 15, "_Latency");
    la.initializeFiles(fileName);
}

void ClockDelay::run()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    // ###############
    // # Run Latency #
    // ###############
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    uint8_t phase, clock_delay, cmd_delay;
                    std::tie(phase, clock_delay, cmd_delay) = bits::unpack<1, 4, 4>(this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY"));
                    cmd_delay                               = cmd_delay - clock_delay;
                    uint16_t clk_data_delay                 = (uint16_t)bits::pack<1, 4, 4>(phase, 0, cmd_delay);
                    ClockDelay::writeSequence(cBoard, cChip, clk_data_delay);
                }
    la.run();
    la.analyze();

    ContainerFactory::copyAndInitChip<GenericDataArray<ClkDelaySize>>(*fDetectorContainer, theOccContainer);

    // #######################
    // # Set Initial latency #
    // #######################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency - 1);

                    for(auto i = 0u; i < ClkDelaySize; i++)
                        theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<ClkDelaySize>>().data[i] = 0;
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
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency + i);
                    }

        ClockDelay::scanDac("CLK_DATA_DELAY", halfDacList, nEvents, &theOccContainer);
    }

    // ################
    // # Error report #
    // ################
    ClockDelay::chipErrorReport();
}

void ClockDelay::draw()
{
    ClockDelay::saveChipRegisters(theCurrentRun);
    la.draw(false);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    ClockDelay::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ClockDelay::analyze()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theClockDelayContainer);

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
                            theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<ClkDelaySize>>().data[i];
                        if(current > best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best clock delay for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " (1.5625 ns) computed over two bx <<<" << RESET;
                    LOG(INFO) << BOLDMAGENTA << ">>> New clock delay dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << (regVal & maxClkDelay) << BOLDMAGENTA << " <<<" << RESET;

                    // ####################################################
                    // # Fill delay container and download new DAC values #
                    // ####################################################
                    theClockDelayContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    uint8_t phase, clock_delay, cmd_delay;
                    std::tie(phase, clock_delay, cmd_delay) = bits::unpack<1, 4, 4>(this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY"));
                    cmd_delay                               = (regVal + cmd_delay - clock_delay) & maxCmdDelay;
                    clock_delay                             = regVal & maxClkDelay;
                    uint16_t clk_data_delay                 = (uint16_t)bits::pack<1, 4, 4>(phase, clock_delay, cmd_delay);
                    ClockDelay::writeSequence(cBoard, cChip, clk_data_delay);

                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    if(regVal / (maxClkDelay + 1) == 0) latency--;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency);
                    LOG(INFO) << BOLDMAGENTA << ">>> New latency dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << latency << BOLDMAGENTA << " <<<" << RESET;
                }
}

void ClockDelay::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillClockDelay(theClockDelayContainer);
#endif
}

void ClockDelay::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer)
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

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
                        uint8_t phase, clock_delay, cmd_delay;
                        std::tie(phase, clock_delay, cmd_delay) = bits::unpack<1, 4, 4>(this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), regName));
                        cmd_delay                               = (dacList[i] + cmd_delay - clock_delay) & maxCmdDelay;
                        clock_delay                             = dacList[i] & maxClkDelay;
                        uint16_t clk_data_delay                 = (uint16_t)bits::pack<1, 4, 4>(phase, clock_delay, cmd_delay);
                        ClockDelay::writeSequence(cBoard, cChip, clk_data_delay);
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
                        theContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<ClkDelaySize>>().data[i] = occ;
                    }

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        ClockDelay::sendData();
    }
}

void ClockDelay::chipErrorReport() const
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->ChipErrorReport(cChip);
                }
}

void ClockDelay::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}

void ClockDelay::writeSequence(const Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::ReadoutChip* pChip, uint16_t clk_data_delay)
{
    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(pChip), "CLK_DATA_DELAY", clk_data_delay, false);
    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[pBoard->getId()])->WriteChipCommand(std::vector<uint16_t>(RD53Constants::NSYNC_WORS, RD53CmdEncoder::SYNC), -1);
    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(pChip), "CLK_DATA_DELAY", clk_data_delay);
}
