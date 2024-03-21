#include "miniDAQ/CombinedCalibrationFactory.h"

#include "MiddlewareController.h"
#include "tools/BeamTestCheck.h"
#include "tools/CBCPulseShape.h"
#include "tools/CalibrationExample.h"
#include "tools/CombinedCalibration.h"
#include "tools/ConfigureOnly.h"
#include "tools/KIRA.h"
#include "tools/LatencyScan.h"
#include "tools/OTCICphaseAlignment.h"
#include "tools/OTCICwordAlignment.h"
#include "tools/OTCMNoise.h"
#include "tools/OTTemperature.h"
#include "tools/OTVTRXLightOff.h"
#include "tools/OTalignBoardDataWord.h"
#include "tools/OTalignLpGBTinputs.h"
#include "tools/OTalignStubPackage.h"
#include "tools/OTverifyBoardDataWord.h"
#include "tools/OTverifyCICdataWord.h"
#include "tools/OTverifyMPASSAdataWord.h"
#include "tools/PSPhysics.h"
#include "tools/PedeNoise.h"
#include "tools/PedestalEqualization.h"
#include "tools/Physics2S.h"
#include "tools/RD53ClockDelay.h"
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
#include "tools/OTinjectionDelayOptimization.h"
#include "tools/ECVLinkAlignmentOT.h"

using namespace MessageUtils;

CombinedCalibrationFactory::CombinedCalibrationFactory()
{
    // Common calibrations
    Register<TuneLpGBTVref>("Common", "tunelpgbtvref");
    Register<ConfigureOnly>("Common", "configureonly");

    // OT calibrations
    Register<PedeNoise>("Outer Tracker", "noiseOT");
    Register<OTVTRXLightOff>("Outer Tracker", "vtrxoff");
    Register<ECVLinkAlignmentOT>("Outer Tracker", "ecv");
    Register<OTalignLpGBTinputs>("Outer Tracker", "OTalignLpGBTinputs");
    Register<OTalignBoardDataWord>("Outer Tracker", "OTalignBoardDataWord");
    Register<OTverifyBoardDataWord>("Outer Tracker", "OTverifyBoardDataWord");
    Register<OTalignStubPackage>("Outer Tracker", "OTalignStubPackage");
    Register<OTalignLpGBTinputs, OTalignBoardDataWord, OTverifyBoardDataWord, OTalignStubPackage, OTCICphaseAlignment, OTCICwordAlignment, OTverifyCICdataWord, OTverifyMPASSAdataWord>("Outer Tracker",
                                                                                                                                                                                        "alignment");
    Register<OTalignLpGBTinputs, OTalignBoardDataWord, OTverifyBoardDataWord, OTalignStubPackage, OTCICphaseAlignment, OTCICwordAlignment, OTverifyCICdataWord, OTverifyMPASSAdataWord, OTinjectionDelayOptimization>("Outer Tracker", "injectionDelayOptimization");
                                      
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedestalEqualization>("Outer Tracker", "calibration");
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedestalEqualization,
             BeamTestCheck>("Outer Tracker", "takedata"); // will be used in future version of GIPHT
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedestalEqualization,
             KIRA>("Outer Tracker", "calibrationandkira");
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedestalEqualization,
             PedeNoise,
             KIRA>("Outer Tracker", "calibrationandpedenoiseandkira"); // will be used in future version of GIPHT
    Register< OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedeNoise>("Outer Tracker", "pedenoise");
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             PedestalEqualization,
             PedeNoise>("Outer Tracker", "calibrationandpedenoise");
    Register<OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             OTverifyCICdataWord,
             OTverifyMPASSAdataWord,
             CalibrationExample>("Outer Tracker", "calibrationexample");
    Register<OTalignLpGBTinputs, OTalignBoardDataWord, OTverifyBoardDataWord, OTalignStubPackage, OTCICphaseAlignment, OTCICwordAlignment, OTverifyCICdataWord, OTverifyMPASSAdataWord, LatencyScan>(
        "Outer Tracker", "otlatency");
    Register<TuneLpGBTVref,
             OTTemperature,
             OTalignLpGBTinputs,
             OTalignBoardDataWord,
             OTverifyBoardDataWord,
             OTalignStubPackage,
             OTCICphaseAlignment,
             OTCICwordAlignment,
             PedestalEqualization,
             PedeNoise,
             TuneLpGBTVref,
             OTCMNoise,
             OTTemperature>("Outer Tracker", "cmNoise");

    // 2S specific calibrations
    Register<OTalignLpGBTinputs, OTalignBoardDataWord, OTverifyBoardDataWord, OTalignStubPackage, OTCICphaseAlignment, OTCICwordAlignment, OTverifyCICdataWord, OTverifyMPASSAdataWord, CBCPulseShape>(
        "2S Module", "cbcpulseshape");
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
}

CombinedCalibrationFactory::~CombinedCalibrationFactory()
{
    for(auto& calibrationListPerHardware: fCalibrationMap)
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
