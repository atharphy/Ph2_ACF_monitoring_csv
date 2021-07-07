/*!
  \file                  RD53ThrAdjustment.cc
  \brief                 Implementaion of threshold adjustment
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrAdjustment.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ThrAdjustment::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::ConfigureCalibration();
    PixelAlive::doDisplay    = false;
    PixelAlive::doUpdateChip = false;
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart        = this->findValueInSettings<double>("ROWstart");
    rowStop         = this->findValueInSettings<double>("ROWstop");
    colStart        = this->findValueInSettings<double>("COLstart");
    colStop         = this->findValueInSettings<double>("COLstop");
    nEvents         = this->findValueInSettings<double>("nEvents");
    targetThreshold = this->findValueInSettings<double>("TargetThr");
    ThrStart        = this->findValueInSettings<double>("ThrStart");
    ThrStop         = this->findValueInSettings<double>("ThrStop");
    doDisplay       = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip    = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData  = this->findValueInSettings<double>("SaveBinaryData");

    frontEnd = RD53::getMajorityFE(colStart, colStop);
    colStart = std::max(colStart, frontEnd->colStart);
    colStop  = std::min(colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrAdjustment will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << RESET << BOLDYELLOW << colStart << ", " << colStop << RESET
              << GREEN << "]" << RESET;

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrAdjustment::getNumberIterations();
}

void ThrAdjustment::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrAdjustment::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_ThrAdjustment.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrAdjustment::run();
    ThrAdjustment::analyze();
    ThrAdjustment::saveChipRegisters(theCurrentRun);
    ThrAdjustment::sendData();

    PixelAlive::sendData();
}

void ThrAdjustment::sendData()
{
    auto theThrStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>();

    if(fStreamerEnabled == true)
        for(const auto cBoard: theThrContainer) theThrStream.streamAndSendBoard(cBoard, fNetworkStreamer);
}

void ThrAdjustment::Stop()
{
    LOG(INFO) << GREEN << "[ThrAdjustment::Stop] Stopping" << RESET;

    Tool::Stop();

    ThrAdjustment::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void ThrAdjustment::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos             = nullptr;
    PixelAlive::histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[ThrAdjustment::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    ThrAdjustment::ConfigureCalibration();
    ThrAdjustment::initializeFiles(fileRes_, currentRun);
}

void ThrAdjustment::initializeFiles(const std::string fileRes_, int currentRun)
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::initializeFiles("", -1);

    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrAdjustment.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new ThresholdHistograms;
#endif
}

void ThrAdjustment::run()
{
    ThrAdjustment::bitWiseScanGlobal(frontEnd->thresholdReg, nEvents, targetThreshold, ThrStart, ThrStop);

    // ############################
    // # Fill threshold container #
    // ############################
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theThrContainer);
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    theThrContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<RD53*>(cChip)->getReg(frontEnd->thresholdReg);

    // ################
    // # Error report #
    // ################
    ThrAdjustment::chipErrorReport();
}

void ThrAdjustment::draw()
{
    ThrAdjustment::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> ThrAdjustment saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    ThrAdjustment::fillHisto();
    histos->process();

    PixelAlive::draw(false);

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrAdjustment::analyze()
{
    for(const auto cBoard: theThrContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    LOG(INFO) << GREEN << "Global threshold for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << cChip->getSummary<uint16_t>() << RESET;
}

void ThrAdjustment::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theThrContainer);
#endif
}

void ThrAdjustment::bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, float target, uint16_t startValue, uint16_t stopValue)
{
    std::vector<uint16_t> chipCommandList;
    std::vector<uint32_t> hybridCommandList;

    uint16_t init;
    uint16_t numberOfBits = floor(log2(stopValue - startValue + 1) + 1);

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;

    DetectorDataContainer bestDACcontainer;
    DetectorDataContainer bestContainer;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, minDACcontainer, init = startValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, maxDACcontainer, init = (stopValue + 1));

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, bestDACcontainer);
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, bestContainer);

    // #########################
    // # Initialize containers #
    // #########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = 0;
                    bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>()       = 0;
                }

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
            {
                hybridCommandList.clear();

                for(const auto cHybrid: *cOpticalGroup)
                {
                    chipCommandList.clear();
                    int hybridId = cHybrid->getId();

                    for(const auto cChip: *cHybrid)
                    {
                        // #########################################
                        // # Set VCAL_HIGH to get target threshold #
                        // #########################################
                        uint16_t vcal_med_setting =
                            static_cast<RD53*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()))->getReg("VCAL_MED");
                        uint16_t vcal_high_setting = round(RD53chargeConverter::Charge2VCal(target)) + vcal_med_setting;

                        static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackChipCommands(cChip, "VCAL_HIGH", vcal_high_setting, chipCommandList, true);

                        LOG(INFO) << GREEN << "The target threshold is " << std::setprecision(1) << BOLDYELLOW << target << RESET << GREEN << " electrons" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Closest charge setting is " << BOLDYELLOW << "VCAL_HIGH" << RESET << GREEN << " = " << BOLDYELLOW << vcal_high_setting << RESET << GREEN
                                  << " for " << BOLDYELLOW << "VCAL_MED" << RESET << GREEN << " = " << BOLDYELLOW << vcal_med_setting << std::setprecision(-1) << RESET;

                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() +
                             maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()) /
                            2;

                        static_cast<RD53Interface*>(this->fReadoutChipInterface)
                            ->PackChipCommands(cChip,
                                               regName,
                                               midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(),
                                               chipCommandList,
                                               true);

                        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                  << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDMAGENTA << "] = " << RESET << BOLDYELLOW
                                  << midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() << BOLDMAGENTA
                                  << " <<<" << RESET;
                    }

                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackHybridCommands(cBoard, chipCommandList, hybridId, hybridCommandList);
                }

                static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendHybridCommandsPack(cBoard, hybridCommandList);
            }

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();
        output->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        PixelAlive::sendData();

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float newValue = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>();

                        if(fabs(newValue - TARGETEFF) <= fabs(oldValue - TARGETEFF))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() = newValue;

                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if(newValue < TARGETEFF)

                            maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();

                        else

                            minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            hybridCommandList.clear();

            for(const auto cHybrid: *cOpticalGroup)
            {
                chipCommandList.clear();
                int hybridId = cHybrid->getId();

                for(const auto cChip: *cHybrid)
                    if(bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() != 0)
                    {
                        static_cast<RD53Interface*>(this->fReadoutChipInterface)
                            ->PackChipCommands(cChip,
                                               regName,
                                               bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(),
                                               chipCommandList,
                                               true);

                        LOG(INFO) << BOLDMAGENTA << ">>> Best " << BOLDYELLOW << regName << BOLDMAGENTA << " value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                  << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] = " << BOLDYELLOW
                                  << bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() << BOLDMAGENTA
                                  << " <<<" << RESET;
                    }
                    else
                        LOG(WARNING) << BOLDRED << ">>> Best " << BOLDYELLOW << regName << BOLDRED << " value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                     << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "] was not found <<<" << RESET;

                static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackHybridCommands(cBoard, chipCommandList, hybridId, hybridCommandList);
            }

            static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendHybridCommandsPack(cBoard, hybridCommandList);
        }

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();
}

void ThrAdjustment::chipErrorReport() const
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

void ThrAdjustment::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> ThrAdjustment saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
