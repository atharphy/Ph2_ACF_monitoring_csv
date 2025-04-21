#include "tools/PedestalEqualizationPSAtPedestal.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cPSCounterFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string PedestalEqualizationPSAtPedestal::fCalibrationDescription = "Equalize the pedestal/threshold for all channels with higher precision near the pedestal";
// This class should do the same as the PedestalEqualization with PS FullScan but do the MPA in a different way!!!!
// I will have to copy stuff from PedestalEqualization because of the SSA (cannot have fast counter without MPA!), but then do things differently for MPA!
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

    // fSkipMaskedChannels                = findValueInSettings<double>("SkipMaskedChannels", 0);
    // fMaskChannelsFromOtherGroups       = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    // fCheckLoop                         = findValueInSettings<double>("VerificationLoop", 1);
    // fPedestalEqualizationMaskUntrimmed = findValueInSettings<double>("PedestalEqualization_MaskUntrimmed", 0);

    // FOR SSA Full Scan
    fOriginalIsFullScan                = findValueInSettings<double>("FullScan", 0) > 0;
    setValueInSettings<double>("FullScan", 1);

    // To use full scan
    // fOriginalUseFixRange = findValueInSettings<double>("PedeNoise_UseFixRange", 0) > 0;
    // setValueInSettings<double>("PedeNoise_UseFixRange", 1);
    // fOriginalMinThreshold = findValueInSettings<double>("PedeNoise_MinThreshold", 0);
    // setValueInSettings<double>("PedeNoise_MinThreshold", 0);
    // fOriginalMaxThreshold = findValueInSettings<double>("PedeNoise_MaxThreshold", 0);
    // setValueInSettings<double>("PedeNoise_MaxThreshold", 254);
    
    fPedestalEqualizationFullScanStart = findValueInSettings<double>("PedestalEqualization_FullScanStart", 110);
    fPedestalEqualizationFullScanCAP   = findValueInSettings<double>("PedestalEqualizationFullScanCAP", 1.0);

    fTestPulseAmplitude    = findValueInSettings<double>("PedestalEqualization_PulseAmplitude", 0);
    fTestPulseAmplitudePix = findValueInSettings<double>("PedestalEqualization_PulseAmplitudePix", fTestPulseAmplitude);

    if(fFullScan)
    {
        std::cout << " FULL SCAN AMPLITUDE!" << std::endl;
        fTestPulseAmplitude    = 1; //findValueInSettings<double>("PedestalEqualization_PulseAmplitudeFullScan", 0);
        fTestPulseAmplitudePix = 1; //findValueInSettings<double>("PedestalEqualization_PulseAmplitudePixFullScan", 0);
    }

    fEventsPerPoint          = findValueInSettings<double>("Nevents", 10);
    fNEventsPerBurst         = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    // fOccupancyAtPedestal     = findValueInSettings<double>("PedestalEqualization_Occupancy", 0.56);
    uint8_t cDefTargetOffset = 0xF;
    fTargetOffset            = findValueInSettings<double>("PedestalEqualizationTargetOffset", cDefTargetOffset);
    bool fastCounterReadout  = findValueInSettings<double>("PedestalEqualization_FastCounterReadout", 1) > 0;
    // LOG(INFO) << BOLDBLUE << "PedestalEqualization::Initialise Occupancy at pedestal is " << fOccupancyAtPedestal << " target offset is " << +fTargetOffset << RESET;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;


    fStopValue = 255;
    fStartValue = 0;
    const size_t nSteps = fStopValue - fStartValue + 1;
    for(auto i = 0u; i < nSteps; i++)
    {
        dacList.push_back(fStartValue + i);
    }

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramPedestalEqualizationPSAtPedestal.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
    // event types
    ContainerFactory::copyAndInitBoard<EventType>(*fDetectorContainer, fEventTypes);
    bool cForcePSasync = true;
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.getObject(cBoard->getId())->getSummary<EventType>() = cBoard->getEventType();
        if(!fWithSSA && !fWithMPA) continue;
        if(!cForcePSasync) continue;
        cBoard->setEventType(EventType::PSAS);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->InitializePSCounterFWInterface(cBoard);
        static_cast<D19cPSCounterFWInterface*>(static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->getL1ReadoutInterface())->configureFastReadout(fastCounterReadout);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1); }
            }
        }
    }
}

void PedestalEqualizationPSAtPedestal::ConfigureCalibration()
{

}

void PedestalEqualizationPSAtPedestal::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;
    
    // std::function<bool(const ChipContainer*)>        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::SSA2); };
    // std::string selectSSAfunctionName = "SelectSSAfunctionPS";

    // std::function<bool(const ChipContainer*)>        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() == FrontEndType::MPA2); };
    // std::string selectMPAfunctionName = "SelectMPAfunctionPS";

    LOG(INFO) << BOLDMAGENTA <<  "Starting PedestalEqualizationPSAtPedestal measurement." << RESET;
    Initialise();
    PrepareForInjection();
    ScanThreshold();
    TuneTrimBits();
    LOG(INFO) << BOLDMAGENTA <<  "Done with PedestalEqualizationPSAtPedestal." << RESET;


}
void PedestalEqualizationPSAtPedestal::PrepareForInjection()
{


    // figure  out if you should normalize or not
    uint cNormalize = 1;
    setNormalization(cNormalize);

    this->enableTestPulse(true);
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto cType = cChip->getFrontEndType();
                    if(cType == FrontEndType::MPA2)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitudePix);
                        fReadoutChipInterface->WriteChipReg(cChip, "TrimDAC_ALL", 0x1F);

                        // Vtrim
                        fReadoutChipInterface->WriteChipReg(cChip, "C0", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C1", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C2", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C3", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C4", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C5", 0x0);
                        fReadoutChipInterface->WriteChipReg(cChip, "C6", 0x0);

                    }
                    else //SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitude);
                        fReadoutChipInterface->WriteChipReg(cChip, "THTRIMMING", 0x1F);
                        fReadoutChipInterface->WriteChipReg(cChip, "Bias_D5DAC8", 0x00);
        
                    }
                }
            }
        }

    }
    LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    this->setTestAllChannels(true);

}

void PedestalEqualizationPSAtPedestal::ScanThreshold()
{
    std::vector<DetectorDataContainer> detectorContainerVector(dacList.size());
    std::vector<DetectorDataContainer*> detectorContainerVectorPointers;
    for(auto& container: detectorContainerVector)
    {
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, container);
        detectorContainerVectorPointers.push_back(&container);
    }
    this->scanDac("Threshold", dacList, fEventsPerPoint,detectorContainerVectorPointers, fNEventsPerBurst);
    DetectorDataContainer dacOccupancyContainers;
    ContainerFactory::copyAndInitChannel<std::map<uint16_t, float>>(*fDetectorContainer, dacOccupancyContainers);

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        for(auto cBoard: detectorContainerVector.at(dacIt))
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto theChipContainer = detectorContainerVector.at(dacIt).getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        if(theChipContainer->hasChannelContainer() == false) continue;

                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto targetMap = &(dacOccupancyContainers
                                    .getObject(cBoard->getId())
                                    ->getObject(cOpticalGroup->getId())
                                    ->getObject(cHybrid->getId())
                                    ->getObject(cChip->getId())
                                    ->getChannel<std::map<uint16_t, float>>(row, col));
                                
                                (*targetMap)[dacIt] = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
                            } //col
                        } //row
                    } // chip
                } // hybrid
            } //optical group
        } // board
    } // dac 

    GetMaximumDAC(dacOccupancyContainers);


#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillSCurvePlots(detectorContainerVector, dacList);
#else
    // if(fDQMStreamerEnabled)
    // {
    //         ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalOccupancy");
    //         theContainerSerialization.streamByHybridContainer(fDQMStreamer, detectorContainerVector, dacList);
    // }
    
#endif

}


void PedestalEqualizationPSAtPedestal::GetMaximumDAC(const DetectorDataContainer& dacOccupancyContainers)
{

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, fTheMaxOccupancyDACContainers);
    ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fTheSmallestThresholdAtMaxOccupancyContainer);
    LOG(INFO) << BOLDBLUE << " containers " << RESET;

    for(auto cBoard: dacOccupancyContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {

                    auto theChipContainer = fTheMaxOccupancyDACContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    uint16_t smallestThresholdAtMaxOccupancy = 255;
                    uint16_t rowAtsmallestThresholdAtMaxOccupancy = -1, colAtsmallestThresholdAtMaxOccupancy = -1;
                    uint16_t binAtsmallestThresholdAtMaxOccupancy = -1;
                    LOG(INFO) << BOLDBLUE << "Looking for DACmaxOccupancy for chip "<< cChip->getId() << RESET;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t bin = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            const auto& occupancyMap = cChip->getChannel<std::map<uint16_t, float>>(row, col);
                            if (occupancyMap.empty()) continue;

                            auto maxIter = std::max_element(
                            occupancyMap.begin(),
                            occupancyMap.end(),
                            [](const auto& a, const auto& b) {
                                return a.second < b.second;
                            });
                            uint16_t DACmaxOccupancy = maxIter->first;
                            theChipContainer->getChannel<uint16_t>(row, col) = DACmaxOccupancy;
                            
                            if (DACmaxOccupancy < smallestThresholdAtMaxOccupancy && DACmaxOccupancy > 0)
                            {
                                smallestThresholdAtMaxOccupancy = DACmaxOccupancy;
                                rowAtsmallestThresholdAtMaxOccupancy = row;
                                colAtsmallestThresholdAtMaxOccupancy = col;
                                binAtsmallestThresholdAtMaxOccupancy = bin;
                            }   
                        }// col   
                    } // row
                    auto theChipSmallestThresholdContainer = fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipSmallestThresholdContainer->getSummary<uint16_t>() = smallestThresholdAtMaxOccupancy;
                    LOG(INFO) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << rowAtsmallestThresholdAtMaxOccupancy << " col " << colAtsmallestThresholdAtMaxOccupancy << " (bin "<< binAtsmallestThresholdAtMaxOccupancy << ") has the lowest DAC giving the maximum occupancy at " << smallestThresholdAtMaxOccupancy << RESET;
                } //chip
            }
        }
    }


#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillMaxPlots(fTheMaxOccupancyDACContainers);
#else
    // if(fDQMStreamerEnabled)
    // {
    //         ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalOccupancy");
    //         theContainerSerialization.streamByHybridContainer(fDQMStreamer, detectorContainerVector, dacList);
    // }
    
#endif

}

void PedestalEqualizationPSAtPedestal::TuneTrimBits()
{
    int iteration = 31;
    while (iteration > 0)
    {
        for(auto cBoard: fTheSmallestThresholdAtMaxOccupancyContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto theChipContainer = fTheMaxOccupancyDACContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        uint16_t smallestThresholdAtMaxOccupancy = cChip->getSummary<uint16_t>();
                        std::cout << " the smallestThresholdAtMaxOccupancy " << smallestThresholdAtMaxOccupancy << std::endl;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            { 
                                if (theChipContainer->getChannel<uint16_t>(row, col) >  smallestThresholdAtMaxOccupancy)
                                {
                                    std::cout<< " while loop " << std::endl;
                                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                                    std::cout<< " theReadoutChip " << std::endl;
                                    auto     cType = theReadoutChip->getFrontEndType();
                                    std::cout<< " type " << std::endl;
                                    if(cType == FrontEndType::MPA2)
                                    {
                                        std::cout<< " MPA " << std::endl;
                                        std::string cRegName = "TrimDAC_C" + std::to_string(col) + "_R" + std::to_string(row);
                                        auto currentTrim = fReadoutChipInterface->ReadChipReg(theReadoutChip,cRegName);
                                        auto newTrim = currentTrim -1;
                                        std::cout << " trim " << cRegName << " 0x"<< std::hex << currentTrim << " -1 0x"<<  newTrim << std::dec << std::endl;
                                    
                                        fReadoutChipInterface->WriteChipReg(theReadoutChip, cRegName, newTrim );
                                    }
                                    else //SSA
                                    {
                                        std::cout<< " SSA " << std::endl;
                                        std::string cRegName = "THTRIMMING_S" + std::to_string(col+1);
                                        std::cout << " trim " << cRegName << std::endl;
                                        auto currentTrim = fReadoutChipInterface->ReadChipReg(theReadoutChip,cRegName);
                                        std::cout << " read done " <<std::endl;
                                        auto newTrim = currentTrim -1;
                                        std::cout << " trim " << cRegName << " 0x"<< std::hex << currentTrim << " -1 0x"<<  newTrim << std::dec << std::endl;
                                    
                                        fReadoutChipInterface->WriteChipReg(theReadoutChip, cRegName, newTrim );
                                    }

                                }

                            }// col
                        } // row
                    } //chip
                }
            }
        }
        std::cout<< " run scan threshold for iteration "<< iteration << std::endl;
        ScanThreshold();
        iteration--;
        std::cout<< " done " << std::endl;
    }// iteration
    std::cout << " close all loops" << std::endl;
}
void PedestalEqualizationPSAtPedestal::Stop(void)
{
    LOG(INFO) << "Stopping PedestalEqualizationPSAtPedestal measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramPedestalEqualizationPSAtPedestal.process();
    #endif
    SaveResults();
    // writeObjects();
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
    // setValueInSettings<double>("PedeNoise_UseFixRange", fOriginalUseFixRange ? 1 : 0);
    // setValueInSettings<double>("PedeNoise_MinThreshold", fOriginalMinThreshold);
    // setValueInSettings<double>("PedeNoise_MinThreshold", fOriginalMaxThreshold);
    PedestalEqualization::Reset();
}
