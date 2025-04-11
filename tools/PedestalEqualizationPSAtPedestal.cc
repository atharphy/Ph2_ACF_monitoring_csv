#include "tools/PedestalEqualizationPSAtPedestal.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string PedestalEqualizationPSAtPedestal::fCalibrationDescription = "Equalize the pedestal/threshold for all channels with higher precision near the pedestal";
// This class should do the same as the PedestalEqualization with PS FullScan but do the MPA in a different way!!!!
// I will have to copy stuff from PedestalEqualization because of the SSA, but then do things differently for MPA!
// PedestalEqualizationPSAtPedestal::PedestalEqualizationPSAtPedestal() : Tool() {}

// PedestalEqualizationPSAtPedestal::~PedestalEqualizationPSAtPedestal() {}

void PedestalEqualizationPSAtPedestal::Initialise(bool pAllChan, bool pDisableStubLogic)
{

    
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^TrimDAC_C\\d+_R\\d+$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^THTRIMMING_S\\d+$");

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    fWithSSA = false;
    fWithMPA = false;
    std::vector<FrontEndType> cAllFrontEndTypes;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }
    if(fWithSSA && !fWithMPA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with SSAs" << RESET;
    if(fWithMPA && !fWithSSA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with MPAs" << RESET;
    if(fWithSSA && fWithMPA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with SSAs+MPAs" << RESET;

    for(auto cFrontEndType: cAllFrontEndTypes)
    {
        if(cFrontEndType == FrontEndType::SSA2)
        {
            SSAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, 1, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
        else if(cFrontEndType == FrontEndType::MPA2)
        {
            MPAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NMPAROWS, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
    }

    this->fAllChan = pAllChan;

    fSkipMaskedChannels                = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups       = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fCheckLoop                         = findValueInSettings<double>("VerificationLoop", 1);
    fPedestalEqualizationMaskUntrimmed = findValueInSettings<double>("PedestalEqualization_MaskUntrimmed", 0);

    // FOR SSA Full Scan
    fOriginalIsFullScan                = findValueInSettings<double>("FullScan", 0) > 0;
    setValueInSettings<double>("FullScan", 1);

    fPedestalEqualizationFullScanStart = findValueInSettings<double>("PedestalEqualization_FullScanStart", 110);
    fPedestalEqualizationFullScanCAP   = findValueInSettings<double>("PedestalEqualizationFullScanCAP", 1.0);

    fTestPulseAmplitude    = findValueInSettings<double>("PedestalEqualization_PulseAmplitude", 0);
    fTestPulseAmplitudePix = findValueInSettings<double>("PedestalEqualization_PulseAmplitudePix", fTestPulseAmplitude);

    if(fFullScan)
    {
        std::cout << " FULL SCAN AMPLITUDE!" << std::endl;
        fTestPulseAmplitude    = findValueInSettings<double>("PedestalEqualization_PulseAmplitudeFullScan", 0);
        fTestPulseAmplitudePix = findValueInSettings<double>("PedestalEqualization_PulseAmplitudePixFullScan", 0);
    }

    fEventsPerPoint          = findValueInSettings<double>("Nevents", 10);
    fNEventsPerBurst         = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    fOccupancyAtPedestal     = findValueInSettings<double>("PedestalEqualization_Occupancy", 0.56);
    uint8_t cDefTargetOffset = (fWithCBC) ? 0x7F : 0xF;
    fTargetOffset            = findValueInSettings<double>("PedestalEqualizationTargetOffset", cDefTargetOffset);
    // bool fastCounterReadout  = findValueInSettings<double>("PedestalEqualization_FastCounterReadout", 1) > 0;
    //  FIXME! This fastCounterReadout was commented only for compiling and testing!! 
    LOG(INFO) << BOLDBLUE << "PedestalEqualization::Initialise Occupancy at pedestal is " << fOccupancyAtPedestal << " target offset is " << +fTargetOffset << RESET;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;

    // relevant registers, should not be here
    // if(cType == FrontEndType::MPA2)
    // { 
    //     fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitudePix);
    //     fReadoutChipInterface->WriteChipReg(cChip, "TrimDAC_ALL", 0xFF);

    //     // Vtrim
    //     fReadoutChipInterface->WriteChipReg(cChip, "C0", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C1", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C2", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C3", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C4", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C5", 0x0);
    //     fReadoutChipInterface->WriteChipReg(cChip, "C6", 0x0);

    // }
    // else 
    // { 
    //     fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitude); 
    //     fReadoutChipInterface->WriteChipReg(cChip, "THTRIMMING", 0x0);
        
    //     // Vtrim
    //     fReadoutChipInterface->WriteChipReg(cChip, "Bias_D5TDR", 0xFF); 

    //     // Bias_D5DAC8
    //     fReadoutChipInterface->WriteChipReg(cChip, "Bias_D5DAC8", 0xFF); 
    // }

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramPedestalEqualizationPSAtPedestal.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
    // FIXME this part is missing
}

void PedestalEqualizationPSAtPedestal::ConfigureCalibration()
{

}

void PedestalEqualizationPSAtPedestal::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;
    LOG(INFO) << "Starting PedestalEqualizationPSAtPedestal measurement.";

 
    Initialise(true, true);

    // Initialise(true, true);
    LOG(INFO) << "Done with PedestalEqualizationPSAtPedestal.";
    Reset();
}

void PedestalEqualizationPSAtPedestal::Stop(void)
{
    LOG(INFO) << "Stopping PedestalEqualizationPSAtPedestal measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramPedestalEqualizationPSAtPedestal.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "PedestalEqualizationPSAtPedestal stopped.";
}

void PedestalEqualizationPSAtPedestal::Pause()
{

}


void PedestalEqualizationPSAtPedestal::Resume()
{

}


void PedestalEqualizationPSAtPedestal::Reset()
{
    setValueInSettings<double>("FullScan", fOriginalIsFullScan ? 1 : 0); // restoring full scan original setting
    PedestalEqualization::Reset();
}
