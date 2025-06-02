#include "tools/PedestalEqualizationPSAtPedestal.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cPSCounterFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "tools/Tool.h"

#include <bitset>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string PedestalEqualizationPSAtPedestal::fCalibrationDescription =
    "Equalize the pedestal/threshold for all channels with higher precision near the pedestal. In addition to the trim bits it equalizes also Vtrim.";

void PedestalEqualizationPSAtPedestal::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^TrimDAC_C\\d+_R\\d+$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^C[0-6]$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::MPA2, "^ThDAC\\d$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^THTRIMMING_S\\d+$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^Bias_D5DAC8$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::SSA2, "^Bias_THDAC$");

    // This is very similar to the PedestalEqualization class.
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

    // We are pretending to be injecting to have the calibration working but we want to be at the pedestal so we put the smallest charge
    fTestPulseAmplitude    = 0;
    fTestPulseAmplitudePix = 0;

    fEventsPerPoint = findValueInSettings<double>("Nevents", 10);
    if(fEventsPerPoint > 1000)
    {
        fEventsPerPoint = 1000;
        LOG(INFO) << BOLDRED << " Limiting the number of events to 1000 to avoid rollover of the MPA Ripple counter." << RESET;
    }
    fNEventsPerBurst        = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    bool fastCounterReadout = findValueInSettings<double>("PedestalEqualizationPSAtPedestal_FastCounterReadout", 1) > 0;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    fStopValue          = findValueInSettings<double>("PedestalEqualizationPSAtPedestal_MaxThreshold", 150);
    fStartValue         = findValueInSettings<double>("PedestalEqualizationPSAtPedestal_MinThreshold", 0);
    const size_t nSteps = fStopValue - fStartValue + 1;
    for(auto i = 0u; i < nSteps; i++) { dacList.push_back(fStartValue + i); }

    fNsigma       = findValueInSettings<double>("PedestalEqualizationPSAtPedestal_Sigma", 3);
    fDoDebugHists = findValueInSettings<double>("PedestalEqualizationPSAtPedestal_SaveDebugHists", 1) > 0;

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

void PedestalEqualizationPSAtPedestal::ConfigureCalibration() {}

void PedestalEqualizationPSAtPedestal::Running()
{
    if(fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTracker2S) return;

    // std::function<bool(const ChipContainer*)>        selectSSAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() ==
    // FrontEndType::SSA2); }; std::string selectSSAfunctionName = "SelectSSAfunctionPS";

    // std::function<bool(const ChipContainer*)>        selectMPAfunction     = [](const ChipContainer* theChip) { return (static_cast<const ReadoutChip*>(theChip)->getFrontEndType() ==
    // FrontEndType::MPA2); }; std::string selectMPAfunctionName = "SelectMPAfunctionPS";

    LOG(INFO) << BOLDMAGENTA << "Starting PedestalEqualizationPSAtPedestal measurement." << RESET;
    Initialise(false);
    PrepareForInjection();  // This sets the chips in the correct status.
    SetInitialConditions(); // Sets Vtrim and Trim Bits in their initial configuration
    ScanThreshold();
    GetLowestAndHighestMaxOccupancyThreshold();
    FindTargetThreshold();
    TuneVtrimBinary();
    SetTargetThreshold();
    ScanTrimBit();
    SetTargetTrimBits();
    ScanThreshold();
    Reset();
    LOG(INFO) << BOLDMAGENTA << "Done with PedestalEqualizationPSAtPedestal." << RESET;
}

void PedestalEqualizationPSAtPedestal::PrepareForInjection()
{
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
                    if(cType == FrontEndType::MPA2) { fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitudePix); }
                    else // SSA
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fTestPulseAmplitude);
                    }
                }
            }
        }
    }
    LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    this->setTestAllChannels(true);
}
void PedestalEqualizationPSAtPedestal::SetInitialConditions()
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    // As a starting point we choose the max trim bit but the Vtrim giving the smallest range.
                    // Vtrim goes in opposites directions for MPAs and SSAs
                    uint16_t VtrimForMaxRange;
                    auto     cType = cChip->getFrontEndType();
                    if(cType == FrontEndType::MPA2) { VtrimForMaxRange = 0x0; }
                    else // SSA
                    {
                        VtrimForMaxRange = 0x1F;
                    }
                    fReadoutChipInterface->SetTrimBitsAll(cChip, 0x1F);
                    fReadoutChipInterface->SetVtrim(cChip, VtrimForMaxRange);
                }
            }
        }
    }
}
void PedestalEqualizationPSAtPedestal::ScanThreshold()
{
    std::vector<DetectorDataContainer>  detectorContainerVector(dacList.size());
    std::vector<DetectorDataContainer*> detectorContainerVectorPointers;
    for(auto& container: detectorContainerVector)
    {
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, container);
        detectorContainerVectorPointers.push_back(&container);
    }

    this->scanDac("Threshold", dacList, fEventsPerPoint, detectorContainerVectorPointers, fNEventsPerBurst);
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
                        if(cChip->hasChannelContainer() == false) continue;

                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                auto targetMap = &(dacOccupancyContainers.getObject(cBoard->getId())
                                                       ->getObject(cOpticalGroup->getId())
                                                       ->getObject(cHybrid->getId())
                                                       ->getObject(cChip->getId())
                                                       ->getChannel<std::map<uint16_t, float>>(row, col));

                                (*targetMap)[dacIt] = cChip->getChannel<Occupancy>(row, col).fOccupancy;
                            } // col
                        } // row
                    } // chip
                } // hybrid
            } // optical group
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

void PedestalEqualizationPSAtPedestal::ScanTrimBit()
{
    LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << RESET;
    std::vector<uint16_t> trimbitList;
    uint8_t               theMaxTrimBit = 31;
    uint8_t               theMinTrimBit = 0;
    const size_t          nSteps        = theMaxTrimBit - theMinTrimBit + 1;
    for(auto i = 0u; i < nSteps; i++) { trimbitList.push_back(theMinTrimBit + i); }

    std::vector<DetectorDataContainer>  detectorContainerVector(trimbitList.size());
    std::vector<DetectorDataContainer*> detectorContainerVectorPointers;
    for(auto& container: detectorContainerVector)
    {
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, container);
        detectorContainerVectorPointers.push_back(&container);
    }

    this->scanDac("Offsets", trimbitList, fEventsPerPoint, detectorContainerVectorPointers, fNEventsPerBurst);
    DetectorDataContainer dacOccupancyContainers;
    ContainerFactory::copyAndInitChannel<std::map<uint16_t, float>>(*fDetectorContainer, dacOccupancyContainers);

    for(size_t dacIt = 0; dacIt < trimbitList.size(); ++dacIt)
    {
        for(auto cBoard: detectorContainerVector.at(dacIt))
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->hasChannelContainer() == false) continue;

                        for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                        {
                            for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                            {
                                // if(row == 0 && col == 3 ) std::cout << " for chip " << cChip->getId() << " for 1 channel at trim " << dacIt << " occupancy is " << cChip->getChannel<Occupancy>(row,
                                // col).fOccupancy << std::endl;
                                auto targetMap = &(dacOccupancyContainers.getObject(cBoard->getId())
                                                       ->getObject(cOpticalGroup->getId())
                                                       ->getObject(cHybrid->getId())
                                                       ->getObject(cChip->getId())
                                                       ->getChannel<std::map<uint16_t, float>>(row, col));

                                (*targetMap)[dacIt] = cChip->getChannel<Occupancy>(row, col).fOccupancy;
                            } // col
                        } // row
                    } // chip
                } // hybrid
            } // optical group
        } // board
    } // dac

    GetMaximumOccupancyTrimBits(dacOccupancyContainers);

    // These plots are useful for debugging only
    if(fDoDebugHists)
    {
#ifdef __USE_ROOT__
        fDQMHistogramPedestalEqualizationPSAtPedestal.fillTrimCurvePlotsVector(detectorContainerVector, trimbitList);
#else
        if(fDQMStreamerEnabled)
        {
            for(size_t dacIt = 0; dacIt < trimbitList.size(); ++dacIt)
            {
                ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalOccupancyTrimBits");
                theContainerSerialization.streamByChipContainer(fDQMStreamer, *detectorContainerVector.at(dacIt), dacIt);
            }
        }
#endif
    }
}

void PedestalEqualizationPSAtPedestal::FillMaxOccupancyMap(std::vector<DetectorDataContainer> detectorContainerVector,
                                                           DetectorDataContainer&             dacOccupancyContainers,
                                                           uint16_t                           boardId,
                                                           uint16_t                           OGId,
                                                           uint16_t                           hybridId,
                                                           uint16_t                           ChipId)
{
    for(size_t dacIt = 0; dacIt < dacList.size(); ++dacIt)
    {
        auto theChipContainer = detectorContainerVector.at(dacIt).getObject(boardId)->getObject(OGId)->getObject(OGId)->getObject(ChipId);
        if(theChipContainer->hasChannelContainer() == false) continue;

        for(uint16_t row = 0; row < theChipContainer->getNumberOfRows(); ++row)
        {
            for(uint16_t col = 0; col < theChipContainer->getNumberOfCols(); ++col)
            {
                auto targetMap = &(dacOccupancyContainers.getObject(boardId)->getObject(OGId)->getObject(OGId)->getObject(ChipId)->getChannel<std::map<uint16_t, float>>(row, col));

                (*targetMap)[dacIt] = theChipContainer->getChannel<Occupancy>(row, col).fOccupancy;
            } // col
        } // row
    }
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
                    LOG(DEBUG) << BOLDBLUE << "Looking for DACmaxOccupancy for chip " << cChip->getId() << RESET;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            const auto& occupancyMap = cChip->getChannel<std::map<uint16_t, float>>(row, col);
                            if(occupancyMap.empty()) continue;

                            float DACmaxOccupancy = 0;
                            float totalOccupancy  = 0;
                            for(const auto& [trimBit, occupancy]: occupancyMap)
                            {
                                DACmaxOccupancy += static_cast<float>(trimBit) * occupancy;
                                totalOccupancy += occupancy;
                            }
                            DACmaxOccupancy /= totalOccupancy;
                            theChipContainer->getChannel<uint16_t>(row, col) = static_cast<uint16_t>(std::lround(DACmaxOccupancy));
                        } // col
                    } // row
                } // chip
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

void PedestalEqualizationPSAtPedestal::GetMaximumOccupancyTrimBits(const DetectorDataContainer& dacOccupancyContainers)
{
    ContainerFactory::copyAndInitChannel<uint16_t>(*fDetectorContainer, fTheMaxOccupancyTrimBitsContainers);

    for(auto cBoard: dacOccupancyContainers)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto theChipContainer = fTheMaxOccupancyTrimBitsContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    LOG(DEBUG) << BOLDBLUE << "Looking for DACmaxOccupancy for chip " << cChip->getId() << RESET;
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            const auto& occupancyMap = cChip->getChannel<std::map<uint16_t, float>>(row, col);
                            if(occupancyMap.empty()) continue;

                            auto     maxIter         = std::max_element(occupancyMap.begin(), occupancyMap.end(), [](const auto& a, const auto& b) { return a.second < b.second; });
                            uint16_t DACmaxOccupancy = maxIter->first;
                            theChipContainer->getChannel<uint16_t>(row, col) = DACmaxOccupancy;
                        } // col
                    } // row
                } // chip
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillMaxPlots(fTheMaxOccupancyTrimBitsContainers);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalMax");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, fTheMaxOccupancyTrimBitsContainers);
    }

#endif
}

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
                    uint16_t     smallestThresholdAtMaxOccupancy      = 255;
                    uint16_t     largestThresholdAtMaxOccupancy       = 0;
                    uint16_t     rowAtSmallestThresholdAtMaxOccupancy = -1, colAtSmallestThresholdAtMaxOccupancy = -1, binAtSmallestThresholdAtMaxOccupancy = -1;
                    uint16_t     rowAtLargestThresholdAtMaxOccupancy = -1, colAtLargestThresholdAtMaxOccupancy = -1, binAtLargestThresholdAtMaxOccupancy = -1;
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
                    LOG(DEBUG) << BLUE << "Chip " << cChip->getId() << " Mean Threshold for max occupancy: " << theMean << RESET;

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
                    LOG(DEBUG) << BLUE << "Chip " << cChip->getId() << "Standard deviation: " << stddev << RESET;

                    // Define Nσ range
                    float lowerBound = std::max(float(1), float(theMean - fNsigma * stddev));
                    float upperBound = std::min(float(255), float(theMean + fNsigma * stddev));
                    LOG(DEBUG) << BLUE << "Chip " << cChip->getId() << " " << fNsigma << "σ range: [" << lowerBound << ", " << upperBound << "]" << RESET;

                    // Get max and min channel
                    for(uint16_t row = 0; row < cChip->getNumberOfRows(); ++row)
                    {
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col)
                        {
                            uint16_t bin                      = linearizeRowAndCols(row, col, cChip->getNumberOfCols());
                            uint16_t theMaxOccupancyThreshold = cChip->getChannel<uint16_t>(row, col);
                            if(theMaxOccupancyThreshold < lowerBound)
                                LOG(DEBUG) << BOLDBLUE << "Outlier LOW " << theMaxOccupancyThreshold << " channel row " << row << " col " << col << " bin " << bin << RESET;
                            if(theMaxOccupancyThreshold > upperBound)
                            {
                                LOG(DEBUG) << BOLDBLUE << "Outlier HIGH " << theMaxOccupancyThreshold << " channel row " << row << " col " << col << " bin " << bin << RESET;
                                fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip, 0x00, row, col);
                            }
                            if(theMaxOccupancyThreshold < smallestThresholdAtMaxOccupancy && theMaxOccupancyThreshold > lowerBound && theMaxOccupancyThreshold < upperBound)
                            {
                                smallestThresholdAtMaxOccupancy      = theMaxOccupancyThreshold;
                                rowAtSmallestThresholdAtMaxOccupancy = row;
                                colAtSmallestThresholdAtMaxOccupancy = col;
                                binAtSmallestThresholdAtMaxOccupancy = bin;
                            }

                            if(theMaxOccupancyThreshold > largestThresholdAtMaxOccupancy && theMaxOccupancyThreshold < upperBound && theMaxOccupancyThreshold > lowerBound)
                            {
                                largestThresholdAtMaxOccupancy      = theMaxOccupancyThreshold;
                                rowAtLargestThresholdAtMaxOccupancy = row;
                                colAtLargestThresholdAtMaxOccupancy = col;
                                binAtLargestThresholdAtMaxOccupancy = bin;
                            }
                        } // col
                    } // row

                    LOG(DEBUG) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << rowAtSmallestThresholdAtMaxOccupancy << " col " << colAtSmallestThresholdAtMaxOccupancy
                               << " (bin " << binAtSmallestThresholdAtMaxOccupancy << ") has the lowest DAC giving the maximum occupancy at " << smallestThresholdAtMaxOccupancy << RESET;
                    LOG(DEBUG) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << rowAtLargestThresholdAtMaxOccupancy << " col " << colAtLargestThresholdAtMaxOccupancy
                               << " (bin " << binAtLargestThresholdAtMaxOccupancy << ") has the highest DAC giving the maximum occupancy at " << largestThresholdAtMaxOccupancy << RESET;
                    auto theLowestThreshold = std::make_pair(std::make_pair(rowAtSmallestThresholdAtMaxOccupancy, colAtSmallestThresholdAtMaxOccupancy), smallestThresholdAtMaxOccupancy);
                    auto theChipSmallestThresholdContainer =
                        fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>() = theLowestThreshold;
                    auto theHighestThreshold = std::make_pair(std::make_pair(rowAtLargestThresholdAtMaxOccupancy, colAtLargestThresholdAtMaxOccupancy), largestThresholdAtMaxOccupancy);
                    auto theChipLargestThresholdContainer =
                        fTheLargestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipLargestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>() = theHighestThreshold;

                } // chip
            } // hybrid
        } // OG
    } // board

    // These plots are useful for debugging only
    if(fDoDebugHists)
    {
#ifdef __USE_ROOT__
        LOG(INFO) << BLUE << "fillReferenceChannelPlots " << RESET;
        fDQMHistogramPedestalEqualizationPSAtPedestal.fillReferenceChannelPlots(fTheSmallestThresholdAtMaxOccupancyContainer, true);
        fDQMHistogramPedestalEqualizationPSAtPedestal.fillReferenceChannelPlots(fTheLargestThresholdAtMaxOccupancyContainer, false);
#else
        if(fDQMStreamerEnabled)
        {
            ContainerSerialization theContainerSerializationSmall("PedestalEqualizationPSAtPedestalReferenceChannelSmall");
            theContainerSerializationSmall.streamByChipContainer(fDQMStreamer, fTheSmallestThresholdAtMaxOccupancyContainer);
            ContainerSerialization theContainerSerializationLarge("PedestalEqualizationPSAtPedestalReferenceChannelLarge");
            theContainerSerializationLarge.streamByChipContainer(fDQMStreamer, fTheLargestThresholdAtMaxOccupancyContainer);
        }
#endif
    }
}

void PedestalEqualizationPSAtPedestal::SetTargetThreshold()
{
    LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << RESET;

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
void PedestalEqualizationPSAtPedestal::SetTargetTrimBits()
{
    LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << RESET;

    for(auto cBoard: fTheMaxOccupancyTrimBitsContainers)
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
                        for(uint16_t col = 0; col < cChip->getNumberOfCols(); ++col) { fReadoutChipInterface->SetTrimBitsChannel(theReadoutChip, cChip->getChannel<uint16_t>(row, col), row, col); }
                    }
                }
            }
        }
    }
#ifdef __USE_ROOT__
    LOG(INFO) << BLUE << "fillTrimBitsPlots " << RESET;
    fDQMHistogramPedestalEqualizationPSAtPedestal.fillTrimBitsPlots(fTheMaxOccupancyTrimBitsContainers);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PedestalEqualizationPSAtPedestalTrimBits");
        theContainerSerialization.streamByChipContainer(fDQMStreamer, fTheMaxOccupancyTrimBitsContainers);
    }
#endif
}

void PedestalEqualizationPSAtPedestal::FindTargetThreshold()
{
    LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << RESET;
    ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, fTheTargetThresholdContainers);

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->SetTrimBitsAll(cChip, 0x0); }
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
                    auto theChipLargestThresholdContainer =
                        fTheLargestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    auto theLargestThreshold = theChipLargestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();

                    auto     row               = theLargestThreshold.first.first;
                    auto     col               = theLargestThreshold.first.second;
                    uint16_t theTargetTreshold = cChip->getChannel<uint16_t>(row, col);
                    LOG(DEBUG) << BOLDGREEN << " for chip " << cChip->getId() << " the channel in row " << row << " col " << col << " has the maximum occupancy at " << theTargetTreshold
                               << " before was at " << theLargestThreshold.second << RESET;
                    auto theChipTargetThresholdContainer =
                        fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    theChipTargetThresholdContainer->getSummary<uint16_t>() = theTargetTreshold;
                }
            }
        }
    }
}
void PedestalEqualizationPSAtPedestal::TuneVtrimBinary()
{
    LOG(INFO) << BOLDMAGENTA << __PRETTY_FUNCTION__ << RESET;
    uint8_t               theStartVTrim      = 0x10;
    uint8_t               theVTrimBitsNumber = 5;
    DetectorDataContainer theFinalVTrimContainers;
    ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, theFinalVTrimContainers);
    DetectorDataContainer theDistanceFromTargetContainers;
    ContainerFactory::copyAndInitStructure<uint16_t>(*fDetectorContainer, theDistanceFromTargetContainers);

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    fReadoutChipInterface->SetTrimBitsAll(cChip, 0x1F);
                    fReadoutChipInterface->SetVtrim(cChip, theStartVTrim);
                }
            }
        }
    }

    for(int ibit = theVTrimBitsNumber - 1; ibit >= 0; ibit--)
    {
        LOG(INFO) << BOLDMAGENTA << " Scanning Bit " << ibit << RESET;
        ScanThreshold();

        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto theChipSmallestThresholdContainer =
                            fTheSmallestThresholdAtMaxOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        auto theSmallestThreshold = theChipSmallestThresholdContainer->getSummary<std::pair<std::pair<uint16_t, uint16_t>, uint16_t>>();
                        auto theTargetThreshold =
                            fTheTargetThresholdContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getSummary<uint16_t>();
                        LOG(DEBUG) << BOLDYELLOW << " Chip " << cChip->getId() << " target threshold " << theTargetThreshold << RESET;
                        auto row = theSmallestThreshold.first.first;
                        auto col = theSmallestThreshold.first.second;

                        auto theMinimumThreshold = theSmallestThreshold.second;
                        LOG(DEBUG) << BOLDYELLOW << " Chip " << cChip->getId() << " minimum threshold at minimum Vtrim " << theMinimumThreshold << RESET;

                        LOG(DEBUG) << BOLDYELLOW << " getting threhsold fTheMaxOccupancyThresholdContainers row, col "
                                   << fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())
                                          ->getObject(cOpticalGroup->getId())
                                          ->getObject(cHybrid->getId())
                                          ->getObject(cChip->getId())
                                          ->getChannel<uint16_t>(row, col)
                                   << RESET;

                        auto theCurrentThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())
                                                       ->getObject(cOpticalGroup->getId())
                                                       ->getObject(cHybrid->getId())
                                                       ->getObject(cChip->getId())
                                                       ->getChannel<uint16_t>(row, col);
                        LOG(DEBUG) << BOLDYELLOW << " Chip " << cChip->getId() << " current threshold  " << theCurrentThreshold << RESET;
                        auto theChipDistanceFromTargetContainer =
                            theDistanceFromTargetContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                        auto theChipVTrimBitsContainer = theFinalVTrimContainers.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());

                        auto theCurrentMaxOccupancyThreshold = fTheMaxOccupancyThresholdContainers.getObject(cBoard->getId())
                                                                   ->getObject(cOpticalGroup->getId())
                                                                   ->getObject(cHybrid->getId())
                                                                   ->getObject(cChip->getId())
                                                                   ->getChannel<uint16_t>(row, col);
                        LOG(DEBUG) << BLUE << " theCurrentMaxOccupancyThreshold " << theCurrentMaxOccupancyThreshold << RESET;

                        // first iteration
                        if(ibit == theVTrimBitsNumber - 1) theChipDistanceFromTargetContainer->getSummary<uint16_t>() = std::fabs(theCurrentMaxOccupancyThreshold - theTargetThreshold);

                        uint8_t theVTrimBits = fReadoutChipInterface->ReadVtrim(cChip);
                        LOG(DEBUG) << BLUE << " current Vtrim  " << +theVTrimBits << RESET;

                        float distanceFromTarget = std::fabs(theCurrentMaxOccupancyThreshold - theTargetThreshold);
                        LOG(DEBUG) << MAGENTA << " bit  " << +ibit << " current threshold " << theCurrentMaxOccupancyThreshold << " target " << theTargetThreshold << RESET;

                        if(distanceFromTarget < theChipDistanceFromTargetContainer->getSummary<uint16_t>() || ibit == theVTrimBitsNumber - 1)
                        {
                            theChipDistanceFromTargetContainer->getSummary<uint16_t>() = distanceFromTarget;
                            theChipVTrimBitsContainer->getSummary<uint16_t>()          = theVTrimBits;
                        }

                        ReadoutChip* theReadoutChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        if(theReadoutChip->getFrontEndType() == FrontEndType::MPA2)
                        {
                            // Reject bit if the occupancy is too high
                            if((ibit > 0 && theCurrentMaxOccupancyThreshold > theTargetThreshold)) { theVTrimBits &= ~(1 << ibit); }
                            else { theVTrimBits |= (1 << ibit); }
                        }
                        else if(theReadoutChip->getFrontEndType() == FrontEndType::SSA2)
                        {
                            if((ibit > 0 && theCurrentMaxOccupancyThreshold < theTargetThreshold)) { theVTrimBits &= ~(1 << ibit); }
                            else { theVTrimBits |= (1 << ibit); }
                        }
                        if(ibit > 0) theVTrimBits |= (1 << (ibit - 1)); // Setting next bit to 1 for the test

                        fReadoutChipInterface->SetVtrim(cChip, theVTrimBits);

                        LOG(DEBUG) << BOLDYELLOW << "Vtrim for  HYBRID " << cHybrid->getId() << " CHIP " << cChip->getId() << " is " << +theVTrimBits << RESET;
                        LOG(DEBUG) << BOLDYELLOW << "Distance from target  " << distanceFromTarget << RESET;
                    }
                }
            }
        }
    }
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

void PedestalEqualizationPSAtPedestal::Pause() {}

void PedestalEqualizationPSAtPedestal::Resume() {}

void PedestalEqualizationPSAtPedestal::Reset()
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto theEventType = fEventTypes.getObject(cBoard->getId())->getSummary<EventType>();
        cBoard->setEventType(theEventType);
        if(theEventType != EventType::PSAS) { static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->InitalizeL1ReadoutInterface(cBoard); }
    }
    fRegisterHelper->restoreSnapshot();
}
