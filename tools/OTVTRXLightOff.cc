#include "OTVTRXLightOff.h"
#include "HWInterface/BeBoardInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cOpticalInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTVTRXLightOff::OTVTRXLightOff() : OTTool() {}

OTVTRXLightOff::~OTVTRXLightOff() {}

// Initialization function
void OTVTRXLightOff::Initialise()
{
    Prepare();
    SetName("OTVTRXLightOff");
}

// State machine control functions
void OTVTRXLightOff::Running()
{
    Initialise();
    TurnOffLight();
    fSuccess     = true;
    fKeepRunning = false;
    // Reset();
}

void OTVTRXLightOff::TurnOffLight()
{
    // bool cIgnoreI2c    = true;
    // bool cReInitialize = true;
    // this->ConfigureHw(cIgnoreI2c, cReInitialize);
    for(const auto cBoard: *fDetectorContainer)
    {
        D19cFWInterface*      pInterface        = static_cast<D19cFWInterface*>(fBeBoardFWMap.find(cBoard->getId())->second);
        D19cOpticalInterface* cOpticalInterface = static_cast<D19cOpticalInterface*>(pInterface->getFEConfigurationInterface());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;

            uint8_t cMasterId = 1, cSlaveAddress = 0x50, cNbyte = 2, cFrequency = 2; //, cSlaveData = 0x15;
            uint8_t cMasterConfig = (cNbyte << 2) | (cFrequency << 0);
            LOG(INFO) << BOLDRED << "Turn off light ouptput of VTRX. Powercycle mandatory to re-establish module communication" << RESET;
            cOpticalInterface->SingleMultiByteWriteI2C(clpGBT, cMasterId, cMasterConfig, cSlaveAddress, 0x0000, false);
        }
        LOG(INFO) << BOLDRED << "Turn off light ouptput of SFP cages" << RESET;
        pInterface->WriteReg("fc7_daq_cnfg.optical_block.enable.l8", 0xFF);
        pInterface->WriteReg("fc7_daq_cnfg.optical_block.enable.l12", 0xFF);
    }
    LOG(INFO) << BOLDRED << "SFP cages dark, VTRX asleep, good night..." << RESET;
    // exit(0);
}

void OTVTRXLightOff::Stop() {}

void OTVTRXLightOff::Pause() {}

void OTVTRXLightOff::Resume() {}

void OTVTRXLightOff::writeObjects() {}