/*!
  \file                  RD53Physics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Physics.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void Physics::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings<double>("ROWstart");
    rowStop        = this->findValueInSettings<double>("ROWstop");
    colStart       = this->findValueInSettings<double>("COLstart");
    colStop        = this->findValueInSettings<double>("COLstop");
    nTRIGxEvent    = this->findValueInSettings<double>("nTRIGxEvent");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip   = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings<double>("SaveBinaryData");

    // ################################
    // # Custom channel group handler #
    // ################################
    ChannelGroup<RD53::nRows, RD53::nCols> customChannelGroup;
    customChannelGroup.disableAllChannels();

    for(auto row = rowStart; row <= rowStop; row++)
        for(auto col = colStart; col <= colStop; col++) customChannelGroup.enableChannel(row, col);

    theChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(customChannelGroup, RD53GroupType::AllPixels);
    theChnGroupHandler->setCustomChannelGroup(customChannelGroup);

    // ##############################
    // # Initialize data containers #
    // ##############################
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;
    ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, theOccContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<BCIDsize>>(*fDetectorContainer, theBCIDContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<TrgIDsize>>(*fDetectorContainer, theTrgIDContainer);

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void Physics::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[Physics::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Physics.raw", 'w');
        this->initializeWriteFileHandler();
    }

    // ##############################
    // # Download mask to the chips #
    // ##############################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid) fReadoutChipInterface->maskChannelsAndSetInjectionSchema(cChip, theChnGroupHandler->allChannelGroup(), true, false);

    for(const auto cBoard: *fDetectorContainer) static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ChipReSync();
    SystemController::Start(theCurrentRun);

    numberOfEventsPerRun = 0;
    Physics::run();
}

void Physics::sendBoardData(const BoardContainer* cBoard)
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    auto theOccStream   = prepareChannelContainerStreamer<OccupancyAndPh>("Occ");
    auto theBCIDStream  = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<BCIDsize>>("BCID");
    auto theTrgIDStream = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TrgIDsize>>("TrgID");

    if(fStreamerEnabled == true)
    {
        theOccStream.streamAndSendBoard(theOccContainer.at(cBoard->getIndex()), fNetworkStreamer);
        theBCIDStream.streamAndSendBoard(theBCIDContainer.at(cBoard->getIndex()), fNetworkStreamer);
        theTrgIDStream.streamAndSendBoard(theTrgIDContainer.at(cBoard->getIndex()), fNetworkStreamer);
    }
}

void Physics::Stop()
{
    LOG(INFO) << GREEN << "[Physics::Stop] Stopping" << RESET;

    Tool::Stop();

    // ################
    // # Error report #
    // ################
    Physics::chipErrorReport();

    Physics::draw();
    this->closeFileHandler();
    LOG(INFO) << GREEN << "[Physics::Stop] Stopped" << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Total number of recorded events: " << BOLDYELLOW << numberOfEventsPerRun << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Total number of received triggers: " << BOLDYELLOW << numberOfEventsPerRun / nTRIGxEvent << RESET;
}

void Physics::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[Physics::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }

    Physics::ConfigureCalibration();
    Physics::initializeFiles(fileRes_, currentRun);
}

void Physics::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_Physics.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new PhysicsHistograms;
    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)) this->InitResultFile(fileRes);
    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void Physics::run()
{
    std::unique_lock<std::mutex> theGuard(theMtx, std::defer_lock);
    while(this->fKeepRunning == true)
    {
        RD53Event::decodedEvents.clear();
        Physics::analyze();
        theGuard.lock();
        genericEvtConverter(RD53Event::decodedEvents);
        numberOfEventsPerRun += RD53Event::decodedEvents.size();
        theGuard.unlock();
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    }
}

void Physics::draw()
{
    Physics::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    LOG(INFO) << BOLDBLUE << "\t--> Physics saving histograms..." << RESET;

    Physics::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void Physics::analyze(bool doReadBinary)
{
    for(const auto cBoard: *fDetectorContainer)
    {
        size_t dataSize = 0;

        if(doReadBinary == false)
            dataSize = SystemController::ReadData(cBoard, true);
        else
        {
            dataSize = 1;
            SystemController::DecodeData(cBoard, {}, 0, cBoard->getBoardType());
        }

        if(dataSize != 0)
        {
#ifdef __USE_ROOT__
            Physics::fillHisto();
#endif
            Physics::fillDataContainer(cBoard);
            Physics::sendBoardData(cBoard);
        }
    }
}

void Physics::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theOccContainer);
    histos->fillBCID(theBCIDContainer);
    histos->fillTrgID(theTrgIDContainer);
#endif
}

void Physics::fillDataContainer(BeBoard* theBoard)
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;
    const auto   cBoard    = theOccContainer.at(theBoard->getIndex());

    // ###################
    // # Clear container #
    // ###################
    Physics::clearContainers(theBoard);

    // ###################
    // # Fill containers #
    // ###################
    const std::vector<Event*>& events = SystemController::GetEvents();
    for(const auto& event: events) event->fillDataContainer(cBoard, theChnGroupHandler->allChannelGroup());

    // ######################################
    // # Copy register values for streaming #
    // ######################################
    for(const auto cOpticalGroup: *cBoard)
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
            {
                for(auto i = 1u; i < cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.size(); i++)
                {
                    int deltaBCID = cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1[i] - cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1[i - 1];
                    deltaBCID += (deltaBCID >= 0 ? 0 : RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1);
                    if(deltaBCID >= int(BCIDsize))
                        LOG(ERROR) << BOLDBLUE << "[Physics::fillDataContainer] " << BOLDRED << "deltaBCID out of range: " << BOLDYELLOW << deltaBCID << RESET;
                    else
                        theBCIDContainer.at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<BCIDsize>>()
                            .data[deltaBCID]++;
                }

                for(auto i = 1u; i < cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.size(); i++)
                {
                    int deltaTrgID = cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2[i] - cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2[i - 1];
                    deltaTrgID += (deltaTrgID >= 0 ? 0 : RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1);
                    if(deltaTrgID >= int(TrgIDsize))
                        LOG(ERROR) << BOLDBLUE << "[Physics::fillDataContainer] " << BOLDRED << "deltaTrgID out of range: " << BOLDYELLOW << deltaTrgID << RESET;
                    else
                        theTrgIDContainer.at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<TrgIDsize>>()
                            .data[deltaTrgID]++;
                }
            }

    // #######################
    // # Normalize container #
    // #######################
    for(const auto cOpticalGroup: *cBoard)
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
                for(auto row = 0u; row < RD53::nRows; row++)
                    for(auto col = 0u; col < RD53::nCols; col++) cChip->getChannel<OccupancyAndPh>(row, col).normalize(events.size(), true);
}

void Physics::chipErrorReport() const
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

void Physics::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> Physics saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId()
                              << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}

void Physics::clearContainers(BeBoard* theBoard)
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53EvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;
    const auto   cBoard    = theOccContainer.at(theBoard->getIndex());

    // ####################
    // # Clear containers #
    // ####################
    for(const auto cOpticalGroup: *cBoard)
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
            {
                for(auto row = 0u; row < RD53::nRows; row++)
                    for(auto col = 0u; col < RD53::nCols; col++)
                    {
                        cChip->getChannel<OccupancyAndPh>(row, col).fOccupancy   = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).fPh          = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).fPhError     = 0;
                        cChip->getChannel<OccupancyAndPh>(row, col).readoutError = false;
                    }

                cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.clear();
                cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.clear();

                for(auto i = 0u; i < BCIDsize; i++)
                    theBCIDContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<BCIDsize>>().data[i] = 0;
                for(auto i = 0u; i < TrgIDsize; i++)
                    theTrgIDContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TrgIDsize>>().data[i] = 0;
            }
}
