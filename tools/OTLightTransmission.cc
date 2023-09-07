#include "OTLightTransmission.h"
#include "HWInterface/D19cFWInterface.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTLightTransmission::OTLightTransmission(int channel) : OTTool() 
{
    cChannel = channel;
}

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
    json j;
    j["type"] = "data";
    D19cFWInterface* interface = dynamic_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
    std::list<std::string> to_read{ "T", "V", "I", "TX", "RX" };
    for (auto item : to_read)
    {
        j["data"][item] = interface->GetSFPParameter_L12(item, cChannel);
	LOG(INFO) << BOLDBLUE << "Transciever measurement for" <<  item << " is " << j["data"][item] << RESET;
    }
    if(fOfStream != nullptr)
    {
	*(fOfStream) << j << std::endl;
    }
}

void OTLightTransmission::Stop() {}

void OTLightTransmission::Pause() {}

void OTLightTransmission::Resume() {}
