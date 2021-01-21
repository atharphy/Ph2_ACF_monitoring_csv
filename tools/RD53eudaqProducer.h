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
#include "eudaq/Producer.hh"

class RD53eudaqProducer : public eudaq::Producer
{
    class RD53eudaqEvtConverter
    {
      public:
        RD53eudaqEvtConverter(RD53eudaqProducer* eudaqProducer) : eudaqProducer(eudaqProducer) {}
        void operator()(const std::vector<Ph2_HwInterface::RD53FWInterface::Event>& RD53EvtList);

      private:
        RD53eudaqProducer* eudaqProducer;
    };

  public:
    RD53eudaqProducer(Ph2_System::SystemController& RD53SysCntr, const std::string configFile, const std::string producerName, const std::string runControl);

    void DoReset() override;
    void DoInitialise() override;
    void DoConfigure() override;
    void DoStartRun() override;
    void DoStopRun() override;
    void DoTerminate() override;

  private:
    std::string configFile;
    Physics     RD53sysCntrPhys;
    int         currentRun;
};

// ##################################
// # Call to EUDAQ producer factory #
// ##################################
namespace
{
__attribute__((unused)) auto dummy =
    eudaq::Factory<eudaq::Producer>::Register<RD53eudaqProducer, Ph2_System::SystemController&, const std::string, const std::string, const std::string>(eudaq::cstr2hash("RD53eudaqProducer"));
}

#endif
