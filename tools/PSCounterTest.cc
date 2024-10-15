#include "tools/PSCounterTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string PSCounterTest::fCalibrationDescription = "Insert brief calibration description here";

PSCounterTest::PSCounterTest() : Tool() {}

PSCounterTest::~PSCounterTest() {}

void PSCounterTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramPSCounterTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PSCounterTest::ConfigureCalibration()
{

}

void PSCounterTest::Running()
{
    LOG(INFO) << "Starting PSCounterTest measurement.";
    Initialise();
    RunFast();
    LOG(INFO) << "Done with PSCounterTest.";
    Reset();
}

void PSCounterTest::RunFast()
{
    std::cout<<"Running"<<std::endl;
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
    int fEventsPerPoint     = 100;
    // int fNEventsPerBurst    = 100;
    for(auto cBoard: *fDetectorContainer)
    {
        if(fWithSSA || fWithMPA)
        {
            // Allow for different SSA and MPA injection amplitudes
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();

                        if(cType == FrontEndType::MPA2)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 145);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 225);
                        }
                        else
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 132);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 111);
                        }
                    }
                }
            }
        }

        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", 145);
    }
    std::cout<<"Measuring Data"<<std::endl;

    // this->measureData(fEventsPerPoint, fNEventsPerBurst);
    std::vector<uint32_t> test_data;
    for(auto cBoard: *fDetectorContainer)
    {
        this->ReadNEvents(cBoard, fEventsPerPoint, test_data);
    }
    // this->ReadNEvents(fEventsPerPoint);
}

void PSCounterTest::Stop(void)
{
    LOG(INFO) << "Stopping PSCounterTest measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramPSCounterTest.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "PSCounterTest stopped.";
}

void PSCounterTest::Pause()
{

}


void PSCounterTest::Resume()
{

}


void PSCounterTest::Reset()
{
    fRegisterHelper->restoreSnapshot();
}
