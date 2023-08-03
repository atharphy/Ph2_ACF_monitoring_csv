#include "TH1F.h"
#include "TFile.h"
#include "TCanvas.h"
#include "TSystem.h"
#include "TLegend.h"
#include "TStyle.h"
#include "TGraph.h"
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
void Occupancies(int runNumber)
{
    gStyle->SetPalette(1);
    gStyle->SetOptStat(0);
    TCanvas* C  = new TCanvas("Canvas", "", 1200, 1000);
    C->Divide(2,3);
    std::string directoryPath = "Results/OT_ModuleTest_ModuleOT_Run";
    std::string fileName = "Hybrid.root";
    std::string filePath = directoryPath  + std::to_string(runNumber) + "/" + fileName;
    TFile* file = TFile::Open(filePath.c_str(), "READ");
    std::cout << "Processing run number: " << runNumber<< std::endl;
    std::vector<int> hybridList = {0,1};
    for(unsigned int iHybrid=0; iHybrid<hybridList.size(); iHybrid++)
    {
        std::string hybridNumberStr = std::to_string(hybridList[iHybrid]);

        TH1F* h     = (TH1F*)file->Get(("Detector/Board_0/OpticalGroup_0/Hybrid_"+hybridNumberStr+"/D_B(0)_O(0)_HitOccupancyS0_Hybrid("+hybridNumberStr+")").c_str());
        TH1F* h1    = (TH1F*)file->Get(("Detector/Board_0/OpticalGroup_0/Hybrid_"+hybridNumberStr+"/D_B(0)_O(0)_HitOccupancyS1_Hybrid("+hybridNumberStr+")").c_str());
        TH1F* h2    = (TH1F*)file->Get(("Detector/Board_0/OpticalGroup_0/Hybrid_"+hybridNumberStr+"/D_B(0)_O(0)_StubOccupancyS0_Hybrid("+hybridNumberStr+")").c_str());
        C->cd(iHybrid +1);
        h->Draw("colz");
        C->cd(iHybrid +3);
        h1->Draw("colz");
        C->cd(iHybrid +5);
        h2->Draw("colz");
        //C->cd(iHybrid +7); 
        //h3->Draw();
        C->Update();
    }
}
