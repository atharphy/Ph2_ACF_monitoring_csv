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
    rowStart       = this->findValueInSettings<double>("ROWstart");
    rowStop        = this->findValueInSettings<double>("ROWstop");
    colStart       = this->findValueInSettings<double>("COLstart");
    colStop        = this->findValueInSettings<double>("COLstop");
    nEvents        = this->findValueInSettings<double>("nEvents");
    startValue     = this->findValueInSettings<double>("VCalHstart");
    stopValue      = this->findValueInSettings<double>("VCalHstop");
    targetCharge   = RD53chargeConverter::Charge2VCal(this->findValueInSettings<double>("TargetCharge"));
    nSteps         = this->findValueInSettings<double>("VCalHnsteps");
    offset         = this->findValueInSettings<double>("VCalMED");
    nHITxCol       = this->findValueInSettings<double>("nHITxCol");
    doFast         = this->findValueInSettings<double>("DoFast");
    doDisplay      = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip   = this->findValueInSettings<double>("UpdateChipCfg");
    saveBinaryData = this->findValueInSettings<double>("SaveBinaryData");

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

void Gain::draw(bool doSaveData)
{
    if(doSaveData == true) Gain::saveChipRegisters(theCurrentRun);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    if((doSaveData == true) && ((this->fResultFile == nullptr) || (this->fResultFile->IsOpen() == false)))
    {
        this->InitResultFile(fileRes);
        LOG(INFO) << BOLDBLUE << "\t--> Gain saving histograms..." << RESET;
    }

    histos->book(this->fResultFile, *fDetectorContainer, fSettingsMap);
    Gain::fillHisto();
    histos->process();
    saveData = doSaveData;

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
    float slope, slopeErr, intercept, interceptErr, lowQslope, lowQslopeErr, lowQintercept, lowQinterceptErr, chi2, DoF;

    std::vector<float> par(NGAINPAR, 0);
    std::vector<float> parErr(NGAINPAR, 0);

    std::vector<float> x(dacList.size(), 0);
    std::vector<float> y(dacList.size(), 0);
    std::vector<float> e(dacList.size(), 0);
    std::vector<float> o(dacList.size(), 0);

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
                                    o[i] = detectorContainerVector[i]
                                               ->at(cBoard->getIndex())
                                               ->at(cOpticalGroup->getIndex())
                                               ->at(cHybrid->getIndex())
                                               ->at(cChip->getIndex())
                                               ->getChannel<OccupancyAndPh>(row, col)
                                               .fOccupancy;
                                }

                                // ##################
                                // # Run regression #
                                // ##################
                                Gain::computeStats(x, y, e, o, par, parErr, chi2, DoF);
                                intercept        = par[0];
                                interceptErr     = parErr[0];
                                slope            = par[1];
                                slopeErr         = parErr[1];
                                lowQintercept    = par[2];
                                lowQinterceptErr = parErr[2];
                                lowQslope        = par[3];
                                lowQslopeErr     = parErr[3];

                                if(chi2 != 0)
                                {
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fIntercept =
                                        intercept;
                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fInterceptError = interceptErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fSlope =
                                        slope;
                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fSlopeError =
                                        slopeErr;

                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fInterceptLowQ = lowQintercept;
                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fInterceptLowQError = lowQinterceptErr;

                                    theGainContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<GainFit>(row, col).fSlopeLowQ =
                                        lowQslope;
                                    theGainContainer->at(cBoard->getIndex())
                                        ->at(cOpticalGroup->getIndex())
                                        ->at(cHybrid->getIndex())
                                        ->at(cChip->getIndex())
                                        ->getChannel<GainFit>(row, col)
                                        .fSlopeLowQError = lowQslopeErr;

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
                    float ToTatTarget = Gain::gainFunction({cChip->getSummary<GainFit, GainFit>().fIntercept, cChip->getSummary<GainFit, GainFit>().fSlope}, targetCharge);

                    if(ToTatTarget >= RD53Shared::setBits(RD53Constants::NBIT_TDAC))
                        LOG(INFO) << GREEN << "Average ToT for [board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                                  << +cChip->getId() << RESET << GREEN << "] at VCal = " << BOLDYELLOW << std::fixed << std::setprecision(2) << targetCharge << RESET << GREEN << " (" << BOLDYELLOW
                                  << RD53chargeConverter::VCal2Charge(targetCharge) << RESET << GREEN << " electrons) is greater than " << BOLDYELLOW
                                  << RD53Shared::setBits(RD53Constants::NBIT_TDAC) - 1 << RESET << GREEN << " (ToT)" << std::setprecision(-1) << RESET;
                    else
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

void Gain::computeStats(const std::vector<float>& x,
                        const std::vector<float>& y,
                        const std::vector<float>& e,
                        const std::vector<float>& o,
                        std::vector<float>&       par,
                        std::vector<float>&       parErr,
                        float&                    chi2,
                        float&                    DoF)
// #######################################################
// # Linear regression with least-square method          #
// # Model: y = f(x) = [0] + [1]*x + [2]*x^2 + [3]*ln(x) #
// #######################################################
{
    int nPar  = NGAINPAR - 2; // @TMP@
    int nData = 0;

    for(auto i = 0u; i < x.size(); i++)
        if((e[i] != 0) && (o[i] == 1)) nData++;

    do
    {
        chi2 = 0;
        DoF  = nData - nPar;
        for(auto c = 0; c < NGAINPAR; c++)
        {
            par[c]    = 0;
            parErr[c] = 0;
        }
        if(DoF < 1) return;

        ublas::matrix<double> H(nData, nPar);
        ublas::matrix<double> V(nData, nData);
        ublas::vector<double> myY(nData);

        for(auto c = 0u; c < V.size2(); c++)
            for(auto r = 0u; r < V.size1(); r++) V(r, c) = 0;

        int r = 0;
        for(auto i = 0u; i < x.size(); i++)
            if((e[i] != 0) && (o[i] == 1))
            {
                H(r, 0) = 1;
                H(r, 1) = x[i];
                // @TMP@
                // if(H.size2() > 2) H(r, 2) = x[i] * x[i];
                // if(H.size2() > 3) H(r, 3) = log(x[i]);

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
            for(auto c = 0; c < nPar; c++) parErr[c] = sqrt(parCov(c, c));

            // ################
            // # Compute chi2 #
            // ################
            ublas::vector<double> num(myY - ublas::prod(H, myPar));
            ublas::vector<double> tmpNum(ublas::prod(invV, num));
            chi2 = ublas::inner_prod(num, tmpNum);
        }

        if((chi2 == 0) || ((nPar == 4) && (par[3] - parErr[3] < 0))) // @TMP@
            nPar--;
        else
            break;

    } while(nPar > 1);

    // #############################################
    // # Extract best estimate of low-charge range # // @TMP@
    // #############################################
    auto itHi = std::find_if(o.begin(), o.end(), [&](double val) { return val == 1.0; }) - o.begin();
    auto itLo = std::find_if(o.begin(), o.end(), [&](double val) { return val >= 0.1; }) - o.begin();
    if((itHi != itLo) && (itHi != static_cast<int>(o.size())) && (itLo != static_cast<int>(o.size())) && ((x[itHi] - x[itLo]) != 0))
    {
        par[NGAINPAR - 1] = (y[itHi] - y[itLo]) / (x[itHi] - x[itLo]);
        par[NGAINPAR - 2] = y[itHi] - par[NGAINPAR - 1] * x[itHi];
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
