/*!
  \file                  SSAPhysics.cc
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "PSPhysics.h"
#include "BackEndAlignment.h"
#include "CicFEAlignment.h"
#include "../Utils/Occupancy.h"
#include "../Utils/PSSync.h"
#include "../Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void PSPhysics::ConfigureCalibration()
{
    CicFEAlignment cCicAligner;
    cCicAligner.Inherit(this);
    cCicAligner.Start(0);
    cCicAligner.waitForRunToBeCompleted();
    // reset all chip and board registers
    // to what they were before this tool was called
    cCicAligner.Reset();
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(this);
    cBackEndAligner.Start(0);
    cBackEndAligner.waitForRunToBeCompleted();
    // reset all chip and board registers
    // to what they were before this tool was called
    cBackEndAligner.Reset();

    // #######################
    // # Retrieve parameters #
    // #######################
    saveRawData = this->findValueInSettings("SaveRawData");
    doLocal     = false;

    // ###########################################
    // # Initialize directory and data container #
    // ###########################################
    this->CreateResultDirectory(RESULTDIR, false, false);
    ContainerFactory::copyAndInitStructure<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>(*fDetectorContainer, fPSSyncContainer);

    fChannelGroupHandler = new MPAChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(120, 16);
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

    PSPhysics::run();
}

void PSPhysics::sendBoardData(BoardContainer* const& cBoard)
{
    auto thePSSyncStream = prepareChipContainerStreamer<EmptyContainer, PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>();

    if(fStreamerEnabled == true) { thePSSyncStream.streamAndSendBoard(fPSSyncContainer.at(cBoard->getIndex()), fNetworkStreamer); }
}

void PSPhysics::Stop()
{
    LOG(INFO) << GREEN << "[PSPhysics::Stop] Stopping" << RESET;

    Tool::Stop();

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

void PSPhysics::run()
{
    unsigned int totalDataSize = 0;

    while(fKeepRunning)
    {
        for(const auto cBoard: *fDetectorContainer)
        {
            // unsigned int dataSize = 10;
            // SystemController::ReadNEvents(static_cast<BeBoard*>(cBoard), dataSize);
            unsigned int dataSize = SystemController::ReadData(static_cast<BeBoard*>(cBoard), false);
            if(dataSize != 0)
            {
                PSPhysics::fillDataContainer(cBoard);
                PSPhysics::sendBoardData(cBoard);
            }
            totalDataSize += dataSize;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    LOG(WARNING) << BOLDBLUE << "Number of collected events = " << totalDataSize << RESET;

    if(totalDataSize == 0) LOG(WARNING) << BOLDBLUE << "No data collected" << RESET;
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
    histos.fillSync(fPSSyncContainer);
#endif
}

void PSPhysics::display()
{
#ifdef __USE_ROOT__
    histos.process();
#endif
}

void PSPhysics::fillDataContainer(BoardContainer* const& cBoard)
{


    // ###################
    // # Fill containers #
    // ###################
    const std::vector<Event*>& events = SystemController::GetEvents();
    for(const auto& event: events) 
	{ 
		for(const auto cOpticalGroup: *fPSSyncContainer.at(cBoard->getIndex()))
		{
		    for(const auto cHybrid: *cOpticalGroup)
			{
		        for(const auto cChip: *cHybrid)
				{

    				auto curchip = cBoard->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex());;
                    if(curchip->getFrontEndType() != FrontEndType::MPA) continue;

					auto curPSSync = cChip->getSummary<PSSync<MAX_NUMBER_OF_STRIP_CLUSTERS, MAX_NUMBER_OF_PIXEL_CLUSTERS,MAX_NUMBER_OF_STUB_CLUSTERS_PS>>();
                    curPSSync.fPClusters = fromVectorToGenericDataArray<MAX_NUMBER_OF_PIXEL_CLUSTERS, PCluster>(static_cast<D19cCic2Event*>(event)->GetPixelClusters(cHybrid->getId(), cChip->getId()));
                    // std::cout<<"Pixel cluster centers = ";
                    // for(size_t pos = 0; pos<MAX_NUMBER_OF_PIXEL_CLUSTERS; ++pos) std::cout << +curPSSync.fPClusters[pos].fAddress << " ";
                    // std::cout<<std::endl;
                    curPSSync.fSClusters = fromVectorToGenericDataArray<MAX_NUMBER_OF_STRIP_CLUSTERS, SCluster>(static_cast<D19cCic2Event*>(event)->GetStripClusters(cHybrid->getId(), cChip->getId()));
                    // std::cout<<"Strip cluster centers = ";
                    // for(size_t pos = 0; pos<MAX_NUMBER_OF_STRIP_CLUSTERS; ++pos) std::cout << +curPSSync.fSClusters[pos].fAddress << " ";
                    // std::cout<<std::endl;
                    curPSSync.fStubs     = fromVectorToGenericDataArray<MAX_NUMBER_OF_STUB_CLUSTERS_PS , Stub    >(static_cast<D19cCic2Event*>(event)->StubVector      (cHybrid->getId(), cChip->getId()));
                    // std::cout<<"Stub cluster centers = ";
                    // for(size_t pos = 0; pos<MAX_NUMBER_OF_STUB_CLUSTERS_PS; ++pos) std::cout << +curPSSync.fStubs[pos].getPosition() << " ";
                    // std::cout<<std::endl;
				}
			}
		}
	}
}

void PSPhysics::chipErrorReport() {}



