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

#include <boost/any.hpp>
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

namespace user_detail
{
template <typename>
struct sfinae_true_DQMHistogramBase : std::true_type
{
};

template <typename T>
static auto test_SetZTitle(int) -> sfinae_true_DQMHistogramBase<decltype(std::declval<T>().SetZTitle(""))>;
template <typename>
static auto test_SetZTitle(long) -> std::false_type;
} // namespace user_detail

// SFINAE: check if object T has SetZTitle
template <typename T>
struct has_SetZTitle : decltype(user_detail::test_SetZTitle<T>(0))
{
};

// Functor for SetZTitle - default case
template <typename T, bool hasSetZTitle = false>
struct CallSetZTitle
{
    void operator()(T* thePlot, const char* theTitle) { return; }
};

// Functor for SetZTitle - case when SetZTitle is defined
template <typename T>
struct CallSetZTitle<T, true>
{
    void operator()(T* thePlot, const char* theTitle)
    {
        thePlot->SetZTitle(theTitle);
        return;
    }
};

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
                         const char*                  YTitle = nullptr,
                         const char*                  ZTitle = nullptr)
    {
        if(XTitle != nullptr) histContainer.fTheHistogram->GetXaxis()->SetTitle(XTitle);
        if(YTitle != nullptr) histContainer.fTheHistogram->GetYaxis()->SetTitle(YTitle);
        if(ZTitle != nullptr)
        {
            CallSetZTitle<Hist, has_SetDirectory<Hist>::value> setZTitleFunctor;
            setZTitleFunctor(histContainer.fTheHistogram, ZTitle);
        }

        RootContainerFactory::bookChipHistograms(theOutputFile, theDetectorStructure, dataContainer, histContainer);
    }

    template <typename Hist>
    void draw(DetectorDataContainer& HistDataContainer, const char* opt = "", const std::string additionalAxisType = "", const char* additionalAxisTitle = "", bool isNoise = false)
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

                        if(additionalAxisType != "")
                        {
                            TPad* myPad = static_cast<TPad*>(canvas->GetPad(0));
                            myPad->SetTopMargin(0.16);

                            if(additionalAxisType == "electron")
                                axes.emplace_back(new TGaxis(myPad->GetUxmin(),
                                                             myPad->GetUymax(),
                                                             myPad->GetUxmax(),
                                                             myPad->GetUymax(),
                                                             RD53chargeConverter::VCal2Charge(hist->GetXaxis()->GetBinLowEdge(1), isNoise),
                                                             RD53chargeConverter::VCal2Charge(hist->GetXaxis()->GetBinLowEdge(hist->GetXaxis()->GetNbins()), isNoise),
                                                             510,
                                                             "-"));
                            else if(additionalAxisType == "frequency") // @TMP@
                            {
                                axes.emplace_back(new TGaxis(myPad->GetUxmax(),
                                                             myPad->GetUymin(),
                                                             myPad->GetUxmax(),
                                                             myPad->GetUymax(),
                                                             RD53FWconstants::CDR2Freq(hist->GetYaxis()->GetBinLowEdge(1)),
                                                             RD53FWconstants::CDR2Freq(hist->GetYaxis()->GetBinLowEdge(hist->GetYaxis()->GetNbins())),
                                                             510,
                                                             "+L"));

                                axes.back()->SetTitle("Frequency (MHz)");
                                axes.back()->SetTitleOffset(1.4);
                                axes.back()->SetTitleSize(0.035);
                                axes.back()->SetTitleFont(40);
                                axes.back()->SetLabelOffset(0.01);
                                axes.back()->SetLabelSize(0.035);
                                axes.back()->SetLabelFont(42);
                                axes.back()->SetLabelColor(kRed);
                                axes.back()->SetLineColor(kRed);
                                axes.back()->Draw();

                                axes.emplace_back(new TGaxis(myPad->GetUxmin(),
                                                             myPad->GetUymax(),
                                                             myPad->GetUxmax(),
                                                             myPad->GetUymax(),
                                                             RD53FWconstants::VDDD2Volt(hist->GetXaxis()->GetBinLowEdge(1)),
                                                             RD53FWconstants::VDDD2Volt(hist->GetXaxis()->GetBinLowEdge(hist->GetXaxis()->GetNbins())),
                                                             510,
                                                             "-"));
                            }

                            axes.back()->SetTitle(additionalAxisTitle);
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

    template <typename T>
    T findValueInSettings(const Ph2_System::SettingsMap& settingsMap, const std::string name, T defaultValue = T()) const
    {
        auto setting = settingsMap.find(name);
        return (setting != std::end(settingsMap) ? boost::any_cast<T>(setting->second) : defaultValue);
    }
};

#endif
