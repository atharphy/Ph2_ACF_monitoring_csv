#include "../miniDAQ/CombinedCalibrationFactory.h"

#include "../tools/CBCPulseShape.h"
#include "../tools/CalibrationExample.h"
#include "../tools/CombinedCalibration.h"
#include "../tools/LatencyScan.h"
#include "../tools/PedeNoise.h"
#include "../tools/BeamTestCheck.h"
#include "../tools/PedestalEqualization.h"
#include "../tools/RD53ClockDelay.h"
#include "../tools/RD53DataTransmissionTest.h"
#include "../tools/RD53Gain.h"
#include "../tools/RD53GainOptimization.h"
#include "../tools/RD53InjectionDelay.h"
#include "../tools/RD53Latency.h"
#include "../tools/RD53Physics.h"
#include "../tools/RD53PixelAlive.h"
#include "../tools/RD53SCurve.h"
#include "../tools/RD53ThrAdjustment.h"
#include "../tools/RD53ThrEqualization.h"
#include "../tools/RD53ThrMinimization.h"
#include "../tools/Tool.h"
#include "MiddlewareController.h"
//#include "../tools/SSAPhysics.h"
#include "../tools/CicFEAlignment.h"
#include "../tools/LinkAlignmentOT.h"
#include "../tools/PSPhysics.h"
#include "../tools/Physics2S.h"
#include "../tools/StubBackEndAlignment.h"

using namespace MessageUtils;

CombinedCalibrationFactory::CombinedCalibrationFactory()
{
    // OT calibrations
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>("calibration", CalibrationList::CALIBRATION);
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, BeamTestCheck>("takedata", CalibrationList::TAKEDATA); // will be used in future version of GIPHT
    Register<LinkAlignmentOT, CicFEAlignment, PedeNoise>("pedenoise", CalibrationList::PEDENOISE);
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise>("calibrationandpedenoise", CalibrationList::CALIBRATIONANDPEDENOISE);
    Register<LinkAlignmentOT, CicFEAlignment, CalibrationExample>("calibrationexample", CalibrationList::CALIBRATIONEXAMPLE);
    Register<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>("cbcPulseShape", CalibrationList::CBCPULSESHAPE);
    Register<LinkAlignmentOT, CicFEAlignment, LatencyScan>("OTLatency", CalibrationList::OTLATENCY);

    // IT calibrations
    Register<PixelAlive>("pixelalive", CalibrationList::PIXELALIVE);
    Register<PixelAlive>("noise", CalibrationList::NOISE);
    Register<SCurve>("scurve", CalibrationList::SCURVE);
    Register<Gain>("gain", CalibrationList::GAIN);
    Register<GainOptimization>("gainopt", CalibrationList::GAINOPT);
    Register<ThrEqualization>("threqu", CalibrationList::THREQU);
    Register<ThrMinimization>("thrmin", CalibrationList::THRMIN);
    Register<ThrAdjustment>("thradj", CalibrationList::THRADJ);
    Register<Latency>("latency", CalibrationList::LATENCY);
    Register<InjectionDelay>("injdelay", CalibrationList::INJDELAY);
    Register<ClockDelay>("clockdelay", CalibrationList::CLOCKDELAY);
    Register<Physics>("physics", CalibrationList::PHYSICS);
    Register<PSPhysics>("psphysics", CalibrationList::PSPHYSICS);
    Register<Physics2S>("2sphysics", CalibrationList::PHYSICS2S);
    Register<DataTransmissionTest>("datatrtest", CalibrationList::DATATRTEST);
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

Tool* CombinedCalibrationFactory::CreateCombinedCalibration(const std::string& calibrationTag) const
{
    try
    {
        return fCalibrationMap.at(calibrationTag)->Create();
    }
    catch(const std::exception& theException)
    {
        std::string errorMessage = "Error: calibration tag " + calibrationTag + " does not exist";
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
