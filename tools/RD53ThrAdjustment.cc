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
    relStartValue   = this->findValueInSettings<double>("ThrRelStart");
    amplitudeValue  = this->findValueInSettings<double>("ThrAmplitude");
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
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrAdjustment::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(CalibBase::theCurrentRun) + "_ThrAdjustment.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrAdjustment::run();
    ThrAdjustment::analyze();
    ThrAdjustment::draw();
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
    CalibBase::Stop();
}

void ThrAdjustment::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos             = nullptr;
    PixelAlive::histos = nullptr;

    LOG(INFO) << GREEN << "[ThrAdjustment::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    ThrAdjustment::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "ThrAdjustment", histos, currentRun, PixelAlive::saveBinaryData);
    CalibBase::initializeFiles(histoFileName, "PixelAlive", PixelAlive::histos);
}

void ThrAdjustment::run()
{
    LOG(INFO) << RESET;
    LOG(INFO) << BOLDMAGENTA << ">>> Searching for threshold corresponding to " << std::setprecision(1) << BOLDYELLOW << TARGETEFF * 100 << BOLDMAGENTA << "% efficiency <<<" << RESET;
    ThrAdjustment::bitWiseScanGlobal(frontEnd->thresholdRegs, targetThreshold, relStartValue, amplitudeValue);

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

void ThrAdjustment::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
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
                {
                    LOG(INFO) << GREEN << "Global threshold(s) for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                              << "/" << +cChip->getId() << RESET << GREEN << "] is(are)" << RESET;
                    for(unsigned int i = 0u; i < frontEnd->thresholdRegs.size(); i++)
                        LOG(INFO) << GREEN << "\t--> " << frontEnd->thresholdRegs[i] << " = " << BOLDYELLOW << cChip->getSummary<std::vector<uint16_t>>()[i] << RESET;
                }
}

void ThrAdjustment::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theThrContainer);
#endif
}

void ThrAdjustment::bitWiseScanGlobal(const std::vector<const char*>& regNames, float targetThreshold, int16_t relStartValue, uint16_t amplitudeValue)
{
    char           zeroC        = 'L'; // 'H' and 'L' set direction
    const uint16_t numberOfBits = floor(log2(amplitudeValue + 1) + 1);
    const float    goldenRatio  = (sqrt(5) - 1) / 2.;

    DetectorDataContainer outputMidH;
    DetectorDataContainer outputMidL;

    DetectorDataContainer directionContainer;
    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midHDACcontainer;
    DetectorDataContainer midLDACcontainer;
    DetectorDataContainer maxDACcontainer;
    DetectorDataContainer chargeContainer;

    std::vector<DetectorDataContainer*> downloadDACcontainer;
    std::vector<DetectorDataContainer*> originalDACcontainer;

    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, outputMidH);
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, outputMidL);

    ContainerFactory::copyAndInitChip<char>(*fDetectorContainer, directionContainer, zeroC);
    ContainerFactory::copyAndInitChip<int32_t>(*fDetectorContainer, minDACcontainer);
    ContainerFactory::copyAndInitChip<int32_t>(*fDetectorContainer, midHDACcontainer);
    ContainerFactory::copyAndInitChip<int32_t>(*fDetectorContainer, midLDACcontainer);
    ContainerFactory::copyAndInitChip<int32_t>(*fDetectorContainer, maxDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, chargeContainer);

    for(unsigned int i = 0; i < regNames.size(); i++)
    {
        DetectorDataContainer* tmpCont1(new DetectorDataContainer);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *tmpCont1);
        downloadDACcontainer.push_back(tmpCont1);

        DetectorDataContainer* tmpCont2(new DetectorDataContainer);
        ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *tmpCont2);
        originalDACcontainer.push_back(tmpCont2);
    }

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    // ##########################################
                    // # Find VCAL_HIGH to get target threshold #
                    // ##########################################
                    uint16_t vcal_med_setting  = static_cast<RD53*>(cChip)->getReg("VCAL_MED");
                    uint16_t vcal_high_setting = round(static_cast<RD53*>(cChip)->Charge2VCal(targetThreshold)) + vcal_med_setting;
                    chargeContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() = vcal_high_setting;

                    LOG(INFO) << GREEN << "The target threshold for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                              << "/" << +cChip->getId() << RESET << GREEN "] is " << std::setprecision(1) << BOLDYELLOW << targetThreshold << RESET << GREEN << " electrons" << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Closest charge setting is " << BOLDYELLOW << "VCAL_HIGH" << BOLDBLUE << " = " << BOLDYELLOW << vcal_high_setting << BOLDBLUE << " for "
                              << BOLDYELLOW << "VCAL_MED" << BOLDBLUE << " = " << BOLDYELLOW << vcal_med_setting << std::setprecision(-1) << RESET;

                    // ####################################
                    // # Compute startValue and stopValue #
                    // ####################################
                    for(auto [regName, container]: boost::combine(regNames, originalDACcontainer))
                        container->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                            static_cast<RD53*>(cChip)->getReg(regName);

                    minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() = relStartValue;
                    maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                        minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() + amplitudeValue +
                        1;
                }

    // #######################################################################
    // # Prepare query, disable all chips, and set weak check of data status #
    // #######################################################################
    CalibBase::prepareChipQueryForEnDis("chipSubset");
    CalibBase::setChipEnDis(false);
    RD53Event::weakCheckDataStatus = true;

    for(auto indx = 0u; indx < RD53FWconstants::NMAXCHIP_HYBRID; indx++)
    {
        if(CalibBase::shiftEnable(indx) == true) break;

        LOG(INFO) << RESET;
        LOG(INFO) << BOLDMAGENTA << ">>> Optimizing frontend chip #" << BOLDYELLOW << indx << BOLDMAGENTA << " <<<" << RESET;

        for(auto i = 0u; i <= numberOfBits + 1u; i++)
        {
            // ###########################
            // # Download new DAC values #
            // ###########################
            for(const auto cBoard: *fDetectorContainer)
                for(const auto cOpticalGroup: *cBoard)
                    for(const auto cHybrid: *cOpticalGroup)
                        for(const auto cChip: *cHybrid)
                        {
                            auto minDAC = minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();
                            auto maxDAC = maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();

                            int32_t DACval;
                            if(directionContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<char>() == 'L')
                            {
                                // Converting float to int rounds towards 0, but rounding towards -inf for +ve and -ve values alike is required here: use floor()
                                DACval = std::floor(minDAC + (maxDAC - minDAC) * (1 - goldenRatio));

                                midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() = DACval;
                            }
                            else
                            {
                                // Converting float to int rounds towards 0, but rounding towards -inf for +ve and -ve values alike is required here: use floor()
                                DACval = std::floor(minDAC + (maxDAC - minDAC) * goldenRatio);

                                midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() = DACval;
                            }

                            for(auto [download, original]: boost::combine(downloadDACcontainer, originalDACcontainer))
                                download->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                    original->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() + DACval;
                        }

            // ################
            // # Run analysis #
            // ################
            ThrAdjustment::establishStartingPoint(chargeContainer);
            CalibBase::downloadNewDACvalues(downloadDACcontainer, regNames);
            PixelAlive::doSilentRunning = true;
            PixelAlive::run();
            PixelAlive::doSilentRunning = false;
            auto output                 = PixelAlive::analyze();

            // ##################
            // # Reset sequence #
            // ##################
            CalibBase::copyMaskFromDefault("en in");
            CalibBase::setChipEnDis(false);
            CalibBase::shiftEnable(indx);

            // ##############################################
            // # Send periodic data to monitor the progress #
            // ##############################################
            PixelAlive::sendData();

            // #####################
            // # Compute next step #
            // #####################
            for(const auto cBoard: *fDetectorContainer)
                for(const auto cOpticalGroup: *cBoard)
                    for(const auto cHybrid: *cOpticalGroup)
                        for(const auto cChip: *cHybrid)
                        {
                            // #######################
                            // # Build discriminator #
                            // #######################
                            auto value = output->getObject(cBoard->getId())
                                             ->getObject(cOpticalGroup->getId())
                                             ->getObject(cHybrid->getId())
                                             ->getObject(cChip->getId())
                                             ->getSummary<GenericDataVector, OccupancyAndPh>()
                                             .fOccupancy;

                            if(i == 0) // First iteration
                            {
                                outputMidL.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>()        = value;
                                directionContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<char>() = 'H';
                                continue;
                            }
                            else if(directionContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<char>() == 'L')
                                outputMidL.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = value;
                            else
                                outputMidH.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = value;

                            auto valueMidH = outputMidH.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>();
                            auto valueMidL = outputMidL.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>();

                            if(i == numberOfBits + 1u)
                            {
                                // #######################
                                // # Save best DAC value #
                                // #######################
                                if(fabs(valueMidL - TARGETEFF) < fabs(valueMidH - TARGETEFF))
                                {
                                    for(auto [download, original]: boost::combine(downloadDACcontainer, originalDACcontainer))
                                        download->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                            original->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                                            midLDACcontainer.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<int32_t>();
                                }
                                else if(fabs(valueMidH - TARGETEFF) < fabs(valueMidL - TARGETEFF))
                                {
                                    for(auto [download, original]: boost::combine(downloadDACcontainer, originalDACcontainer))
                                        download->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                            original->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                                            midHDACcontainer.getObject(cBoard->getId())
                                                ->getObject(cOpticalGroup->getId())
                                                ->getObject(cHybrid->getId())
                                                ->getObject(cChip->getId())
                                                ->getSummary<int32_t>();
                                }
                                break; // Allows to compute last move
                            }

                            // #####################
                            // # Compute next move #
                            // #####################

                            // #########################
                            // # Set new window limits #
                            // #########################
                            if((valueMidL < valueMidH) || (valueMidL > TARGETEFF))
                                minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                                    midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();

                            if(valueMidH > TARGETEFF)
                                minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                                    midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();

                            if((valueMidL >= valueMidH) && (valueMidH < TARGETEFF))
                                maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                                    midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();

                            // #########################################
                            // # Set new internal values and direction #
                            // #########################################
                            if(valueMidL < valueMidH || valueMidH > TARGETEFF)
                            {
                                // If slope is positive or valueMidH is above TARGETEFF, Global Zero is to the right of midHDAC

                                midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                                    midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();
                                outputMidL.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = valueMidH;
                            }

                            if(valueMidL >= valueMidH && valueMidL < TARGETEFF)
                            {
                                // If slope is negative or zero and valueMidL is below TARGETEFF, Global Zero is to the left of midLDAC and we move left, otherwise we move right

                                midHDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>() =
                                    midLDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<int32_t>();
                                outputMidH.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = valueMidL;

                                directionContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<char>() = 'L';
                            }
                            else
                                directionContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<char>() = 'H';
                        }
        }
    }

    // ########################################################################
    // # Restore query, enable all chips, and reset weak check of data status #
    // ########################################################################
    fDetectorContainer->resetReadoutChipQueryFunction();
    CalibBase::setChipEnDis(true);
    RD53Event::weakCheckDataStatus = false;

    // ###########################
    // # Download new DAC values #
    // ###########################
    LOG(INFO) << BOLDMAGENTA << ">>> Best values <<<" << RESET;
    CalibBase::downloadNewDACvalues(downloadDACcontainer, regNames, false, true, 0);

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
}

void ThrAdjustment::establishStartingPoint(DetectorDataContainer& chargeContainer)
{
    CalibBase::downloadNewDACvalues({&chargeContainer}, {"VCAL_HIGH"}, true);
    CalibBase::SetInjectionType(PixelAlive::injType);
}
