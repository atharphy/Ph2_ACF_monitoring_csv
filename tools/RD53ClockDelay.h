/*!
  \file                  RD53ClockDelay.h
  \brief                 Implementaion of Clock Delay scan
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53ClockDelay_H
#define RD53ClockDelay_H

#include "RD53Latency.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53ClockDelayHistograms.h"
#endif

// ##########################
// # Clock delay test suite #
// ##########################
class ClockDelay : public PixelAlive
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
    ClockDelayHistograms* histos;
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
    bool    doFast;

    std::vector<uint16_t> dacList;

    DetectorDataContainer theOccContainer;
    DetectorDataContainer theClockDelayContainer;

    void fillHisto();
    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, uint32_t nEvents, DetectorDataContainer* theContainer);
    void chipErrorReport() const;
    void writeSequence(const Ph2_HwDescription::BeBoard* pBoard, Ph2_HwDescription::ReadoutChip* pChip, uint16_t clk_data_delay);

  protected:
    std::string fileRes;
    int         theCurrentRun;
    uint16_t    maxClkDelay;
    uint16_t    maxCmdDelay;
    bool        doUpdateChip;
    bool        doDisplay;
    bool        saveBinaryData;
};

#endif
