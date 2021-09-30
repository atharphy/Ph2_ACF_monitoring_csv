/*!
  \file                  RD53DataTransmissionTest.cc
  \brief                 TAP0 scan to measure the Bit Error Rate and determine data transmission quality
  \author                Marijus AMBROZAS
  \version               1.0
  \date                  26/04/20
  Support:               email to marijus.ambrozas@cern.ch
*/

#include "RD53DataTransmissionTest.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void DataTransmissionTest::ConfigureCalibration()
{
    // ##############################
    // # Initialize sub-calibration #
    // ##############################
    BERtest::ConfigureCalibration();

    // #######################
    // # Retrieve parameters #
    // #######################
    BERtarget      = this->findValueInSettings<double>("TargetBER");
    given_time     = this->findValueInSettings<double>("byTime");
    frames_or_time = this->findValueInSettings<double>("framesORtime");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void DataTransmissionTest::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[DataTransmissionTest::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    DataTransmissionTest::run();
    DataTransmissionTest::sendData();
}

void DataTransmissionTest::sendData()
{
    // Store (TAP0, BER, BERlowErr, BERupErr)
    auto theStreamTAP0scan = prepareChipContainerStreamer<EmptyContainer, std::array<std::tuple<uint16_t, double, double, double>, 11>>("DataTransmissionTestTAP0scan");
    // Store TAP0 value that has nearest BER to the target
    auto theStreamTAP0tgt = prepareChipContainerStreamer<EmptyContainer, uint16_t>("DataTransmissionTestTAP0target");

    if(fStreamerEnabled == true)
    {
        for(const auto cBoard: theTAP0scanContainer) theStreamTAP0scan.streamAndSendBoard(cBoard, fNetworkStreamer);
        for(const auto cBoard: theTAP0tgtContainer) theStreamTAP0tgt.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void DataTransmissionTest::Stop()
{
    LOG(INFO) << GREEN << "[DataTransmissionTest::Stop] Stopping" << RESET;

    Tool::Stop();
    DataTransmissionTest::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void DataTransmissionTest::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[DataTransmissionTest::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    DataTransmissionTest::ConfigureCalibration();
    DataTransmissionTest::initializeFiles(fileRes_, currentRun);
}

void DataTransmissionTest::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

#ifdef __USE_ROOT__
    delete histos;
    histos = new DataTransmissionTestGraphs;
#endif
}

void DataTransmissionTest::run()
{
    ContainerFactory::copyAndInitChip<std::array<std::tuple<uint16_t, double, double, double>, 11>>(*fDetectorContainer, theTAP0scanContainer);

    for(const auto cBoard: *fDetectorContainer) static_cast<RD53Interface*>(this->fReadoutChipInterface)->WriteBoardBroadcastChipReg(cBoard, "CML_CONFIG_SER_EN_TAP", 0x0);
    DataTransmissionTest::binSearch(&theTAP0scanContainer);
    DataTransmissionTest::analyze(theTAP0scanContainer, theTAP0tgtContainer);

    DataTransmissionTest::chipErrorReport();
}

void DataTransmissionTest::draw(bool saveData)
{
#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    this->InitResultFile(fileRes);
    LOG(INFO) << BOLDBLUE << "\t--> DataTransmissionTest saving histograms..." << RESET;

    histos->book(fResultFile, *fDetectorContainer, fSettingsMap);
    DataTransmissionTest::fillHisto();
    histos->process();
    this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);

    this->CloseResultFile();
#endif
}

void DataTransmissionTest::analyze(const DetectorDataContainer& theTAP0scanContainer, DetectorDataContainer& theTAP0tgtContainer)
{
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theTAP0tgtContainer);

    for(const auto cBoard: theTAP0scanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    double nearestBERup  = 2; // to store the nearest BER value to the target
                    double nearestBERlo  = 2; // to store the second nearest BER value to the target
                    double nearestTAP0up = 0; // to store the TAP0 value that gives the nearest BER
                    double nearestTAP0lo = 0; // to store the TAP0 value that gives the second nearest BER

                    // Find the TAP0 values nearest to the target BER
                    for(auto i = 0u; i < 11u; i++)
                    {
                        auto currentTAP0 = std::get<0>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]);
                        auto currentBER  = std::get<1>((cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i]);
                        if(currentBER > 0 && currentBER >= BERtarget &&
                           (fabs(currentBER - BERtarget) < fabs(nearestBERup - BERtarget) || (fabs(currentBER - BERtarget) == fabs(nearestBERup - BERtarget) && currentTAP0 > nearestTAP0up)))
                        {
                            nearestBERup  = currentBER;
                            nearestTAP0up = (double)currentTAP0;
                        }
                        else if(currentBER > 0 && currentBER < BERtarget &&
                                (fabs(currentBER - BERtarget) < fabs(nearestBERlo - BERtarget) || (fabs(currentBER - BERtarget) == fabs(nearestBERlo - BERtarget) && currentTAP0 < nearestTAP0lo)))
                        {
                            nearestBERlo  = currentBER;
                            nearestTAP0lo = (double)currentTAP0;
                        }
                    }

                    // Saving the very nearest value
                    auto nearestTAP0 = fabs(BERtarget - nearestBERup) < fabs(BERtarget - nearestBERlo) ? nearestTAP0up : nearestTAP0lo;

                    LOG(INFO) << BOLDMAGENTA << ">>> TAP0 value at target (BER=" << std::setprecision(4) << std::scientific << BOLDYELLOW << BERtarget << BOLDMAGENTA << std::fixed
                              << std::setprecision(0) << ") for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << BOLDMAGENTA << "] is " << BOLDYELLOW << nearestTAP0 << BOLDMAGENTA << " <<<" << RESET;

                    // Fill the container with TAP0 value at target BER
                    theTAP0tgtContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() = (uint16_t)nearestTAP0;

                    // Fitting the last two points with exp(a-bx)
                    if(nearestBERup <= 1 && nearestBERlo < BERtarget)
                    {
                        double a = log(nearestBERup) + log(nearestBERup / nearestBERlo) * nearestTAP0up / (nearestTAP0lo - nearestTAP0up);
                        double b = log(nearestBERup / nearestBERlo) / (nearestTAP0lo - nearestTAP0up);

                        LOG(INFO) << BOLDBLUE << "Exponential fit function between the two points around target (" << BOLDYELLOW << nearestTAP0up << BOLDBLUE << ", " << BOLDYELLOW << nearestTAP0lo
                                  << BOLDBLUE << "): BER = EXP(a-b*TAP0)" << RESET;
                        LOG(INFO) << BOLDBLUE << std::scientific << std::setprecision(5) << "a = " << BOLDYELLOW << a << RESET;
                        LOG(INFO) << BOLDBLUE << "b = " << BOLDYELLOW << b << RESET;
                        if(fabs(nearestTAP0lo - nearestTAP0up) > 1)
                            LOG(INFO) << BOLDYELLOW << "You might want to consider adjusting the BER target as the distance between nearest two points is >2 (likely due to fluctuations)" << RESET;
                    }
                    else
                        LOG(ERROR) << BOLDRED << "Could not find two closest points around the BER target that are proper to fit. BER target should be adjusted." << RESET;
                }
}

void DataTransmissionTest::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillTAP0scan(theTAP0scanContainer);
    histos->fillTAP0tgt(theTAP0tgtContainer);
#endif
}

void DataTransmissionTest::binSearch(DetectorDataContainer* theTAP0scanContainer)
{
    uint16_t currentTAP0   = 1023; // start from highest TAP0
    uint16_t step          = 512;  // next point will be 512 units away
    uint8_t  timesRepeated = 0;    // to be used for dealing with errors

    for(auto i = 0u; i < 11u; i++)
    {
        // Setting new TAP0 value
        LOG(INFO) << BOLDMAGENTA << ">>> " << BOLDYELLOW << "CML_TAP0_BIAS" << BOLDMAGENTA << " value = " << BOLDYELLOW << currentTAP0 << BOLDMAGENTA << " <<<" << RESET;
        for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "CML_TAP0_BIAS", currentTAP0);

        // Run BER test
        BERtest::run();

        bool searchUp = 0; // to determine if next TAP0 point will be higher or lower
        bool repeat   = 0; // in case of an error

        // Saving the data
        for(const auto cBoard: *theTAP0scanContainer)
        {
            // Finding the total number of frames
            uint32_t       frontendSpeed   = static_cast<RD53FWInterface*>(fBeBoardFWMap[cBoard->getId()])->ReadoutSpeed();
            const uint32_t nBitInClkPeriod = 32. / std::pow(2, frontendSpeed); // Number of bits in the 40 MHz clock period
            const double   fps             = 1.28e9 / nBitInClkPeriod;         // Frames per second
            double         nFrames         = frames_or_time;
            if(given_time) nFrames = frames_or_time * fps;

            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        auto bitErrRate  = BERtest::theBERtestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<double>();
                        auto bitErrCount = bitErrRate * nFrames;

                        if(bitErrRate < 0) // in case a measurement fails for some reason
                            repeat = 1;
                        else
                        {
                            // ######################################################### //
                            // From Ulrich and Xu:                                       //
                            // n = number of frames sent                                 //
                            // k = number of errors measured                             //
                            // mode = k/n                                                //
                            // mean = (k+1)/(n+2)                                        //
                            // variance = ((k+1)(k+2))/((n+2)(n+3)) - (k+1)^2/(n+2)^2    //
                            // std = sqrt(variance)                                      //
                            // 68% confidence is mean+-std                               //
                            // 68% confidence around mode is not necessarily symmetrical //
                            // ######################################################### //
                            auto mean     = (bitErrCount + 1) / (nFrames + 2);
                            auto variance = ((bitErrCount + 1) * (bitErrCount + 2)) / ((nFrames + 2) * (nFrames + 3)) - ((bitErrCount + 1) * (bitErrCount + 1)) / ((nFrames + 2) * (nFrames + 3));
                            auto std      = sqrt(variance);
                            auto errUp    = mean - bitErrRate + std;
                            auto errLo    = bitErrRate - mean + std;
                            if(bitErrRate == 0) errLo = 0;

                            // Saving everything at once
                            (cChip->getSummary<std::array<std::tuple<uint16_t, double, double, double>, 11>>())[i] = std::make_tuple(currentTAP0, bitErrRate, errLo, errUp);

                            // Deciding where the next point should be
                            if(bitErrRate > BERtarget) searchUp = 1;
                        }
                    }
        }

        // Send periodic data to monitor the progress
        DataTransmissionTest::sendData();

        // Repeat in case the measurement has failed (no more than twice)
        if(repeat && timesRepeated < 3)
        {
            i -= 1;
            timesRepeated += 1;
            LOG(ERROR) << BOLDRED << "Failed to record data. Repeating the same step." << RESET;
        }
        // Deciding the next TAP0 value
        else
        {
            timesRepeated = 0;
            if(i == 0u && searchUp)
            {
                LOG(ERROR) << BOLDRED << "BER is over the set target at the first step. Stopping..." << RESET;
                break;
            }
            else
            {
                if(searchUp)
                    currentTAP0 += step;
                else
                    currentTAP0 -= step;
                step /= 2;
            }
        }
    }
}

void DataTransmissionTest::chipErrorReport() const
{
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->ChipErrorReport(cChip);
                }
}
