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

    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "vref");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "A[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "B[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "C[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "D[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "E[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "ThDAC[0-6]");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "CalDAC[0-6]");

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
                    fReadoutChipInterface->disableTestPadsOutput(theChip);
                    float theGroundValue = fReadoutChipInterface->readADCGround(theChip);
                    LOG(INFO) << BOLDMAGENTA << "Ground Value for " << theChip->getFrontEndName(theChip->getFrontEndType()) << "#" << +theChip->getId() << "on hybrid "<< +theHybrid->getId() << " is " << theGroundValue << RESET;
                    uint8_t theVrefRegisterValue = 0;
                    float theVrefValue = CalibrateVref(theChip, &theVrefRegisterValue);
                    std::cout << "theVrefValue " << theVrefValue <<std::endl;
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


// One should first tune Vref using the BandGap as reference to tune it and then tune the different bias registers.
uint8_t OTPSADCCalibration::TuneDAC(Ph2_HwDescription::ReadoutChip* theChip, float theSlope, float theExpectedValue, std::string theDACtoTuneName, uint8_t theDACValue, bool isVref)
{

    LOG(INFO) << MAGENTA << " theDACtoTuneName " << theDACtoTuneName << RESET;

    uint32_t theGroundADCValue = fReadoutChipInterface->readADCGround(theChip);
    LOG(INFO) << MAGENTA << " theGroundADCValue " << theGroundADCValue  << RESET;

    // write DAC (ie one of the registers) with value 0 (minimum)
    uint8_t theDACMinValue = 0;
    if(isVref)
        fReadoutChipInterface->setVref(theChip, theDACMinValue);
    else
        fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACMinValue);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t theOffsetValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName) : fReadoutChipInterface->readADCBandGap(theChip);
    LOG(INFO) << BLUE << "DAC " << theDACtoTuneName << " at " << +theDACMinValue << " gives theOffsetValue " << theOffsetValue << RESET;

    // now set the DAC value to its max value
    uint8_t theDACMaxValue = 0x1F;
    if(isVref)
        fReadoutChipInterface->setVref(theChip, theDACMaxValue);
    else
    fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACMaxValue);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t theMaxValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
    LOG(INFO) << BLUE << " DAC " << theDACtoTuneName << " at " << +theDACMaxValue << " gives theMaxValue " << theMaxValue << RESET;

    float theLSB = abs(float(theMaxValue) - float(theOffsetValue)) / float(theDACMaxValue);
    LOG(INFO) << BOLDMAGENTA << " abs(float(theMaxValue) - float(theOffsetValue)) " << abs(float(theMaxValue) - float(theOffsetValue)) << " float(theDACMaxValue) " << float(theDACMaxValue) << RESET;
    LOG(INFO) << BLUE << theDACtoTuneName << " LSB " << theLSB << RESET;

    float theADCDExpectedValue = 0.0;
    theADCDExpectedValue       = theExpectedValue / theSlope + theGroundADCValue; // converted from volts to ADC
    LOG(INFO) << BLUE << theDACtoTuneName << " expected value in ADC " << theADCDExpectedValue << RESET;

    // now set the DAC value to its default value and get the value at the default value 
    if(isVref)
        fReadoutChipInterface->setVref(theChip, theDACValue);
    else
    fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t theCurrentValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
    LOG(INFO) << BLUE << " theDACValue at nominal value " << +theDACValue << " gives theCurrentValue " << theCurrentValue << RESET;

    int theStepSign = 0;
    if(theADCDExpectedValue < theCurrentValue)
        theStepSign =(isVref) ? 1 : -1;
    else
        theStepSign =(isVref) ? -1 : 1;

    uint8_t theSteps       = uint8_t(std::round(abs(float(theADCDExpectedValue) - float(theCurrentValue)) / float(theLSB)));
    uint8_t theDACNewValue = 0;
    LOG(INFO) << MAGENTA << " theSteps " << +theSteps << RESET;
    if(float(theDACValue + theStepSign * theSteps) > theDACMaxValue)
        theDACNewValue = theDACMaxValue;
    else if((float(theDACValue + theStepSign * theSteps) < theDACMinValue))
        theDACNewValue = theDACMinValue;
    else
        theDACNewValue = theDACValue + theStepSign * theSteps;
 
    theDACValue = theDACNewValue;

    LOG(INFO) << MAGENTA << " theDACNewValue " << +theDACNewValue << RESET;

    // now writing the DAC to the same new value  estimated above
    if(isVref)
        fReadoutChipInterface->setVref(theChip, theDACNewValue);
    else
        fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACNewValue);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    uint32_t theNewValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
    LOG(INFO) << BOLDBLUE << "after changing value for DAC " << theDACtoTuneName << " to " << +theDACValue << " the theNewValue is " << theNewValue << RESET;


    bool     isSearching = true;
    uint32_t theCurrentIteration = 0;
    while(isSearching)
    {

        LOG(INFO) << MAGENTA << " theDACNewValue - 1 " << +theDACNewValue - 1 << RESET;
        uint8_t theDACDownValue = std::max(theDACMinValue, uint8_t(theDACNewValue - 1));
        if(isVref)
            fReadoutChipInterface->setVref(theChip, theDACDownValue);
        else
            fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACDownValue);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint32_t theNewValueDown =  isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);


        uint8_t theDACUpValue = std::min(uint8_t(theDACMaxValue), uint8_t(theDACNewValue + 1));
        LOG(INFO) << MAGENTA << "theDACUpValue " << +theDACUpValue << RESET;
        if(isVref)
            fReadoutChipInterface->setVref(theChip, theDACUpValue);
        else
            fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACUpValue);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        uint32_t theNewValueUp = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
    
        float theExpectedDifference     = std::fabs(theADCDExpectedValue - theNewValue);
        float theExpectedDifferenceDown = std::fabs(theADCDExpectedValue - theNewValueDown);
        float theExpectedDifferenceUp   = std::fabs(theADCDExpectedValue - theNewValueUp);

        if((theExpectedDifferenceDown < theExpectedDifference) || (theExpectedDifferenceUp < theExpectedDifference))
        {
            LOG(INFO) << BOLDRED << "Bad extrapolation in PSBiasCal: theExpectedDifferenceDown:" << theExpectedDifferenceDown << ", theExpectedDifferenceUp:" << theExpectedDifferenceUp << ", theExpectedDifference:" << theExpectedDifference << ", iteration:" << theCurrentIteration << RESET;
            if((theExpectedDifferenceDown < theExpectedDifference))
            {
                theDACValue     = theDACDownValue;
                theDACNewValue = theDACNewValue - 1;
            }
            if((theExpectedDifferenceUp < theExpectedDifference))
            {
                theDACValue     = theDACUpValue;
                theDACNewValue = std::min(uint8_t(theDACMaxValue), uint8_t(theDACNewValue + 1));
            }
        }
        else
        {
            LOG(INFO) << BOLDMAGENTA << "Correct extrapolation in PSBiasCal , iteration:" << theCurrentIteration << " theDACValue "<< +theDACValue << RESET;
            if(isVref)
                fReadoutChipInterface->setVref(theChip, theDACValue);
            else
                fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            usleep(100000);
            uint32_t theCheckValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
            LOG(INFO) << BOLDGREEN << " Register " << theDACtoTuneName << " gives ADC " << theCheckValue << RESET;
            isSearching = false;
        }
        LOG(INFO) << BOLDMAGENTA << "Writing DAC val " << +theDACValue << RESET;
        if(isVref)
            fReadoutChipInterface->setVref(theChip, theDACValue);
        else
            fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

        theNewValue = isVref == 0 ? fReadoutChipInterface->readADC(theChip,theDACtoTuneName): fReadoutChipInterface->readADCBandGap(theChip);
 
        theCurrentIteration += 1;
    }

    LOG(INFO) << BOLDBLUE << "New DAC val: " << theNewValue << " Expected val: " << theADCDExpectedValue << "+/-" << theLSB << RESET;

    return theDACValue;
}

float OTPSADCCalibration::CalibrateVref(Ph2_HwDescription::ReadoutChip* theChip, uint8_t* theVrefRegisterValue)
{

    //FIXME At the moment we are setting the exepected values 
    // of bandgap and vref to the default nominal value.
    // This will be updated once we have the real values for each chip
    float    theBandGapExpectedValue  = fReadoutChipInterface->getBandGapExpectedValue(theChip);
    float    theVrefExpectedValue     = fReadoutChipInterface->getVrefExpectedValue(theChip);
    float    theVrefMinValue          = fReadoutChipInterface->getVrefMinValue(theChip);
    float    theVrefMaxValue          = fReadoutChipInterface->getVrefMaxValue(theChip);
    float    theVrefPrecision         = fReadoutChipInterface->getVrefPrecision(theChip);

    uint32_t theADCGroundValue        = fReadoutChipInterface->readADCGround(theChip);   
    uint32_t theADCMaxValue           = 4095;
    uint32_t theADCBandGapValue       = fReadoutChipInterface->readADCBandGap(theChip);
    uint8_t  theRetrievedVrefADCValue = 0;
    uint8_t  theVrefFuseIDValue       = theChip->pChipFuseID.ADCRef();
    uint8_t  theVrefReadRegisterValue = fReadoutChipInterface->readVrefRegister(theChip);
    //FIXME for now the VREF is not written in the SSA fuse ID so we check if it is zero or not. 
    uint8_t  theVrefToUse = theVrefFuseIDValue !=0 ? theVrefFuseIDValue : theVrefReadRegisterValue;

    std::cout << " theVrefFuseIDValue " << +theVrefFuseIDValue << " theVrefReadRegisterValue " << +theVrefReadRegisterValue << " theVrefToUse " << +theVrefToUse << std::endl; 
    
    *theVrefRegisterValue = theVrefToUse;

    float theADCSlope  = theBandGapExpectedValue / (float(theADCBandGapValue) - float(theADCGroundValue));
    float theADCOffset = -float(theADCGroundValue) * theADCSlope;

    LOG(INFO) << BLUE << " theADCOffset " << theADCOffset << " theADCSlope " << theADCSlope << " theADCBandGapValue " << theADCBandGapValue << " theADCGroundValue " << theADCGroundValue << RESET;

    float theVrefObtained = theADCMaxValue * theADCSlope + theADCOffset;
    LOG(INFO) << BLUE << "for theVrefToUse " << +theVrefToUse << " VREF val extrapolated: " << theVrefObtained << " Expected val: " << theVrefExpectedValue << RESET;

    LOG(INFO) << BOLDBLUE << " theVrefObtained " << theVrefObtained << " theVrefMaxValue " << theVrefMaxValue << " theVrefMinValue " << theVrefMinValue << " (theVrefObtained - theVrefExpectedValue) " << (theVrefObtained - theVrefExpectedValue)
               << " theVrefPrecision " << theVrefPrecision << RESET;

    if(theVrefObtained > theVrefMaxValue || theVrefObtained < theVrefMinValue || abs(theVrefObtained - theVrefExpectedValue) > theVrefPrecision)
    {
        LOG(INFO) << BOLDRED << " Need to calibrate VREF" << RESET;
        LOG(INFO) << BOLDRED << " theVrefToUse " << +theVrefToUse << RESET;

        theVrefToUse = TuneDAC(theChip, theVrefExpectedValue / (theADCMaxValue - theADCGroundValue), theBandGapExpectedValue, "vref", theVrefToUse, true); 

        LOG(INFO) << BLUE << "calibrated theVrefToUse " << +theVrefToUse << RESET;

        fReadoutChipInterface->setVref(theChip, theVrefToUse);

        theRetrievedVrefADCValue = fReadoutChipInterface->readVrefRegister(theChip);

        LOG(INFO) << BOLDRED << " retrieve dac after writing " << theRetrievedVrefADCValue << RESET;
        LOG(INFO) << BOLDRED << " VREF calibrated" << RESET;

        
        theADCBandGapValue = fReadoutChipInterface->readADCBandGap(theChip);

        theADCSlope  = (theBandGapExpectedValue) / (theADCBandGapValue - theADCGroundValue);
        theADCOffset = -theADCGroundValue * theADCSlope;

        *theVrefRegisterValue = theRetrievedVrefADCValue;

        theVrefObtained = theADCMaxValue * theADCSlope + theADCOffset;
        LOG(DEBUG) << BOLDRED << "for new theVrefToUse " << +theRetrievedVrefADCValue << " New VREF val: " << theVrefObtained << " Expected val: " << theVrefExpectedValue << RESET;
    }

    LOG(INFO) << BOLDMAGENTA << " VREF calibrated *theVrefRegisterValue " << +(*theVrefRegisterValue) << " theVrefObtained " << theVrefObtained << RESET;
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
