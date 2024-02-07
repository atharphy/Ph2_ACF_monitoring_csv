#include "DQMUtils/DQMCalibrationFactory.h"
#include "DQMUtils/CBCHistogramPulseShape.h"
#include "DQMUtils/DQMHistogramBeamTestCheck.h"
#include "DQMUtils/DQMHistogramCalibrationExample.h"
#include "DQMUtils/DQMHistogramKira.h"
#include "DQMUtils/DQMHistogramLatencyScan.h"
#include "DQMUtils/DQMHistogramPedeNoise.h"
#include "DQMUtils/DQMHistogramPedestalEqualization.h"
#include "DQMUtils/DQMMetadataIT.h"
#include "DQMUtils/DQMMetadataOT.h"
#include "DQMUtils/PSPhysicsHistograms.h"
#include "DQMUtils/Physics2SHistograms.h"
#include "DQMUtils/RD53ClockDelayHistograms.h"
#include "DQMUtils/RD53DataReadbackOptimizationHistograms.h"
#include "DQMUtils/RD53DataTransmissionTestGraphs.h"
#include "DQMUtils/RD53GainHistograms.h"
#include "DQMUtils/RD53GainOptimizationHistograms.h"
#include "DQMUtils/RD53GenericDacDacScanHistograms.h"
#include "DQMUtils/RD53InjectionDelayHistograms.h"
#include "DQMUtils/RD53LatencyHistograms.h"
#include "DQMUtils/RD53PhysicsHistograms.h"
#include "DQMUtils/RD53PixelAliveHistograms.h"
#include "DQMUtils/RD53SCurveHistograms.h"
#include "DQMUtils/RD53ThrEqualizationHistograms.h"
#include "DQMUtils/RD53ThresholdHistograms.h"
#include "DQMUtils/RD53VoltageTuningHistograms.h"

using namespace MessageUtils;

DQMCalibrationFactory::DQMCalibrationFactory()
{
    // Common calibrations
    Register<DQMMetadataOT>("tunelpgbtvref");
    Register<DQMMetadataOT>("configureonly");

    // OT calibrations
    Register<DQMMetadataOT>("OTalignLpGBTinputs");
    Register<DQMMetadataOT, DQMHistogramPedestalEqualization>("calibration");
    Register<DQMMetadataOT, DQMHistogramPedestalEqualization, DQMHistogramBeamTestCheck>("takedata"); // will be used in future version of GIPHT
    Register<DQMMetadataOT, DQMHistogramPedestalEqualization, DQMHistogramKira>("calibrationandkira");
    Register<DQMMetadataOT, DQMHistogramPedestalEqualization, DQMHistogramPedeNoise, DQMHistogramKira>("calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register<DQMMetadataOT, DQMHistogramPedeNoise>("pedenoise");
    Register<DQMMetadataOT, DQMHistogramPedestalEqualization, DQMHistogramPedeNoise>("calibrationandpedenoise");
    Register<DQMMetadataOT, DQMHistogramCalibrationExample>("calibrationexample");
    Register<DQMMetadataOT, CBCHistogramPulseShape>("cbcpulseshape");
    Register<DQMMetadataOT, DQMHistogramLatencyScan>("otlatency");
    Register<DQMMetadataIT, PSPhysicsHistograms>("psphysics");
    Register<DQMMetadataIT, Physics2SHistograms>("physics2s");

    // IT calibrations
    Register<DQMMetadataIT, PixelAliveHistograms>("pixelalive");
    Register<DQMMetadataIT, PixelAliveHistograms>("noise");
    Register<DQMMetadataIT, SCurveHistograms>("scurve");
    Register<DQMMetadataIT, GainHistograms>("gain");
    Register<DQMMetadataIT, GainOptimizationHistograms>("gainopt");
    Register<DQMMetadataIT, ThrEqualizationHistograms>("threqu");
    Register<DQMMetadataIT, ThresholdHistograms>("thrmin");
    Register<DQMMetadataIT, ThresholdHistograms>("thradj");
    Register<DQMMetadataIT, LatencyHistograms>("latency");
    Register<DQMMetadataIT, InjectionDelayHistograms>("injdelay");
    Register<DQMMetadataIT, ClockDelayHistograms>("clockdelay");
    Register<DQMMetadataIT, DataReadbackOptimizationHistograms>("datarbopt");
    Register<DQMMetadataIT, DataTransmissionTestGraphs>("datatrtest");
    Register<DQMMetadataIT, GenericDacDacScanHistograms>("genericdacdac");
    Register<DQMMetadataIT, VoltageTuningHistograms>("voltagetuning");
    Register<DQMMetadataIT, PhysicsHistograms>("physics");
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
