/*!
  \file                  RD53SCurve.cc
  \brief                 Implementaion of SCurve scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53SCurve.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void SCurve::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    startValue     = this->findValueInSettings("VCalHstart");
    stopValue      = this->findValueInSettings("VCalHstop");
    nSteps         = this->findValueInSettings("VCalHnsteps");
    offset         = this->findValueInSettings("VCalMED");
    nHITxCol       = this->findValueInSettings("nHITxCol");
    doFast         = this->findValueInSettings("DoFast");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    // ########################
    // # Custom channel group #
    // ########################
    ChannelGroup<RD53::nRows, RD53::nCols> customChannelGroup;
    customChannelGroup.disableAllChannels();

    for(auto row = rowStart; row <= rowStop; row++)
        for(auto col = colStart; col <= colStop; col++) customChannelGroup.enableChannel(row, col);

    theChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(customChannelGroup, doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol);
    theChnGroupHandler->setCustomChannelGroup(customChannelGroup);

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const float step = (stopValue - startValue) / nSteps;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + step * i);

    // #################################
    // # Initialize container recycler #
    // #################################
    theRecyclingBin.setDetectorContainer(fDetectorContainer);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += SCurve::getNumberIterations();

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void SCurve::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[SCurve::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_SCurve.raw", 'w');
        this->initializeWriteFileHandler();
    }

    SCurve::run();
    SCurve::analyze();
    SCurve::saveChipRegisters(theCurrentRun);
    SCurve::sendData();
}

void SCurve::sendData()
{
    auto theOccStream         = prepareChannelContainerStreamer<OccupancyAndPh, uint16_t>("Occ");
    auto theThrAndNoiseStream = prepareChannelContainerStreamer<ThresholdAndNoise>("ThrAndNoise");

    if(fStreamerEnabled == true)
    {
        size_t index = 0;
        for(const auto theOccContainer: detectorContainerVector)
        {
            theOccStream.setHeaderElement(dacList[index] - offset);
            for(const auto cBoard: *theOccContainer) theOccStream.streamAndSendBoard(cBoard, fNetworkStreamer);
            index++;
        }

        if(theThresholdAndNoiseContainer != nullptr)
            for(const auto cBoard: *theThresholdAndNoiseContainer.get()) theThrAndNoiseStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void SCurve::Stop()
{
    LOG(INFO) << GREEN << "[SCurve::Stop] Stopping" << RESET;

    Tool::Stop();

    SCurve::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void SCurve::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[SCurve::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    SCurve::ConfigureCalibration();
    SCurve::initializeFiles(fileRes_, currentRun);
}

void SCurve::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_SCurve.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new SCurveHistograms;
#endif
}

void SCurve::run()
{
    // ##########################
    // # Set new VCAL_MED value #
    // ##########################
    for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "VCAL_MED", offset);

    for(auto container: detectorContainerVector) theRecyclingBin.free(container);
    detectorContainerVector.clear();
    for(auto i = 0u; i < dacList.size(); i++) detectorContainerVector.push_back(theRecyclingBin.get(&ContainerFactory::copyAndInitStructure<OccupancyAndPh>, OccupancyAndPh()));

    this->fChannelGroupHandler = theChnGroupHandler.get();
    this->SetBoardBroadcast(true);
    this->SetTestPulse(true);
    this->fMaskChannelsFromOtherGroups = true;
    this->scanDac("VCAL_HIGH", dacList, nEvents, detectorContainerVector);

    // #########################
    // # Mark enabled channels #
    // #########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                                for(auto i = 0u; i < dacList.size(); i++)
                                    detectorContainerVector[i]
                                        ->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<OccupancyAndPh>(row, col)
                                        .fOccupancy = RD53Shared::ISDISABLED;

    // ################
    // # Error report #
    // ################
    SCurve::chipErrorReport();
}

void SCurve::draw()
{
    SCurve::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> SCurve saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    SCurve::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    this->CloseResultFile();
#endif

    // #####################
    // # @TMP@ : CalibFile #
    // #####################
    if(saveBinaryData == true)
    {
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        std::stringstream myString;
                        myString.clear();
                        myString.str("");
                        myString << this->fDirectoryName + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_SCurve_"
                                 << "B" << std::setfill('0') << std::setw(2) << cBoard->getId() << "_"
                                 << "O" << std::setfill('0') << std::setw(2) << cOpticalGroup->getId() << "_"
                                 << "M" << std::setfill('0') << std::setw(2) << cHybrid->getId() << "_"
                                 << "C" << std::setfill('0') << std::setw(2) << cChip->getId() << ".dat";
                        std::ofstream fileOutID(myString.str(), std::ios::out);
                        for(auto i = 0u; i < dacList.size(); i++)
                        {
                            fileOutID << "Iteration " << i << " --- reg = " << dacList[i] - offset << std::endl;
                            for(auto row = 0u; row < RD53::nRows; row++)
                                for(auto col = 0u; col < RD53::nCols; col++)
                                    if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                                        fileOutID << "r " << row << " c " << col << " h "
                                                  << detectorContainerVector[i]
                                                             ->at(cBoard->getIndex())
                                                             ->at(cOpticalGroup->getIndex())
                                                             ->at(cHybrid->getIndex())
                                                             ->at(cChip->getIndex())
                                                             ->getChannel<OccupancyAndPh>(row, col)
                                                             .fOccupancy *
                                                         nEvents
                                                  << " a "
                                                  << detectorContainerVector[i]
                                                         ->at(cBoard->getIndex())
                                                         ->at(cOpticalGroup->getIndex())
                                                         ->at(cHybrid->getIndex())
                                                         ->at(cChip->getIndex())
                                                         ->getChannel<OccupancyAndPh>(row, col)
                                                         .fPh
                                                  << std::endl;
                        }
                        fileOutID.close();
                    }
    }
}

std::shared_ptr<DetectorDataContainer> SCurve::analyze()
{
    float              nHits, mean, rms;
    std::vector<float> measurements(dacList.size(), 0);

    theThresholdAndNoiseContainer = std::make_shared<DetectorDataContainer>();
    ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *theThresholdAndNoiseContainer);
    DetectorDataContainer theMaxThresholdContainer;
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, theMaxThresholdContainer, mean = 0);

    size_t index = 0;
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                            {
                                for(auto i = 1u; i < dacList.size(); i++)
                                    measurements[i - 1] = fabs(detectorContainerVector[i]
                                                                   ->at(cBoard->getIndex())
                                                                   ->at(cOpticalGroup->getIndex())
                                                                   ->at(cHybrid->getIndex())
                                                                   ->at(cChip->getIndex())
                                                                   ->getChannel<OccupancyAndPh>(row, col)
                                                                   .fOccupancy -
                                                               detectorContainerVector[i - 1]
                                                                   ->at(cBoard->getIndex())
                                                                   ->at(cOpticalGroup->getIndex())
                                                                   ->at(cHybrid->getIndex())
                                                                   ->at(cChip->getIndex())
                                                                   ->getChannel<OccupancyAndPh>(row, col)
                                                                   .fOccupancy);

                                SCurve::computeStats(measurements, offset, nHits, mean, rms);

                                if((rms > 0) && (nHits > 0) && (isnan(rms) == false))
                                {
                                    theThresholdAndNoiseContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<ThresholdAndNoise>(row, col)
                                        .fThreshold = mean;
                                    theThresholdAndNoiseContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<ThresholdAndNoise>(row, col)
                                        .fThresholdError = rms / sqrt(nHits);
                                    theThresholdAndNoiseContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<ThresholdAndNoise>(row, col)
                                        .fNoise = rms;

                                    if(mean > theMaxThresholdContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>())
                                        theMaxThresholdContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() = mean;
                                }
                                else
                                    theThresholdAndNoiseContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<ThresholdAndNoise>(row, col)
                                        .fNoise = RD53Shared::FITERROR;
                            }

                    index++;
                }

    theThresholdAndNoiseContainer->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

    for(const auto cBoard: *theThresholdAndNoiseContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Average threshold for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << GREEN << "] is " << BOLDYELLOW << std::fixed << std::setprecision(1) << cChip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fThreshold
                              << RESET << GREEN << " (Delta_VCal)" << std::setprecision(-1) << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Highest threshold: " << BOLDYELLOW << std::fixed << std::setprecision(1)
                              << theMaxThresholdContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>()
                              << std::setprecision(-1) << RESET;
                    RD53Shared::resetDefaultFloat();
                }

    return theThresholdAndNoiseContainer;
}

void SCurve::fillHisto()
{
#ifdef __USE_ROOT__
    for(auto i = 0u; i < dacList.size(); i++) histos->fillOccupancy(*detectorContainerVector[i], dacList[i] - offset);
    histos->fillThrAndNoise(*theThresholdAndNoiseContainer);
#endif
}

void SCurve::computeStats(const std::vector<float>& measurements, int offset, float& nHits, float& mean, float& rms)
{
    float mean2  = 0;
    float weight = 0;
    mean         = 0;

    for(auto i = 0u; i < dacList.size() - 1; i++)
    {
        auto dacCenter = (dacList[i] + dacList[i + 1]) / 2.;

        mean += measurements[i] * (dacCenter - offset);
        weight += measurements[i];
        mean2 += measurements[i] * (dacCenter - offset) * (dacCenter - offset);
    }

    nHits = weight * nEvents;

    if(weight != 0)
    {
        mean /= weight;
        rms = sqrt((mean2 / weight - mean * mean) * weight / (weight - 1. / nEvents));
    }
    else
    {
        mean = 0;
        rms  = 0;
    }
}

void SCurve::chipErrorReport()
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

void SCurve::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> SCurve saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
