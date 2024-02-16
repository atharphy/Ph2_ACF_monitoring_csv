#include "DQMUtils/DQMCalibrationFactory.h"
#include "DQMUtils/CBCHistogramPulseShape.h"
#include "DQMUtils/DQMHistogramBeamTestCheck.h"
#include "DQMUtils/DQMHistogramCalibrationExample.h"
#include "DQMUtils/DQMHistogramKira.h"
#include "DQMUtils/DQMHistogramLatencyScan.h"
#include "DQMUtils/DQMHistogramOTCICphaseAlignment.h"
#include "DQMUtils/DQMHistogramOTCICwordAlignment.h"
#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#include "DQMUtils/DQMHistogramOTalignLpGBTinputs.h"
#include "DQMUtils/DQMHistogramOTalignStubPackage.h"
#include "DQMUtils/DQMHistogramOTverifyBoardDataWord.h"
#include "DQMUtils/DQMHistogramPedeNoise.h"
#include "DQMUtils/DQMHistogramPedestalEqualization.h"
#include "DQMUtils/DQMMetadataIT.h"
#include "DQMUtils/DQMMetadataOT.h"
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
#include "DQMUtils/DQMHistogramOTverifyCICdataWord.h"

using namespace MessageUtils;

DQMCalibrationFactory::DQMCalibrationFactory()
{
    // Common calibrations
    Register<DQMMetadataOT>("tunelpgbtvref");
    Register<DQMMetadataOT>("configureonly");

    // OT calibrations
    Register<DQMMetadataOT, DQMHistogramPedeNoise>("noiseOT");
    Register<DQMMetadataOT>("vtrxoff");
    Register<DQMMetadataOT, DQMHistogramOTalignLpGBTinputs>("OTalignLpGBTinputs");
    Register<DQMMetadataOT, DQMHistogramOTalignBoardDataWord>("OTalignBoardDataWord");
    Register<DQMMetadataOT, DQMHistogramOTverifyBoardDataWord>("OTverifyBoardDataWord");
    Register<DQMMetadataOT, DQMHistogramOTalignStubPackage>("OTalignStubPackage");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord>("alignment");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedestalEqualization>("calibration");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedestalEqualization,
             DQMHistogramBeamTestCheck>("takedata"); // will be used in future version of GIPHT
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedestalEqualization,
             DQMHistogramKira>("calibrationandkira");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedestalEqualization,
             DQMHistogramPedeNoise,
             DQMHistogramKira>("calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedeNoise>("pedenoise");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramPedestalEqualization,
             DQMHistogramPedeNoise>("calibrationandpedenoise");
    Register<DQMMetadataOT, DQMHistogramCalibrationExample>("calibrationexample");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             CBCHistogramPulseShape>("cbcpulseshape");
    Register<DQMMetadataOT,
             DQMHistogramOTalignLpGBTinputs,
             DQMHistogramOTalignBoardDataWord,
             DQMHistogramOTverifyBoardDataWord,
             DQMHistogramOTalignStubPackage,
             DQMHistogramOTCICphaseAlignment,
             DQMHistogramOTCICwordAlignment,
             DQMHistogramOTverifyCICdataWord,
             DQMHistogramLatencyScan>("otlatency");

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
    Register<DQMMetadataIT, PhysicsHistograms>("physics");
    Register<DQMMetadataIT, PSPhysicsHistograms>("psphysics");
    Register<DQMMetadataIT, Physics2SHistograms>("physics2s");
    Register<DQMMetadataIT, DataTransmissionTestGraphs>("datatrtest");
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
