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
    doDisplay    = this->findValueInSettings<double>("DisplayHisto");
    targetDig    = this->findValueInSettings<double>("VDDDTrimTarget", 1.3);
    targetAna    = this->findValueInSettings<double>("VDDATrimTarget", 1.2);
    toleranceDig = this->findValueInSettings<double>("VDDDTrimTolerance", 0.02);
    toleranceAna = this->findValueInSettings<double>("VDDATrimTolerance", 0.02);

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
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
    auto theDigStreamer = prepareChipContainerStreamer<EmptyContainer, double>("VoltageDig");
    auto theAnaStreamer = prepareChipContainerStreamer<EmptyContainer, double>("VoltageAna");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theDigContainer) theDigStreamer.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theAnaContainer) theAnaStreamer.streamAndSendBoard(cBoard, fNetworkStreamer);
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

void VoltageTuning::localConfigure(const std::string fileRes_, int currentRun)
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

void VoltageTuning::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

#ifdef __USE_ROOT__
    delete histos;
    histos = new VoltageTuningHistograms;
#endif
}

void VoltageTuning::run()
{
    const int conversionFactor = 2; // @CONST@
    const int NSIGMA           = 2; // @CONST@
    bool      doRepeatDig;
    bool      doRepeatAna;
    float     targetDig_ = targetDig;
    float     targetAna_ = targetAna;

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
                                  << +cChip->getId() << std::setprecision(3) << RESET << GREEN << "] starts with target value = " << BOLDYELLOW << targetDig_ << RESET << GREEN
                                  << " and tolerance = " << BOLDYELLOW << toleranceDig << RESET;

                        std::vector<float> trimVoltageDig;
                        std::vector<int>   trimVoltageDigIndex;

                        auto defaultDig = bits::pack<5, 5>(16, 16);
                        RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", defaultDig);
                        float initDig = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_dig_ShuLDO") * conversionFactor;

                        std::vector<int> scanrangeDig = VoltageTuning::createScanRange(cChip, "VOLTAGE_TRIM_DIG", targetDig_, initDig);
                        bool             isUpward     = false;

                        if(initDig < targetDig_) isUpward = true;

                        for(it = 0; it < scanrangeDig.size(); it++)
                        {
                            auto vTrimDecimal = bits::pack<5, 5>(16, scanrangeDig[it]);

                            RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", vTrimDecimal);
                            float readingDig = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_dig_ShuLDO") * conversionFactor;
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

                        auto defaultAna = bits::pack<5, 5>(16, vdddNewSetting);
                        RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", defaultAna);
                        float initAna = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_ana_ShuLDO") * conversionFactor;

                        std::vector<int> scanrangeAna = VoltageTuning::createScanRange(cChip, "VOLTAGE_TRIM_ANA", targetAna_, initAna);
                        isUpward                      = false;

                        if(initAna < targetAna_) isUpward = true;

                        for(it = 0; it < scanrangeAna.size(); it++)
                        {
                            auto vTrimDecimal = bits::pack<5, 5>(scanrangeAna[it], vdddNewSetting);

                            RD53ChipInterface->WriteChipReg(cChip, "VOLTAGE_TRIM", vTrimDecimal);
                            float readingAna = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_ana_ShuLDO") * conversionFactor;
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

                        auto finalVDDD = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_dig_ShuLDO") * conversionFactor;
                        auto finalVDDA = RD53ChipInterface->ReadChipMonitor(cChip, "VOUT_ana_ShuLDO") * conversionFactor;

                        LOG(INFO) << CYAN << "Final voltage readings after tuning" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Final VDDD reading = " << std::setprecision(3) << BOLDYELLOW << finalVDDD << BOLDYELLOW << " V" << RESET;
                        LOG(INFO) << BOLDBLUE << "\t--> Final VDDA reading = " << std::setprecision(3) << BOLDYELLOW << finalVDDA << BOLDYELLOW << " V" << RESET;

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

    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
}

void VoltageTuning::draw()
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
