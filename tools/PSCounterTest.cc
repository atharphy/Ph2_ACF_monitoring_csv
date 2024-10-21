#include "tools/PSCounterTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

#include "HWDescription/BeBoardRegItem.h"
#include "HWDescription/Cbc.h"
#include "HWDescription/MPA2.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cPSCounterFWInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/EmptyContainer.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/ThresholdAndNoise.h"
#include "boost/format.hpp"
#include <math.h>
#include "Utils/Timer.h"

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
    std::cout << "Initializing" << std::endl;

    for(auto cBoard: *fDetectorContainer)
    {
        BeBoardRegMap cRegMap      = cBoard->getBeBoardRegMap();
        uint32_t      cTriggerFreq = cRegMap["fc7_daq_cnfg.fast_command_block.user_trigger_frequency"].fValue;

        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.clear();
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerFreq});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        //        LOG(INFO) << BOLDYELLOW << "Noise measured on BeBoard#" << +cBoard->getId() << " with a trigger rate of " << cTriggerFreq << "kHz." << RESET;
    }
    fDisableStubLogic = false;

    this->enableTestPulse(true); // For Testing Purposes

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

    // initializeRecycleBin();

    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.getObject(cBoard->getId())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // make sure register tracking is on
    for(auto board: *fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    chip->setRegisterTracking(1);
                    chip->ClearModifiedRegisterMap();
                }
            }
        }
    }

    // for now.. force to use async mode here (does not track timing of hits), only used for testing
    // event types
    fEventTypes.clear();
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.push_back(cBoard->getEventType());
        if(!fWithSSA && !fWithMPA) continue;
        cBoard->setEventType(EventType::PSAS); // Sets up board to expect async input
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(cBoard))->InitializePSCounterFWInterface(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1); } // Tells chip to expect async
            }
        }
    }

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramPSCounterTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PSCounterTest::ConfigureCalibration() {}

void PSCounterTest::Running()
{
    LOG(INFO) << "Starting PSCounterTest measurement.";
    Initialise();
    Timer       scanTimer;
    scanTimer.start();

    uint16_t  thrOffset   = findValueInSettings<double>("PSCounterTest_thrOffset", 0);
    RunFast(75 + thrOffset, 210 + thrOffset);

    // for(uint16_t thrOffset = 0; thrOffset <= 30; thrOffset+=1)
    // {
    //     RunFast(65 + thrOffset, 200 + thrOffset);
    //     usleep(3000000);
    // }

    scanTimer.stop();
    scanTimer.show("Scan time: ");

    LOG(INFO) << "Done with PSCounterTest.";
    Reset();
}

void PSCounterTest::RunFast(uint16_t stripThreshold, uint16_t pixelThreshold)
{
    std::cout << "Running" << std::endl;
    // fWithSSA = false;
    // fWithMPA = false;
    // std::vector<FrontEndType> cAllFrontEndTypes;
    // for(auto cBoard: *fDetectorContainer)
    // {
    // auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
    // fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
    // fWithMPA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
    // for(auto cFrontEndType: cFrontEndTypes)
    // {
    // if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
    // }
    // }
    // int fNEventsPerBurst    = 0x5555;
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
                            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] pixelThreshold = " << pixelThreshold << std::endl;

                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 250);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", pixelThreshold);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", pixelThreshold);
                        }
                        else
                        {
                            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] stripThreshold = " << stripThreshold << std::endl;
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 100);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", stripThreshold);
                            // fReadoutChipInterface->WriteChipReg(cChip, "ENFLAGS", 0x15);
                            // fReadoutChipInterface->WriteChipReg(cChip, "CalPulse_duration", 0x1);
                            // fReadoutChipInterface->WriteChipReg(cChip, "StripControl2", 0x7);
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] ENFLAGS_S1 = " << std::hex << fReadoutChipInterface->ReadChipReg(cChip, "ENFLAGS_S1") << std::dec << std::endl;
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] InjectedCharge = " << std::hex << fReadoutChipInterface->ReadChipReg(cChip, "InjectedCharge") << std::dec << std::endl;
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Threshold = " << std::hex << fReadoutChipInterface->ReadChipReg(cChip, "Threshold") << std::dec << std::endl;
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] CalPulse_duration = " << std::hex << fReadoutChipInterface->ReadChipReg(cChip, "CalPulse_duration") << std::dec << std::endl;
                            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] StripControl2 = " << std::hex << fReadoutChipInterface->ReadChipReg(cChip, "StripControl2") << std::dec << std::endl;
                            
                        }
                    }
                }
            }
        }
    }
    std::cout << "Measuring Data" << std::endl;
    auto theBoard = fDetectorContainer->getFirstObject();


    int fEventsPerPoint     = 254;
    auto theFWinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
    auto dL1Interface = static_cast<D19cPSCounterFWInterface*>(theFWinterface->getL1ReadoutInterface());

    std::string outputFileName = fDirectoryName + "/fastCounter_StripTh_" +  std::to_string(stripThreshold) + "_PixelTh_" + std::to_string(pixelThreshold) + ".txt";
    dL1Interface->setOutputFile(outputFileName);

    dL1Interface->setNEvents(fEventsPerPoint);
    dL1Interface->ReadEvents(theBoard);
    uint16_t mpa_lsb11 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_LSB",1,1));
    uint16_t mpa_msb11 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(8), MPA2::getPixelRegisterName("ReadCounter_MSB",1,1));
   
    std::cout<<"MPA="<< std::dec << (mpa_lsb11 | (mpa_msb11<<8)) <<std::endl;

    uint16_t ssa_lsb1 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterLSB",1));
    uint16_t ssa_msb1 = fReadoutChipInterface->ReadChipReg(fDetectorContainer->getFirstObject()->getFirstObject()->getFirstObject()->getObject(0), SSA2::getStripRegisterName("AC_ReadCounterMSB",1));

    std::cout<<"SSA="<< std::dec << (ssa_lsb1 | (ssa_msb1<<8)) <<std::endl;

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

void PSCounterTest::Pause() {}

void PSCounterTest::Resume() {}

void PSCounterTest::Reset() { fRegisterHelper->restoreSnapshot(); }
