import sys
import os
import time

sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR'))

import lib.Ph2_ACF_PythonInterface as Ph2_ACF
import MessageUtils.python.QueryMessage_pb2 as Query
import MessageUtils.python.ReplyMessage_pb2 as Reply

Ph2_ACF_controller = Ph2_ACF.MiddlewareMessageHandler()

class StateMachine(object):
    def __init__(self, configuration_file, calibration_name):
        self.configuration_file_ = configuration_file;
        self.calibration_name_ = calibration_name;
        self.status_ = "INITIAL"
        self.calibrationResult = "SUCCESS"

    def switch(self, status):
        default = "Incorrect state"
        return getattr(self, 'state_' + str(status), lambda: default)()

    def parseReply(self, replyBuffer):
        theReply = Reply.ReplyMessage()
        theReply.ParseFromString(replyBuffer.encode())
        if(theReply.reply_type.type == Reply.ReplyType.ERROR):
            print(theReply.message)
        return theReply.reply_type.type

    def state_INITIAL(self):
        initializeMessage = Query.QueryMessage()
        initializeMessage.query_type.type = Query.QueryType.INITIALIZE
        stringMessage = initializeMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.initialize(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "HALTED"

    def state_HALTED(self):
        configureMessage = Query.ConfigurationMessage()
        configureMessage.query_type.type = Query.QueryType.CONFIGURE
        configureMessage.data.calibration_name = Query.ConfigurationInfo.CALIBRATIONANDPEDENOISE
        configureMessage.data.configuration_file = self.configuration_file_
        stringMessage = configureMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.configure(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "CONFIGURED"

    def state_CONFIGURED(self):
        startMessage = Query.StartMessage()
        startMessage.query_type.type = Query.QueryType.START
        startMessage.data.run_number = 999
        stringMessage = startMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.start(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "RUNNING"

    def state_RUNNING(self):
        while True:
            statusMessage = Query.QueryMessage()
            statusMessage.query_type.type = Query.QueryType.STATUS
            stringMessage = statusMessage.SerializeToString()
            replyBuffer = Ph2_ACF_controller.status(stringMessage)
            type = self.parseReply(replyBuffer)
            if type == Reply.ReplyType.ERROR:
                self.status_ = "ERROR"
                return
            elif type == Reply.ReplyType.RUNNING:
                time.sleep(0.5)
                continue
            elif type == Reply.ReplyType.SUCCESS:
                break
            else:
                print("Unrecognized status from Ph2_ACF")
                self.status_ = "ERROR"
                return
        self.status_ = "STOPPED"
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"

    def state_STOPPED(self):
        haltMessage = Query.QueryMessage()
        haltMessage.query_type.type = Query.QueryType.HALT
        stringMessage = haltMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.halt(stringMessage)
        self.status_ = "DONE"

    def state_ERROR(self):
        print("An error occurred")
        self.status_ = "DONE"
        self.calibrationResult = "FAILED"

    def runCalibration(self):
        while(self.status_ != "DONE"):
            self.switch(self.status_)
        return self.calibrationResult
        

