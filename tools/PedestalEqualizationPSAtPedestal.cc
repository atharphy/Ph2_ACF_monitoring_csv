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
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^C[0-6]$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^ThDAC\\d$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^THTRIMMING_S\\d+$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^Bias_D5DAC8$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^Bias_THDAC$");

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

    std::cout << " FULL SCAN AMPLITUDE!" << std::endl;
    fTestPulseAmplitude    = 1; //findValueInSettings<double>("PedestalEqualization_PulseAmplitudeFullScan", 0);
    fTestPulseAmplitudePix = 1; //findValueInSettings<double>("PedestalEqualization_PulseAmplitudePixFullScan", 0);

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
    GetLowestAndHighestMaxOccupancyThreshold();
    FindTargetThreshold();
    TuneVtrim();
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
                        fReadoutChipInterface->WriteChipReg(cChip, "Bias_D5DAC8", 0x0);
        
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

    GetMaximumOccupancyThreshold(dacOccupancyContainers);


#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillSCurvePlotsVector(detectorContainerVector, dacList);
#else
    if(fDQMStreamerEnabled)
    {
        for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
        {
            ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalOccupancy");
            theContainerSerialization.streamByChipContainer(fDQMStreamer, *detectorContainerVector.at(dacIt), dacIt);        
        }
    }    
#endif

}


void PedestalEqualizationPSAtPedestal::GetMaximumOccupancyThreshold(const DetectorDataContainer& dacOccupancyContainers)
{

    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, fTheMaxOccupancyThresholdContainers);

    for(auto cBoard: dacOccupancyContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto theChipContainer = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    LOG(INFO) << BOLDBLUE << "Looking for DACmaxOccupancy for chip "<< cChip->getId() << RESET;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
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
                        }// col   
                    } // row
                } //chip
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillMaxPlots(fTheMaxOccupancyThresholdContainers);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalMax");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, fTheMaxOccupancyThresholdContainers);
    }
    
#endif

}
// uint16_t PedestalEqualizationPSAtPedestal::GetMean(ChipDataContainer theChip)
// {

// }


void PedestalEqualizationPSAtPedestal::GetLowestAndHighestMaxOccupancyThreshold()
{
    //((row, col), threshold)
    ContainerFactory::copyAndInitStructure<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>(*fDetectorContainer, fTheSmallestThresholdAtMaxOccupancyContainer);
    ContainerFactory::copyAndInitStructure<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>(*fDetectorContainer, fTheLargestThresholdAtMaxOccupancyContainer);

    for(auto cBoard: fTheMaxOccupancyThresholdContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    std::cout << "the chip " << cChip->getId() << std::endl;
                    uint16_t smallestThresholdAtMaxOccupancy = 255;
                    uint16_t largestThresholdAtMaxOccupancy = 0;
                    uint16_t rowAtSmallestThresholdAtMaxOccupancy = -1, colAtSmallestThresholdAtMaxOccupancy = -1, binAtSmallestThresholdAtMaxOccupancy = -1;
                    uint16_t rowAtLargestThresholdAtMaxOccupancy = -1, colAtLargestThresholdAtMaxOccupancy = -1, binAtLargestThresholdAtMaxOccupancy = -1;

                    // Get Mean
                    float theMean = 0.0;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t theMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row, col);
                            theMean += theMaxOccupancyThreshold;
                        }
                    }
                    theMean /= (cChip->getNumberOfRows() * cChip->getNumberOfCols());
                    std::cout << "Mean Threshold for max occupancy: " << theMean << std::endl;

                    // Get std
                    float stddev = 0.0;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t theMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row, col);
                            stddev += (theMaxOccupancyThreshold - theMean) * (theMaxOccupancyThreshold - theMean);
                        }
                    }                    
                    stddev = std::sqrt(stddev / (cChip->getNumberOfRows() * cChip->getNumberOfCols()));
                    std::cout << "Standard deviation: " << stddev << std::endl;

                    // Define 2σ range
                    float lowerBound = theMean - 2 * stddev;
                    float upperBound = theMean + 2 * stddev;
                
                    std::cout << "2σ range: [" << lowerBound << ", " << upperBound << "]" << std::endl;

                    // Get max and min channel
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t bin = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            uint16_t theMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row, col);
                            
                            if (theMaxOccupancyThreshold < smallestThresholdAtMaxOccupancy && theMaxOccupancyThreshold > lowerBound && theMaxOccupancyThreshold < upperBound)
                            {
                                smallestThresholdAtMaxOccupancy = theMaxOccupancyThreshold;
                                rowAtSmallestThresholdAtMaxOccupancy = row;
                                colAtSmallestThresholdAtMaxOccupancy = col;
                                binAtSmallestThresholdAtMaxOccupancy = bin;
                            }


                            if (theMaxOccupancyThreshold > largestThresholdAtMaxOccupancy && theMaxOccupancyThreshold < upperBound && theMaxOccupancyThreshold > lowerBound)
                            {
                                largestThresholdAtMaxOccupancy = theMaxOccupancyThreshold;
                                rowAtLargestThresholdAtMaxOccupancy = row;
                                colAtLargestThresholdAtMaxOccupancy = col;
                                binAtLargestThresholdAtMaxOccupancy = bin;
                            }
                        } // col
                    } // row

                    LOG(INFO) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << rowAtSmallestThresholdAtMaxOccupancy << " col " << colAtSmallestThresholdAtMaxOccupancy << " (bin "<< binAtSmallestThresholdAtMaxOccupancy << ") has the lowest DAC giving the maximum occupancy at " << smallestThresholdAtMaxOccupancy << RESET;
                    LOG(INFO) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << rowAtLargestThresholdAtMaxOccupancy << " col " << colAtLargestThresholdAtMaxOccupancy << " (bin "<< binAtLargestThresholdAtMaxOccupancy << ") has the highest DAC giving the maximum occupancy at " << largestThresholdAtMaxOccupancy << RESET;
                    auto theLowestThreshold = std::make_pair(std::make_pair(rowAtSmallestThresholdAtMaxOccupancy, colAtSmallestThresholdAtMaxOccupancy), smallestThresholdAtMaxOccupancy);
                    auto theChipSmallestThresholdContainer = fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>() = theLowestThreshold;
                    auto theHighestThreshold = std::make_pair(std::make_pair(rowAtLargestThresholdAtMaxOccupancy, colAtLargestThresholdAtMaxOccupancy), largestThresholdAtMaxOccupancy);
                    auto theChipLargestThresholdContainer = fTheLargestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipLargestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>() = theHighestThreshold;
    
                } //chip
            } // hybrid
        } // OG
    } // board
}

void PedestalEqualizationPSAtPedestal::FindTargetThreshold()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;
    ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fTheTargetThresholdContainers);

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
                        fReadoutChipInterface->WriteChipReg(cChip, "TrimDAC_ALL", 0x0);
                    }
                    else //SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "THTRIMMING", 0x0);        
                    }
                }
            }
        }
    }
    ScanThreshold();

    for(auto cBoard: fTheMaxOccupancyThresholdContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto theChipLargestThresholdContainer = fTheLargestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto theLargestThreshold = theChipLargestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();

                    auto row = theLargestThreshold.first.first;
                    auto col = theLargestThreshold.first.second;
                    uint16_t theTargetTreshold = cChip->getChannel<uint16_t>(row, col);
                    LOG(INFO) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << row << " col " << col << " has the maximum occupancy at " << theTargetTreshold << " before was at "<< theLargestThreshold.second << RESET;
                    auto theChipTargetThresholdContainer = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipTargetThresholdContainer->getSummary<uint16_t>() = theTargetTreshold;

                }
            }
        }
    }
}

void PedestalEqualizationPSAtPedestal::TuneVtrim()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;
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
                        fReadoutChipInterface->WriteChipReg(cChip, "TrimDAC_ALL", 0x1F);
                    }
                    else //SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "THTRIMMING", 0x1F);        
                    }
                }
            }
        }
    }
    ScanThreshold();
    LOG(INFO) << BOLDMAGENTA << " Scan Vtrim " << RESET;

    
    for(uint16_t vtrim = 0; vtrim <=31; vtrim++)
    {
        for(auto cBoard: fTheMaxOccupancyThresholdContainers)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto theChipSmallestThresholdContainer = fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto theSmallestThreshold = theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();
                        auto theTargetThreshold = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        auto row = theSmallestThreshold.first.first;
                        auto col = theSmallestThreshold.first.second;

                        auto threshold = theSmallestThreshold.second;
                        auto currentThreshold = cChip->getChannel<uint16_t>(row,col);
                        //if(vtrim == 0 ) theThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() = threshold;
                        LOG(INFO) << BOLDGREEN << "For iteration " << vtrim << " at chip " <<cChip->getId() << " the initial threshold is " << threshold << " current "<< currentThreshold << " and target is " << theTargetThreshold << RESET;
                        
                        
                        ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto     cType = theReadoutChip->getFrontEndType();
                        if(currentThreshold < theTargetThreshold) //FIXME - need to improve precision here!
                        {
                            if(cType == FrontEndType::MPA2)
                            {

                                // Vtrim
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C0", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C1", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C2", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C3", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C4", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C5", vtrim);
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "C6", vtrim);
                            }
                            else //SSA
                            {
                                fReadoutChipInterface->WriteChipReg(theReadoutChip, "Bias_D5DAC8", vtrim);
                            }
                        }
                    } //chip
                }
            }
        }
        std::cout<< " run scan threshold for iteration "<< vtrim << std::endl;
        ScanThreshold();
        std::cout<< " done " << std::endl;
    
    } // iteration
    

        
    std::cout << " close all loops" << std::endl;
}


// uint8_t PedestalEqualizationPSAtPedestal::GenericTune(ReadoutChip* theChip, float theExpectedValue, std::string theDACtoTuneName, uint8_t theDACValue, bool isPerChannel)
// {
//     LOG(INFO) << CYAN << "Register being tuned: " << theDACtoTuneName << RESET;

//     // write DAC (ie one of the registers) with value 0 (minimum)
//     uint8_t theDACMinValue = 0x00;
//     fReadoutChipInterface->WriteChipReg(theChip, theDACtoTuneName, theDACMinValue);
//     ScanThreshold();
//     uint32_t theOffsetValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);
//     LOG(DEBUG) << MAGENTA << "The register for " << theDACtoTuneName << " at " << +theDACMinValue << "  gives theOffsetValue " << theOffsetValue << " [ADC]" << RESET;

//     // now set the DAC value to its max value
//     uint8_t theDACMaxValue = 0x1F;
//     if(isVref)
//         this->setVref(theChip, theDACMaxValue);
//     else
//         this->WriteChipReg(theChip, theDACtoTuneName, theDACMaxValue);
//     std::this_thread::sleep_for(std::chrono::milliseconds(2));
//     uint32_t theMaxValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);
//     LOG(DEBUG) << MAGENTA << "The register for " << theDACtoTuneName << " at " << +theDACMaxValue << " gives theMaxValue " << theMaxValue << " [ADC]" << RESET;

//     float theLSB = abs(float(theMaxValue) - float(theOffsetValue)) / float(theDACMaxValue);
//     LOG(DEBUG) << BOLDMAGENTA << " abs(float(theMaxValue) - float(theOffsetValue)) " << abs(float(theMaxValue) - float(theOffsetValue)) << " float(theDACMaxValue) " << float(theDACMaxValue) << RESET;
//     LOG(DEBUG) << BLUE << theDACtoTuneName << " LSB " << theLSB << RESET;

//     float theADCDExpectedValue = 0.0;
//     theADCDExpectedValue       = theExpectedValue / theSlope + theGroundADCValue; // converted from volts to ADC
//     LOG(DEBUG) << MAGENTA << "The register for " << theDACtoTuneName << " expected value in ADC " << theADCDExpectedValue << " [ADC]" << RESET;

//     // now set the DAC value to its default value and get the value at the default value
//     if(isVref)
//         this->setVref(theChip, theDACValue);
//     else
//         this->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
//     std::this_thread::sleep_for(std::chrono::milliseconds(2));
//     uint32_t theCurrentValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);
//     LOG(INFO) << MAGENTA << "The register to tune at nominal value " << +theDACValue << " gives theCurrentValue " << theCurrentValue << " [ADC]" << RESET;

//     int theStepSign = 0;
//     if(theADCDExpectedValue < theCurrentValue)
//         theStepSign = (isVref) ? 1 : -1;
//     else
//         theStepSign = (isVref) ? -1 : 1;

//     uint8_t theSteps       = uint8_t(std::round(abs(float(theADCDExpectedValue) - float(theCurrentValue)) / float(theLSB)));
//     uint8_t theDACNewValue = 0;
//     if(float(theDACValue + theStepSign * theSteps) > theDACMaxValue)
//         theDACNewValue = theDACMaxValue;
//     else if((float(theDACValue + theStepSign * theSteps) < theDACMinValue))
//         theDACNewValue = theDACMinValue;
//     else
//         theDACNewValue = theDACValue + theStepSign * theSteps;

//     theDACValue = theDACNewValue;

//     LOG(DEBUG) << MAGENTA << "Predicted number of register steps to get the expected value " << +theSteps << " giving the new register value of " << +theDACNewValue << RESET;

//     // now writing the DAC to the new value estimated above
//     if(isVref)
//         this->setVref(theChip, theDACNewValue);
//     else
//         this->WriteChipReg(theChip, theDACtoTuneName, theDACNewValue);
//     std::this_thread::sleep_for(std::chrono::milliseconds(2));

//     uint32_t theNewValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);
//     LOG(DEBUG) << MAGENTA << "After changing value for DAC " << theDACtoTuneName << " to " << +theDACValue << " the ADC value is " << theNewValue << " [ADC]" << RESET;

//     // Now checking if we can go even closer to the expected value
//     bool     isSearching         = true;
//     uint32_t theCurrentIteration = 0;
//     while(isSearching)
//     {
//         LOG(DEBUG) << YELLOW << "Checking if we can go closer to the expected value. Iteration " << theCurrentIteration << RESET;
//         LOG(DEBUG) << MAGENTA << " theDACNewValue - 1 " << +theDACNewValue - 1 << RESET;
//         uint8_t theDACDownValue = std::max(theDACMinValue, uint8_t(theDACNewValue - 1));
//         if(isVref)
//             this->setVref(theChip, theDACDownValue);
//         else
//             this->WriteChipReg(theChip, theDACtoTuneName, theDACDownValue);
//         std::this_thread::sleep_for(std::chrono::milliseconds(2));
//         uint32_t theNewValueDown = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);

//         uint8_t theDACUpValue = std::min(uint8_t(theDACMaxValue), uint8_t(theDACNewValue + 1));
//         LOG(DEBUG) << MAGENTA << "theDACUpValue " << +theDACUpValue << RESET;
//         if(isVref)
//             this->setVref(theChip, theDACUpValue);
//         else
//             this->WriteChipReg(theChip, theDACtoTuneName, theDACUpValue);
//         std::this_thread::sleep_for(std::chrono::milliseconds(2));
//         uint32_t theNewValueUp = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);

//         float theExpectedDifference     = std::fabs(theADCDExpectedValue - theNewValue);
//         float theExpectedDifferenceDown = std::fabs(theADCDExpectedValue - theNewValueDown);
//         float theExpectedDifferenceUp   = std::fabs(theADCDExpectedValue - theNewValueUp);

//         if((theExpectedDifferenceDown < theExpectedDifference) || (theExpectedDifferenceUp < theExpectedDifference))
//         {
//             LOG(DEBUG) << BOLDRED << "Not precise extrapolation in OTPSADCCalibration: theExpectedDifferenceDown:" << theExpectedDifferenceDown
//                        << ", theExpectedDifferenceUp:" << theExpectedDifferenceUp << ", theExpectedDifference:" << theExpectedDifference << RESET;
//             if((theExpectedDifferenceDown < theExpectedDifference))
//             {
//                 theDACValue    = theDACDownValue;
//                 theDACNewValue = theDACNewValue - 1;
//             }
//             if((theExpectedDifferenceUp < theExpectedDifference))
//             {
//                 theDACValue    = theDACUpValue;
//                 theDACNewValue = std::min(uint8_t(theDACMaxValue), uint8_t(theDACNewValue + 1));
//             }
//         }
//         else
//         {
//             LOG(DEBUG) << BOLDGREEN << "Good extrapolation in OTPSADCCalibration for register value " << +theDACValue << RESET;
//             if(isVref)
//                 this->setVref(theChip, theDACValue);
//             else
//                 this->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
//             std::this_thread::sleep_for(std::chrono::milliseconds(2));

//             uint32_t theCheckValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);
//             LOG(DEBUG) << BOLDGREEN << "Register " << theDACtoTuneName << " gives ADC " << theCheckValue << RESET;
//             isSearching = false;
//         }
//         LOG(DEBUG) << BOLDMAGENTA << "Writing DAC val " << +theDACValue << RESET;
//         if(isVref)
//             this->setVref(theChip, theDACValue);
//         else
//             this->WriteChipReg(theChip, theDACtoTuneName, theDACValue);
//         std::this_thread::sleep_for(std::chrono::milliseconds(2));

//         theNewValue = isVref == 0 ? this->readADC(theChip, theDACtoTuneName, 1) : this->readADCBandGap(theChip);

//         theCurrentIteration += 1;
//     }

//     LOG(INFO) << BOLDGREEN << "Register: " << theDACtoTuneName << " -> New tuned value: " << theNewValue << " Expected value: " << theADCDExpectedValue << "+/-" << theLSB << RESET;

//     return theDACValue;
// }


void PedestalEqualizationPSAtPedestal::TuneTrimBits()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;
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
                        fReadoutChipInterface->WriteChipReg(cChip, "TrimDAC_ALL", 0x1F);
                    }
                    else //SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "THTRIMMING", 0x1F);        
                    }
                }
            }
        }
    }


    ScanThreshold();


    LOG(INFO) << BOLDMAGENTA << " Scan Trim Bit " << RESET;
    int totalIterations = 31;
    int iteration = totalIterations;
    while (iteration > 0)
    {
        std::cout << " iteration " << iteration << std::endl;
        for(auto cBoard: fTheMaxOccupancyThresholdContainers)
        {
            std::cout << " board " << cBoard->getId() << std::endl;
            for(auto cOpticalGroup: *cBoard)
            {
                std::cout << " cOpticalGroup " << cOpticalGroup->getId() << std::endl;
                for(auto cHybrid: *cOpticalGroup)
                {
                    std::cout << " cHybrid " << cHybrid->getId() << std::endl;
                    for(auto cChip: *cHybrid)
                    {
                        std::cout << " cChip " << cChip->getId() << std::endl;
                        auto theTargetThreshold = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();

 //                       auto theChipContainer = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        std::cout << " target threshold got "<< theTargetThreshold << std::endl;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            // std::cout << " row "<< row << std::endl;
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            { 
                                // std::cout << " col "<< col << std::endl;
                                // if(iteration == totalIterations ) theFirstSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>() = cChip->getChannel<uint16_t>(row, col);
                                // std::cout << " theFirstSmallestThresholdAtMaxOccupancyContainer " << std::endl;        
                                // uint16_t smallestThresholdAtMaxOccupancy = theFirstSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                                //LOG(INFO) << BOLDGREEN << "For iteration " << iteration << " at chip " <<cChip->getId() << " the current max occupancy is at  " << cChip->getChannel<uint16_t>(row, col) << RESET;
    
                                if (theTargetThreshold >  cChip->getChannel<uint16_t>(row, col))
                                {
                                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                                    auto     cType = theReadoutChip->getFrontEndType();
                                    if(cType == FrontEndType::MPA2)
                                    {
                                        std::string cRegName = "TrimDAC_C" + std::to_string(col) + "_R" + std::to_string(row);
                                        auto currentTrim = fReadoutChipInterface->ReadChipReg(theReadoutChip,cRegName);
                                        auto newTrim = currentTrim -1;
                                        if(col == 5 && row == 2)std::cout << "MPA trim " << cRegName << " 0x"<< std::hex << currentTrim << " -1 0x"<<  newTrim << std::dec << std::endl;
                                    
                                        fReadoutChipInterface->WriteChipReg(theReadoutChip, cRegName, newTrim );
                                    }
                                    else //SSA
                                    {
                                        std::string cRegName = "THTRIMMING_S" + std::to_string(col+1);
                                        auto currentTrim = fReadoutChipInterface->ReadChipReg(theReadoutChip,cRegName);
                                        auto newTrim = currentTrim -1;
                                        if(col +1 == 5) std::cout << "SSA trim " << cRegName << " 0x"<< std::hex << currentTrim << " -1 0x"<<  newTrim << std::dec << std::endl;
                                    
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
