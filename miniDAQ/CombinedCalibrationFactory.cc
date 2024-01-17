#include "miniDAQ/CombinedCalibrationFactory.h"

#include "MiddlewareController.h"
#include "tools/BeamTestCheck.h"
#include "tools/CBCPulseShape.h"
#include "tools/CalibrationExample.h"
#include "tools/CombinedCalibration.h"
#include "tools/KIRA.h"
#include "tools/LatencyScan.h"
#include "tools/OTTemperature.h"
#include "tools/OTVTRXLightOff.h"
#include "tools/PSAlignment.h"
#include "tools/PedeNoise.h"
#include "tools/PedestalEqualization.h"
#include "tools/RD53ClockDelay.h"
#include "tools/RD53DataTransmissionTest.h"
#include "tools/RD53Gain.h"
#include "tools/RD53GainOptimization.h"
#include "tools/RD53InjectionDelay.h"
#include "tools/RD53Latency.h"
#include "tools/RD53Physics.h"
#include "tools/RD53PixelAlive.h"
#include "tools/RD53SCurve.h"
#include "tools/RD53ThrAdjustment.h"
#include "tools/RD53ThrEqualization.h"
#include "tools/RD53ThrMinimization.h"
#include "tools/Tool.h"
#include "tools/TuneLpGBTVref.h"
//#include "tools/SSAPhysics.h"
#include "tools/CicFEAlignment.h"
#include "tools/ConfigureOnly.h"
#include "tools/LinkAlignmentOT.h"
#include "tools/OTalignLpGBTinputs.h"
#include "tools/PSPhysics.h"
#include "tools/Physics2S.h"
#include "tools/StubBackEndAlignment.h"

using namespace MessageUtils;

CombinedCalibrationFactory::CombinedCalibrationFactory()
{
    // Common calibrations
    Register<TuneLpGBTVref>("Common", "tunelpgbtvref");
    Register<ConfigureOnly>("Common", "configureonly");

    // OT calibrations
    Register<OTVTRXLightOff>("Outer Tracker", "vtrxoff");
    Register<OTalignLpGBTinputs>("Outer Tracker", "OTalignLpGBTinputs");
    Register<LinkAlignmentOT, CicFEAlignment>("Outer Tracker", "alignment");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>("Outer Tracker", "calibration");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, BeamTestCheck>("Outer Tracker", "takedata"); // will be used in future version of GIPHT
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, KIRA>("Outer Tracker", "calibrationandkira");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise, KIRA>("Outer Tracker", "calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register<OTTemperature, PSAlignment, LinkAlignmentOT, CicFEAlignment, PedeNoise, OTTemperature>("Outer Tracker", "pedenoise");
    Register<TuneLpGBTVref, OTTemperature, PSAlignment, LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise, OTTemperature>("Outer Tracker", "calibrationandpedenoise");
    Register<LinkAlignmentOT, CicFEAlignment, CalibrationExample>("Outer Tracker", "calibrationexample");
    Register<LinkAlignmentOT, CicFEAlignment, LatencyScan>("Outer Tracker", "otlatency");

    // 2S specific calibrations
    Register<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>("2S Module", "cbcpulseshape");
    Register<Physics2S>("2S Module", "physics2s");

    // PS specific calibrations
    Register<PSPhysics>("PS Module", "psphysics");

    // IT calibrations
    Register<PixelAlive>("Inner Tracker", "pixelalive");
    Register<PixelAlive>("Inner Tracker", "noise");
    Register<SCurve>("Inner Tracker", "scurve");
    Register<Gain>("Inner Tracker", "gain");
    Register<GainOptimization>("Inner Tracker", "gainopt");
    Register<ThrEqualization>("Inner Tracker", "threqu");
    Register<ThrMinimization>("Inner Tracker", "thrmin");
    Register<ThrAdjustment>("Inner Tracker", "thradj");
    Register<Latency>("Inner Tracker", "latency");
    Register<InjectionDelay>("Inner Tracker", "injdelay");
    Register<ClockDelay>("Inner Tracker", "clockdelay");
    Register<Physics>("Inner Tracker", "physics");
    Register<DataTransmissionTest>("Inner Tracker", "datatrtest");
}

CombinedCalibrationFactory::~CombinedCalibrationFactory()
{
    for(auto& calibrationListPerHardware : fCalibrationMap)
    {
        delete calibrationListPerHardware.second.second;
        calibrationListPerHardware.second.second = nullptr;
    }
    fCalibrationMap.clear();
}

Tool* CombinedCalibrationFactory::createCombinedCalibration(const std::string& calibrationName) const
{
    try
    {
        return fCalibrationMap.at(calibrationName).second->Create();
    }
    catch(const std::exception& theException)
    {
        std::string errorMessage = "Error: calibration tag " + calibrationName + " does not exist";
        throw std::runtime_error(errorMessage);
    }

    return nullptr;
}

std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> CombinedCalibrationFactory::getAvailableCalibrations() const
{
    std::map<std::string, std::map<std::string, std::vector<std::pair<std::string, std::string>>>> listOfCalibrations;

    for(const auto& element: fCalibrationMap) { listOfCalibrations[element.second.first][element.first] = element.second.second->fSubCalibrationAndDescriptionList; }
    return listOfCalibrations;
}
