/*!
  \file                  SSAPhysics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "PSPhysics.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/PSSync.h"
#include "../Utils/SSAChannelGroupHandler.h"
#include "BackEndAlignment.h"
#include "CicFEAlignment.h"
#include "PSAlignment.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void PSPhysics::ConfigureCalibration()
{
    PSAlignment cPSAlignment;
    cPSAlignment.Inherit(this);
    cPSAlignment.Initialise();
    // map MPA outputs for PS module
    cPSAlignment.MapMPAOutputs();

    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(this);
    cCicAligner.Start(0);
    cCicAligner.waitForRunToBeCompleted();
    cCicAligner.Reset();
    cCicAligner.dumpConfigFiles();

    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(this);
    cBackEndAligner.Initialise();
    bool cAligned = cBackEndAligner.Align();
    cBackEndAligner.resetPointers();

    // cPSAlignment.Align();
    cPSAlignment.Reset();

    if(!cAligned)
    {
        LOG(ERROR) << BOLDRED << "Failed to align back-end" << RESET;
        exit(1);
    }

    for(auto board: *fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::SSA)
                    {
                        LOG(INFO) << "SSA";
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "ENFLAGS_ALL", 0x1);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "Threshold", 80);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "L1-Latency_LSB", 79);
                    }
                    if(chip->getFrontEndType() == FrontEndType::MPA)
                    {
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "ENFLAGS_ALL", 0xF);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "ModeSel_ALL", 0x0);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "HipCut_ALL", 0x0);
                        // static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "ENFLAGS_ALL", 0x57);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "Threshold", 90);
                        static_cast<PSInterface*>(fReadoutChipInterface)->WriteChipReg(chip, "L1Offset_1_ALL", 79);
                        std::cout << static_cast<PSInterface*>(fReadoutChipInterface)->ReadChipReg(chip, "ReadoutMode") << std::endl;
                    }
                }
            }
        }
    }

    // SystemController::Configure("settings/PS_HalfModule.xml");

    // #######################
    // # Retrieve parameters #
    // #######################
    saveRawData = this->findValueInSettings("SaveRawData");
    doLocal     = false;

    // ###########################################
    // # Initialize directory and data container #
    // ###########################################
    this->CreateResultDirectory(RESULTDIR, false, false);
    // ContainerFactory::copyAndInitStructure<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>(*fDetectorContainer, fPSSyncContainer);

    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fOccupancyContainer);
    ContainerFactory::copyAndInitChannel<float>(*fDetectorContainer, fStubContainer);

    SSAChannelGroupHandler theSSAChannelGroupHandler;
    theSSAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
    setChannelGroupHandler(theSSAChannelGroupHandler, FrontEndType::SSA);
    setChannelGroupHandler(theSSAChannelGroupHandler, FrontEndType::SSA2);

    MPAChannelGroupHandler theMPAChannelGroupHandler;
    theMPAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
    setChannelGroupHandler(theMPAChannelGroupHandler, FrontEndType::MPA);
    setChannelGroupHandler(theMPAChannelGroupHandler, FrontEndType::MPA2);
}

void PSPhysics::Running()
{
    LOG(INFO) << GREEN << "[PSPhysics::Start] Starting" << RESET;

    if(saveRawData == true)
    {
        char runString[7];
        sprintf(runString, "%06d", (fRunNumber));
        this->addFileHandler(std::string(RESULTDIR) + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getId()])->ChipReSync();

    SystemController::Start(fRunNumber);

    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;
    std::cout << "handshake = " << static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;

    PSPhysics::run();
}

// void PSPhysics::sendBoardData(BoardContainer* const& cBoard)
// {
//     auto thePSSyncStream = prepareChipContainerStreamer<EmptyContainer, PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>();

//     if(fDQMStreamerEnabled == true) { thePSSyncStream.streamAndSendBoard(fPSSyncContainer.at(cBoard->getIndex()), fDQMStreamer); }
// }

void PSPhysics::sendBoardData(BoardContainer* const& cBoard)
{
    auto theOccupancyStream = prepareChannelContainerStreamer<float>("Occupancy");
    auto theStubStream      = prepareChannelContainerStreamer<float>("Stub");

    // for(const auto board : fOccupancyContainer)
    // {
    //     for(const auto opticalGroup : *board)
    //     {
    //         for(const auto hybrid : *opticalGroup)
    //         {
    //             for(const auto chip : *hybrid)
    //             {
    //                 for(const auto channel : *chip->getChannelContainer<float>()) std::cout<< channel << " ";
    //                 std::cout<<std::endl;
    //             }
    //         }

    //     }
    // }

    if(fDQMStreamerEnabled == true)
    {
        theOccupancyStream.streamAndSendBoard(fOccupancyContainer.at(cBoard->getIndex()), fDQMStreamer);
        theStubStream.streamAndSendBoard(fStubContainer.at(cBoard->getIndex()), fDQMStreamer);
    }
}

void PSPhysics::Stop()
{
    LOG(INFO) << GREEN << "[PSPhysics::Stop] Stopping" << RESET;

    Tool::Stop();

    fTotalDataSize += getDataFromBoards();

    LOG(WARNING) << BOLDBLUE << "Number of collected events = " << fTotalDataSize << RESET;

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
        // std::cout<<__LINE__<<std::endl;
        dataSize += SystemController::ReadData(static_cast<BeBoard*>(cBoard), false);
        // std::cout<<__LINE__<<std::endl;
        if(dataSize != 0)
        {
            // std::cout<<__LINE__<<std::endl;
            const std::vector<Event*>& events = SystemController::GetEvents();
            // std::cout<<__LINE__<<std::endl;
            PSPhysics::fillDataContainer(cBoard, events);
            // std::cout<<__LINE__<<std::endl;
            PSPhysics::sendBoardData(cBoard);
            // std::cout<<__LINE__<<std::endl;
        }
        // std::cout<<__LINE__<<std::endl;
        // std::cout<<__LINE__<<std::endl;
    }
    std::cout << "Readout " << dataSize << " events" << std::endl;
    return dataSize;
}

void PSPhysics::run()
{
    fTotalDataSize = 0;

    while(fKeepRunning)
    {
        // std::cout<<"Trigger status = "<< static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_stat.fast_command_block.general.source") << std::endl;
        // std::cout<<"handshake = "<< static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable") << std::endl;

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
        for(const auto cOpticalGroup: *fOccupancyContainer.at(cBoard->getIndex()))
        {
            for(const auto cHybrid: *cOpticalGroup)
            {
                // uint16_t L1Status = static_cast<D19cCic2Event*>(event)->L1Status(cHybrid->getId());
                // if( (L1Status & 0x1) == 1 && (L1Status & 0x1FE) != 0 ) LOG(WARNING) << BOLDRED << "No packet from MPA to CIC" << RESET;
                // std::cout<<"L1 id = " << std::dec<<static_cast<D19cCic2Event*>(event)->L1Id(cHybrid->getId(),0)<<std::endl;
                for(const auto cChip: *cHybrid)
                {
                    // std::cout<<__LINE__<<std::endl;
                    auto currentChip = fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex());
                    if(currentChip->getFrontEndType() != FrontEndType::MPA) continue;

                    // std::cout<<__LINE__<<std::endl;
                    std::vector<PCluster> pixelClusterList = static_cast<D19cCic2Event*>(event)->GetPixelClusters(cHybrid->getId(), cChip->getId());
                    // std::cout<<"Numer of pixel clusters = "<<pixelClusterList.size() << " - ";
                    std::vector<SCluster> stripClusterList = static_cast<D19cCic2Event*>(event)->GetStripClusters(cHybrid->getId(), cChip->getId());
                    std::vector<Stub>     stubList         = static_cast<D19cCic2Event*>(event)->StubVector(cHybrid->getId(), cChip->getId());

                    // std::cout<<__LINE__<<std::endl;
                    for(auto& pixelCluster: pixelClusterList)
                    {
                        for(uint8_t subPixel = 0; subPixel <= (pixelCluster.fWidth); ++subPixel)
                        {
                            if(pixelCluster.fAddress + subPixel < 120u) ++cChip->getChannel<float>(pixelCluster.fZpos, pixelCluster.fAddress + subPixel);
                        }
                    }

                    // std::cout<<__LINE__<<std::endl;
                    ChipDataContainer* theStubChipContainer = fStubContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex());
                    // std::cout<<__LINE__<<std::endl;
                    for(auto& stub: stubList)
                    {
                        // std::cout<<"stub.getRow()            = "<<+stub.getRow()           <<std::endl;
                        // std::cout<<"stub.getCenter()         = "<<stub.getCenter()         <<std::endl;
                        // std::cout<<"size_t(stub.getCenter()) = "<<size_t(stub.getCenter())<<std::endl;
                        // std::cout<<"stub.getPosition()       = "<<+stub.getPosition()     <<std::endl;
                        // std::cout<<__LINE__<<std::endl;
                        if(ceil(stub.getCenter()) != stub.getCenter())
                        {
                            // std::cout<<__LINE__<<std::endl;
                            if(size_t(ceil(stub.getCenter())) < 120u) theStubChipContainer->getChannel<float>(stub.getRow(), size_t(ceil(stub.getCenter()))) += 0.5;
                            // std::cout<<__LINE__<<std::endl;
                            if(size_t(floor(stub.getCenter())) < 120u) theStubChipContainer->getChannel<float>(stub.getRow(), size_t(floor(stub.getCenter()))) += 0.5;
                            // std::cout<<__LINE__<<std::endl;
                        }
                        else
                        {
                            // std::cout<<__LINE__<<std::endl;

                            if(size_t(stub.getCenter()) < 120u) ++theStubChipContainer->getChannel<float>(stub.getRow(), size_t(stub.getCenter()));
                            // std::cout<<__LINE__<<std::endl;
                        }
                        // std::cout<<__LINE__<<std::endl;
                    }

                    // std::cout<<__LINE__<<std::endl;

                    if(currentChip->getId() == 3) continue; // patch for bug in the FEH I2C address

                    // std::cout<<__LINE__<<std::endl;
                    uint16_t theCorrespondingSSAIndex = 9999;
                    for(auto theCorrespondingSSA: *fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex()))
                    {
                        if(theCorrespondingSSA->getFrontEndType() != FrontEndType::SSA) continue;
                        if(theCorrespondingSSA->getId() == currentChip->getId())
                        {
                            theCorrespondingSSAIndex = theCorrespondingSSA->getIndex();
                            break;
                        }
                    }

                    // std::cout<<__LINE__<<std::endl;

                    ChipDataContainer* theSSAContainer = cHybrid->at(theCorrespondingSSAIndex);

                    for(auto& stripCluster: stripClusterList)
                    {
                        for(uint8_t subStrip = 0; subStrip <= (stripCluster.fWidth); ++subStrip)
                        {
                            if(stripCluster.fAddress + subStrip < 120u) ++theSSAContainer->getChannel<float>(stripCluster.fAddress + subStrip);
                        }
                    }
                    // std::cout<<__LINE__<<std::endl;
                }
                // std::cout<<std::endl;
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
    for(const auto cOpticalGroup: *fOccupancyContainer.at(theBoard->getIndex()))
    {
        for(const auto cHybrid: *cOpticalGroup)
        {
            for(const auto cChip: *cHybrid)
            {
                for(auto& cChannel: *cChip->getChannelContainer<float>()) cChannel = 0.;
            }
        }
    }

    for(const auto cOpticalGroup: *fStubContainer.at(theBoard->getIndex()))
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
