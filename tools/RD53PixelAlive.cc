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
    rowStart       = this->findValueInSettings<double>("ROWstart");
    rowStop        = this->findValueInSettings<double>("ROWstop");
    colStart       = this->findValueInSettings<double>("COLstart");
    colStop        = this->findValueInSettings<double>("COLstop");
    nEvents        = this->findValueInSettings<double>("nEvents");
    nEvtsBurst     = this->findValueInSettings<double>("nEvtsBurst") < nEvents ? this->findValueInSettings<double>("nEvtsBurst") : nEvents;
    nTRIGxEvent    = this->findValueInSettings<double>("nTRIGxEvent");
    injType        = this->findValueInSettings<double>("INJtype");
    nHITxCol       = this->findValueInSettings<double>("nHITxCol");
    doOnlyNGroups  = this->findValueInSettings<double>("DoOnlyNGroups");
    occPerPixel    = this->findValueInSettings<double>("OccPerPixel");
    unstuckPixels  = this->findValueInSettings<double>("UnstuckPixels");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip   = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings<double>("SaveBinaryData");
    frontEnd       = RD53Shared::firstChip->getFEtype(colStart, colStop);

    // ################################
    // # Custom channel group handler #
    // ################################
    auto groupType = injType != INJtype::None ? RD53GroupType::Groups : RD53GroupType::AllPixels;
    theChnGroupHandler =
        std::make_shared<RD53ChannelGroupHandler>(rowStart, rowStop, colStart, colStop, RD53Shared::firstChip->getNRows(), RD53Shared::firstChip->getNCols(), groupType, nHITxCol, doOnlyNGroups);
    this->setChannelGroupHandler(theChnGroupHandler);

    // ######################
    // # Set injection type #
    // ######################
    for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "DIGITAL_INJ_EN", injType == INJtype::Digital);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += PixelAlive::getNumberIterations();
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
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    PixelAlive::sendData();
}

void PixelAlive::sendData()
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53BEvtEncoder::NBIT_TRIGID) + 1;

    auto theOccStream   = this->prepareChannelContainerStreamer<OccupancyAndPh>("Occ");
    auto theBCIDStream  = this->prepareChipContainerStreamer<EmptyContainer, GenericDataArray<BCIDsize>>("BCID");
    auto theTrgIDStream = this->prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TrgIDsize>>("TrgID");

    if(fDQMStreamerEnabled == true)
    {
        for(const auto cBoard: *theOccContainer.get()) theOccStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theBCIDContainer) theBCIDStream->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theTrgIDContainer) theTrgIDStream->streamAndSendBoard(cBoard, fDQMStreamer);
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

void PixelAlive::localConfigure(const std::string& fileRes_, int currentRun)
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
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false, "PixelAlive");
    PixelAlive::initializeFiles(fileRes_, currentRun);
}

void PixelAlive::initializeFiles(const std::string& fileRes_, int currentRun)
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
    theOccContainer              = std::make_shared<DetectorDataContainer>();
    this->fDetectorDataContainer = theOccContainer.get();
    ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, *this->fDetectorDataContainer);

    this->SetTestPulse(injType != INJtype::None);
    this->fMaskChannelsFromOtherGroups = true;
    this->measureData(nEvents, nEvtsBurst);

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void PixelAlive::draw(bool doSaveData)
{
    if(doSaveData == true) CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((doSaveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> PixelAlive saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    PixelAlive::fillHisto();
    histos->process();
    saveData = doSaveData;

    if(doDisplay == true) myApp->Run(true);
#endif
}

std::shared_ptr<DetectorDataContainer> PixelAlive::analyze()
{
    const size_t BCIDsize  = RD53Shared::setBits(RD53AEvtEncoder::NBIT_BCID) + 1;
    const size_t TrgIDsize = RD53Shared::setBits(RD53BEvtEncoder::NBIT_TRIGID) + 1;

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
                    if(injType == INJtype::None)
                        theOccContainer->at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataVector, OccupancyAndPh>()
                            .fOccupancy /= nTRIGxEvent;

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
                                if(injType == INJtype::None)
                                    theOccContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<OccupancyAndPh>(row, col)
                                        .fOccupancy /= nTRIGxEvent;

                                float occupancy = theOccContainer->at(cBoard->getIndex())
                                                      ->at(cOpticalGroup->getIndex())
                                                      ->at(cHybrid->getIndex())
                                                      ->at(cChip->getIndex())
                                                      ->getChannel<OccupancyAndPh>(row, col)
                                                      .fOccupancy;
                                bool enable = (injType == INJtype::None ? occupancy <= occPerPixel : occupancy >= occPerPixel);
                                if(unstuckPixels == false)
                                    static_cast<RD53*>(cChip)->enablePixel(row, col, enable);
                                else if(enable == false)
                                    static_cast<RD53*>(cChip)->setTDAC(row, col, 0);
                                if(enable == false)
                                {
                                    nMaskedPixelsPerCalib++;
                                    theOccContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<OccupancyAndPh>(row, col)
                                        .fOccupancy = RD53Shared::ISMASKED;
                                }
                            }
                            else
                                theOccContainer->at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getChannel<OccupancyAndPh>(row, col)
                                    .fOccupancy = RD53Shared::ISDISABLED;

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
                        deltaBCID += (deltaBCID >= 0 ? 0 : frontEnd->maxBCIDvalue + 1);
                        if(deltaBCID >= int(frontEnd->maxBCIDvalue))
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
                        deltaTrgID += (deltaTrgID >= 0 ? 0 : frontEnd->maxTRIGIDvalue + 1);
                        if(deltaTrgID > int(frontEnd->maxTRIGIDvalue))
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
