/*!
  \file                  RD53PixelAlive.cc
  \brief                 Implementaion of PixelAlive scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53PixelAlive.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void PixelAlive::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    nEvtsBurst     = this->findValueInSettings("nEvtsBurst") < nEvents ? this->findValueInSettings("nEvtsBurst") : nEvents;
    injType        = this->findValueInSettings("INJtype");
    nHITxCol       = this->findValueInSettings("nHITxCol");
    doFast         = this->findValueInSettings("DoFast");
    thrOccupancy   = this->findValueInSettings("TargetOcc");
    unstuckPixels  = this->findValueInSettings("UnstuckPixels");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    // ################################
    // # Custom channel group handler #
    // ################################
    ChannelGroup<RD53::nRows, RD53::nCols> customChannelGroup;
    customChannelGroup.disableAllChannels();

    for(auto row = rowStart; row <= rowStop; row++)
        for(auto col = colStart; col <= colStop; col++) customChannelGroup.enableChannel(row, col);

    theChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(
        customChannelGroup, injType != INJtype::None ? (doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups) : RD53GroupType::AllPixels, nHITxCol);
    theChnGroupHandler->setCustomChannelGroup(customChannelGroup);

    // ######################
    // # Set injection type #
    // ######################
    size_t inj = 0;
    if(injType == INJtype::Digital) inj = 1 << static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY");
    size_t maxDelay = RD53Shared::setBits(static_cast<RD53*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0))->getNumberOfBits("INJECTION_SELECT_DELAY"));

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto val = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT");
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "INJECTION_SELECT", inj | (val & maxDelay));
                }

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += PixelAlive::getNumberIterations();

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void PixelAlive::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[PixelAlive::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_PixelAlive.raw", 'w');
        this->initializeWriteFileHandler();
    }

    PixelAlive::run();
    PixelAlive::analyze();
    PixelAlive::saveChipRegisters(theCurrentRun);
    PixelAlive::sendData();
}

void PixelAlive::sendData()
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    auto theOccStream   = prepareChannelContainerStreamer<OccupancyAndPh>("Occ");
    auto theBCIDStream  = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<BCIDsize>>("BCID");
    auto theTrgIDStream = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TrgIDsize>>("TrgID");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: *theOccContainer.get()) theOccStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theBCIDContainer) theBCIDStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTrgIDContainer) theTrgIDStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void PixelAlive::Stop()
{
    LOG(INFO) << GREEN << "[PixelAlive::Stop] Stopping" << RESET;

    Tool::Stop();

    PixelAlive::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void PixelAlive::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[PixelAlive::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    PixelAlive::ConfigureCalibration();
    PixelAlive::initializeFiles(fileRes_, currentRun);
}

void PixelAlive::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_PixelAlive.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new PixelAliveHistograms;
#endif
}

void PixelAlive::run()
{
    theOccContainer              = std::shared_ptr<DetectorDataContainer>(new DetectorDataContainer());
    this->fDetectorDataContainer = theOccContainer.get();
    ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, *this->fDetectorDataContainer);

    this->fChannelGroupHandler = theChnGroupHandler.get();
    this->SetTestPulse(injType);
    this->fMaskChannelsFromOtherGroups = true;
    this->measureData(nEvents, nEvtsBurst);

    // ################
    // # Error report #
    // ################
    PixelAlive::chipErrorReport();
}

void PixelAlive::draw(bool saveData)
{
    if(saveData == true) PixelAlive::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((saveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> PixelAlive saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    PixelAlive::fillHisto();
    histos->process();
    if(saveData == true) this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

std::shared_ptr<DetectorDataContainer> PixelAlive::analyze()
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    theBCIDContainer.reset();
    theTrgIDContainer.reset();
    ContainerFactory::copyAndInitChip<GenericDataArray<BCIDsize>>(*fDetectorContainer, theBCIDContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<TrgIDsize>>(*fDetectorContainer, theTrgIDContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    size_t nMaskedPixelsPerCalib = 0;

                    LOG(INFO) << GREEN << "Average occupancy for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET << GREEN << "] is " << BOLDYELLOW
                              << theOccContainer->at(cBoard->getIndex())
                                     ->at(cOpticalGroup->getIndex())
                                     ->at(cHybrid->getIndex())
                                     ->at(cChip->getIndex())
                                     ->getSummary<GenericDataVector, OccupancyAndPh>()
                                     .fOccupancy
                              << RESET;

                    static_cast<RD53*>(cChip)->copyMaskFromDefault();

                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                            {
                                float occupancy = theOccContainer->at(cBoard->getIndex())
                                                      ->at(cOpticalGroup->getIndex())
                                                      ->at(cHybrid->getIndex())
                                                      ->at(cChip->getIndex())
                                                      ->getChannel<OccupancyAndPh>(row, col)
                                                      .fOccupancy;
                                bool enable = (injType == INJtype::None ? occupancy <= thrOccupancy : occupancy >= thrOccupancy);
                                if(unstuckPixels == false)
                                    static_cast<RD53*>(cChip)->enablePixel(row, col, enable);
                                else if(enable == false)
                                    static_cast<RD53*>(cChip)->setTDAC(row, col, 0);
                                if(enable == false) nMaskedPixelsPerCalib++;
                            }

                    if(unstuckPixels == false)
                    {
                        LOG(INFO) << BOLDBLUE << "\t--> Number of potentially " << BOLDYELLOW << "masked" << BOLDBLUE << " pixels in this iteration: " << BOLDYELLOW << nMaskedPixelsPerCalib << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Total number of potentially masked pixels: " << BOLDYELLOW << static_cast<RD53*>(cChip)->getNbMaskedPixels() << RESET;
                    }
                    else
                        LOG(INFO) << BOLDBLUE << "\t--> Number of potentially " << BOLDYELLOW << "unstuck" << BOLDBLUE << " pixels in this iteration: " << BOLDYELLOW << nMaskedPixelsPerCalib << RESET;

                    // ######################################
                    // # Copy register values for streaming #
                    // ######################################
                    for(auto i = 0u; i < BCIDsize; i++)
                        theBCIDContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<BCIDsize>>().data[i] = 0;
                    for(auto i = 0u; i < TrgIDsize; i++)
                        theTrgIDContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TrgIDsize>>().data[i] = 0;

                    for(auto i = 1u; i < theOccContainer->at(cBoard->getIndex())
                                             ->at(cOpticalGroup->getIndex())
                                             ->at(cHybrid->getIndex())
                                             ->at(cChip->getIndex())
                                             ->getSummary<GenericDataVector, OccupancyAndPh>()
                                             .data1.size();
                        i++)
                    {
                        long int deltaBCID = theOccContainer->at(cBoard->getIndex())
                                                 ->at(cOpticalGroup->getIndex())
                                                 ->at(cHybrid->getIndex())
                                                 ->at(cChip->getIndex())
                                                 ->getSummary<GenericDataVector, OccupancyAndPh>()
                                                 .data1[i] -
                                             theOccContainer->at(cBoard->getIndex())
                                                 ->at(cOpticalGroup->getIndex())
                                                 ->at(cHybrid->getIndex())
                                                 ->at(cChip->getIndex())
                                                 ->getSummary<GenericDataVector, OccupancyAndPh>()
                                                 .data1[i - 1];
                        deltaBCID += (deltaBCID >= 0 ? 0 : RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1);
                        if(deltaBCID >= int(BCIDsize))
                            LOG(ERROR) << BOLDBLUE << "[PixelAlive::analyze] " << BOLDRED << "deltaBCID out of range: " << BOLDYELLOW << deltaBCID << RESET;
                        else
                            theBCIDContainer.at(cBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cHybrid->getIndex())
                                ->at(cChip->getIndex())
                                ->getSummary<GenericDataArray<BCIDsize>>()
                                .data[deltaBCID]++;
                    }

                    for(auto i = 1u; i < theOccContainer->at(cBoard->getIndex())
                                             ->at(cOpticalGroup->getIndex())
                                             ->at(cHybrid->getIndex())
                                             ->at(cChip->getIndex())
                                             ->getSummary<GenericDataVector, OccupancyAndPh>()
                                             .data2.size();
                        i++)
                    {
                        long int deltaTrgID = theOccContainer->at(cBoard->getIndex())
                                                  ->at(cOpticalGroup->getIndex())
                                                  ->at(cHybrid->getIndex())
                                                  ->at(cChip->getIndex())
                                                  ->getSummary<GenericDataVector, OccupancyAndPh>()
                                                  .data2[i] -
                                              theOccContainer->at(cBoard->getIndex())
                                                  ->at(cOpticalGroup->getIndex())
                                                  ->at(cHybrid->getIndex())
                                                  ->at(cChip->getIndex())
                                                  ->getSummary<GenericDataVector, OccupancyAndPh>()
                                                  .data2[i - 1];
                        deltaTrgID += (deltaTrgID >= 0 ? 0 : RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1);
                        if(deltaTrgID >= int(TrgIDsize))
                            LOG(ERROR) << BOLDBLUE << "[PixelAlive::analyze] " << BOLDRED << "deltaTrgID out of range: " << BOLDYELLOW << deltaTrgID << RESET;
                        else
                            theTrgIDContainer.at(cBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cHybrid->getIndex())
                                ->at(cChip->getIndex())
                                ->getSummary<GenericDataArray<TrgIDsize>>()
                                .data[deltaTrgID]++;
                    }
                }

    return theOccContainer;
}

void PixelAlive::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(*theOccContainer.get());
    histos->fillBCID(theBCIDContainer);
    histos->fillTrgID(theTrgIDContainer);
#endif
}

void PixelAlive::chipErrorReport() const
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

void PixelAlive::saveChipRegisters(int currentRun)
{
    const std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + RD53Shared::RESULTDIR);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> PixelAlive saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
