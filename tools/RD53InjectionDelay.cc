/*!
  \file                  RD53InjectionDelay.cc
  \brief                 Implementaion of Injection Delay scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53InjectionDelay.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void InjectionDelay::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    PixelAlive::ConfigureCalibration();
    PixelAlive::doDisplay    = false;
    PixelAlive::doUpdateChip = false;
    PixelAlive::doSaveData   = false;
    RD53RunProgress::total() -= PixelAlive::getNumberIterations();

    // #######################
    // # Retrieve parameters #
    // #######################
    startValue = 0u;
    stopValue  = frontEnd->nLatencyBins2Span * (RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) - 1;

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const size_t nSteps = stopValue - startValue + 1;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + i);

    // ######################
    // # Initialize Latency #
    // ######################
    la.Inherit(this);
    la.ConfigureCalibration();

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += InjectionDelay::getNumberIterations();
}

void InjectionDelay::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[InjectionDelay::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(PixelAlive::saveBinaryData == true)
    {
        this->fDirectoryName = dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR;
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_InjectionDelay.raw", 'w');
        this->initializeWriteFileHandler();
    }

    InjectionDelay::run();
    InjectionDelay::analyze();
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    InjectionDelay::sendData();
    la.sendData();
}

void InjectionDelay::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theOccupancySerialization("InjectionDelayOccupancy");
        theOccupancySerialization.streamByChipContainer(fDQMStreamer, theOccContainer);

        ContainerSerialization theInjectionDelaySerialization("InjectionDelayInjectionDelay");
        theInjectionDelaySerialization.streamByChipContainer(fDQMStreamer, theInjectionDelayContainer);
    }
}

void InjectionDelay::Stop()
{
    LOG(INFO) << GREEN << "[InjectionDelay::Stop] Stopping" << RESET;

    Tool::Stop();

    InjectionDelay::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void InjectionDelay::localConfigure(const std::string& histoFileName, int currentRun)
{
    histos                = nullptr;
    la.histos             = nullptr;
    la.PixelAlive::histos = nullptr;
    PixelAlive::histos    = nullptr;
    theCurrentRun         = currentRun;

    LOG(INFO) << GREEN << "[InjectionDelay::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // ##########################
    // # Initialize calibration #
    // ##########################
    InjectionDelay::ConfigureCalibration();

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles<InjectionDelayHistograms>(histoFileName, "InjectionDelay", histos, currentRun, PixelAlive::saveBinaryData);
    // ######################
    // # Initialize Latency #
    // ######################
    std::string fileName = histoFileName;
    fileName.replace(fileName.find("_InjectionDelay"), 15, "_Latency");
    la.initializeFiles<LatencyHistograms>(fileName, "Latency", la.histos);
}

void InjectionDelay::run()
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    // ###############
    // # Run Latency #
    // ###############
    for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "CAL_EDGE_FINE_DELAY", 0);

    la.run();
    la.analyze();

    ContainerFactory::copyAndInitChip<GenericDataArray<InjDelaySize>>(*fDetectorContainer, theOccContainer);

    // #######################
    // # Set initial latency #
    // #######################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg);
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency - 1);

                    for(auto i = 0u; i < InjDelaySize; i++)
                        theOccContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<InjDelaySize>>().data[i] = 0;
                }

    // ###############################
    // # Scan two adjacent latencies #
    // ###############################
    for(auto i = 0u; i < frontEnd->nLatencyBins2Span; i++)
    {
        std::vector<uint16_t> halfDacList(dacList.begin() + i * (dacList.end() - dacList.begin()) / frontEnd->nLatencyBins2Span,
                                          dacList.begin() + (i + 1) * (dacList.end() - dacList.begin()) / frontEnd->nLatencyBins2Span);

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg);
                        this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency + i);
                    }

        InjectionDelay::scanDac("CAL_EDGE_FINE_DELAY", halfDacList, &theOccContainer);
    }

    // ################
    // # Error report #
    // ################
    CalibBase::chipErrorReport();
}

void InjectionDelay::draw(bool saveData)
{
    CalibBase::saveChipRegisters(theCurrentRun, doUpdateChip);
    la.draw(false);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(PixelAlive::doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(CalibBase::theHistoFileName);
        LOG(INFO) << BOLDBLUE << "\t--> InjectionDelay saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    InjectionDelay::fillHisto();
    histos->process();

    if(PixelAlive::doDisplay == true) myApp->Run(true);
#endif
}

void InjectionDelay::analyze()
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;
    const size_t maxRegValue  = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1;
    const auto unitTime = 1. / RD53Constants::ACCELERATOR_CLK * 1000 / ((RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1) / (2. / frontEnd->nLatencyBins2Span));

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theInjectionDelayContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    float  best   = 0u;
                    size_t regVal = 0u;

                    for(auto i = 0u; i < dacList.size(); i++)
                    {
                        auto current = round(theOccContainer.at(cBoard->getIndex())
                                                 ->at(cOpticalGroup->getIndex())
                                                 ->at(cHybrid->getIndex())
                                                 ->at(cChip->getIndex())
                                                 ->getSummary<GenericDataArray<InjDelaySize>>()
                                                 .data[i] /
                                             RD53Shared::PRECISION) *
                                       RD53Shared::PRECISION;
                        if(current > best)
                        {
                            regVal = dacList[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best injection delay for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " (" << unitTime << " ns) computed over two bx <<<"
                              << RESET;
                    LOG(INFO) << BOLDMAGENTA << ">>> New injection delay dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal % maxRegValue << BOLDMAGENTA << " <<<" << RESET;

                    // ####################################################
                    // # Fill delay container and download new DAC values #
                    // ####################################################
                    theInjectionDelayContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), "CAL_EDGE_FINE_DELAY", regVal % maxRegValue);

                    auto latency = this->fReadoutChipInterface->ReadChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg) - frontEnd->nLatencyBins2Span + regVal / maxRegValue + 1;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), frontEnd->latencyReg, latency);

                    LOG(INFO) << BOLDMAGENTA << ">>> New latency dac value for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << latency << BOLDMAGENTA << " <<<" << RESET;
                }
}

void InjectionDelay::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillOccupancy(theOccContainer);
    histos->fillInjectionDelay(theInjectionDelayContainer);
#endif
}

void InjectionDelay::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer)
{
    const size_t InjDelaySize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;
    const size_t maxRegValue  = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits("CAL_EDGE_FINE_DELAY")) + 1;

    for(auto i = 0u; i < dacList.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regName, dacList[i] % maxRegValue);

        // ################
        // # Run analysis #
        // ################
        PixelAlive::run();
        auto output = PixelAlive::analyze();

        // ###############
        // # Save output #
        // ###############
        for(const auto cBoard: *output)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                        theContainer->at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<InjDelaySize>>()
                            .data[dacList[i]] = cChip->getSummary<GenericDataVector, OccupancyAndPh>().fOccupancy;

        // ##############################################
        // # Send periodic data to monitor the progress #
        // ##############################################
        InjectionDelay::sendData();
    }

    // #################################
    // # Reset masks to default values #
    // #################################
    CalibBase::copyMaskFromDefault("en in");
}
