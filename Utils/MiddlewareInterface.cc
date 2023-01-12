#include "Utils/MiddlewareInterface.h"
#include "MessageUtils/cpp/ReplyMessage.pb.h"
#include <iostream>

using namespace MessageUtils;

//========================================================================================================================
MiddlewareInterface::MiddlewareInterface(std::string serverIP, int serverPort) : TCPClient(serverIP, serverPort) {}

//========================================================================================================================
MiddlewareInterface::~MiddlewareInterface(void) { std::cout << __PRETTY_FUNCTION__ << "DESTRUCTOR!" << std::endl; }

//========================================================================================================================
std::string MiddlewareInterface::sendCommand(const std::string& command)
{
    try
    {
        return TCPClient::sendAndReceivePacket(command);
    }
    catch(const std::exception& e)
    {
        std::cout << __PRETTY_FUNCTION__ << "Error: " << e.what() << " Need to take some actions here!" << std::endl;
        return "";
    }
}

//========================================================================================================================
void MiddlewareInterface::initialize(void)
{
    if(!TCPClient::connect())
    {
        std::cout << __PRETTY_FUNCTION__ << "ERROR CAN'T CONNECT TO SERVER!" << std::endl;
        abort();
    }

    QueryMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::INITIALIZE);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);
    std::cout << __PRETTY_FUNCTION__ << "DONE WITH Initialize-" << readBuffer << "-" << std::endl;
}

//========================================================================================================================
void MiddlewareInterface::configure(std::string const& calibrationName, std::string const& configurationFilePath)
{
    const google::protobuf::EnumDescriptor* fCalibrationEnumDescriptor = MessageUtils::CalibrationList_CalibrationNameEnum_descriptor();
    const auto theCalibrationEnum = static_cast<MessageUtils::CalibrationList::CalibrationNameEnum>(fCalibrationEnumDescriptor->FindValueByName(calibrationName)->number());
    ConfigurationMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::CONFIGURE);
    theQuery.mutable_data()->mutable_calibration()->set_calibration_name(theCalibrationEnum);
    theQuery.mutable_data()->set_configuration_file(configurationFilePath);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);
    std::cout << __PRETTY_FUNCTION__ << "DONE WITH Configure-" << readBuffer << "-" << std::endl;
}

//========================================================================================================================
void MiddlewareInterface::halt(void)
{
    std::cout << __PRETTY_FUNCTION__ << "Sending Halt!" << std::endl;

    QueryMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::HALT);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);

    std::cout << __PRETTY_FUNCTION__ << "DONE WITH Halt-" << readBuffer << "-" << std::endl;
}

//========================================================================================================================
void MiddlewareInterface::pause(void) {}

//========================================================================================================================
void MiddlewareInterface::resume(void) {}

//========================================================================================================================
void MiddlewareInterface::start(int runNumber)
{
    StartMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::START);
    theQuery.mutable_data()->set_run_number(runNumber);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);
    std::cout << __PRETTY_FUNCTION__ << "DONE WITH Start-" << readBuffer << "-" << std::endl;
}

//========================================================================================================================
std::string MiddlewareInterface::status()
{
    QueryMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::STATUS);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);

    ReplyMessage theStatus;
    theStatus.ParseFromString(readBuffer);

    std::string status;
    switch(theStatus.reply_type().type())
    {
    case ReplyType::RUNNING:
    {
        status = "Running";
        break;
    }
    case ReplyType::SUCCESS:
    {
        status = "Done";
        break;
    }
    case ReplyType::ERROR:
    {
        status = "Error";
        break;
    }

    default: status = "Unknown"; break;
    }

    std::cout << __PRETTY_FUNCTION__ << "Status: " << status << std::endl;
    return status;
}
//========================================================================================================================
void MiddlewareInterface::stop(void)
{
    std::cout << __PRETTY_FUNCTION__ << "Sending Stop!" << std::endl;
    QueryMessage theQuery;
    theQuery.mutable_query_type()->set_type(QueryType::STOP);
    std::string theCommandString;
    theQuery.SerializeToString(&theCommandString);
    std::string readBuffer = sendCommand(theCommandString);

    std::cout << __PRETTY_FUNCTION__ << "DONE WITH Stop-" << readBuffer << "-" << std::endl;
}
