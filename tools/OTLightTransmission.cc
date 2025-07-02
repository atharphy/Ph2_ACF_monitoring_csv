#include "OTLightTransmission.h"
#include "HWInterface/D19cFWInterface.h"
#ifdef __USE_ROOT__
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTLightTransmission::fCalibrationDescription = "Measure all light transmission parameters";

OTLightTransmission::OTLightTransmission() : OTTool() {}

OTLightTransmission::~OTLightTransmission() {}

// Initialization function
void OTLightTransmission::Initialise()
{
    Prepare();
    SetName("OTLightTransmission");
}

// State machine control functions
void OTLightTransmission::Running()
{
    Initialise();
    fSuccess = true;
    ReadRegisters();
    Reset();
}

void OTLightTransmission::ReadRegisters()
{
#ifdef __USE_ROOT__
    json j;
    j["type"] = "data";
    for(auto theBoard: *fDetectorContainer)
    {
        D19cFWInterface* theFWinterface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
        for(auto theOpticalGroup: *theBoard)
        {
            j["data"]["RX"] = theFWinterface->GetSFPParameter(theOpticalGroup, "RX");
            LOG(INFO) << BOLDBLUE << "Transciever RX is : " << j["data"]["RX"] << RESET;
        }
    }
    if(fOfStream != nullptr) { *(fOfStream) << j << std::endl; }
#endif
}

void OTLightTransmission::Stop() {}

void OTLightTransmission::Pause() {}

void OTLightTransmission::Resume() {}
