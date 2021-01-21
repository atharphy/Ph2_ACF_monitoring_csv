/*!
  \file                  SSAPhysics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "SSAPhysics.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void SSAPhysics::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    saveRawData = this->findValueInSettings("SaveRawData");
    doLocal     = false;
    keepRunning = true;

    // ###########################################
    // # Initialize directory and data container #
    // ###########################################
    this->CreateResultDirectory(RESULTDIR, false, false);
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, fOccContainer);

    fChannelGroupHandler = new SSAChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
}

void SSAPhysics::Start(int currentRun)
{
    LOG(INFO) << GREEN << "[SSAPhysics::Start] Starting" << RESET;

    if(saveRawData == true)
    {
        char runString[6];
        sprintf(runString, "%06d", currentRun);
        this->addFileHandler(std::string(RESULTDIR) + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getBeBoardId()])->ChipReSync();
    SystemController::Start(currentRun);

    keepRunning = true;
    thrRun      = std::thread(&SSAPhysics::run, this);
}

void SSAPhysics::sendData(BoardContainer* const& cBoard)
{
    auto theOccStream = prepareChannelContainerStreamer<Occupancy>("Occ");

    if(fStreamerEnabled == true) { theOccStream.streamAndSendBoard(fOccContainer.at(cBoard->getIndex()), fNetworkStreamer); }
}

void SSAPhysics::Stop()
{
    LOG(INFO) << GREEN << "[SSAPhysics::Stop] Stopping" << RESET;

    SystemController::Stop();
    keepRunning = false;
    if(thrRun.joinable() == true) thrRun.join();

    // ################
    // # Error report #
    // ################
    SSAPhysics::chipErrorReport();

    this->closeFileHandler();
}

void SSAPhysics::initialize(const std::string fileRes_, const std::string fileReg_)
{
    fileRes = fileRes_;
    fileReg = fileReg_;

    SSAPhysics::ConfigureCalibration();

#ifdef __USE_ROOT__
    myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);

    SSAPhysics::initHisto();
#endif

    doLocal = true;
}

void SSAPhysics::run()
{
    unsigned int totalDataSize = 0;

    while(keepRunning == true)
    {
        for(const auto cBoard: *fDetectorContainer)
        {
            unsigned int dataSize = SystemController::ReadData(static_cast<BeBoard*>(cBoard), false);
            if(dataSize != 0)
            {
                SSAPhysics::fillDataContainer(cBoard);
                SSAPhysics::sendData(cBoard);
            }
            totalDataSize += dataSize;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(READOUTSLEEP));
    }

    LOG(WARNING) << BOLDBLUE << "Number of collected events = " << totalDataSize << RESET;

    if(totalDataSize == 0) LOG(WARNING) << BOLDBLUE << "No data collected" << RESET;
}

void SSAPhysics::draw()
{
#ifdef __USE_ROOT__
    SSAPhysics::fillHisto();
    SSAPhysics::display();

    if(doDisplay == true) myApp->Run(true);
    this->WriteRootFile();
    this->CloseResultFile();
#endif
}

void SSAPhysics::initHisto()
{
#ifdef __USE_ROOT__
    histos.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void SSAPhysics::fillHisto()
{
#ifdef __USE_ROOT__
    histos.fillOccupancy(fOccContainer);
#endif
}

void SSAPhysics::display()
{
#ifdef __USE_ROOT__
    histos.process();
#endif
}

void SSAPhysics::fillDataContainer(BoardContainer* const& cBoard)
{
    // ###################
    // # Clear container #
    // ###################
    for(const auto cOpticalGroup: *fOccContainer.at(cBoard->getIndex()))
        for(const auto cModule: *cOpticalGroup)
            for(const auto cChip: *cModule)
                for(auto& channel: *cChip->getChannelContainer<Occupancy>())
                {
                    channel.fOccupancy      = 0;
                    channel.fOccupancyError = 0;
                }

    // ###################
    // # Fill containers #
    // ###################
    const std::vector<Event*>& events = SystemController::GetEvents(static_cast<BeBoard*>(cBoard));
    for(const auto& event: events) { event->fillDataContainer(fOccContainer.at(cBoard->getIndex()), fChannelGroupHandler->allChannelGroup()); }
}

void SSAPhysics::chipErrorReport() {}
