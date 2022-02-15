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
float OTTemperature::ReadThermistor(const OpticalGroup* pOpticalGroup, std::string pADC, float pR0, float pB)
{
    auto& clpGBT = pOpticalGroup->flpGBT;
    if(clpGBT == nullptr) return -1;

    auto cLSQResistance = flpGBTInterface->ReadResistance(clpGBT, pADC, fCurrentDACs, fGain); // in ADC units
    cLSQResistance      = (cLSQResistance * fVref / 1023) * 1e-3;                             // in kOhms
    float cTinvK        = 1.0 / (25 + 273.5) + (1. / pB) * std::log(cLSQResistance / pR0);
    float cT            = 1.0 / cTinvK - 273.5;
    LOG(INFO) << BOLDBLUE << "Resistance is " << cLSQResistance << " kOhms"
              << " R[25°C] is " << pR0 << " temperature [inv K ] " << cTinvK << " temperature in celsius is " << cT << RESET;
    return cT;
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
            flpGBTInterface->ConfigureInternalMonitoring(clpGBT, 0);
            // read temperature sensor
            auto cLpgbtTemp = flpGBTInterface->GetInternalTemperature(clpGBT);
            LOG(INFO) << BOLDBLUE << "Internal temperature sensor of lpGBT reads " << cLpgbtTemp << " which converts to " << cLpgbtTemp * (fVref / 1023) << RESET;

            bool                     cWith2S       = true;
            std::vector<std::string> cReferenceADC = {"ADC2"};
            if(cOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS)
            {
                cWith2S = false;
                cReferenceADC.clear();
            }
            for(auto cRefADC: cReferenceADC)
            {
                float              cExpected = (fVinput2S * 0.49 / 10.); // for 2S modules powered at 10.4 V
                std::vector<float> cMeasurements(0);
                for(uint8_t cIndx = 0; cIndx < 10; cIndx++) { cMeasurements.push_back(flpGBTInterface->ReadADC(clpGBT, cRefADC, "VREF/2", fGain)); }
                float cMean    = std::accumulate(cMeasurements.begin(), cMeasurements.end(), 0.) / cMeasurements.size();
                float cVoltage = (cMean) * (fVref / 1023);
                LOG(INFO) << BOLDBLUE << "Gain of " << +fGain << "\t" << cRefADC << " ADC reading " << cMean << " converted voltage " << cVoltage << " expected voltage is " << cExpected
                          << " offset is " << std::fabs(cVoltage - cExpected) << RESET;
            }

            float cR0 = cWith2S ? 10.0 : 1.0;
            float cB  = cWith2S ? 3950 : 3500;
            ReadThermistor(cOpticalGroup, "ADC4", cR0, cB);
        }
    }
}

void OTTemperature::Stop() {}

void OTTemperature::Pause() {}

void OTTemperature::Resume() {}

void OTTemperature::writeObjects() {}