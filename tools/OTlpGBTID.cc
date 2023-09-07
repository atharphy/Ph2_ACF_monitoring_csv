#include "OTlpGBTID.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTlpGBTID::OTlpGBTID() : OTTool() {}

OTlpGBTID::~OTlpGBTID() {}

// Initialization function
void OTlpGBTID::Initialise()
{
    Prepare();
    SetName("OTlpGBTID");
}

// State machine control functions
void OTlpGBTID::Running()
{
    Initialise();
    fSuccess = true;
    ReadlpGBTIDs();
    Reset();
}

void OTlpGBTID::ReadlpGBTIDs() 
{
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto theLpGBT = cOpticalGroup->flpGBT;
            if(theLpGBT == nullptr) continue;
            uint32_t chipFuseId = flpGBTInterface->ReadChipFuseID(theLpGBT);
	    LOG(INFO) << BOLDBLUE << "lpGBT ID: " << chipFuseId << RESET;
	    if(fOfStream != nullptr)
	    {
	        json j;
	        j["type"] = "data";
	        j["data"]["ID"] = chipFuseId;
	        *(fOfStream) << j << std::endl;
	    }
        }
    }
}

void OTlpGBTID::Stop() {}

void OTlpGBTID::Pause() {}

void OTlpGBTID::Resume() {}
