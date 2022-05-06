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

using namespace MessageUtils;

MiddlewareStateMachine::MiddlewareStateMachine(){}

MiddlewareStateMachine::~MiddlewareStateMachine()
{
    delete fTheTool;
}

ReplyMessage MiddlewareStateMachine::initialize()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::configure(const MessageUtils::ConfigurationInfo& configurationInfo)
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    std::string calibrationName = configurationInfo.calibration_name();

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
        std::string calibrationFile = configurationInfo.configuration_file();
        fTheTool->Configure(calibrationFile, true);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        delete fTheTool;
        std::string errorString = std::string("Error: ") + e.what();
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
        return theReplyMessage;
    }
    
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::start(const StartInfo& startInfo){
    currentRun_ = startInfo.run_number();
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);
    try
    {
        fTheTool->Start(currentRun_);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::stop()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    try
    {
        fTheTool->Stop();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    LOG(INFO) << "Run " << currentRun_ << " stopped" << RESET;
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::halt()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    try
    {
        fTheTool->Stop();
        fTheTool->Destroy();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    LOG(INFO) << "Halted!" << RESET;
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::pause()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    try
    {
        fTheTool->Pause();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    LOG(INFO) << "Paused!" << RESET;
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::resume()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    try
    {
        fTheTool->Resume();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    LOG(INFO) << "Resumed!" << RESET;
    return theReplyMessage;
}

ReplyMessage MiddlewareStateMachine::abort()
{
    ReplyMessage theReplyMessage;
    theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    try
    {
        fTheTool->Destroy();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    
    LOG(INFO) << "Aborted!" << RESET;
    return theReplyMessage;
}


ReplyMessage MiddlewareStateMachine::status()
{
    ReplyMessage theReplyMessage;

    try
    {
        theReplyMessage.mutable_reply_type()->set_type(fTheTool->GetRunningStatus() ? ReplyType::SUCCESS : ReplyType::RUNNING);
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    }
    return theReplyMessage;
}
