#include "tools/OTPSADCCalibration.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPSADCCalibration::fCalibrationDescription = "Insert brief calibration description here";

OTPSADCCalibration::OTPSADCCalibration() : Tool() {}

OTPSADCCalibration::~OTPSADCCalibration() {}

void OTPSADCCalibration::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "A[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "B[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "C[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "D[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "E[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "ThDAC[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "CalDAC[0-6]");

    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5BFEED");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5PREAMP");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5TDR");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5ALLV");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5ALLI");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "Bias_D5DAC8");


#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTPSADCCalibration.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTPSADCCalibration::ConfigureCalibration()
{

}

void OTPSADCCalibration::Running()
{
    LOG(INFO) << "Starting OTPSADCCalibration measurement.";
    Initialise();
    CalibrateBias() // FIXMEEEE!!! This needs to be added (and maybe renamed)
    LOG(INFO) << "Done with OTPSADCCalibration.";
    Reset();
}

void OTPSADCCalibration::Stop(void)
{
    LOG(INFO) << "Stopping OTPSADCCalibration measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTPSADCCalibration.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTPSADCCalibration stopped.";
}

void OTPSADCCalibration::Pause()
{

}


void OTPSADCCalibration::Resume()
{

}


void OTPSADCCalibration::Reset()
{
    fRegisterHelper->restoreSnapshot();
}
