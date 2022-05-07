#include "../miniDAQ/CombinedCalibrationFactory.h"

#include "../tools/Tool.h"
#include "../tools/CBCPulseShape.h"
#include "../tools/CalibrationExample.h"
#include "../tools/CombinedCalibration.h"
#include "../tools/LatencyScan.h"
#include "../tools/PedeNoise.h"
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
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>("calibration",ConfigurationInfo::CALIBRATION);
    Register<LinkAlignmentOT, CicFEAlignment, PedeNoise>("pedenoise",ConfigurationInfo::PEDENOISE);
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise>("calibrationandpedenoise",ConfigurationInfo::CALIBRATIONANDPEDENOISE);
    Register<LinkAlignmentOT, CicFEAlignment, CalibrationExample>("calibrationexample",ConfigurationInfo::CALIBRATIONEXAMPLE);
    Register<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>("cbcPulseShape",ConfigurationInfo::CBCPULSESHAPE);
    Register<LinkAlignmentOT, CicFEAlignment, LatencyScan>("OTLatency",ConfigurationInfo::OTLATENCY);

    // IT calibrations
    Register<PixelAlive>("pixelalive", ConfigurationInfo::PIXELALIVE);
    Register<PixelAlive>("noise", ConfigurationInfo::NOISE);
    Register<SCurve>("scurve", ConfigurationInfo::SCURVE);
    Register<Gain>("gain", ConfigurationInfo::GAIN);
    Register<GainOptimization>("gainopt", ConfigurationInfo::GAINOPT);
    Register<ThrEqualization>("threqu", ConfigurationInfo::THREQU);
    Register<ThrMinimization>("thrmin", ConfigurationInfo::THRMIN);
    Register<ThrAdjustment>("thradj", ConfigurationInfo::THRADJ);
    Register<Latency>("latency", ConfigurationInfo::LATENCY);
    Register<InjectionDelay>("injdelay", ConfigurationInfo::INJDELAY);
    Register<ClockDelay>("clockdelay", ConfigurationInfo::CLOCKDELAY);
    Register<Physics>("physics", ConfigurationInfo::PHYSICS);
    Register<PSPhysics>("psphysics", ConfigurationInfo::PSPHYSICS);
    Register<Physics2S>("2sphysics", ConfigurationInfo::PHYSICS2S);
    Register<DataTransmissionTest>("datatrtest", ConfigurationInfo::DATATRTEST);

}

CombinedCalibrationFactory::~CombinedCalibrationFactory()
{
    for(auto& element : fCalibrationMap)
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

    for(const auto& element : fCalibrationMap)
    {
        listOfCalibrations.emplace_back(element.first);
    }
    return listOfCalibrations;
}
