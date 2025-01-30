#include "tools/OTPatternCheckerTest.h"
#include "HWInterface/D19cBERTinterface.h"
#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTPatternCheckerTest::fCalibrationDescription = "Pattern checker test";

OTPatternCheckerTest::OTPatternCheckerTest() : OTalignBoardDataWord() {}

OTPatternCheckerTest::~OTPatternCheckerTest() {}

void OTPatternCheckerTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    initializeContainers();
    fBroadcastAlignSetting = 1;

    fNumberOfBits = findValueInSettings<double>("OTPatternCheckerTest_NumberOfBits", 1E10);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTPatternCheckerTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTPatternCheckerTest::ConfigureCalibration() {}

void OTPatternCheckerTest::Running()
{
    LOG(INFO) << "Starting OTPatternCheckerTest measurement.";
    Initialise();
    PatternCheckerTest();
    LOG(INFO) << "Done with OTPatternCheckerTest.";
    Reset();
}

void OTPatternCheckerTest::Stop(void)
{
    LOG(INFO) << "Stopping OTPatternCheckerTest measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTPatternCheckerTest.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTPatternCheckerTest stopped.";
}

void OTPatternCheckerTest::Pause() {}

void OTPatternCheckerTest::Resume() {}

void OTPatternCheckerTest::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTPatternCheckerTest::PatternCheckerTest(uint8_t line)
{
    LOG(INFO) << BOLDBLUE << "Running Pattern Checker on line " << +line << RESET;

    std::vector<uint32_t> pattern{0xeaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa, 0xaaaaaaaa};
    std::vector<uint32_t> patternMask{0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

    DetectorDataContainer theBERTcounterCountainer;
    ContainerFactory::copyAndInitHybrid<GenericDataArray<uint64_t, 2>>(*fDetectorContainer, theBERTcounterCountainer);

    bool is10Gmodule = flpGBTInterface->GetChipRate(fDetectorContainer->getFirstObject()->getFirstObject()->flpGBT) == 10;

    for(auto theBoard: *fDetectorContainer)
    {
        D19cBackendAlignmentFWInterface* theAlignerInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBackendAlignmentInterface();
        theAlignerInterface->enableAlignmentOnCustomPattern(0xeaaa, 0xffff);

        for(auto theOpticalGroup: *theBoard)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                if(!tryLineAlignment(theAlignerInterface, theHybrid, line))
                {
                    LOG(ERROR) << ERROR_FORMAT << "Failed to align OpticalGroup " << theOpticalGroup->getId() << " Hybrid " << theHybrid->getId() << " line " << +line << RESET;
                }
            }
        }

        theAlignerInterface->disableAlignmentOnCustomPattern();
        D19cBERTinterface* theBERTinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBERTinterface();

        theBERTinterface->setUsePRBS(false);
        theBERTinterface->setCheckedPattern(pattern);
        theBERTinterface->setCheckedPatternMask(patternMask);

        BoardDataContainer bertResultsBoardContainer = theBERTinterface->runBERTonSingleLine(theBoard, line, is10Gmodule, fNumberOfBits);

        for(auto theOpticalGroup: bertResultsBoardContainer)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                const auto& receivedBERTresultsVector = theHybrid->getSummary<GenericDataArray<uint64_t, 2>>();
                theBERTcounterCountainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<GenericDataArray<uint64_t, 2>>() = receivedBERTresultsVector;
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTPatternCheckerTest.fillErrorCounter(theBERTcounterCountainer, line);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theErrorCounterSerialization("OTPatternCheckerTestErrorCounter");
        theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBERTcounterCountainer, line);
    }
#endif
}

void OTPatternCheckerTest::PatternCheckerTest()
{
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            LOG(INFO) << BOLDMAGENTA << "    Optical Group " << +theOpticalGroup->getId() << RESET;
            for(auto theHybrid: *theOpticalGroup)
            {
                LOG(INFO) << BOLDMAGENTA << "        Hybrid " << +theHybrid->getId() << RESET;
                auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                fCicInterface->SelectOutput(cCic, true);
            }
        }
    }

    uint8_t numberOfLines = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS ? 7 : 6;
    for(uint8_t line = 1; line < numberOfLines; ++line) { PatternCheckerTest(line); }
}
