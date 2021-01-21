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

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart        = this->findValueInSettings("ROWstart");
    rowStop         = this->findValueInSettings("ROWstop");
    colStart        = this->findValueInSettings("COLstart");
    colStop         = this->findValueInSettings("COLstop");
    nEvents         = this->findValueInSettings("nEvents");
    VCalStart       = this->findValueInSettings("VCalHstart");
    VCalStop        = this->findValueInSettings("VCalHstop");
    targetThreshold = this->findValueInSettings("TargetThr");
    ThrStart        = this->findValueInSettings("ThrStart");
    ThrStop         = this->findValueInSettings("ThrStop");
    doDisplay       = this->findValueInSettings("DisplayHisto");
    doUpdateChip    = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData  = this->findValueInSettings("SaveBinaryData");

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

void ThrAdjustment::Start(int currentRun)
{
    LOG(INFO) << GREEN << "[ThrAdjustment::Start] Starting" << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrAdjustment.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrAdjustment::run();
    ThrAdjustment::analyze();
    ThrAdjustment::saveChipRegisters(currentRun);
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
    this->closeFileHandler();
}

void ThrAdjustment::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos             = nullptr;
    PixelAlive::histos = nullptr;
#endif

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

    if(saveBinaryData == true)
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
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    theThrContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<RD53*>(cChip)->getReg(frontEnd->thresholdReg);

    // ################
    // # Error report #
    // ################
    ThrAdjustment::chipErrorReport();
}

void ThrAdjustment::draw(int currentRun)
{
    ThrAdjustment::saveChipRegisters(currentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> ThrAdjustment saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    ThrAdjustment::fillHisto();
    histos->process();

    PixelAlive::draw(-1);

    this->WriteRootFile();
    this->CloseResultFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrAdjustment::analyze()
{
    for(const auto cBoard: theThrContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    LOG(INFO) << GREEN << "Global threshold for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cModule->getId() << "/"
                              << cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << cChip->getSummary<uint16_t>() << RESET;
}

void ThrAdjustment::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theThrContainer);
#endif
}

void ThrAdjustment::bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue)
{
    uint16_t init;
    uint16_t numberOfBits = log2(stopValue - startValue + 1) + 1;

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

    for(const auto cBoard: bestContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule) cChip->getSummary<float>() = 0;

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() +
                             maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()) /
                            2;

                        this->fReadoutChipInterface->WriteChipReg(
                            static_cast<RD53*>(cChip),
                            regName,
                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(),
                            true);
                    }

        // ################
        // # Run analysis #
        // ################
        auto output = ThrAdjustment::bitWiseScanGlobal_MeasureThr("VCAL_HIGH", nEvents, TARGETEFF, VCalStart, VCalStop);

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float newValue = RD53chargeConverter::VCAl2Charge(
                            cChip->getSummary<uint16_t>() -
                            static_cast<RD53*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex()))->getReg("VCAL_MED"));

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<float>();

                        if(fabs(newValue - target) < fabs(oldValue - target))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<float>() = newValue;

                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if(newValue > target)

                            maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();

                        else

                            minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    this->fReadoutChipInterface->WriteChipReg(
                        static_cast<RD53*>(cChip),
                        regName,
                        bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(),
                        true);

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();
}

std::shared_ptr<DetectorDataContainer> ThrAdjustment::bitWiseScanGlobal_MeasureThr(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue)
{
    uint16_t init;
    uint16_t numberOfBits = log2(stopValue - startValue + 1) + 1;

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;

    std::shared_ptr<DetectorDataContainer> bestDACcontainer = std::shared_ptr<DetectorDataContainer>(new DetectorDataContainer());
    DetectorDataContainer                  bestContainer;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, minDACcontainer, init = startValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, maxDACcontainer, init = (stopValue + 1));

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *bestDACcontainer);
    ContainerFactory::copyAndInitChip<OccupancyAndPh>(*fDetectorContainer, bestContainer);

    for(const auto cBoard: bestContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule) cChip->getSummary<OccupancyAndPh>().fPh = 0;

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() +
                             maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()) /
                            2;

                        this->fReadoutChipInterface->WriteChipReg(
                            static_cast<RD53*>(cChip),
                            regName,
                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>(),
                            true);
                    }

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();
        output->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                    {
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float newValue = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<OccupancyAndPh>().fPh;

                        if(fabs(newValue - target) < fabs(oldValue - target))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<OccupancyAndPh>().fPh = newValue;

                            bestDACcontainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if(newValue > target)

                            maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();

                        else

                            minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                    }
    }

    return bestDACcontainer;
}

void ThrAdjustment::chipErrorReport()
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

void ThrAdjustment::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> ThrAdjustment saved the configuration file for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cModule->getId() << "/" << cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
