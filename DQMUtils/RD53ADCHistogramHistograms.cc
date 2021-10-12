/*!
  \file                  RD53ADCHistogramHistograms.cc
  \brief                 Implementation of ADCHistogram histograms
  \author                Umberto MOLINATTI
  \version               1.0
  \date                  19/04/21
  Support:               email to umberto.molinatti@cern.ch
*/

#include "RD53ADCHistogramHistograms.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void ADCHistogramHistograms::fillHH(std::vector<double> ADCcode, int max_counts)
{
    auto canvas = new TCanvas();
    remove("Results/ADCHistogram.root"); // Remove old file
    TFile* file = new TFile("Results/ADCHistogram.root", "new");
    canvas->Print("Results/ADCHistogram.pdf[");
    auto spad1 = new TPad("spad1", "The first subpad", 0, 0.8, 1, 1);
    auto spad2 = new TPad("spad2", "The second subpad", 0, 0.4, 1, 0.8);
    auto spad3 = new TPad("spad3", "The third subpad", 0, 0, 1, 0.4);
    spad1->Draw();
    spad2->Draw();
    spad3->Draw();
    spad1->cd();
    // TH1D *histogram = new TH1D("h1","ADC Histogram;ADC Code;Counts",4296,-100,4195);
    TH1D* histogram = new TH1D("h1", "ADC Histogram;ADC Code;Counts", 4094, 1, 4094);
    // TH1D *histogram = new TH1D("h1","ADC Histogram;ADC Code;Counts",4100,1,4100);//stella test

    for(int input = 0; input < max_counts; input++) { histogram->Fill(ADCcode[input]); }
    histogram->Draw();
    // canvas->Print("Results/ADCHistogram.pdf");
    histogram->Write();

    // WIP DNL+INL Calculation, Working fine so far but needs a few stress tests
    // auto dnlCanvas = new TCanvas();
    for(int code = 0; code < 4094; code++)
    {
        codes[code]    = code + 1;
        DNLvalue[code] = 4094 * histogram->GetBinContent(code + 1) / histogram->Integral(1, 4094) - 1;
        if(code == 0)
            INLvalue[0] = 0;
        else
            INLvalue[code] = INLvalue[code - 1] + DNLvalue[code];
        // if(code<10)
        //	std::cout << code << " " << DNLvalue[code] << " " << histogram->GetBinContent(code+1) << " " << histogram->Integral(1,4094) << std::endl;
    }
    spad2->cd();
    TGraph* dnlGraph = new TGraph(4094, codes, DNLvalue);
    dnlGraph->Draw();
    dnlGraph->SetTitle("DNL Graph");
    dnlGraph->SetName("DNL Graph");
    // dnlCanvas->Print("Results/ADCHistogram.pdf");
    // dnlGraph->Write();
    // auto inlCanvas = new TCanvas();
    spad3->cd();
    TGraph* inlGraph = new TGraph(4094, codes, INLvalue);
    inlGraph->Draw();
    inlGraph->SetTitle("INL Graph");
    inlGraph->SetName("INL Graph");
    // inlCanvas->Print("Results/ADCHistogram.pdf");
    // inlGraph->Write();
    canvas->cd();
    canvas->Print("Results/ADCHistogram.pdf");
    histogram->Write();
    dnlGraph->Write();
    inlGraph->Write();

    file->Write();
    canvas->Print("Results/ADCHistogram.pdf]");
}
