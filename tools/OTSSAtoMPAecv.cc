#include "tools/OTSSAtoMPAecv.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/ReadoutChip.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"
#include "HWInterface/MPA2Interface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTSSAtoMPAecv::fCalibrationDescription = "Can phase and strenght on the SSA to MPA lines";

OTSSAtoMPAecv::OTSSAtoMPAecv() : OTverifyMPASSAdataWord() {}

OTSSAtoMPAecv::~OTSSAtoMPAecv() {}

void OTSSAtoMPAecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fFirstStrip = 6;
    fStripGap   = 6;
    // free the registers in case any
    fNumberOfIterations = findValueInSettings<double>("OTSSAtoMPAecv_NumberOfIterations", 1000);
    fListOfSSAslvsCurrents = convertStringToFloatList(findValueInSettings<std::string>("OTSSAtoMPAecv_ListOfSSAslvsCurrents", "1, 4, 7"));

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTSSAtoMPAecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTSSAtoMPAecv::ConfigureCalibration()
{

}

void OTSSAtoMPAecv::Running()
{
    LOG(INFO) << "Starting OTSSAtoMPAecv measurement.";
    Initialise();
    runSSAtoMPAecvScan();
    LOG(INFO) << "Done with OTSSAtoMPAecv.";
    Reset();
}

void OTSSAtoMPAecv::Stop(void)
{
    LOG(INFO) << "Stopping OTSSAtoMPAecv measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTSSAtoMPAecv.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTSSAtoMPAecv stopped.";
}

void OTSSAtoMPAecv::Pause()
{

}

void OTSSAtoMPAecv::Resume()
{

}

void OTSSAtoMPAecv::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTSSAtoMPAecv::runSSAtoMPAecvScan()
{
    for(uint8_t slvsCurrent : fListOfSSAslvsCurrents)
    {
        for(uint8_t phase = 0; phase < 2; ++phase)
        {
            LOG(INFO) << BOLDGREEN << "MPA sampling phase = " << +phase << " SSA SLVS current = " << +slvsCurrent << RESET;
            //reset fPatternMatchingEfficiencyContainer
            for(auto theBoard: fPatternMatchingEfficiencyContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        theHybrid->getSummary<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>() = GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>();
                    }
                }
            }

            // setting phases and driver strenghts
            for(auto theBoard: *fDetectorContainer)
            {
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        for(auto theChip: *theHybrid)
                        {
                            if(theChip->getFrontEndType() == FrontEndType::SSA2)
                            {
                                std::vector<std::pair<std::string, uint16_t>> ssaRegisterList;
                                ssaRegisterList.push_back({"SLVS_pad_current_L1"      , slvsCurrent});
                                ssaRegisterList.push_back({"SLVS_pad_current_Stub_0_1", slvsCurrent | (slvsCurrent << 3)});
                                ssaRegisterList.push_back({"SLVS_pad_current_Stub_2_3", slvsCurrent | (slvsCurrent << 3)});
                                ssaRegisterList.push_back({"SLVS_pad_current_Stub_4_5", slvsCurrent | (slvsCurrent << 3)});
                                ssaRegisterList.push_back({"SLVS_pad_current_Stub_6_7", slvsCurrent | (slvsCurrent << 3)});

                                fReadoutChipInterface->WriteChipMultReg(theChip, ssaRegisterList);
                            }
                            else if(theChip->getFrontEndType() == FrontEndType::MPA2)
                            {
                                auto theMPAInterface = static_cast<MPA2Interface*>(fReadoutChipInterface);
                                if(phase == 0)
                                {
                                    theMPAInterface->WriteChipReg(theChip, "EdgeSelTrig", 0x00);
                                    theMPAInterface->WriteChipRegBits(theChip, "EdgeSelT1Raw", 0x00, "Mask", 0x01);
                                }
                                else
                                {
                                    theMPAInterface->WriteChipReg(theChip, "EdgeSelTrig", 0xFF);
                                    theMPAInterface->WriteChipRegBits(theChip, "EdgeSelT1Raw", 0x01, "Mask", 0x01);
                                }
                            }
                        }
                    }
                }
            }
            runIntegrityTest();
            
#ifdef __USE_ROOT__
            fDQMHistogramOTSSAtoMPAecv.fillPatternEfficiencyScan(fPatternMatchingEfficiencyContainer, phase, slvsCurrent);
#else
            if(fDQMStreamerEnabled)
            {
                ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTSSAtoMPAecvPatternMatchingEfficiency");
                thePatternMatchinEfficiencyContainerSerialization.streamByHybridContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer, phase, slvsCurrent);
            }
#endif
        }
    }

}

std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> OTSSAtoMPAecv::produceMatchingPixelClusterList(uint8_t colCoordinate)
{
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> thePixelClusterList;
    thePixelClusterList.push_back({fStubRowCoordinate, colCoordinate, 1}); // matching strip cluster
    uint8_t centroidCode = 2*colCoordinate + 9;
    uint8_t centroidCodeNegativeShift = centroidCode >> 1;
    uint8_t centroidCodePositiveShift = centroidCode << 1;
    std::vector<uint8_t> centroidList {centroidCodeNegativeShift, centroidCodePositiveShift};
    for(auto centroid: centroidList)
    {
        thePixelClusterList.push_back({fStubRowCoordinate, (centroid-9) >> 1, (centroid-9)%2 + 1}); 
    }

    return thePixelClusterList;
}

void OTSSAtoMPAecv::matchAllPossibleStubPatterns(uint8_t numberOfBytesInSinglePacket, size_t numberOfLines, std::vector<std::pair<PatternMatcher, float>>& thePatternAndEfficiencyList, const std::vector<uint32_t>& concatenatedStubPackage, Ph2_HwDescription::ReadoutChip* theMPA)
{
    for(auto& thePatternAndEfficiency: thePatternAndEfficiencyList)
    {
        float maximumNumberOfMatchedStubs = 0;
        for(uint8_t numberOfPacketsToSkip = 0; numberOfPacketsToSkip < numberOfLines * 8; ++numberOfPacketsToSkip)
        {
            float currentNumberOfMatchedStubs = 0;
            std::vector<uint32_t> theShiftedWordVector = applyByteShift(concatenatedStubPackage, numberOfBytesInSinglePacket, numberOfPacketsToSkip);
            for(size_t stubNumber = 0; stubNumber < 8; ++stubNumber)
            {
                if(thePatternAndEfficiency.first.isSubsetMatched(theShiftedWordVector, 29 + 21 * stubNumber, 21)) ++currentNumberOfMatchedStubs;
            }
            if(maximumNumberOfMatchedStubs < currentNumberOfMatchedStubs) maximumNumberOfMatchedStubs = currentNumberOfMatchedStubs;
            if(currentNumberOfMatchedStubs == 8) break;
        }
        thePatternAndEfficiency.second += (maximumNumberOfMatchedStubs / 8);
    }

    return;
}
