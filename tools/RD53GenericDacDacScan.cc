/*!
  \file                  RD53GenericDacDacScan.cc
  \brief                 Implementaion of a generic DAC-DAC scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/05/21
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53GenericDacDacScan.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void GenericDacDacScan::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::ConfigureCalibration();
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    regNameDAC1    = this->findValueInSettings<std::string>("RegNameDAC1");
    startValueDAC1 = this->findValueInSettings<double>("StartValueDAC1");
    stopValueDAC1  = this->findValueInSettings<double>("StopValueDAC1");
    stepDAC1       = this->findValueInSettings<double>("StepDAC1");
    regNameDAC2    = this->findValueInSettings<std::string>("RegNameDAC2");
    startValueDAC2 = this->findValueInSettings<double>("StartValueDAC2");
    stopValueDAC2  = this->findValueInSettings<double>("StopValueDAC2");
    stepDAC2       = this->findValueInSettings<double>("StepDAC2");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip   = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings<double>("SaveBinaryData");

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    size_t nSteps = (stopValueDAC1 - startValueDAC1) / stepDAC1 + 1;
    for(auto i = 0u; i < nSteps; i++) dac1List.push_back(startValueDAC1 + stepDAC1 * i);
    nSteps = (stopValueDAC2 - startValueDAC2) / stepDAC2 + 1;
    for(auto i = 0u; i < nSteps; i++) dac2List.push_back(startValueDAC2 + stepDAC2 * i);

    // ##################################
    // # Check if it's RD53 or FPGA reg #
    // ##################################
    isDAC1ChipReg = (regNameDAC1.find(".") == std::string::npos ? true : false);
    isDAC2ChipReg = (regNameDAC2.find(".") == std::string::npos ? true : false);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += GenericDacDacScan::getNumberIterations();
}

void GenericDacDacScan::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[GenericDacDacScan::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_GenericDacDacScan.raw", 'w');
        this->initializeWriteFileHandler();
    }

    GenericDacDacScan::run();
    GenericDacDacScan::analyze();
    GenericDacDacScan::saveChipRegisters(theCurrentRun);
    GenericDacDacScan::sendData();
}

void GenericDacDacScan::sendData()
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theStream                  = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<GenericDacDacScanSize>>("Occ");
    auto theGenericDacDacScanStream = prepareChipContainerStreamer<EmptyContainer, std::pair<uint16_t, uint16_t>>("DACDAC");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theOccContainer) theStream.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theGenericDacDacScanContainer) theGenericDacDacScanStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void GenericDacDacScan::Stop()
{
    LOG(INFO) << GREEN << "[GenericDacDacScan::Stop] Stopping" << RESET;

    Tool::Stop();

    GenericDacDacScan::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void GenericDacDacScan::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[GenericDacDacScan::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    GenericDacDacScan::ConfigureCalibration();
    GenericDacDacScan::initializeFiles(fileRes_, currentRun);
}

void GenericDacDacScan::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_GenericDacDacScan.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new GenericDacDacScanHistograms;
#endif
}

void GenericDacDacScan::run()
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<GenericDataArray<GenericDacDacScanSize>>(*fDetectorContainer, theOccContainer);
    GenericDacDacScan::scanDacDac(regNameDAC1, regNameDAC2, dac1List, dac2List, &theOccContainer);

    // ################
    // # Error report #
    // ################
    GenericDacDacScan::chipErrorReport();
}

void GenericDacDacScan::draw()
{
    GenericDacDacScan::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> GenericDacDacScan saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    GenericDacDacScan::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void GenericDacDacScan::analyze()
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<std::pair<uint16_t, uint16_t>>(*fDetectorContainer, theGenericDacDacScanContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto best    = 0.;
                    int  regVal1 = 0;
                    int  regVal2 = 0;

                    for(auto i = 0u; i < dac1List.size(); i++)
                        for(auto j = 0u; j < dac2List.size(); j++)
                        {
                            auto current = theOccContainer.at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getSummary<GenericDataArray<GenericDacDacScanSize>>()
                                               .data[i * dac2List.size() + j];
                            if(current > best)
                            {
                                regVal1 = dac1List[i];
                                regVal2 = dac2List[j];
                                best    = current;
                            }
                        }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best register values for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] are " << BOLDYELLOW << regVal1 << BOLDMAGENTA << " for " << BOLDYELLOW << regNameDAC1 << BOLDMAGENTA
                              << " and " << BOLDYELLOW << regVal2 << BOLDMAGENTA << " for " << BOLDYELLOW << regNameDAC2 << RESET;

                    // ######################################################
                    // # Fill latency container and download new DAC values #
                    // ######################################################
                    theGenericDacDacScanContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<std::pair<uint16_t, uint16_t>>() =
                        std::pair<uint16_t, uint16_t>(regVal1, regVal2);
                }
}

void GenericDacDacScan::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillGenericDacDacScan(theGenericDacDacScanContainer);
#endif
}

void GenericDacDacScan::scanDacDac(const std::string&           regNameDAC1,
                                   const std::string&           regNameDAC2,
                                   const std::vector<uint16_t>& dac1List,
                                   const std::vector<uint16_t>& dac2List,
                                   DetectorDataContainer*       theContainer)
{
    const size_t GenericDacDacScanSize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(auto i = 0u; i < dac1List.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regNameDAC1 << BOLDMAGENTA << " value = " << BOLDYELLOW << dac1List[i] << BOLDMAGENTA << " <<<" << RESET;
        if(isDAC1ChipReg == true)
            for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regNameDAC1, dac1List[i]);
        else
            for(const auto cBoard: *fDetectorContainer)
                static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])
                    ->WriteArbitraryRegister(regNameDAC1, dac1List[i], cBoard, this->fReadoutChipInterface, (regNameDAC2.find("cdr") != std::string::npos ? true : false));

        for(auto j = 0u; j < dac2List.size(); j++)
        {
            // ###########################
            // # Download new DAC values #
            // ###########################
            LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regNameDAC2 << BOLDMAGENTA << " value = " << BOLDYELLOW << dac2List[j] << BOLDMAGENTA << " <<<" << RESET;
            if(isDAC2ChipReg == true)
                for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regNameDAC2, dac2List[j]);
            else
                for(const auto cBoard: *fDetectorContainer)
                    static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])
                        ->WriteArbitraryRegister(regNameDAC2, dac2List[j], cBoard, this->fReadoutChipInterface, (regNameDAC2.find("cdr") != std::string::npos ? true : false));

            // ################
            // # Run analysis #
            // ################
            PixelAlive::run();
            auto output = PixelAlive::analyze();
            output->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

            // ###############
            // # Save output #
            // ###############
            for(const auto cBoard: *output)
                for(const auto cOpticalGroup: *cBoard)
                    for(const auto cHybrid: *cOpticalGroup)
                        for(const auto cChip: *cHybrid)
                        {
                            float occ = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;
                            theContainer->at(cBoard->getIndex())
                                ->at(cOpticalGroup->getIndex())
                                ->at(cHybrid->getIndex())
                                ->at(cChip->getIndex())
                                ->getSummary<GenericDataArray<GenericDacDacScanSize>>()
                                .data[i * dac2List.size() + j] = occ;
                        }

            // ##############################################
            // # Send periodic data to minitor the progress #
            // ##############################################
            GenericDacDacScan::sendData();
        }
    }
}

void GenericDacDacScan::chipErrorReport() const
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

void GenericDacDacScan::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> GenericDacDacScan saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
