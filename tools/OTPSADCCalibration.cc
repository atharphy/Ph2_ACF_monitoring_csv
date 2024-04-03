#include "tools/OTPSADCCalibration.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/ADCSlope.h"
#include "HWInterface/D19cFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPSADCCalibration::fCalibrationDescription = "Calibrate the ADC of MPA and SSA chips. First it calibrates VREF using the bandgap values, then it calibrates the ADC biases.";

OTPSADCCalibration::OTPSADCCalibration() : Tool() {}

OTPSADCCalibration::~OTPSADCCalibration() {}

void OTPSADCCalibration::Initialise(void)
{
    fRegisterHelper->takeSnapshot();

    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "ADCcontrol");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "A[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "B[0-6]");
    // fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "C[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "D[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "E[0-6]");

    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "ADC_VREF");
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

    for(const auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalReadout: *theBoard)
        {
            for(auto theHybrid: *theOpticalReadout)
            {
                for(auto theChip: *theHybrid) 
                { 
                    LOG(INFO) << BOLDGREEN << "------------------------------------------------- "  << RESET;
                    LOG(INFO) << BOLDGREEN << "Calibrating ADC of " << theChip->getFrontEndName(theChip->getFrontEndType()) << "#" << +theChip->getId() << " on Hybrid#"<< +theHybrid->getId() << RESET;
                    LOG(INFO) << BOLDGREEN << "------------------------------------------------- "  << RESET;

                    fReadoutChipInterface->disableTestPadsOutput(theChip);
                    float theGroundValue = fReadoutChipInterface->readADCGround(theChip);
                    LOG(DEBUG) << BOLDMAGENTA << "Ground ADC value is " << theGroundValue << RESET;
                    uint8_t theVrefRegisterValue = 0;
                    LOG(INFO) << BOLDYELLOW << "Going to calibrate the voltage reference value..."<< RESET;
                    float theVrefValue = CalibrateVref(theChip, &theVrefRegisterValue);

                    LOG(DEBUG) << MAGENTA << " ------ setting VREF in theVREFDACContainer "  << RESET;
                    theVREFDACContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<std::pair<uint8_t, float>>()
                        .first = theVrefRegisterValue;
                    theVREFDACContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<std::pair<uint8_t, float>>()
                        .second = theVrefValue;

                    LOG(INFO) << BOLDYELLOW << "Going to calibrate the ADC bias registers..."<< RESET;
                    CalibrateChipBias(theChip, theVrefValue);

                    theADCSlopeContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<ADCSlope>()
                        .fADC_GND = theGroundValue;
                    uint32_t theADCBandgapValue =  fReadoutChipInterface->readADCBandGap(theChip);
                    theADCSlopeContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<ADCSlope>()
                        .fADC_VBG = theADCBandgapValue;
                    float theBandgapVoltage = fReadoutChipInterface->getBandGapExpectedValue(theChip); //FIXME this should be the real bandgap value!!
                    theADCSlopeContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<ADCSlope>()
                        .fMeasured_VBG = theBandgapVoltage; 
                    float theSlope       = theBandgapVoltage / (theADCBandgapValue - theGroundValue);
                    float theOffset      = -theGroundValue * theSlope;
                    theADCSlopeContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<ADCSlope>()
                        .fSlope = theSlope;
                    theADCSlopeContainer.getObject(theChip->getBeBoardId())
                        ->getObject(theChip->getOpticalGroupId())
                        ->getObject(theChip->getHybridId())
                        ->getObject(theChip->getId())
                        ->getSummary<ADCSlope>()
                        .fOffset = theOffset;


                    // make sure test pads output is disabled
                    fReadoutChipInterface->disableTestPadsOutput(theChip);

                } // chip
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTPSADCCalibration.fillDACPlots(theVREFDACContainer);
    fDQMHistogramOTPSADCCalibration.fillSlopePlots(theADCSlopeContainer);
    fDQMHistogramOTPSADCCalibration.fillVDDPlots(theAVDDContainer, true);
    fDQMHistogramOTPSADCCalibration.fillVDDPlots(theDVDDContainer, false);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("OTPSADCCalibrationVrefDac");
        theContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theVREFDACContainer);
        ContainerSerialization theSecondContainerSerialization("OTPSADCCalibrationADCSlope");
        theSecondContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theADCSlopeContainer);
        ContainerSerialization theAVDDContainerSerialization("OTPSADCCalibrationAVDD");
        theAVDDContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theAVDDContainer);
        ContainerSerialization theDVDDContainerSerialization("OTPSADCCalibrationDVDD");
        theDVDDContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theDVDDContainer);
    }
#endif

}

void OTPSADCCalibration::CalibrateChipBias(ReadoutChip* theChip, float theVrefValue)
{

    // The register table is < std::string register name, < uint8_t register default value, float register expected value>>
    auto theRegistersTable = fReadoutChipInterface->getBiasStructureDefaultTable(theChip);
    for (auto it = theRegistersTable.begin(); it != theRegistersTable.end(); it++)
    {
        std::string theRegisterName  = it->first;
        uint8_t     theDefaultValue  = it->second.first;
        float       theExpectedValue = it->second.second;

        float theADCLSB = fReadoutChipInterface->calculateADCLSB(theChip,theVrefValue);
        fReadoutChipInterface->TuneDAC(theChip,theADCLSB,theExpectedValue,theRegisterName,theDefaultValue,false);
    }

}


float OTPSADCCalibration::CalibrateVref(Ph2_HwDescription::ReadoutChip* theChip, uint8_t* theVrefRegisterValue)
{

    //FIXME At the moment we are setting the exepected values 
    // of bandgap and ADC_VREF to the default nominal value.
    // This will be updated once we have the real values for each chip
    float    theBandGapExpectedValue  = fReadoutChipInterface->getBandGapExpectedValue(theChip);
    float    theVrefExpectedValue     = fReadoutChipInterface->getVrefExpectedValue(theChip);
    float    theVrefMinValue          = fReadoutChipInterface->getVrefMinValue(theChip);
    float    theVrefMaxValue          = fReadoutChipInterface->getVrefMaxValue(theChip);
    float    theVrefPrecision         = fReadoutChipInterface->getVrefPrecision(theChip);

    uint32_t theADCGroundValue        = fReadoutChipInterface->readADCGround(theChip);   
    uint32_t theADCMaxValue           = 4095;
    uint32_t theADCBandGapValue       = fReadoutChipInterface->readADCBandGap(theChip);
    uint8_t  theVrefFuseIDValue       = theChip->pChipFuseID.ADCRef();
    uint8_t  theVrefReadRegisterValue = fReadoutChipInterface->readVrefRegister(theChip);
    //FIXME for now the VREF is not written in the SSA fuse ID so we check if it is zero or not. 
    uint8_t  theVrefToUse = theVrefFuseIDValue !=0 ? theVrefFuseIDValue : theVrefReadRegisterValue;
    
    *theVrefRegisterValue = theVrefToUse;

    float theADCSlope  = theBandGapExpectedValue / (float(theADCBandGapValue) - float(theADCGroundValue));
    float theADCOffset = -float(theADCGroundValue) * theADCSlope;

    LOG(DEBUG) << BLUE << "theADCOffset " << theADCOffset << " theADCSlope " << theADCSlope << RESET;
    LOG(INFO) << MAGENTA << "The initial BandGapValue in ADC is " << theADCBandGapValue << RESET;
 
    float theVrefObtained = theADCMaxValue * theADCSlope + theADCOffset;
    LOG(INFO) << MAGENTA << "For the intial ADC VREF register value " << +theVrefToUse << ":" << RESET;
    LOG(INFO) << MAGENTA << "VREF extrapolated value: " << theVrefObtained << " [V]" << RESET;
    LOG(INFO) << MAGENTA << "VREF expected     value: " << theVrefExpectedValue << " [V]" << RESET;

    LOG(DEBUG) << BOLDBLUE << "theVrefObtained " << theVrefObtained << " theVrefMaxValue " << theVrefMaxValue << " theVrefMinValue " << theVrefMinValue << " (theVrefObtained - theVrefExpectedValue) " << (theVrefObtained - theVrefExpectedValue)
               << " theVrefPrecision " << theVrefPrecision << RESET;

    if(theVrefObtained > theVrefMaxValue || theVrefObtained < theVrefMinValue || abs(theVrefObtained - theVrefExpectedValue) > theVrefPrecision)
    {
        LOG(INFO) << BOLDRED << "Need to calibrate VREF" << RESET;

        theVrefToUse = fReadoutChipInterface->TuneDAC(theChip, theVrefExpectedValue / (theADCMaxValue - theADCGroundValue), theBandGapExpectedValue, "ADC_VREF", theVrefToUse, true); 
        fReadoutChipInterface->setVref(theChip, theVrefToUse);

        LOG(DEBUG) << BOLDGREEN << " VREF calibrated" << RESET;

        
        theADCBandGapValue = fReadoutChipInterface->readADCBandGap(theChip);
        LOG(INFO) << BLUE << " theADCBandGapValue " << theADCBandGapValue << " theADCGroundValue " << theADCGroundValue << RESET;
        theADCSlope  = (theBandGapExpectedValue) / (theADCBandGapValue - theADCGroundValue);
        theADCOffset = -(float(theADCGroundValue) * theADCSlope);
        LOG(INFO) << BLUE << "theADCSlope " << theADCSlope << " theADCOffset " << theADCOffset << RESET;
        *theVrefRegisterValue = theVrefToUse;

        theVrefObtained = theADCMaxValue * theADCSlope + theADCOffset;
        LOG(INFO) << BOLDRED << "for new theVrefToUse " << theVrefToUse << " New VREF val: " << theVrefObtained << " Expected val: " << theVrefExpectedValue << RESET;
    }

    LOG(INFO) << BOLDGREEN << "VREF calibrated *theVrefRegisterValue " << +(*theVrefRegisterValue) << " theVrefObtained " << theVrefObtained << RESET;
    return theVrefObtained;
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
