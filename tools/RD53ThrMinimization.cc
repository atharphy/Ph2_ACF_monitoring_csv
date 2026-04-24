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
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    targetOccupancy = this->findValueInSettings<double>("TargetOcc");
    maxMaskedPixels = this->findValueInSettings<double>("MaxMaskedPixels");
    relStartValue   = this->findValueInSettings<double>("ThrRelStart");
    amplitudeValue  = this->findValueInSettings<double>("ThrAmplitude");
    doDisplay       = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip    = this->findValueInSettings<double>("UpdateChipCfg");

    colStart = std::max(PixelAlive::colStart, frontEnd->colStart);
    colStop  = std::min(PixelAlive::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrMinimization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << BOLDYELLOW << colStart << ", " << colStop << RESET
              << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    for(auto row = PixelAlive::rowStart; row <= PixelAlive::rowStop; row++)
        for(auto col = PixelAlive::colStart; col <= PixelAlive::colStop; col++) PixelAlive::theChnGroupHandler->getRegionOfInterest().enableChannel(row, col);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrMinimization::getNumberIterations();
}

void ThrMinimization::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrMinimization::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(CalibBase::theCurrentRun) + "_ThrMinimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrMinimization::run();
    ThrMinimization::analyze();
    ThrMinimization::draw();
    ThrMinimization::sendData();
    PixelAlive::sendData();
}

void ThrMinimization::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("ThrAdjustmentThreshold");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, theThrContainer);
    }
}

void ThrMinimization::Stop()
{
    LOG(INFO) << GREEN << "[ThrMinimization::Stop] Stopping" << RESET;
    CalibBase::Stop();
}

void ThrMinimization::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos             = nullptr;
    PixelAlive::histos = nullptr;

    LOG(INFO) << GREEN << "[ThrMinimization::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    ThrMinimization::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "ThrMinimization", histos, currentRun, PixelAlive::saveBinaryData);
    CalibBase::initializeFiles(histoFileName, "PixelAlive", PixelAlive::histos);
}

void ThrMinimization::run()
{
    ThrMinimization::bitWiseScanGlobal(frontEnd->thresholdRegs, targetOccupancy, maxMaskedPixels, relStartValue, amplitudeValue);

    // ############################
    // # Fill threshold container #
    // ############################
    ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, theThrContainer);
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    for(unsigned int i = 0u; i < frontEnd->thresholdRegs.size(); i++)
                        theThrContainer.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<std::vector<uint16_t>>()
                            .push_back(static_cast<RD53*>(cChip)->getReg(frontEnd->thresholdRegs[i]));

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void ThrMinimization::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
    ThrMinimization::fillHisto();
    histos->process();

    PixelAlive::draw(false);

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrMinimization::analyze()
{
    for(const auto cBoard: theThrContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Global threshold(s) for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                              << "/" << +cChip->getId() << RESET << GREEN << "] is(are)" << RESET;
                    for(unsigned int i = 0u; i < frontEnd->thresholdRegs.size(); i++)
                        LOG(INFO) << GREEN << "\t--> " << frontEnd->thresholdRegs[i] << " = " << BOLDYELLOW << cChip->getSummary<std::vector<uint16_t>>()[i] << RESET;
                }
}

void ThrMinimization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theThrContainer);
#endif
}

void ThrMinimization::bitWiseScanGlobal(const std::vector<const char*>& regNames, float target, float threshold, int16_t relStartValue, uint16_t amplitudeValue)
{
    float          tmp          = 0;
    int16_t        relStopValue = relStartValue + amplitudeValue + 1;
    uint16_t       init         = 0;
    const size_t   totalPixels  = RD53Shared::firstChip->getNRows() * RD53Shared::firstChip->getNCols();
    const uint16_t numberOfBits = floor(log2(amplitudeValue + 1) + 1);

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;
    DetectorDataContainer bestContainer;

    std::vector<DetectorDataContainer*> originalDACcontainer;
    std::vector<DetectorDataContainer*> downloadDACcontainer;
    std::vector<DetectorDataContainer*> bestDACcontainer;

    ContainerFactory::copyAndInitChip<int16_t>(*fDetectorContainer, minDACcontainer, relStartValue);
    ContainerFactory::copyAndInitChip<int16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChip<int16_t>(*fDetectorContainer, maxDACcontainer, relStopValue);
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, bestContainer, tmp);

    for(unsigned int i = 0; i < regNames.size(); i++)
    {
        originalDACcontainer.push_back(new DetectorDataContainer);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *originalDACcontainer.back());

        downloadDACcontainer.push_back(new DetectorDataContainer);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *downloadDACcontainer.back());

        bestDACcontainer.push_back(new DetectorDataContainer);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *bestDACcontainer.back(), init);
    }

    // ####################################
    // # Compute startValue and stopValue #
    // ####################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    for(auto [regName, original]: boost::combine(regNames, originalDACcontainer))
                        original->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                            static_cast<RD53*>(cChip)->getReg(regName);

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>() =
                            std::floor((minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>() +
                                        maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>()) /
                                       2.);
                        for(auto [original, download]: boost::combine(originalDACcontainer, downloadDACcontainer))
                        {
                            download->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                original->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>();
                        }
                    }
        CalibBase::downloadNewDACvalues(downloadDACcontainer, regNames);

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();

        // ##############################################
        // # Send periodic data to monitor the progress #
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
                        // float newValue = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancyMedian; // @TMP@

                        // #######################
                        // # Build discriminator #
                        // #######################
                        size_t maskedPixels = 0;
                        for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                            for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                                if(cChip->getChannel<OccupancyAndPh>(row, col).fStatus == RD53Shared::ISMASKED) maskedPixels++;
                        maskedPixels = maskedPixels / totalPixels * 100;

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>();

                        if(fabs(newValue - target) < fabs(oldValue - target))
                        {
                            bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = newValue;

                            for(auto [download, best]: boost::combine(downloadDACcontainer, bestDACcontainer))
                                best->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                    download->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        }

                        if((newValue < target) && (maskedPixels < threshold))
                            maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>();
                        else
                            minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int16_t>();
                    }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    LOG(INFO) << BOLDMAGENTA << ">>> Best values <<<" << RESET;
    CalibBase::downloadNewDACvalues(bestDACcontainer, regNames, false, true);

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");

    // ###################
    // # Free the memory #
    // ###################
    for(unsigned int i = 0; i < regNames.size(); i++)
    {
        delete(downloadDACcontainer[i]);
        delete(originalDACcontainer[i]);
    }
    downloadDACcontainer.clear();
    originalDACcontainer.clear();
}
