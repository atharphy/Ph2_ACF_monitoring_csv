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
    Initialise();
    bitErrorRateTest();
    // for(size_t index = 0; index < 512; ++index)
    // {
    //     uint16_t phaseDelay = index;
    //     std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Phase delay " << +phaseDelay << std::endl;
    //     LOG(INFO) << "Starting OTBitErrorRateTest measurement.";

    //     // for(auto theBoard: *fDetectorContainer)
    //     // {
    //     //     // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getLinkInterface()->GeneralLinkReset(theBoard);

    //     //     // fBeBoardInterface->ConfigureBoard(theBoard);
    //     //     for(auto cOpticalGroup: *theBoard)
    //     //     {
    //     //         if(!flpGBTInterface->ConfigureChip(cOpticalGroup->flpGBT))
    //     //         {
    //     //             LOG(INFO) << BOLDRED << "SOMETHING FUNNY" << RESET;
    //     //             continue;
    //     //         }
    //     //     }
    //     // }
        
    //     bitErrorRateTest(phaseDelay);
    //     LOG(INFO) << "Done with OTBitErrorRateTest.";
    // }
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
    bool is10Gmodule = flpGBTInterface->GetChipRate(fDetectorContainer->getFirstObject()->getFirstObject()->flpGBT) == 10;
    uint16_t maximumPhase = is10Gmodule ? 32 : 64;

    std::map<uint16_t, DetectorDataContainer> thePhaseScanContainer;
    for(uint16_t phase = 0; phase < maximumPhase; ++phase)
    {
        ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<uint64_t, 2>>>(*fDetectorContainer, thePhaseScanContainer[phase]);
    }

    DetectorDataContainer theBERTcounterCountainer;
    ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<uint64_t, 2>>>(*fDetectorContainer, theBERTcounterCountainer);

    DetectorDataContainer theFECcounterCountainer;
    ContainerFactory::copyAndInitOpticalGroup<uint32_t>(*fDetectorContainer, theFECcounterCountainer);

    DetectorDataContainer theBestPhaseCountainer;
    ContainerFactory::copyAndInitOpticalGroup<uint16_t>(*fDetectorContainer, theBestPhaseCountainer);
    
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            LOG(INFO) << BOLDBLUE << "Running BERT on OpticalGroup " << theOpticalGroup->getId() << RESET;
            std::map<uint16_t, float> cumulativeErrorRateMap;
            std::map<uint16_t, float> cumulativeBitCountMap;
            for(auto phase = 0; phase < maximumPhase; ++phase)
            {
                cumulativeErrorRateMap[phase] = 0;
                cumulativeBitCountMap[phase] = 0;
                bitErrorRateTestPerOpticalGroup(theOpticalGroup, thePhaseScanContainer[phase].getOpticalGroup(theBoard->getId(), theOpticalGroup->getId()), nullptr, phase, 1e6);
                for(auto theHybrid: *thePhaseScanContainer[phase].getOpticalGroup(theBoard->getId(), theOpticalGroup->getId()))
                {
                    for(auto theBERTValues: theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>())
                    {
                        cumulativeBitCountMap[phase] += theBERTValues.at(0);
                        cumulativeErrorRateMap[phase] += theBERTValues.at(1);
                    }
                }
            }

            // find mimumum BERT
            float theMinimum = 1.;
            for(auto& cumulativeErrorRate : cumulativeErrorRateMap)
            {
                auto cumulativeBitCount = cumulativeBitCountMap[cumulativeErrorRate.first];
                cumulativeErrorRate.second = cumulativeBitCount == 0 ? 1. : cumulativeErrorRate.second / cumulativeBitCount;

                if(cumulativeErrorRate.second < theMinimum) theMinimum = cumulativeErrorRate.second;
            }

            // find minimum sequences
            std::vector<std::pair<uint16_t, uint16_t>> minimumPhaseRanges;
            bool minimumFound = false;
            for(auto& cumulativeErrorRate : cumulativeErrorRateMap)
            {
                if(cumulativeErrorRate.second == theMinimum)
                {
                    if(!minimumFound)
                    {
                        minimumFound = true;
                        minimumPhaseRanges.push_back({cumulativeErrorRate.first, cumulativeErrorRate.first});
                    }
                    else
                    {
                        minimumPhaseRanges.back().second = cumulativeErrorRate.first;
                    }
                }
                else minimumFound = false;
            }

            // find longest minimum sequence;
            uint16_t longestSequenceRange = 0;
            uint16_t longestSequenceIndex = 0;
            for(size_t index = 0; index < minimumPhaseRanges.size(); ++index)
            {
                uint16_t sequenceRange = minimumPhaseRanges.at(index).second - minimumPhaseRanges.at(index).first;
                if(sequenceRange > longestSequenceRange)
                {
                    longestSequenceRange = sequenceRange;
                    longestSequenceIndex = index;
                }
            }

            uint16_t bestPhase =  minimumPhaseRanges.at(longestSequenceIndex).first + longestSequenceRange/2;

            LOG(INFO) << BOLDBLUE << "Best Phase for BERT on OpticalGroup " << theOpticalGroup->getId() << " = " << bestPhase << RESET;

            theBestPhaseCountainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<uint16_t>() = bestPhase;

            bitErrorRateTestPerOpticalGroup(theOpticalGroup, theBERTcounterCountainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId()), theFECcounterCountainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId()), bestPhase, fNumberOfBits);
        }
    }


#ifdef __USE_ROOT__
    for(uint16_t phase = 0; phase < maximumPhase; ++phase)
    {
        fDQMHistogramOTBitErrorRateTest.fillErrorCounterPhaseScan(thePhaseScanContainer[phase], phase);
    }
    fDQMHistogramOTBitErrorRateTest.fillBERTbestPhase(theBestPhaseCountainer);
    fDQMHistogramOTBitErrorRateTest.fillErrorCounter(theBERTcounterCountainer);
    fDQMHistogramOTBitErrorRateTest.fillFECcounter(theFECcounterCountainer);
#else
    if(fDQMStreamerEnabled)
    {
        for(uint16_t phase = 0; phase < maximumPhase; ++phase)
        {
            ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounterPhaseScan");
            theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, thePhaseScanContainer[phase], phase);
        }

        ContainerSerialization theBestPhaseSerialization("OTBitErrorRateTestBestPhase");
        theBestPhaseSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBestPhaseCountainer);

        ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounter");
        theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBERTcounterCountainer);

        ContainerSerialization theFECcounterSerialization("OTBitErrorRateTestFECcounter");
        theFECcounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theFECcounterCountainer);
    }
#endif
}

void OTBitErrorRateTest::bitErrorRateTestPerOpticalGroup(OpticalGroup* theOpticalGroup, OpticalGroupDataContainer* theBertContainer,  OpticalGroupDataContainer* theFECContainer, uint16_t phaseClockDelay, float numberOfBits)
{
    bool is10Gmodule = flpGBTInterface->GetChipRate(theOpticalGroup->flpGBT) == 10;
    uint8_t numberOfLines = theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS ? 7 : 6;
    auto theBoard = fDetectorContainer->getObject(theOpticalGroup->getBeBoardId());

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.physical_interface_block.control.bert_link_select", theOpticalGroup->getId());

    uint16_t iteration                 = 0;
    uint16_t maximumNumberOfIterations = 10;
    bool     allAligned                = false;
    while(iteration < maximumNumberOfIterations)
    {
        allAligned = static_cast<D19clpGBTInterface*>(flpGBTInterface)->enablePRBS(theOpticalGroup, phaseClockDelay);
        if(allAligned) break;
        ++iteration;
        LOG(WARNING) << WARNING_FORMAT << "Failed to align LpGBT on Board " << theOpticalGroup->getBeBoardId() << " OpticalGroup " << theOpticalGroup->getId() << ", retrying "
                        << maximumNumberOfIterations - iteration << " more times" << RESET;
    }

    if(!allAligned)
    {
        LOG(ERROR) << ERROR_FORMAT << "Failed to align LpGBT on Board " << theOpticalGroup->getBeBoardId() << " OpticalGroup " << theOpticalGroup->getId() << " after " << maximumNumberOfIterations
                    << "trials. OpticalGroup will be disabled" << RESET;
        ExceptionHandler::getInstance()->disableOpticalGroup(theOpticalGroup->getBeBoardId(), theOpticalGroup->getId());
    }

    D19cBackendAlignmentFWInterface* theAlignerInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBackendAlignmentInterface();
    theAlignerInterface->enableAlignmentOnPRBS();

    opticalGroupWordAlignment(theOpticalGroup, theAlignerInterface);

    theAlignerInterface->disableAlignmentOnPRBS();

    D19cBERTinterface* theBERTinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBERTinterface();

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_en_bit", 1);
    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 1);
    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 0);

    OpticalGroupDataContainer bertResultsOpticalGroupContainer = theBERTinterface->runBERTonAllLines(theOpticalGroup, numberOfLines, is10Gmodule, numberOfBits);

    for(auto theHybrid: bertResultsOpticalGroupContainer)
    {
        const auto& receivedBERTresultsVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
        auto&       storedBERTresultsVector = theBertContainer->getHybrid(theHybrid->getId())->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
        storedBERTresultsVector.assign(receivedBERTresultsVector.begin(), receivedBERTresultsVector.end());
    }
    if(theFECContainer != nullptr)
    {
        fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_sel_offset", theOpticalGroup->getId());
        auto theFECcounter = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.lpgbt_fec_counter");
        theFECContainer->getSummary<uint32_t>() = theFECcounter;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] FECcounter = 0x" << std::hex << +theFECcounter << std::dec << std::endl;
    }
}

// void OTBitErrorRateTest::bitErrorRateTest(uint16_t phaseClockDelay)
// {
//     uint8_t numberOfLines = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS ? 7 : 6;

//     DetectorDataContainer theBERTcounterCountainer;
//     ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<uint64_t, 2>>>(*fDetectorContainer, theBERTcounterCountainer);

//     DetectorDataContainer theFECcounterCountainer;
//     ContainerFactory::copyAndInitOpticalGroup<uint32_t>(*fDetectorContainer, theFECcounterCountainer);

//     for(auto theBoard: *fDetectorContainer)
//     {
//         for(auto theOpticalGroup: *theBoard)
//         {
//             uint16_t iteration                 = 0;
//             uint16_t maximumNumberOfIterations = 10;
//             bool     allAligned                = false;
//             while(iteration < maximumNumberOfIterations)
//             {
//                 allAligned = static_cast<D19clpGBTInterface*>(flpGBTInterface)->enablePRBS(theOpticalGroup, phaseClockDelay);
//                 if(allAligned) break;
//                 ++iteration;
//                 LOG(WARNING) << WARNING_FORMAT << "Failed to align LpGBT on Board " << theBoard->getId() << " OpticalGroup " << theOpticalGroup->getId() << ", retrying "
//                              << maximumNumberOfIterations - iteration << " more times" << RESET;
//             }

//             if(!allAligned)
//             {
//                 LOG(ERROR) << ERROR_FORMAT << "Failed to align LpGBT on Board " << theBoard->getId() << " OpticalGroup " << theOpticalGroup->getId() << " after " << maximumNumberOfIterations
//                            << "trials. OpticalGroup will be disabled" << RESET;
//                 ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
//             }
//         }
//         // fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_ctrl.physical_interface_block.control.bert_link_select", fDetectorContainer->getFirstObject()->getFirstObject()->getId());

//         D19cBackendAlignmentFWInterface* theAlignerInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBackendAlignmentInterface();
//         theAlignerInterface->enableAlignmentOnPRBS();

//         runAlignment(theBoard);

//         theAlignerInterface->disableAlignmentOnPRBS();

//         D19cBERTinterface* theBERTinterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard))->getBERTinterface();

//         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_en_bit", 1);
//         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 1);
//         fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_rst_bit", 0);

//         auto bertResultsBoardContainer = theBERTinterface->runBERTonAllOpticalGroups(theBoard, numberOfLines, flpGBTInterface->GetChipRate(theBoard->getFirstObject()->flpGBT) == 10, fNumberOfBits);

//         for(auto theOpticalGroup: bertResultsBoardContainer)
//         {
//             for(auto theHybrid: *theOpticalGroup)
//             {
//                 const auto& receivedBERTresultsVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
//                 auto&       storedBERTresultsVector =
//                     theBERTcounterCountainer.getHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId())->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
//                 storedBERTresultsVector.assign(receivedBERTresultsVector.begin(), receivedBERTresultsVector.end());
//             }
//             fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.physical_interface_block.lpgbt_fec_config.fec_err_cnt_sel_offset", theOpticalGroup->getId());
//             auto theFECcounter = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.lpgbt_fec_counter");
//             theFECcounterCountainer.getOpticalGroup(theBoard->getId(), theOpticalGroup->getId())->getSummary<uint32_t>() = theFECcounter;

//             std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] FECcounter = 0x" << std::hex << +theFECcounter << std::dec << std::endl;
//         }
//     }

// #ifdef __USE_ROOT__
//     fDQMHistogramOTBitErrorRateTest.fillErrorCounterPhaseScan(theBERTcounterCountainer, phaseClockDelay);
//     fDQMHistogramOTBitErrorRateTest.fillFECcounter(theFECcounterCountainer);
// #else
//     if(fDQMStreamerEnabled)
//     {
//         ContainerSerialization theErrorCounterSerialization("OTBitErrorRateTestErrorCounter");
//         theErrorCounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBERTcounterCountainer);

//         ContainerSerialization theFECcounterSerialization("OTBitErrorRateTestFECcounter");
//         theFECcounterSerialization.streamByOpticalGroupContainer(fDQMStreamer, theFECcounterCountainer);
//     }
// #endif
// }
