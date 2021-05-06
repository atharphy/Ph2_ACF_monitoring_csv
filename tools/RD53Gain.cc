/*!OA
  \file                  RD53Gain.cc
  \brief                 Implementaion of Gain scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Gain.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void Gain::ConfigureCalibration()
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
    RD53RunProgress::total() += Gain::getNumberIterations();

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void Gain::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[Gain::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Gain.raw", 'w');
        this->initializeWriteFileHandler();
    }

    Gain::run();
    Gain::analyze();
    Gain::saveChipRegisters(theCurrentRun);
    Gain::sendData();
}

void Gain::sendData()
{
    auto theOccStream              = prepareChannelContainerStreamer<OccupancyAndPh, uint16_t>("Occ");
    auto theGainAndInterceptStream = prepareChannelContainerStreamer<GainAndIntercept>("GainAndIntercept");

    if(fStreamerEnabled == true)
    {
        size_t index = 0;
        for(const auto theOccContainer: detectorContainerVector)
        {
            theOccStream.setHeaderElement(dacList[index] - offset);
            for(const auto cBoard: *theOccContainer) theOccStream.streamAndSendBoard(cBoard, fNetworkStreamer);
            index++;
        }

        if(theGainAndInterceptContainer != nullptr)
            for(const auto cBoard: *theGainAndInterceptContainer.get()) theGainAndInterceptStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void Gain::Stop()
{
    LOG(INFO) << GREEN << "[Gain::Stop] Stopping" << RESET;

    Tool::Stop();

    Gain::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void Gain::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[Gain::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    Gain::ConfigureCalibration();
    Gain::initializeFiles(fileRes_, currentRun);
}

void Gain::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_Gain.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new GainHistograms;
#endif
}

void Gain::run()
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
    Gain::chipErrorReport();
}

void Gain::draw(bool saveData)
{
    if(saveData == true) Gain::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if(saveData == true)
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> Gain saving histograms..." << RESET;
    }

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    Gain::fillHisto();
    histos->process();
    if(saveData == true) this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    if(saveData == true) this->CloseResultFile();
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
                        myString << this->fDirectoryName + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Gain_"
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

std::shared_ptr<DetectorDataContainer> Gain::analyze()
{
    float              gain, gainErr, intercept, interceptErr;
    std::vector<float> x(dacList.size(), 0);
    std::vector<float> y(dacList.size(), 0);
    std::vector<float> e(dacList.size(), 0);

    theGainAndInterceptContainer = std::make_shared<DetectorDataContainer>();
    ContainerFactory::copyAndInitStructure<GainAndIntercept>(*fDetectorContainer, *theGainAndInterceptContainer);
    DetectorDataContainer theMaxGainContainer;
    ContainerFactory::copyAndInitChip<float>(*fDetectorContainer, theMaxGainContainer, gain = 0);

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
                                for(auto i = 0u; i < dacList.size(); i++)
                                {
                                    x[i] = dacList[i] - offset;
                                    y[i] = detectorContainerVector[i]
                                               ->at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getChannel<OccupancyAndPh>(row, col)
                                               .fPh;
                                    e[i] = detectorContainerVector[i]
                                               ->at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getChannel<OccupancyAndPh>(row, col)
                                               .fPhError;
                                }

                                Gain::computeStats(x, y, e, gain, gainErr, intercept, interceptErr);

                                if(gain != 0)
                                {
                                    theGainAndInterceptContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainAndIntercept>(row, col)
                                        .fGain = gain;
                                    theGainAndInterceptContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainAndIntercept>(row, col)
                                        .fGainError = gainErr;
                                    theGainAndInterceptContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainAndIntercept>(row, col)
                                        .fIntercept = intercept;
                                    theGainAndInterceptContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainAndIntercept>(row, col)
                                        .fInterceptError = interceptErr;

                                    if(gain > theMaxGainContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>())
                                        theMaxGainContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() = gain;
                                }
                                else
                                    theGainAndInterceptContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainAndIntercept>(row, col)
                                        .fGain = RD53Shared::FITERROR;
                            }

                    index++;
                }

    theGainAndInterceptContainer->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

    for(const auto cBoard: *theGainAndInterceptContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Average gain for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << GREEN << "] is " << BOLDYELLOW << std::fixed << std::setprecision(4) << cChip->getSummary<GainAndIntercept, GainAndIntercept>().fGain << RESET
                              << GREEN << " (ToT/Delta_VCal)" << std::setprecision(-1) << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Highest gain: " << BOLDYELLOW << std::fixed << std::setprecision(4)
                              << theMaxGainContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<float>() << std::setprecision(-1)
                              << RESET;
                    RD53Shared::resetDefaultFloat();
                }

    return theGainAndInterceptContainer;
}

void Gain::fillHisto()
{
#ifdef __USE_ROOT__
    for(auto i = 0u; i < dacList.size(); i++) histos->fillOccupancy(*detectorContainerVector[i], dacList[i] - offset);
    histos->fillGainAndIntercept(*theGainAndInterceptContainer);
#endif
}

void Gain::computeStats(const std::vector<float>& x, const std::vector<float>& y, const std::vector<float>& e, float& gain, float& gainErr, float& intercept, float& interceptErr)
// ##############################################
// # Linear regression with least-square method #
// # Model: y = f(x) = q + mx                   #
// # Measurements with uncertainty: Y = AX + E  #
// ##############################################
// # A = (XtX)^(-1)XtY                          #
// # X = | 1 x1 |                               #
// #     | 1 x2 |                               #
// #     ...                                    #
// # A = | q |                                  #
// #     | m |                                  #
// ##############################################
{
    float a = 0, b = 0, c = 0, d = 0;
    float ai = 0, bi = 0, ci = 0, di = 0;
    float it = 0;
    float det;

    intercept    = 0;
    gain         = 0;
    interceptErr = 0;
    gainErr      = 0;

    // #######
    // # XtX #
    // #######
    for(auto i = 0u; i < x.size(); i++)
        if(e[i] != 0)
        {
            b += x[i];
            d += x[i] * x[i];
            it++;
        }
    a = it;
    c = b;

    // ##############
    // # (XtX)^(-1) #
    // ##############
    det = a * d - b * c;
    if(det != 0)
    {
        ai = d / det;
        bi = -b / det;
        ci = -c / det;
        di = a / det;

        // #################
        // # (XtX)^(-1)XtY #
        // #################
        for(auto i = 0u; i < x.size(); i++)
            if(e[i] != 0)
            {
                intercept += (ai + bi * x[i]) * y[i];
                gain += (ci + di * x[i]) * y[i];

                interceptErr += (ai + bi * x[i]) * (ai + bi * x[i]) * e[i] * e[i];
                gainErr += (ci + di * x[i]) * (ci + di * x[i]) * e[i] * e[i];
            }

        interceptErr = sqrt(interceptErr);
        gainErr      = sqrt(gainErr);
    }
}

void Gain::chipErrorReport() const
{
    for(const auto cBoard: *fDetectorContainer)
        for(auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->ChipErrorReport(cChip);
                }
}

void Gain::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> Gain saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
