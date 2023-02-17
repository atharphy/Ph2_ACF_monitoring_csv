#include "DQMUtils/DQMInterface.h"
#include "DQMUtils/DQMCalibrationFactory.h"
#include "NetworkUtils/TCPSubscribeClient.h"
#include "Parser/FileParser.h"
#include "Utils/ContainerSerialization.h"

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
    // for(auto dqmHistogrammer: fDQMHistogrammerVector) delete dqmHistogrammer;
    // fDQMHistogrammerVector.clear();
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
    int                       timeout = 3; // in seconds

    fListener->disconnect();
    while(fRunningFuture.wait_for(span) == std::future_status::timeout && timeout > 0)
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
    // CheckStream* theCurrentStream;
    // int               packetNumber = -1;
    std::vector<char> tmpDataBuffer;
    PacketHeader thePacketHeader;
    uint8_t packerHeaderSize = thePacketHeader.getPacketHeaderSize();

    while(fRunning)
    {
        // LOG(INFO) << __PRETTY_FUNCTION__ << " Running = " << fRunning << RESET;
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
        LOG(DEBUG) << "Tmp buffer size: " << tmpDataBuffer.size() << RESET;
        fDataBuffer.insert(fDataBuffer.end(), tmpDataBuffer.begin(), tmpDataBuffer.end());
        LOG(DEBUG) << "Data buffer size: " << fDataBuffer.size() << RESET;
        while(fDataBuffer.size() > 0)
        {
            if(fDataBuffer.size() < packerHeaderSize)
            {
                LOG(WARNING) << BOLDBLUE << "Not enough bytes to retrieve data stream" << RESET;
                break; // Not enough bytes to retreive the packet size
            }
            uint32_t packetSize = thePacketHeader.getPacketSize(fDataBuffer);

            LOG(DEBUG) << "Vector size  = " << fDataBuffer.size() << "; expected = " << packetSize << RESET;

            if(fDataBuffer.size() < packetSize)
            {
                LOG(DEBUG) << "Packet not completed, waiting" << RESET;
                break;
            }

            std::vector<char> streamDataBuffer(fDataBuffer.begin() + packerHeaderSize, fDataBuffer.begin() + packetSize);
            fDataBuffer.erase(fDataBuffer.begin(), fDataBuffer.begin() + packetSize);

            for(auto dqmHistogrammer: fDQMHistogrammerVector)
                if(dqmHistogrammer->fill(streamDataBuffer)) break;
        }
    }

    return fRunning;
}
