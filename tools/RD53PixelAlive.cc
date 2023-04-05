/*!
  \file                  RD53PixelAlive.cc
  \brief                 Implementaion of PixelAlive scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53PixelAlive.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void PixelAlive::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart        = this->findValueInSettings<double>("ROWstart");
    rowStop         = this->findValueInSettings<double>("ROWstop");
    colStart        = this->findValueInSettings<double>("COLstart");
    colStop         = this->findValueInSettings<double>("COLstop");
    nEvents         = this->findValueInSettings<double>("nEvents");
    nEvtsBurst      = this->findValueInSettings<double>("nEvtsBurst") < nEvents ? this->findValueInSettings<double>("nEvtsBurst") : nEvents;
    nTRIGxEvent     = this->findValueInSettings<double>("nTRIGxEvent");
    injType         = static_cast<RD53Shared::INJtype>(this->findValueInSettings<double>("INJtype"));
    nHITxCol        = this->findValueInSettings<double>("nHITxCol");
    doDataIntegrity = this->findValueInSettings<double>("DoDataIntegrity");
    doOnlyNGroups   = this->findValueInSettings<double>("DoOnlyNGroups");
    occPerPixel     = this->findValueInSettings<double>("OccPerPixel");
    unstuckPixels   = this->findValueInSettings<double>("UnstuckPixels");
    doDisplay       = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip    = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData  = this->findValueInSettings<double>("SaveBinaryData");
    dataOutputDir   = this->findValueInSettings<std::string>("DataOutputDir", "");
    frontEnd        = RD53Shared::firstChip->getFEtype(colStart, colStop);

    // ################################
    // # Custom channel group handler #
    // ################################
    auto groupType = CalibBase::assignGroupType(injType);
    theChnGroupHandler =
        std::make_shared<RD53ChannelGroupHandler>(rowStart, rowStop, colStart, colStop, RD53Shared::firstChip->getNRows(), RD53Shared::firstChip->getNCols(), groupType, nHITxCol, doOnlyNGroups);
    this->setChannelGroupHandler(theChnGroupHandler);

    // ######################
    // # Set injection type #
    // ######################
    for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "DIGITAL_INJ_EN", injType == RD53Shared::INJtype::Digital);

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
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
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
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theOccupancySerialization("PixelAliveOccupancy");
        theOccupancySerialization.streamByChipContainer(fDQMStreamer, *theOccContainer.get());

        ContainerSerialization theBCIDSerialization("PixelAliveBCID");
        theBCIDSerialization.streamByChipContainer(fDQMStreamer, theBCIDContainer);

        ContainerSerialization theTrgIDSerialization("PixelAliveTrgID");
        theTrgIDSerialization.streamByChipContainer(fDQMStreamer, theTrgIDContainer);
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

void PixelAlive::localConfigure(const std::string& histoFileName, int currentRun)
{
    histos        = nullptr;
    theCurrentRun = currentRun;

    LOG(INFO) << GREEN << "[PixelAlive::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    PixelAlive::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<PixelAliveHistograms>(histoFileName, "PixelAlive", histos, currentRun, saveBinaryData);
}

void PixelAlive::run()
{
    if((doDataIntegrity == true) && (strcmp(frontEnd->name, "RD53B") == 0))
    {
        RD53RunProgress::turnOFF();

        const std::string regName = "EN_CORE_COL";

        std::shared_ptr<DetectorDataContainer> localOccContainer = std::make_shared<DetectorDataContainer>();
        this->fDetectorDataContainer                             = localOccContainer.get();
        ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, *this->fDetectorDataContainer);

        std::shared_ptr<RD53ChannelGroupHandler> localChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(
            rowStart, rowStop, colStart, colStop, RD53Shared::firstChip->getNRows(), RD53Shared::firstChip->getNCols(), RD53GroupType::AllPixels, nHITxCol, doOnlyNGroups);
        this->setChannelGroupHandler(localChnGroupHandler);

        LOG(INFO) << GREEN << "[PixelAlive::run] Running detection of Core-Column data corruption" << RESET;

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
            {
                // ############################
                // # Disable all core columns #
                // ############################
                for(auto suffix: {"_0", "_1", "_2", "_3"}) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regName + suffix, 0);

                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        std::map<std::string, uint16_t> regValueMap;

                        for(auto suffix: {"_0", "_1", "_2", "_3"})
                        {
                            const auto numberOfBits = RD53Shared::firstChip->getRegMap()[regName + suffix].fBitSize;
                            regValueMap[suffix]     = RD53Shared::setBits(numberOfBits);

                            for(auto i = 0u; i < numberOfBits; i++)
                            {
                                // ###########################
                                // # Download new DAC values #
                                // ###########################
                                this->fReadoutChipInterface->WriteChipReg(cChip, regName + suffix, 1 << i);

                                // ################
                                // # Run analysis #
                                // ################
                                this->SetTestPulse(false);
                                this->measureData(1, 1);

                                // #####################
                                // # Compute next step #
                                // #####################
                                bool statusGood = true;
                                for(const auto& ev: RD53Event::decodedEvents)
                                    if(ev.eventStatus != RD53FWEvtEncoder::GOOD)
                                    {
                                        statusGood = false;
                                        break;
                                    }
                                if((statusGood == false) || (RD53Event::decodedEvents.size() == 0)) regValueMap[suffix] ^= 1 << i;
                            }
                        }

                        // ###########################
                        // # Download new DAC values #
                        // ###########################
                        LOG(INFO) << GREEN << "Results for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                  << +cChip->getId() << RESET << GREEN << "]" << RESET;
                        for(auto suffix: {"_0", "_1", "_2", "_3"})
                        {
                            this->fReadoutChipInterface->WriteChipReg(cChip, regName + suffix, regValueMap[suffix]);
                            const auto numberOfBits = RD53Shared::firstChip->getRegMap()[regName + suffix].fBitSize;
                            uint16_t   mask         = RD53Shared::setBits(numberOfBits);
                            auto       value        = (std::bitset<16>(regValueMap[suffix]) & std::bitset<16>(mask)).to_string().erase(0, 16 - numberOfBits);
                            bool       problems     = (regValueMap[suffix] != mask);
                            LOG(INFO) << (problems ? BOLDRED : BOLDBLUE) << "\t--> " << BOLDYELLOW << regName + suffix << (problems ? BOLDRED : BOLDBLUE) << " value = " << BOLDYELLOW << value
                                      << (problems ? BOLDRED : BOLDBLUE) << " (0 = disabled)" << RESET;
                        }

                        LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
                    }
            }

        // ############################
        // # Reset to original values #
        // ############################
        this->setChannelGroupHandler(theChnGroupHandler);
        RD53RunProgress::turnON();
    }
    else if((doDataIntegrity == true) && (strcmp(frontEnd->name, "RD53B") != 0))
    {
        throw std::runtime_error("Option -DoDataIntegrity- not available for RD53A");
    }

    PixelAlive::runPixelAlive();
}

void PixelAlive::runPixelAlive()
{
    theOccContainer              = std::make_shared<DetectorDataContainer>();
    this->fDetectorDataContainer = theOccContainer.get();
    ContainerFactory::copyAndInitStructure<OccupancyAndPh, GenericDataVector>(*fDetectorContainer, *this->fDetectorDataContainer);

    auto groupType = CalibBase::assignGroupType(injType);
    this->SetTestPulse(groupType != RD53GroupType::AllPixels);
    this->fMaskChannelsFromOtherGroups = true;
    this->measureData(nEvents, nEvtsBurst);

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void PixelAlive::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((saveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        LOG(INFO) << BOLDBLUE << "\t--> PixelAlive saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    PixelAlive::fillHisto();
    histos->process();
    doSaveData = saveData;

    if(doDisplay == true) myApp->Run(true);
#endif
}

std::shared_ptr<DetectorDataContainer> PixelAlive::analyze()
{
    // ####################
    // # Clear containers #
    // ####################
    theBCIDContainer.reset();
    ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, theBCIDContainer);
    CalibBase::fillVectorContainer<uint16_t>(theBCIDContainer, frontEnd->maxBCIDvalue + 1, 0);

    theTrgIDContainer.reset();
    ContainerFactory::copyAndInitChip<std::vector<uint16_t>>(*fDetectorContainer, theTrgIDContainer);
    CalibBase::fillVectorContainer<uint16_t>(theTrgIDContainer, frontEnd->maxTRIGIDvalue + 1, 0);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    size_t nMaskedPixelsPerCalib = 0;
                    if(injType == RD53Shared::INJtype::None)
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
                                if(injType == RD53Shared::INJtype::None)
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
                                bool enable = (injType == RD53Shared::INJtype::None ? occupancy <= occPerPixel : occupancy >= occPerPixel);
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
                                        .fStatus = RD53Shared::ISMASKED;
                                }
                            }
                            else
                                theOccContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<OccupancyAndPh>(row, col).fStatus =
                                    RD53Shared::ISDISABLED;

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
                            theBCIDContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<std::vector<uint16_t>>().at(deltaBCID)++;
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
                                ->getSummary<std::vector<uint16_t>>()
                                .at(deltaTrgID)++;
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
