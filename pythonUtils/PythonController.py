import sys
import os

sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR'))

import lib.Ph2_ACF_PythonInterface as Ph2_ACF
import MessageUtils.python.QueryMessage_pb2 as query

Ph2_ACF_controller = Ph2_ACF.MiddlewareMessageHandler()
Ph2_ACF_controller.print()


initializeMessage = query.QueryMessage()
initializeMessage.query_type.type = query.QueryType.INITIALIZE

stringMessage = initializeMessage.SerializeToString()

output = Ph2_ACF_controller.initialize(stringMessage)

configureMessage = query.ConfigurationMessage()

configureMessage.query_type.type = query.QueryType.CONFIGURE
configureMessage.data.calibration_name = query.ConfigurationInfo.CALIBRATIONANDPEDENOISE
configureMessage.data.configuration_file = os.getenv('PH2ACF_BASE_DIR') + "/settings/D19CDescription_2Sskeleton.xml"

print(configureMessage)

stringMessage = initializeMessage.SerializeToString()
output = Ph2_ACF_controller.configure(stringMessage)
