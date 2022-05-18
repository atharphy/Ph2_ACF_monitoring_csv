import sys
import os
import Ph2_ACF_StateMachine
import argparse

sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR'))
import lib.Ph2_ACF_PythonInterface as Ph2_ACF

###########OPTIONS
parser = argparse.ArgumentParser(description='Command line parser of skim options')
parser.add_argument('-f', dest='configurationFile', help='xml configuration file', required = True)
parser.add_argument('-c', dest='calibrationName'  , help='calibration name'      , required = True)

args = parser.parse_args()
configurationFile = args.configurationFile
calibrationName   = args.calibrationName

Ph2_ACF.configureLogger(os.getenv('PH2ACF_BASE_DIR') + "/settings/logger.conf")

theStateMachine = Ph2_ACF_StateMachine.StateMachine()
theStateMachine.setConfigurationFile(configurationFile)
theStateMachine.setCalibrationName(calibrationName)

theStateMachine.runCalibration()

print("-------------------------------------------------")
print("Calibration " + calibrationName + " result:")
if(theStateMachine.isSuccess()):
    print("Success")
else:
    print("Failed, Error message = " + theStateMachine.getErrorMessage())
