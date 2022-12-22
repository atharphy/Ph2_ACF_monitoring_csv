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
    auto theThrStream = this->prepareChipContainerStreamer<EmptyContainer, uint16_t>();

    if(fDQMStreamerEnabled == true)
        for(const auto cBoard: theThrContainer) theThrStream->streamAndSendBoard(cBoard, fDQMStreamer);
}

void ThrAdjustment::Stop()
{
    LOG(INFO) << GREEN << "[ThrAdjustment::Stop] Stopping" << RESET;

    Tool::Stop();

    ThrAdjustment::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void ThrAdjustment::localConfigure(const std::string& histoFileName, int currentRun)
{
#ifdef __USE_ROOT__
    histos             = nullptr;
    PixelAlive::histos = nullptr;
#endif

    theCurrentRun = currentRun;
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
    ThrAdjustment::bitWiseScanGlobal(frontEnd->thresholdReg, targetThreshold, startValue, stopValue);

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

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");

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

void ThrAdjustment::bitWiseScanGlobal(const std::string& regName, float target, uint16_t startValue, uint16_t stopValue)
{
    float    tmp;
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
                        static_cast<RD53*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()))->getReg("VCAL_MED");
                    uint16_t vcal_high_setting = round(RD53Shared::firstChip->Charge2VCal(target)) + vcal_med_setting;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "VCAL_HIGH", vcal_high_setting, true);

                    LOG(INFO) << GREEN << "The target threshold is " << std::setprecision(1) << BOLDYELLOW << target << RESET << GREEN << " electrons" << RESET;
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
                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() +
                             maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()) /
                            2;
        CalibBase::downloadNewDACvalues(midDACcontainer, regName);

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
    CalibBase::downloadNewDACvalues(bestDACcontainer, regName, true, 0);

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();
}
