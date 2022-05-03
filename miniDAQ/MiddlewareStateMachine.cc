#include "../miniDAQ/MiddlewareStateMachine.h"
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


MiddlewareStateMachine::MiddlewareStateMachine(){}

MiddlewareStateMachine::~MiddlewareStateMachine()
{
    delete fTheTool;
}

Message MiddlewareStateMachine::initialize()
{
    Message theMessage;
    theMessage.setMessage<std::string>("InitializeDone");
    return theMessage;
}

Message MiddlewareStateMachine::configure(const ConfigurationMessage& configurationMessage)
{
    Message theMessage;

    std::string calibrationName = configurationMessage.getCalibrationName();

    if(calibrationName == "calibration")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, PedestalEqualization>;
    else if(calibrationName == "pedenoise")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, PedeNoise>;
    else if(calibrationName == "calibrationandpedenoise")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, PedestalEqualization, PedeNoise>;
    else if(calibrationName == "calibrationexample")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, CalibrationExample>;
    else if(calibrationName == "cbcPulseShape")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, CBCPulseShape>;
    else if(calibrationName == "OTLatency")
        fTheTool = new CombinedCalibration<LinkAlignmentOT, CicFEAlignment, LatencyScan>;

    else if(calibrationName == "pixelalive")
        fTheTool = new CombinedCalibration<PixelAlive>;
    else if(calibrationName == "noise")
        fTheTool = new CombinedCalibration<PixelAlive>;
    else if(calibrationName == "scurve")
        fTheTool = new CombinedCalibration<SCurve>;
    else if(calibrationName == "gain")
        fTheTool = new CombinedCalibration<Gain>;
    else if(calibrationName == "gainopt")
        fTheTool = new CombinedCalibration<GainOptimization>;
    else if(calibrationName == "threqu")
        fTheTool = new CombinedCalibration<ThrEqualization>;
    else if(calibrationName == "thrmin")
        fTheTool = new CombinedCalibration<ThrMinimization>;
    else if(calibrationName == "thradj")
        fTheTool = new CombinedCalibration<ThrAdjustment>;
    else if(calibrationName == "latency")
        fTheTool = new CombinedCalibration<Latency>;
    else if(calibrationName == "injdelay")
        fTheTool = new CombinedCalibration<InjectionDelay>;
    else if(calibrationName == "clockdelay")
        fTheTool = new CombinedCalibration<ClockDelay>;
    else if(calibrationName == "physics")
        fTheTool = new Physics;
    else if(calibrationName == "psphysics")
        fTheTool = new PSPhysics;
    else if(calibrationName == "2sphysics")
        fTheTool = new Physics2S;
    else if(calibrationName == "datatrtest")
        fTheTool = new CombinedCalibration<DataTransmissionTest>;

    else
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Calibration type " << calibrationName << " not found, Aborting" << RESET;
        abort();
    }

    LOG(INFO) << BOLDBLUE << "Tool created" << RESET;
    try
    {
        std::string calibrationFile = configurationMessage.getCalibrationFile();
        fTheTool->Configure(calibrationFile, true);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        delete fTheTool;
        std::string errorString = std::string("Error: ") + e.what();
        theMessage.setError(errorString);
        return theMessage;
    }
    
    theMessage.setMessage<std::string>("ConfigureDone");

    return Message();
}

Message MiddlewareStateMachine::start(){return Message();}

Message MiddlewareStateMachine::stop(){return Message();}

Message MiddlewareStateMachine::halt(){return Message();}

Message MiddlewareStateMachine::pause(){return Message();}

Message MiddlewareStateMachine::resume(){return Message();}

