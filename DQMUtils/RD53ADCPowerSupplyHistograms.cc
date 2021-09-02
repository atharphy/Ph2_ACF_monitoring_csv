/*!
  \file                  RD53ADCPowerSupplyHistograms.cc
  \brief                 Implementation of ADCPowerSupply histograms
  \author                Umberto MOLINATTI
  \version               1.0
  \date                  19/04/21
  Support:               email to umberto.molinatti@cern.ch
*/

#include "RD53ADCPowerSupplyHistograms.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ADCPowerSupplyHistograms::fillPS(DetectorContainer& DataContainer, double fitStart, double fitEnd, double* VMUXvolt, double* ADCcode)
{
    auto canvas = new TCanvas();
    canvas->Print("Results/plots.pdf[");
    remove("Results/calibration.root"); // Remove old file
    TFile* file = new TFile("Results/calibration.root", "new");
    // for(const auto cBoard: *fDetectorContainer)
    // for(const auto cOpticalGroup: *cBoard)
    // for(const auto cHybrid: *cOpticalGroup)
    // for(const auto cChip: *cHybrid){
    // ChipRegMap& pRD53RegMap = static_cast<RD53*>(cChip)->getRegMap();
    // TGraph *linear = new TGraph (pow(2.0,pRD53RegMap["VCAL_HIGH"].fBitSize), VMUXvolt, ADCcode);
    LOG(INFO) << "fitStart=" << BOLDWHITE << fitStart << RESET;
    LOG(INFO) << "fitEnd=" << BOLDWHITE << fitEnd << RESET;
    TGraph* linear = new TGraph(int(0.9 / 0.0002), VMUXvolt, ADCcode);
    linear->SetTitle("Linear Graph;Voltage;ADC Code");
    TFitResultPtr r = linear->Fit("pol1", "S", "", fitStart, fitEnd);
    gStyle->SetOptFit();
    gStyle->SetFitFormat(".2g");
    linear->Draw("APL");
    canvas->Print("Results/plots.pdf");
    remove("Results/calibration.txt"); // Remove old file
    std::ofstream outfile;
    outfile.open("Results/calibration.txt", std::ios_base::app);
    outfile << "Intercept: " << r->Value(0) << "\nSlope: " << r->Value(1) << "\nMax: " << (r->Value(0)) + (r->Value(1)) * (fitEnd);
    linear->Write();
    //			}
    file->Write();
    canvas->Print("Results/plots.pdf]");
}
