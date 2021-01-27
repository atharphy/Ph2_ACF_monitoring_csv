/*!
  \file                  RD53GainOptimization.cc
  \brief                 Implementaion of gain optimization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53GainOptimization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void GainOptimization::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    Gain::ConfigureCalibration();
    Gain::doDisplay    = false;
    Gain::doUpdateChip = false;
    RD53RunProgress::total() -= Gain::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart      = this->findValueInSettings("ROWstart");
    rowStop       = this->findValueInSettings("ROWstop");
    colStart      = this->findValueInSettings("COLstart");
    colStop       = this->findValueInSettings("COLstop");
    nEvents       = this->findValueInSettings("nEvents");
    startValue    = this->findValueInSettings("VCalHstart");
    stopValue     = this->findValueInSettings("VCalHstop");
    targetCharge  = RD53chargeConverter::Charge2VCal(this->findValueInSettings("TargetCharge"));
    KrumCurrStart = this->findValueInSettings("KrumCurrStart");
    KrumCurrStop  = this->findValueInSettings("KrumCurrStop");
    ;
    doFast         = this->findValueInSettings("DoFast");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    frontEnd = RD53::getMajorityFE(colStart, colStop);
    colStart = std::max(colStart, frontEnd->colStart);
    colStop  = std::min(colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "GainOptimization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << RESET << BOLDYELLOW << colStart << ", " << colStop
              << RESET << GREEN << "]" << RESET;

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += GainOptimization::getNumberIterations();
}

void GainOptimization::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[GainOptimization::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_GainOptimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    GainOptimization::run();
    GainOptimization::analyze();
    GainOptimization::saveChipRegisters(theCurrentRun);
    GainOptimization::sendData();

    Gain::sendData();
}

void GainOptimization::sendData()
{
    auto theKrumStream = prepareChipContainerStreamer<EmptyContainer, uint16_t>();

    if(fStreamerEnabled == true)
        for(const auto cBoard: theKrumCurrContainer) theKrumStream.streamAndSendBoard(cBoard, fNetworkStreamer);
}

void GainOptimization::Stop()
{
    LOG(INFO) << GREEN << "[GainOptimization::Stop] Stopping" << RESET;

    Tool::Stop();

    GainOptimization::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void GainOptimization::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos       = nullptr;
    Gain::histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[GainOptimization::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    GainOptimization::ConfigureCalibration();
    GainOptimization::initializeFiles(fileRes_, currentRun);
}

void GainOptimization::initializeFiles(const std::string fileRes_, int currentRun)
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    Gain::initializeFiles("", -1);

    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_GainOptimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new GainOptimizationHistograms;
#endif
}

void GainOptimization::run()
{
    GainOptimization::bitWiseScanGlobal(frontEnd->gainReg, nEvents, targetCharge, KrumCurrStart, KrumCurrStop);

    // #######################################
    // # Fill Krummenacher Current container #
    // #######################################
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theKrumCurrContainer);
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    theKrumCurrContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                        static_cast<RD53*>(cChip)->getReg(frontEnd->gainReg);

    // ################
    // # Error report #
    // ################
    GainOptimization::chipErrorReport();
}

void GainOptimization::draw()
{
    GainOptimization::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> GainOptimization saving histograms..." << RESET;

    Gain::draw(false);

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    GainOptimization::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    this->CloseResultFile();
#endif
}

void GainOptimization::analyze()
{
    for(const auto cBoard: theKrumCurrContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    LOG(INFO) << GREEN << "Krummenacher Current for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                              << "/" << +cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW << cChip->getSummary<uint16_t>() << RESET;
}

void GainOptimization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theKrumCurrContainer);
#endif
}

void GainOptimization::bitWiseScanGlobal(const std::string& regName, uint32_t nEvents, const float& target, uint16_t startValue, uint16_t stopValue)
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
    ContainerFactory::copyAndInitChip<OccupancyAndPh>(*fDetectorContainer, bestContainer);

    // #########################
    // # Initialize containers #
    // #########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()               = 0;
                    bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<OccupancyAndPh>().fOccupancy = 0;
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

                    // static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendChipCommandsPack(chipCommandList, hybridId);
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackHybridCommands(chipCommandList, hybridId, hybridCommandList);
                }

                static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendHybridCommandsPack(hybridCommandList);
            }

        // ################
        // # Run analysis #
        // ################
        Gain::run();
        auto output = Gain::analyze();
        output->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        Gain::sendData();

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        // ##############################################
                        // # Search for maximum and build discriminator #
                        // ##############################################
                        float  stdDev = 0;
                        size_t cnt    = 0;
                        for(auto row = 0u; row < RD53::nRows; row++)
                            for(auto col = 0u; col < RD53::nCols; col++)
                                if(cChip->getChannel<GainAndIntercept>(row, col).fGain != 0)
                                {
                                    stdDev += cChip->getChannel<GainAndIntercept>(row, col).fGain * cChip->getChannel<GainAndIntercept>(row, col).fGain;
                                    cnt++;
                                }
                        stdDev          = (cnt != 0 ? stdDev / cnt : 0) - cChip->getSummary<GainAndIntercept>().fGain * cChip->getSummary<GainAndIntercept>().fGain;
                        stdDev          = (stdDev > 0 ? sqrt(stdDev) : 0);
                        size_t ToTpoint = RD53Shared::setBits(RD53EvtEncoder::NBIT_TOT / RD53Constants::NPIX_REGION) - 2;
                        float  newValue = (ToTpoint - cChip->getSummary<GainAndIntercept>().fIntercept) / (cChip->getSummary<GainAndIntercept>().fGain + NSTDEV * stdDev);

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<OccupancyAndPh>().fPh;
                        if(fabs(newValue - target) < fabs(oldValue - target))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<OccupancyAndPh>().fPh = newValue;
                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if((newValue > target) || (newValue < 0))

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

                // static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendChipCommandsPack(chipCommandList, hybridId);
                static_cast<RD53Interface*>(this->fReadoutChipInterface)->PackHybridCommands(chipCommandList, hybridId, hybridCommandList);
            }

            static_cast<RD53Interface*>(this->fReadoutChipInterface)->SendHybridCommandsPack(hybridCommandList);
        }

    // ################
    // # Run analysis #
    // ################
    Gain::run();
    Gain::analyze();
}

void GainOptimization::chipErrorReport()
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

void GainOptimization::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> GainOptimization saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
