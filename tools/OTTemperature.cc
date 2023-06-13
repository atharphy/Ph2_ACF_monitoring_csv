#include "OTTemperature.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTTemperature::OTTemperature() : OTTool() {}

OTTemperature::~OTTemperature() {}

// Initialization function
void OTTemperature::Initialise()
{
    Prepare();
    SetName("OTTemperature");
}

// State machine control functions
void OTTemperature::Running()
{
    Initialise();
    fSuccess = true;
    ReadModuleTemperatures();
    Reset();
}
// Set current
void OTTemperature::SetCurrents(std::vector<uint8_t> pCurrents)
{
    fCurrentDACs.clear();
    for(auto cCurrent: pCurrents) fCurrentDACs.push_back(cCurrent);
}
// Read thermistor temperature
float OTTemperature::ReadThermistor(const OpticalGroup* pOpticalGroup, std::string pADC)
{
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return -1;

    uint16_t cOffset = flpGBTInterface->GetADCOffset(clpGBT,0);
    float cGain = flpGBTInterface->GetADCGain(clpGBT,0);
    LOG(DEBUG) << "Offset: " << +cOffset << " --- Gain: " << cGain << RESET;

    auto cLSQResistance = flpGBTInterface->ReadResistance(clpGBT, pADC, fCurrentDACs, fGain); // in ADC units
    LOG(DEBUG) << "Resistance in ADC units: " << cLSQResistance << RESET;
    cLSQResistance = ( cLSQResistance - cOffset * ( 1 - cGain / 2 ) ) / ( cGain * 512 ) * 1e-3; // in kOhms
    LOG(DEBUG) << "Resistance in kOhms: " << cLSQResistance << RESET;
    
    // get them from file
    float cFirstTemp = 0, cSecondTemp = 0, cFirstResistance = 0, cSecondResistance = 0;

    // read file line by line
    std::string cFilename = pOpticalGroup->fNTCLookUpTable;
    std::ifstream file(cFilename);
    if (file.is_open()) {
        std::string line;
        float cPrevTemp = -40;              //Min temperature
        float cPrevResistance = 41.78;      //Max resistance
        std::string delimiter = ",";
        while (std::getline(file, line)) {
            // get temp and resistance from line string
            size_t pos = 0;
            std::string token;
            std::vector<float> cLineValues(0);
            while ((pos = line.find(delimiter)) != std::string::npos) {
                token = line.substr(0, pos);
                line.erase(0, pos + delimiter.length());
                cLineValues.push_back(stof(token));
            }

            float cTemp = cLineValues.at(0);
            float cResistance = cLineValues.at(2);
            LOG(DEBUG) << "Temperature: " << cTemp << " --- Resistance: " << cResistance << RESET;

            if ( cLSQResistance <= cPrevResistance && cLSQResistance > cResistance ) {
                cFirstTemp = cPrevTemp;
                cSecondTemp = cTemp;
                cFirstResistance = cPrevResistance;
                cSecondResistance = cResistance;
                LOG(DEBUG) << "Resistance between " << cFirstResistance << " and " << cSecondResistance
                        << " --- Interpolate between " << cFirstTemp << "°C and " << cSecondTemp << "°C" << RESET;
            }
            cPrevTemp = cTemp;
            cPrevResistance = cResistance;
        }
        file.close();
    }
    else
    {
        LOG(INFO) <<BOLDRED << "File " << cFilename <<" could not be opened! Resistance to temperature translation not possible!" << RESET;
    }
    float cSlope = ( cSecondTemp - cFirstTemp ) / ( cSecondResistance - cFirstResistance );
    float cIntercept = cSecondTemp - cSlope * cSecondResistance;
    float cTemp = cSlope * cLSQResistance + cIntercept;
    LOG(INFO) << BOLDBLUE << "NTC Resistance is " << cLSQResistance << " kOhms ---- Temperature of NTC is " << cTemp << "°C" << RESET;

    // Current time
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%d-%m-%Y %H-%M-%S");
    auto cTime = oss.str();

    // Write temperature to file
    std::ofstream cOutputfile;
    int cOGID =  pOpticalGroup->getId();
    std::string cOutputfilename = "./Temperatures/Temps_OG" + std::to_string(cOGID) + ".txt";
    cOutputfile.open(cOutputfilename, std::ios_base::app); // append instead of overwrite
    cOutputfile << cTime << "," << cTemp << "," << cLSQResistance << "\n";

    return cTemp;

}

float  OTTemperature::ReadInternalThermistor(const OpticalGroup* pOpticalGroup)
{
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return -1;
    auto cLpgbtTempInADC = flpGBTInterface->GetInternalTemperature(clpGBT);

    uint16_t cOffset = flpGBTInterface->GetADCOffset(clpGBT,0);
    float cGain = flpGBTInterface->GetADCGain(clpGBT,0);
    LOG(DEBUG) << "Offset: " << +cOffset << " --- Gain: " << cGain << RESET;
    float vPos = ( cLpgbtTempInADC- cOffset*(1 - cGain / 2.0 ) )/ ( cGain * 512 );
    // V = m * T + V0  where V0 is voltage at zero degrees and m is temperature coefficien and T the current temperature, resulting in a Voltage V
    // -> T = (V-V0 ) / m
    std::pair<float,float> coeff = clpGBT->getTemperatureCoefficients();
    float m = coeff.first;
    float v0 = coeff.second;
    LOG(DEBUG) <<" V0: " << v0 << RESET;
    LOG(DEBUG) <<" vPos: " << vPos << RESET;

    float temperature = (vPos - v0) /m;
    LOG(INFO)<<BOLDBLUE << "APPROXIMATED internal lpGBT Temperature: " << +temperature << "°C"<< RESET;
    return temperature;

}
// Read module temperatures
void OTTemperature::ReadModuleTemperatures()
{
    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;

            // first some sanity checks
            flpGBTInterface->ConfigureInternalMonitoring(clpGBT, 1);
            std::vector<std::string> cVoltages = {"VREF/2", "VDDIO", "VDD", "VDDA"};
            std::vector<float>       cVoltageADCReadings;
            for(auto cVoltageADC: cVoltages)
            {
                std::vector<float> cMeasurements(0);
                for(uint8_t cIndx = 0; cIndx < 10; cIndx++) { cMeasurements.push_back(flpGBTInterface->ReadADC(clpGBT, cVoltageADC, "VREF/2", fGain)); }
                float cMean    = std::accumulate(cMeasurements.begin(), cMeasurements.end(), 0.) / cMeasurements.size();
                float cVoltage = (cMean) * (fVref / 1023);
                LOG(INFO) << BOLDBLUE << "Gain of " << +fGain << "\t" << cVoltageADC << " ADC reading " << cMean << " converted voltage " << cVoltage << RESET;
                cVoltageADCReadings.push_back(cMean);
            }
            do
            {
               ReadInternalThermistor(cOpticalGroup);
                flpGBTInterface->ConfigureInternalMonitoring(clpGBT, 0);
                // read ADC value of temperature sensor
               ReadThermistor(cOpticalGroup, "ADC4");
            }
            while(fLoopReadout);
        }
    }
}

uint8_t OTTemperature::TuneLpGBTVref(std::string pADC, float pVoltage)
{
    uint8_t vref = 0;
    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;
            flpGBTInterface->ConfigureInternalMonitoring(clpGBT, 0);
            if ((pADC == "") && (pVoltage == 0))
            {
                vref = flpGBTInterface->TuneVref(clpGBT);
            }
            else{
                vref = flpGBTInterface->TuneVref(clpGBT, pADC, pVoltage);
            }
        }
    }
    return vref;
}


void OTTemperature::Stop() {}

void OTTemperature::Pause() {}

void OTTemperature::Resume() {}

void OTTemperature::writeObjects() {}