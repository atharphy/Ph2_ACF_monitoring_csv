#include "OTSensorTemperature.h"
#ifdef __USE_ROOT__
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTSensorTemperature::OTSensorTemperature() : OTTool() {}

OTSensorTemperature::~OTSensorTemperature() {}

// Initialization function
void OTSensorTemperature::Initialise()
{
    Prepare();
    SetName("OTSensorTemperature");
}

// State machine control functions
void OTSensorTemperature::Running()
{
    Initialise();
    fSuccess = true;
    ReadModuleTemperatures();
    Reset();
}

void OTSensorTemperature::ReadThermistors(const OpticalGroup* pOpticalGroup)
{
#ifdef __USE_ROOT__
    std::map<std::string, std::pair<std::string, std::string>> cNTCMap = pOpticalGroup->fNTCMap;
    for(auto it = cNTCMap.begin(); it != cNTCMap.end(); it++)
    {
        std::string type        = it->first;
        std::string adc         = it->second.first;
        std::string lut         = it->second.second;
        float       temperature = ReadThermistor(pOpticalGroup, adc, lut);
        LOG(INFO) << BOLDBLUE << type << " (" << adc << ") Temperature: " << temperature << "°C" << RESET;
        if(fOfStream != nullptr)
        {
            json j;
            j["type"]                = "data";
            j["data"]["temperature"] = temperature;
            *(fOfStream) << j << std::endl;
        }
    }
#endif
}

// Read thermistor temperature
float OTSensorTemperature::ReadThermistor(const OpticalGroup* pOpticalGroup, std::string pADC, std::string pLUT)
{
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return -1;

    uint16_t cOffset = flpGBTInterface->GetADCOffset(clpGBT, 0);
    float    cGain   = flpGBTInterface->GetADCGain(clpGBT, 0);
    LOG(DEBUG) << "Offset: " << +cOffset << " --- Gain: " << cGain << RESET;

    auto cLSQResistance = flpGBTInterface->ReadResistance(clpGBT, pADC, fCurrentDACs, fGain); // in ADC units
    LOG(DEBUG) << "Resistance in ADC units: " << cLSQResistance << RESET;
    cLSQResistance = (cLSQResistance - cOffset * (1 - cGain / 2)) / (cGain * 512) * 1e-3; // in kOhms
    LOG(DEBUG) << "Resistance in kOhms: " << cLSQResistance << RESET;

    // get them from file
    float cFirstTemp = 0, cSecondTemp = 0, cFirstResistance = 0, cSecondResistance = 0;

    // read file line by line
    std::string   cFilename = pLUT;
    std::ifstream file(cFilename);
    if(file.is_open())
    {
        std::string line;
        float       cPrevTemp       = -40;   // Min temperature
        float       cPrevResistance = 41.78; // Max resistance
        std::string delimiter       = ",";
        while(std::getline(file, line))
        {
            // get temp and resistance from line string
            size_t             pos = 0;
            std::string        token;
            std::vector<float> cLineValues(0);
            while((pos = line.find(delimiter)) != std::string::npos)
            {
                token = line.substr(0, pos);
                line.erase(0, pos + delimiter.length());
                cLineValues.push_back(stof(token));
            }

            float cTemp       = cLineValues.at(0);
            float cResistance = cLineValues.at(2);
            LOG(DEBUG) << "Temperature: " << cTemp << " --- Resistance: " << cResistance << RESET;

            if(cLSQResistance <= cPrevResistance && cLSQResistance > cResistance)
            {
                cFirstTemp        = cPrevTemp;
                cSecondTemp       = cTemp;
                cFirstResistance  = cPrevResistance;
                cSecondResistance = cResistance;
                LOG(DEBUG) << "Resistance between " << cFirstResistance << " and " << cSecondResistance << " --- Interpolate between " << cFirstTemp << "°C and " << cSecondTemp << "°C" << RESET;
            }
            cPrevTemp       = cTemp;
            cPrevResistance = cResistance;
        }
        file.close();
    }
    else
    {
        LOG(INFO) << BOLDRED << "File " << cFilename << " could not be opened! Resistance to temperature translation not possible!" << RESET;
    }
    float cSlope     = (cSecondTemp - cFirstTemp) / (cSecondResistance - cFirstResistance);
    float cIntercept = cSecondTemp - cSlope * cSecondResistance;
    float cTemp      = cSlope * cLSQResistance + cIntercept;
    LOG(DEBUG) << BOLDBLUE << "NTC Resistance is " << cLSQResistance << " kOhms ---- Temperature of NTC is " << cTemp << "°C" << RESET;

    // Current time
    auto               t  = std::time(nullptr);
    auto               tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
    auto cTime = oss.str();

    // Write temperature to file
    std::ofstream cOutputfile;
    int           cOGID           = pOpticalGroup->getId();
    std::string   cOutputfilename = "./Temperatures/Temps_OG" + std::to_string(cOGID) + ".txt";
    cOutputfile.open(cOutputfilename, std::ios_base::app); // append instead of overwrite
    cOutputfile << cTime << "," << cTemp << "," << cLSQResistance << "\n";

    return cTemp;
}

// Read module temperatures
void OTSensorTemperature::ReadModuleTemperatures()
{
    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;
            ReadThermistors(cOpticalGroup);
        }
    }
}

void OTSensorTemperature::Stop() {}

void OTSensorTemperature::Pause() {}

void OTSensorTemperature::Resume() {}

void OTSensorTemperature::writeObjects() {}
