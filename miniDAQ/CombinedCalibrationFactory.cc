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
    Register<TuneLpGBTVref>("tunelpgbtvref");
    Register<ConfigureOnly>("configureonly");

    // OT calibrations
    Register<OTVTRXLightOff>("vtrxoff");
    Register<OTalignLpGBTinputs>("OTalignLpGBTinputs");
    Register<LinkAlignmentOT, CicFEAlignment>("alignment");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>("calibration");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, BeamTestCheck>("takedata"); // will be used in future version of GIPHT
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, KIRA>("calibrationandkira");
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise, KIRA>("calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register<OTTemperature, PSAlignment, LinkAlignmentOT, CicFEAlignment, PedeNoise, OTTemperature>("pedenoise");
    Register<TuneLpGBTVref, OTTemperature, PSAlignment, LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise, OTTemperature>("calibrationandpedenoise");
    Register<LinkAlignmentOT, CicFEAlignment, CalibrationExample>("calibrationexample");
    Register<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>("cbcpulseshape");
    Register<LinkAlignmentOT, CicFEAlignment, LatencyScan>("otlatency");

    // IT calibrations
    Register<PixelAlive>("pixelalive");
    Register<PixelAlive>("noise");
    Register<SCurve>("scurve");
    Register<Gain>("gain");
    Register<GainOptimization>("gainopt");
    Register<ThrEqualization>("threqu");
    Register<ThrMinimization>("thrmin");
    Register<ThrAdjustment>("thradj");
    Register<Latency>("latency");
    Register<InjectionDelay>("injdelay");
    Register<ClockDelay>("clockdelay");
    Register<Physics>("physics");
    Register<PSPhysics>("psphysics");
    Register<Physics2S>("physics2s");
    Register<DataTransmissionTest>("datatrtest");
}

CombinedCalibrationFactory::~CombinedCalibrationFactory()
{
    for(auto& element: fCalibrationMap)
    {
        delete element.second;
        element.second = nullptr;
    }
    fCalibrationMap.clear();
}

Tool* CombinedCalibrationFactory::createCombinedCalibration(const std::string& calibrationName) const
{
    try
    {
        return fCalibrationMap.at(calibrationName)->Create();
    }
    catch(const std::exception& theException)
    {
        std::string errorMessage = "Error: calibration tag " + calibrationName + " does not exist";
        throw std::runtime_error(errorMessage);
    }

    return nullptr;
}

std::vector<std::string> CombinedCalibrationFactory::getAvailableCalibrations() const
{
    std::vector<std::string> listOfCalibrations;

    for(const auto& element: fCalibrationMap) { listOfCalibrations.emplace_back(element.first); }
    return listOfCalibrations;
}
