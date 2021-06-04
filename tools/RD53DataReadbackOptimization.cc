/*!
  \file                  RD53DataReadbackOptimization.cc
  \brief                 Implementaion of data readback optimization scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53DataReadbackOptimization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void DataReadbackOptimization::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    BERtest::ConfigureCalibration();

    // #######################
    // # Retrieve parameters #
    // #######################
    startValueTAP0 = this->findValueInSettings("TAP0Start");
    stopValueTAP0  = this->findValueInSettings("TAP0Stop");
    startValueTAP1 = this->findValueInSettings("TAP1Start");
    stopValueTAP1  = this->findValueInSettings("TAP1Stop");
    invTAP1        = this->findValueInSettings("InvTAP1");
    startValueTAP2 = this->findValueInSettings("TAP2Start");
    stopValueTAP2  = this->findValueInSettings("TAP2Stop");
    invTAP2        = this->findValueInSettings("InvTAP2");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    size_t nSteps = (stopValueTAP0 - startValueTAP0 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP0 - startValueTAP0 + 1);
    size_t step   = floor((stopValueTAP0 - startValueTAP0 + 1) / nSteps);
    for(auto i = 0u; i < nSteps; i++) dacListTAP0.push_back(startValueTAP0 + step * i);

    nSteps = (stopValueTAP1 - startValueTAP1 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP1 - startValueTAP1 + 1);
    step   = floor((stopValueTAP1 - startValueTAP1 + 1) / nSteps);
    for(auto i = 0u; i < nSteps; i++) dacListTAP1.push_back(startValueTAP1 + step * i);

    nSteps = (stopValueTAP2 - startValueTAP2 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP2 - startValueTAP2 + 1);
    step   = floor((stopValueTAP2 - startValueTAP2 + 1) / nSteps);
    for(auto i = 0u; i < nSteps; i++) dacListTAP2.push_back(startValueTAP2 + step * i);

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void DataReadbackOptimization::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[DataReadbackOptimization::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    DataReadbackOptimization::run();
    DataReadbackOptimization::saveChipRegisters(theCurrentRun);
    DataReadbackOptimization::sendData();
}

void DataReadbackOptimization::sendData()
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    auto theStreamTAP0scan = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TAPsize>>("TAP0scan");
    auto theStreamTAP0     = prepareChipContainerStreamer<EmptyContainer, uint16_t>("TAP0");

    auto theStreamTAP1scan = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TAPsize>>("TAP1scan");
    auto theStreamTAP1     = prepareChipContainerStreamer<EmptyContainer, uint16_t>("TAP1");

    auto theStreamTAP2scan = prepareChipContainerStreamer<EmptyContainer, GenericDataArray<TAPsize>>("TAP2scan");
    auto theStreamTAP2     = prepareChipContainerStreamer<EmptyContainer, uint16_t>("TAP2");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theTAP0scanContainer) theStreamTAP0scan.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTAP0Container) theStreamTAP0.streamAndSendBoard(cBoard, fNetworkStreamer);

        for(const auto cBoard: theTAP1scanContainer) theStreamTAP1scan.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTAP1Container) theStreamTAP1.streamAndSendBoard(cBoard, fNetworkStreamer);

        for(const auto cBoard: theTAP2scanContainer) theStreamTAP2scan.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTAP2Container) theStreamTAP2.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void DataReadbackOptimization::Stop()
{
    LOG(INFO) << GREEN << "[DataReadbackOptimization::Stop] Stopping" << RESET;

    Tool::Stop();

    DataReadbackOptimization::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void DataReadbackOptimization::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[DataReadbackOptimization::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    DataReadbackOptimization::ConfigureCalibration();
    DataReadbackOptimization::initializeFiles(fileRes_, currentRun);
}

void DataReadbackOptimization::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

#ifdef __USE_ROOT__
    delete histos;
    histos = new DataReadbackOptimizationHistograms;
#endif
}

void DataReadbackOptimization::run()
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<GenericDataArray<TAPsize>>(*fDetectorContainer, theTAP0scanContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<TAPsize>>(*fDetectorContainer, theTAP1scanContainer);
    ContainerFactory::copyAndInitChip<GenericDataArray<TAPsize>>(*fDetectorContainer, theTAP2scanContainer);

    for(const auto cBoard: *fDetectorContainer) static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_EN_TAP", 0x0);
    DataReadbackOptimization::scanDac("CML_TAP0_BIAS", dacListTAP0, &theTAP0scanContainer);
    DataReadbackOptimization::analyze("CML_TAP0_BIAS", dacListTAP0, theTAP0scanContainer, theTAP0Container);

    for(const auto cBoard: *fDetectorContainer) static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_EN_TAP", 0x1);
    for(const auto cBoard: *fDetectorContainer) static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_INV_TAP", invTAP1);
    DataReadbackOptimization::scanDac("CML_TAP1_BIAS", dacListTAP1, &theTAP1scanContainer);
    DataReadbackOptimization::analyze("CML_TAP1_BIAS", dacListTAP1, theTAP1scanContainer, theTAP1Container);

    for(const auto cBoard: *fDetectorContainer) static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_EN_TAP", 0x3);
    for(const auto cBoard: *fDetectorContainer)
        static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_INV_TAP", bits::pack<1, 1>(invTAP2, invTAP1));
    DataReadbackOptimization::scanDac("CML_TAP2_BIAS", dacListTAP2, &theTAP2scanContainer);
    DataReadbackOptimization::analyze("CML_TAP2_BIAS", dacListTAP2, theTAP2scanContainer, theTAP2Container);

    // ################
    // # Error report #
    // ################
    DataReadbackOptimization::chipErrorReport();
}

void DataReadbackOptimization::draw(bool saveData)
{
    if(saveData == true) DataReadbackOptimization::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> DataReadbackOptimization saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    DataReadbackOptimization::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void DataReadbackOptimization::analyze(const std::string& regName, const std::vector<uint16_t>& dacListTAP, const DetectorDataContainer& theTAPscanContainer, DetectorDataContainer& theTAPContainer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theTAPContainer);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    auto best = *std::max_element(
                        theTAPscanContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TAPsize>>().data,
                        theTAPscanContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TAPsize>>().data +
                            dacListTAP.size());
                    int regVal = 0;

                    for(auto i = 1u; i < dacListTAP.size(); i++)
                    {
                        auto current =
                            theTAPscanContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<GenericDataArray<TAPsize>>().data[i];
                        if((current >= 0) && (current < best))
                        {
                            regVal = dacListTAP[i];
                            best   = current;
                        }
                    }

                    LOG(INFO) << BOLDMAGENTA << ">>> Best " << BOLDYELLOW << regName << BOLDMAGENTA << " for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << regVal << BOLDMAGENTA << " <<<" << RESET;

                    // ##################################################
                    // # Fill TAP container and download new DAC values #
                    // ##################################################
                    theTAPContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = regVal;
                    this->fReadoutChipInterface->WriteChipReg(static_cast<RD53*>(cChip), regName, regVal);
                }
}

void DataReadbackOptimization::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillScanTAP0(theTAP0scanContainer);
    histos->fillTAP0(theTAP0Container);

    histos->fillScanTAP1(theTAP1scanContainer);
    histos->fillTAP1(theTAP1Container);

    histos->fillScanTAP2(theTAP2scanContainer);
    histos->fillTAP2(theTAP2Container);
#endif
}

void DataReadbackOptimization::scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(auto i = 0u; i < dacList.size(); i++)
    {
        // ###########################
        // # Download new DAC values #
        // ###########################
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << regName << BOLDMAGENTA << " value = " << BOLDYELLOW << dacList[i] << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, regName, dacList[i]);

        // ################
        // # Run analysis #
        // ################
        BERtest::run();

        // ######################
        // # Save BER test data #
        // ######################
        for(const auto cBoard: *theContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                        cChip->getSummary<GenericDataArray<TAPsize>>().data[i] =
                            BERtest::theBERtestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<double>();

        // ##############################################
        // # Send periodic data to minitor the progress #
        // ##############################################
        DataReadbackOptimization::sendData();
    }
}

void DataReadbackOptimization::chipErrorReport() const
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

void DataReadbackOptimization::saveChipRegisters(int currentRun)
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
                    LOG(INFO) << BOLDBLUE << "\t--> DataReadbackOptimizationx saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/"
                              << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
