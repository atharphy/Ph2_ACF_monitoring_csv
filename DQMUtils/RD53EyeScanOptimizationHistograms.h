/*!
  \file                  RD53EyeScanOptimizationHistograms.h
  \brief                 Header file of data readback optimization histograms
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53EyeScanOptimizationHistograms_H
#define RD53EyeScanOptimizationHistograms_H

#include "../System/SystemController.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/RD53Shared.h"
#include "DQMHistogramBase.h"

#include <TH1F.h>
#include <TH3F.h>

class EyeScanOptimizationHistograms : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::vector<char>& dataBuffer) override;
    void reset() override{};

    void fillScanTAP0(const DetectorDataContainer& TAP0scanContainer);
    void fillScanTAP1(const DetectorDataContainer& TAP1scanContainer);
    void fillScanTAP2(const DetectorDataContainer& TAP2scanContainer);
    void fillScan3D(const DetectorDataContainer& the3DContainer);

  private:
    DetectorDataContainer DetectorData;

    std::unordered_map<std::string, DetectorDataContainer*> TAP0scan;
    std::unordered_map<std::string, DetectorDataContainer*> TAP1scan;
    std::unordered_map<std::string, DetectorDataContainer*> TAP2scan;
    std::unordered_map<std::string, DetectorDataContainer*> ThreeDscan;

    size_t                   startValueTAP0;
    size_t                   stopValueTAP0;
    size_t                   startValueTAP1;
    size_t                   stopValueTAP1;
    size_t                   startValueTAP2;
    size_t                   stopValueTAP2;
    std::vector<std::string> observables = {"EHEight", "EWIDth", "JITTer RMS", "QFACtor", "CROSsing"};
};

#endif
