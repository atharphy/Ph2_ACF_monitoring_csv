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
    KrumCurrStart = this->findValueInSettings<double>("KrumCurrStart");
    KrumCurrStop  = this->findValueInSettings<double>("KrumCurrStop");
    doDisplay     = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip  = this->findValueInSettings<double>("UpdateChipCfg");

    colStart = std::max(Gain::colStart, frontEnd->colStart);
    colStop  = std::min(Gain::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "GainOptimization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << RESET << BOLDYELLOW << colStart << ", " << colStop
              << RESET << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    for(auto row = Gain::rowStart; row <= Gain::rowStop; row++)
        for(auto col = Gain::colStart; col <= Gain::colStop; col++) Gain::theChnGroupHandler->getRegionOfInterest().enableChannel(row, col);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += GainOptimization::getNumberIterations();
}

void GainOptimization::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[GainOptimization::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(Gain::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_GainOptimization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    GainOptimization::run();
    Tool::InformImDone();
    GainOptimization::analyze();
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    GainOptimization::sendData();

    Gain::sendData();
}

void GainOptimization::sendData()
{
    auto theKrumStream = this->prepareChipContainerStreamer<EmptyContainer, uint16_t>();

    if(fDQMStreamerEnabled == true)
        for(const auto cBoard: theKrumCurrContainer) theKrumStream->streamAndSendBoard(cBoard, fDQMStreamer);
}

void GainOptimization::Stop()
{
    LOG(INFO) << GREEN << "[GainOptimization::Stop] Stopping" << RESET;

    Tool::Stop();

    GainOptimization::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void GainOptimization::localConfigure(const std::string& histoFileName, int currentRun)
{
    histos        = nullptr;
    Gain::histos  = nullptr;
    theCurrentRun = currentRun;

    LOG(INFO) << GREEN << "[GainOptimization::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    GainOptimization::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<GainOptimizationHistograms>(histoFileName, "GainOptimization", histos, currentRun, Gain::saveBinaryData);
    CalibBase::initializeFiles<GainHistograms>(histoFileName, "Gain", Gain::histos);
}

void GainOptimization::run()
{
    GainOptimization::bitWiseScanGlobal(frontEnd->gainReg, Gain::targetCharge, KrumCurrStart, KrumCurrStop);

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
    CalibBase::chipErrorReport();
}

void GainOptimization::draw(bool saveData)
{
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        LOG(INFO) << BOLDBLUE << "\t--> GainOptimization saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    GainOptimization::fillHisto();
    histos->process();

    Gain::draw(false);

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

void GainOptimization::bitWiseScanGlobal(const std::string& regName, float target, uint16_t startValue, uint16_t stopValue)
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
        Gain::run();
        auto output = Gain::analyze();

        // ##############################################
        // # Send periodic data to monitor the progress #
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
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float  avg    = 0;
                        float  stdDev = 0;
                        size_t cnt    = 0;
                        for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                            for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                                if(cChip->getChannel<GainFit>(row, col).fChi2 > 0)
                                {
                                    auto ToTatTarget = Gain::gainFunction({cChip->getChannel<GainFit>(row, col).fInterceptLowQ,
                                                                           cChip->getChannel<GainFit>(row, col).fSlopeLowQ,
                                                                           cChip->getChannel<GainFit>(row, col).fInterceptHighQ,
                                                                           cChip->getChannel<GainFit>(row, col).fSlopeHighQ},
                                                                          target,
                                                                          frontEnd);
                                    avg += ToTatTarget;
                                    stdDev += ToTatTarget * ToTatTarget;
                                    cnt++;
                                }
                        avg              = cnt != 0 ? avg / cnt : 0;
                        stdDev           = (cnt != 0 ? stdDev / cnt : 0) - avg * avg;
                        stdDev           = (stdDev > 0 ? sqrt(stdDev) : 0);
                        float  newValue  = avg + NSTDEV * stdDev;
                        size_t targetToT = frontEnd->maxToTvalue;

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>();
                        if(fabs(newValue - targetToT) < fabs(oldValue - targetToT))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() = newValue;
                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if((newValue < targetToT) && (stdDev != 0))

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
    Gain::run();
    Gain::analyze();
}
