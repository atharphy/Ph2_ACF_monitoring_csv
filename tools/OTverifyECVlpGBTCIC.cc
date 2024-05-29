#include "tools/OTverifyECVlpGBTCIC.h"
#include "HWDescription/BeBoard.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTverifyECVlpGBTCIC::fCalibrationDescription = "Performs the Electric Chain Validation between the lpGBT and the CIC using the algorithms implemented in OTverifyBoardDataWord.";

OTverifyECVlpGBTCIC::OTverifyECVlpGBTCIC() : OTverifyBoardDataWord() {}

OTverifyECVlpGBTCIC::~OTverifyECVlpGBTCIC() {}

void OTverifyECVlpGBTCIC::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

    fNumberOfIterations              = findValueInSettings<double>("OTverifyECVlpGBTCIC_NumberOfIterations", 100);
    size_t             numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    std::vector<float> initialEmptyVector(numberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<float>>(*fDetectorContainer, fPatternMatchingEfficiencyContainer, initialEmptyVector);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTverifyECVlpGBTCIC.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTverifyECVlpGBTCIC::ConfigureCalibration() {}

void OTverifyECVlpGBTCIC::Running()
{
    LOG(INFO) << "Starting OTverifyECVlpGBTCIC measurement.";
    Initialise();
    runECV();
    LOG(INFO) << "Done with OTverifyECVlpGBTCIC.";
    Reset();
}

void OTverifyECVlpGBTCIC::runECV()
{
    uint8_t clockPolarityStart = 0, clockPolarityEnd = 1;
    uint8_t cicClockStrengthStart = 1, cicClockStrengthEnd = 7;
    uint8_t cicSLVSStrengthStart = 1, cicSLVSStrengthEnd = 5;
    uint8_t lpGBTPhaseStart = 0, lpGBTPhaseEnd = 14;

    LOG(INFO) << BOLDYELLOW << "OTverifyECVlpGBTCIC::runIntegrityTest ... start integrity test" << RESET;
    auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            for(uint8_t clockPolarity = clockPolarityStart; clockPolarity <= clockPolarityEnd; clockPolarity++)
            {
                for(uint8_t clockStrength = cicClockStrengthStart; clockStrength <= cicClockStrengthEnd; clockStrength++)
                {
                    static_cast<D19clpGBTInterface*>(flpGBTInterface)->setCICClockPolarityAndStrength(theOpticalGroup->flpGBT, clockPolarity, clockStrength, theOpticalGroup);

                    uint8_t numberOfBytesInSinglePacket = getNumberOfBytesInSinglePacket(theOpticalGroup);
                    if(fIsKickoff && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
                        LOG(INFO) << BOLDYELLOW << "Attention! ignoring failures on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                    size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;

                    for(uint8_t cicStrength = cicSLVSStrengthStart; cicStrength <= cicSLVSStrengthEnd; cicStrength++)
                    {
                        for(auto cHybrid: *theOpticalGroup)
                        {
                            auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                            fCicInterface->ConfigureDriveStrength(cCic, cicStrength);
                        }

                        for(uint8_t phase = lpGBTPhaseStart; phase <= lpGBTPhaseEnd; phase++)
                        {
                            LOG(INFO) << BOLDRED << "CLOCK POLARITY:\t" << +clockPolarity << RESET;
                            LOG(INFO) << BOLDRED << "CLOCK STRENGTH:\t" << +clockStrength << RESET;
                            LOG(INFO) << BOLDRED << "CIC STRENGTH:\t" << +cicStrength << RESET;
                            LOG(INFO) << BOLDRED << "RX PHASE:\t" << +phase << RESET;
                            std::map<uint8_t, std::vector<uint8_t>> theGroupsAndChannels = theOpticalGroup->getLpGBTrxGroupsAndChannels();
                            auto&                                   clpGBT               = theOpticalGroup->flpGBT;
                            flpGBTInterface->ConfigureAllRxPhase(clpGBT, phase, theGroupsAndChannels);

                            for(auto theHybrid: *theOpticalGroup)
                            {
                                auto& theHybridPatternMatchingEfficiency = fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())
                                                                               ->getObject(theOpticalGroup->getId())
                                                                               ->getObject(theHybrid->getId())
                                                                               ->getSummary<std::vector<float>>();
                                LOG(INFO) << BOLDMAGENTA << "Running runStubIntegrityTest on Hybrid " << +theHybrid->getId() << RESET;
                                prepareHybridForStubIntegrityTest(theHybrid);

                                for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                                {
                                    auto lineOutputVector = theFWInterface->StubDebug(true, cNlines, false);
                                    for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
                                    {
                                        for(auto pattern: stubPatterns)
                                        {
                                            uint8_t flagCharacter = pattern.first;
                                            uint8_t idleCharacter = pattern.second;
                                            if(isStubPatternMatched(lineOutputVector[lineIndex], numberOfBytesInSinglePacket, flagCharacter, idleCharacter))
                                            {
                                                ++theHybridPatternMatchingEfficiency[lineIndex + 1];
                                                break;
                                            }
                                            else if(!(fIsKickoff && ((theHybrid->getId() % 2) == 0) && ((lineIndex) == 4) && (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)))
                                                LOG(DEBUG) << BOLDRED << "Error on stub line " << lineIndex + 1 << " occurred in iteration number " << +iteration << RESET;
                                        }
                                    }
                                }

                                LOG(INFO) << BOLDMAGENTA << "Running L1StubIntegrityTeston Hybrid " << +theHybrid->getId() << RESET;
                                prepareFWForL1IntegrityTest(theBoard);
                                prepareHybridForL1IntegrityTest(theHybrid);

                                for(size_t iteration = 0; iteration < fNumberOfIterations; iteration++)
                                {
                                    auto lineOutputVector = theFWInterface->L1ADebug(1, false);

                                    for(auto pattern: L1Patterns)
                                    {
                                        uint32_t header = pattern;

                                        if(isL1HeaderFound(lineOutputVector, numberOfBytesInSinglePacket, header))
                                        {
                                            ++theHybridPatternMatchingEfficiency[0];
                                            break;
                                        }
                                        else { LOG(DEBUG) << BOLDRED << "Error occurred in iteration number " << +iteration << RESET; }
                                    }
                                }

                                for(auto& theNumberOfMatches: fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())
                                                                  ->getObject(theOpticalGroup->getId())
                                                                  ->getObject(theHybrid->getId())
                                                                  ->getSummary<std::vector<float>>())
                                {
                                    theNumberOfMatches /= fNumberOfIterations;
                                }

                            } // hybrid loop
#ifdef __USE_ROOT__
                            fDQMHistogramOTverifyECVlpGBTCIC.fillEfficiency(clockPolarity, clockStrength, cicStrength, phase, fPatternMatchingEfficiencyContainer);
#else
                            if(fDQMStreamerEnabled)
                            {
                                ContainerSerialization theECVlpGBTCICContainerSerialization("OTverifyECVlpGBTCICEfficiencyHistogram");
                                theECVlpGBTCICContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fPatternMatchingEfficiencyContainer, clockPolarity, clockStrength, cicStrength, phase);
                            }
#endif

                            // reset the number of matches!!
                            for(auto theHybrid: *theOpticalGroup)
                            {
                                for(auto& theNumberOfMatches: fPatternMatchingEfficiencyContainer.getObject(theBoard->getId())
                                                                  ->getObject(theOpticalGroup->getId())
                                                                  ->getObject(theHybrid->getId())
                                                                  ->getSummary<std::vector<float>>())
                                {
                                    theNumberOfMatches = 0;
                                }
                            } // hybrid loop
                        }     // lpgbt phase loop
                    }         // CIC driver strenght loop
                }             // clock strenght loop
            }                 // polarity loop
        }                     // optical group loop
    }
}

void OTverifyECVlpGBTCIC::Stop(void)
{
    LOG(INFO) << "Stopping OTverifyECVlpGBTCIC measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTverifyECVlpGBTCIC.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTverifyECVlpGBTCIC stopped.";
}

void OTverifyECVlpGBTCIC::Pause() {}

void OTverifyECVlpGBTCIC::Resume() {}

void OTverifyECVlpGBTCIC::Reset() { fRegisterHelper->restoreSnapshot(); }
