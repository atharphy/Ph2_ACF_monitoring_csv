/*!
  \file                  RD53eudaqProducer.h
  \brief                 Implementaion of EUDAQ producer
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53eudaqProducer_H
#define RD53eudaqProducer_H

#include "RD53Physics.h"
#include "eudaq/CMSITEventData.hh"
#include "eudaq/Producer.hh"
#include "eudaq/RawDataEvent.hh"

#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/vector.hpp"

namespace EUDAQ
{
const std::string EVENT = "CMSIT";
const int         WAIT  = 5000; // [ms]
} // namespace EUDAQ

class RD53eudaqProducer : public eudaq::Producer
{
    class RD53eudaqEvtConverter
    {
      public:
        RD53eudaqEvtConverter(RD53eudaqProducer* eudaqProducer) : eudaqProducer(eudaqProducer) {}
        void operator()(const std::vector<Ph2_HwInterface::RD53Event>& RD53EvtList);

      private:
        RD53eudaqProducer* eudaqProducer;
    };

  public:
    RD53eudaqProducer(Ph2_System::SystemController& RD53SysCntr, const std::string configFile, const std::string producerName, const std::string runControl);

    void OnReset() override;
    void OnInitialise(const eudaq::Configuration& param) override;
    void OnConfigure(const eudaq::Configuration& param) override;
    void OnStartRun(unsigned runNumber) override;
    void OnStopRun() override;
    void OnTerminate() override;

    void MainLoop();
    void MySendEvent(eudaq::Event& theEvent);

    int theRunNumber;
    int evCounter;

  private:
    std::condition_variable wakeUp;
    std::mutex              theMtx;
    bool                    doExit;
    std::string             configFile;
    Physics                 RD53sysCntrPhys;
};

#endif
