/*!
  \file                  RD53EyeScanOptimizationHistograms.cc
  \brief                 Implementation of data readback optimization histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53EyeScanOptimizationHistograms.h"

using namespace Ph2_HwDescription;

void EyeScanOptimizationHistograms::book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap)
{
    ContainerFactory::copyStructure(theDetectorStructure, DetectorData);

    // #######################
    // # Retrieve parameters #
    // #######################
    startValueTAP0 = this->findValueInSettings<double>(settingsMap, "TAP0Start");
    stopValueTAP0  = this->findValueInSettings<double>(settingsMap, "TAP0Stop");
    startValueTAP1 = this->findValueInSettings<double>(settingsMap, "TAP1Start");
    stopValueTAP1  = this->findValueInSettings<double>(settingsMap, "TAP1Stop");
    startValueTAP2 = this->findValueInSettings<double>(settingsMap, "TAP2Start");
    stopValueTAP2  = this->findValueInSettings<double>(settingsMap, "TAP2Stop");

    std::vector<uint16_t> dacListTAP0;
    size_t                nStepsTAP0 = (stopValueTAP0 - startValueTAP0 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP0 - startValueTAP0 + 1);
    size_t                nStepsTAP1 = (stopValueTAP1 - startValueTAP1 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP1 - startValueTAP1 + 1);
    size_t                nStepsTAP2 = (stopValueTAP2 - startValueTAP2 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP2 - startValueTAP2 + 1);

    size_t step = floor((stopValueTAP0 - startValueTAP0 + 1) / nStepsTAP0);
    for(auto i = 0u; i < nStepsTAP0; i++) dacListTAP0.push_back(startValueTAP0 + step * i);

    for(auto& obs: observables)
    {
        TAP0scan[obs]   = new DetectorDataContainer;
        TAP1scan[obs]   = new DetectorDataContainer;
        TAP2scan[obs]   = new DetectorDataContainer;
        ThreeDscan[obs] = new DetectorDataContainer;

        auto hTAP0scan = CanvasContainer<TH1F>(("TAP0scan_" + obs).c_str(), ("TAP0 scan " + obs).c_str(), nStepsTAP0, startValueTAP0, stopValueTAP0 + 1);
        bookImplementer(theOutputFile, theDetectorStructure, *TAP0scan[obs], hTAP0scan, "TAP0 - driver ", obs.c_str());

        auto hTAP1scan = CanvasContainer<TH1F>(("TAP1scan_" + obs).c_str(), ("TAP1 scan " + obs).c_str(), nStepsTAP1, startValueTAP1, stopValueTAP1 + 1);
        bookImplementer(theOutputFile, theDetectorStructure, *TAP1scan[obs], hTAP1scan, "TAP1 - pre-emphasis-1", obs.c_str());

        auto hTAP2scan = CanvasContainer<TH1F>(("TAP2scan_" + obs).c_str(), ("TAP2 scan " + obs).c_str(), nStepsTAP2, startValueTAP2, stopValueTAP2 + 1);
        bookImplementer(theOutputFile, theDetectorStructure, *TAP2scan[obs], hTAP2scan, "TAP2 - pre-emphasis-2", obs.c_str());

        auto hTAP3Dscan = CanvasContainer<TH3F>(("TAP012scan_" + obs).c_str(),
                                                ("TAP12 scan " + obs).c_str(),
                                                nStepsTAP0,
                                                startValueTAP0,
                                                stopValueTAP0 + 1,
                                                nStepsTAP1,
                                                startValueTAP1,
                                                stopValueTAP1 + 1,
                                                nStepsTAP2,
                                                startValueTAP2,
                                                stopValueTAP2 + 1);
        bookImplementer(theOutputFile, theDetectorStructure, *ThreeDscan[obs], hTAP3Dscan, "TAP0 - driver", "TAP1 - pre-emphasis-1", "TAP2 - pre-emphasis-2");
    }
}

bool EyeScanOptimizationHistograms::fill(std::vector<char>& dataBuffer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    ChipContainerStream<EmptyContainer, GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>> theTAP0scanStreamer("TAP0scan");
    ChipContainerStream<EmptyContainer, GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>> theTAP1scanStreamer("TAP1scan");
    ChipContainerStream<EmptyContainer, GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>> theTAP2scanStreamer("TAP2scan");
    ChipContainerStream<EmptyContainer, GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>> theTAP3DscanStreamer("TAP2scan");

    if(theTAP0scanStreamer.attachBuffer(&dataBuffer))
    {
        theTAP0scanStreamer.decodeChipData(DetectorData);
        EyeScanOptimizationHistograms::fillScanTAP0(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTAP1scanStreamer.attachBuffer(&dataBuffer))
    {
        theTAP1scanStreamer.decodeChipData(DetectorData);
        EyeScanOptimizationHistograms::fillScanTAP1(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTAP2scanStreamer.attachBuffer(&dataBuffer))
    {
        theTAP2scanStreamer.decodeChipData(DetectorData);
        EyeScanOptimizationHistograms::fillScanTAP2(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }
    else if(theTAP3DscanStreamer.attachBuffer(&dataBuffer))
    {
        theTAP3DscanStreamer.decodeChipData(DetectorData);
        EyeScanOptimizationHistograms::fillScan3D(DetectorData);
        DetectorData.cleanDataStored();
        return true;
    }

    return false;
}

void EyeScanOptimizationHistograms::fillScanTAP0(const DetectorDataContainer& TAP0scanContainer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: TAP0scanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto& obs: observables)
                    {
                        if(cChip->getSummaryContainer<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>() == nullptr) continue;

                        auto* TAP0scanHist =
                            TAP0scan[obs]->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                        for(auto i = 0; i < TAP0scanHist->GetNbinsX(); i++)
                        {
                            TAP0scanHist->SetBinContent(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(0));
                            TAP0scanHist->SetBinError(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(5));
                        }
                    }
                }
}

void EyeScanOptimizationHistograms::fillScanTAP1(const DetectorDataContainer& TAP1scanContainer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: TAP1scanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto& obs: observables)
                    {
                        if(cChip->getSummaryContainer<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>() == nullptr) continue;

                        auto* TAP1scanHist =
                            TAP1scan[obs]->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                        for(auto i = 0; i < TAP1scanHist->GetNbinsX(); i++)
                        {
                            TAP1scanHist->SetBinContent(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(0));
                            TAP1scanHist->SetBinError(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(5));
                        }
                    }
                }
}

void EyeScanOptimizationHistograms::fillScanTAP2(const DetectorDataContainer& TAP2scanContainer)
{
    const size_t TAPsize = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;

    for(const auto cBoard: TAP2scanContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto& obs: observables)
                    {
                        if(cChip->getSummaryContainer<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>() == nullptr) continue;

                        auto* TAP2scanHist =
                            TAP2scan[obs]->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH1F>>().fTheHistogram;

                        for(auto i = 0; i < TAP2scanHist->GetNbinsX(); i++)
                        {
                            TAP2scanHist->SetBinContent(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(0));
                            TAP2scanHist->SetBinError(i + 1, cChip->getSummary<GenericDataArray<TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>().data[i][obs].at(5));
                        }
                    }
                }
}

void EyeScanOptimizationHistograms::fillScan3D(const DetectorDataContainer& the3DContainer)
{
    const size_t TAPsize    = RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1;
    size_t       nStepsTAP1 = (stopValueTAP1 - startValueTAP1 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP1 - startValueTAP1 + 1);
    size_t       nStepsTAP2 = (stopValueTAP2 - startValueTAP2 + 1 >= RD53Shared::MAXSTEPS ? RD53Shared::MAXSTEPS : stopValueTAP2 - startValueTAP2 + 1);

    for(const auto cBoard: the3DContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    for(auto& obs: observables)
                    {
                        if(cChip->getSummaryContainer<GenericDataArray<TAPsize * TAPsize * TAPsize>>() == nullptr) continue;

                        auto* TAP3DscanHist =
                            ThreeDscan[obs]->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<CanvasContainer<TH3F>>().fTheHistogram;

                        for(auto i = 0; i < TAP3DscanHist->GetNbinsX(); i++)
                        {
                            for(auto j = 0; j < TAP3DscanHist->GetNbinsX(); j++)
                            {
                                for(auto k = 0; k < TAP3DscanHist->GetNbinsX(); k++)
                                {
                                    TAP3DscanHist->SetBinContent(i + 1,
                                                                 j + 1,
                                                                 k + 1,
                                                                 cChip->getSummary<GenericDataArray<TAPsize * TAPsize * TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>()
                                                                     .data[i + j * nStepsTAP1 + k * nStepsTAP1 * nStepsTAP2][obs]
                                                                     .at(0));
                                    TAP3DscanHist->SetBinError(i + 1,
                                                               j + 1,
                                                               k + 1,
                                                               cChip->getSummary<GenericDataArray<TAPsize * TAPsize * TAPsize, std::unordered_map<std::string, std::array<float, 7>>>>()
                                                                   .data[i + j * nStepsTAP1 + k * nStepsTAP1 * nStepsTAP2][obs]
                                                                   .at(5));
                                }
                            }
                        }
                    }
                }
}

void EyeScanOptimizationHistograms::process()
{
    for(auto& obs: observables)
    {
        draw<TH1F>(*TAP0scan[obs]);
        draw<TH1F>(*TAP1scan[obs]);
        draw<TH1F>(*TAP2scan[obs]);
        draw<TH3F>(*ThreeDscan[obs]);
    }
}
