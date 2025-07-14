/*!
  \file                  RD53LpGBTeyeOpening.cc
  \brief                 Implementaion of LpGBT eye opening scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  29/11/24
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53LpGBTeyeOpening.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void LpGBTeyeOpening::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    CalibBase::ConfigureCalibration();
    lpGBTattenuation = this->findValueInSettings<double>("LpGBTattenuation");
    doDisplay        = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip     = this->findValueInSettings<double>("UpdateChipCfg");
}

void LpGBTeyeOpening::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[LpGBTeyeOpening::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    LpGBTeyeOpening::run();
    LpGBTeyeOpening::analyze();
    LpGBTeyeOpening::draw();
    LpGBTeyeOpening::sendData();
}

void LpGBTeyeOpening::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("LpGBTeyeOpening");
        theContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theLpGBTeyeOpeningContainer);
    }
}

void LpGBTeyeOpening::Stop()
{
    LOG(INFO) << GREEN << "[LpGBTeyeOpening::Stop] Stopping" << RESET;
    CalibBase::Stop();
}

void LpGBTeyeOpening::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos = nullptr;

    LOG(INFO) << GREEN << "[LpGBTeyeOpening::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    LpGBTeyeOpening::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "LpGBTeyeOpening", histos);
}

void LpGBTeyeOpening::run()
{
    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint16_t, TIMEMAX, VOLTMAX>>(*fDetectorContainer, theLpGBTeyeOpeningContainer);

    // ####################
    // # Pause monitoring #
    // ####################
    if(this->fDetectorMonitor != nullptr) this->fDetectorMonitor->pauseMonitoring();

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            if(cOpticalGroup->flpGBT == nullptr) throw std::runtime_error("LpGBT not enabled in configuration file for optical group ID " + std::to_string(cOpticalGroup->getId()));
            flpGBTInterface->WriteChipReg(cOpticalGroup->flpGBT, "EQConfig", (lpGBTattenuation << 3) | (cOpticalGroup->flpGBT->getReg("EQConfig") & 0x3));
            flpGBTInterface->ConfigureEOM(cOpticalGroup->flpGBT, 7, false, true);
        }

        for(uint8_t voltage = 0; voltage < VOLTMAX; voltage++)
        {
            for(auto cOpticalGroup: *cBoard) flpGBTInterface->SelectEOMVof(cOpticalGroup->flpGBT, voltage);

            for(uint8_t time = 0; time < TIMEMAX; time++)
            {
                for(auto cOpticalGroup: *cBoard) flpGBTInterface->SelectEOMPhase(cOpticalGroup->flpGBT, time);
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
                for(auto cOpticalGroup: *cBoard) flpGBTInterface->StartEOM(cOpticalGroup->flpGBT, true);

                for(auto cOpticalGroup: *cBoard)
                {
                    auto&    theEyeArray = theLpGBTeyeOpeningContainer.getOpticalGroup(cBoard->getId(), cOpticalGroup->getId())->getSummary<GenericDataArray<uint16_t, TIMEMAX, VOLTMAX>>();
                    uint8_t  EOMStatus   = flpGBTInterface->GetEOMStatus(cOpticalGroup->flpGBT);
                    uint16_t nAttempts   = 0;

                    while((EOMStatus & (0x1 << 1) >> 1) && !(EOMStatus & (0x1 << 0)))
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
                        EOMStatus = flpGBTInterface->GetEOMStatus(cOpticalGroup->flpGBT);
                        if(nAttempts++ == RD53Shared::MAXATTEMPTS) break;
                    }

                    if(nAttempts == RD53Shared::MAXATTEMPTS)
                    {
                        theEyeArray.at(time).at(voltage) = 0;
                        continue;
                    }

                    // #################
                    // # Progress menu #
                    // #################
                    LOG(INFO) << CYAN << "************* " << GREEN << "Scanning" << CYAN << " *************" << RESET;
                    LOG(INFO) << GREEN << "Volt: " << BOLDYELLOW << std::setw(2) << std::fixed << +voltage << " mV (" << VOLTMAX - 1 << ") " << RESET << GREEN << "-- Time: " << BOLDYELLOW
                              << std::setw(2) << std::fixed << +time << " ps (" << TIMEMAX - 1 << ")" << RESET;
                    LOG(INFO) << CYAN << "************************************" << RESET;
                    if((voltage < VOLTMAX - 1) || (time < TIMEMAX - 1)) std::cout << std::setprecision(-1) << "\x1b[A\x1b[A\x1b[A";

                    theEyeArray.at(time).at(voltage) = flpGBTInterface->GetEOMCounter(cOpticalGroup->flpGBT);
                    flpGBTInterface->StartEOM(cOpticalGroup->flpGBT, false);
                }
            }
        }
    }

    // #####################
    // # Resume monitoring #
    // #####################
    if(this->fDetectorMonitor != nullptr) this->fDetectorMonitor->resumeMonitoring();
}

void LpGBTeyeOpening::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
    LpGBTeyeOpening::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

std::shared_ptr<DetectorDataContainer> LpGBTeyeOpening::analyze()
{
    const float eyeOpeningThresholdWrtMaxCut = 0.9; // @CONST@
    bool        False                        = false;
    summaryContainer                         = std::make_shared<DetectorDataContainer>();
    ContainerFactory::copyAndInitOpticalGroup<bool>(*fDetectorContainer, *summaryContainer, False);

    for(const auto cBoard: theLpGBTeyeOpeningContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            auto& theEyeArray = cOpticalGroup->getSummary<GenericDataArray<uint16_t, TIMEMAX, VOLTMAX>>();
            if(theEyeArray.size() == 0) continue;

            float                      zMax = 0;
            std::array<float, VOLTMAX> yProjection{0};
            for(uint8_t voltage = 0; voltage < VOLTMAX; voltage++)
                for(uint8_t time = 0; time < TIMEMAX; time++)
                {
                    if(theEyeArray.at(time).at(voltage) > zMax) zMax = theEyeArray.at(time).at(voltage);
                    yProjection.at(voltage) += theEyeArray.at(time).at(voltage);
                }
            auto yBinMax = std::max_element(yProjection.begin(), yProjection.end());

            float eyeCrossingVoltage          = std::distance(yProjection.begin(), yBinMax);
            float eyeCrossingVoltageSum       = yProjection.at(eyeCrossingVoltage);
            float eyeCrossingVoltageMinus1Sum = yProjection.at(eyeCrossingVoltage - 1);
            float eyeCrossingVoltagePlus1Sum  = yProjection.at(eyeCrossingVoltage + 1);

            // ##############################################################
            // # Delta around max allows to find a bit better the real peak #
            // ##############################################################
            float delta = 0.5 * (eyeCrossingVoltageMinus1Sum - eyeCrossingVoltagePlus1Sum) / (eyeCrossingVoltageMinus1Sum + eyeCrossingVoltagePlus1Sum - 2 * eyeCrossingVoltageSum);
            float eyeCrossingFractionalOffset = eyeCrossingVoltage + delta;

            // ####################################################################################################################
            // # After the projected Y max has been found, the histo is sliced at that point to get the crossing width of the eye #
            // ####################################################################################################################
            std::array<float, TIMEMAX> xProjectionAtMaxy{0};
            for(uint8_t time = 0; time < TIMEMAX; time++) xProjectionAtMaxy.at(time) = theEyeArray.at(time).at(eyeCrossingVoltage);
            float threshold = eyeOpeningThresholdWrtMaxCut * zMax;

            float crossingWidthAtyMax = 0;
            for(const auto& ele: xProjectionAtMaxy)
                if(ele < threshold) crossingWidthAtyMax += 1;

            float eyeOpeningArea = 0;
            for(uint8_t voltage = 0; voltage < VOLTMAX; voltage++)
                for(uint8_t time = 0; time < TIMEMAX; time++)
                {
                    float binContent = theEyeArray.at(time).at(voltage);
                    if(binContent > threshold) eyeOpeningArea += 1 * 100; // Multiplied by 100 to get a percentage
                }
            eyeOpeningArea /= VOLTMAX * TIMEMAX;

            LOG(INFO) << GREEN << "Results for [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << RESET << GREEN << "]" << RESET;

            // #################
            // # Voltage grade #
            // #################
            float voltageCrossingDifference = abs(eyeCrossingFractionalOffset - (VOLTMAX / 2)) / (VOLTMAX / 2);
            LOG(INFO) << GREEN << "Eye crossing fractional offset = " << BOLDYELLOW << eyeCrossingFractionalOffset << RESET;
            if(voltageCrossingDifference < 0.1) // @CONST@
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "A" << RESET;
            else if(voltageCrossingDifference < 0.2)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "B" << RESET;
            else if(voltageCrossingDifference < 0.3)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "C" << RESET;
            else if(voltageCrossingDifference >= 0.3)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDRED << "F" << RESET;

            // ###############
            // # Width grade #
            // ###############
            LOG(INFO) << GREEN << "Crossing width at yMax = " << BOLDYELLOW << crossingWidthAtyMax << RESET;
            if(crossingWidthAtyMax <= 0.1) // @CONST@
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "A" << RESET;
            else if(crossingWidthAtyMax <= 0.2)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "B" << RESET;
            else if(crossingWidthAtyMax <= 0.3)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "C" << RESET;
            else if(crossingWidthAtyMax >= 0.3)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDRED << "F" << RESET;

            // ##############
            // # Area grade #
            // ##############
            LOG(INFO) << GREEN << "Eye opening area = " << BOLDYELLOW << eyeOpeningArea << RESET;
            if(eyeOpeningArea >= 0.7) // @CONST@
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "A" << RESET;
            else if(eyeOpeningArea >= 0.6)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "B" << RESET;
            else if(eyeOpeningArea >= 0.5)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDYELLOW << "C" << RESET;
            else if(eyeOpeningArea < 0.5)
                LOG(INFO) << BOLDBLUE << "\t--> Optical grade: " << BOLDRED << "F" << RESET;
        }

    return summaryContainer;
}

void LpGBTeyeOpening::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillIntensity(theLpGBTeyeOpeningContainer);
#endif
}
