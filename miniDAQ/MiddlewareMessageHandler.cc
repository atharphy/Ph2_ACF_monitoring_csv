#include "../miniDAQ/MiddlewareMessageHandler.h"
#include "../miniDAQ/CombinedCalibrationFactory.h"

using namespace MessageUtils;

MiddlewareMessageHandler::MiddlewareMessageHandler(){}

MiddlewareMessageHandler::~MiddlewareMessageHandler()
{
}

std::string MiddlewareMessageHandler::initialize(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("initialize", &MiddlewareStateMachine::initialize);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::configure(const std::string& message)
{
    ConfigurationMessage theConfigureMessage;
    theConfigureMessage.ParseFromString(message);

    CombinedCalibrationFactory theCombinedCalibrationFactory;
    const std::string calibrationName = theCombinedCalibrationFactory.getCalibrationName(theConfigureMessage.data().calibration_name());
    const std::string configurationFile = theConfigureMessage.data().configuration_file();

    ReplyMessage theReplyMessage = tryCatchWrapper("configure", &MiddlewareStateMachine::configure, calibrationName, configurationFile);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::start(const std::string& message)
{
    StartMessage theStartMessage;
    theStartMessage.ParseFromString(message);
    int runNumber = theStartMessage.data().run_number();

    ReplyMessage theReplyMessage = tryCatchWrapper("start", &MiddlewareStateMachine::start, runNumber);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::stop(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("stop", &MiddlewareStateMachine::stop);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::halt(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("halt", &MiddlewareStateMachine::halt);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::pause(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("pause", &MiddlewareStateMachine::pause);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::resume(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("resume", &MiddlewareStateMachine::resume);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::abort(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper("abort", &MiddlewareStateMachine::abort);
    return serializeMessage(theReplyMessage);
}


std::string MiddlewareMessageHandler::status(const std::string& message)
{
    ReplyMessage theReplyMessage;

    try
    {
        MiddlewareStateMachine::Status theStatus = fMiddlewareStateMachine.status();
        if     (theStatus == MiddlewareStateMachine::Status::DONE) theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);
        else if(theStatus == MiddlewareStateMachine::Status::RUNNING) theReplyMessage.mutable_reply_type()->set_type(ReplyType::RUNNING);
    }
    catch(const std::exception& theException)
    {
        catchFunction(theReplyMessage, theException, "status");
    }
    return serializeMessage(theReplyMessage);
}


ReplyMessage MiddlewareMessageHandler::catchFunction(ReplyMessage& inputReplayMessage, const std::exception& theException, const std::string& currentFunction)
{
    std::string theExceptionMessage = theException.what();
    std::string outputMessage = "Exception thrown during SM step " + currentFunction + " - catched exception message: crashed because it sucks :)" + theExceptionMessage;
    inputReplayMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    inputReplayMessage.set_message(outputMessage.c_str());
    return inputReplayMessage;
}
