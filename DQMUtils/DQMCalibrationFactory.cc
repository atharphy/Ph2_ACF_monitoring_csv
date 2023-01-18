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

#define CALIBRATION_NAME(x) #x, CalibrationList::x

using namespace MessageUtils;

DQMCalibrationFactory::DQMCalibrationFactory()
{
    // OT calibrations
    Register<DQMHistogramPedestalEqualization>(CALIBRATION_NAME(calibration));
    Register<DQMHistogramPedestalEqualization, DQMHistogramBeamTestCheck>(CALIBRATION_NAME(takedata)); // will be used in future version of GIPHT
    Register<DQMHistogramPedestalEqualization, DQMHistogramKira>(CALIBRATION_NAME(calibrationandkira));
    Register<DQMHistogramPedestalEqualization, DQMHistogramPedeNoise, DQMHistogramKira>(CALIBRATION_NAME(calibrationandpedenoiseandkira)); // will be used in future version of GIPHT
    Register<DQMHistogramPedeNoise>(CALIBRATION_NAME(pedenoise));
    Register<DQMHistogramPedestalEqualization, DQMHistogramPedeNoise>(CALIBRATION_NAME(calibrationandpedenoise));
    Register<DQMHistogramCalibrationExample>(CALIBRATION_NAME(calibrationexample));
    Register<CBCHistogramPulseShape>(CALIBRATION_NAME(cbcpulseshape));
    Register<DQMHistogramLatencyScan>(CALIBRATION_NAME(otlatency));

    // IT calibrations
    Register<PixelAliveHistograms>(CALIBRATION_NAME(pixelalive));
    Register<PixelAliveHistograms>(CALIBRATION_NAME(noise));
    Register<SCurveHistograms>(CALIBRATION_NAME(scurve));
    Register<GainHistograms>(CALIBRATION_NAME(gain));
    Register<GainOptimizationHistograms>(CALIBRATION_NAME(gainopt));
    Register<ThrEqualizationHistograms>(CALIBRATION_NAME(threqu));
    Register<ThresholdHistograms>(CALIBRATION_NAME(thrmin));
    Register<ThresholdHistograms>(CALIBRATION_NAME(thradj));
    Register<LatencyHistograms>(CALIBRATION_NAME(latency));
    Register<InjectionDelayHistograms>(CALIBRATION_NAME(injdelay));
    Register<ClockDelayHistograms>(CALIBRATION_NAME(clockdelay));
    Register<PhysicsHistograms>(CALIBRATION_NAME(physics));
    Register<PSPhysicsHistograms>(CALIBRATION_NAME(psphysics));
    Register<Physics2SHistograms>(CALIBRATION_NAME(physics2s));
    Register<DataTransmissionTestGraphs>(CALIBRATION_NAME(datatrtest));
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
