#include <iostream>
#include <stdio.h>
#include <unistd.h>

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
#include "../tools/BackEndAlignment.h"

//========================================================================================================================
MiddlewareController::MiddlewareController(int serverPort) : TCPServer(serverPort, 1)
{
    // TCPServer::setReceiveTimeout(1,0);//Doesn't work
}

//========================================================================================================================
MiddlewareController::~MiddlewareController(void) { LOG(INFO) << __PRETTY_FUNCTION__ << " DESTRUCTOR" << RESET; }

//========================================================================================================================
std::string MiddlewareController::interpretMessage(const std::string& buffer)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << " Message received from OTSDAQ: " << buffer << RESET;

    if(buffer == "Initialize") // Changing the status changes the mode in threadMain (BBC) function
    { return "InitializeDone"; }
    else if(buffer.substr(0, 5) == "Start") // Changing the status changes the mode in threadMain (BBC) function
    {
        currentRun_ = getVariableValue("RunNumber", buffer);
        // running(stoi(currentRun_))
        // runningFuture_ = std::async(std::launch::async, &MiddlewareController::running, this, stoi(currentRun_));
        theSystemController_->Start(stoi(currentRun_));
        return "StartDone";
    }
    else if(buffer.substr(0, 7) == "Status?") // Changing the status changes the mode in threadMain (BBC) function
    {
        return theSystemController_->GetRunningStatus() ? "Done" : "Running";
    }
    else if(buffer.substr(0, 4) == "Stop")
    {
        theSystemController_->Stop();
        // while(runningFuture_.wait_for(std::chrono::milliseconds(500)) != std::future_status::ready) std::cout << _PRETTY_FUNCTION_ << "...still running" << std::endl;
        LOG(INFO) << "Run " << currentRun_ << " stopped" << RESET;
        return "StopDone";
    }
    else if(buffer.substr(0, 4) == "Halt")
    {
        theSystemController_->Stop();
        theSystemController_->Destroy();
        theSystemController_ = nullptr;
        LOG(INFO) << "Run " << currentRun_ << " halted" << RESET;
        return "HaltDone";
    }
    else if(buffer == "Pause")
    {
        LOG(INFO) << BOLDBLUE << "Paused" << RESET;
        return "PauseDone";
    }
    else if(buffer == "Resume")
    {
        LOG(INFO) << BOLDBLUE << "Resumed" << RESET;
        return "ResumeDone";
    }
    else if(buffer.substr(0, 9) == "Configure")
    {
        LOG(INFO) << BOLDBLUE << "Configuring" << RESET;

        if(getVariableValue("Calibration", buffer) == "calibration")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, PedestalEqualization>;
        else if(getVariableValue("Calibration", buffer) == "pedenoise")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, PedeNoise>;
        else if(getVariableValue("Calibration", buffer) == "calibrationandpedenoise")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, PedestalEqualization, PedeNoise>;
        else if(getVariableValue("Calibration", buffer) == "calibrationexample")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, CalibrationExample>;
        else if(getVariableValue("Calibration", buffer) == "cbcPulseShape")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, CBCPulseShape>;
        else if(getVariableValue("Calibration", buffer) == "OTLatency")
            theSystemController_ = new CombinedCalibration<BackEndAlignment, LatencyScan>;

        else if(getVariableValue("Calibration", buffer) == "pixelalive")
            theSystemController_ = new CombinedCalibration<PixelAlive>;
        else if(getVariableValue("Calibration", buffer) == "noise")
            theSystemController_ = new CombinedCalibration<PixelAlive>;
        else if(getVariableValue("Calibration", buffer) == "scurve")
            theSystemController_ = new CombinedCalibration<SCurve>;
        else if(getVariableValue("Calibration", buffer) == "gain")
            theSystemController_ = new CombinedCalibration<Gain>;
        else if(getVariableValue("Calibration", buffer) == "gainopt")
            theSystemController_ = new CombinedCalibration<GainOptimization>;
        else if(getVariableValue("Calibration", buffer) == "threqu")
            theSystemController_ = new CombinedCalibration<ThrEqualization>;
        else if(getVariableValue("Calibration", buffer) == "thrmin")
            theSystemController_ = new CombinedCalibration<ThrMinimization>;
        else if(getVariableValue("Calibration", buffer) == "thradj")
            theSystemController_ = new CombinedCalibration<ThrAdjustment>;
        else if(getVariableValue("Calibration", buffer) == "latency")
            theSystemController_ = new CombinedCalibration<Latency>;
        else if(getVariableValue("Calibration", buffer) == "injdelay")
            theSystemController_ = new CombinedCalibration<InjectionDelay>;
        else if(getVariableValue("Calibration", buffer) == "clockdelay")
            theSystemController_ = new CombinedCalibration<ClockDelay>;
        else if(getVariableValue("Calibration", buffer) == "physics")
            theSystemController_ = new Physics;
        else if(getVariableValue("Calibration", buffer) == "datatrtest")
            theSystemController_ = new CombinedCalibration<DataTransmissionTest>;

        else
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Calibration type " << getVariableValue("Calibration", buffer) << " not found, Aborting" << RESET;
            abort();
        }

        LOG(INFO) << BOLDBLUE << "SystemController created" << RESET;
        theSystemController_->Configure(getVariableValue("ConfigurationFile", buffer), true);
        return "ConfigureDone";
    }
    else if(buffer.substr(0, 6) == "Error:")
    {
        if(buffer == "Error: Connection closed") LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << buffer << ". Closing client server connection!" << RESET;
        return "";
    }
    else
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Can't recognige message: " << buffer << ". Aborting..." << RESET;
        abort();
    }

    if(running_ || paused_) // We go through here after start and resume or pause: sending back current status
    { LOG(INFO) << BOLDBLUE << "Getting time and status here" << RESET; }

    return "Didn't understand the message!";
}
