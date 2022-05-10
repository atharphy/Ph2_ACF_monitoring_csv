import sys
import os
import Ph2_ACF_StateMachine
import argparse

sys.path.insert(1, os.getenv('PH2ACF_BASE_DIR'))
import lib.Ph2_ACF_PythonInterface as Ph2_ACF

###########OPTIONS
parser = argparse.ArgumentParser(description='Command line parser of skim options')
parser.add_argument('-f', dest='configuration_file', help='xml configuration file', required = True)
parser.add_argument('-c', dest='calibration_name'  , help='calibration name'      , required = True)

args = parser.parse_args()
configuration_file = args.configuration_file
calibration_name   = args.calibration_name

Ph2_ACF.configureLogger(os.getenv('PH2ACF_BASE_DIR') + "/settings/logger.conf")

theStateMachine = Ph2_ACF_StateMachine.StateMachine(configuration_file, calibration_name)

calibrationResult = theStateMachine.runCalibration()
print("Calibration result: " + calibrationResult)
