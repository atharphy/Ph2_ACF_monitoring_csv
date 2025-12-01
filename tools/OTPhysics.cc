#include "tools/OTPhysics.h"
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
    fSaveRawData               = this->findValueInSettings<double>("OTPhysics_SaveRawData", 1);
    uint8_t theTriggerSource   = this->findValueInSettings<double>("OTPhysics_TriggerSource", 3);
    uint8_t theUserTriggerRate = this->findValueInSettings<double>("OTPhysics_UserTriggerRate", 10);
    bool injectNoise           = this->findValueInSettings<double>("OTPhysics_InjectNoise", 1);

    std::vector<std::pair<std::string, uint32_t>> boardRegisterVector;
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", theTriggerSource});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", theUserTriggerRate});
    boardRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});
    boardRegisterVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});

    for(auto theBoard: *fDetectorContainer)
    { 
        fBeBoardInterface->WriteBoardMultReg(theBoard, boardRegisterVector); 
        setSparsification(theBoard, true);
        if(injectNoise)
        {
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    for(auto theChip: *theHybrid)
                    {
                        if(theChip->getFrontEndType() == FrontEndType::MPA2 || theChip->getFrontEndType() == FrontEndType::SSA2)
                        {
                            std::vector<Cluster> theClusterList {};
                            if(theChip->getId() % 8 == 2)
                            {
                                theClusterList.push_back(Cluster(10, 10, 1));
                            }
                            static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, theClusterList);
                        }
                        else if(theChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            std::vector<std::pair<uint8_t, uint8_t>> theClusterAddressAndWidthVector {};
                            if(theChip->getId() == 2)
                            {
                                theClusterAddressAndWidthVector.push_back({10, 1});
                                theClusterAddressAndWidthVector.push_back({11, 1});
                            }
                            fReadoutChipInterface->WriteChipReg(theChip, "HitOr", 1);
                            static_cast<CbcInterface*>(fReadoutChipInterface)->selectLogicMode(theChip, "Sampled", true, true);
                            static_cast<CbcInterface*>(fReadoutChipInterface)->injectClusters(theChip, theClusterAddressAndWidthVector);
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
    fDQMHistogramOTPhysics.process();
#endif
}

unsigned int OTPhysics::getDataFromBoards()
{
    unsigned int dataSize = 0;
    for(const auto cBoard: *fDetectorContainer) { dataSize += SystemController::ReadData(static_cast<BeBoard*>(cBoard), false); }

    return dataSize;
}