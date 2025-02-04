#include "tools/OTCICtoLpGBTecv.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/GenericDataArray.h"
#include <algorithm>
#include <unordered_set>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCICtoLpGBTecv::fCalibrationDescription = "Performs the Electric Chain Validation between the lpGBT and the CIC using the algorithms implemented in OTverifyBoardDataWord.";

OTCICtoLpGBTecv::OTCICtoLpGBTecv() : OTverifyBoardDataWord() {}

OTCICtoLpGBTecv::~OTCICtoLpGBTecv() {}

void OTCICtoLpGBTecv::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any
    fNumberOfL1Bits      = findValueInSettings<double>("OTCICtoLpGBTecv_NumberOfL1Bits", 32000);
    fNumberOfStubBits    = findValueInSettings<double>("OTCICtoLpGBTecv_NumberOfStubBits", 32000);
    fListOfLpGBTPhase    = convertStringToFloatList(findValueInSettings<std::string>("OTCICtoLpGBTecv_LpGBTPhase", "0-14"));
    fListOfCICStrength   = convertStringToFloatList(findValueInSettings<std::string>("OTCICtoLpGBTecv_CICStrength", "1, 3, 5"));
    fListOfClockPolarity = convertStringToFloatList(findValueInSettings<std::string>("OTCICtoLpGBTecv_ClockPolarity", "0-1"));
    fListOfClockStrength = convertStringToFloatList(findValueInSettings<std::string>("OTCICtoLpGBTecv_ClockStrength", "1, 4, 7"));

    // Error handle for incorrect LpGBTPhase input
    std::unordered_set<float> allowedLpGBTPhases{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
    std::unordered_set<float> phaseValues;
    for(float phase: fListOfLpGBTPhase)
    {
        if(phaseValues.find(phase) != phaseValues.end())
        {
            LOG(ERROR) << BOLDRED << "Error, repeat detected in OTCICtoLpGBTecv_LpGBTPhase parameter." << std::endl;
            throw Exception("Repeat detected in OTCICtoLpGBTecv_LpGBTPhase parameter");
        }
        if(allowedLpGBTPhases.find(phase) == allowedLpGBTPhases.end())
        {
            LOG(ERROR) << BOLDRED << "Error, " << phase
                       << " is not an allowed value for the OTCICtoLpGBTecv_LpGBTPhase parameter. The allowed values are: 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, and 14." << std::endl;
            throw Exception("Unallowed value for the OTCICtoLpGBTecv_LpGBTPhase parameter");
        }
        phaseValues.insert(phase);
    }

    // Error handle for incorrect CICStrength input
    std::unordered_set<float> allowedCICStrengths{1, 2, 3, 4, 5};
    std::unordered_set<float> cicStrengthValues;
    for(float cicStrength: fListOfCICStrength)
    {
        if(cicStrengthValues.find(cicStrength) != cicStrengthValues.end())
        {
            LOG(ERROR) << BOLDRED << "Error, repeat detected in OTCICtoLpGBTecv_CICStrength parameter." << std::endl;
            throw Exception("Repeat detected in OTCICtoLpGBTecv_CICStrength parameter");
        }
        if(allowedCICStrengths.find(cicStrength) == allowedCICStrengths.end())
        {
            LOG(ERROR) << BOLDRED << "Error, " << cicStrength << " is not an allowed value for the OTCICtoLpGBTecv_CICStrength parameter. The allowed values are: 1, 2, 3, 4, and 5." << std::endl;
            throw Exception("Unallowed value for the OTCICtoLpGBTecv_CICStrength parameter");
        }
        cicStrengthValues.insert(cicStrength);
    }

    // Error handle for incorrect ClockPolarity input
    std::unordered_set<float> allowedClockPolarities{0, 1};
    std::unordered_set<float> clockPolarityValues;
    for(float clockPolarity: fListOfClockPolarity)
    {
        if(clockPolarityValues.find(clockPolarity) != clockPolarityValues.end())
        {
            LOG(ERROR) << BOLDRED << "Error, repeat detected in OTCICtoLpGBTecv_ClockPolarity parameter." << std::endl;
            throw Exception("Repeat detected in OTCICtoLpGBTecv_ClockPolarity parameter");
        }
        if(allowedClockPolarities.find(clockPolarity) == allowedClockPolarities.end())
        {
            LOG(ERROR) << BOLDRED << "Error, " << clockPolarity << " is not an allowed value for the OTCICtoLpGBTecv_ClockPolarity parameter. The allowed values are 0 and 1." << std::endl;
            throw Exception("Unallowed value for the OTCICtoLpGBTecv_ClockPolarity parameter");
        }
        clockPolarityValues.insert(clockPolarity);
    }

    // Error handle for incorrect ClockStrength input
    std::unordered_set<float> allowedClockStrengths{1, 2, 3, 4, 5, 6, 7};
    std::unordered_set<float> clockStrengthValues;
    for(float clockStrength: fListOfClockStrength)
    {
        if(clockStrengthValues.find(clockStrength) != clockStrengthValues.end())
        {
            LOG(ERROR) << BOLDRED << "Error, repeat detected in OTCICtoLpGBTecv_ClockStrength parameter." << std::endl;
            throw Exception("Repeat detected in OTCICtoLpGBTecv_ClockStrength parameter");
        }
        if(allowedClockStrengths.find(clockStrength) == allowedClockStrengths.end())
        {
            LOG(ERROR) << BOLDRED << "Error, " << clockStrength << " is not an allowed value for the OTCICtoLpGBTecv_ClockStrength parameter. The allowed values are: 1, 2, 3, 4, 5, 6, and 7."
                       << std::endl;
            throw Exception("Unallowed value for the OTCICtoLpGBTecv_ClockStrength parameter");
        }
        clockStrengthValues.insert(clockStrength);
    }

    size_t             numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    GenericDataArray<float, 2> theInitialBitAndError;
    theInitialBitAndError.at(0) = 0.;
    theInitialBitAndError.at(1) = 0.;
    std::vector<GenericDataArray<float, 2>> theInitialVector(numberOfLines, theInitialBitAndError);
    ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<float, 2>>>(*fDetectorContainer, fPatternMatchingBitErrorContainer, theInitialVector);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCICtoLpGBTecv.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCICtoLpGBTecv::ConfigureCalibration() {}

void OTCICtoLpGBTecv::Running()
{
    LOG(INFO) << BOLDMAGENTA << "Starting OTCICtoLpGBTecv measurement." << RESET;
    Initialise();
    runECV();
    LOG(INFO) << BOLDGREEN << "Done with OTCICtoLpGBTecv." << RESET;
    Reset();
}

void OTCICtoLpGBTecv::runECV()
{
    LOG(INFO) << BOLDYELLOW << "OTCICtoLpGBTecv::runIntegrityTest ... start integrity test" << RESET;
    size_t numberOfStubIterations = fNumberOfStubBits/32;
    size_t numberOfL1Iterations = fNumberOfL1Bits/32;
    float numberOfMatchedBits = std::bitset<32>(fHeaderMask).count();

    for(auto theBoard: *fDetectorContainer)
    {
        auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
        for(auto theOpticalGroup: *theBoard)
        {
            for(uint8_t clockPolarity: fListOfClockPolarity)
            {
                LOG(INFO) << BOLDMAGENTA << "CLOCK POLARITY: " << +clockPolarity << RESET;
                for(uint8_t clockStrength: fListOfClockStrength)
                {
                    LOG(INFO) << BOLDMAGENTA << "    CLOCK STRENGTH: " << +clockStrength << RESET;
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->setCICClockPolarityAndStrength(theOpticalGroup->flpGBT, clockPolarity, clockStrength, theOpticalGroup);

                    uint8_t numberOfBytesInSinglePacket = getNumberOfBytesInSinglePacket(theOpticalGroup);
                    if(fIsKickoff && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
                        LOG(INFO) << BOLDYELLOW << "Attention! ignoring failures on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                    size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;

                    for(uint8_t cicStrength: fListOfCICStrength)
                    {
                        LOG(INFO) << BOLDMAGENTA << "        CIC STRENGTH: " << +cicStrength << RESET;
                        for(auto cHybrid: *theOpticalGroup)
                        {
                            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                            fCicInterface->ConfigureDriveStrength(cCic, cicStrength);
                        }

                        std::string printout = "            RX PHASE: ";
                        int         i        = 0;
                        for(auto phase: fListOfLpGBTPhase)
                        {
                            printout += (" " + std::to_string(+static_cast<int>(phase)));
                            if(i > 0) std::cout << "\x1b[A";
                            i++;
                            LOG(INFO) << BOLDMAGENTA << printout << RESET;
                            std::map<uint8_t, std::vector<uint8_t>> theGroupsAndChannels = theOpticalGroup->getLpGBTrxGroupsAndChannels();
                            auto&                                   clpGBT               = theOpticalGroup->flpGBT;
                            flpGBTInterface->ConfigureAllRxPhase(clpGBT, phase, theGroupsAndChannels);

                            for(auto theHybrid: *theOpticalGroup)
                            {
                                auto& theHybridPatternMatchingEfficiency = fPatternMatchingBitErrorContainer.getObject(theBoard->getId())
                                                                               ->getObject(theOpticalGroup->getId())
                                                                               ->getObject(theHybrid->getId())
                                                                               ->getSummary<std::vector<GenericDataArray<float, 2>>>();
                                // LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest on Hybrid " << +theHybrid->getId() << RESET;
                                prepareHybridForStubIntegrityTest(theHybrid);

                                for(size_t iteration = 0; iteration < numberOfStubIterations; iteration++)
                                {
                                    auto lineOutputVector = theFWInterface->StubDebug(true, cNlines, false);
                                    for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
                                    {
                                        for(auto pattern: stubPatterns)
                                        {
                                            size_t wordBitSize = lineOutputVector.at(lineIndex).size() * 32;
                                            uint8_t flagCharacter = pattern.first;
                                            uint8_t idleCharacter = pattern.second;
                                            theHybridPatternMatchingEfficiency.at(lineIndex + 1).at(0) += wordBitSize;
                                            if(!isStubPatternMatched(lineOutputVector.at(lineIndex), numberOfBytesInSinglePacket, flagCharacter, idleCharacter))
                                            {
                                                theHybridPatternMatchingEfficiency.at(lineIndex + 1).at(1) += wordBitSize;
                                                if(!(fIsKickoff && ((theHybrid->getId() % 2) == 0) && ((lineIndex) == 4) && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)))
                                                    LOG(DEBUG) << BOLDRED << "Error on stub line " << lineIndex + 1 << " occurred in iteration number " << +iteration << RESET;
                                            }
                                        }
                                    }
                                }

                                // LOG(INFO) << BOLDMAGENTA << "Running L1StubIntegrityTest on Hybrid " << +theHybrid->getId() << RESET;
                                uint32_t theTriggerFrequency = 10; // higher rate reduces L1 efficiency
                                prepareFWForL1IntegrityTest(theBoard, theTriggerFrequency);
                                prepareHybridForL1IntegrityTest(theHybrid);

                                LOG(DEBUG) << BOLDBLUE << "D19cDebugFWInterface::L1ADebug ...." << RESET;

                                // Procedure to start the L1 triggers, outside the iteration loop to speed up the procedure
                                // enable initial fast reset
                                theFWInterface->WriteReg("fc7_daq_cnfg.fast_command_block.misc.initial_fast_reset_enable", 1);
                                // disable back-pressure
                                theFWInterface->WriteReg("fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0);
                                theFWInterface->WriteReg("fc7_daq_ctrl.fast_command_block.control.stop_trigger", 0x1);
                                // reset trigger
                                theFWInterface->WriteReg("fc7_daq_ctrl.fast_command_block.control.reset", 0x1);
                                // load new trigger configuration
                                theFWInterface->WriteReg("fc7_daq_ctrl.fast_command_block.control.load_config", 0x1);
                                theFWInterface->WriteReg("fc7_daq_ctrl.fast_command_block.control.start_trigger", 0x1);
                                LOG(DEBUG) << BOLDBLUE << "Started triggers ...." << RESET;
                                uint8_t  pWait_ms             = 1;
                                uint32_t previousNTriggersRxd = 0;
                                for(size_t iteration = 0; iteration < numberOfL1Iterations; iteration++)
                                {
                                    auto cNTriggersRxd = theFWInterface->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                                    auto cStartTime = std::chrono::high_resolution_clock::now(), cEndTime = cStartTime;
                                    auto cDuration = std::chrono::duration_cast<std::chrono::microseconds>(cEndTime - cStartTime).count();
                                    do {
                                        cEndTime      = std::chrono::high_resolution_clock::now();
                                        cDuration     = std::chrono::duration_cast<std::chrono::microseconds>(cEndTime - cStartTime).count();
                                        cNTriggersRxd = theFWInterface->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
                                        LOG(DEBUG) << BOLDMAGENTA << "Previous trigger " << previousNTriggersRxd << " Trigger in counter is " << cNTriggersRxd << " waited for " << cDuration
                                                   << " us so far" << RESET;
                                    } while((previousNTriggersRxd == cNTriggersRxd) && cDuration < pWait_ms * 1e3);
                                    previousNTriggersRxd = cNTriggersRxd;
                                    // LOG(DEBUG) << BOLDMAGENTA << "First header found after " << theFWInterface->ReadReg("fc7_daq_stat.physical_interface_block.slvs_debug.first_header_delay") << "
                                    // clock cycles." << RESET;
                                    auto lineOutputVector = theFWInterface->ReadBlockReg("fc7_daq_stat.physical_interface_block.l1a_debug", 50);
                                    LOG(DEBUG) << BOLDBLUE << getPatternPrintout(lineOutputVector, numberOfBytesInSinglePacket, true) << RESET;

                                    for(auto pattern: L1Patterns)
                                    {
                                        uint32_t header = pattern;

                                        theHybridPatternMatchingEfficiency.at(0).at(0) += numberOfMatchedBits;
                                        if(!isL1HeaderFound(lineOutputVector, numberOfBytesInSinglePacket, header, fHeaderMask))
                                        {
                                            theHybridPatternMatchingEfficiency.at(0).at(1) += numberOfMatchedBits;
                                            LOG(DEBUG) << BOLDRED << "Error occurred in iteration number " << +iteration << RESET;
                                        }
                                    }
                                }
                                // stop triggers
                            } // hybrid loop
                            auto    phaseIterator = std::find(fListOfLpGBTPhase.begin(), fListOfLpGBTPhase.end(), phase);
                            uint8_t phaseIndex    = std::distance(fListOfLpGBTPhase.begin(), phaseIterator) + 1;
#ifdef __USE_ROOT__
                            // Find the pClockStrength and pPhase indices

                            fDQMHistogramOTCICtoLpGBTecv.fillEfficiency(clockPolarity, clockStrength, cicStrength, phaseIndex, fPatternMatchingBitErrorContainer);
#else
                            if(fDQMStreamerEnabled)
                            {
                                // Find the pClockStrength and pPhase indices
                                ContainerSerialization theECVlpGBTCICContainerSerialization("OTCICtoLpGBTecvEfficiencyHistogram");
                                theECVlpGBTCICContainerSerialization.streamByOpticalGroupContainer(
                                    fDQMStreamer, fPatternMatchingBitErrorContainer, clockPolarity, clockStrength, cicStrength, phaseIndex);
                            }
#endif

                            // reset the number of matches!!
                            for(auto theHybrid: *theOpticalGroup)
                            {
                                for(auto& theNumberOfBitsAndErrors: fPatternMatchingBitErrorContainer.getObject(theBoard->getId())
                                                                  ->getObject(theOpticalGroup->getId())
                                                                  ->getObject(theHybrid->getId())
                                                                  ->getSummary<std::vector<GenericDataArray<float, 2>>>())
                                {
                                    theNumberOfBitsAndErrors.at(0) = 0;
                                    theNumberOfBitsAndErrors.at(1) = 0;
                                }
                            } // hybrid loop
                        } // lpgbt phase loop
                    } // CIC driver strenght loop
                } // clock strenght loop
            } // polarity loop
        } // optical group loop
    }
}

void OTCICtoLpGBTecv::Stop(void)
{
    LOG(INFO) << "Stopping OTCICtoLpGBTecv measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTCICtoLpGBTecv.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCICtoLpGBTecv stopped.";
}

void OTCICtoLpGBTecv::Pause() {}

void OTCICtoLpGBTecv::Resume() {}

void OTCICtoLpGBTecv::Reset() { fRegisterHelper->restoreSnapshot(); }
