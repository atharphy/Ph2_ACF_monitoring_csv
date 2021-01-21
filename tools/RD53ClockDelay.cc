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

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    doFast         = this->findValueInSettings("DoFast");
    startValue     = 0;
    stopValue      = RD53Shared::NLATENCYBINS * (RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CLK_DELAY")) + 1) - 1;
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

    // ##########################
    // # Clock register masking #
    // ##########################
    shiftData = static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_DATA_DELAY");
    saveData  = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY")) -
               (RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CLK_DELAY")) << shiftData);
    maxDelay = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("CLK_DATA_DELAY_CLK_DELAY"));

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ClockDelay::getNumberIterations();
}

void ClockDelay::Start(int currentRun)
{
    LOG(INFO) << GREEN << "[ClockDelay::Start] Starting" << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ClockDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ClockDelay::run();
    ClockDelay::analyze();
    ClockDelay::sendData();

    la.draw(currentRun);
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
    this->closeFileHandler();
}

void ClockDelay::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    ClockDelay::ConfigureCalibration();
    ClockDelay::initializeFiles(fileRes_, currentRun);
}

void ClockDelay::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if(saveBinaryData == true)
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
    la.initializeFiles(fileName, -1);
}

void ClockDelay::run()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    // ###############
    // # Run Latency #
    // ###############
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY", val & saveData, true);
                }
    la.run();
    la.analyze();

    ContainerFactory::copyAndInitChip<GenericDataArray<ClkDelaySize>>(*fDetectorContainer, theOccContainer);

    // #######################
    // # Set Initial latency #
    // #######################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency - 1, true);
                }

    // ###############################
    // # Scan two adjacent latencies #
    // ###############################
    for(auto i = 0; i < 2; i++)
    {
        std::vector<uint16_t> halfDacList(dacList.begin() + i * (dacList.end() - dacList.begin()) / 2, dacList.begin() + (i + 1) * (dacList.end() - dacList.begin()) / 2);

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency + i, true);
                    }

        ClockDelay::scanDac("CLK_DATA_DELAY", halfDacList, nEvents, &theOccContainer);
    }

    // ################
    // # Error report #
    // ################
    ClockDelay::chipErrorReport();
}

void ClockDelay::draw(int currentRun)
{
    la.draw(currentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    ClockDelay::fillHisto();
    histos->process();

    this->WriteRootFile();
    this->CloseResultFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ClockDelay::analyze()
{
    const size_t ClkDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theClockDelayContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    auto best   = 0.;
                    auto regVal = 0;

                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        auto current =
                            theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<ClkDelaySize>>().data[i];
                        if(current > best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << GREEN << "Best delay for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cModule->getId() << "/"
                              << cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << regVal << RESET << GREEN << " (1.5625 ns) computed over two bx" << RESET;
                    LOG(INFO) << GREEN << "New delay dac value for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cModule->getId()
                              << "/" << cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << (regVal & maxDelay) << RESET;

                    // ####################################################
                    // # Fill delay container and download new DAC values #
                    // ####################################################
                    theClockDelayContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "CLK_DATA_DELAY", (val & saveData) | ((regVal & maxDelay) << shiftData), true);

                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG");
                    if(regVal / (maxDelay + 1) == 0) latency--;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "LATENCY_CONFIG", latency, true);
                    LOG(INFO) << GREEN << "New latency dac value for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cModule->getId()
                              << "/" << cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << latency << RESET;
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
        LOG(INFO) << BOLDMAGENTA << ">>> Register value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), regName);
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), regName, (val & saveData) | ((dacList[i] & maxDelay) << shiftData), true);
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
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        float occ = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;
                        theContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<ClkDelaySize>>().data[i] = occ;
                    }
    }
}

void ClockDelay::chipErrorReport()
{
    auto RD53ChipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cModule->getId() << "/" << cChip->getId() << RESET << GREEN << "]" << RESET;
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

void ClockDelay::saveChipRegisters(int currentRun)
{
    std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    static_cast<RD53*>(cChip)->copyMaskFromDefault();
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + RD53Shared::RESULTDIR);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> ClockDelay saved the configuration file for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cModule->getId() << "/" << cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
