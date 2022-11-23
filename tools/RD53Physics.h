/*!
  \file                  RD53Physics.h
  \brief                 Implementaion of Physics data taking
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Physics_H
#define RD53Physics_H

#include "../HWDescription/RD53ACommands.h"
#include "../HWInterface/RD53FWInterface.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/RD53Shared.h"
#include "RD53CalibBase.h"

#ifdef __USE_ROOT__
#include "../DQMUtils/RD53PhysicsHistograms.h"
#endif

// #######################
// # Physics data taking #
// #######################
class Physics : public CalibBase
{
    using evtConvType = std::function<void(const std::vector<Ph2_HwInterface::RD53Event>&)>;

  public:
    Physics() { Physics::setGenericEvtConverter(RD53dummyEvtConverter()); }
    ~Physics()
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

    void localConfigure(const std::string& fileRes_ = "", int currentRun = -1) override;
    void initializeFiles(const std::string& fileRes_ = "", int currentRun = -1) override;
    void run() override;
    void draw(bool saveData = true) override;

    void analyze(bool doReadBinary = false);
    void sendBoardData(const BoardContainer* cBoard);
    void fillDataContainer(Ph2_HwDescription::BeBoard& cBoard);
    void setGenericEvtConverter(evtConvType arg)
    {
        std::lock_guard<std::mutex> theGuard(theMtx);
        genericEvtConverter = std::move(arg);
    }

#ifdef __USE_ROOT__
    PhysicsHistograms* histos;
#endif

  private:
    void fillHisto() override;

    void clearContainers(Ph2_HwDescription::BeBoard& cBoard);

    size_t                                   errors;
    const Ph2_HwDescription::RD53::FrontEnd* frontEnd;
    std::shared_ptr<RD53ChannelGroupHandler> theChnGroupHandler;
    DetectorDataContainer                    theOccContainer;
    DetectorDataContainer                    theBCIDContainer;
    DetectorDataContainer                    theTrgIDContainer;

  protected:
    struct RD53dummyEvtConverter
    {
        void operator()(const std::vector<Ph2_HwInterface::RD53Event>& RD53EvtList){};
    };

    size_t      rowStart;
    size_t      rowStop;
    size_t      colStart;
    size_t      colStop;
    size_t      nTRIGxEvent;
    bool        doDisplay;
    bool        doUpdateChip;
    bool        saveBinaryData;
    std::string outputBinaryDir;

    std::string fileRes;
    int         theCurrentRun;
    size_t      numberOfEventsPerRun;
    std::mutex  theMtx;
    evtConvType genericEvtConverter;
};

#endif
