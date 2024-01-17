#include "tools/TuneLpGBTVref.h"
#include "System/RegisterHelper.h"

std::string TuneLpGBTVref::fCalibrationDescription = "Tune Vref value for LpGBT calibration";

TuneLpGBTVref::TuneLpGBTVref() : Tool() {}

TuneLpGBTVref::~TuneLpGBTVref() {}

void TuneLpGBTVref::Initialise()
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::LpGBT, "VREFTUNE");
}

void TuneLpGBTVref::reset() { fRegisterHelper->restoreSnapshot(); }

void TuneLpGBTVref::tuneVref()
{
    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;
            flpGBTInterface->ConfigureInternalMonitoring(clpGBT, 0);
            flpGBTInterface->TuneVref(clpGBT);
        }
    }
}

void TuneLpGBTVref::Running()
{
    Initialise();
    tuneVref();
    reset();
}

void TuneLpGBTVref::Stop() {}

void TuneLpGBTVref::Pause() {}

void TuneLpGBTVref::Resume() {}
