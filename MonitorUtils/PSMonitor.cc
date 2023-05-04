#include "MonitorUtils/PSMonitor.h"
#include "HWDescription/Definition.h"
#include "HWDescription/OuterTrackerHybrid.h"
#include "HWInterface/D19clpGBTInterface.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/Utilities.h"
#include "Utils/ValueAndTime.h"

#ifdef __USE_ROOT__
#include "TFile.h"
#endif

using namespace Ph2_HwInterface;

PSMonitor::PSMonitor(Ph2_System::SystemController* theSystemController, DetectorMonitorConfig theDetectorMonitorConfig) : DetectorMonitor(theSystemController, theDetectorMonitorConfig)
{
#ifdef __USE_ROOT__
    fMonitorPlotDQM   = new MonitorDQMPlotPS();
    fMonitorDQMPlotPS = static_cast<MonitorDQMPlotPS*>(fMonitorPlotDQM);
    fMonitorDQMPlotPS->book(fOutputFile, *fTheSystemController->fDetectorContainer, fDetectorMonitorConfig);
#endif
}

void PSMonitor::runMonitor()
{
    std::recursive_mutex                  theMutex;
    std::lock_guard<std::recursive_mutex> theGuard(theMutex);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("SSA2")) runSSA2RegisterMonitor(registerName);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("MPA2")) runMPA2RegisterMonitor(registerName);
    for(const auto& registerName: fDetectorMonitorConfig.fMonitorElementList.at("LpGBT")) runLpGBTRegisterMonitor(registerName);
}

void PSMonitor::runSSA2RegisterMonitor(std::string registerName)
{
    DetectorDataContainer theSSA2RegisterContainer;
    ContainerFactory::copyAndInitChip<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theSSA2RegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        auto     SSA2ReadoutChipInterface = fTheSystemController->fReadoutChipInterface;
                        uint16_t registerValue            = static_cast<SSA2Interface*>(static_cast<PSInterface*>(SSA2ReadoutChipInterface)->getInterface(chip))->ReadADC(chip, registerName);
                        LOG(DEBUG) << BOLDMAGENTA << "hybrid " << hybrid->getId() << " - chip " << chip->getId() << " " << registerName << " = " << registerValue << RESET;
                        ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
                        theSSA2RegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<ValueAndTime<uint16_t>>() =
                            theRegisterAndTime;
                    }
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotPS->fillSSA2RegisterPlots(theSSA2RegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSMonitorSSA2Register");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theSSA2RegisterContainer, registerName);
    }
#endif
}

void PSMonitor::runMPA2RegisterMonitor(std::string registerName)
{
    DetectorDataContainer theMPA2RegisterContainer;
    ContainerFactory::copyAndInitChip<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theMPA2RegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        for(const auto& opticalGroup: *board)
        {
            for(const auto& hybrid: *opticalGroup)
            {
                for(const auto& chip: *hybrid)
                {
                    if(chip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        auto     MPA2ReadoutChipInterface = fTheSystemController->fReadoutChipInterface;
                        uint16_t registerValue            = static_cast<MPA2Interface*>(static_cast<PSInterface*>(MPA2ReadoutChipInterface)->getInterface(chip))->ReadADC(chip, registerName);                        
                        LOG(DEBUG) << BOLDMAGENTA << "hybrid " << hybrid->getId() << " - chip " << chip->getId() << " " << registerName << " = " << registerValue << RESET;
                        ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
                        theMPA2RegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<ValueAndTime<uint16_t>>() =
                            theRegisterAndTime;
                    }
                }
            }
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotPS->fillMPA2RegisterPlots(theMPA2RegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSMonitorMPA2Register");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theMPA2RegisterContainer, registerName);
    }
#endif
}

void PSMonitor::runLpGBTRegisterMonitor(std::string registerName)
{
    DetectorDataContainer theLpGBTRegisterContainer;
    ContainerFactory::copyAndInitOpticalGroup<ValueAndTime<uint16_t>>(*fTheSystemController->fDetectorContainer, theLpGBTRegisterContainer);

    for(const auto& board: *fTheSystemController->fDetectorContainer)
    {
        if(board->at(0)->flpGBT == nullptr)
        {
            for(const auto& opticalGroup: *board)
            {
                ValueAndTime<uint16_t> theRegisterAndTime(0, getTimeStamp());
                theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
            }
            continue;
        }
        for(const auto& opticalGroup: *board)
        {
            uint16_t               registerValue = static_cast<D19clpGBTInterface*>(fTheSystemController->flpGBTInterface)->ReadADC(opticalGroup->flpGBT, registerName);
            ValueAndTime<uint16_t> theRegisterAndTime(registerValue, getTimeStamp());
            LOG(DEBUG) << BOLDMAGENTA << "LpGBT " << opticalGroup->getId() << " - " << registerName << " = " << registerValue << RESET;
            theLpGBTRegisterContainer.at(board->getIndex())->at(opticalGroup->getIndex())->getSummary<ValueAndTime<uint16_t>>() = theRegisterAndTime;
        }
    }

#ifdef __USE_ROOT__
    fMonitorDQMPlotPS->fillLpGBTRegisterPlots(theLpGBTRegisterContainer, registerName);
#else
    if(fTheSystemController->fMonitorDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSMonitorLpGBTRegister");
        theContainerSerialization.streamByBoardContainer(fTheSystemController->fMonitorDQMStreamer, theLpGBTRegisterContainer, registerName);
    }
#endif
}
