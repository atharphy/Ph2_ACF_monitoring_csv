/*!
  \file                  RD53ThrMinimization.cc
  \brief                 Implementaion of threshold minimization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrMinimization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ThrMinimization::ConfigureCalibration()
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
    targetOccupancy = this->findValueInSettings("TargetOcc");
    ThrStart        = this->findValueInSettings("ThrStart");
    ThrStop         = this->findValueInSettings("ThrStop");
    doDisplay       = this->findValueInSettings("DisplayHisto");
    doUpdateChip    = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData  = this->findValueInSettings("SaveBinaryData");

    frontEnd = RD53::getMajorityFE(colStart, colStop);
    colStart = std::max(colStart, frontEnd->colStart);
    colStop  = std::min(colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrMinimization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << BOLDYELLOW << colStart << ", " << colStop << RESET
              << GREEN << "]" << RESET;

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrMinimization::getNumberIterations();
}

void ThrMinimization::Start(int currentRun)
{
    LOG(INFO) << GREEN << "[ThrMinimization::Start] Starting" << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrMinimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrMinimization::run();
    ThrMinimization::analyze();
    ThrMinimization::saveChipRegisters(currentRun);
    ThrMinimization::sendData();

    PixelAlive::sendData();
}

void ThrMinimization::sendData()
{
    auto theThrStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>();

    if(fStreamerEnabled == true)
        for(const auto cBoard: theThrContainer) theThrStream.streamAndSendBoard(cBoard, fNetworkStreamer);
}

void ThrMinimization::Stop()
{
    LOG(INFO) << GREEN << "[ThrMinimization::Stop] Stopping" << RESET;
    this->closeFileHandler();
}

void ThrMinimization::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos             = nullptr;
    PixelAlive::histos = nullptr;
#endif

    ThrMinimization::ConfigureCalibration();
    ThrMinimization::initializeFiles(fileRes_, currentRun);
}

void ThrMinimization::initializeFiles(const std::string fileRes_, int currentRun)
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::initializeFiles("", -1);

    fileRes = fileRes_;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrMinimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new ThresholdHistograms;
#endif
}

void ThrMinimization::run()
{
    ThrMinimization::bitWiseScanGlobal(frontEnd->thresholdReg, nEvents, targetOccupancy, ThrStart, ThrStop);

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
    ThrMinimization::chipErrorReport();
}

void ThrMinimization::draw(int currentRun)
{
    ThrMinimization::saveChipRegisters(currentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> ThrMinimization saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    ThrMinimization::fillHisto();
    histos->process();

    PixelAlive::draw(-1);

    this->WriteRootFile();
    this->CloseResultFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrMinimization::analyze()
{
    for(const auto cBoard: theThrContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    LOG(INFO) << GREEN << "Global threshold for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cModule->getId() << "/"
                              << cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << cChip->getSummary<uint16_t>() << RESET;
}

void ThrMinimization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theThrContainer);
#endif
}

void ThrMinimization::bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue)
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

                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if(newValue < target)

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

void ThrMinimization::chipErrorReport()
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

void ThrMinimization::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> ThrMinimization saved the configuration file for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cModule->getId() << "/" << cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
