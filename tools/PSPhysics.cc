/*!
  \file                  SSAPhysics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "tools/PSPhysics.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/PSSync.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/StartInfo.h"
#include "tools/CicFEAlignment.h"
#include "tools/PSAlignment.h"
#include "HWInterface/D19cTriggerInterface.h"
#include "HWInterface/D19cL1ReadoutInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

std::string PSPhysics::fCalibrationDescription = "Take data";

void PSPhysics::ConfigureCalibration()
{
    // SystemController::Configure("settings/PS_HalfModule.xml");

    // #######################
    // # Retrieve parameters #
    // #######################
    saveRawData = this->findValueInSettings<double>("SaveRawData");
    doLocal     = false;

    std::vector<std::pair<std::string, uint32_t>> boardRegisterVector;
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 1});
    boardRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});
    boardRegisterVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});

    for(auto theBoard: *fDetectorContainer)
    {
        fBeBoardInterface->WriteBoardMultReg(theBoard, boardRegisterVector);
    }

    // ###########################################
    // # Initialize directory and data container #
    // ###########################################
    // ContainerFactory::copyAndInitStructure<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>(*fDetectorContainer, fPSSyncContainer);

    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fOccupancyContainer);
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fStubContainer);

    SSAChannelGroupHandler theSSAChannelGroupHandler;
    theSSAChannelGroupHandler.setChannelGroupParameters(1, 1, NSSACHANNELS); // 16*2*8
    setChannelGroupHandler(theSSAChannelGroupHandler, FrontEndType::SSA2);

    MPAChannelGroupHandler theMPAChannelGroupHandler;
    theMPAChannelGroupHandler.setChannelGroupParameters(1, NMPAROWS, NSSACHANNELS); // 16*2*8
    setChannelGroupHandler(theMPAChannelGroupHandler, FrontEndType::MPA2);

    // std::vector<uint8_t> listOfInjectedChipId = {};
    // std::vector<Cluster> theClusterList;
    // theClusterList.push_back(Cluster(10, 100, 1));

    // for(auto theBoard: *fDetectorContainer)
    // {
    //     for(auto theOpticalGroup: *theBoard)
    //     {
    //         for(auto theHybrid: *theOpticalGroup)
    //         {
    //             for(auto theChip: *theHybrid)
    //             {
    //                 if(std::find(listOfInjectedChipId.begin(), listOfInjectedChipId.end(), theChip->getId()) != listOfInjectedChipId.end())
    //                 {
    //                     static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, theClusterList);
    //                 }
    //                 else { static_cast<PSInterface*>(fReadoutChipInterface)->injectNoiseClusters(theChip, {}); }
    //             }
    //         }
    //     }
    // }
}

void PSPhysics::Running()
{
    LOG(INFO) << GREEN << "[PSPhysics::Start] Starting" << RESET;

    if(saveRawData == true)
    {
        char      runString[7];
        const int theRunNumber = Tool::fRunNumber;
        sprintf(runString, "%06d", theRunNumber);
        this->addFileHandler(fDirectoryName + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getId()])->ChipReSync();
    StartInfo theStartInfo;
    theStartInfo.setRunNumber(fRunNumber);

    for(auto theBoard: *fDetectorContainer)
    {
        auto theFWInterface        = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getObject((theBoard)->getId())));
        auto theReadoutInterface = theFWInterface->getL1ReadoutInterface();
        theReadoutInterface->ResetReadout();
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    SystemController::Start(theStartInfo);

    PSPhysics::run();
}

void PSPhysics::Stop()
{
    LOG(INFO) << GREEN << "[PSPhysics::Stop] Stopping" << RESET;

    Tool::Stop();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fTotalDataSize += getDataFromBoards();

    auto cTriggerInterface = static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(fDetectorContainer->getFirstObject())->getId()])->getTriggerInterface();
    LOG(INFO) << BOLDBLUE << "Number of collected triggers = " << cTriggerInterface->getNumberOfTriggerCounter() << std::endl;
    LOG(INFO) << BOLDBLUE << "Number of collected events   = " << fTotalDataSize << RESET;

    if(fTotalDataSize == 0) LOG(WARNING) << BOLDBLUE << "No data collected" << RESET;

    // ################
    // # Error report #
    // ################
    PSPhysics::chipErrorReport();

    this->closeFileHandler();
}

void PSPhysics::initialize(const std::string fileRes_, const std::string fileReg_)
{
    fileRes = fileRes_;
    fileReg = fileReg_;

    PSPhysics::ConfigureCalibration();

#ifdef __USE_ROOT__
    myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);

    PSPhysics::initHisto();
#endif

    doLocal = true;
}

unsigned int PSPhysics::getDataFromBoards()
{
    unsigned int dataSize = 0;
    for(const auto cBoard: *fDetectorContainer)
    {
        dataSize += SystemController::ReadData(static_cast<BeBoard*>(cBoard), false);

        if(dataSize != 0)
        {
            const std::vector<Event*>& events = SystemController::GetEvents();
            PSPhysics::fillDataContainer(cBoard, events);
        }
    }
    
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theOccupancySerialization("PSPhysicsOccupancy");
        theOccupancySerialization.streamByHybridContainer(fDQMStreamer, fOccupancyContainer);

        ContainerSerialization theStubSerialization("PSPhysicsStub");
        theStubSerialization.streamByHybridContainer(fDQMStreamer, fStubContainer);
    }

    return dataSize;
}

void PSPhysics::run()
{
    fTotalDataSize = 0;

    while(fKeepRunning)
    {
        fTotalDataSize += getDataFromBoards();

        std::cout << "Readout in total = " << fTotalDataSize << " events" << std::endl;

        // std::this_thread::sleep_for(std::chrono::seconds(60));
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}

void PSPhysics::draw()
{
#ifdef __USE_ROOT__
    PSPhysics::fillHisto();
    PSPhysics::display();

    if(doDisplay == true) myApp->Run(true);
    this->WriteRootFile();
    this->CloseResultFile();
#endif
}

void PSPhysics::initHisto()
{
#ifdef __USE_ROOT__
    histos.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PSPhysics::fillHisto()
{
#ifdef __USE_ROOT__
    // histos.fillSync(fPSSyncContainer);
    histos.fillOccupancy(fOccupancyContainer);
    histos.fillStub(fStubContainer);
#endif
}

void PSPhysics::display()
{
#ifdef __USE_ROOT__
    histos.process();
#endif
}

void PSPhysics::fillDataContainer(BoardContainer* const& cBoard, const std::vector<Event*> eventList)
{
    // std::cout<<__LINE__<<std::endl;
    clearContainers(cBoard);
    // std::cout<<__LINE__<<std::endl;

    for(auto event: eventList)
    {
        // ###################
        // # Fill containers #
        // ###################
        for(const auto cOpticalGroup: *fOccupancyContainer.getObject(cBoard->getId()))
        {
            for(const auto cHybrid: *cOpticalGroup)
            {
                // uint16_t L1Status = static_cast<D19cCic2Event*>(event)->L1Status(cHybrid->getId());
                // if( (L1Status & 0x1) == 1 && (L1Status & 0x1FE) != 0 ) LOG(WARNING) << BOLDRED << "No packet from MPA to CIC" << RESET;
                // std::cout<<"L1 id = " << std::dec<<static_cast<D19cCic2Event*>(event)->L1Id(cHybrid->getId(),0)<<std::endl;
                for(const auto cChip: *cHybrid)
                {
                    // std::cout<<__LINE__<<std::endl;
                    auto currentChip = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                    if(currentChip->getFrontEndType() != FrontEndType::MPA2) continue;

                    // std::cout<<__LINE__<<std::endl;
                    if(currentChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        auto pixelClusterList = static_cast<D19cCic2Event*>(event)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                        for(auto& pixelCluster: pixelClusterList)
                        {
                            for(uint8_t subPixel = 0; subPixel < (pixelCluster.fWidth); ++subPixel)
                            {
                                if(pixelCluster.fAddress + subPixel < 120u) ++cChip->getChannel<float>(pixelCluster.fZpos, pixelCluster.fAddress + subPixel);
                            }
                        }
                        
                        auto stubList         = static_cast<D19cCic2Event*>(event)->StubVector(cHybrid->getId(), cChip->getId());
                        ChipDataContainer* theStubChipContainer = fStubContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
                        for(auto& stub: stubList)
                        {
                            if(ceil(stub.getCenter()) != stub.getCenter())
                            {
                                if(size_t(ceil(stub.getCenter())) < 120u) theStubChipContainer->getChannel<float>(stub.getRow(), size_t(ceil(stub.getCenter()))) += 0.5;
                                if(size_t(floor(stub.getCenter())) < 120u) theStubChipContainer->getChannel<float>(stub.getRow(), size_t(floor(stub.getCenter()))) += 0.5;
                            }
                            else
                            {
                                if(size_t(stub.getCenter()) < 120u) ++theStubChipContainer->getChannel<float>(stub.getRow(), size_t(stub.getCenter()));
                            }
                        }
                    }
                    else
                    {
                        ChipDataContainer* theSSAContainer = cHybrid->getObject(cChip->getId());
                        
                        auto stripClusterList = static_cast<D19cCic2Event*>(event)->GetStripClusters(cHybrid->getId(), cChip->getId());
                        for(auto& stripCluster: stripClusterList)
                        {
                            for(uint8_t subStrip = 0; subStrip < (stripCluster.fWidth); ++subStrip)
                            {
                                if(stripCluster.fAddress + subStrip < 120u) ++theSSAContainer->getChannel<float>(0, stripCluster.fAddress + subStrip);
                            }
                        }
                    }
                }
            }
        }
    }
}

void PSPhysics::chipErrorReport() {}

void PSPhysics::clearContainers(BoardContainer* theBoard)
{
    // ####################
    // # Clear containers #
    // ####################
    for(const auto cOpticalGroup: *fOccupancyContainer.getObject(theBoard->getId()))
    {
        for(const auto cHybrid: *cOpticalGroup)
        {
            for(const auto cChip: *cHybrid)
            {
                for(auto& cChannel: *cChip->getChannelContainer<float>()) cChannel = 0.;
            }
        }
    }

    for(const auto cOpticalGroup: *fStubContainer.getObject(theBoard->getId()))
    {
        for(const auto cHybrid: *cOpticalGroup)
        {
            for(const auto cChip: *cHybrid)
            {
                for(auto& cChannel: *cChip->getChannelContainer<float>()) cChannel = 0.;
            }
        }
    }
}
