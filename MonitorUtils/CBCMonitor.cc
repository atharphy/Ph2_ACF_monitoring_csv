#include "CBCMonitor.h"
#include "../HWDescription/OuterTrackerHybrid.h"
#include "../Utils/ContainerFactory.h"
#ifdef __USE_ROOT__
#include "TFile.h"
#endif

CBCMonitor::CBCMonitor(const Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
    fDoMonitorThreshold = fDetectorMonitorConfig.isElementToMonitor("CBCThreshold");

#ifdef __USE_ROOT__
    fMonitorPlotDQM.book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void CBCMonitor::runMonitor()
{
    if(fDoMonitorThreshold) runThresholdMonitor();
}

void CBCMonitor::runThresholdMonitor()
{
    DetectorDataContainer theThresholdContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fTheSystemController->fDetectorContainer, theThresholdContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    uint16_t VCth = fTheSystemController->fReadoutChipInterface->ReadChipReg(chip, "VCth"); // just to read something
                    LOG(INFO) << BOLDMAGENTA << "CBC " << hybrid->getId() << " - Threshold = " << VCth << RESET;
                    theThresholdContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint16_t>() = VCth;
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorPlotDQM.fillDQMThresholdPlots(theThresholdContainer, getTimeStamp());
#else
    // auto theCBCThresholdStreamer = prepareHybridContainerStreamer<EmptyContainer, uint16_t, EmptyContainer, time_t>("CBCThreshold");
    // theCBCThresholdStreamer.setHeaderElement(getTimeStamp());
    // if(fStreamerEnabled)
    // {
    //     for(auto board: theThresholdContainer) { theCBCThresholdStreamer.streamAndSendBoard(board, fNetworkStreamer); }
    // }
#endif
}
