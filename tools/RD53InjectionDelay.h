/*!
  \file                  RD53InjectionDelay.h
  \brief                 Implementaion of Injection Delay scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53InjectionDelay_H
#define RD53InjectionDelay_H

#include "RD53Latency.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53InjectionDelayHistograms.h"
#endif

// ##############################
// # Injection delay test suite #
// ##############################
class InjectionDelay : public PixelAlive
{
  public:
    ~InjectionDelay()
    {
#ifdef __USE_ROOT__
        this->WriteRootFile();
        this->CloseResultFile();
        delete histos;
#endif
    }

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string& fileRes_ = "", int currentRun = -1) override;
    void   initializeFiles(const std::string& fileRes_ = "", int currentRun = -1) override;
    void   run() override;
    void   draw(bool saveData = true) override;
    size_t getNumberIterations() override
    {
        return PixelAlive::getNumberIterations() *
               (stopValue - startValue + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1 ? stopValue - startValue + 1 : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    }

    void analyze();

#ifdef __USE_ROOT__
    InjectionDelayHistograms* histos;
#endif

  private:
    void fillHisto() override;

    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer);

    Latency               la;
    std::vector<uint16_t> dacList;
    DetectorDataContainer theOccContainer;
    DetectorDataContainer theInjectionDelayContainer;

  protected:
    size_t startValue;
    size_t stopValue;

    std::string fileRes;
    int         theCurrentRun;
    size_t      saveInjection;
    size_t      maxDelay;
};

#endif
