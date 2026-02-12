#include "tools/OTPhysics.h"
#include "HWDescription/Cbc.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cL1ReadoutInterface.h"
#include "HWInterface/D19cTriggerInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/StartInfo.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

std::string OTPhysics::fCalibrationDescription = "Take data";

void OTPhysics::ConfigureCalibration()
{
    fRegisterHelper->takeSnapshot();

    // #######################
    // # Retrieve parameters #
    // #######################
    fSaveRawData                  = this->findValueInSettings<double>("OTPhysics_SaveRawData", 1);
    uint8_t theUserTriggerRate    = this->findValueInSettings<double>("OTPhysics_UserTriggerRate", 5);
    uint8_t injectionType         = this->findValueInSettings<double>("OTPhysics_InjectionType", 0); // 0 - no injection, 1 - noise injection, 2 - pulse injection
    bool    sparsificationEnabled = this->findValueInSettings<double>("OTPhysics_SparsificationEnabled", 1) > 0;

    uint8_t theTriggerSource = 0xF;

    switch(injectionType)
    {
    case 0:
    case 1:
        theTriggerSource = 3; // User-Defined Frequency
        break;

    case 2:
        theTriggerSource = 6; // Test Pulse Trigger
        break;

    default: break;
    }

    std::vector<std::pair<std::string, uint32_t>> boardRegisterVector;
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", theTriggerSource});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});

    if(injectionType == 2) setFWTestPulse(true);

    for(auto theBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardMultReg(theBoard, boardRegisterVector);
        if(theTriggerSource == 3) { fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency", theUserTriggerRate); }
        else if(theTriggerSource == 6)
        {
            std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
            // this cannot be changed otherwise latency changes
            uint32_t delayAfterTestPulse         = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse");
            uint32_t currentDelayAfterFastReset  = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset");
            uint32_t minimumDelayAfterFastReset  = 20;
            uint32_t minimumDelayBeforeNextPulse = 20;
            uint32_t clockFrequency              = 40e3;                                // in kHz
            uint32_t numberOfClockCyclesBudget   = clockFrequency / theUserTriggerRate; // in clock cycles (40 MHz clock)
            uint32_t newDelayAfterFastReset;
            uint32_t newDelayBeforeNextPulse;
            if(numberOfClockCyclesBudget < minimumDelayAfterFastReset + minimumDelayBeforeNextPulse + delayAfterTestPulse)
            {
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
                newDelayAfterFastReset  = minimumDelayAfterFastReset;
                newDelayBeforeNextPulse = minimumDelayBeforeNextPulse;
                LOG(WARNING) << WARNING_FORMAT << "Requested trigger rate too high for test pulse injection. Trigger rate reduced to "
                             << clockFrequency / (minimumDelayAfterFastReset + minimumDelayBeforeNextPulse + delayAfterTestPulse) << RESET;
            }
            else if(numberOfClockCyclesBudget < currentDelayAfterFastReset + minimumDelayBeforeNextPulse + delayAfterTestPulse)
            {
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << std::endl;
                newDelayAfterFastReset  = numberOfClockCyclesBudget - minimumDelayBeforeNextPulse - delayAfterTestPulse;
                newDelayBeforeNextPulse = minimumDelayBeforeNextPulse;
            }
            else
            {
                newDelayAfterFastReset  = currentDelayAfterFastReset;
                newDelayBeforeNextPulse = numberOfClockCyclesBudget - currentDelayAfterFastReset - delayAfterTestPulse;
            }

            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", newDelayAfterFastReset);
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", newDelayBeforeNextPulse);
        }

        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);

        setSparsification(theBoard, sparsificationEnabled);
        if(injectionType == 1 || injectionType == 2)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theChip: *theHybrid)
                    {
                        if(theChip->getFrontEndType() == FrontEndType::MPA2 || theChip->getFrontEndType() == FrontEndType::SSA2)
                        {
                            if(injectionType == 1)
                            {
                                std::vector<Cluster> theClusterList{};
                                if(theChip->getId() % 8 == 2) { theClusterList.push_back(Cluster(10, 10, 1)); }
                                static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, theClusterList);
                            }
                            else if(injectionType == 2)
                            {
                                uint16_t col = 10;
                                if(theChip->getFrontEndType() == FrontEndType::SSA2)
                                {
                                    std::shared_ptr<ChannelGroup<1, NSSACHANNELS>> theSSAChannelGroup = std::make_shared<ChannelGroup<1, NSSACHANNELS>>();
                                    theSSAChannelGroup->disableAllChannels();
                                    theSSAChannelGroup->enableChannel(0, col);
                                    fReadoutChipInterface->maskChannelsAndSetInjectionSchema(theChip, theSSAChannelGroup, true, true);
                                    fReadoutChipInterface->WriteChipReg(theChip, "InjectedCharge", SSA2::convertMIPtoInjectedCharge(0.5));
                                }
                                else if(theChip->getFrontEndType() == FrontEndType::MPA2)
                                {
                                    std::shared_ptr<ChannelGroup<NMPAROWS, NSSACHANNELS>> theMPAChannelGroup = std::make_shared<ChannelGroup<NMPAROWS, NSSACHANNELS>>();
                                    theMPAChannelGroup->disableAllChannels();
                                    theMPAChannelGroup->enableChannel(10, col);
                                    fReadoutChipInterface->maskChannelsAndSetInjectionSchema(theChip, theMPAChannelGroup, true, true);
                                    fReadoutChipInterface->WriteChipReg(theChip, "InjectedCharge", MPA2::convertMIPtoInjectedCharge(0.5));
                                }
                            }
                        }
                        else if(theChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            if(injectionType == 1)
                            {
                                std::vector<std::pair<uint8_t, uint8_t>> theClusterAddressAndWidthVector{};
                                if(theChip->getId() == 2)
                                {
                                    theClusterAddressAndWidthVector.push_back({10, 1});
                                    theClusterAddressAndWidthVector.push_back({11, 1});
                                }
                                fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
                                static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);
                                static_cast<CbcInterface*>(fReadoutChipInterface)->injectClusters(theChip, theClusterAddressAndWidthVector);
                            }
                            else if(injectionType == 2)
                            {
                                uint16_t bottomChannelIndex = 5;
                                uint16_t bottomChannel      = bottomChannelIndex * 2;
                                ;
                                uint16_t                                    topChannel    = bottomChannel + 1;
                                std::shared_ptr<ChannelGroup<1, NCHANNELS>> theCbcChannel = std::make_shared<ChannelGroup<1, NCHANNELS>>();
                                theCbcChannel->disableAllChannels();
                                theCbcChannel->enableChannel(0, bottomChannel);
                                theCbcChannel->enableChannel(0, topChannel);
                                fReadoutChipInterface->maskChannelGroup(theChip, theCbcChannel);
                                fReadoutChipInterface->WriteChipReg(theChip, "TestPulseGroup", bottomChannelIndex % 8);
                                fReadoutChipInterface->WriteChipReg(theChip, "TestPulsePotNodeSel", Cbc::convertMIPtoInjectedCharge(0.5)); // injected charge
                            }
                        }
                    }
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTPhysics.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTPhysics::Running()
{
    LOG(INFO) << GREEN << "[OTPhysics::Start] Starting" << RESET;

    if(fSaveRawData == true)
    {
        char      runString[7];
        const int theRunNumber = Tool::fRunNumber;
        sprintf(runString, "%06d", theRunNumber);
        this->addFileHandler(fDirectoryName + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    for(auto theBoard: *fDetectorContainer)
    {
        auto theFWInterface      = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getObject((theBoard)->getId())));
        auto theReadoutInterface = theFWInterface->getL1ReadoutInterface();
        theReadoutInterface->ResetReadout();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getId()])->ChipReSync();
    StartInfo theStartInfo;
    theStartInfo.setRunNumber(fRunNumber);

    SystemController::Start(theStartInfo);

    fTotalDataSize = 0;

    auto cTriggerInterface = static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(fDetectorContainer->getFirstObject())->getId()])->getTriggerInterface();
    cTriggerInterface->PrintStatus();

    LOG(INFO) << BOLDYELLOW << "Collecting data using OTPhysics" << RESET;
    LOG(INFO) << BOLDYELLOW << "Press Ctrl + C to stop data taking" << RESET;

    while(fKeepRunning)
    {
        fTotalDataSize += getDataFromBoards();
        if(fTotalDataSize > 1e6) fKeepRunning = false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void OTPhysics::Stop()
{
    LOG(INFO) << GREEN << "[OTPhysics::Stop] Stopping" << RESET;

    Tool::Stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fTotalDataSize += getDataFromBoards();

    auto cTriggerInterface = static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(fDetectorContainer->getFirstObject())->getId()])->getTriggerInterface();
    LOG(INFO) << BOLDBLUE << "Number of collected triggers = " << cTriggerInterface->getNumberOfTriggerCounter() << RESET;
    LOG(INFO) << BOLDBLUE << "Number of collected events   = " << fTotalDataSize << RESET;

    if(fTotalDataSize == 0) LOG(WARNING) << WARNING_FORMAT << "No data collected" << RESET;

    this->closeFileHandler();
    fRegisterHelper->restoreSnapshot();

#ifdef __USE_ROOT__
    if(fSaveRawData) fDQMHistogramOTPhysics.process();
#endif
}

unsigned int OTPhysics::getDataFromBoards()
{
    unsigned int dataSize = 0;
    for(const auto cBoard: *fDetectorContainer) { dataSize += SystemController::ReadData(static_cast<BeBoard*>(cBoard), false); }

    return dataSize;
}