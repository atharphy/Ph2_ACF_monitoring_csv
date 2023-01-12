#include "miniDAQ/CombinedCalibrationFactory.h"

#include "tools/BeamTestCheck.h"
#include "tools/CBCPulseShape.h"
#include "tools/CalibrationExample.h"
#include "tools/CombinedCalibration.h"
#include "tools/KIRA.h"
#include "tools/LatencyScan.h"
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
#include "MiddlewareController.h"
//#include "tools/SSAPhysics.h"
#include "tools/CicFEAlignment.h"
#include "tools/LinkAlignmentOT.h"
#include "tools/PSPhysics.h"
#include "tools/Physics2S.h"
#include "tools/StubBackEndAlignment.h"

#define CALIBRATION_NAME(x) #x,  CalibrationList::x

using namespace MessageUtils;

CombinedCalibrationFactory::CombinedCalibrationFactory()
{
    // OT calibrations
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>(CALIBRATION_NAME(calibration));
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, BeamTestCheck>(CALIBRATION_NAME(takedata)); // will be used in future version of GIPHT
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, KIRA>(CALIBRATION_NAME(calibrationandkira));
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise, KIRA>(CALIBRATION_NAME(calibrationandpedenoiseandkira)); // will be used in future version of GIPHT
    Register<LinkAlignmentOT, CicFEAlignment, PedeNoise>(CALIBRATION_NAME(pedenoise));
    Register<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise>(CALIBRATION_NAME(calibrationandpedenoise));
    Register<LinkAlignmentOT, CicFEAlignment, CalibrationExample>(CALIBRATION_NAME(calibrationexample));
    Register<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>(CALIBRATION_NAME(cbcpulseshape));
    Register<LinkAlignmentOT, CicFEAlignment, LatencyScan>(CALIBRATION_NAME(otlatency));

    // IT calibrations
    Register<PixelAlive>(CALIBRATION_NAME(pixelalive));
    Register<PixelAlive>(CALIBRATION_NAME(noise));
    Register<SCurve>(CALIBRATION_NAME(scurve));
    Register<Gain>(CALIBRATION_NAME(gain));
    Register<GainOptimization>(CALIBRATION_NAME(gainopt));
    Register<ThrEqualization>(CALIBRATION_NAME(threqu));
    Register<ThrMinimization>(CALIBRATION_NAME(thrmin));
    Register<ThrAdjustment>(CALIBRATION_NAME(thradj));
    Register<Latency>(CALIBRATION_NAME(latency));
    Register<InjectionDelay>(CALIBRATION_NAME(injdelay));
    Register<ClockDelay>(CALIBRATION_NAME(clockdelay));
    Register<Physics>(CALIBRATION_NAME(physics));
    Register<PSPhysics>(CALIBRATION_NAME(psphysics));
    Register<Physics2S>(CALIBRATION_NAME(physics2s));
    Register<DataTransmissionTest>(CALIBRATION_NAME(datatrtest));
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
