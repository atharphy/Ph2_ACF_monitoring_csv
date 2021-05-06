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
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string fileRes_, int currentRun);
    void   initializeFiles(const std::string fileRes_, int currentRun);
    void   run();
    void   draw();
    void   analyze();
    size_t getNumberIterations()
    {
        return PixelAlive::getNumberIterations() *
               (stopValue - startValue + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1 ? stopValue - startValue + 1 : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    InjectionDelayHistograms* histos;
#endif

  private:
    Latency la;
    size_t  rowStart;
    size_t  rowStop;
    size_t  colStart;
    size_t  colStop;
    size_t  startValue;
    size_t  stopValue;
    size_t  nEvents;

    std::vector<uint16_t> dacList;

    DetectorDataContainer theOccContainer;
    DetectorDataContainer theInjectionDelayContainer;

    void fillHisto();
    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    size_t      saveInjection;
    size_t      maxDelay;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
