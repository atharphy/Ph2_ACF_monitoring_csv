#include "tools/OTalignStubPackage.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/DataContainer.h"
#include "Utils/ContainerSerialization.h"
#include <algorithm>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignStubPackage::fCalibrationDescription = "Find stub package delay to properly decode stubs in the FC7";

OTalignStubPackage::OTalignStubPackage() : Tool() {}

OTalignStubPackage::~OTalignStubPackage() {}

void OTalignStubPackage::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs.stub_package_delay");
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9");
    fRegisterHelper->freeBoardRegister("fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11");

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignStubPackage.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignStubPackage::ConfigureCalibration() {}

void OTalignStubPackage::Running()
{
    LOG(INFO) << "Starting OTalignStubPackage measurement.";
    Initialise();
    AlignStubPackage();
    LOG(INFO) << "Done with OTalignStubPackage.";
    Reset();
}

void OTalignStubPackage::Stop(void)
{
    LOG(INFO) << "Stopping OTalignStubPackage measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTalignStubPackage.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignStubPackage stopped.";
}

void OTalignStubPackage::Pause() {}

void OTalignStubPackage::Resume() {}

void OTalignStubPackage::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTalignStubPackage::AlignStubPackage()
{
    uint16_t numberOfEvents = 10;
    uint16_t triggerFrequency = 400; // kHz
    uint16_t clockFrequency = 40000; // kHz
    uint16_t cMaxBxCounter = 3564;
    uint16_t numberOfClockCyclesAfterInitialReset = 100; // safety margin to avoid roll over
    uint16_t numberOfClockCyclesBetweenTwoConsecutiveTriggers = clockFrequency / triggerFrequency;

    if(numberOfClockCyclesBetweenTwoConsecutiveTriggers != float(clockFrequency/float(triggerFrequency)))
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Error: numberOfClockCyclesBetweenTwoConsecutiveTriggers must be an integer! Aborting..." << std::endl;
        abort();
    }
    if(numberOfClockCyclesAfterInitialReset + numberOfEvents * numberOfClockCyclesBetweenTwoConsecutiveTriggers >= cMaxBxCounter)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Error: BxId roll over not handled by the procedure! Aborting" << std::endl;
        abort();
    }

    std::vector<uint16_t> emptyBunchCrossingId(numberOfEvents, 0xFFFF);
    DetectorDataContainer theBunchCrossingIdContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, theBunchCrossingIdContainer, emptyBunchCrossingId);

    std::vector<bool> emptyBestPackageDelay(8, false);
    DetectorDataContainer theBestPackageDelayContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<bool>>(*fDetectorContainer, theBestPackageDelayContainer, emptyBestPackageDelay);

    std::vector<uint16_t> emptyBunchCrossingIdDifference(numberOfEvents-1, 0x7FFF);
    DetectorDataContainer theBunchCrossingIdDifferenceContainer;
    ContainerFactory::copyAndInitHybrid<std::vector<uint16_t>>(*fDetectorContainer, theBunchCrossingIdDifferenceContainer, emptyBunchCrossingIdDifference);

    for(auto theBoard: *fDetectorContainer)
    {
        // set board and get interface
        fBeBoardInterface->setBoard(theBoard->getId());
        auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        // make sure you're only sending one trigger at a time here
        LOG(INFO) << GREEN << "Trying to align CIC stub decoder in the back-end" << RESET;

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                // disable all FEs
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
            }
        }

        std::vector<std::pair<std::string, uint32_t>> initialRegisterVector;
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", triggerFrequency});   
        initialRegisterVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 1}); // Ensure a fast reset is sent before reading events to avoid roll over
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
        initialRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", 0});
        initialRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
        fBeBoardInterface->WriteBoardMultReg(theBoard, initialRegisterVector);

        for(uint8_t thePackageDelay = 0; thePackageDelay < 8; thePackageDelay++)
        {
            // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] thePackageDelay = " << +thePackageDelay << std::endl;
            
            uint32_t packageDelayValue = 0;
            for(size_t link=0; link<10; ++link)
            {
                packageDelayValue = packageDelayValue | (thePackageDelay << (3*link));
            }

            std::vector<std::pair<std::string, uint32_t>> packageDelayRegisterVector;
            packageDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9"  , packageDelayValue});
            packageDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11", packageDelayValue & 0x3F});
            fBeBoardInterface->WriteBoardMultReg(theBoard, packageDelayRegisterVector);

            // reset stub readout to load new setting
            cInterface->Bx0Alignment();

            // read events
            ReadNEvents(theBoard, numberOfEvents);
            const std::vector<Event*>& theEventVector = this->GetEvents();
            
            // retrieve bunch crossing id for all events
            for(size_t eventNumber = 0; eventNumber<numberOfEvents; ++eventNumber)
            {
                auto theEvent = theEventVector[eventNumber];
                for(auto theOpticalGroup: *theBoard)
                {
                    for(auto theHybrid: *theOpticalGroup)
                    {
                        auto& eventBxIdVector = theBunchCrossingIdContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<uint16_t>>();
                        eventBxIdVector[eventNumber] = theEvent->BxId(theHybrid->getId());
                        if(eventNumber > 0)
                        {
                            auto& bxIdDifferenceVector = theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<int16_t>>();
                            bxIdDifferenceVector[eventNumber-1] = eventBxIdVector[eventNumber] - eventBxIdVector[eventNumber-1];
                        }
                    }
                }
            }

            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto theBunchCrossingIdDifference = theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<int16_t>>();
                    // std::cout << "Hybrid id = " << theHybrid->getId() << std::endl;
                    // for(auto bxIdDifference: theBunchCrossingIdDifference) std::cout << bxIdDifference << " ";
                    // std::cout << std::endl;
                }
            }

            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto theBunchCrossingIdDifference = theBunchCrossingIdDifferenceContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<uint16_t>>();
                    //remove duplicate
                    std::sort( theBunchCrossingIdDifference.begin(), theBunchCrossingIdDifference.end() );
                    theBunchCrossingIdDifference.erase( unique( theBunchCrossingIdDifference.begin(), theBunchCrossingIdDifference.end() ), theBunchCrossingIdDifference.end() );
                    bool isSameAndCorrectBx = theBunchCrossingIdDifference.size() == 1 && theBunchCrossingIdDifference[0] == numberOfClockCyclesBetweenTwoConsecutiveTriggers;
                    theBestPackageDelayContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<bool>>()[thePackageDelay] = isSameAndCorrectBx;
                }
            }
        }

        uint32_t bestPackageDelayLink0Link9   = 0;
        uint32_t bestPackageDelayLink10Link11 = 0;

        for(auto theOpticalGroup: *theBoard)
        {
            std::vector<uint8_t> hybridBestPackageDelay;
            for(auto theHybrid: *theOpticalGroup)
            {
                auto theBestPackageDelayVector = theBestPackageDelayContainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<bool>>();
                auto numberOfBestPackageDelays = std::count(theBestPackageDelayVector.begin(), theBestPackageDelayVector.end(), true);
                if(numberOfBestPackageDelays != 1)
                {
                    LOG(ERROR) << BOLDRED << "ERROR for Board " << +theBoard->getId() << " OpticalGroup " << +theOpticalGroup->getId()<< " Hybrid " << +theHybrid->getId() << ": number of best package delay = " << numberOfBestPackageDelays << ", expected to be 1" << RESET;
                    continue;
                }
                hybridBestPackageDelay.push_back(std::find_if(theBestPackageDelayVector.begin(), theBestPackageDelayVector.end(), [](bool value){return value;}) - theBestPackageDelayVector.begin()); //find intex of the best phase
            }
            if(hybridBestPackageDelay.size() > 0)
            {
                //remove duplicate
                std::sort( hybridBestPackageDelay.begin(), hybridBestPackageDelay.end() );
                hybridBestPackageDelay.erase( unique( hybridBestPackageDelay.begin(), hybridBestPackageDelay.end() ), hybridBestPackageDelay.end() );
                if(hybridBestPackageDelay.size() > 1)
                {
                    LOG(ERROR) << BOLDRED << "ERROR for Board " << +theBoard->getId() << " OpticalGroup " << +theOpticalGroup->getId() <<": FW cannot handle different stub package delay within same OpticalGroup, setting the value found for Hybrid " << +theOpticalGroup->getFirstObject()->getId() << RESET;
                }

                if(theOpticalGroup->getId() < 10)
                    bestPackageDelayLink0Link9 = (hybridBestPackageDelay[0] << theOpticalGroup->getId() % 10 * 3) | bestPackageDelayLink0Link9;
                else
                    bestPackageDelayLink10Link11 = (hybridBestPackageDelay[0] << theOpticalGroup->getId() % 10 * 3) | bestPackageDelayLink10Link11;
            }
        }

        std::vector<std::pair<std::string, uint32_t>> finalDelayRegisterVector;
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link0_link9"  , bestPackageDelayLink0Link9});
        finalDelayRegisterVector.push_back({"fc7_daq_cnfg.physical_interface_block.stubs_package_delay_link10_link11", bestPackageDelayLink10Link11});
        fBeBoardInterface->WriteBoardMultReg(theBoard, finalDelayRegisterVector);
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTalignStubPackage.fillBestStubPackageDelay(theBestPackageDelayContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theBestStubPackageDelayContainerSerialization("OTalignStubPackageBestStubPackageDelay");
        theBestStubPackageDelayContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestPackageDelayContainer);
    }
#endif
}
