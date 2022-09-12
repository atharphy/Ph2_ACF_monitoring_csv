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
    startValue = 0u;
    stopValue  = RD53Shared::NLATENCYBINS * (RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CLK_DATA_DELAY_CLK")) + 1) - 1;
    frontEnd   = RD53Shared::firstChip->getFEtype(PixelAlive::colStart, PixelAlive::colStop);

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const size_t nSteps = stopValue - startValue + 1;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + i);

    // ######################
    // # Initialize Latency #
    // ######################
    la.Inherit(this);
    la.localConfigure();

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ClockDelay::getNumberIterations();
}

void ClockDelay::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ClockDelay::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
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

    if(fDQMStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theClockDelayContainer) theClockDelayStream->streamAndSendBoard(cBoard, fDQMStreamer);
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

void ClockDelay::localConfigure(const std::string& fileRes_, int currentRun)
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
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false, "ClockDelay");
    ClockDelay::initializeFiles(fileRes_, currentRun);
}

void ClockDelay::initializeFiles(const std::string& fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (PixelAlive::saveBinaryData == true))
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
                for(const auto cChip: *cHybrid) ClockDelay::writeClkDelaySequence(cBoard, cChip, 0);
    la.run();
    la.analyze();

    ContainerFactory::copyAndInitChip<GenericDataArray<ClkDelaySize>>(*fDetectorContainer, theOccContainer);

    // #######################
    // # Set initial latency #
    // #######################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg);
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency - 1);

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
                        auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg);
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency + i);
                    }

        ClockDelay::scanDac("CLK_DATA_DELAY", halfDacList, &theOccContainer);
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

    if(PixelAlive::doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    ClockDelay::fillHisto();
    histos->process();

    if(PixelAlive::doDisplay == true) myApp->Run(true);
#endif
}

void ClockDelay::analyze()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;
    const size_t maxRegValue  = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CLK_DATA_DELAY_CLK")) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theClockDelayContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    size_t best   = 0u;
                    size_t regVal = 0u;

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
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal % maxRegValue << BOLDMAGENTA << " <<<" << RESET;

                    // ####################################################
                    // # Fill delay container and download new DAC values #
                    // ####################################################
                    ClockDelay::writeClkDelaySequence(cBoard, cChip, regVal);

                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg);
                    if(regVal < maxRegValue)
                    {
                        latency--;
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency);
                    }

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

void ClockDelay::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer)
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
                    for(const auto cChip: *cHybrid) ClockDelay::writeClkDelaySequence(cBoard, cChip, dacList[i]);

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();
        output->resetNormalizationStatus();
        output->normalizeAndAverageContainers(fDetectorContainer, this->getChannelGroupHandlerContainer(), 1);

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
        // # Send periodic data to monitor the progress #
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
                    if(PixelAlive::doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + this->fDirectoryName);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}

void ClockDelay::writeClkDelaySequence(const Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::ReadoutChip* pChip, uint16_t value)
{
    const size_t maxClkValue  = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CLK_DATA_DELAY_CLK")) + 1;
    const size_t maxDataValue = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CLK_DATA_DELAY_DATA")) + 1;

    // #################################################
    // # Move data and clock phases of the same amount #
    // #################################################
    auto clk_delay = pChip->getRegItem("CLK_DATA_DELAY_CLK").fValue;
    auto nameAndValue(static_cast<RD53Interface*>(this->fReadoutChipInterface)->SetSpecialRegister("CLK_DATA_DELAY_CLK", value % maxClkValue, RD53Shared::firstChip->getRegMap()));
    pChip->getRegItem("CLK_DATA_DELAY").fValue = nameAndValue.second;
    auto data_delay                            = (pChip->getRegItem("CLK_DATA_DELAY_DATA").fValue + (value % maxClkValue) - clk_delay) % maxDataValue; // Apply to data the same shift of the clock
    nameAndValue                               = static_cast<RD53Interface*>(this->fReadoutChipInterface)->SetSpecialRegister("CLK_DATA_DELAY_DATA", data_delay, RD53Shared::firstChip->getRegMap());

    pChip->getRegItem("CLK_DATA_DELAY_CLK").fValue  = value % maxClkValue;
    pChip->getRegItem("CLK_DATA_DELAY_DATA").fValue = data_delay;

    static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteClokDataDelay(pChip, nameAndValue.second);
}
