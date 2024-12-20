#include "tools/OTBitErrorRateTest.h"
#include "HWInterface/D19cBERTinterface.h"
#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTBitErrorRateTest::fCalibrationDescription = "Insert brief calibration description here";

OTBitErrorRateTest::OTBitErrorRateTest() : OTalignBoardDataWord() {}

OTBitErrorRateTest::~OTBitErrorRateTest() {}

void OTBitErrorRateTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    initializeContainers();
    fBroadcastAlignSetting = 2;

    fNumberOfFrames = findValueInSettings<double>("OTBitErrorRateTest_NumberOfFrames", 1E10);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTBitErrorRateTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTBitErrorRateTest::ConfigureCalibration() {}

void OTBitErrorRateTest::Running()
{
    LOG(INFO) << "Starting OTBitErrorRateTest measurement.";
    Initialise();
    bitErrorRateTest();
    // bitErrorRateTestOld();
    LOG(INFO) << "Done with OTBitErrorRateTest.";
    Reset();
}

void OTBitErrorRateTest::Stop(void)
{
    LOG(INFO) << "Stopping OTBitErrorRateTest measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTBitErrorRateTest.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTBitErrorRateTest stopped.";
}

void OTBitErrorRateTest::Pause() {}

void OTBitErrorRateTest::Resume() {}

void OTBitErrorRateTest::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTBitErrorRateTest::bitErrorRateTest()
{
    uint8_t numberOfLines = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS ? 7 : 6;

    DetectorDataContainer theBERTcounterCountainer;
    ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<uint64_t, 2>>>(*fDetectorContainer, theBERTcounterCountainer);

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            uint16_t iteration                 = 0;
            uint16_t maximumNumberOfIterations = 10;
            bool     allAligned                = false;
            while(iteration < maximumNumberOfIterations)
            {
                allAligned = static_cast<D19clpGBTInterface*>(flpGBTInterface)->enablePRBS(theOpticalGroup);
                if(allAligned) break;
                ++iteration;
                LOG(WARNING) << WARNING_FORMAT << "Failed to align LpGBT on Board " << theBoard->getId() << " OpticalGroup " << theOpticalGroup->getId() << ", retrying "
                             << maximumNumberOfIterations - iteration << " more times" << RESET;
            }

            if(!allAligned)
            {
                LOG(ERROR) << ERROR_FORMAT << "Failed to align LpGBT on Board " << theBoard->getId() << " OpticalGroup " << theOpticalGroup->getId() << " after " << maximumNumberOfIterations
                           << "trials. OpticalGroup will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
            }
        }

        D19cBackendAlignmentFWInterface* theAlignerInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBackendAlignmentInterface();
        theAlignerInterface->enableAlignmentOnPRBS();

        runAlignment(theBoard);

        theAlignerInterface->disableAlignmentOnPRBS();

        // std::cout << "L1    : " << getPatternPrintout(static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getFirstObject()))->L1ADebug(1, false), 1, true) <<
        // std::endl;

        // auto lineOutputVector = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getFirstObject()))->StubDebug(true, 6, false);
        // for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
        // {
        //     std::cout << "Stub " << lineIndex << ": " << getPatternPrintout(lineOutputVector.at(lineIndex), 1, true) << std::endl;
        // }

        D19cBERTinterface* theBERTinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBERTinterface();

        auto bertResultsBoardContainer = theBERTinterface->runBERTonAllHybdrids(theBoard, numberOfLines, flpGBTInterface->GetChipRate(theBoard->getFirstObject()->flpGBT) == 10, fNumberOfFrames);

        for(auto theOpticalGroup: bertResultsBoardContainer)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                const auto& receivedBERTresultsVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
                auto&       storedBERTresultsVector =
                    theBERTcounterCountainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
                storedBERTresultsVector.assign(receivedBERTresultsVector.begin(), receivedBERTresultsVector.end());
            }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTBitErrorRateTest.fillErrorCounter(theBERTcounterCountainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounter");
        theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBERTcounterCountainer);
    }
#endif
}
