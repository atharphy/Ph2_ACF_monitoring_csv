#include "tools/OTBitErrorRateTest.h"
#include "HWInterface/D19cBERTinterface.h"
#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cLinkInterface.h"
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

    fNumberOfBits = findValueInSettings<double>("OTBitErrorRateTest_NumberOfBits", 1E10);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTBitErrorRateTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTBitErrorRateTest::ConfigureCalibration() {}

void OTBitErrorRateTest::Running()
{
    for(size_t index = 0; index < 20; ++index)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Iteration " << index << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "]!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << std::endl;
        LOG(INFO) << "Starting OTBitErrorRateTest measurement.";

        for(auto theBoard: *fDetectorContainer)
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getLinkInterface()->GeneralLinkReset(theBoard);

            // fBeBoardInterface->ConfigureBoard(theBoard);
            for(auto cOpticalGroup: *theBoard)
            {
                if(!flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT))
                {
                    LOG(INFO) << BOLDRED << "SOMETHING FUNNY" << RESET;
                    continue;
                }
            }
        }
        Initialise();
        
        bitErrorRateTest();
        LOG(INFO) << "Done with OTBitErrorRateTest.";
        Reset();
    }
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

    DetectorDataContainer theFECcounterCountainer;
    ContainerFactory::copyAndInitOpticalGroup<uint32_t>(*fDetectorContainer, theFECcounterCountainer);

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

        D19cBERTinterface* theBERTinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBERTinterface();

        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_en_bit", 1);
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 1);
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 0);

        auto bertResultsBoardContainer = theBERTinterface->runBERTonAllHybdrids(theBoard, numberOfLines, flpGBTInterface->GetChipRate(theBoard->getFirstObject()->flpGBT) == 10, fNumberOfBits);

        for(auto theOpticalGroup: bertResultsBoardContainer)
        {
            fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_err_bit", theOpticalGroup->getId());
            for(auto theHybrid: *theOpticalGroup)
            {
                const auto& receivedBERTresultsVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
                auto&       storedBERTresultsVector =
                    theBERTcounterCountainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
                storedBERTresultsVector.assign(receivedBERTresultsVector.begin(), receivedBERTresultsVector.end());
            }
            auto theFECcounter = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.lpgbt_fec_counter");
            theFECcounterCountainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<uint32_t>() = theFECcounter;

            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Writing theFECcounter = 0x" << std::hex << +theFECcounter << std::dec << std::endl;
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTBitErrorRateTest.fillErrorCounter(theBERTcounterCountainer);
    fDQMHistogramOTBitErrorRateTest.fillFECcounter(theFECcounterCountainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounter");
        theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBERTcounterCountainer);

        ContainerSerialization theFECcounterSerialization("OTBitErrorRateTestFECcounter");
        theFECcounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theFECcounterCountainer);
    }
#endif
}
