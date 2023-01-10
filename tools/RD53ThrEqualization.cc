/*!
  \file                  RD53ThrEqualization.cc
  \brief                 Implementaion of threshold equalization
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThrEqualization.h"
#include "../HWDescription/RD53A.h"

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
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    resetTDAC          = this->findValueInSettings<double>("ResetTDAC");
    startValue         = this->findValueInSettings<double>("VCalHstart");
    stopValue          = this->findValueInSettings<double>("VCalHstop");
    startTDACGainValue = this->findValueInSettings<double>("TDACGainStart");
    stopTDACGainValue  = this->findValueInSettings<double>("TDACGainStop");
    TDACGainNSteps     = this->findValueInSettings<double>("TDACGainNSteps");
    doNSteps           = this->findValueInSettings<double>("DoNSteps");
    doDisplay          = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip       = this->findValueInSettings<double>("UpdateChipCfg");

    if(frontEnd == &RD53A::SYNC)
    {
        LOG(ERROR) << BOLDRED << "ThrEqualization cannot be used on the Synchronous FE, please change the selected columns" << RESET;
        exit(EXIT_FAILURE);
    }
    PixelAlive::colStart = std::max(PixelAlive::colStart, frontEnd->colStart);
    PixelAlive::colStop  = std::min(PixelAlive::colStop, frontEnd->colStop);
    LOG(INFO) << GREEN << "ThrEqualization will run on the " << RESET << BOLDYELLOW << frontEnd->name << RESET << GREEN << " FE, columns [" << GREEN << BOLDYELLOW << PixelAlive::colStart << ", "
              << PixelAlive::colStop << RESET << GREEN << "]" << RESET;

    // ########################
    // # Custom channel group #
    // ########################
    for(auto row = PixelAlive::rowStart; row <= PixelAlive::rowStop; row++)
        for(auto col = PixelAlive::colStart; col <= PixelAlive::colStop; col++) PixelAlive::theChnGroupHandler->getRegionOfInterest().enableChannel(row, col);

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const float step = (TDACGainNSteps != 0 ? (stopTDACGainValue - startTDACGainValue) / TDACGainNSteps : 0);
    for(auto i = 0u; i <= TDACGainNSteps; i++) dacList.push_back(startTDACGainValue + step * i);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += ThrEqualization::getNumberIterations();
}

void ThrEqualization::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[ThrEqualization::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_ThrEqualization.raw", 'w');
        this->initializeWriteFileHandler();
    }

    ThrEqualization::run();
    Tool::InformImDone();
    ThrEqualization::analyze();
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    ThrEqualization::sendData();

    PixelAlive::sendData();
}

void ThrEqualization::sendData()
{
    const size_t TDACGainSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theOccStream      = this->prepareChannelContainerStreamer<OccupancyAndPh>("Occ");
    auto theTDACStream     = this->prepareChannelContainerStreamer<uint16_t>("TDAC");
    auto theOccScanStream  = this->prepareChannelContainerStreamer<OccupancyAndPh, GenericDataArray<TDACGainSize>>("OccScan");
    auto theTDACGainStream = this->prepareChannelContainerStreamer<uint16_t>("TDACGain");

    if(fDQMStreamerEnabled == true)
    {
        for(const auto cBoard: *theOccContainer.get()) theOccStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theTDACContainer) theTDACStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theContainer) theOccScanStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theTDACGainContainer) theTDACGainStream->streamAndSendBoard(cBoard, fDQMStreamer);
    }
}

void ThrEqualization::Stop()
{
    LOG(INFO) << GREEN << "[ThrEqualization::Stop] Stopping" << RESET;

    Tool::Stop();

    ThrEqualization::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void ThrEqualization::localConfigure(const std::string& histoFileName, int currentRun)
{
    histos             = nullptr;
    PixelAlive::histos = nullptr;
    theCurrentRun      = currentRun;

    LOG(INFO) << GREEN << "[ThrEqualization::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    ThrEqualization::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<ThrEqualizationHistograms>(histoFileName, "ThrEqualization", histos, currentRun, PixelAlive::saveBinaryData);
    CalibBase::initializeFiles<PixelAliveHistograms>(histoFileName, "PixelAlive", PixelAlive::histos);
}

void ThrEqualization::run()
{
    if(TDACGainNSteps != 0)
    {
        // ###########################################
        // # Scan DAC and run threshold equalization #
        // ###########################################
        const size_t TDACGainSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;
        ContainerFactory::copyAndInitChip<GenericDataArray<TDACGainSize>>(*fDetectorContainer, theContainer);
        ThrEqualization::scanDac(frontEnd->TDACGainReg, dacList, &theContainer);

        // #######################################
        // # Run analysis to find best DAC value #
        // #######################################
        ThrEqualization::analyzeDuringRun();
    }

    // #########################
    // # Find global threshold #
    // #########################
    ThrEqualization::bitWiseScanGlobal("VCAL_HIGH", TARGETEFF, startValue, stopValue);

    // ##############################
    // # Run threshold equalization #
    // ##############################
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theTDACContainer);
    ThrEqualization::bitWiseScanLocal(TARGETEFF, true);

    // #################################################
    // # Fill TDAC container and mark enabled channels #
    // #################################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), "PIX_PORTAL", *theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

                    for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                        for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !this->getChannelGroupHandlerContainer()
                                                                                                                     ->at(cBoard->getIndex())
                                                                                                                     ->at(cOpticalGroup->getIndex())
                                                                                                                     ->at(cHybrid->getIndex())
                                                                                                                     ->at(cChip->getIndex())
                                                                                                                     ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                                                                                                     ->allChannelGroup()
                                                                                                                     ->isChannelEnabled(row, col))
                            {
                                theOccContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<OccupancyAndPh>(row, col).fStatus =
                                    RD53Shared::ISDISABLED;
                                theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                    frontEnd->nTDACvalues;
                            }
                }

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void ThrEqualization::draw(bool saveData)
{
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        LOG(INFO) << BOLDBLUE << "\t--> ThrEqualization saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    ThrEqualization::fillHisto();
    histos->process();

    PixelAlive::draw(false);

    if(doDisplay == true) myApp->Run(true);
#endif
}

void ThrEqualization::analyze()
{
    const size_t TDACcenter = (resetTDAC < 0 ? frontEnd->nTDACvalues / 2 : resetTDAC);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    float avgTDAC       = 0;
                    int   counter       = 0;
                    int   counterMinBin = 0;
                    int   counterMaxBin = 0;

                    for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                        for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                            if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->getChannelGroupHandlerContainer()
                                                                                                                   ->at(cBoard->getIndex())
                                                                                                                   ->at(cOpticalGroup->getIndex())
                                                                                                                   ->at(cHybrid->getIndex())
                                                                                                                   ->at(cChip->getIndex())
                                                                                                                   ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                                                                                                   ->allChannelGroup()
                                                                                                                   ->isChannelEnabled(row, col))
                            {
                                static_cast<RD53*>(cChip)->setTDAC(
                                    row, col, theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col));

                                avgTDAC += theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                counter++;

                                counterMinBin +=
                                    (theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) == 0 ? 1
                                                                                                                                                                                                 : 0);
                                counterMaxBin +=
                                    (theTDACContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) ==
                                             frontEnd->nTDACvalues - 1
                                         ? 1
                                         : 0);
                            }

                    avgTDAC /= counter;

                    // ###########################
                    // # Check TDAC distribution #
                    // ###########################
                    if(fabs(avgTDAC - TDACcenter) > MAXtdacDISTANCE)
                    {
                        LOG(WARNING) << BOLDRED << "Average TDAC distribution not centered around " << BOLDYELLOW << TDACcenter << BOLDRED << " (i.e. " << std::setprecision(1) << BOLDYELLOW << avgTDAC
                                     << BOLDRED << " - center > " << BOLDYELLOW << MAXtdacDISTANCE << BOLDRED << ") for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                                     << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "]" << std::setprecision(-1) << RESET;
                    }
                    else if((counterMaxBin == 0) && (counterMinBin == 0))
                        LOG(WARNING) << BOLDRED << "TDAC distribution is most likely empty for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                                     << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "]" << RESET;
                    else if(((frontEnd->nTDACvalues * counterMaxBin / (counterMinBin + counterMaxBin)) - TDACcenter) > MAXtdacDISTANCE)
                    {
                        LOG(WARNING) << BOLDRED << "Min and Max TDAC bins are not balanced (i.e. low TDAC value with " << std::setprecision(1) << BOLDYELLOW << counterMinBin << BOLDRED
                                     << " entries and high TDAC value with " << BOLDYELLOW << counterMaxBin << BOLDRED << " entries) for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW
                                     << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDRED << "]" << std::setprecision(-1) << RESET;
                    }
                }
}

void ThrEqualization::analyzeDuringRun()
{
    const size_t TDACGainSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theTDACGainContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    float best   = 1;
                    int   regVal = 0;

                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        auto current = round(theContainer.at(cBoard->getIndex())
                                                 ->at(cOpticalGroup->getIndex())
                                                 ->at(cHybrid->getIndex())
                                                 ->at(cChip->getIndex())
                                                 ->getSummary<GenericDataArray<TDACGainSize>>()
                                                 .data[i] /
                                             RD53Shared::PRECISION) *
                                       RD53Shared::PRECISION;
                        if(current < best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best TDAC gain for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId()
                              << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " <<<" << RESET;

                    // ########################################################
                    // # Fill TDAC gain container and download new DAC values #
                    // ########################################################
                    theTDACGainContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->TDACGainReg, regVal);
                }
}

void ThrEqualization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(*theOccContainer.get());
    histos->fillTDAC(theTDACContainer);
    histos->fillOccupancyScan(theContainer);
    histos->fillTDACGain(theTDACGainContainer);
#endif
}

void ThrEqualization::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer)
{
    const size_t TDACGainSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(auto i = 0u; i < dacList.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " broadcast value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regName, dacList[i]);

        // #########################
        // # Find global threshold #
        // #########################
        ThrEqualization::bitWiseScanGlobal("VCAL_HIGH", TARGETEFF, startValue, stopValue);

        // ##############################
        // # Run threshold equalization #
        // ##############################
        ThrEqualization::bitWiseScanLocal(TARGETEFF, false);

        // #####################
        // # Compute next step #
        // #####################
        for(const auto cBoard: *theOccContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        // #######################
                        // # Build discriminator #
                        // #######################
                        float  stdDev = 0;
                        size_t cnt    = 0;
                        for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                            for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                                if(fDetectorContainer->at(cBoard->getIndex())
                                       ->at(cOpticalGroup->getIndex())
                                       ->at(cHybrid->getIndex())
                                       ->at(cChip->getIndex())
                                       ->getChipOriginalMask()
                                       ->isChannelEnabled(row, col) &&
                                   this->getChannelGroupHandlerContainer()
                                       ->at(cBoard->getIndex())
                                       ->at(cOpticalGroup->getIndex())
                                       ->at(cHybrid->getIndex())
                                       ->at(cChip->getIndex())
                                       ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                       ->allChannelGroup()
                                       ->isChannelEnabled(row, col) &&
                                   cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy >= 0)
                                {
                                    auto value = cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy;
                                    stdDev += (value - TARGETEFF) * (value - TARGETEFF);
                                    cnt++;
                                }
                        stdDev = (cnt != 0 ? sqrt(stdDev / cnt) : 0);

                        // ###############
                        // # Save output #
                        // ###############
                        theContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TDACGainSize>>().data[i] =
                            stdDev;

                        // ##############################################
                        // # Send periodic data to monitor the progress #
                        // ##############################################
                        ThrEqualization::sendData();
                    }
    }
}

void ThrEqualization::bitWiseScanGlobal(const std::string& regName, float target, uint16_t startValue, uint16_t stopValue)
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

                        // ########################
                        // # Save best DAC values #
                        // ########################
                        float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>();

                        if(fabs(newValue - target) < fabs(oldValue - target))
                        {
                            bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() = newValue;

                            bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                                midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        }

                        if(newValue > target)

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

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");
}

void ThrEqualization::bitWiseScanLocal(float target, bool updateDACs)
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
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, bestContainer, tmp = (target < 0.5 ? 1 : 0));

    // ############################
    // # Read DAC starting values #
    // ############################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    this->fReadoutChipInterface->ReadChipAllLocalReg(
                        static_cast<RD53*>(cChip), "", *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

    for(auto i = 0u; i <= (doNSteps != 0 ? doNSteps : numberOfBits); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                        this->fReadoutChipInterface->WriteChipAllLocalReg(
                            static_cast<RD53*>(cChip), "", *midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));

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
                        for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                            for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++)
                                if(fDetectorContainer->at(cBoard->getIndex())
                                       ->at(cOpticalGroup->getIndex())
                                       ->at(cHybrid->getIndex())
                                       ->at(cChip->getIndex())
                                       ->getChipOriginalMask()
                                       ->isChannelEnabled(row, col) &&
                                   this->getChannelGroupHandlerContainer()
                                       ->at(cBoard->getIndex())
                                       ->at(cOpticalGroup->getIndex())
                                       ->at(cHybrid->getIndex())
                                       ->at(cChip->getIndex())
                                       ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                       ->allChannelGroup()
                                       ->isChannelEnabled(row, col))
                                {
                                    // #######################
                                    // # Build discriminator #
                                    // #######################
                                    float newValue = cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy;

                                    // ########################
                                    // # Save best DAC values #
                                    // ########################
                                    float oldValue = bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<float>(row, col);

                                    if(fabs(newValue - target) <= fabs(oldValue - target))
                                    {
                                        bestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<float>(row, col) = newValue;
                                        bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                    }

                                    if(newValue < target)

                                        minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                    else

                                        maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);

                                    if(doNSteps == 0)
                                        midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) =
                                            (minDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col) +
                                             maxDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col)) /
                                            2;
                                    else
                                    {
                                        auto& midDAC =
                                            midDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<uint16_t>(row, col);
                                        if(newValue < target)
                                        {
                                            midDAC += 1;
                                            if(midDAC > (frontEnd->nTDACvalues - 1)) midDAC = frontEnd->nTDACvalues - 1;
                                        }
                                        else if(midDAC - 1 < 0)
                                            midDAC = 0;
                                        else
                                            midDAC -= 1;
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
                        static_cast<RD53*>(cChip), "", *bestDACcontainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
                    if(updateDACs == true) static_cast<RD53*>(cChip)->copyMaskToDefault("td");
                }

    // ################
    // # Run analysis #
    // ################
    PixelAlive::run();
    theOccContainer = PixelAlive::analyze();

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in td");
}
