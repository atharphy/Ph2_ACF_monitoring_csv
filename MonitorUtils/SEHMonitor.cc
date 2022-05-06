#include "SEHMonitor.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWInterface/D19clpGBTInterface.h"
#include "../Utils/CharArray.h"
#include "../Utils/ContainerFactory.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

SEHMonitor::SEHMonitor(const Ph2_System::SystemController* theSystCntr, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystCntr, theDetectorMonitorConfig)
{
// doMonitorTemperature = fDetectorMonitorConfig.isElementToMonitor("ModuleTemperature");
// doMonitorInputCurrent = fDetectorMonitorConfig.isElementToMonitor("I_SEH");
#ifdef __USE_ROOT__
    fMonitorPlotDQM    = new MonitorDQMPlotCBC();
    fMonitorDQMPlotCBC = static_cast<MonitorDQMPlotCBC*>(fMonitorPlotDQM);
    fMonitorDQMPlotCBC->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void SEHMonitor::runMonitor()
{
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("Board")) runInputCurrentMonitor(registerName);

    // if(doMonitorInputCurrent) runInputCurrentMonitor();
}

void SEHMonitor::runInputCurrentMonitor(std::string registerName)
{
    LOG(INFO) << BOLDMAGENTA << "Running Input Current Monitor" << RESET;

    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr) continue;
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = (fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, "ADC1");
            LOG(INFO) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - "
                      << "ADC1"
                      << " = " << registerValue << RESET;
            // theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() = std::make_tuple(getTimeStamp(), registerValue);
        }
    }
    LOG(INFO) << BOLDMAGENTA << "We pretend to be a measurement" << RESET;

    // for(const auto& board: *theSystCntr.fDetectorContainer)
    // {
    //     for(const auto& opticalGroup: *board)
    //     {

    // for(const auto& hybrid: *opticalGroup)
    // {
    // uint16_t cbcOrMpa = theSystCntr.fCicInterface->ReadChipReg(static_cast<const Ph2_HwDescription::OuterTrackerHybrid*>(hybrid)->fCic, "CBCMPA_SEL"); // just to read something
    //     LOG(INFO) << BOLDMAGENTA << "Hybrid " << hybrid->getId() << " - CBCMPA_SEL = " << cbcOrMpa << RESET;
    // }
}
