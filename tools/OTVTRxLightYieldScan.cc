#include "tools/OTVTRxLightYieldScan.h"
#include "HWDescription/VTRx.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "HWInterface/VTRxInterface.h"
#include "MonitorUtils/DetectorMonitor.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTVTRxLightYieldScan::fCalibrationDescription = "Scan VTRx+ ouput bias and modulation settings and measure the light power";

OTVTRxLightYieldScan::OTVTRxLightYieldScan() : Tool() {}

OTVTRxLightYieldScan::~OTVTRxLightYieldScan() {}

void OTVTRxLightYieldScan::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTVTRxLightYieldScan.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
    std::string fJsonOutputPath = findValueInSettings<std::string>("JsonOutfile", "");
    if(fJsonOutputPath != "")
    {
        LOG(INFO) << BOLDYELLOW << "Writing json output to : " << fJsonOutputPath << RESET;
        std::ofstream* outStream = new std::ofstream(fJsonOutputPath);
        setOfStream(outStream);
    }
}

void OTVTRxLightYieldScan::ConfigureCalibration() {}

void OTVTRxLightYieldScan::Running()
{
    LOG(INFO) << "Starting OTVTRxLightYieldScan measurement.";
    Initialise();
    scanVTRxLightYield();
    LOG(INFO) << "Done with OTVTRxLightYieldScan.";
    Reset();
}

void OTVTRxLightYieldScan::Stop(void)
{
    LOG(INFO) << "Stopping OTVTRxLightYieldScan measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTVTRxLightYieldScan.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTVTRxLightYieldScan stopped.";
}

void OTVTRxLightYieldScan::Pause() {}

void OTVTRxLightYieldScan::Resume() {}

void OTVTRxLightYieldScan::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTVTRxLightYieldScan::scanVTRxLightYield()
{
    if(fDetectorMonitor != nullptr) fDetectorMonitor->pauseMonitoring();

    json j;
    j["type"] = "data";
    for(int biasValue = 40; biasValue <= 56; biasValue += 4)
    {
        LOG(INFO) << BOLDYELLOW << "Setting VTRx bias to 0x" << std::hex << +biasValue << std::dec << RESET;

        for(int modulationValue = 24; modulationValue <= 40; modulationValue += 4)
        {
            LOG(INFO) << BOLDMAGENTA << "    Setting VTRx modulation to 0x" << std::hex << +modulationValue << std::dec << RESET;

            DetectorDataContainer theOpticalPowerContainer;
            ContainerFactory::copyAndInitOpticalGroup<float>(*fDetectorContainer, theOpticalPowerContainer);
            std::vector<std::pair<std::string, uint16_t>> theRegisterValues;
            theRegisterValues.push_back({"CH1BIAS", biasValue});
            theRegisterValues.push_back({"CH1MOD", modulationValue | 0x80});
            for(auto theBoard: *fDetectorContainer)
            {
                D19cFWInterface* theFWinterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
                for(auto theOpticalGroup: *theBoard)
                {
                    bool     registersWritten       = false;
                    uint16_t numberOfAttempts       = 0;
                    uint16_t maximumAllowedAttempts = 10;
                    while(!registersWritten)
                    {
                        registersWritten = fVTRxInterface->WriteChipMultReg(theOpticalGroup->fVTRx, theRegisterValues, true);
                        if(++numberOfAttempts >= maximumAllowedAttempts) break;
                    }
                    usleep(10000);
                    float VTRxLightYieldRX = 0;
                    if(numberOfAttempts == maximumAllowedAttempts)
                    {
                        VTRxLightYieldRX = -1;
                        continue;
                    }
                    else
                    {
                        // It has been observed that often there are issues reading Optical Group 0 with measurements
                        // giving as power values powers of 2, with often 2^16 or 0xFFFF. To avoid checking multiple
                        // power of 2 combinations, multiple measurements are collected for the same bias and modulation point.
                        // Then the measurement are compared excluding the 7 least significant bits to take into account real fluctuations.
                        // The most common measurement is then chosen.
                        int requiredGood = 5;
                        int valueCount = 0;
                        int maxAttempts = 10; // safety limit
                        
                        std::vector<double> powerValues;
                        std::map<uint16_t, int> counts;
                        for (int attempt = 0; attempt < maxAttempts; ++attempt)
                        {
                            auto VTRxLightYieldRXMap = theFWinterface->GetSFPParameter(theOpticalGroup, "RX");
                            
                            auto error  = VTRxLightYieldRXMap.first;
                            auto result = VTRxLightYieldRXMap.second;
                        
                            uint16_t raw = static_cast<uint16_t>(result * 10); // power measurements divide the raw by 10                       
                            if (error == 0) // considering only measurements with no reading errors
                            {
                                powerValues.push_back(raw);
                                uint16_t masked = raw & 0xFF80;
                                counts[masked]++;
                                valueCount++;
                            }

                            if (valueCount >= requiredGood)
                                break;

                            if (attempt == maxAttempts)
                            {
                                LOG(ERROR) << ERROR_FORMAT << " For OpticalGroup " << theOpticalGroup->getId() << " reached maxRetries: Power: "<<  result << "mW and error " << error << RESET;
                            }
                        }

                        // find most popular "bin"
                        uint16_t bestValue = 0;
                        int maxCount = 0;
                    
                        for (auto& [val, count] : counts)
                        {
                            if (count > maxCount)
                            {
                                maxCount = count;
                                bestValue = val;
                            }
                        }
                        //  filter original values
                        std::vector<double> filtered;

                        for (auto val : powerValues)
                        {
                            uint16_t raw = static_cast<uint16_t>(val);
                            uint16_t masked = raw & 0xFF80;

                            if (masked == bestValue)
                            {
                                filtered.push_back(val);  // keep ORIGINAL value
                            }
                        }


                        // -------- compute final value --------
                        VTRxLightYieldRX =
                            (std::accumulate(filtered.begin(), filtered.end(), 0.0) / filtered.size())*0.1;
                    }
                    theOpticalPowerContainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<float>() = VTRxLightYieldRX;
                    if(fOfStream != nullptr)
                    {
                        j["data"]["VTRxLightYield"][std::to_string(theBoard->getId())][std::to_string(theOpticalGroup->getId())][std::to_string(biasValue)][std::to_string(modulationValue)] =
                            VTRxLightYieldRX;
                    }
                }
            }
#ifdef __USE_ROOT__
            fDQMHistogramOTVTRxLightYieldScan.fillOpticalPower(theOpticalPowerContainer, biasValue, modulationValue);
#else
            if(fDQMStreamer)
            {
                ContainerSerialization theLightYieldSerialization("OTVTRxLightYieldScanOpticalPower");
                theLightYieldSerialization.streamByBoardContainer(fDQMStreamer, theOpticalPowerContainer, biasValue, modulationValue);
            }
#endif
        }
    }
    if(fOfStream != nullptr) { *(fOfStream) << j << std::endl; }

    if(fDetectorMonitor != nullptr) fDetectorMonitor->resumeMonitoring();
}
