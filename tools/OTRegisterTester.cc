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
    fPattern      = findValueInSettings<double>("OTRegisterTester_Pattern", 0xAA);

    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer);

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTRegisterTester.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTRegisterTester::ConfigureCalibration()
{

}

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
    int numberOfReadoutChips = NCHIPS_OT;
    bool isPS = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS;
    if(isPS) numberOfReadoutChips=NCHIPS_OT*2;
    int totalNumberOfChips = numberOfReadoutChips+1;
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
                theRegisterMatchingEfficiency.assign(totalNumberOfChips,0);
                for(auto theChip: *cHybrid)
                {
                    float theEfficiency = 0;
                    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                    {
                    
                        uint16_t theRegisterValueWrite = fPattern;
                        LOG(DEBUG) << BOLDBLUE << " Pattern for matching " << std::hex << +fPattern << std::dec << RESET;
                        fReadoutChipInterface->WriteChipReg(theChip, "Threshold", theRegisterValueWrite);
                        auto theRegisterValueRead = fReadoutChipInterface->ReadChipReg(theChip, "Threshold");

                        if (theRegisterValueRead == theRegisterValueWrite ) theEfficiency++;

                    }
                    theEfficiency/=fNumberOfIterations;
                    theRegisterMatchingEfficiency[theChip->getId()] = theEfficiency;
                    LOG(DEBUG) << BOLDBLUE << " Pattern matching efficiency for chip " << +theChip->getId() << " on hybrid " << +cHybrid->getId() << " is " << theEfficiency << RESET;
                    //FIXME check chips
                }// chip loop
                
                LOG(DEBUG) << BOLDMAGENTA << " Done with chips. Moving to CIC" << RESET;
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                float theEfficiency = 0;
                for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                {
                    uint16_t theRegisterValueWrite = fPattern;
                    LOG(DEBUG) << BOLDBLUE << " Pattern for matching " << std::hex << +fPattern << std::dec << RESET;
                    fCicInterface->WriteChipReg(cCic, "scPhaseSelectB0i0", theRegisterValueWrite);
                    auto theRegisterValueRead = fCicInterface->ReadChipReg(cCic, "scPhaseSelectB0i0");
                    if (theRegisterValueRead == theRegisterValueWrite ) theEfficiency++;

                }
                theEfficiency/=fNumberOfIterations;
                theRegisterMatchingEfficiency[totalNumberOfChips-1] = theEfficiency; //last vector position for CIC
                LOG(DEBUG) << BOLDBLUE << " Pattern matching efficiency for CIC " << +cCic->getId() << " on hybrid " << +cHybrid->getId() << " is " << theEfficiency << RESET;

                // here I should append the CIC efficiency
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
        } // optical group loop
    } // board loop
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

void OTRegisterTester::Pause()
{

}


void OTRegisterTester::Resume()
{

}


void OTRegisterTester::Reset()
{
    fRegisterHelper->restoreSnapshot();
}
