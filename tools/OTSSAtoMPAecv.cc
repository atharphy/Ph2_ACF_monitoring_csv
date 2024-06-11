#include "tools/OTSSAtoMPAecv.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/ReadoutChip.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"

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

    fFirstStrip = 5;
    fStripGap   = 7;
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
    for(uint8_t phase = 0; phase < 8; ++phase)
    {
        for(uint8_t slvsCurrent = 0; slvsCurrent < 8; ++slvsCurrent)
        {
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
                                fReadoutChipInterface->WriteChipReg(theChip, "LatencyRx320", phase | (phase << 3));
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
    thePixelClusterList.push_back({fStubRowCoordinate, colCoordinate << 1, 1});
    thePixelClusterList.push_back({fStubRowCoordinate, colCoordinate >> 1, colCoordinate % 2 + 1});

    return thePixelClusterList;
}
