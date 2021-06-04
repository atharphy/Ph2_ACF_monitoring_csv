/*!OA
  \file                  RD53Gain.cc
  \brief                 Implementaion of Gain scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Gain.h"

#include <boost/multiprecision/number.hpp>

using namespace boost::numeric;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void Gain::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    rowStart       = this->findValueInSettings("ROWstart");
    rowStop        = this->findValueInSettings("ROWstop");
    colStart       = this->findValueInSettings("COLstart");
    colStop        = this->findValueInSettings("COLstop");
    nEvents        = this->findValueInSettings("nEvents");
    startValue     = this->findValueInSettings("VCalHstart");
    stopValue      = this->findValueInSettings("VCalHstop");
    targetCharge   = RD53chargeConverter::Charge2VCal(this->findValueInSettings("TargetCharge"));
    nSteps         = this->findValueInSettings("VCalHnsteps");
    offset         = this->findValueInSettings("VCalMED");
    nHITxCol       = this->findValueInSettings("nHITxCol");
    doFast         = this->findValueInSettings("DoFast");
    doDisplay      = this->findValueInSettings("DisplayHisto");
    doUpdateChip   = this->findValueInSettings("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings("SaveBinaryData");

    // ########################
    // # Custom channel group #
    // ########################
    ChannelGroup<RD53::nRows, RD53::nCols> customChannelGroup;
    customChannelGroup.disableAllChannels();

    for(auto row = rowStart; row <= rowStop; row++)
        for(auto col = colStart; col <= colStop; col++) customChannelGroup.enableChannel(row, col);

    theChnGroupHandler = std::make_shared<RD53ChannelGroupHandler>(customChannelGroup, doFast == true ? RD53GroupType::OneGroup : RD53GroupType::AllGroups, nHITxCol);
    theChnGroupHandler->setCustomChannelGroup(customChannelGroup);

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    const float step = (stopValue - startValue) / nSteps;
    for(auto i = 0u; i < nSteps; i++) dacList.push_back(startValue + step * i);

    // #################################
    // # Initialize container recycler #
    // #################################
    theRecyclingBin.setDetectorContainer(fDetectorContainer);

    // #######################
    // # Initialize progress #
    // #######################
    RD53RunProgress::total() += Gain::getNumberIterations();

    // ############################################################
    // # Create directory for: raw data, config files, histograms #
    // ############################################################
    this->CreateResultDirectory(RD53Shared::RESULTDIR, false, false);
}

void Gain::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[Gain::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    if(saveBinaryData == true)
    {
        this->addFileHandler(std::string(fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Gain.raw", 'w');
        this->initializeWriteFileHandler();
    }

    Gain::run();
    Gain::analyze();
    Gain::saveChipRegisters(theCurrentRun);
    Gain::sendData();
}

void Gain::sendData()
{
    auto theOccStream  = prepareChannelContainerStreamer<OccupancyAndPh, uint16_t>("Occ");
    auto theGainStream = prepareChannelContainerStreamer<GainFit>("Gain");

    if(fStreamerEnabled == true)
    {
        size_t index = 0;
        for(const auto theOccContainer: detectorContainerVector)
        {
            theOccStream.setHeaderElement(dacList[index] - offset);
            for(const auto cBoard: *theOccContainer) theOccStream.streamAndSendBoard(cBoard, fNetworkStreamer);
            index++;
        }

        if(theGainContainer != nullptr)
            for(const auto cBoard: *theGainContainer.get()) theGainStream.streamAndSendBoard(cBoard, fNetworkStreamer);
    }
}

void Gain::Stop()
{
    LOG(INFO) << GREEN << "[Gain::Stop] Stopping" << RESET;

    Tool::Stop();

    Gain::draw();
    this->closeFileHandler();

    RD53RunProgress::reset();
}

void Gain::localConfigure(const std::string fileRes_, int currentRun)
{
#ifdef __USE_ROOT__
    histos = nullptr;
#endif

    if(currentRun >= 0)
    {
        theCurrentRun = currentRun;
        LOG(INFO) << GREEN << "[Gain::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
    }
    Gain::ConfigureCalibration();
    Gain::initializeFiles(fileRes_, currentRun);
}

void Gain::initializeFiles(const std::string fileRes_, int currentRun)
{
    fileRes = fileRes_;

    if((currentRun >= 0) && (saveBinaryData == true))
    {
        this->addFileHandler(std::string(this->fDirectoryName) + "/Run" + RD53Shared::fromInt2Str(currentRun) + "_Gain.raw", 'w');
        this->initializeWriteFileHandler();
    }

#ifdef __USE_ROOT__
    delete histos;
    histos = new GainHistograms;
#endif
}

void Gain::run()
{
    // ##########################
    // # Set new VCAL_MED value #
    // ##########################
    for(const auto cBoard: *fDetectorContainer) this->fReadoutChipInterface->WriteBoardBroadcastChipReg(cBoard, "VCAL_MED", offset);

    for(auto container: detectorContainerVector) theRecyclingBin.free(container);
    detectorContainerVector.clear();
    for(auto i = 0u; i < dacList.size(); i++) detectorContainerVector.push_back(theRecyclingBin.get(&ContainerFactory::copyAndInitStructure<OccupancyAndPh>, OccupancyAndPh()));

    this->fChannelGroupHandler = theChnGroupHandler.get();
    this->SetBoardBroadcast(true);
    this->SetTestPulse(true);
    this->fMaskChannelsFromOtherGroups = true;
    this->scanDac("VCAL_HIGH", dacList, nEvents, detectorContainerVector);

    // #########################
    // # Mark enabled channels #
    // #########################
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(!static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) || !this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                                for(auto i = 0u; i < dacList.size(); i++)
                                    detectorContainerVector[i]
                                        ->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<OccupancyAndPh>(row, col)
                                        .fOccupancy = RD53Shared::ISDISABLED;

    // ################
    // # Error report #
    // ################
    Gain::chipErrorReport();
}

void Gain::draw(bool saveData)
{
    if(saveData == true) Gain::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((saveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> Gain saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    Gain::fillHisto();
    histos->process();
    if(saveData == true) this->WriteRootFile();

    if(doDisplay == true) myApp->Run(true);
#endif

    // #####################
    // # @TMP@ : CalibFile #
    // #####################
    if(saveBinaryData == true)
    {
        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                        std::stringstream myString;
                        myString.clear();
                        myString.str("");
                        myString << this->fDirectoryName + "/Run" + RD53Shared::fromInt2Str(theCurrentRun) + "_Gain_"
                                 << "B" << std::setfill('0') << std::setw(2) << +cBoard->getId() << "_"
                                 << "O" << std::setfill('0') << std::setw(2) << +cOpticalGroup->getId() << "_"
                                 << "M" << std::setfill('0') << std::setw(2) << +cHybrid->getId() << "_"
                                 << "C" << std::setfill('0') << std::setw(2) << +cChip->getId() << ".dat";
                        std::ofstream fileOutID(myString.str(), std::ios::out);
                        for(auto i = 0u; i < dacList.size(); i++)
                        {
                            fileOutID << "Iteration " << i << " --- reg = " << dacList[i] - offset << std::endl;
                            for(auto row = 0u; row < RD53::nRows; row++)
                                for(auto col = 0u; col < RD53::nCols; col++)
                                    if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                                        fileOutID << "r " << row << " c " << col << " h "
                                                  << detectorContainerVector[i]
                                                             ->at(cBoard->getIndex())
                                                             ->at(cOpticalGroup->getIndex())
                                                             ->at(cHybrid->getIndex())
                                                             ->at(cChip->getIndex())
                                                             ->getChannel<OccupancyAndPh>(row, col)
                                                             .fOccupancy *
                                                         nEvents
                                                  << " a "
                                                  << detectorContainerVector[i]
                                                         ->at(cBoard->getIndex())
                                                         ->at(cOpticalGroup->getIndex())
                                                         ->at(cHybrid->getIndex())
                                                         ->at(cChip->getIndex())
                                                         ->getChannel<OccupancyAndPh>(row, col)
                                                         .fPh
                                                  << std::endl;
                        }
                        fileOutID.close();
                    }
    }
}

std::shared_ptr<DetectorDataContainer> Gain::analyze()
{
    float slope, slopeErr, intercept, interceptErr, quadratic, quadraticErr, log, logErr, chi2, DoF;

    std::vector<float> par(NGAINPAR, 0);
    std::vector<float> parErr(NGAINPAR, 0);

    std::vector<float> x(dacList.size(), 0);
    std::vector<float> y(dacList.size(), 0);
    std::vector<float> e(dacList.size(), 0);

    theGainContainer = std::make_shared<DetectorDataContainer>();
    ContainerFactory::copyAndInitStructure<GainFit>(*fDetectorContainer, *theGainContainer);

    size_t index = 0;
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto row = 0u; row < RD53::nRows; row++)
                        for(auto col = 0u; col < RD53::nCols; col++)
                            if(static_cast<RD53*>(cChip)->getChipOriginalMask()->isChannelEnabled(row, col) && this->fChannelGroupHandler->allChannelGroup()->isChannelEnabled(row, col))
                            {
                                for(auto i = 0u; i < dacList.size(); i++)
                                {
                                    x[i] = dacList[i] - offset;
                                    y[i] = detectorContainerVector[i]
                                               ->at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getChannel<OccupancyAndPh>(row, col)
                                               .fPh;
                                    e[i] = detectorContainerVector[i]
                                               ->at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getChannel<OccupancyAndPh>(row, col)
                                               .fPhError;
                                }

                                // ##################
                                // # Run regression #
                                // ##################
                                Gain::computeStats(x, y, e, par, parErr, chi2, DoF);
                                intercept    = par[0];
                                interceptErr = parErr[0];
                                slope        = par[1];
                                slopeErr     = parErr[1];
                                quadratic    = par[2];
                                quadraticErr = parErr[2];
                                log          = par[3];
                                logErr       = parErr[3];

                                if(chi2 != 0)
                                {
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fSlope =
                                        slope;
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fSlopeError =
                                        slopeErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fIntercept =
                                        intercept;
                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fInterceptError = interceptErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fQuadratic =
                                        quadratic;
                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fQuadraticError = quadraticErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fLog = log;
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fLogError =
                                        logErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fChi2 = chi2;
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fDoF  = DoF;
                                }
                                else
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fChi2 =
                                        RD53Shared::FITERROR;
                            }

                    index++;
                }

    theGainContainer->normalizeAndAverageContainers(fDetectorContainer, this->fChannelGroupHandler->allChannelGroup(), 1);

    for(const auto cBoard: *theGainContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    float ToTatTarget = Gain::gainFunction({cChip->getSummary<GainFit, GainFit>().fIntercept,
                                                            cChip->getSummary<GainFit, GainFit>().fSlope,
                                                            cChip->getSummary<GainFit, GainFit>().fQuadratic,
                                                            cChip->getSummary<GainFit, GainFit>().fLog},
                                                           targetCharge);
                    LOG(INFO) << GREEN << "Average ToT for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET << GREEN << "] at VCal = " << BOLDYELLOW << std::fixed << std::setprecision(2) << targetCharge << RESET << GREEN << " (" << BOLDYELLOW
                              << RD53chargeConverter::VCal2Charge(targetCharge) << RESET << GREEN << " electrons) is " << BOLDYELLOW << ToTatTarget << RESET << GREEN << " (ToT)"
                              << std::setprecision(-1) << RESET;
                    RD53Shared::resetDefaultFloat();
                }

    return theGainContainer;
}

void Gain::fillHisto()
{
#ifdef __USE_ROOT__
    for(auto i = 0u; i < dacList.size(); i++) histos->fillOccupancy(*detectorContainerVector[i], dacList[i] - offset);
    histos->fillGain(*theGainContainer);
#endif
}

void Gain::computeStats(const std::vector<float>& x, const std::vector<float>& y, const std::vector<float>& e, std::vector<float>& par, std::vector<float>& parErr, float& chi2, float& DoF)
// ###############################################
// # Linear regression with least-square method  #
// # Model: y = f(x) = a + b*x + c*x^2 + d*ln(x) #
// ###############################################
{
    int nData = 0;

    for(auto err: e)
        if(err != 0) nData++;

    chi2 = 0;
    DoF  = nData - NGAINPAR;
    for(auto c = 0; c < NGAINPAR; c++)
    {
        par[c]    = 0;
        parErr[c] = 0;
    }
    if(DoF < 1) return;

    ublas::matrix<double> H(nData, NGAINPAR);
    ublas::matrix<double> V(nData, nData);
    ublas::vector<double> myY(nData);

    for(auto c = 0u; c < V.size2(); c++)
        for(auto r = 0u; r < V.size1(); r++) V(r, c) = 0;

    int r = 0;
    for(auto i = 0u; i < x.size(); i++)
        if(e[i] != 0)
        {
            H(r, 0) = 1;
            H(r, 1) = x[i];
            H(r, 2) = x[i] * x[i];
            H(r, 3) = log(x[i]);

            V(r, r) = e[i] * e[i];
            myY[r]  = y[i];

            r++;
        }

    auto invV(V);
    for(r = 0; r < nData; r++) invV(r, r) = 1 / V(r, r);

    ublas::matrix<double> tmpMtx(ublas::prod(invV, H));
    ublas::matrix<double> invParCov(ublas::prod(ublas::trans(H), tmpMtx));
    auto                  parCov(invParCov);

    auto det = RD53Shared::mtxInversion<double>(invParCov, parCov);
    if((isnan(det) == false) && (det != 0))
    {
        ublas::vector<double> tmpVec1(ublas::prod(invV, myY));
        ublas::vector<double> tmpVec2(ublas::prod(ublas::trans(H), tmpVec1));
        ublas::vector<double> myPar(ublas::prod(parCov, tmpVec2));

        std::copy(myPar.begin(), myPar.end(), par.begin());
        for(auto c = 0; c < NGAINPAR; c++) parErr[c] = sqrt(parCov(c, c));

        // ################
        // # Compute chi2 #
        // ################
        ublas::vector<double> num(myY - ublas::prod(H, myPar));
        ublas::vector<double> tmpNum(ublas::prod(invV, num));
        chi2 = ublas::inner_prod(num, tmpNum);
    }
}

void Gain::chipErrorReport() const
{
    for(const auto cBoard: *fDetectorContainer)
        for(auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "Readout chip error report for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << GREEN << "]" << RESET;
                    static_cast<RD53Interface*>(this->fReadoutChipInterface)->ChipErrorReport(cChip);
                }
}

void Gain::saveChipRegisters(int currentRun)
{
    const std::string fileReg("Run" + RD53Shared::fromInt2Str(currentRun) + "_");

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    static_cast<RD53*>(cChip)->copyMaskFromDefault();
                    if(doUpdateChip == true) static_cast<RD53*>(cChip)->saveRegMap("");
                    static_cast<RD53*>(cChip)->saveRegMap(fileReg);
                    std::string command("mv " + static_cast<RD53*>(cChip)->getFileName(fileReg) + " " + RD53Shared::RESULTDIR);
                    system(command.c_str());
                    LOG(INFO) << BOLDBLUE << "\t--> Gain saved the configuration file for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                              << cHybrid->getId() << "/" << +cChip->getId() << RESET << BOLDBLUE << "]" << RESET;
                }
}
