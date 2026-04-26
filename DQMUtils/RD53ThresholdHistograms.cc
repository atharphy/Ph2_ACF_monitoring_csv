/*!
  \file                  RD53ThresholdHistograms.cc
  \brief                 Implementation of Threshold calibration histograms
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53ThresholdHistograms.h"

using namespace Ph2_HwDescription;

void ThresholdHistograms::book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap)
{
    fDetectorContainer = &theDetectorStructure;
    RD53Shared::setFirstChip(theDetectorStructure);

    // #######################
    // # Retrieve parameters #
    // #######################
    auto           frontEnd       = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNCols() / 2, RD53Shared::firstChip->getNCols() / 2);
    const uint16_t rangeThreshold = RD53Shared::setBits(RD53Shared::firstChip->getNumberOfBits(frontEnd->thresholdRegs[0])) + 1;

    std::vector<CanvasContainer<TH1F>> hThresholds;
    for(const auto& regName: frontEnd->thresholdRegs) hThresholds.emplace_back(regName, "Threshold", rangeThreshold, 0, rangeThreshold);
    for(const auto& [hThr, regName]: boost::combine(hThresholds, frontEnd->thresholdRegs))
    {
        Thresholds.push_back(std::make_shared<DetectorDataContainer>());
        bookChipImplementer(theOutputFile, theDetectorStructure, *Thresholds.back(), hThr, regName, "Entries");
    }

    AreHistoBooked = true;
}

bool ThresholdHistograms::fill(std::string& inputStream)
{
    ContainerSerialization theContainerSerialization("ThrAdjustmentThreshold");

    if(theContainerSerialization.attachDeserializer(inputStream))
    {
        DetectorDataContainer fDetectorData = theContainerSerialization.deserializeChipContainer<EmptyContainer, std::vector<uint16_t>>(fDetectorContainer);
        ThresholdHistograms::fill(fDetectorData);
        return true;
    }
    return false;
}

void ThresholdHistograms::fill(const DetectorDataContainer& DataContainer)
{
    auto frontEnd = RD53Shared::firstChip->getFEtype(RD53Shared::firstChip->getNCols() / 2, RD53Shared::firstChip->getNCols() / 2);

    for(const auto cBoard: DataContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
                for(const auto cChip: *cHybrid)
                {
                    if(cChip->hasSummary() == false) continue;

                    for(unsigned int i = 0u; i < frontEnd->thresholdRegs.size(); i++)
                    {
                        auto* hThreshold = Thresholds.at(i)
                                               ->getObject(cBoard->getId())
                                               ->getObject(cOpticalGroup->getId())
                                               ->getObject(cHybrid->getId())
                                               ->getObject(cChip->getId())
                                               ->getSummary<CanvasContainer<TH1F>>()
                                               .fTheHistogram;

                        hThreshold->Fill(cChip->getSummary<std::vector<uint16_t>>().at(i));
                    }
                }
}

void ThresholdHistograms::process()
{
    for(auto& ThrPtr: Thresholds) drawChip<TH1F>(*ThrPtr);
}
