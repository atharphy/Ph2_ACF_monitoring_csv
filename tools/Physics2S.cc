/*!
  \file                  SSAPhysics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "Physics2S.h"
#include "../Utils/Occupancy.h"
#include "../Utils/Data2S.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/CBCChannelGroupHandler.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void Physics2S::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    saveRawData = this->findValueInSettings("SaveRawData");
    doLocal     = false;

    // ###########################################
    // # Initialize directory and data container #
    // ###########################################
    this->CreateResultDirectory(RESULTDIR, false, false);
    ContainerFactory::copyAndInitStructure<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>>(*fDetectorContainer, f2SDataContainer);

    fChannelGroupHandler = new CBCChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(120, 16);
}

void Physics2S::Running()
{
    LOG(INFO) << GREEN << "[Physics2S::Start] Starting" << RESET;

    if(saveRawData == true)
    {
        char runString[7];
        sprintf(runString, "%06d", (fRunNumber & 0xF423F)); // max value can be 999999, to avoid GCC 8 warning
        this->addFileHandler(std::string(RESULTDIR) + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getId()])->ChipReSync();
    SystemController::Start(fRunNumber);

    Physics2S::run();
}

void Physics2S::sendBoardData(BoardContainer* const& cBoard)
{
    auto thePSSyncStream = prepareChipContainerStreamer<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>,EmptyContainer>();

    if(fStreamerEnabled == true) { thePSSyncStream.streamAndSendBoard(f2SDataContainer.at(cBoard->getIndex()), fNetworkStreamer); }
}

void Physics2S::Stop()
{
    LOG(INFO) << GREEN << "[Physics2S::Stop] Stopping" << RESET;

    Tool::Stop();

    // ################
    // # Error report #
    // ################
    Physics2S::chipErrorReport();

    this->closeFileHandler();
}

void Physics2S::initialize(const std::string fileRes_, const std::string fileReg_)
{
    fileRes = fileRes_;
    fileReg = fileReg_;

    Physics2S::ConfigureCalibration();

#ifdef __USE_ROOT__
    myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);

    Physics2S::initHisto();
#endif

    doLocal = true;
}

void Physics2S::run()
{
    unsigned int totalDataSize = 0;

    while(fKeepRunning)
    {
        for(const auto cBoard: *fDetectorContainer)
        {
            unsigned int dataSize = SystemController::ReadData(static_cast<BeBoard*>(cBoard), false);
            if(dataSize != 0)
            {
                Physics2S::fillDataContainer(cBoard);
                Physics2S::sendBoardData(cBoard);
            }
            totalDataSize += dataSize;
        }

        std::this_thread::sleep_for(std::chrono::microseconds(RD53FWconstants::READOUTSLEEP));
    }

    LOG(WARNING) << BOLDBLUE << "Number of collected events = " << totalDataSize << RESET;

    if(totalDataSize == 0) LOG(WARNING) << BOLDBLUE << "No data collected" << RESET;
}

void Physics2S::draw()
{
#ifdef __USE_ROOT__
    Physics2S::fillHisto();
    Physics2S::display();

    if(doDisplay == true) myApp->Run(true);
    this->WriteRootFile();
    this->CloseResultFile();
#endif
}

void Physics2S::initHisto()
{
#ifdef __USE_ROOT__
    histos.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void Physics2S::fillHisto()
{
#ifdef __USE_ROOT__
    histos.fillData(f2SDataContainer);
#endif
}

void Physics2S::display()
{
#ifdef __USE_ROOT__
    histos.process();
#endif
}

void Physics2S::fillDataContainer(BoardContainer* const& cBoard)
{


    // ###################
    // # Fill containers #
    // ###################
    const std::vector<Event*>& events = SystemController::GetEvents();
    for(const auto& event: events) 
	{ 
		for(const auto cOpticalGroup: *f2SDataContainer.at(cBoard->getIndex()))
		{
		    for(const auto cHybrid: *cOpticalGroup)
			{
		        for(const auto cChip: *cHybrid)
				{

    				auto curchip = cBoard->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex());;
                    if(curchip->getFrontEndType() != FrontEndType::MPA) continue;

					auto data2S = cChip->getSummary<Data2S<NCHANNELS, MAX_NUMBER_OF_STUB_CLUSTERS_2S>>();
                    data2S.fClusters    = fromVectorToGenericDataArray<NCHANNELS, Cluster>(static_cast<D19cCic2Event*>(event)->getClusters(cHybrid->getId(), cChip->getId()));
                    data2S.fStubs       = fromVectorToGenericDataArray<MAX_NUMBER_OF_STUB_CLUSTERS_2S, Stub>(static_cast<D19cCic2Event*>(event)->StubVector(cHybrid->getId(), cChip->getId()));
				}
			}
		}
	}
}

void Physics2S::chipErrorReport() {}



