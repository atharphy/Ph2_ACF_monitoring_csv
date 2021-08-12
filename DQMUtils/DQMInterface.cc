#include "../NetworkUtils/TCPSubscribeClient.h"
#include "../System/FileParser.h"
#include "../System/SystemController.h"
#include "../Utils/Container.h"
#include "../Utils/ObjectStream.h"

#include "CBCHistogramPulseShape.h"
#include "DQMHistogramCalibrationExample.h"
#include "DQMHistogramLatencyScan.h"
#include "DQMHistogramPedeNoise.h"
#include "DQMHistogramPedestalEqualization.h"
#include "DQMInterface.h"
#include "RD53ClockDelayHistograms.h"
#include "RD53DataTransmissionTestGraphs.h"
#include "RD53GainHistograms.h"
#include "RD53GainOptimizationHistograms.h"
#include "RD53InjectionDelayHistograms.h"
#include "RD53LatencyHistograms.h"
#include "RD53PhysicsHistograms.h"
#include "RD53PixelAliveHistograms.h"
#include "RD53SCurveHistograms.h"
#include "RD53ThrEqualizationHistograms.h"
#include "RD53ThresholdHistograms.h"
#include "SSAPhysicsHistograms.h"

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

    Ph2_System::FileParser                                   fParser;
    std::map<uint16_t, Ph2_HwInterface::BeBoardFWInterface*> fBeBoardFWMap;
    std::stringstream                                        out;
    DetectorContainer                                        fDetectorStructure;
    Ph2_System::SettingsMap                                  pSettingsMap;

    fParser.parseHW(configurationFilePath, fBeBoardFWMap, &fDetectorStructure, out, true);
    fParser.parseSettings(configurationFilePath, pSettingsMap, out, true);

    if(calibrationName == "pedenoise")
        fDQMHistogrammerVector.push_back(new DQMHistogramPedeNoise());
    else if(calibrationName == "calibrationandpedenoise")
    {
        fDQMHistogrammerVector.push_back(new DQMHistogramPedestalEqualization());
        fDQMHistogrammerVector.push_back(new DQMHistogramPedeNoise());
    }
    else if(calibrationName == "OTLatency")
        fDQMHistogrammerVector.push_back(new DQMHistogramLatencyScan());
    else if(calibrationName == "calibrationexample")
        fDQMHistogrammerVector.push_back(new DQMHistogramCalibrationExample());
    else if(calibrationName == "cbcPulseShape")
        fDQMHistogrammerVector.push_back(new CBCHistogramPulseShape());
    else if(calibrationName == "pixelalive")
        fDQMHistogrammerVector.push_back(new PixelAliveHistograms());
    else if(calibrationName == "noise")
        fDQMHistogrammerVector.push_back(new PixelAliveHistograms());
    else if(calibrationName == "scurve")
        fDQMHistogrammerVector.push_back(new SCurveHistograms());
    else if(calibrationName == "gain")
        fDQMHistogrammerVector.push_back(new GainHistograms());
    else if(calibrationName == "gainopt")
        fDQMHistogrammerVector.push_back(new GainOptimizationHistograms());
    else if(calibrationName == "threqu")
        fDQMHistogrammerVector.push_back(new ThrEqualizationHistograms());
    else if(calibrationName == "thrmin")
        fDQMHistogrammerVector.push_back(new ThresholdHistograms());
    else if(calibrationName == "thradj")
        fDQMHistogrammerVector.push_back(new ThresholdHistograms());
    else if(calibrationName == "latency")
        fDQMHistogrammerVector.push_back(new LatencyHistograms());
    else if(calibrationName == "injdelay")
        fDQMHistogrammerVector.push_back(new InjectionDelayHistograms());
    else if(calibrationName == "clockdelay")
        fDQMHistogrammerVector.push_back(new ClockDelayHistograms());
    else if(calibrationName == "physics")
        fDQMHistogrammerVector.push_back(new PhysicsHistograms());
    else if(calibrationName == "ssaphysics")
        fDQMHistogrammerVector.push_back(new SSAPhysicsHistograms());
    else if(calibrationName == "datatrtest")
        fDQMHistogrammerVector.push_back(new DataTransmissionTestGraphs());

    fOutputFile = new TFile("tmp.root", "RECREATE");
    for(auto dqmHistogrammer: fDQMHistogrammerVector) dqmHistogrammer->book(fOutputFile, fDetectorStructure, pSettingsMap);
}

//========================================================================================================================
void DQMInterface::startProcessingData(std::string const& runNumber)
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

                for(auto dqmHistogrammer: fDQMHistogrammerVector)
                    if(dqmHistogrammer->fill(streamDataBuffer)) break;

                if(++packetNumber >= 256) packetNumber = 0;
            }
        }
    }

    return fRunning;
}
