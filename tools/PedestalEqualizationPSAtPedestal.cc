#include "tools/PedestalEqualizationPSAtPedestal.h"
#include "tools/Tool.h"
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


    fStopValue = 150;
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
    Initialise(false);
    PrepareForInjection();
    ScanThreshold();
    GetLowestAndHighestMaxOccupancyThreshold();
    FindTargetThreshold();
    TuneVtrim();
    // TuneTrimBits();
    TuneTrimBitsBinary();
    SetTargetThreshold();
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
                    
                    uint16_t VtrimForMaxRange;
                    auto cType = cChip->getFrontEndType();
                    if(cType == FrontEndType::MPA2)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitudePix);
                        VtrimForMaxRange = 0x0;
                    }
                    else //SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitude);
                        VtrimForMaxRange = 0x1F;
                    }
                    fReadoutChipInterface->SetTrimBitsAll(cChip,0x1F);
                    fReadoutChipInterface->SetVtrim(cChip,VtrimForMaxRange);
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
                        //auto theChipContainer = detectorContainerVector.at(dacIt).getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        if(cChip->hasChannelContainer() == false) continue;

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
                                
                                (*targetMap)[dacIt] = cChip->getChannel<Occupancy>(row, col).fOccupancy;
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


void PedestalEqualizationPSAtPedestal::FillMaxOccupancyMap(std::vector<DetectorDataContainer> detectorContainerVector, DetectorDataContainer& dacOccupancyContainers, uint16_t boardId,uint16_t OGId, uint16_t hybridId,uint16_t ChipId)
{
    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        auto theChipContainer = detectorContainerVector.at(dacIt).getObject(boardId)->getObject(OGId)->getObject(OGId)->getObject(ChipId);
        if(theChipContainer->hasChannelContainer() == false) continue;

        for(uint16_t row = 0; row < theChipContainer->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < theChipContainer->getNumberOfCols(); ++col)
            {
                auto targetMap = &(dacOccupancyContainers
                    .getObject(boardId)
                    ->getObject(OGId)
                    ->getObject(OGId)
                    ->getObject(ChipId)
                    ->getChannel<std::map<uint16_t, float>>(row, col));
                
                (*targetMap)[dacIt] = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
            } //col
        } //row
    }
}

void PedestalEqualizationPSAtPedestal::ScanThresholdChip(uint16_t boardId,uint16_t OGId, uint16_t hybridId,uint16_t ChipId)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    std::vector<DetectorDataContainer> detectorContainerVector(dacList.size());
    std::vector<DetectorDataContainer*> detectorContainerVectorPointers;
    for(auto& container: detectorContainerVector)
    {
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, container);
        detectorContainerVectorPointers.push_back(&container);
    }

    this->scanDacChip("Threshold", dacList, fEventsPerPoint,detectorContainerVectorPointers, fNEventsPerBurst, boardId,OGId,hybridId,ChipId);
    DetectorDataContainer dacOccupancyContainers;
    ContainerFactory::copyAndInitChannel<std::map<uint16_t, float>>(*fDetectorContainer, dacOccupancyContainers);

    // FillMaxOccupancyMap(detectorContainerVector, dacOccupancyContainers, boardId, OGId, hybridId, ChipId);

    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        auto theChipContainer = detectorContainerVector.at(dacIt).getObject(boardId)->getObject(OGId)->getObject(hybridId)->getObject(ChipId);
        //if(theChipContainer->hasChannelContainer() == false) continue;
        for(uint16_t row = 0; row < theChipContainer->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < theChipContainer->getNumberOfCols(); ++col)
            {
                
                auto targetMap = &(dacOccupancyContainers
                    .getObject(boardId)
                    ->getObject(OGId)
                    ->getObject(hybridId)
                    ->getObject(ChipId)
                    ->getChannel<std::map<uint16_t, float>>(row, col));
                
                (*targetMap)[dacIt] = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;

                // auto theChipSmallestThresholdContainer = fTheSmallestThresholdAtMaxOccupancyContainer.getObject(boardId)->getObject(OGId)->getObject(hybridId)->getObject(ChipId);
                // auto theSmallestThreshold = theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();
                // if (row ==  theSmallestThreshold.first.first && col == theSmallestThreshold.first.second) std::cout << "Target Occupancy " << theChipContainer->getChannel<Occupancy>(row, col).fOccupancy << std::endl;
                // if (row ==  0 && col == 3) std::cout << "(0, 3) Occupancy " << theChipContainer->getChannel<Occupancy>(row, col).fOccupancy << std::endl;
    
            } //col
        } //row
    }
    auto theChipContainer = fTheMaxOccupancyThresholdContainers.getObject(boardId)->getObject(OGId)->getObject(hybridId)->getObject(ChipId);

    //if(theChipContainer->hasChannelContainer() == false) continue;
    for(uint16_t row = 0; row < theChipContainer->getNumberOfRows(); ++row)
    {
        for(uint16_t col = 0; col < theChipContainer->getNumberOfCols(); ++col)
        {
            const auto& occupancyMap = dacOccupancyContainers.getObject(boardId)->getObject(OGId)->getObject(hybridId)->getObject(ChipId)->getChannel<std::map<uint16_t, float>>(row, col);
            if (occupancyMap.empty()) continue;

            auto maxIter = std::max_element(
            occupancyMap.begin(),
            occupancyMap.end(),
            [](const auto& a, const auto& b) {
                return a.second < b.second;
            });
            uint16_t DACmaxOccupancy = maxIter->first;
            theChipContainer->getChannel<uint16_t>(row, col) = DACmaxOccupancy;
        } //col
    } //row
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
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
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
                    float lowerBound = std::max(float(1), float(theMean - 2 * stddev));
                    float upperBound = std::min(float(255), float(theMean + 2 * stddev));
                
                    std::cout << "2σ range: [" << lowerBound << ", " << upperBound << "]" << std::endl;

                    // Get max and min channel
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t bin = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            uint16_t theMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row, col);
                            if(theMaxOccupancyThreshold < lowerBound) std::cout << " Outlier LOW " << theMaxOccupancyThreshold <<" channel row " <<  row << " col " << col << " bin " << bin << std::endl;
                            if(theMaxOccupancyThreshold > upperBound)
                            {
                                std::cout << " Outlier HIGH " << theMaxOccupancyThreshold <<" channel row " <<  row << " col " << col << " bin " << bin << std::endl;
                                
                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,0x00, row, col);
                            }
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

void PedestalEqualizationPSAtPedestal::SetTargetThreshold()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    for(auto cBoard: fTheTargetThresholdContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    fReadoutChipInterface->WriteChipReg(theReadoutChip, "Threshold", cChip->getSummary<uint16_t>());
                }
            }
        }
    }
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
                    fReadoutChipInterface->SetTrimBitsAll(cChip, 0x0);
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
    LOG(INFO) << BOLDMAGENTA << " Scan Vtrim " << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {

                    fReadoutChipInterface->SetTrimBitsAll(cChip, 0x1F);
                    auto theChipSmallestThresholdContainer = fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto theSmallestThreshold = theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();
                    auto theTargetThreshold = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                    LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " target threshold " << theTargetThreshold << RESET;
                    auto row = theSmallestThreshold.first.first;
                    auto col = theSmallestThreshold.first.second;
                    
                    auto theMinimumThreshold = theSmallestThreshold.second;
                    LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " minimum threshold at minimum Vtrim " << theMinimumThreshold << RESET;
                    //now find maximum threshold
                    uint16_t theMaxVtrim = 0x1F;
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    fReadoutChipInterface->SetVtrim(theReadoutChip, theMaxVtrim);
                    ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                    std::cout << " getting threhsold fTheMaxOccupancyThresholdContainers row, col " << fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col) << std::endl;
                    std::cout << " getting threhsold fTheMaxOccupancyThresholdContainers 0, 3 " << fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(0,3) << std::endl;
                    auto theMaximumThreshold =  fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                    LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " maximum threshold at maximum Vtrim " << theMaximumThreshold << RESET;

                    if(theMaximumThreshold < theTargetThreshold || theTargetThreshold < theMinimumThreshold) 
                    {
                        LOG(INFO) << BOLDRED << " theMaximumThreshold < theTargetThreshold || theTargetThreshold < theMinimumThreshold -> skip Vtrim tuning "  << RESET;
                        continue;
                    }

                    // Assume linear relationship for change in Vtrim and change in the threshold giving max occupancy
                    float theVtrimStepSize = (float(theMaximumThreshold) - float(theMinimumThreshold))/float(theMaxVtrim);
                    LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " Vtrim step " << theVtrimStepSize << RESET;
                    uint16_t VtrimAttempt = (theTargetThreshold - theMinimumThreshold)/theVtrimStepSize;
                    LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " first Vtrim to set  " << VtrimAttempt << RESET;

                    fReadoutChipInterface->SetVtrim(theReadoutChip,VtrimAttempt);
                    ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                    auto theCurrentMaxThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                    LOG(INFO) << BOLDGREEN <<  " Chip " <<cChip->getId() << " the first guessed threshold is " << theCurrentMaxThreshold  << " and target is " << theTargetThreshold << RESET;
                    
                    // Now checking if we can go even closer to the expected value
                    bool     isSearching         = true;
                    uint32_t theCurrentIteration = 0;
                    while(isSearching)
                    {
                        LOG(INFO) << YELLOW << "Checking if we can go closer to the expected value. Iteration " << theCurrentIteration << RESET;
                        LOG(INFO) << MAGENTA << " Vtrim - 1 " << +VtrimAttempt - 1 << RESET;
                        uint8_t theDACDownValue = std::max(uint8_t(0), uint8_t(VtrimAttempt - 1));
                        fReadoutChipInterface->SetVtrim(theReadoutChip,theDACDownValue);
                        
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                        auto MaxThresholdDown = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                    
                        uint8_t theDACUpValue = std::min(uint8_t(theMaxVtrim), uint8_t(VtrimAttempt + 1));
                        LOG(DEBUG) << MAGENTA << "Vtrim + 1 " << +VtrimAttempt + 1<< RESET;
                        fReadoutChipInterface->SetVtrim(theReadoutChip,theDACUpValue);
                        
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                        auto MaxThresholdUp = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                        
                        float theExpectedDifference     = std::fabs(theTargetThreshold - theCurrentMaxThreshold);
                        float theExpectedDifferenceDown = std::fabs(theTargetThreshold - MaxThresholdDown);
                        float theExpectedDifferenceUp   = std::fabs(theTargetThreshold - MaxThresholdUp);
                    
                        if((theExpectedDifferenceDown < theExpectedDifference) || (theExpectedDifferenceUp < theExpectedDifference))
                        {
                            LOG(DEBUG) << BOLDRED << "Not precise extrapolation in PedestalEqualizationAtPedestal: theExpectedDifferenceDown:" << theExpectedDifferenceDown
                                       << ", theExpectedDifferenceUp:" << theExpectedDifferenceUp << ", theExpectedDifference:" << theExpectedDifference << RESET;
                            if((theExpectedDifferenceDown < theExpectedDifference))
                            {
                                VtrimAttempt    = theDACDownValue;
                            }
                            if((theExpectedDifferenceUp < theExpectedDifference))
                            {
                                VtrimAttempt    = theDACUpValue;
                            }
                        }
                        else
                        {
                            LOG(DEBUG) << BOLDGREEN << "Good extrapolation of Vtrim " << VtrimAttempt << RESET;
                            fReadoutChipInterface->SetVtrim(theReadoutChip,VtrimAttempt); 
                            
                            ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                            auto theThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                            LOG(INFO) << BOLDGREEN << " get threshold " << theThreshold << " with target " << theTargetThreshold << RESET;
                            isSearching = false;
                        }
                        LOG(INFO) << BOLDMAGENTA << "Writing DAC val " << +VtrimAttempt << RESET;
                        fReadoutChipInterface->SetVtrim(theReadoutChip,VtrimAttempt);   
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                        theCurrentMaxThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                        LOG(INFO) << BOLDMAGENTA << "Get Threshold " << theCurrentMaxThreshold << RESET;
                        theCurrentIteration += 1;
                    }
                
                    LOG(INFO) << BOLDGREEN << "Vtrim " << VtrimAttempt << " gives -> New max occupancy threshold value: " << theCurrentMaxThreshold << " Expected value: " << theTargetThreshold << RESET;
                } //chip
            }
        }
    }        
    std::cout << " close all loops" << std::endl;
}

void PedestalEqualizationPSAtPedestal::TuneTrimBitsBinary()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    DetectorDataContainer theFinalTrimBitsContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theFinalTrimBitsContainers);
    DetectorDataContainer theDistanceFromTargetContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theDistanceFromTargetContainers);

    // uint8_t theMaxTrimBits = 31;
    // uint8_t theMinTrimBits = 0;
    uint8_t theStartTrimBits = 15;
    uint8_t theTrimBitsNumber = 5;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    fReadoutChipInterface->SetTrimBitsAll(theReadoutChip,theStartTrimBits);
                }
            }
        }
    }

    // Start from most significant bit
    for (int ibit = theTrimBitsNumber - 1; ibit >= 0; ibit--) 
    {
        ScanThreshold();
        for(auto cBoard: fTheMaxOccupancyThresholdContainers)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto theTargetThreshold = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        auto theChipDistanceFromTargetContainer = theDistanceFromTargetContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto theChipTrimBitsContainer = theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        float averageTrim = 0;
                        float averageDistance = 0;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                bool isChannelPrint = (row == 4 && col == 118);

                                // first iteration
                                if(ibit== theTrimBitsNumber - 1) theChipDistanceFromTargetContainer->getChannel<uint16_t>(row,col) = 255;
                                uint8_t theTrimBits = fReadoutChipInterface->ReadTrimBitsChannel(theReadoutChip,row, col);
                                averageTrim += theTrimBits;
                                auto theCurrentMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row,col);
                                float distanceFromTarget = std::fabs(theCurrentMaxOccupancyThreshold - theTargetThreshold);
                                if(isChannelPrint) LOG(INFO) << MAGENTA << " bit  " << +ibit << " currentthreshold " << theCurrentMaxOccupancyThreshold << " target "<< theTargetThreshold << RESET;

                                if (distanceFromTarget < theChipDistanceFromTargetContainer->getChannel<uint16_t>(row,col) || ibit == theTrimBitsNumber - 1) 
                                {
                                    if(isChannelPrint) LOG(INFO) << MAGENTA << " updating final trimbits " << RESET;
                                    theChipDistanceFromTargetContainer->getChannel<uint16_t>(row,col) = distanceFromTarget;
                                    theChipTrimBitsContainer->getChannel<uint16_t>(row,col) = theTrimBits;
                                }
                                averageDistance += distanceFromTarget;
                                // Reject bit if the occupancy is too high
                                if ((ibit > 0 && theCurrentMaxOccupancyThreshold > theTargetThreshold)) 
                                {
                                    theTrimBits &= ~(1 << ibit);
                                } 
                                else 
                                {
                                    theTrimBits |= (1 << ibit);
                                }
                                if (ibit > 0) theTrimBits |= (1 << (ibit - 1)); // Setting next bit to 1 for the test
                            
                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip, theTrimBits, row, col);
                            }
                        }
                        averageTrim /= cChip->getNumberOfRows() * cChip->getNumberOfCols();
                        LOG(INFO) << BOLDYELLOW << "Average TrimDAC / PixelOccupancy for CHIP " << cChip->getId() << " is " << averageTrim  << RESET;
                        LOG(INFO) << BOLDYELLOW << "Average distance from target  " << averageDistance/( cChip->getNumberOfRows() * cChip->getNumberOfCols()) << RESET;
                    }
                }
            }
        }
    } 
    #ifdef __USE_ROOT__
    LOG(INFO) << BLUE << "fillTrimBitsPlots " << RESET;
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillTrimBitsPlots(theFinalTrimBitsContainers);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalTrimBits");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, theFinalTrimBitsContainers);
    }
    
#endif
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {   
                            auto trimBits = theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                            fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,trimBits, row, col);
                        }
                    }
                }
            }
        }
    }
    std::cout << " close all loops" << std::endl;
    ScanThreshold();
}


void PedestalEqualizationPSAtPedestal::TuneTrimBits()
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;
    DetectorDataContainer theMaxTrimBitsMaxOccupancyThresholdContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theMaxTrimBitsMaxOccupancyThresholdContainers);
    DetectorDataContainer theMinTrimBitsMaxOccupancyThresholdContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theMinTrimBitsMaxOccupancyThresholdContainers);

    uint16_t theMaxTrimBits = 31;
    uint16_t theMinTrimBits = 0;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    fReadoutChipInterface->SetTrimBitsAll(theReadoutChip,theMaxTrimBits);
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
                    auto theChipContainer = theMaxTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            theChipContainer->getChannel<uint16_t>(row,col) = cChip->getChannel<uint16_t>(row,col);
                            bool isChannelPrint = (row == 4 && col == 118);
                            if(isChannelPrint) LOG(INFO) << MAGENTA << " theMaxTrimBitsMaxOccupancyThresholdContainers " <<  theChipContainer->getChannel<uint16_t>(row,col) << " fTheMaxOccupancyThresholdContainers " << cChip->getChannel<uint16_t>(row,col) << RESET;
                        }
                    }
                }
            }
        }
    }



    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                { 
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    fReadoutChipInterface->SetTrimBitsAll(theReadoutChip,theMinTrimBits);
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
                    auto theChipContainer = theMinTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            theChipContainer->getChannel<uint16_t>(row,col) = cChip->getChannel<uint16_t>(row,col);
                        }
                    }
                }
            }
        }
    }

    LOG(INFO) << BOLDMAGENTA << " Scan Trim Bit " << RESET;
    DetectorDataContainer theCurrentThresholdContainers;
    // GenericDataArray < threshold, trim bit, isSearching >
    ContainerFactory::copyAndInitChannel<std::pair<std::pair<uint16_t, uint16_t>, bool>>(*fDetectorContainer, theCurrentThresholdContainers);
    DetectorDataContainer theUpTrimBitsMaxOccupancyThresholdContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theUpTrimBitsMaxOccupancyThresholdContainers);
    DetectorDataContainer theDownTrimBitsMaxOccupancyThresholdContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theDownTrimBitsMaxOccupancyThresholdContainers);
    DetectorDataContainer theFinalTrimBitsContainers;
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, theFinalTrimBitsContainers);

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto theChipContainer = theCurrentThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                    auto theTargetThreshold = fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    LOG(INFO) << BOLDYELLOW << " CHIP " << cChip->getId() << " TARGET THRESHOLD "<< theTargetThreshold << RESET;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).second = true;
                            bool isChannelPrint = (row == 4 && col == 118);
                            auto theMaximumThreshold =  theMaxTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                            if (isChannelPrint) LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " maximum threshold at maximum trim bit " << theMaximumThreshold << RESET;

                            auto theMinimumThreshold =  theMinTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                            uint16_t TrimBitsAttempt;
                            if (isChannelPrint) LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " minimum threshold at minumum trim bit " << theMinimumThreshold << RESET;
                            if(theMinimumThreshold > theTargetThreshold)
                            {
                                LOG(INFO) << BOLDRED << " Chip " << cChip->getId() << " row " << row << " col " << col << " bin " << linearizeRowAndCols(row, col, cChip->getNumberOfCols()) << " minimum thr > target " << RESET;

                                theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).second = false;
                                TrimBitsAttempt = 0;
                                theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row, col) = 0;
                            }
                            else if(theMaximumThreshold < theTargetThreshold)
                            {
                                LOG(INFO) << BOLDRED << " Chip " << cChip->getId() << " row " << row << " col " << col << " bin " << linearizeRowAndCols(row, col, cChip->getNumberOfCols()) << " max thr < target " << RESET;

                                theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).second = false;
                                TrimBitsAttempt = 31;
                                theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row, col) = 31;
                            }
                            else
                            {
                                // Assume linear relationship for change in Vtrim and change in the threshold giving max occupancy
                                float theTrimBitStepSize = (float(theMaximumThreshold) - float(theMinimumThreshold))/float(theMaxTrimBits);
                                if (isChannelPrint) LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << "  step size " << theTrimBitStepSize << RESET;
                                TrimBitsAttempt = (float(theTargetThreshold) - float(theMinimumThreshold))/theTrimBitStepSize;
                                if (isChannelPrint) LOG(INFO) << BOLDYELLOW << " Chip " << cChip->getId() << " first TrimBit to set  " << TrimBitsAttempt << RESET;
                                if(TrimBitsAttempt > theMaxTrimBits) 
                                {
                                    LOG(INFO) << BOLDRED << " Chip " << cChip->getId() << " row " << row << " col " << col << " bin " << linearizeRowAndCols(row, col, cChip->getNumberOfCols()) << " estimated trim bit above range!" << RESET;

                                    TrimBitsAttempt = theMaxTrimBits;
                                    theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).second = false;
                                }
                            }

                            // initialize to first guess
                            theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row, col) = TrimBitsAttempt;              
        
                            fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,TrimBitsAttempt, row, col);
                        }
                    }
                    ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());

                    bool     isSearching         = true;
                    uint32_t theMaxNumberOfIterations = 3;
                    uint32_t theCurrentIteration = 0;
                    while(isSearching)
                    {

                        isSearching = false; // Assume trimmed than prove false.
                        LOG(INFO) << MAGENTA <<  " Searching iteration " << theCurrentIteration << RESET;
            
                        LOG(INFO )<< MAGENTA <<  "Read the current threshold and then set the trim bit lower one unit wrt guess " << RESET;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto theCurrentValues = theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col);   
                                if (theCurrentValues.second == false) continue;
                     
                                bool isChannelPrint = (row == 4 && col == 118);
                                auto theCurrentMaxThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                                if(isChannelPrint) LOG(INFO) << YELLOW << "Checking if we can go closer to the expected value. Iteration " << theCurrentIteration << RESET;
                                auto TrimBitsAttempt = fReadoutChipInterface->ReadTrimBitsChannel(cChip, row, col);
                                theChipContainer->getChannel<std::pair<std::pair<u_int16_t,uint16_t>, bool>>(row,col) = std::make_pair(std::make_pair(theCurrentMaxThreshold,TrimBitsAttempt), true);

                                if(isChannelPrint) LOG(INFO) << BOLDGREEN <<  " Chip " <<cChip->getId() << " the first guessed threshold is " << theCurrentMaxThreshold  << " and target is " << theTargetThreshold << RESET;
                            
                                // Now checking if we can go even closer to the expected value
                                uint8_t theDACDownValue = std::max(uint8_t(0), uint8_t(TrimBitsAttempt - 1));
                                if(isChannelPrint) LOG(INFO) << MAGENTA << " theDACDownValue " << +theDACDownValue << RESET;
                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,theDACDownValue, row, col);
                            }
                        }
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());

                        
                        auto theDownChipContainer = theDownTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto theUpChipContainer = theUpTrimBitsMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        LOG(INFO )<< MAGENTA <<  "Read the down threshold and then set the trim bit higher by one unit wrt guess" << RESET;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto theCurrentValues = theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col);
                                if (theCurrentValues.second == false) continue;
                                bool isChannelPrint = (row == 4 && col == 118);

                                auto MaxThresholdDown = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                                if(isChannelPrint) LOG(INFO) << MAGENTA <<" DownThr " << MaxThresholdDown << RESET;
                                theDownChipContainer->getChannel<uint16_t>(row, col) = MaxThresholdDown;

                                auto TrimBitsAttempt = theCurrentValues.first.second;
                                uint8_t theDACUpValue = std::min(uint8_t(theMaxTrimBits), uint8_t(TrimBitsAttempt + 1));
                                if(isChannelPrint) LOG(INFO) << MAGENTA << " theDACUpValue " << +theDACUpValue << RESET;

                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,theDACUpValue, row, col);
                            }
                        }
                        
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());

                        LOG(INFO) << BOLDYELLOW << "Read the up threshold and then check wich trim bit is closer to target " << RESET;
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto theCurrentValues = theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col);
                                if (theCurrentValues.second == false) continue;
    
                                bool isChannelPrint = (row == 4 && col == 118);

                                auto MaxThresholdUp = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                                if(isChannelPrint) LOG(INFO) << MAGENTA <<" UpThr " << MaxThresholdUp << RESET;
                                theUpChipContainer->getChannel<uint16_t>(row, col) = MaxThresholdUp;

                                auto theCurrentMaxThreshold = theCurrentValues.first.first;
                                auto TrimBitsAttempt =  theCurrentValues.first.second;
                                auto MaxThresholdDown = theDownChipContainer->getChannel<uint16_t>(row, col);
                                // auto MaxThresholdUp = theUpChipContainer->getChannel<uint16_t>(row, col);
                                if(isChannelPrint) LOG(INFO) << BLUE << " up " << MaxThresholdUp << " current " << theCurrentMaxThreshold << " down " << MaxThresholdDown << RESET;
                                float theExpectedDifference     = std::fabs(theTargetThreshold - theCurrentMaxThreshold);
                                float theExpectedDifferenceDown = std::fabs(theTargetThreshold - MaxThresholdDown);
                                float theExpectedDifferenceUp   = std::fabs(theTargetThreshold - MaxThresholdUp);
                            
                                if((theExpectedDifferenceDown < theExpectedDifference) || (theExpectedDifferenceUp < theExpectedDifference))
                                {
                                    if(isChannelPrint) LOG(INFO) << BOLDRED << "Not precise extrapolation in PedestalEqualizationAtPedestal: theExpectedDifferenceDown:" << theExpectedDifferenceDown
                                               << ", theExpectedDifferenceUp:" << theExpectedDifferenceUp << ", theExpectedDifference:" << theExpectedDifference << RESET;
                                    if((theExpectedDifferenceDown < theExpectedDifference))
                                    {
                                        TrimBitsAttempt    = std::max(uint8_t(0), uint8_t(TrimBitsAttempt - 1));
                                    }
                                    if((theExpectedDifferenceUp < theExpectedDifference))
                                    {
                                        TrimBitsAttempt    = std::min(uint8_t(theMaxTrimBits), uint8_t(TrimBitsAttempt + 1));
                                    }
                                    theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row, col) = TrimBitsAttempt;              
                                }
                                else
                                {
                                    if(isChannelPrint) LOG(INFO) << BOLDGREEN << "Good extrapolation of TrimBits " << TrimBitsAttempt << RESET;
                                    fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip, TrimBitsAttempt, row, col); 
                                    theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row, col) = TrimBitsAttempt;
                                    
                                    theCurrentValues.second = false;
                                }
                                //isSearching = (isSearching && theCurrentValues.second);
                                if (isChannelPrint)  LOG(INFO) << MAGENTA << "Writing DAC val " << TrimBitsAttempt << " isSearching " <<  isSearching << RESET; 
                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,TrimBitsAttempt, row, col);
                                theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).first.second = TrimBitsAttempt;
                                theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).second = theCurrentValues.second;
                            }
                        }
                        LOG(INFO) << BOLDYELLOW << "Scan threshold after updating trim bits and then get new current threshold " << RESET;
                        ScanThresholdChip(cBoard->getId(),cOpticalGroup->getId(),cHybrid->getId(),cChip->getId());
                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto theCurrentValues = theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col);
                                if (theCurrentValues.second == false) continue;
                                if (theCurrentValues.second == true) isSearching = true;
                                bool isChannelPrint = (row == 4 && col == 118);
                                auto theCurrentMaxThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                                if (isChannelPrint) LOG(INFO) << BOLDMAGENTA << "Get Threshold " << theCurrentMaxThreshold << RESET;
                                theChipContainer->getChannel<std::pair<std::pair<uint16_t,uint16_t>, bool>>(row, col).first.first = theCurrentMaxThreshold;
                            
                            }
                        }
                        theCurrentIteration += 1;
                        LOG(INFO) << BOLDYELLOW << "Update iteration " << RESET;
                        if (theCurrentIteration > theMaxNumberOfIterations)
                        { 
                            isSearching = false;
                            LOG(INFO) << BOLDRED << "REACHED MAX NUMBER OF ITERATIONS!!!! " << RESET;          
                        }
                    }// searching
                } //chip
            }
        }
    }
    
#ifdef __USE_ROOT__
    LOG(INFO) << BLUE << "fillTrimBitsPlots " << RESET;
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillTrimBitsPlots(theFinalTrimBitsContainers);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalTrimBits");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, theFinalTrimBitsContainers);
    }
    
#endif
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {   
                            auto trimBits = theFinalTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<uint16_t>(row,col);
                            fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip,trimBits, row, col);
                        }
                    }
                }
            }
        }
    }
    std::cout << " close all loops" << std::endl;
    ScanThreshold();



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
