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

#include "../user/CMSIT/module/include/CMSITEventData.hh"
#include "RD53Physics.h"
#include "eudaq/Producer.hh"
#include "eudaq/RawEvent.hh"

#include "boost/archive/binary_oarchive.hpp"
#include "boost/serialization/vector.hpp"

namespace EUDAQ
{
const std::string EVENT = "CMSIT";
const int         WAIT  = 5000; // [ms]
const std::string FILERUNNUMBER("./RunNumber.txt");
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
    RD53eudaqProducer(Ph2_System::SystemController& RD53SysCntr, const std::string& configFile, const std::string& producerName, const std::string& runControl);

    void DoReset() override;
    void DoInitialise() override;
    void DoConfigure() override;
    void DoStartRun() override;
    void DoStopRun() override;
    void DoTerminate() override;

    void RunLoop();
    void MySendEvent(eudaq::EventSP theEvent);

    int      theRunNumber;
    int      evCounter;
    uint32_t nTRIGxEvent;

    Physics RD53sysCntrPhys;

    static const uint32_t m_id_factory = eudaq::cstr2hash("RD53eudaqProducer");

  private:
    std::condition_variable wakeUp;
    std::mutex              theMtx;
    bool                    doExit;
    std::string             configFile;
};

#endif
