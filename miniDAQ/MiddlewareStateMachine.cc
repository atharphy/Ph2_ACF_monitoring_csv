#include "../miniDAQ/MiddlewareStateMachine.h"
#include "../miniDAQ/CombinedCalibrationFactory.h"
#include "../tools/Tool.h"


MiddlewareStateMachine::MiddlewareStateMachine()
{
}

MiddlewareStateMachine::~MiddlewareStateMachine()
{
    delete fTheTool;
    fTheTool = nullptr;
}

void MiddlewareStateMachine::initialize()
{
    LOG(INFO) << "Initialized" << RESET;
}

void MiddlewareStateMachine::configure(const std::string& calibrationName, const std::string& configurationFile)
{
    CombinedCalibrationFactory theCombinedCalibrationFactory;
    fTheTool = theCombinedCalibrationFactory.CreateCombinedCalibration(calibrationName);

    LOG(INFO) << BOLDBLUE << "Tool created" << RESET;

    fTheTool->Configure(configurationFile, true);
    
    LOG(INFO) << "Configured" << RESET;

    return;
}

void MiddlewareStateMachine::start(int runNumber)
{
    currentRun_ = runNumber;
    fTheTool->Start(currentRun_);
    LOG(INFO) << "Run " << currentRun_ << " started" << RESET;
    return;
}

void MiddlewareStateMachine::stop()
{
    fTheTool->Stop();
    LOG(INFO) << "Run " << currentRun_ << " stopped" << RESET;
    return;
}

void MiddlewareStateMachine::halt()
{
    try
    {
        stop();
    }
    catch(const std::exception& e)
    {
        LOG(WARNING) << "Could not stop the run, going to call Destroy anyway" << RESET;
    }
    
    fTheTool->Destroy();
    LOG(INFO) << "Halted" << RESET;
}

void MiddlewareStateMachine::pause()
{
    fTheTool->Pause();
    LOG(INFO) << "Paused" << RESET;
}

void MiddlewareStateMachine::resume()
{
    fTheTool->Resume();
    LOG(INFO) << "Resumed" << RESET;
}

void MiddlewareStateMachine::abort()
{
    fTheTool->Destroy();
    LOG(INFO) << "Aborted" << RESET;
}

MiddlewareStateMachine::Status MiddlewareStateMachine::status()
{
    return fTheTool->GetRunningStatus() ? Status::DONE : Status::RUNNING;
}
