import sys
import os
import time

sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR'))
sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR') + "/MessageUtils/python/")

import lib.Ph2_ACF_PythonInterface as Ph2_ACF
import Common_pb2 as Common
import QueryMessage_pb2 as Query
import ReplyMessage_pb2 as Reply

Ph2_ACF_controller = Ph2_ACF.MiddlewareMessageHandler()

class StateMachine(object):
    def __init__(self):
        self.configurationFile_ = ""
        self.calibrationName_ = ""
        self.runNumber_ = 0;
        self.status_ = "INITIAL"
        self.calibrationResult_ = "SUCCESS"
        self.errorMessage_ = ""
        self.mapOfCalibrations_ = {}

    def setConfigurationFile(self, configurationFile):
        self.configurationFile_ = configurationFile

    def setCalibrationName(self, calibrationName):
        self.calibrationName_ = calibrationName

    def setRunNumber(self, runNumber):
        self.runNumber_ = runNumber

    def resetStatus(self):
        self.calibrationResult_ = "SUCCESS"
        self.errorMessage_ = ""

    def switch(self, status):
        default = "Incorrect state"
        return getattr(self, 'state_' + str(status), lambda: default)()

    def parseReply(self, replyBuffer):
        theReply = Reply.ReplyMessage()
        theReply.ParseFromString(replyBuffer)
        if(theReply.reply_type.type == Reply.ReplyType.ERROR):
            self.errorMessage_ = theReply.message
        return theReply.reply_type.type

    def state_INITIAL(self):
        self.resetStatus()
        initializeMessage = Query.QueryMessage()
        initializeMessage.query_type.type = Query.QueryType.INITIALIZE
        stringMessage = initializeMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.initialize(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "HALTED"

    def state_HALTED(self):
        if not self.mapOfCalibrations_:
            self.queryCalibrationList()
        self.resetStatus()
        configureMessage = Query.ConfigurationMessage()
        configureMessage.query_type.type = Query.QueryType.CONFIGURE
        configureMessage.data.calibration.calibration_name = self.mapOfCalibrations_[self.calibrationName_]
        configureMessage.data.configuration_file = self.configurationFile_
        stringMessage = configureMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.configure(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "CONFIGURED"

    def state_CONFIGURED(self):
        self.resetStatus()
        startMessage = Query.StartMessage()
        startMessage.query_type.type = Query.QueryType.START
        startMessage.data.run_number = self.runNumber_
        stringMessage = startMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.start(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"
            return
        self.status_ = "RUNNING"

    def state_RUNNING(self):
        self.resetStatus()
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
                print("RUNNING")
                time.sleep(0.5)
                continue
            elif type == Reply.ReplyType.SUCCESS:
                print("SUCCESS")
                break
            else:
                print("Unrecognized status from Ph2_ACF")
                self.status_ = "ERROR"
                return
        stopMessage = Query.QueryMessage()
        stopMessage.query_type.type = Query.QueryType.STOP
        stringStopMessage = stopMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.stop(stringStopMessage)
        self.status_ = "STOPPED"
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"

    def state_STOPPED(self):
        self.resetStatus()
        haltMessage = Query.QueryMessage()
        haltMessage.query_type.type = Query.QueryType.HALT
        stringMessage = haltMessage.SerializeToString()
        replyBuffer = Ph2_ACF_controller.halt(stringMessage)
        self.status_ = "DONE"
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.status_ = "ERROR"

    def state_ERROR(self):
        print("An error occurred")
        self.status_ = "DONE"
        self.calibrationResult_ = "FAILED"

    def runCalibration(self):
        while(self.status_ != "DONE"):
            self.switch(self.status_)
        if self.calibrationResult_ == "SUCCESS":
            self.status_ = "HALTED"
        return self.calibrationResult_

    def isSuccess(self):
        if(self.calibrationResult_ == "SUCCESS"): return True
        return False

    def getErrorMessage(self):
        return self.errorMessage_

    def queryFirmware(self, action, configurationFile, boardId, firmwareName = "", fileName = ""):
        firmwareListQuery = Query.FirmwareQueryMessage()
        firmwareListQuery.query_type.type = Query.QueryType.FPGA
        firmwareListQuery.configuration_file = configurationFile
        firmwareListQuery.board_id = boardId
        if action == "LIST": firmwareListQuery.action = Query.FirmwareQueryMessage.LIST
        if action == "LOAD": firmwareListQuery.action = Query.FirmwareQueryMessage.LOAD
        if action == "UPLOAD": firmwareListQuery.action = Query.FirmwareQueryMessage.UPLOAD
        if action == "DOWNLOAD": firmwareListQuery.action = Query.FirmwareQueryMessage.DOWNLOAD
        if action == "DELETE": firmwareListQuery.action = Query.FirmwareQueryMessage.DELETE
        if action != "LIST":
            firmwareListQuery.firmware_name = firmwareName
        if action == "UPLOAD" or action == "DOWNLOAD":
            firmwareListQuery.file_name = fileName
        stringMessage = firmwareListQuery.SerializeToString()
        replyBuffer = Ph2_ACF_controller.firmwareAction(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.calibrationResult_ = "FAILED"
            return "FAILED"
        else:
            return replyBuffer

    def listFirmware(self, configurationFile, boardId):
        replyBuffer = self.queryFirmware("LIST", configurationFile, boardId)
        if replyBuffer == "FAILED": return
        theFirmwareReply = Reply.FirmwareReplyMessage()
        theFirmwareReply.ParseFromString(replyBuffer)
        listOfFirmwares = []
        for firmware in theFirmwareReply.firmware_name:
            listOfFirmwares.append(firmware)
        return listOfFirmwares

    def loadFirmware(self, configurationFile, firmwareName, boardId):
        self.queryFirmware("LOAD", configurationFile, boardId, firmwareName)

    def uploadFirmware(self, configurationFile, firmwareName, fileName, boardId):
        self.queryFirmware("UPLOAD", configurationFile, boardId, firmwareName, fileName)

    def downloadFirmware(self, configurationFile, firmwareName, fileName, boardId):
        self.queryFirmware("DOWNLOAD", configurationFile, boardId, firmwareName, fileName)

    def deleteFirmware(self, configurationFile, firmwareName, boardId):
        self.queryFirmware("DELETE", configurationFile, boardId, firmwareName)

    def queryCalibrationList(self):
        print(Common.CalibrationList.calibrationandpedenoise)
        print(Common.CalibrationList)
        calibrationListQuery = Query.QueryMessage()
        calibrationListQuery.query_type.type = Query.QueryType.CALIBRATION
        stringMessage = calibrationListQuery.SerializeToString()
        replyBuffer = Ph2_ACF_controller.calibrationList(stringMessage)
        if self.parseReply(replyBuffer) != Reply.ReplyType.SUCCESS:
            self.calibrationResult_ = "FAILED"
            return "FAILED"
        theCalibrationListReply = Reply.CalibrationListReplyMessage()
        theCalibrationListReply.ParseFromString(replyBuffer)
        for calibration in theCalibrationListReply.calibration:
            self.mapOfCalibrations_[calibration.calibration_name] = calibration.calibration_tag.calibration_name

    def getCalibrationList(self):
        if not self.mapOfCalibrations_:
            self.queryCalibrationList()
        listOfCalibrations = []
        for key in self.mapOfCalibrations_:
            listOfCalibrations.append(key)
        return listOfCalibrations
