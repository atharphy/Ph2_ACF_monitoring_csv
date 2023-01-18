#include "DQMUtils/DQMInterface.h"
#include "DQMUtils/DQMCalibrationFactory.h"
#include "NetworkUtils/TCPSubscribeClient.h"
#include "Parser/FileParser.h"
#include "Utils/ObjectStream.h"

#include "TFile.h"

#include <iostream>
#include <string>

//========================================================================================================================
DQMInterface::DQMInterface() : fListener(nullptr), fRunning(false), fOutputFile(nullptr) {}

//========================================================================================================================
DQMInterface::~DQMInterface(void)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    destroy();
}

//========================================================================================================================
void DQMInterface::destroy(void)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    if(fListener != nullptr) delete fListener;
    destroyHistogram();
    fListener = nullptr;
    for(auto dqmHistogrammer: fDQMHistogrammerVector) delete dqmHistogrammer;
    fDQMHistogrammerVector.clear();
    delete fOutputFile;
    fOutputFile = nullptr;

    LOG(INFO) << __PRETTY_FUNCTION__ << " DONE" << RESET;
}

//========================================================================================================================
void DQMInterface::destroyHistogram(void)
{
    for(auto dqmHistogrammer: fDQMHistogrammerVector) delete dqmHistogrammer;
    fDQMHistogrammerVector.clear();

    LOG(INFO) << __PRETTY_FUNCTION__ << " DONE" << RESET;
}

//========================================================================================================================
void DQMInterface::configure(std::string const& calibrationName, std::string const& configurationFilePath)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    std::string serverIP   = "127.0.0.1";
    int         serverPort = 6000;
    fListener              = new TCPSubscribeClient(serverIP, serverPort);

    if(!fListener->connect())
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " CAN'T CONNECT TO SERVER!" << RESET;
        abort();
    }
    LOG(INFO) << __PRETTY_FUNCTION__ << " DQM connected" << RESET;

    Ph2_Parser::FileParser  fParser;
    std::stringstream       out;
    Ph2_Parser::SettingsMap pSettingsMap;

    fParser.parseHW(configurationFilePath, &fDetectorStructure, out);
    fParser.parseSettings(configurationFilePath, pSettingsMap, out);

    DQMCalibrationFactory theDQMCalibrationFactory;
    fDQMHistogrammerVector = theDQMCalibrationFactory.createDQMHistogrammerVector(calibrationName);

    fOutputFile = new TFile("tmp.root", "RECREATE");
    for(auto dqmHistogrammer: fDQMHistogrammerVector) dqmHistogrammer->book(fOutputFile, fDetectorStructure, pSettingsMap);
}

//========================================================================================================================
void DQMInterface::startProcessingData(int runNumber)
{
    fRunning       = true;
    fRunningFuture = std::async(std::launch::async, &DQMInterface::running, this);
}

//========================================================================================================================
void DQMInterface::stopProcessingData(void)
{
    fRunning = false;
    std::chrono::milliseconds span(1000);
    int                       timeout = 10; // in seconds

    fListener->close();
    while(fRunningFuture.wait_for(span) == std::future_status::timeout && timeout >= 0)
    { LOG(INFO) << __PRETTY_FUNCTION__ << " Process still running! Waiting " << timeout-- << " more seconds!" << RESET; }

    LOG(INFO) << __PRETTY_FUNCTION__ << " Thread done running" << RESET;

    if(fDataBuffer.size() > 0)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Buffer should be empty, some data were not read, Aborting" << RESET;
        abort();
    }

    for(auto dqmHistogrammer: fDQMHistogrammerVector) dqmHistogrammer->process();
    fOutputFile->Write();
}

//========================================================================================================================
bool DQMInterface::running()
{
    CheckStream* theCurrentStream;
    // int               packetNumber = -1;
    std::vector<char> tmpDataBuffer;

    while(fRunning)
    {
        // LOG(INFO) << __PRETTY_FUNCTION__ << " Running = " << fRunning << RESET;
        // if(receive(configBuffer, 1) != -1)
        // if(receive(*reinterpret_cast<std::vector<char>*>(*configBuffer.end()), 1) != -1)
        // TODO We need to optimize the data readout so we don't do multiple copies
        // TODO We need to optimize the data readout so we don't do multiple copies
        // TODO We need to optimize the data readout so we don't do multiple copies
        // if(fListener->receive(tmpDataBuffer, 0, 100000) > 0)
        try
        {
            tmpDataBuffer = fListener->receive<std::vector<char>>();
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << "Error: " << e.what() << RESET;
            fRunning = false;
            break;
        }
        LOG(DEBUG) << "Got something" << RESET;
        fDataBuffer.insert(fDataBuffer.end(), tmpDataBuffer.begin(), tmpDataBuffer.end());
        LOG(DEBUG) << "Data buffer size: " << fDataBuffer.size() << RESET;
        while(fDataBuffer.size() > 0)
        {
            if(fDataBuffer.size() < sizeof(CheckStream))
            {
                LOG(WARNING) << BOLDBLUE << "Not enough bytes to retrieve data stream" << RESET;
                break; // Not enough bytes to retreive the packet size
            }
            theCurrentStream = reinterpret_cast<CheckStream*>(&fDataBuffer.at(0));
            LOG(DEBUG) << "Packet number received = " << int(theCurrentStream->getPacketNumber()) << RESET;

            LOG(DEBUG) << "Vector size  = " << fDataBuffer.size() << "; expected = " << theCurrentStream->getPacketSize() << RESET;

            if(fDataBuffer.size() < theCurrentStream->getPacketSize())
            {
                LOG(DEBUG) << "Packet not completed, waiting" << RESET;
                break;
            }

            std::vector<char> streamDataBuffer(fDataBuffer.begin(), fDataBuffer.begin() + theCurrentStream->getPacketSize());
            fDataBuffer.erase(fDataBuffer.begin(), fDataBuffer.begin() + theCurrentStream->getPacketSize());

            for(auto dqmHistogrammer: fDQMHistogrammerVector)
                if(dqmHistogrammer->fill(streamDataBuffer)) break;
        }
    }

    return fRunning;
}
