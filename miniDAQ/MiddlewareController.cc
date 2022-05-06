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
#include "../tools/CicFEAlignment.h"
#include "../tools/LinkAlignmentOT.h"
#include "../tools/PSPhysics.h"
#include "../tools/Physics2S.h"
#include "../tools/StubBackEndAlignment.h"

#include "../MessageUtils/cpp/ReplyMessage.pb.h"
#include "../MessageUtils/cpp/QueryMessage.pb.h"

using namespace MessageUtils;

//========================================================================================================================
MiddlewareController::MiddlewareController(uint16_t portShift) : TCPServer(PORT_BASE + portShift, 1)
{
}

//========================================================================================================================
MiddlewareController::~MiddlewareController(void) { LOG(INFO) << __PRETTY_FUNCTION__ << " DESTRUCTOR" << RESET; }

//========================================================================================================================
std::string MiddlewareController::interpretMessage(const std::string& buffer)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << " Message received from OTSDAQ: " << buffer << RESET;
    ReplyMessage theReplyMessage;

    QueryMessage theInputQuery;
    theInputQuery.ParseFromString(buffer);
    theInputQuery.PrintDebugString();

    switch (theInputQuery.query_type().type()) 
    {
        case QueryType::INITIALIZE:
        {
            theReplyMessage = fMiddlewareStateMachine.initialize();
            break;
        }
        case QueryType::CONFIGURE:
        {
            ConfigurationMessage theConfigurationMessage;
            theConfigurationMessage.ParseFromString(buffer);
            theReplyMessage = fMiddlewareStateMachine.configure(theConfigurationMessage.data());
            break;
        }
        case QueryType::START:
        {
            StartMessage theStartQuery;
            theStartQuery.ParseFromString(buffer);
            theReplyMessage = fMiddlewareStateMachine.start(theStartQuery.data());
            break;
        }
        case QueryType::STOP:
        {
            theReplyMessage = fMiddlewareStateMachine.stop();
            break;
        }
        case QueryType::HALT:
        {
            theReplyMessage = fMiddlewareStateMachine.halt();
            break;
        }
        case QueryType::PAUSE:
        {
            theReplyMessage = fMiddlewareStateMachine.pause();
            break;
        }
        case QueryType::RESUME:
        {
            theReplyMessage = fMiddlewareStateMachine.resume();
            break;
        }
        case QueryType::ABORT:
        {
            theReplyMessage = fMiddlewareStateMachine.abort();
            break;
        }
        case QueryType::ERROR:
        {
            theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
            theReplyMessage.set_message("Received an Error message from the client");
            break;
        }
        case QueryType::STATUS:
        {
            theReplyMessage = fMiddlewareStateMachine.status();
            break;
        }
        default:
        {
            theReplyMessage.mutable_reply_type()->set_type(ReplyType::ERROR);
            theReplyMessage.set_message("Can't recognize message");
            break;
        }
    }

    std::string replyString;
    theReplyMessage.SerializeToString(&replyString);
    return replyString;

}
