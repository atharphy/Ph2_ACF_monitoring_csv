#include "CBCMonitor.h"
#include "../HWDescription/Definition.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../HWInterface/D19clpGBTInterface.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/CharArray.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

using namespace Ph2_HwInterface;

CBCMonitor::CBCMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
    fDoMonitorThreshold  = fDetectorMonitorConfig.isElementToMonitor("CBCThreshold");
    fDoMonitorLpGBT_ADC1 = fDetectorMonitorConfig.isElementToMonitor("LpGBT_ADC1");
    fDoMonitorLpGBT_VDD  = fDetectorMonitorConfig.isElementToMonitor("LpGBT_VDD");
    fDoMonitorLpGBT_VDDA = fDetectorMonitorConfig.isElementToMonitor("LpGBT_VDDA");
    fDoMonitorLpGBT_TEMP = fDetectorMonitorConfig.isElementToMonitor("LpGBT_TEMP");

#ifdef __USE_ROOT__
    fMonitorPlotDQM    = new MonitorDQMPlotCBC();
    fMonitorDQMPlotCBC = static_cast<MonitorDQMPlotCBC*>(fMonitorPlotDQM);
    fMonitorDQMPlotCBC->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void CBCMonitor::runMonitor()
{
    if(fDoMonitorThreshold) runCBCRegisterMonitor("VCth");
    if(fDoMonitorLpGBT_ADC1) runLpGBTRegisterMonitor("ADC1");
    if(fDoMonitorLpGBT_VDD) runLpGBTRegisterMonitor("VDD");
    if(fDoMonitorLpGBT_VDDA) runLpGBTRegisterMonitor("VDDA");
    if(fDoMonitorLpGBT_TEMP) runLpGBTRegisterMonitor("TEMP");
}

void CBCMonitor::runCBCRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theCBCRegisterContainer;
    ContainerFactory::copyAndInitChip<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theCBCRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    uint16_t registerValue = fTheSystemController->fReadoutChipInterface->ReadChipReg(chip, registerName); // just to read something
                    LOG(DEBUG) << BOLDMAGENTA << "CBC " << hybrid->getId() << " - " << registerName << " = " << registerValue << RESET;
                    theCBCRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() =
                        std::make_tuple(getTimeStamp(), registerValue);
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillCBCRegisterPlots(theCBCRegisterContainer, registerName);
#else
    // auto theCBCRegisterStreamer = prepareHybridContainerStreamer<EmptyContainer, std::tuple<time_t,uint16_t>, EmptyContainer>("CBCRegister");
    // theCBCRegisterStreamer.setHeaderElement(getTimeStamp());
    // if(fTheSystemController->fDQMStreamerEnabled)
    // {
    //     for(auto board: theCBCRegisterContainer) { theCBCRegisterStreamer.streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer); }
    // }
#endif
}

void CBCMonitor::runLpGBTRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<std::tuple<time_t, uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            uint16_t registerValue = static_cast<D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, registerName);
            LOG(INFO) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << registerName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<std::tuple<time_t, uint16_t>>() = std::make_tuple(getTimeStamp(), registerValue);
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotCBC->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    auto theLpGBTRegisterStreamer = prepareOpticalGroupContainerStreamer<EmptyContainer, EmptyContainer, EmptyContainer, std::tuple<time_t,uint16_t>, CharArray>("LpGBTRegister");
    theLpGBTRegisterStreamer.setHeaderElement(CharArray(registerName));
    if(fTheSystemController->fDQMStreamerEnabled)
    {
        for(auto board: theLpGBTRegisterContainer) theLpGBTRegisterStreamer.streamAndSendBoard(board, fTheSystemController->fMonitorDQMStreamer);
    }
#endif
}