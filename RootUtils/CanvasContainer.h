/*!
  \file                  CanvasContainer.h
  \brief                 Header file of canvas container
  \author                Alkiviadis PAPADOPOULOS
  \version               1.0
  \date                  28/06/18
  Support:               email to alkiviadis.papadopoulos@cern.ch
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef CanvasContainer_H
#define CanvasContainer_H

#include "../Utils/Container.h"
#include "PlotContainer.h"

#include <TCanvas.h>
#include <TDirectory.h>

#include <iostream>

template <class Hist>
class CanvasContainer : public PlotContainer
{
  public:
    CanvasContainer() : fTheHistogram(nullptr), fCanvas(nullptr) {}

    CanvasContainer(const CanvasContainer<Hist>& container) = delete;
    CanvasContainer<Hist>& operator=(const CanvasContainer<Hist>& container) = delete;

    template <class... Args>
    CanvasContainer(Args... args)
    {
        fTheHistogram = new Hist(args...);
        fTheHistogram->SetDirectory(0);
        fCanvas = nullptr;
    }

    ~CanvasContainer()
    {
        if(fHasToBeDeletedManually == true)
        {
            delete fTheHistogram;
            if(fCanvas != nullptr) delete fCanvas;
        }

        fTheHistogram = nullptr;
        fCanvas       = nullptr;
    }

    CanvasContainer(CanvasContainer<Hist>&& container)
    {
        fHasToBeDeletedManually = container.fHasToBeDeletedManually;
        fTheHistogram           = container.fTheHistogram;
        container.fTheHistogram = nullptr;
        fCanvas                 = container.fCanvas;
        container.fCanvas       = nullptr;
    }

    CanvasContainer<Hist>& operator=(CanvasContainer<Hist>&& container)
    {
        fHasToBeDeletedManually = container.fHasToBeDeletedManually;
        fTheHistogram           = container.fTheHistogram;
        container.fTheHistogram = nullptr;
        fCanvas                 = container.fCanvas;
        container.fCanvas       = nullptr;
        return *this;
    }

    void initialize(std::string name, std::string title, const PlotContainer* reference) override
    {
        fHasToBeDeletedManually = false;

        fCanvas = new TCanvas(name.data(), title.data());

        fTheHistogram = new Hist(*(static_cast<const CanvasContainer<Hist>*>(reference)->fTheHistogram));
        fTheHistogram->SetName(name.data());
        fTheHistogram->SetTitle(title.data());
        fTheHistogram->SetDirectory(0);

        gDirectory->Append(fCanvas);
    }

    void print(void) { std::cout << "CanvasContainer " << fTheHistogram->GetName() << std::endl; }

    void setNameTitle(std::string histogramName, std::string histogramTitle) override { fTheHistogram->SetNameTitle(histogramName.data(), histogramTitle.data()); }

    std::string getName() const override { return fTheHistogram->GetName(); }

    std::string getTitle() const override { return fTheHistogram->GetTitle(); }

    Hist*    fTheHistogram;
    TCanvas* fCanvas;
};

#endif
