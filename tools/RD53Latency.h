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
    ~Latency()
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
        return PixelAlive::getNumberIterations() * ((stopValue - startValue) / nTRIGxEvent + 1 <= RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1
                                                        ? (stopValue - startValue) / nTRIGxEvent + 1
                                                        : RD53Shared::setBits(RD53Shared::MAXBITCHIPREG) + 1);
    }

    void analyze();

#ifdef __USE_ROOT__
    LatencyHistograms* histos;
#endif

  private:
    void fillHisto() override;

    void scanDac(const std::string& regName, const std::vector<uint16_t>& dacList, DetectorDataContainer* theContainer);

    std::vector<uint16_t> dacList;
    DetectorDataContainer theOccContainer;
    DetectorDataContainer theLatencyContainer;

  protected:
    size_t startValue;
    size_t stopValue;

    std::string fileRes;
    int         theCurrentRun;
    bool        doUpdateChip;
};

#endif
