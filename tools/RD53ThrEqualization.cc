/*!
  \file                  RD53ThrEqualization.cc
  \brief                 Implementaion of threshold equalization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrEqualization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ThrEqualization::ConfigureCalibration()
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
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    nEvtsBurst     = this->findValueInSettings("nEvtsBurst");
    startValue     = this->findValueInSettings("VCalHstart");
    stopValue      = this->findValueInSettings("VCalHstop");
    nHITxCol       = this->findValueInSettings("nHITxCol");
    doFast         = this->findValueInSettings("DoFast");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    frontEnd = RD53::getMajorityFE(colStart, colStop);
    if(frontEnd == &RD53::SYNC)
    {
        LOG(ERROR) << BOLDRED << "ThrEqualization cannot be used on the Synchronous FE, please change the selected columns" << RESET;
        exit(EXIT_FAILURE);
    }
    colStart = std::max(colStart, frontEnd->colStart);
    colStop  = std::min(colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrEqualization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << GREEN << BOLDYELLOW << colStart << ", " << colStop
              << RESET << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    ChannelGroup<RD53::nRows, RD53::nCols> customChannelGroup;
    customChannelGroup.disableAllChannels();

    for(auto row = rowStart; row <= rowStop; row++)
        for(auto col = colStart; col <= colStop; col++) customChannelGroup.enableChannel(row, col);

    theChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(customChannelGroup, doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol);
    theChnGroupHandler->setCustomChannelGroup(customChannelGroup);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrEqualization::getNumberIterations();
}

void ThrEqualization::Start(int currentRun)
{
    LOG(INFO) << GREEN << "[ThrEqualization::Start] Starting" << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrEqualization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrEqualization::run();
    ThrEqualization::analyze();
    ThrEqualization::sendData();

    PixelAlive::sendData();
}

void ThrEqualization::sendData()
{
    auto theOccStream  = prepareChannelContainerStreamer<OccupancyAndPh>("Occ");
    auto theTDACStream = prepareChannelContainerStreamer<uint16_t>("TDAC");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theOccStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTDACcontainer) theTDACStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void ThrEqualization::Stop()
{
    LOG(INFO) << GREEN << "[ThrEqualization::Stop] Stopping" << RESET;
    this->closeFileHandler();
}

void ThrEqualization::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos             = nullptr;
    PixelAlive::histos = nullptr;
#endif

    ThrEqualization::ConfigureCalibration();
    ThrEqualization::initializeFiles(fileRes_, currentRun);
}

void ThrEqualization::initializeFiles(const std::string fileRes_, int currentRun)
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::initializeFiles("", -1);

    fileRes = fileRes_;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrEqualization.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new ThrEqualizationHistograms;
#endif
}

void ThrEqualization::run()
{
    ThrEqualization::bitWiseScanGlobal("VCAL_HIGH", nEvents, TARGETEFF, startValue, stopValue);

    // ##############################
    // # Run threshold equalization #
    // ##############################
    size_t TDACsize = RD53Shared::setBits(RD53Constants::NBIT_TDAC) + 1;

    this->fDetectorDataContainer = &theOccContainer;
    ContainerFactory::copyAndInitStructure<OccupancyAndPh>(*fDetectorContainer, *this->fDetectorDataContainer);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theTDACcontainer);

    this->fChannelGroupHandler = theChnGroupHandler.get();
    this->SetTestPulse(true);
    this->fMaskChannelsFromOtherGroups = true;
    ThrEqualization::bitWiseScanLocal(frontEnd->name, nEvents, TARGETEFF, nEvtsBurst);

    // #################################################
    // # Fill TDAC container and mark enabled channels #
    // #################################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), "PIX_PORTAL", *theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex()));

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                            {
                                theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<OccupancyAndPh>(row, col).fOccupancy =
                                    RD53Shared::ISDISABLED;
                                theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) = TDACsize;
                            }
                }

    // ################
    // # Error report #
    // ################
    ThrEqualization::chipErrorReport();
}

void ThrEqualization::draw(int currentRun)
{
    ThrEqualization::saveChipRegisters(currentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> ThrEqualization saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    ThrEqualization::fillHisto();
    histos->process();

    PixelAlive::draw(-1);

    this->WriteRootFile();
    this->CloseResultFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrEqualization::analyze()
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    static_cast<RD53*>(cChip)->copyMaskFromDefault();

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                                static_cast<RD53*>(cChip)->setTDAC(
                                    row, col, theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col));

                    static_cast<RD53*>(cChip)->copyMaskToDefault();
                }
}

void ThrEqualization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillTDAC(theTDACcontainer);
#endif
}

void ThrEqualization::bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue)
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

void ThrEqualization::bitWiseScanLocal(const std::string& regName, uint32_t nEvents, const float& target, uint32_t nEvtsBurst)
{
    uint16_t     init;
    unsigned int numberOfBits = ceil(log2(frontEnd->nTDACvalues));

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;

    DetectorDataContainer bestDACcontainer;
    DetectorDataContainer bestContainer;

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, minDACcontainer, init = 0);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, maxDACcontainer, init = frontEnd->nTDACvalues);

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, bestDACcontainer);
    ContainerFactory::copyAndInitChannel<OccupancyAndPh>(*fDetectorContainer, bestContainer);

    for(const auto cBoard: bestContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++) cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy = 0;

    // ############################
    // # Read DAC starting values #
    // ############################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), regName, *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex()));

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                        this->fReadoutChipInterface->WriteChipAllLocalReg(
                            static_cast<RD53*>(cChip), regName, *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex()));

        // ################
        // # Run analysis #
        // ################
        this->measureData(nEvents, nEvtsBurst);

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: theOccContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cModule: *cOpticalGroup)
                    for(const auto cChip: *cModule)
                        for(auto row = 0u; row < RD53::nRows; row++)
                            for(auto col = 0u; col < RD53::nCols; col++)
                            {
                                // #######################
                                // # Build discriminator #
                                // #######################
                                float newValue = cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy;

                                // ########################
                                // # Save best DAC values #
                                // ########################
                                float oldValue = bestContainer.at(cBoard->getIndex())
                                                     ->at(cOpticalGroup->getIndex())
                                                     ->at(cModule->getIndex())
                                                     ->at(cChip->getIndex())
                                                     ->getChannel<OccupancyAndPh>(row, col)
                                                     .fOccupancy;

                                if(fabs(newValue - target) < fabs(oldValue - target) || (newValue == oldValue))
                                {
                                    bestContainer.at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cModule->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<OccupancyAndPh>(row, col)
                                        .fOccupancy = newValue;
                                    bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                }

                                if(newValue < target)

                                    minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                else

                                    maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                    (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) +
                                     maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col)) /
                                    2;
                            }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                    this->fReadoutChipInterface->WriteChipAllLocalReg(
                        static_cast<RD53*>(cChip), regName, *bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cModule->getIndex())->at(cChip->getIndex()));

    // ################
    // # Run analysis #
    // ################
    this->measureData(nEvents, nEvtsBurst);
}

void ThrEqualization::chipErrorReport()
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

void ThrEqualization::saveChipRegisters(int currentRun)
{
    std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cModule: *cOpticalGroup)
                for(const auto cChip: *cModule)
                {
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + RD53Shared::RESULTDIR);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> ThrEqualization saved the configuration file for [board/opticalGroup/module/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cModule->getId() << "/" << cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
