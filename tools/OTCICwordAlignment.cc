#include "tools/OTCICwordAlignment.h"
#include "HWInterface/ExceptionHandler.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCICwordAlignment::fCalibrationDescription = "Insert brief calibration description here";

OTCICwordAlignment::OTCICwordAlignment() : Tool() {}

OTCICwordAlignment::~OTCICwordAlignment() {}

void OTCICwordAlignment::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "EXT_WA_DELAY[0-1][0-9]");

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCICwordAlignment.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCICwordAlignment::ConfigureCalibration() {}

void OTCICwordAlignment::Running()
{
    LOG(INFO) << "Starting OTCICwordAlignment measurement.";
    Initialise();
    WordAlignment();
    LOG(INFO) << "Done with OTCICwordAlignment.";
    Reset();
}

void OTCICwordAlignment::Stop(void)
{
    LOG(INFO) << "Stopping OTCICwordAlignment measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTCICwordAlignment.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCICwordAlignment stopped.";
}

void OTCICwordAlignment::Pause() {}

void OTCICwordAlignment::Resume() {}

void OTCICwordAlignment::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTCICwordAlignment::WordAlignment(uint32_t pWait_us)
{
    LOG(INFO) << BOLDBLUE << "Starting CIC automated word alignment procedure .... " << RESET;
    std::string theQueryFunction = "skipSSAQuery";
    auto        theSkipSSAquery  = [](const ChipContainer* theReadoutChip)
    {
        if(static_cast<const ReadoutChip*>(theReadoutChip)->getFrontEndType() == FrontEndType::SSA2) return false;
        return true;
    };
    fDetectorContainer->addReadoutChipQueryFunction(theSkipSSAquery, theQueryFunction);

    DetectorDataContainer theWordAlignmentDelayContainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<uint8_t, NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS - 1>>(*fDetectorContainer, theWordAlignmentDelayContainer);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;

                // configure word alignment pattern on CBCs
                std::vector<uint8_t> cAlignmentPatterns = fReadoutChipInterface->getWordAlignmentPatterns();
                for(auto cChip: *theHybrid) { fReadoutChipInterface->produceWordAlignmentPattern(cChip); }
                bool  cSuccessAlign          = fCicInterface->AutomatedWordAlignment(cCic, cAlignmentPatterns);
                auto& theWordAlignmentValues = theWordAlignmentDelayContainer.getObject(theBoard->getId())
                                                   ->getObject(theOpticalGroup->getId())
                                                   ->getObject(theHybrid->getId())
                                                   ->getSummary<GenericDataArray<uint8_t, NUMBER_OF_CIC_PORTS, NUMBER_OF_LINES_PER_CIC_PORTS - 1>>();
                theWordAlignmentValues = fCicInterface->retrieveExternalWordAlignmentValues(cCic);
                cSuccessAlign          = cSuccessAlign && fCicInterface->ConfigureExternalWordAlignment(cCic, theWordAlignmentValues);
                if(cSuccessAlign) { LOG(INFO) << BOLDBLUE << "Automated word alignment procedure " << BOLDGREEN << " SUCCEEDED!" << RESET; }
                else
                {
                    LOG(INFO) << BOLDRED << "Automated word alignment procedure " << BOLDRED << " FAILED!" << RESET;
                    LOG(INFO) << BOLDRED << "FAILED CIC word alignment word on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id"
                              << +theHybrid->getId() << " --- Hybrid will be disabled" << RESET;
                    ExceptionHandler::getInstance()->disableHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId());
                    continue;
                }
                fCicInterface->SetStaticWordAlignment(cCic);
            } // hybrid - configure word alignment patterns
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTCICwordAlignment.fillWordAlignmentDelay(theWordAlignmentDelayContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theWordAlignmentDelayContainerSerialization("OTCICwordAlignmentWordAlignmentDelay");
        theWordAlignmentDelayContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theWordAlignmentDelayContainer);
    }
#endif

    fDetectorContainer->removeReadoutChipQueryFunction(theQueryFunction);
}
