#include "../NetworkUtils/TCPSubscribeClient.h"
#include "../System/FileParser.h"
#include "../Utils/Container.h"
#include "../Utils/ObjectStream.h"

#include "../MonitorUtils/DetectorMonitorConfig.h"
#include "MonitorDQMInterface.h"
#include "MonitorDQMPlotCBC.h"

#include "TFile.h"

#include <ctime>
#include <iostream>
#include <string>

//========================================================================================================================
MonitorDQMInterface::MonitorDQMInterface() : fListener(nullptr), fRunning(false), fOutputFile(nullptr) {}

//========================================================================================================================
MonitorDQMInterface::~MonitorDQMInterface(void)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    destroy();
}

//========================================================================================================================
void MonitorDQMInterface::destroy(void)
{
    LOG(INFO) << __PRETTY_FUNCTION__ << RESET;

    if(fListener != nullptr) delete fListener;
    destroyDQMs();
    fListener = nullptr;
    for(auto monitorDQM: fMonitorDQMVector) delete monitorDQM;
    fMonitorDQMVector.clear();
    delete fOutputFile;
    fOutputFile = nullptr;

    LOG(INFO) << __PRETTY_FUNCTION__ << " DONE" << RESET;
}

//========================================================================================================================
void MonitorDQMInterface::destroyDQMs(void)
{
    for(auto monitorDQM: fMonitorDQMVector) delete monitorDQM;
    fMonitorDQMVector.clear();

    LOG(INFO) << __PRETTY_FUNCTION__ << " DONE" << RESET;
}

//========================================================================================================================
void MonitorDQMInterface::configure(std::string const& monitorName, std::string const& configurationFilePath)
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

    Ph2_System::FileParser                                   fParser;
    std::map<uint16_t, Ph2_HwInterface::BeBoardFWInterface*> fBeBoardFWMap;
    std::stringstream                                        out;
    DetectorContainer                                        fDetectorStructure;

    fParser.parseHW(configurationFilePath, fBeBoardFWMap, &fDetectorStructure, out, true);

    DetectorMonitorConfig theDetectorMonitorConfig;
    std::string           monitoringType = fParser.parseMonitor(configurationFilePath, theDetectorMonitorConfig, out, true);

    if(monitoringType == "2S") fMonitorDQMVector.push_back(new MonitorDQMPlotCBC());

    fOutputFile = new TFile("Monitor_tmp.root", "RECREATE");
    for(auto monitorDQM: fMonitorDQMVector) monitorDQM->book(fOutputFile, fDetectorStructure, theDetectorMonitorConfig);
}

//========================================================================================================================
void MonitorDQMInterface::startProcessingData()
{
    fRunning       = true;
    fRunningFuture = std::async(std::launch::async, &MonitorDQMInterface::running, this);
}

//========================================================================================================================
void MonitorDQMInterface::stopProcessingData(void)
{
    fRunning = false;
    std::chrono::milliseconds span(1000);
    int                       timeout = 10; // in seconds

    fListener->close();
    while(fRunningFuture.wait_for(span) == std::future_status::timeout && timeout >= 0)
    {
        LOG(INFO) << __PRETTY_FUNCTION__ << " Process still running! Waiting " << timeout-- << " more seconds!" << RESET;
    }

    LOG(INFO) << __PRETTY_FUNCTION__ << " Thread done running" << RESET;

    if(fDataBuffer.size() > 0)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " Buffer should be empty, some data were not read, Aborting" << RESET;
        abort();
    }

    for(auto monitorDQM: fMonitorDQMVector) monitorDQM->process();
    fOutputFile->Write();
}

//========================================================================================================================
bool MonitorDQMInterface::running()
{
    CheckStream*      theCurrentStream;
    int               packetNumber = -1;
    std::vector<char> tmpDataBuffer;

    while(fRunning)
    {
        LOG(INFO) << __PRETTY_FUNCTION__ << " Running = " << fRunning << RESET;
        // if(receive(configBuffer, 1) != -1)
        // if(receive(*reinterpret_cast<std::vector<char>*>(*configBuffer.end()), 1) != -1)
        // TODO We need to optimize the data readout so we don't do multiple copies
        // TODO We need to optimize the data readout so we don't do multiple copies
        // TODO We need to optimize the data readout so we don't do multiple copies
        // if(fListener->receive(tmpDataBuffer, 0, 100000) > 0)
        {
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

                if(packetNumber < 0)
                    packetNumber = int(theCurrentStream->getPacketNumber()); // first packet received
                else if(theCurrentStream->getPacketNumber() != packetNumber)
                {
                    LOG(ERROR) << BOLDRED << "Packet number expected = " << --packetNumber << " But received " << int(theCurrentStream->getPacketNumber()) << ", Aborting" << RESET;
                    LOG(ERROR) << GREEN << "Did you check that the Endianness of the two comupters is the same?" << RESET;
                    abort();
                }

                LOG(DEBUG) << "Vector size  = " << fDataBuffer.size() << "; expected = " << theCurrentStream->getPacketSize() << RESET;

                if(fDataBuffer.size() < theCurrentStream->getPacketSize())
                {
                    LOG(DEBUG) << "Packet not completed, waiting" << RESET;
                    break;
                }

                std::vector<char> streamDataBuffer(fDataBuffer.begin(), fDataBuffer.begin() + theCurrentStream->getPacketSize());
                fDataBuffer.erase(fDataBuffer.begin(), fDataBuffer.begin() + theCurrentStream->getPacketSize());

                for(auto monitorDQM: fMonitorDQMVector)
                    if(monitorDQM->fill(streamDataBuffer)) break;

                if(++packetNumber >= 256) packetNumber = 0;
            }
        }
    }

    return fRunning;
}
