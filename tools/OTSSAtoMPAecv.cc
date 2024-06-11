#include "tools/OTSSAtoMPAecv.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/ReadoutChip.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/GenericDataArray.h"
#include "Utils/PatternMatcher.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTSSAtoMPAecv::fCalibrationDescription = "Can phase and strenght on the SSA to MPA lines";

OTSSAtoMPAecv::OTSSAtoMPAecv() : OTverifyCICdataWord() {}

OTSSAtoMPAecv::~OTSSAtoMPAecv() {}

void OTSSAtoMPAecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    ContainerFactory::copyAndInitHybrid<GenericDataArray<float, NUMBER_OF_CIC_PORTS, 9>>(*fDetectorContainer, fScanEfficiencyContainer);

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTSSAtoMPAecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTSSAtoMPAecv::ConfigureCalibration()
{

}

void OTSSAtoMPAecv::Running()
{
    LOG(INFO) << "Starting OTSSAtoMPAecv measurement.";
    Initialise();
    runIntegrityTest();
    LOG(INFO) << "Done with OTSSAtoMPAecv.";
    Reset();
}

void OTSSAtoMPAecv::Stop(void)
{
    LOG(INFO) << "Stopping OTSSAtoMPAecv measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTSSAtoMPAecv.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTSSAtoMPAecv stopped.";
}

void OTSSAtoMPAecv::Pause()
{

}

void OTSSAtoMPAecv::Resume()
{

}

void OTSSAtoMPAecv::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTSSAtoMPAecv::injectStubsPS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket)
{
    // ReadoutChip* theSSA = nullptr;
    // try
    // {
    //     theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId() % 8);
    // }
    // catch(const std::exception& e)
    // {
    //     LOG(INFO) << YELLOW << "            skipping MPA Id " << +theMPA->getId() << " since corresponding SSA is not enabled" << RESET;
    //     return;
    // }

}

void OTSSAtoMPAecv::injectL1PS(ReadoutChip* theMPA, uint8_t chipIdForCIC, D19cFWInterface* theFWInterface, uint8_t numberOfBytesInSinglePacket)
{
    // ReadoutChip* theSSA = nullptr;
    // try
    // {
    //     theSSA = fDetectorContainer->getObject(theMPA->getBeBoardId())->getObject(theMPA->getOpticalGroupId())->getObject(theMPA->getHybridId())->getObject(theMPA->getId() % 8);
    // }
    // catch(const std::exception& e)
    // {
    //     LOG(INFO) << YELLOW << "            skipping MPA Id " << +theMPA->getId() << " since corresponding SSA is not enabled" << RESET;
    //     return;
    // }



}