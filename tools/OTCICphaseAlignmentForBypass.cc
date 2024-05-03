#include "tools/OTCICphaseAlignmentForBypass.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCICphaseAlignmentForBypass::fCalibrationDescription = "Insert brief calibration description here";

OTCICphaseAlignmentForBypass::OTCICphaseAlignmentForBypass() : OTCICphaseAlignment() {}

OTCICphaseAlignmentForBypass::~OTCICphaseAlignmentForBypass() {}

void OTCICphaseAlignmentForBypass::Initialise()
{
    OTCICphaseAlignment::Initialise();

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                fCicInterface->WriteChipReg(static_cast<OuterTrackerHybrid*>(theHybrid)->fCic, "MUX_CTRL", 0x10); // enable CIC bypass;
            }
        }
    }
}
