#include "tools/OTRegisterTester.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTRegisterTester::fCalibrationDescription = "Check the stability of the I2C register writing";

OTRegisterTester::OTRegisterTester() : Tool() {}

OTRegisterTester::~OTRegisterTester() {}

void OTRegisterTester::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fNumberOfIterations = findValueInSettings<double>("OTRegisterTester_NumberOfIterations", 100);
    fPattern            = findValueInSettings<double>("OTRegisterTester_Pattern", 0xAA);

    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTRegisterTester.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTRegisterTester::ConfigureCalibration() {}

void OTRegisterTester::Running()
{
    LOG(INFO) << BOLDMAGENTA << "Starting OTRegisterTester measurement." << RESET;
    Initialise();
    TestRegisters();
    LOG(INFO) << BOLDMAGENTA << "Done with OTRegisterTester." << RESET;
    Reset();
}
void OTRegisterTester::TestRegisters()
{
    int  numberOfReadoutChips = NCHIPS_OT;
    bool isPS                 = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS;
    if(isPS) numberOfReadoutChips = NCHIPS_OT * 2;
    int                      totalNumberOfChips = numberOfReadoutChips + 1;
    std::vector<std::string> theReadoutChipRegisters{"Threshold", "TriggerLatency1", "Vplus1&2", "Channel001", "Channel254"}; // CBCs chosen registers from both page 0 and page 1. PS registers below
    std::vector<std::string> theCICRegisters{"scPhaseSelectB0i0", "scPhaseSelectB2i5", "scDllCurrentSet2", "EXT_WA_DELAY14", "CALIB_PATTERN3"}; // checking different blocks
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto cHybrid: *theOpticalGroup)
            {
                auto& theRegisterMatchingEfficiency = fPatternMatchingEfficiencyContainer.getObject(cHybrid->getBeBoardId())
                                                          ->getObject(cHybrid->getOpticalGroupId())
                                                          ->getObject(cHybrid->getHybridId())
                                                          ->getSummary<std::vector<float>>();
                theRegisterMatchingEfficiency.assign(totalNumberOfChips, 0);
                for(auto theChip: *cHybrid)
                {
                    if(isPS)
                    {
                        if(theChip->getFrontEndType() == FrontEndType::MPA2)
                            theReadoutChipRegisters = {"Threshold", "ECM", "LatencyRx320", "PixelControl_R7", "TrimDAC_C22_R3"};
                        else // SSA2
                            theReadoutChipRegisters = {"Threshold", "control_3", "ClockDeskewing_coarse", "DigCalibPattern_L_S2"};
                    }
                    theRegisterMatchingEfficiency[theChip->getId()] = EfficiencyCalculator(theChip, theReadoutChipRegisters);
                } // chip loop

                LOG(DEBUG) << BOLDMAGENTA << " Done with chips. Moving to CIC" << RESET;
                auto& cCic                                            = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                theRegisterMatchingEfficiency[totalNumberOfChips - 1] = EfficiencyCalculator(cCic, theCICRegisters);

#ifdef __USE_ROOT__
                fDQMHistogramOTRegisterTester.fillPatternMatchingEfficiencyResults(fPatternMatchingEfficiencyContainer);
#else
                if(fDQMStreamerEnabled)
                {
                    ContainerSerialization thePatternMatchinEfficiencyContainerSerialization("OTRegisterTesterPatternMatchingEfficiency");
                    thePatternMatchinEfficiencyContainerSerialization.streamByHybridContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer);
                }
#endif
            } // hybrid loop
        }     // optical group loop
    }         // board loop
}

float OTRegisterTester::EfficiencyCalculator(Ph2_HwDescription::Chip* theChip, std::vector<std::string> theRegisters)
{
    float theEfficiency = 0;
    for(auto registerIterator: theRegisters)
    {
        for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
        {
            auto thePattern = fPattern;
            if(iteration % 2 == 0) thePattern = ~fPattern;
            uint16_t theRegisterValueRead = 0;
            if(theChip->getFrontEndType() != FrontEndType::CIC2)
            {
                fReadoutChipInterface->WriteChipReg(theChip, registerIterator, thePattern);
                theRegisterValueRead = fReadoutChipInterface->ReadChipReg(theChip, registerIterator);
            }
            else
            {
                fCicInterface->WriteChipReg(theChip, registerIterator, thePattern);
                theRegisterValueRead = fCicInterface->ReadChipReg(theChip, registerIterator);
            }
            if(theRegisterValueRead == thePattern) theEfficiency++;
        }
    }
    return theEfficiency /= (fNumberOfIterations * theRegisters.size());
}

void OTRegisterTester::Stop(void)
{
    LOG(INFO) << "Stopping OTRegisterTester measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTRegisterTester.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTRegisterTester stopped.";
}

void OTRegisterTester::Pause() {}

void OTRegisterTester::Resume() {}

void OTRegisterTester::Reset() { fRegisterHelper->restoreSnapshot(); }
