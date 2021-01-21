/*!
  \file                DQMHistogramBase.h
  \brief               base class to create and fill monitoring histograms
  \author              Fabio Ravera, Lorenzo Uplegger
  \version             1.0
  \date                6/5/19
  Support :            mail to : fabio.ravera@cern.ch

*/

#ifndef __DQMHISTOGRAMBASE_H__
#define __DQMHISTOGRAMBASE_H__

#include <memory>
#include <string>
#include <vector>

#include "../RootUtils/CanvasContainer.h"
#include "../RootUtils/HistContainer.h"
#include "../RootUtils/RootContainerFactory.h"
#include "../System/SystemController.h"
#include "../Utils/Container.h"

#include <TCanvas.h>
#include <TFile.h>
#include <TGaxis.h>
#include <TPad.h>

class DetectorDataContainer;
class DetectorContainer;

/*!
 * \class DQMHistogramBase
 * \brief Base class for monitoring histograms
 */
class DQMHistogramBase
{
  public:
    /*!
     * constructor
     */
    DQMHistogramBase() { ; }

    /*!
     * destructor
     */
    virtual ~DQMHistogramBase() { ; }

    /*!
     * \brief Book histograms
     * \param theDetectorStructure : Container of the Detector structure
     */
    virtual void book(TFile* outputFile, const DetectorContainer& theDetectorStructure, const Ph2_System::SettingsMap& pSettingsMap) = 0;

    /*!
     * \brief Book histograms
     * \param configurationFileName : xml configuration file
     */
    virtual bool fill(std::vector<char>& dataBuffer) = 0;

    /*!
     * \brief SAve histograms
     * \param outFile : ouput file name
     */
    virtual void process() = 0;

    /*!
     * \brief Book histograms
     * \param configurationFileName : xml configuration file
     */
    virtual void reset(void) = 0;

  private:
    std::vector<std::unique_ptr<TGaxis>> axes;

  protected:
    template <typename Hist>
    void bookImplementer(TFile*                       theOutputFile,
                         const DetectorContainer&     theDetectorStructure,
                         DetectorDataContainer&       dataContainer,
                         const CanvasContainer<Hist>& histContainer,
                         const char*                  XTitle = nullptr,
                         const char*                  YTitle = nullptr)
    {
        if(XTitle != nullptr) histContainer.fTheHistogram->SetXTitle(XTitle);
        if(YTitle != nullptr) histContainer.fTheHistogram->SetYTitle(YTitle);

        RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, dataContainer, histContainer);
    }

    template <typename Hist>
    void draw(DetectorDataContainer& HistDataContainer, const char* opt = "", bool electronAxis = false, const char* electronAxisTitle = "", bool isNoise = false)
    {
        for(auto cBoard: HistDataContainer)
            for(auto cOpticalGroup: *cBoard)
                for(auto cHybrid: *cOpticalGroup)
                    for(auto cChip: *cHybrid)
                    {
                        TCanvas* canvas = cChip->getSummary<CanvasContainer<Hist>>().fCanvas;
                        Hist*    hist   = cChip->getSummary<CanvasContainer<Hist>>().fTheHistogram;

                        canvas->cd();
                        hist->Draw(opt);
                        canvas->Modified();
                        canvas->Update();

                        if(electronAxis == true)
                        {
                            TPad* myPad = static_cast<TPad*>(canvas->GetPad(0));
                            myPad->SetTopMargin(0.16);

                            axes.emplace_back(new TGaxis(myPad->GetUxmin(),
                                                         myPad->GetUymax(),
                                                         myPad->GetUxmax(),
                                                         myPad->GetUymax(),
                                                         RD53chargeConverter::VCAl2Charge(hist->GetXaxis()->GetBinLowEdge(1), isNoise),
                                                         RD53chargeConverter::VCAl2Charge(hist->GetXaxis()->GetBinLowEdge(hist->GetXaxis()->GetNbins()), isNoise),
                                                         510,
                                                         "-"));
                            axes.back()->SetTitle(electronAxisTitle);
                            axes.back()->SetTitleOffset(1.2);
                            axes.back()->SetTitleSize(0.035);
                            axes.back()->SetTitleFont(40);
                            axes.back()->SetLabelOffset(0.001);
                            axes.back()->SetLabelSize(0.035);
                            axes.back()->SetLabelFont(42);
                            axes.back()->SetLabelColor(kRed);
                            axes.back()->SetLineColor(kRed);
                            axes.back()->Draw();

                            canvas->Modified();
                            canvas->Update();
                        }
                    }
    }

    double findValueInSettings(const Ph2_System::SettingsMap& settingsMap, const std::string name, double defaultValue = 0.) const
    {
        auto setting = settingsMap.find(name);
        return (setting != std::end(settingsMap) ? setting->second : defaultValue);
    }
};

#endif
