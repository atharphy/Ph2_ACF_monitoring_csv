/*!
  \file                  RD53VTRxLightYieldScan.cc
  \brief                 Implementaion of VTRx light yield scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  29/11/24
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53VTRxLightYieldScan.h"
#include "Utils/ContainerSerialization.h"
#include <boost/multiprecision/number.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <boost/numeric/ublas/matrix_proxy.hpp>

using namespace boost::numeric;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void VTRxLightYieldScan::ConfigureCalibration()
{
    // #######################
    // # Retrieve parameters #
    // #######################
    CalibBase::ConfigureCalibration();
    biasStart       = this->findValueInSettings<double>("VTRxBiasStart");
    biasStop        = this->findValueInSettings<double>("VTRxBiasStop");
    biasStep        = this->findValueInSettings<double>("VTRxBiasStep", 1);
    modulationStart = this->findValueInSettings<double>("VTRxModulationStart");
    modulationStop  = this->findValueInSettings<double>("VTRxModulationStop");
    modulationStep  = this->findValueInSettings<double>("VTRxModulationStep", 1);
    doDisplay       = this->findValueInSettings<double>("DisplayHisto");
    doUpdateChip    = this->findValueInSettings<double>("UpdateChipCfg");

    // ##############################
    // # Initialize dac scan values #
    // ##############################
    size_t nSteps = (biasStop - biasStart) / biasStep + 1;
    for(auto i = 0u; i < nSteps; i++) dac1List.push_back(biasStart + biasStep * i);
    nSteps = (modulationStop - modulationStart) / modulationStep + 1;
    for(auto i = 0u; i < nSteps; i++) dac2List.push_back(modulationStart + modulationStep * i);
}

void VTRxLightYieldScan::Running()
{
    CalibBase::theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[VTRxLightYieldScan::Running] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    VTRxLightYieldScan::run();
    VTRxLightYieldScan::analyze();
    VTRxLightYieldScan::draw();
    VTRxLightYieldScan::sendData();
}

void VTRxLightYieldScan::sendData()
{
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("VTRxLightYieldScan");
        theContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theVTRxLightYieldScanContainer);
    }
}

void VTRxLightYieldScan::Stop()
{
    LOG(INFO) << GREEN << "[VTRxLightYieldScan::Stop] Stopping" << RESET;
    CalibBase::Stop();
}

void VTRxLightYieldScan::localConfigure(const std::string& histoFileName, int currentRun)
{
    // ############################
    // # CalibBase localConfigure #
    // ############################
    CalibBase::localConfigure(histoFileName, currentRun);

    histos = nullptr;

    LOG(INFO) << GREEN << "[VTRxLightYieldScan::localConfigure] Starting run: " << BOLDYELLOW << CalibBase::theCurrentRun << RESET;

    // ##########################
    // # Initialize calibration #
    // ##########################
    VTRxLightYieldScan::ConfigureCalibration();

    // ###############################
    // # Initialize output directory #
    // ###############################
    this->CreateResultDirectory(dataOutputDir != "" ? dataOutputDir : RD53Shared::RESULTDIR, false, false);

    // #########################################
    // # Initialize histogram and binary files #
    // #########################################
    CalibBase::initializeFiles(histoFileName, "VTRxLightYieldScan", histos);
}

void VTRxLightYieldScan::run()
{
    ContainerFactory::copyAndInitOpticalGroup<std::vector<float>>(*fDetectorContainer, theVTRxLightYieldScanContainer);

    // ####################
    // # Pause monitoring #
    // ####################
    if(this->fDetectorMonitor != nullptr) this->fDetectorMonitor->pauseMonitoring();

    for(auto cBoard: *fDetectorContainer)
        for(auto cOpticalGroup: *cBoard)
        {
            theVTRxLightYieldScanContainer.getOpticalGroup(cBoard->getId(), cOpticalGroup->getId())->getSummary<std::vector<float>>().clear();

            for(auto i = 0u; i < dac1List.size(); i++)
            {
                if(cOpticalGroup->flpGBT == nullptr) throw std::runtime_error("LpGBT not enabled in configuration file for optical group ID " + std::to_string(cOpticalGroup->getId()));
                this->flpGBTInterface->WriteChipReg(cOpticalGroup->flpGBT, "_I2CVTRxRegCH0BIAS", dac1List[i]);

                for(auto j = 0u; j < dac2List.size(); j++)
                {
                    this->flpGBTInterface->WriteChipReg(cOpticalGroup->flpGBT, "_I2CVTRxRegCH0MOD", dac2List[j] | 0x80);
                    std::this_thread::sleep_for(std::chrono::microseconds(lpGBTconstants::DEEPSLEEP));
                    auto value = static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->GetSFPParameter("RX", flpGBTInterface->GetSFPchannel(cOpticalGroup));

                    // #################
                    // # Progress menu #
                    // #################
                    LOG(INFO) << CYAN << "************* " << GREEN << "Scanning" << CYAN << " *************" << RESET;
                    LOG(INFO) << GREEN << "Bias: " << BOLDYELLOW << std::setw(3) << std::fixed << dac1List[i] << "/" << std::setw(3) << std::fixed << dac1List[dac1List.size() - 1] << RESET << GREEN
                              << " -- Modulation: " << BOLDYELLOW << std::setw(3) << std::fixed << dac2List[j] << "/" << std::setw(3) << std::fixed << dac2List[dac2List.size() - 1] << RESET;
                    LOG(INFO) << CYAN << "************************************" << RESET;
                    if((i < dac1List.size() - 1) || (j < dac2List.size() - 1)) std::cout << std::setprecision(-1) << "\x1b[A\x1b[A\x1b[A";

                    theVTRxLightYieldScanContainer.getOpticalGroup(cBoard->getId(), cOpticalGroup->getId())->getSummary<std::vector<float>>().push_back(value);
                }
            }
        }

    // #####################
    // # Resume monitoring #
    // #####################
    if(this->fDetectorMonitor != nullptr) this->fDetectorMonitor->resumeMonitoring();
}

void VTRxLightYieldScan::draw(bool saveData)
{
    if(saveData == true) CalibBase::saveChipRegisters(doUpdateChip);

#ifdef __USE_ROOT__
    TApplication* myApp = nullptr;

    if(doDisplay == true) myApp = new TApplication("myApp", nullptr, nullptr);

    CalibBase::bookHistoSaveMetadata(histos);
    VTRxLightYieldScan::fillHisto();
    histos->process();

    if(doDisplay == true) myApp->Run(true);
#endif
}

std::shared_ptr<DetectorDataContainer> VTRxLightYieldScan::analyze()
{
    bool False       = false;
    summaryContainer = std::make_shared<DetectorDataContainer>();
    ContainerFactory::copyAndInitOpticalGroup<bool>(*fDetectorContainer, *summaryContainer, False);

    std::vector<float> measurements1(dac1List.size(), 0);
    std::vector<float> measurements2(dac2List.size(), 0);

    for(const auto cBoard: theVTRxLightYieldScanContainer)
        for(const auto cOpticalGroup: *cBoard)
        {
            if(cOpticalGroup->getSummary<std::vector<float>>().size() == 0) continue;
            float slope1, sloErr1;
            float slope2, sloErr2;
            float chi21, DoF1;
            float chi22, DoF2;

            // #############################
            // # Evaluate slope along dac1 #
            // #############################
            auto midPoint = dac2List.size() / 2;
            for(auto i = 0u; i < dac1List.size(); i++) measurements1[i] = cOpticalGroup->getSummary<std::vector<float>>().at(i * dac2List.size() + midPoint);
            VTRxLightYieldScan::computeStats(dac1List, measurements1, slope1, sloErr1, chi21, DoF1);

            // #############################
            // # Evaluate slope along dac2 #
            // #############################
            midPoint = dac1List.size() / 2;
            for(auto j = 0u; j < dac2List.size(); j++) measurements2[j] = cOpticalGroup->getSummary<std::vector<float>>().at(midPoint * dac2List.size() + j);
            VTRxLightYieldScan::computeStats(dac2List, measurements2, slope2, sloErr2, chi22, DoF2);

            // ##########
            // # Result #
            // ##########
            if((slope1 / sloErr1 > 1) && (slope2 / sloErr2 < -1)) summaryContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<bool>() = true;
            LOG(INFO) << GREEN << "VTRx+ [board/opticalGroup = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << RESET << GREEN << std::setprecision(2)
                      << "] has slope along x = " << BOLDYELLOW << slope1 << "+/-" << sloErr1 << RESET << GREEN << " and slope along y = " << BOLDYELLOW << slope2 << "+/-" << sloErr2 << RESET << GREEN
                      << " --> " << (summaryContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<bool>() == true ? BOLDYELLOW : BOLDRED)
                      << (summaryContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getSummary<bool>() == true ? "GOOD" : "BAD") << std::setprecision(-1) << RESET;
        }

    return summaryContainer;
}

void VTRxLightYieldScan::fillHisto()
{
#ifdef __USE_ROOT__
    histos->fillIntensity(theVTRxLightYieldScanContainer);
#endif
}

void VTRxLightYieldScan::computeStats(const std::vector<uint16_t>& x, const std::vector<float>& y, float& slope, float& sloErr, float& chi2, float& DoF)
{
    chi2               = -1;
    slope              = 0;
    sloErr             = 0;
    const size_t nData = x.size();
    const size_t nPar  = 2;
    DoF                = nData - nPar;
    if(DoF < 1) return;

    // ########################
    // # Compose error vector #
    // ########################
    std::vector<double> e(nData);
    std::transform(y.begin(), y.end(), e.begin(), [](double y) { return sqrt(y); });

    // ################################################
    // # Declare matrices and vector for minimization #
    // ################################################
    ublas::matrix<double> H(nData, nPar, 0);
    ublas::matrix<double> V(nData, nData, 0);
    ublas::vector<double> myY(nData);

    // ########################
    // # Declare columns of H #
    // ########################
    ublas::vector<double> col0(nData, 0);
    ublas::vector<double> col1(nData, 0);

    // #####################
    // # Fill columns of H #
    // #####################
    std::vector<double> ones(nData, 1);
    std::copy(ones.begin(), ones.end(), col0.begin());
    std::copy(x.begin(), x.end(), col1.begin());

    // #############
    // # Compose H #
    // #############
    column(H, 0) = col0;
    column(H, 1) = col1;

    // #############
    // # Compose V #
    // #############
    ublas::identity_matrix<double> identityMatrix(nData);
    ublas::vector<double>          identityVector(nData, 1);
    ublas::vector<double>          e2(e.size());
    std::copy(e.begin(), e.end(), e2.begin());
    std::transform(e2.begin(), e2.end(), e2.begin(), [](double x) { return x * x; });
    V = ublas::element_prod(ublas::outer_prod(identityVector, e2), identityMatrix);

    // ############
    // # Fill myY #
    // ############
    std::copy(y.begin(), y.end(), myY.begin());

    // ################
    // # Minimization #
    // ################
    auto invV(V);
    for(auto i = 0u; i < nData; i++) invV(i, i) = 1 / V(i, i);

    ublas::matrix<double> tmpMtx(ublas::prod(invV, H));
    ublas::matrix<double> invParCov(ublas::prod(ublas::trans(H), tmpMtx));
    auto                  parCov(invParCov);

    auto det = RD53Shared::mtxInversion<double>(invParCov, parCov);
    if((isnan(det) == false) && (det != 0))
    {
        ublas::vector<double> tmpVec1(ublas::prod(invV, myY));
        ublas::vector<double> tmpVec2(ublas::prod(ublas::trans(H), tmpVec1));
        ublas::vector<double> myPar(ublas::prod(parCov, tmpVec2));

        // ###################
        // # Save parameters #
        // ###################
        slope  = myPar[1];
        sloErr = sqrt(parCov(1, 1));

        // ################
        // # Compute chi2 #
        // ################
        ublas::vector<double> num(myY - ublas::prod(H, myPar));
        ublas::vector<double> tmpNum(ublas::prod(invV, num));
        chi2 = ublas::inner_prod(num, tmpNum);
    }
}
