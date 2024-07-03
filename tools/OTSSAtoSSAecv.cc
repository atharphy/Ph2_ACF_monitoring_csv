#include "tools/OTSSAtoSSAecv.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTSSAtoSSAecv::fCalibrationDescription = "Scan phase and strenght on the SSA to SSA lines";

OTSSAtoSSAecv::OTSSAtoSSAecv() : OTSSAtoMPAecv() {}

OTSSAtoSSAecv::~OTSSAtoSSAecv() {}

void OTSSAtoSSAecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTSSAtoSSAecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTSSAtoSSAecv::ConfigureCalibration() {}

void OTSSAtoSSAecv::Running()
{
    LOG(INFO) << "Starting OTSSAtoSSAecv measurement.";
    Initialise();
    runSSAtoSSAecvScan();
    LOG(INFO) << "Done with OTSSAtoSSAecv.";
    Reset();
}

void OTSSAtoSSAecv::Stop(void)
{
    LOG(INFO) << "Stopping OTSSAtoSSAecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTSSAtoSSAecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTSSAtoSSAecv stopped.";
}

void OTSSAtoSSAecv::Pause() {}

void OTSSAtoSSAecv::Resume() {}

void OTSSAtoSSAecv::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTSSAtoSSAecv::runSSAtoSSAecvScan() {}

void OTSSAtoSSAecv::setStubLogicParameters(ReadoutChip* theMPA)
{
    fReadoutChipInterface->WriteChipReg(theMPA, "StubWindow", 8);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeDM8", 0x01);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM76", 0x02);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM54", 0x03);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM32", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeM10", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP12", 0);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP34", 0x04);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP56", 0x05);
    fReadoutChipInterface->WriteChipReg(theMPA, "CodeP78", 0x06);
}

std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> OTSSAtoSSAecv::produceStripClusterList()
{
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> listOfInjectedStrips;

    listOfInjectedStrips.push_back({0,   1, 1});
    listOfInjectedStrips.push_back({0, 118, 1});

    return listOfInjectedStrips;
}

std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> OTSSAtoSSAecv::produceMatchingPixelClusterList(uint8_t colCoordinate)
{
    std::vector<std::tuple<uint8_t, uint8_t, uint8_t>> thePixelClusterList;
    thePixelClusterList.push_back({fStubRowCoordinate, colCoordinate == 1 ? 118 : 1, 1});
    return thePixelClusterList;
}

std::vector<std::vector<std::tuple<uint8_t, uint8_t, int>>> OTSSAtoSSAecv::producePossibleStubVectorList(const std::vector<std::tuple<uint8_t, uint8_t, uint8_t>>& thePixelClusterList)
{
    std::vector<std::vector<std::tuple<uint8_t, uint8_t, int>>> possibleStubVectorList;
    for(const auto& thePixelCluster: thePixelClusterList)
    {
        std::vector<std::tuple<uint8_t, uint8_t, int>> theStubVector{{fStubRowCoordinate, std::get<1>(thePixelCluster) * 2 + std::get<2>(thePixelCluster) - 1, 0}};
        possibleStubVectorList.push_back(theStubVector);
    }

    return possibleStubVectorList;
}
