#include "tools/TestPSEvents.h"
#include "HWDescription/MPA2.h"
#include "HWDescription/SSA2.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/D19cCic2Event.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/Utilities.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string TestPSEvents::fCalibrationDescription = "Insert brief calibration description here";

TestPSEvents::TestPSEvents() : OTinjectionDelayOptimization() {}

TestPSEvents::~TestPSEvents() {}

void TestPSEvents::Initialise(void)
{
    OTinjectionDelayOptimization::Initialise();
    // free the registers in case any

    // for(auto theBoard: *fDetectorContainer) { fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 0); }

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramTestPSEvents.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void TestPSEvents::ConfigureCalibration() {}

void TestPSEvents::Running()
{
    LOG(INFO) << "Starting TestPSEvents measurement.";
    Initialise();
    testEvents(10, 80, {0, 1, 8, 9});
    LOG(INFO) << "Done with TestPSEvents.";
    Reset();
}

void TestPSEvents::Stop(void)
{
    LOG(INFO) << "Stopping TestPSEvents measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramTestPSEvents.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "TestPSEvents stopped.";
}

void TestPSEvents::Pause() {}

void TestPSEvents::Resume() {}

void TestPSEvents::Reset() { fRegisterHelper->restoreSnapshot(); }

void TestPSEvents::testEvents(uint8_t injectedRow, uint8_t injectedCol, const std::vector<uint8_t>& listOfInjectedChipId)
{
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Injecting pixel and strip with row " << +injectedRow << " and col " << +injectedCol << std::endl;
    std::vector<Cluster> theClusterList;
    theClusterList.push_back(Cluster(injectedRow, injectedCol, 1));
    theClusterList.push_back(Cluster(injectedRow, injectedCol + 10, 1));
    theClusterList.push_back(Cluster(injectedRow, injectedCol - 10, 1));

    setFWTestPulse(false);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                for(auto theChip: *theHybrid)
                {
                    if(std::find(listOfInjectedChipId.begin(), listOfInjectedChipId.end(), theChip->getId()) != listOfInjectedChipId.end())
                    {
                        static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, theClusterList);
                    }
                    else { static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, {}); }
                }
            }
        }
    }

    auto theBoard = fDetectorContainer->getFirstObject();

    for(uint32_t stubPackage = 0; stubPackage < 8; ++stubPackage)
    {
        std::cout << "----------  Stub package delay = " << stubPackage << " ----------" << std::endl;
        uint32_t combinedStubDelay = 0;
        for(uint8_t linkId = 0; linkId < 10; ++linkId) { combinedStubDelay |= (stubPackage << (3 * linkId)); }

        std::bitset<32> theBitset(combinedStubDelay);
        std::cout << "----------  combinedStubDelay = " << theBitset << " ----------" << std::endl;

        std::vector<std::pair<std::string, uint32_t>> finalDelayRegisterVector;
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_hybrid0_link0_link9", combinedStubDelay});
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_hybrid0_link10_link11", combinedStubDelay});
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_hybrid1_link0_link9", combinedStubDelay});
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_hybrid1_link10_link11", combinedStubDelay});
        fBeBoardInterface->WriteBoardMultReg(theBoard, finalDelayRegisterVector);

        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
        cInterface->Bx0Alignment();

        std::vector<uint32_t> theDataVector;
        fBeBoardInterface->ReadNEvents(theBoard, 1, theDataVector, true);

        for(size_t the128bitWordCounter = 0; the128bitWordCounter < theDataVector.size() / 4; ++the128bitWordCounter)
        {
            std::vector<uint32_t> dataSubset(theDataVector.begin() + the128bitWordCounter * 4, theDataVector.begin() + the128bitWordCounter * 4 + 4);
            std::cout << getPatternPrintout(dataSubset, 1) << std::endl;
        }
    }

    // this->DecodeData(theBoard, theDataVector, 5, fBeBoardInterface->getBoardType(theBoard));

    // for(auto theEvent: fEventList)
    // {
    //     std::cout << std::endl << std::endl << std::endl;
    //     static_cast<D19cCic2Event*>(theEvent)->print();
    // }
}
