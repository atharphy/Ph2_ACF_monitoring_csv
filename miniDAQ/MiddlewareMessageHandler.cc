#include "../miniDAQ/MiddlewareMessageHandler.h"
#include "../MessageUtils/cpp/Common.pb.h"
#include "../miniDAQ/CombinedCalibrationFactory.h"

using namespace MessageUtils;

MiddlewareMessageHandler::MiddlewareMessageHandler() {}

MiddlewareMessageHandler::~MiddlewareMessageHandler() {}

std::string MiddlewareMessageHandler::initialize(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::initialize);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::configure(const std::string& message)
{
    ConfigurationMessage theConfigureMessage;
    theConfigureMessage.ParseFromString(message);

    const std::string calibrationName   = getAvailableCalibrationMap().at(theConfigureMessage.data().calibration().calibration_name());
    const std::string configurationFile = theConfigureMessage.data().configuration_file();

    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::configure, calibrationName, configurationFile);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::start(const std::string& message)
{
    StartMessage theStartMessage;
    theStartMessage.ParseFromString(message);
    int runNumber = theStartMessage.data().run_number();

    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::start, runNumber);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::stop(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::stop);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::halt(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::halt);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::pause(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::pause);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::resume(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::resume);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::abort(const std::string& message)
{
    ReplyMessage theReplyMessage = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::abort);
    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::status(const std::string& message)
{
    ReplyMessage theReplyMessage;

    try
    {
        MiddlewareStateMachine::Status theStatus = fMiddlewareStateMachine.status();
        if(theStatus == MiddlewareStateMachine::Status::DONE)
            theReplyMessage.mutable_reply_type()->set_type(ReplyType::SUCCESS);
        else if(theStatus == MiddlewareStateMachine::Status::RUNNING)
            theReplyMessage.mutable_reply_type()->set_type(ReplyType::RUNNING);
    }
    catch(const std::exception& theException)
    {
        catchFunction(theReplyMessage, theException, __PRETTY_FUNCTION__);
    }
    return serializeMessage(theReplyMessage);
}

void MiddlewareMessageHandler::catchFunction(ReplyMessage& inputReplayMessage, const std::exception& theException, const std::string& currentFunction)
{
    std::string theExceptionMessage = theException.what();
    std::string outputMessage       = "Exception thrown during SM step " + currentFunction + " - catched exception message: " + theExceptionMessage;
    inputReplayMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
    inputReplayMessage.set_message(outputMessage.c_str());
}

std::string MiddlewareMessageHandler::firmwareAction(const std::string& message)
{
    ReplyMessage         theReplyMessage;
    FirmwareQueryMessage theFirmwareMessage;
    theFirmwareMessage.ParseFromString(message);
    const std::string& configurationFile = theFirmwareMessage.configuration_file();
    uint32_t           boardIdRaw        = theFirmwareMessage.board_id();
    if(boardIdRaw > 0xFFFF)
    {
        std::runtime_error theError("Board Id has to be contained in 16 bits");
        catchFunction(theReplyMessage, theError, __PRETTY_FUNCTION__);
        return serializeMessage(theReplyMessage);
    }
    uint16_t boardId = boardIdRaw;

    switch(theFirmwareMessage.action())
    {
    case FirmwareQueryMessage::LIST:
    {
        try
        {
            FirmwareReplyMessage     theFirmwareListReply;
            std::vector<std::string> firmwareList = fMiddlewareStateMachine.getFirmwareList(configurationFile, boardId);
            theFirmwareListReply.mutable_reply_type()->set_type(ReplyType::SUCCESS);

            for(const auto& firmware: firmwareList) theFirmwareListReply.add_firmware_name(firmware);

            return serializeMessage(theFirmwareListReply);
        }
        catch(const std::exception& theException)
        {
            catchFunction(theReplyMessage, theException, __PRETTY_FUNCTION__);
        }
        break;
    }

    case FirmwareQueryMessage::LOAD:
    {
        const std::string& firmwareName = theFirmwareMessage.firmware_name();
        theReplyMessage                 = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::loadFirmwareInFPGA, configurationFile, firmwareName, boardId);
        break;
    }

    case FirmwareQueryMessage::UPLOAD:
    {
        const std::string& firmwareName = theFirmwareMessage.firmware_name();
        const std::string& fileName     = theFirmwareMessage.file_name();
        theReplyMessage                 = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::uploadFirmwareOnSDcard, configurationFile, firmwareName, fileName, boardId);
        break;
    }

    case FirmwareQueryMessage::DOWNLOAD:
    {
        const std::string& firmwareName = theFirmwareMessage.firmware_name();
        const std::string& fileName     = theFirmwareMessage.file_name();
        theReplyMessage                 = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::downloadFirmwareFromSDcard, configurationFile, firmwareName, fileName, boardId);
        break;
    }

    case FirmwareQueryMessage::DELETE:
    {
        const std::string& firmwareName = theFirmwareMessage.firmware_name();
        theReplyMessage                 = tryCatchWrapper(__PRETTY_FUNCTION__, &MiddlewareStateMachine::deleteFirmwareFromSDcard, configurationFile, firmwareName, boardId);
        break;
    }

    default:
    {
        std::runtime_error theError("MiddlewareMessageHandler::firmwareAction not able to identity action");
        catchFunction(theReplyMessage, theError, __PRETTY_FUNCTION__);
        break;
    }
    }

    return serializeMessage(theReplyMessage);
}

std::string MiddlewareMessageHandler::calibrationList(const std::string& message)
{
    const std::map<CalibrationList::CalibrationNameEnum, std::string> calibrationMap = getAvailableCalibrationMap();

    CalibrationListReplyMessage theCalibrationList;
    theCalibrationList.mutable_reply_type()->set_type(ReplyType::SUCCESS);

    for(const auto& theCalibrationPair: calibrationMap)
    {
        auto* theCalibrationAndName = theCalibrationList.add_calibration();
        theCalibrationAndName->mutable_calibration_tag()->set_calibration_name(theCalibrationPair.first);
        theCalibrationAndName->set_calibration_name(theCalibrationPair.second);
    }

    return serializeMessage(theCalibrationList);
}
