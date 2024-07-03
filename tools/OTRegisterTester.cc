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
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            //FIXME check lpgbt
            //static_cast<D19clpGBTInterface*>(flpGBTInterface)->setCICClockPolarityAndStrength(theOpticalGroup->flpGBT, clockPolarity, clockStrength, theOpticalGroup);


            for(auto cHybrid: *theOpticalGroup)
            {
                // auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                //FIXME check CIC
                //fCicInterface->ConfigureDriveStrength(cCic, cicStrength);
                for(auto theChip: *cHybrid)
                {
                    float theRegisterMatchingEfficiency = 0;
                    for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                    {
                    
                        uint16_t theRegisterValueWrite = 0x01;
                        fReadoutChipInterface->WriteChipReg(theChip, "Threshold", theRegisterValueWrite);
                        auto theRegisterValueRead = fReadoutChipInterface->ReadChipReg(theChip, "Threshold");

                        if (theRegisterValueRead == theRegisterValueWrite ) theRegisterMatchingEfficiency++;

                    }
                    theRegisterMatchingEfficiency/=fNumberOfIterations;
                    LOG(INFO) << BOLDBLUE << " Pattern matching efficiency for chip " << +theChip->getId() << " on hybrid " << +cHybrid->getId() << " is " << theRegisterMatchingEfficiency << RESET;
                    //FIXME check chips
                }// chip loop
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
