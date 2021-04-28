/*!
  \file                  RD53Latency.h
  \brief                 Implementaion of Latency scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Latency_H
#define RD53Latency_H

#include "RD53PixelAlive.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53LatencyHistograms.h"
#endif

// ######################
// # Latency test suite #
// ######################
class Latency : public PixelAlive
{
  public:
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void sendData() override;

    void   localConfigure(const std::string fileRes_, int currentRun);
    void   initializeFiles(const std::string fileRes_, int currentRun);
    void   run();
    void   draw(bool saveData = true);
    void   analyze();
    size_t getNumberIterations()
    {
        return PixelAlive::getNumberIterations() * ((stopValue - startValue) / nTRIGxEvent + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1
                                                        ? (stopValue - startValue) / nTRIGxEvent + 1
                                                        : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    }
    void saveChipRegisters(int currentRun);

#ifdef __USE_ROOT__
    LatencyHistograms* histos;
#endif

  private:
    size_t rowStart;
    size_t rowStop;
    size_t colStart;
    size_t colStop;
    size_t startValue;
    size_t stopValue;
    size_t nEvents;
    size_t nTRIGxEvent;

    std::vector<uint16_t> dacList;

    DetectorDataContainer theOccContainer;
    DetectorDataContainer theLatencyContainer;

    void fillHisto();
    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer);
    void chipErrorReport() const;

  protected:
    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
