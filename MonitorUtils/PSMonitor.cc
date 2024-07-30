#include "MonitorUtils/PSMonitor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#include "Utils/ValueAndTime.h"

#ifdef __USE_ROOT__
#include "MonitorDQM/MonitorDQMPlotPS.h"
#include "TFile.h"
#endif

using namespace Ph2_HwInterface;

PSMonitor::PSMonitor(Ph2_System::SystemController* theSystemController, const DetectorMonitorConfig& theDetectorMonitorConfig) : OTMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM = new MonitorDQMPlotPS();
    static_cast<MonitorDQMPlotPS*>(fMonitorPlotDQM)->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void PSMonitor::runMonitor()
{
    std::recursive_mutex                  theMutex;
    std::lock_guard<std::recursive_mutex> theGuard(theMutex);
    for(const auto& monitorValueName: fDetectorMonitorConfig.fMonitorElementList.at("SSA2"))
        if(monitorValueName.second) runMonitorSSA(monitorValueName.first);
    for(const auto& monitorValueName: fDetectorMonitorConfig.fMonitorElementList.at("MPA2"))
        if(monitorValueName.second) runMonitorMPA(monitorValueName.first);
    for(const auto& monitorValueName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT"))
        if(monitorValueName.second) runMonitorLpGBT(monitorValueName.first);
}

void PSMonitor::runMonitorSSA(const std::string& monitorValueName)
{
    auto theSSA2RegisterContainer = getReadoutChipMonitorValues(monitorValueName, FrontEndType::SSA2);

#ifdef __USE_ROOT__
    static_cast<MonitorDQMPlotPS*>(fMonitorPlotDQM)->fillSSA2RegisterPlots(theSSA2RegisterContainer, monitorValueName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSMonitorSSA2Register");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theSSA2RegisterContainer, monitorValueName);
    }
#endif
}

void PSMonitor::runMonitorMPA(const std::string& monitorValueName)
{
    auto theMPA2RegisterContainer = getReadoutChipMonitorValues(monitorValueName, FrontEndType::MPA2);

#ifdef __USE_ROOT__
    static_cast<MonitorDQMPlotPS*>(fMonitorPlotDQM)->fillMPA2RegisterPlots(theMPA2RegisterContainer, monitorValueName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSMonitorMPA2Register");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theMPA2RegisterContainer, monitorValueName);
    }
#endif
}

void PSMonitor::readChipMonitorValue(const std::string& monitorValueName, Ph2_HwDescription::ReadoutChip* theChip, DetectorDataContainer& theDataContainer)
{
    uint16_t registerValue = fTheSystemController->fReadoutChipInterface->readADC(theChip, monitorValueName);
    LOG(DEBUG) << BOLDMAGENTA << "board " << theChip->getBeBoardId() << "opticalGroup " << theChip->getOpticalGroupId() << "hybrid " << theChip->getHybridId() << " - chip " << theChip->getId() << " "
               << monitorValueName << " = " << registerValue << RESET;
    auto  theADCcalibrationMap = theChip->getADCCalibrationMap();
    float theConversionFactor  = 1000;                                                                          // without the conversion factor the voltages are not visible
    if(monitorValueName == "AVDD" || monitorValueName == "DVDD") theConversionFactor = theConversionFactor * 2; // keep into account a voltage divider
    auto theSlope  = theADCcalibrationMap["ADC_SLOPE"] * theConversionFactor;
    auto theOffset = theADCcalibrationMap["ADC_OFFSET"] * theConversionFactor;
    registerValue  = registerValue * theSlope + theOffset;
    ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
    theDataContainer.getChip(theChip->getBeBoardId(), theChip->getOpticalGroupId(), theChip->getHybridId(), theChip->getId())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
}
