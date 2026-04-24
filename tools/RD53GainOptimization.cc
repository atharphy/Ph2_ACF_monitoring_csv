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
    PixelAlive::ConfigureCalibration();
    PixelAlive::doDisplay    = false;
    PixelAlive::doUpdateChip = false;
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    targetCharge  = this->findValueInSettings<double>("TargetCharge");
    targetToT     = this->findValueInSettings<double>("TargetToT");
    KrumCurrStart = this->findValueInSettings<double>("KrumCurrStart");
    KrumCurrStop  = this->findValueInSettings<double>("KrumCurrStop");
    doDisplay     = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip  = this->findValueInSettings<double>("UpdateChipCfg");

    colStart = std::max(PixelAlive::colStart, frontEnd->colStart);
    colStop  = std::min(PixelAlive::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "GainOptimization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << RESET << BOLDYELLOW << colStart << ", " << colStop
              << RESET << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    for(auto row = PixelAlive::rowStart; row <= PixelAlive::rowStop; row++)
        for(auto col = PixelAlive::colStart; col <= PixelAlive::colStop; col++) PixelAlive::theChnGroupHandler->getRegionOfInterest().enableChannel(row, col);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += GainOptimization::getNumberIterations();
}

void GainOptimization::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[GainOptimization::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(CalibBase::theCurrentRun) + "_GainOptimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    GainOptimization::run();
    GainOptimization::analyze();
    GainOptimization::draw();
    GainOptimization::sendData();
    PixelAlive::sendData();
}

void GainOptimization::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("GainOptimizationKrumCurr");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, theKrumCurrContainer);
    }
}

void GainOptimization::Stop()
{
    LOG(INFO) << GREEN << "[GainOptimization::Stop] Stopping" << RESET;
    CalibBase::Stop();
}

void GainOptimization::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos             = nullptr;
    PixelAlive::histos = nullptr;

    LOG(INFO) << GREEN << "[GainOptimization::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    GainOptimization::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "GainOptimization", histos, currentRun, PixelAlive::saveBinaryData);
    CalibBase::initializeFiles(histoFileName, "PixelAlive", PixelAlive::histos);
}

void GainOptimization::run()
{
    GainOptimization::bitWiseScanGlobal(frontEnd->gainReg, KrumCurrStart, KrumCurrStop, targetCharge, targetToT);

    // #######################################
    // # Fill Krummenacher Current container #
    // #######################################
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theKrumCurrContainer);
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    theKrumCurrContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                        static_cast<RD53*>(cChip)->getReg(frontEnd->gainReg);

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void GainOptimization::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
    GainOptimization::fillHisto();
    histos->process();

    PixelAlive::draw(false);

    if(doDisplay == true) myApp->Run(true);
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

void GainOptimization::bitWiseScanGlobal(const std::string& regName, uint16_t startValue, uint16_t stopValue, float targetCharge, uint16_t targetToT)
{
    float          tmp = 0;
    uint16_t       init;
    const uint16_t numberOfBits = floor(log2(stopValue - startValue + 1) + 1);

    DetectorDataContainer minDACcontainer;
    DetectorDataContainer midDACcontainer;
    DetectorDataContainer maxDACcontainer;
    DetectorDataContainer chargeContainer;

    DetectorDataContainer bestDACcontainer;
    DetectorDataContainer bestContainer;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, minDACcontainer, init = startValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, midDACcontainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, maxDACcontainer, init = (stopValue + 1));
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, chargeContainer);

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, bestDACcontainer, init = 0);
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, bestContainer, tmp);

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
                        // ##########################################
                        // # Find VCAL_HIGH to get target threshold #
                        // ##########################################
                        uint16_t vcal_med_setting  = static_cast<RD53*>(cChip)->getReg("VCAL_MED");
                        uint16_t vcal_high_setting = round(static_cast<RD53*>(cChip)->Charge2VCal(targetCharge)) + vcal_med_setting;
                        chargeContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                            vcal_high_setting;

                        LOG(INFO) << GREEN << "The target charge for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                                  << "/" << +cChip->getId() << RESET << GREEN "] is " << std::setprecision(1) << BOLDYELLOW << targetCharge << RESET << GREEN << " electrons" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Closest charge setting is " << BOLDYELLOW << "VCAL_HIGH" << BOLDBLUE << " = " << BOLDYELLOW << vcal_high_setting << BOLDBLUE << " for "
                                  << BOLDYELLOW << "VCAL_MED" << BOLDBLUE << " = " << BOLDYELLOW << vcal_med_setting << std::setprecision(-1) << RESET;

                        // ########################
                        // # Compute middle value #
                        // ########################
                        midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                            (minDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() +
                             maxDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>()) /
                            2;
                    }
        CalibBase::downloadNewDACvalues({&chargeContainer}, {"VCAL_HIGH"});
        CalibBase::downloadNewDACvalues({&midDACcontainer}, {regName.c_str()});

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
                        float newValue = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fPh;

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>();

                        if(fabs(newValue - targetToT) < fabs(oldValue - targetToT))
                        {
                            bestContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<float>() = newValue;
                            bestDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() =
                                midDACcontainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        }

                        if(newValue < targetToT)
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
    LOG(INFO) << BOLDMAGENTA << ">>> Best values <<<" << RESET;
    CalibBase::downloadNewDACvalues({&bestDACcontainer}, {regName.c_str()}, false, true);

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    PixelAlive::analyze();
}
