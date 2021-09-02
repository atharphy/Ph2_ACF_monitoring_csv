/*!
 *
 * \file CicFEAlignment.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \author2 Younes OTARID
 * \date 13 / 11 / 19
 *
 * \Support : sarah.storey@cern.ch
 * \Support2 : younes.otarid@desy.de
 *
 */

////////////////////////////////////////////
// Mauro: needs update to new EUDAQ (9/2021)
////////////////////////////////////////////

#ifndef Eudaq2Producer_h__
#define Eudaq2Producer_h__

#include "Tool.h"

#include <cmath>
#include <map>
#include <memory>
#include <stdlib.h>

#ifdef __USE_ROOT__
#include "TCanvas.h"
#include "TGraphErrors.h"
#include "TH2.h"
#include "TProfile.h"
#include "TProfile2D.h"
#include "TString.h"
#include "TText.h"
#endif

// eudaq stuff
#ifdef __EUDAQ__
#include "eudaq/Configuration.hh"
#include "eudaq/Event.hh"
//#include "eudaq/Factory.hh"
#include "eudaq/Logger.hh"
#include "eudaq/OptionParser.hh"
#include "eudaq/Producer.hh"
//#include "eudaq/RawEvent.hh"
#include "eudaq/RawDataEvent.hh"
#include "eudaq/Time.hh"
#include "eudaq/Utils.hh"
#endif

#ifdef __EUDAQ__
class Eudaq2Producer
    : public Tool
    , public eudaq::Producer
{
  public:
    Eudaq2Producer(const std::string& name, const std::string& runcontrol) : eudaq::Producer(name, runcontrol){};
    ~Eudaq2Producer(){};

    // ph2 acf tool init
    void Initialise();
    void writeObjects();

    // to offload overriden methods a bit
    void ReadoutLoop();
    void ConvertToSubEvent(const Ph2_HwDescription::BeBoard*, const Ph2_HwInterface::Event*, eudaq::RawDataEvent);
    bool EventsPending();

    // override initialization from euDAQ
    /*
    void DoConfigure() override;
    void DoInitialise() override;
    void DoStartRun() override;
    void DoStopRun() override;
    void DoTerminate() override;
    void DoReset() override;
    */
    // void RunLoop() override; //is replaced by ReadOutLoop()

    // register producer in eudaq2
    // static const uint32_t m_id_factory = eudaq::cstr2hash("CMSPhase2Producer");

  protected:
  private:
    // settings
    bool        fHandshakeEnabled;
    uint32_t    fTriggerMultiplicity;
    uint32_t    fHitsCounter;
    std::string fHWFile;
    std::string fRawPh2ACF;

    // status variables
    bool        fInitialised, fConfigured, fStarted, fStopped, fTerminated;
    std::thread fThreadRun;

    // for raw data
    FileHandler* fPh2FileHandler;
    // for s-link data [TBD]
    FileHandler* fSLinkFileHandler;
};

// Register Producer in EUDAQ Factory
// namespace
//{
// auto dummy0 = eudaq::Factory<eudaq::Producer>::Register<Eudaq2Producer, const std::string&, const std::string&>(Eudaq2Producer::m_id_factory);
//}

#endif
#endif
