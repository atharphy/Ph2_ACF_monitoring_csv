#include "tools/OTLpGBTEyeOpeningTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTLpGBTEyeOpeningTest::fCalibrationDescription = "Insert brief calibration description here";

OTLpGBTEyeOpeningTest::OTLpGBTEyeOpeningTest() : Tool() {}

OTLpGBTEyeOpeningTest::~OTLpGBTEyeOpeningTest() {}

void OTLpGBTEyeOpeningTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTLpGBTEyeOpeningTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTLpGBTEyeOpeningTest::ConfigureCalibration() {}

void OTLpGBTEyeOpeningTest::Running()
{
    LOG(INFO) << "Starting OTLpGBTEyeOpeningTest measurement.";
    Initialise();
    runEyeOpeningTest();
    LOG(INFO) << "Done with OTLpGBTEyeOpeningTest.";
    Reset();
}

void OTLpGBTEyeOpeningTest::Stop(void)
{
    LOG(INFO) << "Stopping OTLpGBTEyeOpeningTest measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTLpGBTEyeOpeningTest.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTLpGBTEyeOpeningTest stopped.";
}

void OTLpGBTEyeOpeningTest::Pause() {}

void OTLpGBTEyeOpeningTest::Resume() {}

void OTLpGBTEyeOpeningTest::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTLpGBTEyeOpeningTest::runEyeOpeningTest()
{
    DetectorDataContainer theEyeOpeningContainer;
    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint16_t, 64, 31>>(*fDetectorContainer, theEyeOpeningContainer);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard) { flpGBTInterface->ConfigureEOM(theOpticalGroup->flpGBT, 7, false, true); }

        for(uint8_t cVoltageStep = 0; cVoltageStep < 31; cVoltageStep++)
        {
            for(auto theOpticalGroup: *theBoard) { flpGBTInterface->SelectEOMVof(theOpticalGroup->flpGBT, cVoltageStep); }

            for(uint8_t cTimeStep = 0; cTimeStep < 64; cTimeStep++)
            {
                for(auto theOpticalGroup: *theBoard) { flpGBTInterface->SelectEOMPhase(theOpticalGroup->flpGBT, cTimeStep); }
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                for(auto theOpticalGroup: *theBoard) { flpGBTInterface->StartEOM(theOpticalGroup->flpGBT, true); }

                for(auto theOpticalGroup: *theBoard)
                {
                    auto&    theEyeArray            = theEyeOpeningContainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<GenericDataArray<uint16_t, 64, 31>>();
                    uint8_t  cEOMStatus             = flpGBTInterface->GetEOMStatus(theOpticalGroup->flpGBT);
                    uint16_t numberOfAttempts       = 0;
                    uint16_t maximumAllowedAttempts = 10;
                    while((cEOMStatus & (0x1 << 1) >> 1) && !(cEOMStatus & (0x1 << 0)))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        cEOMStatus = flpGBTInterface->GetEOMStatus(theOpticalGroup->flpGBT);
                        if(numberOfAttempts++ > 10) break;
                        ;
                    }
                    if(numberOfAttempts == maximumAllowedAttempts)
                    {
                        theEyeArray[cTimeStep][cVoltageStep] = 0;
                        continue;
                    }
                    uint16_t cCounterValue = flpGBTInterface->GetEOMCounter(theOpticalGroup->flpGBT);
                    flpGBTInterface->StartEOM(theOpticalGroup->flpGBT, false);
                    theEyeArray[cTimeStep][cVoltageStep] = cCounterValue;
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTLpGBTEyeOpeningTest.fillEyeOpening(theEyeOpeningContainer);
#else
    if(fDQMStreamer)
    {
        ContainerSerialization theEyeOpeningSerialization("OTLpGBTEyeOpeningTestEyeOpening");
        theEyeOpeningSerialization.streamByOpticalGroupContainer(fDQMStreamer, theEyeOpeningContainer);
    }
#endif
}