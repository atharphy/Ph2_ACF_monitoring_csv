#include "DQMUtils/DQMCalibrationFactory.h"
#include "DQMUtils/CBCHistogramPulseShape.h"
#include "DQMUtils/DQMHistogramBeamTestCheck.h"
#include "DQMUtils/DQMHistogramCalibrationExample.h"
#include "DQMUtils/DQMHistogramKira.h"
#include "DQMUtils/DQMHistogramLatencyScan.h"
#include "DQMUtils/DQMHistogramPedeNoise.h"
#include "DQMUtils/DQMHistogramPedestalEqualization.h"
#include "DQMUtils/PSPhysicsHistograms.h"
#include "DQMUtils/Physics2SHistograms.h"
#include "DQMUtils/RD53ClockDelayHistograms.h"
#include "DQMUtils/RD53DataTransmissionTestGraphs.h"
#include "DQMUtils/RD53GainHistograms.h"
#include "DQMUtils/RD53GainOptimizationHistograms.h"
#include "DQMUtils/RD53InjectionDelayHistograms.h"
#include "DQMUtils/RD53LatencyHistograms.h"
#include "DQMUtils/RD53PhysicsHistograms.h"
#include "DQMUtils/RD53PixelAliveHistograms.h"
#include "DQMUtils/RD53SCurveHistograms.h"
#include "DQMUtils/RD53ThrEqualizationHistograms.h"
#include "DQMUtils/RD53ThresholdHistograms.h"

using namespace MessageUtils;

DQMCalibrationFactory::DQMCalibrationFactory()
{
    // OT calibrations
    Register<DQMHistogramPedestalEqualization>("calibration");
    Register<DQMHistogramPedestalEqualization, DQMHistogramBeamTestCheck>("takedata"); // will be used in future version of GIPHT
    Register<DQMHistogramPedestalEqualization, DQMHistogramKira>("calibrationandkira");
    Register<DQMHistogramPedestalEqualization, DQMHistogramPedeNoise, DQMHistogramKira>("calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register<DQMHistogramPedeNoise>("pedenoise");
    Register<DQMHistogramPedestalEqualization, DQMHistogramPedeNoise>("calibrationandpedenoise");
    Register<DQMHistogramCalibrationExample>("calibrationexample");
    Register<CBCHistogramPulseShape>("cbcpulseshape");
    Register<DQMHistogramLatencyScan>("otlatency");

    // IT calibrations
    Register<PixelAliveHistograms>("pixelalive");
    Register<PixelAliveHistograms>("noise");
    Register<SCurveHistograms>("scurve");
    Register<GainHistograms>("gain");
    Register<GainOptimizationHistograms>("gainopt");
    Register<ThrEqualizationHistograms>("threqu");
    Register<ThresholdHistograms>("thrmin");
    Register<ThresholdHistograms>("thradj");
    Register<LatencyHistograms>("latency");
    Register<InjectionDelayHistograms>("injdelay");
    Register<ClockDelayHistograms>("clockdelay");
    Register<PhysicsHistograms>("physics");
    Register<PSPhysicsHistograms>("psphysics");
    Register<Physics2SHistograms>("physics2s");
    Register<DataTransmissionTestGraphs>("datatrtest");
}

DQMCalibrationFactory::~DQMCalibrationFactory()
{
    for(auto& element: fDQMInterfaceMap)
    {
        delete element.second;
        element.second = nullptr;
    }
    fDQMInterfaceMap.clear();
}

std::vector<DQMHistogramBase*> DQMCalibrationFactory::createDQMHistogrammerVector(const std::string& calibrationTag) const
{
    try
    {
        return fDQMInterfaceMap.at(calibrationTag)->Create();
    }
    catch(const std::exception& theException)
    {
        std::string errorMessage = "Error: calibration tag " + calibrationTag + " does not exist";
        throw std::runtime_error(errorMessage);
    }
}

std::vector<std::string> DQMCalibrationFactory::getAvailableCalibrations() const
{
    std::vector<std::string> listOfCalibrations;

    for(const auto& element: fDQMInterfaceMap) { listOfCalibrations.emplace_back(element.first); }
    return listOfCalibrations;
}
