/*!
  \file                  RD53ThrEqualizationSC.cc
  \brief                 Implementaion of threshold equalization with SCurves
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrEqualizationSC.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ThrEqualizationSC::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    SCurve::ConfigureCalibration();
    SCurve::doDisplay    = false;
    SCurve::doUpdateChip = false;
    RD53RunProgress::total() -= SCurve::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    doDisplay    = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip = this->findValueInSettings<double>("UpdateChipCfg");

    frontEnd = firstChip.getMajorityFE(SCurve::colStart, SCurve::colStop);
    if(frontEnd == &RD53::SYNC)
    {
        LOG(ERROR) << BOLDRED << "ThrEqualizationSC cannot be used on the Synchronous FE, please change the selected columns" << RESET;
        exit(EXIT_FAILURE);
    }
    SCurve::colStart = std::max(SCurve::colStart, frontEnd->colStart);
    SCurve::colStop  = std::min(SCurve::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrEqualizationSC will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << GREEN << BOLDYELLOW << SCurve::colStart << ", "
              << SCurve::colStop << RESET << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    auto customChannelGroup = firstChip.getChannelGroup();
    customChannelGroup->disableAllChannels();

    for(auto row = SCurve::rowStart; row <= SCurve::rowStop; row++)
        for(auto col = SCurve::colStart; col <= SCurve::colStop; col++) customChannelGroup->enableChannel(row, col);

    SCurve::theChnGroupHandler->setCustomChannelGroup(*customChannelGroup);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrEqualizationSC::getNumberIterations();
}

void ThrEqualizationSC::Running()
{
    theCurrentRun         = this->fRunNumber;
    SCurve::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrEqualizationSC::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(SCurve::saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_ThrEqualizationSC.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrEqualizationSC::run();
    ThrEqualizationSC::analyze();
    ThrEqualizationSC::saveChipRegisters(theCurrentRun);
    ThrEqualizationSC::sendData();

    SCurve::sendData();
}

void ThrEqualizationSC::sendData()
{
    auto theTDACStream = prepareChannelContainerStreamer<uint16_t>("TDAC");

    if(fDQMStreamerEnabled == true)
        for(const auto cBoard: theTDACcontainer) theTDACStream->streamAndSendBoard(cBoard, fDQMStreamer);
}

void ThrEqualizationSC::Stop()
{
    LOG(INFO) << GREEN << "[ThrEqualizationSC::Stop] Stopping" << RESET;

    Tool::Stop();

    ThrEqualizationSC::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void ThrEqualizationSC::localConfigure(const std::string& fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos         = nullptr;
    SCurve::histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun         = currentRun;
        SCurve::theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[ThrEqualizationSC::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    ThrEqualizationSC::ConfigureCalibration();
    ThrEqualizationSC::initializeFiles(fileRes_, currentRun);
}

void ThrEqualizationSC::initializeFiles(const std::string& fileRes_, int currentRun)
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    SCurve::initializeFiles("", -1);

    fileRes = fileRes_;

    if((currentRun >= 0) && (SCurve::saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_ThrEqualizationSC.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new ThrEqualizationHistograms;
#endif
}

void ThrEqualizationSC::run()
{
    // #########################
    // # Find global threshold #
    // #########################
    SCurve::run();
    auto targetThr = SCurve::analyze();

    // ##############################
    // # Run threshold equalization #
    // ##############################
    size_t TDACsize = RD53Shared::setBits(RD53Constants::NBIT_TDAC) + 1;
    if(frontEnd == &RD53::DIFF) TDACsize *= 2;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theTDACcontainer);
    ThrEqualizationSC::bitWiseScanLocal(frontEnd->name, targetThr);

    // #################################################
    // # Fill TDAC container and mark enabled channels #
    // #################################################
    for(const auto cBoard: *fDetectorContainer)
    {
        uint16_t cGroupNumber = 1; // assume all channel group is group 0
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), "PIX_PORTAL", *theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

                    auto& theChannelGroupHandler = this->getChannelGroupHandlerContainer()
                                                       ->at(cBoard->getIndex())
                                                       ->at(cOpticalGroup->getIndex())
                                                       ->at(cHybrid->getIndex())
                                                       ->at(cChip->getIndex())
                                                       ->getSummary<std::shared_ptr<ChannelGroupHandler>>();
                    auto cTestChannelGroup = theChannelGroupHandler->getTestGroup(cGroupNumber);
                    for(auto row = 0u; row < firstChip.getNRows(); row++)
                        for(auto col = 0u; col < firstChip.getNCols(); col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !cTestChannelGroup->isChannelEnabled(row, col))
                                theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) = TDACsize;
                }
    }

    // ################
    // # Error report #
    // ################
    ThrEqualizationSC::chipErrorReport();
}

void ThrEqualizationSC::draw()
{
    ThrEqualizationSC::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> ThrEqualizationSC saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    ThrEqualizationSC::fillHisto();
    histos->process();

    SCurve::draw(false);

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrEqualizationSC::analyze()
{
    const float  maxTDACdistance = 2; // @CONST@
    const size_t TDACcenter      = RD53Shared::setBits(RD53Constants::NBIT_TDAC) / 2;

    for(const auto cBoard: *fDetectorContainer)
    {
        uint16_t cGroupNumber = 1; // assume all channel group is group 0
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto& theChannelGroupHandler = this->getChannelGroupHandlerContainer()
                                                       ->at(cBoard->getIndex())
                                                       ->at(cOpticalGroup->getIndex())
                                                       ->at(cHybrid->getIndex())
                                                       ->at(cChip->getIndex())
                                                       ->getSummary<std::shared_ptr<ChannelGroupHandler>>();
                    auto cTestChannelGroup = theChannelGroupHandler->getTestGroup(cGroupNumber);
                    static_cast<RD53*>(cChip)->copyMaskFromDefault();
                    float avgTDAC = 0;
                    int   counter = 0;

                    for(auto row = 0u; row < firstChip.getNRows(); row++)
                        for(auto col = 0u; col < firstChip.getNCols(); col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !cTestChannelGroup->isChannelEnabled(row, col))
                            // if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                            {
                                static_cast<RD53*>(cChip)->setTDAC(
                                    row, col, theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col));

                                avgTDAC += theTDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                counter++;
                            }

                    avgTDAC /= counter;

                    // ###########################
                    // # Check TDAC distribution #
                    // ###########################
                    if(fabs(avgTDAC - TDACcenter) > maxTDACdistance)
                    {
                        LOG(WARNING) << BOLDRED << "Average TDAC distribution not centered around " << BOLDYELLOW << TDACcenter << BOLDRED << " (i.e. " << std::setprecision(1) << BOLDYELLOW << avgTDAC
                                     << BOLDRED << " - center > " << BOLDYELLOW << maxTDACdistance << BOLDRED << ") for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                     << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "]" << std::setprecision(-1) << RESET;
                    }
                }
    }
}

void ThrEqualizationSC::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillTDAC(theTDACcontainer);
#endif
}

void ThrEqualizationSC::bitWiseScanLocal(const std::string& regName, std::shared_ptr<DetectorDataContainer> target)
{
    float    tmp;
    uint16_t init;
    uint16_t numberOfBits = floor(log2(frontEnd->nTDACvalues) + 1);

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;

    DetectorDataContainer bestDACcontainer;
    DetectorDataContainer bestContainer;

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, minDACcontainer, init = 0);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, maxDACcontainer, init = frontEnd->nTDACvalues);

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, bestDACcontainer);
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, bestContainer, tmp = 0);

    // ############################
    // # Read DAC starting values #
    // ############################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), regName, *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                        this->fReadoutChipInterface->WriteChipAllLocalReg(
                            static_cast<RD53*>(cChip), regName, *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

        // ################
        // # Run analysis #
        // ################
        SCurve::run();
        auto output = SCurve::analyze();

        // ##############################################
        // # Send periodic data to monitor the progress #
        // ##############################################
        SCurve::sendData();

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *output)
        {
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        float theTarget = target->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<ThresholdAndNoise>().fThreshold;

                        for(auto row = 0u; row < firstChip.getNRows(); row++)
                            for(auto col = 0u; col < firstChip.getNCols(); col++)
                            {
                                // #######################
                                // # Build discriminator #
                                // #######################
                                float newValue = cChip->getChannel<ThresholdAndNoise>(row, col).fThreshold;

                                // ########################
                                // # Save best DAC values #
                                // ########################
                                float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<float>(row, col);

                                if(fabs(newValue - theTarget) <= fabs(oldValue - theTarget))
                                {
                                    bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<float>(row, col) = newValue;
                                    bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                }

                                if(newValue > theTarget)

                                    minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                else

                                    maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                    (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) +
                                     maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col)) /
                                    2;
                            }
                    }
        }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    this->fReadoutChipInterface->WriteChipAllLocalReg(
                        static_cast<RD53*>(cChip), regName, *bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
                    static_cast<RD53*>(cChip)->copyMaskToDefault("td");
                }

    // ################
    // # Run analysis #
    // ################
    SCurve::run();
    SCurve::analyze();
}

void ThrEqualizationSC::chipErrorReport() const
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

void ThrEqualizationSC::saveChipRegisters(int currentRun)
{
    const std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + this->fDirectoryName);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> ThrEqualizationSC saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
