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
}

void LpGBTeyeOpening::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[LpGBTeyeOpening::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    LpGBTeyeOpening::run();
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
    const int timeMax = 64;
    const int voltMax = 31;

    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint16_t, timeMax, voltMax>>(*fDetectorContainer, theLpGBTeyeOpeningContainer);

    // ####################
    // # Pause monitoring #
    // ####################
    if(this->fDetectorMonitor != nullptr) this->fDetectorMonitor->pauseMonitoring();

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            flpGBTInterface->WriteChipReg(cOpticalGroup->flpGBT, "EQConfig", (lpGBTattenuation << 3) | (cOpticalGroup->flpGBT->getReg("EQConfig") & 0x3));
            flpGBTInterface->ConfigureEOM(cOpticalGroup->flpGBT, 7, false, true);
        }

        for(uint8_t voltage = 0; voltage < voltMax; voltage++)
        {
            for(auto cOpticalGroup: *cBoard) flpGBTInterface->SelectEOMVof(cOpticalGroup->flpGBT, voltage);

            for(uint8_t time = 0; time < timeMax; time++)
            {
                for(auto cOpticalGroup: *cBoard) flpGBTInterface->SelectEOMPhase(cOpticalGroup->flpGBT, time);
                std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
                for(auto cOpticalGroup: *cBoard) flpGBTInterface->StartEOM(cOpticalGroup->flpGBT, true);

                for(auto cOpticalGroup: *cBoard)
                {
                    auto     theEyeArray = theLpGBTeyeOpeningContainer.getOpticalGroup(cBoard->getId(), cOpticalGroup->getId())->getSummary<GenericDataArray<uint16_t, timeMax, voltMax>>();
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
                        theEyeArray[time][voltage] = 0;
                        continue;
                    }

                    flpGBTInterface->StartEOM(cOpticalGroup->flpGBT, false);
                    theEyeArray[time][voltage] = flpGBTInterface->GetEOMCounter(cOpticalGroup->flpGBT);
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
#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
    LpGBTeyeOpening::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void LpGBTeyeOpening::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillIntensity(theLpGBTeyeOpeningContainer);
#endif
}
