/*!
  \file                  RD53Physics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Physics.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/StartInfo.h"

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
    dataOutputDir  = this->findValueInSettings<std::string>("DataOutputDir", "");
    frontEnd       = RD53Shared::firstChip->getFEtype(colStart, colStop);

    // ################################
    // # Custom channel group handler #
    // ################################
    theChnGroupHandler =
        std::make_shared<RD53ChannelGroupHandler>(rowStart, rowStop, colStart, colStop, RD53Shared::firstChip->getNRows(), RD53Shared::firstChip->getNCols(), RD53GroupType::AllPixels);
    this->setChannelGroupHandler(theChnGroupHandler);

    // ##############################
    // # Initialize data containers #
    // ##############################
    ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, theOccContainer);
    ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, theBCIDContainer);
    ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, theTrgIDContainer);
}

void Physics::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[Physics::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
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

    StartInfo theStartInfo;
    theStartInfo.setRunNumber(theCurrentRun);
    SystemController::Start(theStartInfo);

    numberOfEventsPerRun  = 0;
    corruptedEventCounter = 0;
    Physics::run();
}

void Physics::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theOccupancySerialization("PhysicsOccupancy");
        theOccupancySerialization.streamByChipContainer(fDQMStreamer, theOccContainer);

        ContainerSerialization theBCIDSerialization("PhysicsBCID");
        theBCIDSerialization.streamByChipContainer(fDQMStreamer, theBCIDContainer);

        ContainerSerialization theTrgIDSerialization("PhysicsTrgID");
        theTrgIDSerialization.streamByChipContainer(fDQMStreamer, theTrgIDContainer);
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
    LOG(INFO) << BOLDBLUE << "\t--> Total number of recorded bunch crossings: " << BOLDYELLOW << numberOfEventsPerRun << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Total number of received triggers (i.e. events): " << BOLDYELLOW << numberOfEventsPerRun / nTRIGxEvent << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Total number of corrupted bunch crossings: " << BOLDYELLOW << std::setprecision(3) << corruptedEventCounter << " ("
              << 1. * corruptedEventCounter / numberOfEventsPerRun * 100. << "%)" << std::setprecision(-1) << RESET;
}

void Physics::localConfigure(const std::string& histoFileName, int currentRun)
{
    corruptedEventCounter = 0;
    histos                = nullptr;
    theCurrentRun         = currentRun;

    LOG(INFO) << GREEN << "[Physics::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    Physics::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<PhysicsHistograms>(histoFileName, "Physics", histos, currentRun);
#ifdef __USE_ROOT__
    if(this->fResultFile != nullptr) this->fResultFile->Close();
    this->InitResultFile(CalibBase::theHistoFileName);
#endif
}

void Physics::run()
{
    std::unique_lock<std::recursive_mutex> theGuard(theMtx, std::defer_lock);

    while(Tool::fKeepRunning == true)
    {
        RD53Event::decodedEvents.clear();
        Physics::analyze();
        Physics::draw();

        if(strcmp(frontEnd->name, "SYNC") == 0) // @TMP@
            for(const auto cBoard: *fDetectorContainer)
            {
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])
                    ->WriteChipCommand(serialize(RD53ACmd::WrReg{RD53AConstants::BROADCAST_CHIPID, RD53AConstants::GLOBAL_PULSE_ADDR, 1 << 14}), -1);
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->WriteChipCommand(serialize(RD53ACmd::GlobalPulse{RD53AConstants::BROADCAST_CHIPID, 0x6}), -1);
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->WriteChipCommand(serialize(RD53ACmd::ECR{}), -1);
                std::this_thread::sleep_for(std::chrono::microseconds(20));
            }

        theGuard.lock();
        genericEvtConverter(RD53Event::decodedEvents);
        theGuard.unlock();
        numberOfEventsPerRun += RD53Event::decodedEvents.size();

        if((RD53Event::decodedEvents.size() != 0) && (numberOfEventsPerRun % PRINTeventsEVERY == 0))
            LOG(INFO) << BOLDBLUE << "\t--> Total number of recorded bunch crossings up to now: " << BOLDYELLOW << numberOfEventsPerRun << RESET;

        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    }
}

void Physics::draw(bool saveData)
{
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((saveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
        LOG(INFO) << BOLDBLUE << "\t--> Physics saving histograms..." << RESET;
    }

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

        if(dataSize != 0) Physics::fillDataContainer(*cBoard);
    }

    Physics::sendData();
}

void Physics::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fill(theOccContainer);
    histos->fillBCID(theBCIDContainer);
    histos->fillTrgID(theTrgIDContainer);
#endif
}

void Physics::fillDataContainer(BeBoard& theBoard)
{
    const auto cBoard = theOccContainer.getObject(theBoard.getId());

    // ###################
    // # Clear container #
    // ###################
    Physics::clearContainers(theBoard);

    // ###################
    // # Fill containers #
    // ###################
    const std::vector<Event*>& events     = SystemController::GetEvents();
    size_t                     evtCounter = numberOfEventsPerRun;
    for(const auto& event: events)
    {
        event->fillDataContainer(cBoard, getChannelGroup(-1));

        if(RD53Event::EvtErrorHandler(static_cast<RD53Event*>(event)->eventStatus) == false)
        {
            LOG(ERROR) << BOLDBLUE << "\t--> Corrupted event n. " << BOLDYELLOW << evtCounter << RESET;
            corruptedEventCounter++;
            RD53Event::PrintEvents({*static_cast<RD53Event*>(event)});
        }

        evtCounter++;
    }

    // ######################################
    // # Copy register values for streaming #
    // ######################################
    for(const auto cOpticalGroup: *cBoard)
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
            {
                for(auto i = 1u; i < cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.size(); i++)
                {
                    int deltaBCID = cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.at(i) - cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.at(i - 1);
                    deltaBCID += (deltaBCID >= 0 ? 0 : RD53Shared::firstChip->getMaxBCIDvalue() + 1);
                    if(deltaBCID >= int(RD53Shared::firstChip->getMaxBCIDvalue()))
                        LOG(DEBUG) << BOLDBLUE << "[Physics::fillDataContainer] " << BOLDRED << "deltaBCID out of range: " << BOLDYELLOW << deltaBCID << RESET;
                    else
                        theBCIDContainer.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<std::vector<uint16_t>>()
                            .at(deltaBCID)++;
                }

                for(auto i = 1u; i < cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.size(); i++)
                {
                    int deltaTrgID = cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.at(i) - cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.at(i - 1);
                    deltaTrgID += (deltaTrgID >= 0 ? 0 : RD53Shared::firstChip->getMaxTRIGIDvalue() + 1);
                    if(deltaTrgID >= int(RD53Shared::firstChip->getMaxTRIGIDvalue()))
                        LOG(DEBUG) << BOLDBLUE << "[Physics::fillDataContainer] " << BOLDRED << "deltaTrgID out of range: " << BOLDYELLOW << deltaTrgID << RESET;
                    else
                        theTrgIDContainer.getObject(cBoard->getId())
                            ->getObject(cOpticalGroup->getId())
                            ->getObject(cHybrid->getId())
                            ->getObject(cChip->getId())
                            ->getSummary<std::vector<uint16_t>>()
                            .at(deltaTrgID)++;
                }
            }

    // #######################
    // # Normalize container #
    // #######################
    for(const auto cOpticalGroup: *cBoard)
        for(const auto cHybrid: *cOpticalGroup)
            for(const auto cChip: *cHybrid)
                for(auto row = 0u; row < RD53Shared::firstChip->getNRows(); row++)
                    for(auto col = 0u; col < RD53Shared::firstChip->getNCols(); col++) cChip->getChannel<OccupancyAndPh>(row, col).normalize(events.size(), true);
}

void Physics::clearContainers(BeBoard& theBoard)
{
    RD53Event::clearEventContainer(theBoard, theOccContainer);
    CalibBase::fillVectorContainer<uint16_t>(theBCIDContainer, RD53Shared::firstChip->getMaxBCIDvalue() + 1, 0, theBoard.getId());
    CalibBase::fillVectorContainer<uint16_t>(theTrgIDContainer, RD53Shared::firstChip->getMaxTRIGIDvalue() + 1, 0, theBoard.getId());
}
