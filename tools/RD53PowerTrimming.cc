/*!
  \file                  RD53PowerTrimming.cc
  \brief                 Implementaion of power trimming
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53PowerTrimming.h"
#include <chrono>
#include <fstream>
#include <iomanip>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

using dataType = std::vector<std::pair<uint16_t, float>>;

void PowerTrimming::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    CalibBase::ConfigureCalibration();
    COMP_CURRENT_mA   = findValueInSettings<double>("PowerTrimmingCOMPTarget", 319.0);
    PREAMP_CURRENT_mA = findValueInSettings<double>("PowerTrimmingPREAMPTarget", 435.0);
    LDAC_CURRENT_mA   = findValueInSettings<double>("PowerTrimmingLDACTarget", 58.0);
}

void PowerTrimming::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[PowerTrimming::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    PowerTrimming::run();
    PowerTrimming::draw();
    PowerTrimming::sendData();
}

void PowerTrimming::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization thePreamplifierCurrentSerialization("PowerTrimmingPreamplifierCurrent");
        thePreamplifierCurrentSerialization.streamByChipContainer(fDQMStreamer, thePreamplifierCurrentContainer);

        ContainerSerialization theComparatorCurrentSerialization("PowerTrimmingComparatorCurrent");
        theComparatorCurrentSerialization.streamByChipContainer(fDQMStreamer, theComparatorCurrentContainer);

        ContainerSerialization theLDACCurrentSerialization("PowerTrimmingLDACCurrent");
        theLDACCurrentSerialization.streamByChipContainer(fDQMStreamer, theLDACCurrentContainer);
    }
}

void PowerTrimming::Stop()
{
    LOG(INFO) << GREEN << "[PowerTrimming::Stop] Stopping" << RESET;
    CalibBase::Stop();
}

void PowerTrimming::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos = nullptr;

    LOG(INFO) << GREEN << "[PowerTrimming::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    PowerTrimming::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "PowerTrimming", histos);
}

void PowerTrimming::run()
{
    LOG(INFO) << RESET;
    dataType init;

    ContainerFactory::copyAndInitChip<dataType>(*fDetectorContainer, theComparatorCurrentContainer, init);
    ContainerFactory::copyAndInitChip<dataType>(*fDetectorContainer, thePreamplifierCurrentContainer, init);
    ContainerFactory::copyAndInitChip<dataType>(*fDetectorContainer, theLDACCurrentContainer, init);

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    // ###################################
                    // # Chip initialization and masking #
                    // ###################################
                    auto theChip = static_cast<RD53*>(cChip);
                    fReadoutChipInterface->MaskAllChannels(cChip, true);
                    theChip->resetTDAC(0);
                    static_cast<RD53Interface*>(fReadoutChipInterface)->WriteRD53Mask(theChip, 0, false);

                    // #######################################################
                    // # Set default global DAC values required for the scan #
                    // #######################################################
                    uint16_t gdac_value = 900;
                    fReadoutChipInterface->WriteChipReg(theChip, "DAC_GDAC_L_LIN", gdac_value, true);
                    fReadoutChipInterface->WriteChipReg(theChip, "DAC_GDAC_M_LIN", gdac_value, true);
                    fReadoutChipInterface->WriteChipReg(theChip, "DAC_GDAC_R_LIN", gdac_value, true);

                    std::vector<std::string> comp_registers   = {"DAC_COMP_LIN"};
                    std::vector<std::string> preamp_registers = {"DAC_PREAMP_M_LIN", "DAC_PREAMP_R_LIN", "DAC_PREAMP_L_LIN", "DAC_PREAMP_T_LIN", "DAC_PREAMP_TR_LIN", "DAC_PREAMP_TL_LIN"};
                    std::vector<std::string> ldac_registers   = {"DAC_LDAC_LIN"};

                    MAX_PREAMP = RD53Shared::setBits(cChip->getNumberOfBits("DAC_PREAMP_M_LIN"));
                    MAX_COMP   = RD53Shared::setBits(cChip->getNumberOfBits("DAC_COMP_LIN"));
                    MAX_LDAC   = RD53Shared::setBits(cChip->getNumberOfBits("DAC_LDAC_LIN"));

                    // #################################
                    // # Reset COMP and LDAC registers #
                    // #################################
                    for(const auto& regName: comp_registers) fReadoutChipInterface->WriteChipReg(theChip, regName, 0, true);
                    for(const auto& regName: ldac_registers) fReadoutChipInterface->WriteChipReg(theChip, regName, 0, true);

                    // ###############
                    // # PREAMP scan #
                    // ###############
                    thePreamplifierCurrentContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<dataType>() =
                        linearScanBottomUp(theChip, preamp_registers, 0, MAX_PREAMP, "ANA_IN_CURR", PREAMP_CURRENT_mA);

                    // #############
                    // # COMP scan #
                    // #############
                    theComparatorCurrentContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<dataType>() =
                        linearScanBottomUp(theChip, comp_registers, 0, MAX_COMP, "ANA_IN_CURR", COMP_CURRENT_mA);

                    // ############################################
                    // # Set TDAC to a set value before LDAC scan #
                    // ############################################
                    theChip->resetTDAC(16);
                    static_cast<RD53Interface*>(fReadoutChipInterface)->WriteRD53Mask(theChip, 0, false);

                    // #################################
                    // # LDAC scan and final unmasking #
                    // #################################
                    theLDACCurrentContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<dataType>() =
                        linearScanBottomUp(theChip, ldac_registers, 0, MAX_LDAC, "ANA_IN_CURR", LDAC_CURRENT_mA);

                    fReadoutChipInterface->MaskAllChannels(cChip, false);
                }

    CalibBase::chipErrorReport();
}

void PowerTrimming::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillComparatorCurrentHisto(theComparatorCurrentContainer);
    histos->fillPreamplifierCurrentHisto(thePreamplifierCurrentContainer);
    histos->fillLDACCurrentHisto(theLDACCurrentContainer);
    histos->fillCustomHistos(fPowerTrimmingResults);
#endif
}

void PowerTrimming::draw(bool saveData)
{
#ifdef __USE_ROOT__
    CalibBase::bookHistoSaveMetadata(histos);
    PowerTrimming::fillHisto();
    histos->process();
#endif
}

dataType PowerTrimming::linearScanBottomUp(RD53* pChip, const std::vector<std::string>& regNames, uint16_t startValue, uint16_t maxValue, const std::string targetName, float& targetDiff)
{
    auto WriteChipRegisters = [this, pChip, regNames](uint16_t value)
    {
        for(const auto& regName: regNames) fReadoutChipInterface->WriteChipReg(pChip, regName, value, false);
    };

    std::vector<std::pair<std::string, uint16_t>> regDefaults;
    for(const auto& regName: regNames) regDefaults.push_back({regName, fReadoutChipInterface->ReadChipReg(pChip, regName)});

    std::stringstream _regNames;
    for(const auto& regName: regNames)
    {
        _regNames << regName;
        if(&regName != &regNames.back()) _regNames << " ";
    }

    LOG(INFO) << BOLDGREEN << "Chip " << BOLDYELLOW << pChip->geteFuseCode() << BOLDGREEN << ": starting a linear scan from " << BOLDYELLOW << startValue << BOLDGREEN
              << " to reach target current increase of " << BOLDYELLOW << targetDiff << BOLDGREEN << " mA for registers " << BOLDYELLOW << _regNames.str() << RESET;

    dataType result;

    uint16_t set_value = startValue;
    uint16_t pre_value = startValue;
    WriteChipRegisters(set_value);

    // ############################################
    // # Initial current reading and target setup #
    // ############################################
    float monitor      = 1.0e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, targetName, true);
    float target_value = monitor + targetDiff;
    float set_diff     = target_value - monitor;
    float pre_diff     = set_diff;

    std::ofstream outFile("current_manager.txt", std::ios_base::app);

    // ##############################################################################
    // # Main Scanning Loop: ramp up the DAC until the current difference is closed #
    // ##############################################################################
    while(set_diff > 0.0)
    {
        if(set_value < maxValue)
            set_value++;
        else
        {
            LOG(WARNING) << BOLDYELLOW << "Reached the maximum allowed value for the registers " << BOLDMAGENTA << _regNames.str() << BOLDYELLOW << " at " << BOLDMAGENTA << set_value << BOLDYELLOW
                         << ". Stopping the scan." << RESET;
            break;
        }
        WriteChipRegisters(set_value);
        monitor = 1.e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, targetName, true);

        PowerTrimmingData PTData;

        const auto now     = std::chrono::system_clock::now();
        const auto epoch   = now.time_since_epoch();
        const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(epoch);

        // Read all relevant ADC monitors (Currents and Voltages) at each step of the scan and save them in the result vector and in a txt file for monitoring
        PTData.timestamp      = seconds.count();
        PTData.bit            = set_value;
        PTData.ANA_IN_CURR    = 1.e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, "ANA_IN_CURR", true);
        PTData.DIG_IN_CURR    = 1.e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, "DIG_IN_CURR", true);
        PTData.VINA           = 4.0 * fReadoutChipInterface->ReadChipMonitor(pChip, "VINA", true);
        PTData.VDDA           = 2.0 * fReadoutChipInterface->ReadChipMonitor(pChip, "VDDA", true);
        PTData.VIND           = 4.0 * fReadoutChipInterface->ReadChipMonitor(pChip, "VIND", true);
        PTData.VDDD           = 2.0 * fReadoutChipInterface->ReadChipMonitor(pChip, "VDDD", true);
        PTData.Iref           = fReadoutChipInterface->ReadChipMonitor(pChip, "Iref", true);
        PTData.ANA_SHUNT_CURR = 1.e-3 * RD53Constants::SHUNT_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, "ANA_SHUNT_CURR", true);
        PTData.DIG_SHUNT_CURR = 1.e-3 * RD53Constants::SHUNT_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, "DIG_SHUNT_CURR", true);

        fPowerTrimmingResults.push_back(PTData);

        monitor = 1.e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, targetName, true);

        auto t  = std::time(nullptr);
        auto tm = *std::localtime(&t);

        if(outFile.is_open())
            outFile << std::put_time(&tm, "%H:%M:%S") << "\t" << set_value << "\t" << PTData.ANA_IN_CURR << "\t" << PTData.DIG_IN_CURR << "\t" << PTData.VINA << "\t" << PTData.VDDA << "\t"
                    << PTData.VIND << "\t" << PTData.VDDD << "\t" << PTData.Iref << "\t" << PTData.ANA_SHUNT_CURR << "\t" << PTData.DIG_SHUNT_CURR << "\n";

        pre_diff  = set_diff;
        pre_value = set_value;
        set_diff  = target_value - monitor;
        result.push_back(std::make_pair(set_value, monitor));
    }

    if(outFile.is_open()) outFile.close();

    if(std::abs(set_diff) > std::abs(pre_diff))
    {
        set_value = pre_value;
        set_diff  = pre_diff;
        WriteChipRegisters(set_value);
        result.pop_back();
    }
    float digcurr = 1.e-3 * RD53Constants::IN_CURR_FACTOR * fReadoutChipInterface->ReadChipMonitor(pChip, "DIG_IN_CURR", true);
    LOG(INFO) << BOLDYELLOW << "Chip " << pChip->geteFuseCode() << BOLDGREEN << ": scan ended. Found value " << BOLDYELLOW << result.back().first << BOLDGREEN << " with current " << targetName
              << " = " << BOLDYELLOW << result.back().second << BOLDGREEN << " mA (difference from target: " << set_diff << " mA) // dig. current: " << digcurr << " mA" << RESET;

    // ####################################################################################################
    // # PREAMP requires specific calibration factors depending on the physical area of the matrix region #
    // ####################################################################################################
    for(const auto& regName: regNames)
    {
        float calib_factor = 1.0;
        if(regName == "DAC_PREAMP_M_LIN")
            calib_factor = 1.0;
        else if(regName == "DAC_PREAMP_R_LIN")
            calib_factor = RD53BConstants::PREAMP_R;
        else if(regName == "DAC_PREAMP_L_LIN")
            calib_factor = RD53BConstants::PREAMP_L;
        else if(regName == "DAC_PREAMP_T_LIN")
            calib_factor = RD53BConstants::PREAMP_T;
        else if(regName == "DAC_PREAMP_TR_LIN")
            calib_factor = RD53BConstants::PREAMP_TR;
        else if(regName == "DAC_PREAMP_TL_LIN")
            calib_factor = RD53BConstants::PREAMP_TL;

        uint16_t bit_value   = result.back().first;
        int      calib_value = std::round(bit_value * calib_factor);

        if(calib_value > MAX_PREAMP) calib_value = MAX_PREAMP;

        LOG(INFO) << BOLDGREEN << "Register: " << BOLDYELLOW << std::setw(18) << std::left << regName << BOLDGREEN << " | Value: " << BOLDYELLOW << calib_value << RESET;
        fReadoutChipInterface->WriteChipReg(pChip, regName, calib_value, true);
    }

    // ###################################################################################################
    // # Scale DAC_FC_LIN and DAC_COMP_TA_LIN proportionally based on the newly found DAC_COMP_LIN value #
    // ###################################################################################################
    if(regNames.front() == "DAC_COMP_LIN")
    {
        uint16_t comp_value   = 110;
        uint16_t fc_value     = fReadoutChipInterface->ReadChipReg(pChip, "DAC_FC_LIN");
        uint16_t new_fc_value = std::round(fc_value * static_cast<float>(set_value) / comp_value);
        if(new_fc_value > RD53Shared::setBits(pChip->getNumberOfBits("DAC_FC_LIN"))) new_fc_value = RD53Shared::setBits(pChip->getNumberOfBits("DAC_FC_LIN"));

        uint16_t comp_ta_value     = fReadoutChipInterface->ReadChipReg(pChip, "DAC_COMP_TA_LIN");
        uint16_t new_comp_ta_value = std::round(comp_ta_value * static_cast<float>(set_value) / comp_value);
        if(new_comp_ta_value > RD53Shared::setBits(pChip->getNumberOfBits("DAC_COMP_TA_LIN"))) new_comp_ta_value = RD53Shared::setBits(pChip->getNumberOfBits("DAC_COMP_TA_LIN"));

        LOG(INFO) << BOLDGREEN << "Updated " << BOLDYELLOW << "DAC_FC_LIN     " << BOLDGREEN << " from " << fc_value << " to " << BOLDYELLOW << new_fc_value << RESET;
        LOG(INFO) << BOLDGREEN << "Updated " << BOLDYELLOW << "DAC_COMP_TA_LIN" << BOLDGREEN << " from " << comp_ta_value << " to " << BOLDYELLOW << new_comp_ta_value << RESET;
    }

    return result;
}
