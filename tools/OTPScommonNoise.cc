#include <boost/math/distributions/normal.hpp>
#include "tools/OTPScommonNoise.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPScommonNoise::fCalibrationDescription = "Measure common noise in PS modules";

OTPScommonNoise::OTPScommonNoise() : Tool() {}

OTPScommonNoise::~OTPScommonNoise() {}

void OTPScommonNoise::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTPScommonNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTPScommonNoise::ConfigureCalibration()
{

}

void OTPScommonNoise::SetThresholds()
{
    // For PS modules the CIC is in sparsified mode. Therefore we cannot have more 128 channel per hybrid on the strips and pixels sensor.
    // Therefore we must set a threshold that allows around 64 channels per pixels and strips
    uint8_t theMaximumChannelNumber = 64;
    float theStripAllowedOccupancy  = float(theMaximumChannelNumber)/(NSSACHANNELS*NCHIPS_OT);
    float thePixelAllowedOccupancy  = float(theMaximumChannelNumber)/(NSSACHANNELS*NMPAROWS*NCHIPS_OT);

    LOG(INFO) << BOLDYELLOW << "theStripAllowedOccupancy: " << theStripAllowedOccupancy << " thePixelAllowedOccupancy: "<< thePixelAllowedOccupancy << RESET;

    // Now we calculate how many sigmas away from the pedestal we should be to have that occupancy
    boost::math::normal gaus(0, 1); // we consider a standard gaussian
    float theStripSigma = quantile(complement(gaus, theStripAllowedOccupancy));
    float thePixelSigma = quantile(complement(gaus, thePixelAllowedOccupancy));

    LOG(INFO) << BOLDYELLOW << "theStripSigma: " << theStripSigma << " thePixelSigma: "<< thePixelSigma << RESET;

    // now we set the thresold  to pedestal + noise*sigma
    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    float theSigma = (cChip->getFrontEndType() == FrontEndType::SSA2) ? theStripSigma : thePixelSigma;
                    LOG(INFO) << BOLDYELLOW << " chip " << +cChip->getId() << " cChip->getAveragePedestal(): " << cChip->getAveragePedestal() << " cChip->getAverageNoise: " << cChip->getAverageNoise() << RESET;
                    float theThreshold = cChip->getAveragePedestal() + cChip->getAverageNoise() + theSigma;
                    LOG(INFO) << BOLDYELLOW << " chip " << +cChip->getId() << " theThreshold: " << theThreshold << " rounded " << std::round(theThreshold) << RESET;
                    fReadoutChipInterface->WriteChipReg(cChip,"Threshold",std::round(theThreshold));
                }
            }
        }
    }
}
void OTPScommonNoise::TakeData()
{

}

void OTPScommonNoise::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S)
    { 
        LOG(ERROR) << ERROR_FORMAT << " Running a PS calibration on a 2S module! " << RESET;
        return;
    }
    LOG(INFO) << BOLDMAGENTA << "Starting OTPScommonNoise measurement." << RESET;
    Initialise();
    SetThresholds();
    TakeData();
    LOG(INFO) << BOLDMAGENTA << "Done with OTPScommonNoise." << RESET;
    Reset();
}

void OTPScommonNoise::Stop(void)
{
    LOG(INFO) << "Stopping OTPScommonNoise measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTPScommonNoise.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTPScommonNoise stopped.";
}

void OTPScommonNoise::Pause()
{

}


void OTPScommonNoise::Resume()
{

}


void OTPScommonNoise::Reset()
{
    fRegisterHelper->restoreSnapshot();
}
