/*!
  \file                  RD53BERtest.h
  \brief                 Implementaion of Bit Error Rate test
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53BERtest.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void BERtest::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    chain2test     = this->findValueInSettings<double>("chain2Test");
    given_time     = this->findValueInSettings<double>("byTime");
    frames_or_time = this->findValueInSettings<double>("framesORtime");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");

    // ##########################################################################################
    // # Select BER counter meaning: number of frames with errors or number of bits with errors #
    // ##########################################################################################
    for(const auto cBoard: *fDetectorContainer) static_cast<RD53FWInterface*>(fBeBoardFWMap[cBoard->getId()])->SelectBERcheckBitORFrame(0);

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void BERtest::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[BERtest::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    BERtest::run();
    BERtest::sendData();
}

void BERtest::sendData()
{
    auto theStream = prepareChipContainerStreamer<EmptyContainer, double>("BERtest");

    if(fStreamerEnabled == true)
        for(const auto cBoard: theBERtestContainer) theStream.streamAndSendBoard(cBoard, fNetworkStreamer);
}

void BERtest::Stop()
{
    LOG(INFO) << GREEN << "[BERtest::Stop] Stopping" << RESET;

    Tool::Stop();

    BERtest::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void BERtest::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[BERtest::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    BERtest::ConfigureCalibration();
    BERtest::initializeFiles(fileRes_, currentRun);
}

void BERtest::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

#ifdef __USE_ROOT__
    delete histos;
    histos = new BERtestHistograms;
#endif
}

void BERtest::run()
{
    ContainerFactory::copyAndInitChip<double>(*fDetectorContainer, theBERtestContainer);

    if(chain2test == 1)
        for(const auto cBoard: *fDetectorContainer)
        {
            uint32_t frontendSpeed = static_cast<RD53FWInterface*>(fBeBoardFWMap[cBoard->getId()])->ReadoutSpeed();

            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                {
                    flpGBTInterface->StartPRBSpattern(cOpticalGroup->flpGBT);

                    auto value = fBeBoardFWMap[cBoard->getId()]->RunBERtest(given_time, frames_or_time, cHybrid->getId(), 0, frontendSpeed); // @TMP@
                    theBERtestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(0)->getSummary<double>() = value;

                    LOG(INFO) << GREEN << "BER test for [board/opticalGroup/hybrid = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << RESET << GREEN
                              << "]: " << BOLDYELLOW << (value == 0 ? "PASSED" : "NOT PASSED") << RESET;

                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->InitRD53Downlink(cBoard);
                    flpGBTInterface->StopPRBSpattern(cOpticalGroup->flpGBT);
                }
        }
    else
        for(const auto cBoard: *fDetectorContainer)
        {
            uint32_t frontendSpeed = static_cast<RD53FWInterface*>(fBeBoardFWMap[cBoard->getId()])->ReadoutSpeed();

            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        uint8_t cGroup   = static_cast<RD53*>(cChip)->getRxGroup();
                        uint8_t cChannel = static_cast<RD53*>(cChip)->getRxChannel();

                        fReadoutChipInterface->StartPRBSpattern(cChip);

                        auto value = (chain2test == 0 ? fBeBoardFWMap[cBoard->getId()]->RunBERtest(given_time, frames_or_time, cHybrid->getId(), cChip->getId(), frontendSpeed)
                                                      : flpGBTInterface->RunBERtest(cOpticalGroup->flpGBT, cGroup, cChannel, given_time, frames_or_time, frontendSpeed));
                        theBERtestContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<double>() = value;

                        LOG(INFO) << GREEN << "BER test for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                  << +cChip->getId() << RESET << GREEN << "]: " << BOLDYELLOW << (value == 0 ? "PASSED" : "NOT PASSED") << RESET;

                        fReadoutChipInterface->StopPRBSpattern(cChip);
                        static_cast<RD53Interface*>(this->fReadoutChipInterface)->InitRD53Downlink(cBoard);
                        fReadoutChipInterface->StopPRBSpattern(cChip);
                    }
        }
}

void BERtest::draw()
{
#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> BERtest saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    BERtest::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

void BERtest::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillBERtest(theBERtestContainer);
#endif
}
