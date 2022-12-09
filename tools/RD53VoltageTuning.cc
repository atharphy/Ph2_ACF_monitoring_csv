/*!
  \file                  RD53VoltageTuning.h
  \brief                 Implementaion of Bit Error Rate test
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53VoltageTuning.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void VoltageTuning::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    colStart     = this->findValueInSettings<double>("COLstart");
    colStop      = this->findValueInSettings<double>("COLstop");
    targetDig    = this->findValueInSettings<double>("VDDDTrimTarget", 1.3);
    targetAna    = this->findValueInSettings<double>("VDDATrimTarget", 1.2);
    toleranceDig = this->findValueInSettings<double>("VDDDTrimTolerance", 0.02);
    toleranceAna = this->findValueInSettings<double>("VDDATrimTolerance", 0.02);
    doDisplay    = this->findValueInSettings<double>("DisplayHisto");

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false, "VoltageTuning");
}

void VoltageTuning::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[VoltageTuning::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    VoltageTuning::run();
    VoltageTuning::analyze();
    VoltageTuning::sendData();
}

void VoltageTuning::sendData()
{
    auto theDigStreamer = this->prepareChipContainerStreamer<EmptyContainer, double>("VoltageDig");
    auto theAnaStreamer = this->prepareChipContainerStreamer<EmptyContainer, double>("VoltageAna");

    if(fDQMStreamerEnabled == true)
    {
        for(const auto cBoard: theDigContainer) theDigStreamer->streamAndSendBoard(cBoard, fDQMStreamer);
        for(const auto cBoard: theAnaContainer) theAnaStreamer->streamAndSendBoard(cBoard, fDQMStreamer);
    }
}

void VoltageTuning::Stop()
{
    LOG(INFO) << GREEN << "[VoltageTuning::Stop] Stopping" << RESET;

    Tool::Stop();

    VoltageTuning::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void VoltageTuning::localConfigure(const std::string& fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[VoltageTuning::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    VoltageTuning::ConfigureCalibration();
    VoltageTuning::initializeFiles(fileRes_, currentRun);
}

void VoltageTuning::initializeFiles(const std::string& fileRes_, int currentRun)
{
    fileRes = fileRes_;

#ifdef __USE_ROOT__
    delete histos;
    histos = new VoltageTuningHistograms;
#endif
}

void VoltageTuning::run()
{
    const int         conversionFactor = 2; // @CONST@
    const int         NSIGMA           = 2; // @CONST@
    const size_t      nBitsDig         = RD53Shared::firstChip->getFEtype(colStart, colStop)->nBitTrimDig;
    const size_t      nBitsAna         = RD53Shared::firstChip->getFEtype(colStart, colStop)->nBitTrimAna;
    const std::string VDDDreg          = RD53Shared::firstChip->getFEtype(colStart, colStop)->VDDDreadReg;
    const std::string VDDAreg          = RD53Shared::firstChip->getFEtype(colStart, colStop)->VDDAreadReg;
    float             targetDig_       = targetDig;
    float             targetAna_       = targetAna;
    bool              doRepeatDig;
    bool              doRepeatAna;

    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theDigContainer);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theAnaContainer);

    auto RD53ChipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    auto chipSubset = [](const ChipContainer* theChip) { return theChip->isEnabled(); };
    fDetectorContainer->setReadoutChipQueryFunction(chipSubset);
    fDetectorContainer->setEnabledAll(true);

    for(auto nAttempt = 0; nAttempt < RD53Shared::MAXATTEMPTS; nAttempt++)
    {
        doRepeatDig = false;
        doRepeatAna = false;

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        unsigned int it;

                        // ##############
                        // # Start VDDD #
                        // ##############

                        LOG(INFO) << GREEN << "VDDD tuning for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                  << +cChip->getId() << RESET << GREEN << "] starts with target value = " << BOLDYELLOW << std::setprecision(3) << targetDig_ << RESET << GREEN
                                  << " and tolerance = " << BOLDYELLOW << toleranceDig << RESET;

                        std::vector<float> trimVoltageDig;
                        std::vector<int>   trimVoltageDigIndex;

                        auto defaultDig = ((RD53Shared::setBits(nBitsAna) / 2) << nBitsDig) | (RD53Shared::setBits(nBitsDig) / 2);
                        RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", defaultDig);
                        float initDig = RD53ChipInterface->ReadChipMonitor(cChip, VDDDreg) * conversionFactor;

                        std::vector<int> scanrangeDig = VoltageTuning::createScanRange(cChip, "VOLTAGE_TRIM_DIG", targetDig_, initDig);
                        bool             isUpward     = false;

                        if(initDig < targetDig_) isUpward = true;

                        for(it = 0; it < scanrangeDig.size(); it++)
                        {
                            auto vTrimDecimal = bits::pack<5, 5>(16, scanrangeDig[it]);

                            RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", vTrimDecimal);
                            float readingDig = RD53ChipInterface->ReadChipMonitor(cChip, VDDDreg) * conversionFactor;
                            float diff       = fabs(readingDig - targetDig_);

                            trimVoltageDig.push_back(diff);
                            trimVoltageDigIndex.push_back(scanrangeDig[it]);

                            if(isUpward && readingDig > (targetDig_ + toleranceDig)) break;
                            if(!isUpward && readingDig < (targetDig_ - toleranceDig)) break;
                        }

                        int minDigIndex    = std::min_element(trimVoltageDig.begin(), trimVoltageDig.end()) - trimVoltageDig.begin();
                        int vdddNewSetting = trimVoltageDigIndex[minDigIndex];

                        LOG(INFO) << GREEN << "VDDD best setting: " << BOLDYELLOW << vdddNewSetting << std::setprecision(3) << RESET << GREEN << ", difference with respect to target = " << BOLDYELLOW
                                  << trimVoltageDig[minDigIndex] << RESET << GREEN << " V" << RESET;

                        if(trimVoltageDig[minDigIndex] > toleranceDig)
                        {
                            doRepeatDig = true;
                            LOG(WARNING) << GREEN << "Optimal value not found: " << BOLDYELLOW << "RETRY" << RESET;
                            break;
                        }
                        else
                            cChip->setEnabled(false);

                        // ##############
                        // # Start VDDA #
                        // ##############

                        LOG(INFO) << GREEN << "VDDA tuning for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                  << +cChip->getId() << std::setprecision(3) << RESET << GREEN << "] starts with target value = " << BOLDYELLOW << targetAna_ << RESET << GREEN
                                  << " and tolerance = " << BOLDYELLOW << toleranceAna << RESET << GREEN << " and with VDDD = " << BOLDYELLOW << vdddNewSetting << RESET;

                        std::vector<float> trimVoltageAna;
                        std::vector<int>   trimVoltageAnaIndex;

                        auto defaultAna = ((RD53Shared::setBits(nBitsAna) / 2) << nBitsDig) | vdddNewSetting;
                        RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", defaultAna);
                        float initAna = RD53ChipInterface->ReadChipMonitor(cChip, VDDAreg) * conversionFactor;

                        std::vector<int> scanrangeAna = VoltageTuning::createScanRange(cChip, "VOLTAGE_TRIM_ANA", targetAna_, initAna);
                        isUpward                      = false;

                        if(initAna < targetAna_) isUpward = true;

                        for(it = 0; it < scanrangeAna.size(); it++)
                        {
                            auto vTrimDecimal = bits::pack<5, 5>(scanrangeAna[it], vdddNewSetting);

                            RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", vTrimDecimal);
                            float readingAna = RD53ChipInterface->ReadChipMonitor(cChip, VDDAreg) * conversionFactor;
                            float diff       = fabs(readingAna - targetAna_);

                            trimVoltageAna.push_back(diff);
                            trimVoltageAnaIndex.push_back(scanrangeAna[it]);

                            if(isUpward && readingAna > (targetAna_ + toleranceAna)) break;
                            if(!isUpward && readingAna < (targetAna_ - toleranceAna)) break;
                        }

                        int minAnaIndex    = std::min_element(trimVoltageAna.begin(), trimVoltageAna.end()) - trimVoltageAna.begin();
                        int vddaNewSetting = trimVoltageAnaIndex[minAnaIndex];

                        LOG(INFO) << GREEN << "VDDA best setting: " << BOLDYELLOW << vddaNewSetting << std::setprecision(3) << RESET << GREEN << ", difference with respect to target = " << BOLDYELLOW
                                  << trimVoltageAna[minAnaIndex] << RESET << GREEN << " V" << RESET;

                        if(trimVoltageAna[minAnaIndex] > toleranceAna)
                        {
                            doRepeatAna = true;
                            LOG(WARNING) << GREEN << "Optimal value not found: " << BOLDYELLOW << "RETRY" << RESET;
                            break;
                        }
                        else
                            cChip->setEnabled(false);

                        // ################
                        // # Final values #
                        // ################

                        auto finalDecimal = bits::pack<5, 5>(vddaNewSetting, vdddNewSetting);

                        RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", finalDecimal);

                        auto finalVDDD = RD53ChipInterface->ReadChipMonitor(cChip, VDDDreg) * conversionFactor;
                        auto finalVDDA = RD53ChipInterface->ReadChipMonitor(cChip, VDDAreg) * conversionFactor;

                        LOG(INFO) << CYAN << "Final voltage readings after tuning" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Final VDDD reading = " << std::setprecision(3) << BOLDYELLOW << finalVDDD << BOLDBLUE << " V" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Final VDDA reading = " << std::setprecision(3) << BOLDYELLOW << finalVDDA << BOLDBLUE << " V" << RESET;

                        theDigContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = vdddNewSetting;
                        theAnaContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = vddaNewSetting;
                    }

        if((doRepeatDig == false) && (doRepeatAna == false))
        {
            if((targetDig_ != targetDig) || (targetAna_ != targetAna))
                LOG(WARNING) << GREEN << "The original target values could not be achieved --> they were lowered by an automatic procedure" << RESET;
            break;
        }
        else
        {
            if(doRepeatDig == true) targetDig_ -= toleranceDig * NSIGMA;
            if(doRepeatAna == true) targetAna_ -= toleranceAna * NSIGMA;
        }

        // ##################
        // # Reset sequence #
        // ##################
        for(const auto cBoard: *fDetectorContainer)
        {
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ResetBoard();
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureBoard(cBoard);
            RD53ChipInterface->InitRD53Downlink(cBoard);
        }
    }

    if((doRepeatDig == true) || (doRepeatAna == true)) LOG(ERROR) << BOLDRED << "The calibration was not able to run successfully on all chips" << RESET;

    // ###################################
    // # Read analog current consumption #
    // ###################################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    // ###############################
                    // # Save original configuration #
                    // ###############################

                    auto memCoreCol0 = RD53ChipInterface->ReadChipReg(cChip, "EN_CORE_COL_0");
                    auto memCoreCol1 = RD53ChipInterface->ReadChipReg(cChip, "EN_CORE_COL_1");
                    auto memCoreCol2 = RD53ChipInterface->ReadChipReg(cChip, "EN_CORE_COL_3");
                    auto memCoreCol3 = RD53ChipInterface->ReadChipReg(cChip, "EN_CORE_COL_3");

                    // ########################
                    // # Disable all channels #
                    // ########################

                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_0", 0);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_1", 0);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_2", 0);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_3", 0);

                    RD53ChipInterface->MaskAllChannels(cChip, true);

                    auto allDisabled_current = RD53ChipInterface->ReadChipMonitor(cChip, "ANA_IN_CURRENT");

                    // #######################
                    // # Enable all channels #
                    // #######################

                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_0", 65535);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_1", 65535);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_2", 65535);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_3", 63);

                    RD53ChipInterface->MaskAllChannels(cChip, false);

                    auto allEnabled_current = RD53ChipInterface->ReadChipMonitor(cChip, "ANA_IN_CURRENT");

                    // ##################################
                    // # Restore original configuration #
                    // ##################################

                    static_cast<RD53*>(cChip)->copyMaskFromDefault();
                    RD53ChipInterface->ConfigureChipOriginalMask(cChip, false, true);

                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_0", memCoreCol0);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_1", memCoreCol1);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_2", memCoreCol2);
                    RD53ChipInterface->WriteChipReg(cChip, "EN_CORE_COL_3", memCoreCol3);

                    LOG(INFO) << GREEN << "Analog current consumption for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Entire chip: " << std::setprecision(3) << BOLDYELLOW << allEnabled_current - allDisabled_current << BOLDBLUE << " A" << RESET;
                    LOG(INFO) << BOLDBLUE << "\t--> Single pixel cell: " << std::setprecision(3) << BOLDYELLOW
                              << (allEnabled_current - allDisabled_current) / (RD53Shared::firstChip->getNRows() * RD53Shared::firstChip->getNCols()) << BOLDBLUE << " A" << RESET;
                }

    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
}

void VoltageTuning::draw(bool saveData)
{
#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> VoltageTuning saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    VoltageTuning::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void VoltageTuning::analyze()
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    LOG(INFO) << GREEN << "VDDD and VDDA for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET << GREEN << "] are: VDDD = " << BOLDYELLOW
                              << theDigContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() << RESET << GREEN
                              << ", VDDA = " << BOLDYELLOW
                              << theAnaContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() << RESET;
}

void VoltageTuning::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillDig(theDigContainer);
    histos->fillAna(theAnaContainer);
#endif
}

std::vector<int> VoltageTuning::createScanRange(Chip* pChip, const std::string regName, float target, float initial)
{
    std::vector<int> scanRange;

    if(initial <= target)
        for(int vTrim = (RD53Shared::setBits(pChip->getRegItem(regName).fBitSize) + 1) / 2; vTrim <= static_cast<int>(RD53Shared::setBits(pChip->getRegItem(regName).fBitSize)); vTrim++)
            scanRange.push_back(vTrim);
    else if(initial > target)
        for(int vTrim = (RD53Shared::setBits(pChip->getRegItem(regName).fBitSize) + 1) / 2; vTrim >= 0; vTrim--) scanRange.push_back(vTrim);

    return scanRange;
}
