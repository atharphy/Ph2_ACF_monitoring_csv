/*!
  \file                  RD53ThrAdjustment.cc
  \brief                 Implementaion of threshold adjustment
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrAdjustment.h"
#include "Utils/ContainerSerialization.h"

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
    targetThreshold = this->findValueInSettings<double>("TargetThr");
    startValue      = this->findValueInSettings<double>("ThrStart");
    stopValue       = this->findValueInSettings<double>("ThrStop");
    doDisplay       = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip    = this->findValueInSettings<double>("UpdateChipCfg");

    PixelAlive::colStart = std::max(PixelAlive::colStart, frontEnd->colStart);
    PixelAlive::colStop  = std::min(PixelAlive::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrAdjustment will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << RESET << BOLDYELLOW << colStart << ", " << colStop << RESET
              << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    for(auto row = PixelAlive::rowStart; row <= PixelAlive::rowStop; row++)
        for(auto col = PixelAlive::colStart; col <= PixelAlive::colStop; col++) PixelAlive::theChnGroupHandler->getRegionOfInterest().enableChannel(row, col);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrAdjustment::getNumberIterations();
}

void ThrAdjustment::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrAdjustment::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_ThrAdjustment.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrAdjustment::run();
    ThrAdjustment::analyze();
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    ThrAdjustment::sendData();
    PixelAlive::sendData();
}

void ThrAdjustment::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("ThrAdjustmentThreshold");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, theThrContainer);
    }
}

void ThrAdjustment::Stop()
{
    LOG(INFO) << GREEN << "[ThrAdjustment::Stop] Stopping" << RESET;

    Tool::Stop();

    ThrAdjustment::draw();
    this->SaveAndClose();

    RD53RunProgress::reset();
}

void ThrAdjustment::localConfigure(const std::string& histoFileName, int currentRun)
{
    histos             = nullptr;
    PixelAlive::histos = nullptr;
    theCurrentRun      = currentRun;

    LOG(INFO) << GREEN << "[ThrAdjustment::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    ThrAdjustment::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<ThresholdHistograms>(histoFileName, "ThrAdjustment", histos, currentRun, PixelAlive::saveBinaryData);
    CalibBase::initializeFiles<PixelAliveHistograms>(histoFileName, "PixelAlive", PixelAlive::histos);
}

void ThrAdjustment::run()
{
    LOG(INFO) << RESET;
    LOG(INFO) << BOLDMAGENTA << ">>> Searching for a threshold maximizing the efficiency <<<" << RESET;
    ThrAdjustment::bitWiseScanGlobal_Maximum(frontEnd->thresholdRegs, targetThreshold, startValue, stopValue);

    LOG(INFO) << RESET;
    LOG(INFO) << BOLDMAGENTA << ">>> Searching for a threshold corresponding to " << std::setprecision(1) << BOLDYELLOW << TARGETEFF * 100 << "%" << BOLDMAGENTA << " efficiency <<<" << RESET;
    ThrAdjustment::bitWiseScanGlobal_Zero(frontEnd->thresholdRegs, targetThreshold, startValue, stopValue);

    // ############################
    // # Fill threshold container #
    // ############################
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theThrContainer);
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    theThrContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                        static_cast<RD53*>(cChip)->getReg(frontEnd->thresholdRegs[0]);

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void ThrAdjustment::draw(bool saveData)
{
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        LOG(INFO) << BOLDBLUE << "\t--> ThrAdjustment saving histograms..." << RESET;
    }

    if(histos->AreHistoBooked == false) histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
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

void ThrAdjustment::bitWiseScanGlobal_Maximum(const std::vector<const char*>& regNames, float targetThreshold, uint16_t startValue, uint16_t stopValue)
{
    bool        step2H = false;
    bool        step2L = false;
    uint16_t    init;
    uint16_t    numberOfBits = floor(log2(stopValue - startValue + 1) + 1);
    const float goldenRatio  = (sqrt(5) - 1) / 2.;

    std::shared_ptr<DetectorDataContainer> outputMidH;
    std::shared_ptr<DetectorDataContainer> outputMidL;
    std::shared_ptr<DetectorDataContainer> outputMidTmp;

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midHDACcontainer;
    DetectorDataContainer midLDACcontainer;
    DetectorDataContainer maxDACcontainer;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, minDACcontainer, init = startValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midHDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midLDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, maxDACcontainer, init = (stopValue + 1));

    // #########################################
    // # Set VCAL_HIGH to get target threshold #
    // #########################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    uint16_t vcal_med_setting =
                        static_cast<RD53*>(fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId()))
                            ->getReg("VCAL_MED");
                    uint16_t vcal_high_setting = round(RD53Shared::firstChip->Charge2VCal(targetThreshold)) + vcal_med_setting;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "VCAL_HIGH", vcal_high_setting, true);

                    LOG(INFO) << GREEN << "The target threshold is " << std::setprecision(1) << BOLDYELLOW << targetThreshold << RESET << GREEN << " electrons" << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Closest charge setting is " << BOLDYELLOW << "VCAL_HIGH" << BOLDBLUE << " = " << BOLDYELLOW << vcal_high_setting << BOLDBLUE << " for "
                              << BOLDYELLOW << "VCAL_MED" << BOLDBLUE << " = " << BOLDYELLOW << vcal_med_setting << std::setprecision(-1) << RESET;
                }

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
                        if(step2H == true)
                            midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

                        if(step2L == true)
                            midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

                        if(step2L == false)
                            midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                                (maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() -
                                 minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>()) *
                                    goldenRatio;

                        if(step2H == false)
                            midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                                (maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() -
                                 minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>()) *
                                    (1 - goldenRatio);
                    }

        if(step2H == true) outputMidL = outputMidH;

        if(step2L == true) outputMidH = outputMidL;

        if(step2L == false)
        {
            // ################
            // # Run analysis #
            // ################
            CalibBase::downloadNewDACvalues(midHDACcontainer, regNames);
            PixelAlive::run();
            outputMidH = PixelAlive::analyze();

            // ##############################################
            // # Send periodic data to monitor the progress #
            // ##############################################
            PixelAlive::sendData();
        }

        if(step2H == false)
        {
            // ################
            // # Run analysis #
            // ################
            CalibBase::downloadNewDACvalues(midLDACcontainer, regNames);
            PixelAlive::run();
            outputMidL = PixelAlive::analyze();

            // ##############################################
            // # Send periodic data to monitor the progress #
            // ##############################################
            PixelAlive::sendData();
        }

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *outputMidH)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float newValueMidH = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;
                        float newValueMidL = outputMidL->getObject(cBoard->getId())
                                                 ->getObject(cOpticalGroup->getId())
                                                 ->getObject(cHybrid->getId())
                                                 ->getObject(cChip->getId())
                                                 ->getSummary<GenericDataVector, OccupancyAndPh>()
                                                 .fOccupancy;

                        // #####################
                        // # Compute next move #
                        // #####################
                        if(newValueMidH > newValueMidL)
                        {
                            minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

                            step2H = true;
                            step2L = false;
                        }
                        else
                        {
                            maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

                            step2H = false;
                            step2L = true;
                        }
                    }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    CalibBase::downloadNewDACvalues(midHDACcontainer, regNames);

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");
}

void ThrAdjustment::bitWiseScanGlobal_Zero(const std::vector<const char*>& regNames, float targetThreshold, uint16_t startValue, uint16_t stopValue)
{
    float    tmp;
    uint16_t init;
    uint16_t numberOfBits = floor(log2(stopValue - startValue + 1) + 1);

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;

    DetectorDataContainer bestDACcontainer;
    DetectorDataContainer bestContainer;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, minDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, maxDACcontainer, init = (stopValue + 1));

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, bestDACcontainer, init = 0);
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, bestContainer, tmp = 0);

    // #########################################
    // # Set VCAL_HIGH to get target threshold #
    // #########################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    uint16_t vcal_med_setting =
                        static_cast<RD53*>(fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId()))
                            ->getReg("VCAL_MED");
                    uint16_t vcal_high_setting = round(RD53Shared::firstChip->Charge2VCal(targetThreshold)) + vcal_med_setting;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "VCAL_HIGH", vcal_high_setting, true);

                    LOG(INFO) << GREEN << "The target threshold is " << std::setprecision(1) << BOLDYELLOW << targetThreshold << RESET << GREEN << " electrons" << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Closest charge setting is " << BOLDYELLOW << "VCAL_HIGH" << BOLDBLUE << " = " << BOLDYELLOW << vcal_high_setting << BOLDBLUE << " for "
                              << BOLDYELLOW << "VCAL_MED" << BOLDBLUE << " = " << BOLDYELLOW << vcal_med_setting << std::setprecision(-1) << RESET;

                    // ###################
                    // # Set start value #
                    // ###################
                    uint16_t minValue = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits(regNames.at(0)));
                    for(const auto& regName: regNames)
                    {
                        auto value = static_cast<RD53*>(fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId()))
                                         ->getReg(regName);
                        if(value < minValue) minValue = value;
                    }
                    minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() = minValue;
                }

    for(auto i = 0u; i <= numberOfBits; i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                        midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                            (minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                             maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>()) /
                            2;
        CalibBase::downloadNewDACvalues(midDACcontainer, regNames);

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

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>();

                        if(fabs(newValue - TARGETEFF) <= fabs(oldValue - TARGETEFF))
                        {
                            bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = newValue;

                            bestDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        }

                        if(newValue < TARGETEFF)

                            maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

                        else

                            minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                    }
    }

    // ###########################
    // # Download new DAC values #
    // ###########################
    CalibBase::downloadNewDACvalues(bestDACcontainer, regNames, true, 0);

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");
}
