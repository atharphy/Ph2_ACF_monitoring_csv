#include "tools/OTPSADCCalibration.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/ADCSlope.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPSADCCalibration::fCalibrationDescription = "Calibrate the ADC of MPA and SSA chips. First it calibrates VREF using the bandgap values, then it calibrates the ADC biases.";

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
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;
    LOG(INFO) << BOLDMAGENTA << "Starting OTPSADCCalibration measurement." << RESET;
    Initialise();
    CalibrateBias(); 
    LOG(INFO) << BOLDMAGENTA << "Done with OTPSADCCalibration." << RESET;
    Reset();
}

void OTPSADCCalibration::CalibrateBias()
{

    DetectorDataContainer theVREFDACContainer;
    ContainerFactory::copyAndInitChip<std::pair<uint8_t, float>>(*fDetectorContainer, theVREFDACContainer);
    DetectorDataContainer theADCSlopeContainer;
    ContainerFactory::copyAndInitChip<ADCSlope>(*fDetectorContainer, theADCSlopeContainer);
    DetectorDataContainer theAVDDContainer;
    ContainerFactory::copyAndInitChip<std::pair<uint32_t, float>>(*fDetectorContainer, theAVDDContainer);
    DetectorDataContainer theDVDDContainer;
    ContainerFactory::copyAndInitChip<std::pair<uint32_t, float>>(*fDetectorContainer, theDVDDContainer);

    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) 
                { 
                  DisableTestPadsOutput(cChip);
                } // chip
            }
        }
    }

// #ifdef __USE_ROOT__
//     fDQMHistogramPSBiasCal.fillDACPlots(theVREFDACContainer);
//     fDQMHistogramPSBiasCal.fillSlopePlots(theADCSlopeContainer);
//     fDQMHistogramPSBiasCal.fillVDDPlots(theAVDDContainer, true);
//     fDQMHistogramPSBiasCal.fillVDDPlots(theDVDDContainer, false);
// #else
//     if(fDQMStreamerEnabled)
//     {
//         ContainerSerialization theContainerSerialization("PSBiasCalVrefDac");
//         theContainerSerialization.streamByBoardContainer(fDQMStreamer, theVREFDACContainer);
//         ContainerSerialization theSecondContainerSerialization("PSBiasCalADCSlope");
//         theSecondContainerSerialization.streamByBoardContainer(fDQMStreamer, theADCSlopeContainer);
//         ContainerSerialization theAVDDContainerSerialization("PSBiasCalAVDD");
//         theAVDDContainerSerialization.streamByBoardContainer(fDQMStreamer, theAVDDContainer);
//         ContainerSerialization theDVDDContainerSerialization("PSBiasCalDVDD");
//         theDVDDContainerSerialization.streamByBoardContainer(fDQMStreamer, theDVDDContainer);
//     }
// #endif

}

void OTPSADCCalibration::DisableTestPadsOutput(ReadoutChip* cChip)
{
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        LOG(INFO) << BOLDMAGENTA << "Disable all MPA test pads outputs... " << RESET;
        static_cast<MPA2Interface*>(static_cast<PSInterface*>(fReadoutChipInterface)->getInterface(cChip))->selectBlock(cChip, 0);
    }
    else if(cChip->getFrontEndType() == FrontEndType::SSA2)
    {
        LOG(INFO) << BOLDMAGENTA << "Disable all SSA test pads outputs... " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_lsb", 0x0);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_msb", 0x0);
    }
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
