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

#include "BackEndAlignment.h"
#include "CBCChannelGroupHandler.h"
#include "Channel.h"
#include "CicFEAlignment.h"
#include "ContainerFactory.h"
#include "Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

#ifdef __EUDAQ__
#include "Eudaq2Producer.h"

// std::map<Chip*, uint16_t> CicFEAlignment::fVplusMap;

Eudaq2Producer::Eudaq2Producer(const std::string& name, const std::string& runcontrol)
    : Tool(), eudaq::Producer(name, runcontrol), fInitialised(false), fConfigured(false), fStarted(false), fStopped(false), fTerminated(false)
{
    fPh2FileHandler   = nullptr;
    fSLinkFileHandler = nullptr;
    fLastThreshold    = -1;
    fLastExtTriggerID = -1;
    fFirstEvent       = true;
    fDifference       = 1;
    fLastTrigId       = 0;
}

Eudaq2Producer::~Eudaq2Producer() {}

void Eudaq2Producer::Initialise() {}

void Eudaq2Producer::DoInitialise()
{
    LOG(INFO) << "Initialising producer..." << RESET;
    auto ini = GetInitConfiguration();
    fHWFile  = ini->Get("HWFile", "./settings/D19CDescription.xml");

    std::stringstream outp;
    // initialisng ph2acf
    this->InitializeHw(fHWFile, outp);
    this->InitializeSettings(fHWFile, outp);
    LOG(INFO) << outp.str();
    BeBoard* theFirstBoard = static_cast<BeBoard*>(fDetectorContainer->at(0));

    // configure hardware
    this->ConfigureHw();
    fHandshakeEnabled    = (this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable") > 0);
    fTriggerMultiplicity = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");

    // now do cic alignment if needed
    // udtc does not support different front-end types that is why checking for only board 0 is enough
    if(theFirstBoard->getFrontEndType() == FrontEndType::CIC || theFirstBoard->getFrontEndType() == FrontEndType::CIC2)
    {
        // CIC FE alignment tool
        CicFEAlignment cCicAligner;
        cCicAligner.Inherit(this);
        cCicAligner.Start(0);
        cCicAligner.waitForRunToBeCompleted();
        cCicAligner.Reset();
    }
    // align back-end
    BackEndAlignment cBackEndAligner;
    cBackEndAligner.Inherit(this);
    cBackEndAligner.Start(0);
    cBackEndAligner.waitForRunToBeCompleted();
    cBackEndAligner.Reset();
    // when I get here .. I want to update the common_stub_data_delay
    // then I am sure that I should see stubs as long as the
    // correct hit latency is set
    for(auto cBoard: *cBackEndAligner.fDetectorContainer)
    {
        auto     cStubOffset     = cBoard->getStubOffset();
        uint16_t cTriggerLatency = 0;
        uint8_t  cReTimePix      = 0;
        for(auto cOpticalReadout: *cBoard)
        {
            if(cOpticalReadout->getIndex() > 0) break;
            for(auto cHybrid: *cOpticalReadout)
            {
                if(cHybrid->getIndex() > 0) break;
                for(auto cReadoutChip: *cHybrid)
                {
                    if(cReadoutChip->getFrontEndType() == FrontEndType::SSA) continue;
                    if(cTriggerLatency != 0) continue;
                    cTriggerLatency = cBackEndAligner.fReadoutChipInterface->ReadChipReg(cReadoutChip, "TriggerLatency");
                    if(cReadoutChip->getFrontEndType() == FrontEndType::MPA) cReTimePix = cBackEndAligner.fReadoutChipInterface->ReadChipReg(cReadoutChip, "RetimePix");
                }
            }
        }
        uint32_t cStubDataDelay = cTriggerLatency - (cStubOffset + cReTimePix);
        LOG(INFO) << BOLDMAGENTA << "Trigger latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " set to " << cTriggerLatency << RESET;
        LOG(INFO) << BOLDMAGENTA << "Stub latency on FEs connected to BeBoard#" << +cBoard->getIndex() << " will be set to " << cStubDataDelay << RESET;
        cBackEndAligner.fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubDataDelay);
    }

    fInitialised = true;
    LOG(INFO) << "  INITIALIZE ID: " << ini->Get("initid", 0) << std::endl;
    EUDAQ_INFO("TLU INITIALIZE ID: " + std::to_string(ini->Get("initid", 0)));
    LOG(INFO) << " READBACK configuration : " << std::endl;

    auto cReadbackHandshakeMode      = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.tlu_block.handshake_mode");
    auto cReadbackTluEnable          = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled");
    auto cReadbackTriggerSource      = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.fast_command_block.trigger_source");
    auto cReadbackHitLatencyFeh0     = this->fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(theFirstBoard->front()->at(0)->front()), "TriggerLatency");
    auto cReadbackHitLatencyFeh1     = this->fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(theFirstBoard->front()->at(1)->front()), "TriggerLatency");
    auto cReadbackCommonStubDataDely = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");
    auto cReadbackPackageDelay       = this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_cnfg.physical_interface_block.cic.stub_package_delay");

    LOG(INFO) << "  HANDSHAKE        : " << cReadbackHandshakeMode << std::endl;
    LOG(INFO) << "  TLU ENABLE       : " << cReadbackTluEnable << std::endl;
    LOG(INFO) << "  TRIGGER SOURCE   : " << cReadbackTriggerSource << std::endl;
    LOG(INFO) << "  HIT LATENCY FEH0 : " << cReadbackHitLatencyFeh0 << std::endl;
    LOG(INFO) << "  HIT LATENCY FEH1 : " << cReadbackHitLatencyFeh1 << std::endl;
    LOG(INFO) << "  COM STUB DELAY   : " << cReadbackCommonStubDataDely << std::endl;
    LOG(INFO) << "  PACKAGE DELAY    : " << cReadbackPackageDelay << std::endl;
    EUDAQ_INFO("BEB PACKAGE DELAY : " + std::to_string(cReadbackPackageDelay));
    EUDAQ_INFO("HIT LATENCY FEH 0 : " + std::to_string(cReadbackHitLatencyFeh0));
    EUDAQ_INFO("HIT LATENCY FEH 1 : " + std::to_string(cReadbackHitLatencyFeh1));
    EUDAQ_INFO("COM STUBDATA DELAY: " + std::to_string(cReadbackCommonStubDataDely));
}

void Eudaq2Producer::DoConfigure()
{
    LOG(INFO) << "Configuring producer..." << RESET;

    // only thing I don't understand is where the run number goes
    // getting the configuration
    // auto              cRunNumber = GetRunNumber();
    auto conf      = GetConfiguration();
    int  threshold = conf->Get("threshold", 550);
    if(threshold >= 0)
    {
        LOG(INFO) << RED << "Set Threshold on all CBCs to " << threshold << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        // ReadoutChip* theChip = static_cast<ReadoutChip*>(cChip);
                        ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, threshold);
                        static_cast<ReadoutChip*>(cChip)->accept(cThresholdVisitor);
                    }
                }
            }
        }
    }
    // Insert Thresholdvisitor

    // done configuration
    fConfigured = true;
}

void Eudaq2Producer::DoStartRun()
{
    LOG(INFO) << "Starting Run..." << RESET;

    // Send BORE event at beginning of run with all register information from CBCs
    eudaq::EventSP cEudaqEvent = eudaq::Event::MakeShared("CMSPhase2RawEvent");
    std::time_t    cTimestamp  = std::time(nullptr);
    cEudaqEvent->SetTimestamp(cTimestamp, cTimestamp);
    cEudaqEvent->SetBORE();
    // Readout the CBCs register data and store them as Tags in a BORE event
    char name[150];
    char name2[150];
    LOG(INFO) << "Downloading the register configuration of the CBCs" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    int              cCbcId    = int(cChip->getId());
                    uint32_t         cHybridId = cHybrid->getId();
                    auto             cRegMap   = cChip->getRegMap();
                    ThresholdVisitor cThresholdVisitor(fReadoutChipInterface);
                    static_cast<ReadoutChip*>(cChip)->accept(cThresholdVisitor);
                    uint16_t cOriginalThreshold = cThresholdVisitor.getThreshold();
                    std::sprintf(name2, "Threshold_%02d_%02d", int(cHybridId), int(cCbcId));
                    cEudaqEvent->SetTag(name2, (uint32_t)cOriginalThreshold);
                    LOG(INFO) << "Threshold FE" << int(cHybridId) << "CBC" << int(cCbcId) << ": " << cOriginalThreshold << RESET;
                    for(auto& ireg: cRegMap)
                    {
                        std::sprintf(name, "%s_%02d_%02d", ireg.first.c_str(), int(cHybridId), int(cCbcId));
                        cEudaqEvent->SetTag(name, (uint32_t)ireg.second.fValue);
                        LOG(DEBUG) << "Register " << ireg.first.c_str() << "\t" << ireg.second.fValue << RESET;
                    } // end of ireg loop
                }
            }
        }
    }
    SendEvent(cEudaqEvent);

    auto conf       = GetConfiguration();
    auto cRunNumber = GetRunNumber();
    // file handlers for Ph2ACF raw + s-link
    fRawPh2ACF = conf->Get("RawDataDirectory", "/tmp/") + "Run_" + std::to_string(cRunNumber) + ".raw";
    LOG(INFO) << BOLDBLUE << "Writing raw ph2_acf data to " << fRawPh2ACF << RESET;

    BeBoard* theFirstBoard = static_cast<BeBoard*>(fDetectorContainer->at(0));
    uint32_t cBeId         = theFirstBoard->getId();
    uint32_t cNChip        = 0;
    // this is hard coded now .. should figure out how to calculate this
    uint32_t    cNEventSize32 = 80; // this->computeEventSize32 (cBoard);
    std::string cBoardTypeString;
    BoardType   cBoardType = theFirstBoard->getBoardType();
    for(const auto cHybrid: *theFirstBoard) cNChip += cHybrid->size();
    if(cBoardType == BoardType::D19C)
        cBoardTypeString = "D19C";
    else if(cBoardType != BoardType::RD53)
        cBoardTypeString = "FC7";
    uint32_t   cFWWord  = fBeBoardInterface->getBoardInfo(theFirstBoard);
    uint32_t   cFWMajor = (cFWWord & 0xFFFF0000) >> 16;
    uint32_t   cFWMinor = (cFWWord & 0x0000FFFF);
    FileHeader cHeader(cBoardTypeString, cFWMajor, cFWMinor, cBeId, cNChip, cNEventSize32, theFirstBoard->getEventType());

    fPh2FileHandler          = new FileHandler(fRawPh2ACF, 'w', cHeader);
    std::string cSlinkPh2ACF = conf->Get("RawDataDirectory", "/tmp/") + "Run_" + std::to_string(cRunNumber) + ".daq";
    // LOG(INFO) << BOLDBLUE << "Writing s-link data to " << cSlinkPh2ACF << RESET;
    // fSLinkFileHandler = new FileHandler(cSlinkPh2ACF, 'w', cHeader);

    // Ph2 object stuff
    LOG(INFO) << BOLDBLUE << "Opening shutter ...." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        // Start() also does CBC fast reset and readout reset
        this->fBeBoardInterface->Start(static_cast<BeBoard*>(cBoard));
        LOG(INFO) << BOLDBLUE << "Shutter opened on board " << +cBoard->getId() << RESET;
    }

    LOG(INFO) << "Run Started, number of triggers received so far: " << +this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");

    // starting readout loop in thread
    fStarted = true, fStopped = false;
    fThreadRun = std::thread(&Eudaq2Producer::ReadoutLoop, this);
}

void Eudaq2Producer::DoStopRun()
{
    fStarted = false, fStopped = true;
    // TLU stuff
    LOG(INFO) << "Stopping Run..." << RESET;

    // Ph2ACF stuff
    LOG(INFO) << BOLDBLUE << "Closing shutter..." << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        this->fBeBoardInterface->Stop(static_cast<BeBoard*>(cBoard));
        LOG(INFO) << BOLDBLUE << "Shutter closed on board " << +cBoard->getId() << RESET;
    }
    BeBoard* theFirstBoard = static_cast<BeBoard*>(fDetectorContainer->at(0));
    LOG(INFO) << "Run Stopped, number of triggers received so far: " << +this->fBeBoardInterface->ReadBoardReg(theFirstBoard, "fc7_daq_stat.fast_command_block.trigger_in_counter");

    if(fThreadRun.joinable()) { fThreadRun.join(); }
    // check if file handler is open
    if(fPh2FileHandler->isFileOpen())
    {
        LOG(INFO) << BOLDBLUE << "Closing file handler for .raw " << RESET;
        fPh2FileHandler->closeFile();
    }
    fDifference = 1, fLastTrigId = 0;
    // delete fPh2FileHandler;

    // check if file handler is open
    // if(fSLinkFileHandler->isFileOpen())
    // {
    //     LOG(INFO) << BOLDBLUE << "Closing file handler for .daq " << RESET;
    //     fSLinkFileHandler->closeFile();
    // }
    // delete fPh2FileHandler;
}

void Eudaq2Producer::DoReset()
{
    if(fInitialised)
    {
        // just in case close the shutter
        for(auto cBoard: *fDetectorContainer) { this->fBeBoardInterface->Stop(static_cast<BeBoard*>(cBoard)); }
    }

    // finish data processing
    fStarted = false, fStopped = true, fConfigured = false;
    std::cout << "Try to join thread" << std::endl;
    if(fThreadRun.joinable()) { fThreadRun.join(); }

    // // configure the board again
    // this->DoConfigure();
    // fStarted = false, fStopped = true, fConfigured = true;
}

void Eudaq2Producer::DoTerminate()
{
    fInitialised = false, fConfigured = false, fStarted = false, fStopped = true, fTerminated = true;
    if(fThreadRun.joinable()) { fThreadRun.join(); }
    LOG(INFO) << "Terminating ...";
    // this->Destroy();
}

// ReadoutLoop has been modified in order to allow for the acquisition of multilple events by a single trigger signal.
// This way, time walk performance can be evaluated in the analysis
// Multiple Ph2ACF Events are read, converted and stored as EUDAQ SubEvents within in one EUDAQ Event
void Eudaq2Producer::ReadoutLoop()
{
    std::vector<Event*> cPh2Events; // Ph2ACF Event vector to store newly read data and previously remaining one
    while(!fStopped)
    {
        if(!EventsPending()) { continue; }
        for(auto cBoard: *fDetectorContainer)
        {
            BeBoard* theBoard  = static_cast<BeBoard*>(cBoard);
            int      cRunState = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.fast_command_block.general.fsm_state");
            if(cRunState != 1)
                LOG(INFO) << BOLDRED << cRunState << RESET;
            else
                LOG(INFO) << BOLDBLUE << cRunState << RESET;

            std::vector<uint32_t> cRawData(0);
            this->ReadData(theBoard, cRawData);
            // empty data - wait and pass
            if(cRawData.size() == 0)
            {
                LOG(INFO) << BOLDBLUE << "Read-back 0 words from the DD3 memory using ReadData.. waiting 100 ms " << RESET;
                std::this_thread::sleep_for(std::chrono::microseconds(100));
                continue;
            }
            fPh2FileHandler->setData(cRawData);
            std::vector<Event*> cPh2Events = this->GetEvents();
            if(cPh2Events.size() == 0)
            {
                LOG(INFO) << BOLDBLUE << "Decoded 0 valid events.. not going to send anything ... " << RESET;
                continue;
            }

            //{
            LOG(INFO) << BOLDBLUE << +cPh2Events.size() << " events read back from FC7 with ReadData" << RESET;
            std::time_t cTimestamp = std::time(nullptr);
            for(auto cPh2Event: cPh2Events)
            {
                eudaq::EventSP cEudaqEvent = eudaq::Event::MakeShared("CMSPhase2RawEvent");
                cEudaqEvent->SetTimestamp(cTimestamp, cTimestamp);
                // Add multiple Ph2ACF Events as EUDAQ SubEvents to a EUDAQ Event
                // // sarah
                // // un-comment this to test s-link event writing
                // SLinkEvent            cSLev = cPh2Event->GetSLinkEvent(cBoard);
                // std::vector<uint32_t> tmp   = cSLev.getData<uint32_t>();
                // fSLinkFileHandler->setData(tmp);

                /// christian's fix, some call it awesome (florian at least) of trying
                /// to align the ref+telescope with the du data, as we have seen that
                /// the trigger id for the telescope was advanced by one comp. to the dut,
                /// as well as the du had started with a"bonus" event to begin with
                if(cPh2Event->GetEventCount() < 1)
                {
                    LOG(INFO) << BOLDRED << "Skip first event with ID : " << cPh2Event->GetEventCount() << RESET;
                    continue;
                }
                /// end of awesoness

                eudaq::EventSP cEudaqSubEvent = eudaq::Event::MakeShared("CMSPhase2RawEvent");
                this->ConvertToSubEvent(cBoard, cPh2Event, cEudaqSubEvent);
                cEudaqSubEvent->SetTimestamp(cTimestamp, cTimestamp);
                cEudaqEvent->AddSubEvent(cEudaqSubEvent);
                SendEvent(cEudaqEvent);
            } // end of Ph2Events.size()
        }     // end of cBoard loop
    }         // end of !fStopped loop
}

void Eudaq2Producer::ConvertToSubEvent(const BeBoard* pBoard, const Event* pPh2Event, eudaq::EventSP pEudaqSubEvent)
{
    /// roland's fix for missing consequtive trigger ids
    // Try to fix correlation loss at rates higher than 450 Hz
    int cTrigNDiff = pPh2Event->GetExternalTriggerId() - (pPh2Event->GetEventCount() & 0x7FFF);
    // int cTrigNDiffHalf = pPh2Event->GetExternalTriggerId()/2 - pPh2Event->GetEventCount();
    // LOG(INFO) << cTrigNDiff << "\t" << fDifference << RESET;
    // if (cTrigNDiffHalf > 0) {
    //   cTrigNDiff = uint32_t(cTrigNDiffHalf);
    // }
    // if (fLastTrigId != 0 && fLastTrigId > pPh2Event->GetExternalTriggerId() && cTrigNDiffHalf <= 0) {
    //   // Don't send out anything
    //   LOG(WARNING) << "Found strange combination " << pPh2Event->GetExternalTriggerId() << "\t" << pPh2Event->GetEventCount() << "\t" << fLastTrigId << RESET;
    // }

    if(cTrigNDiff == fDifference + 1 && cTrigNDiff > 0)
    {
        eudaq::EventSP cEudaqEvent = eudaq::Event::MakeShared("CMSPhase2RawEvent");
        cEudaqEvent->SetTag("INVALID", 1);
        SendEvent(cEudaqEvent);
        LOG(WARNING) << "Sending out invalid event " << pPh2Event->GetExternalTriggerId() << "\t" << pPh2Event->GetEventCount() << RESET;
        fDifference++;
    }
    pEudaqSubEvent->SetTag("INVALID", 0);
    fLastTrigId = pPh2Event->GetExternalTriggerId();
    /// end of roland's fix

    pEudaqSubEvent->SetTag("L1_COUNTER_BOARD", pPh2Event->GetEventCount());
    pEudaqSubEvent->SetTag("TDC", pPh2Event->GetTDC());
    pEudaqSubEvent->SetTag("BX_COUNTER", pPh2Event->GetBunch());
    pEudaqSubEvent->SetTriggerN(pPh2Event->GetExternalTriggerId());
    pEudaqSubEvent->SetTag("TLU_TRIGGER_ID", pPh2Event->GetExternalTriggerId());

    auto cL1Id = (static_cast<const D19cCic2Event*>(pPh2Event))->L1Id(0, 0);
    LOG(INFO) << "Event " << pPh2Event->GetEventCount() << " with TriggerNr " << pPh2Event->GetExternalTriggerId() << " and L1Id " << cL1Id << RESET;

    // in order to get proper data alignment always 8
    uint32_t cMaxChipNumber = 8;
    uint32_t cNRows         = (NCHANNELS / 2) * cMaxChipNumber;
    // top bottom sensors
    uint32_t cSensorId = 0;
    // iterator for the hybrid vector

    for(auto cOpticalReadout: *pBoard)
    {
        auto cFeIter = cOpticalReadout->begin();
        while(cFeIter != cOpticalReadout->end())
        {
            // make sure that we always start counting from the right hybrid (hybrid0 within the hybrid)
            uint32_t cFeId0 = (*cFeIter)->getId();
            // build sensor id (one needs divide by two because 2 hybrids per hybrid, but multiply by two because 2
            // sensors per hybrid)
            cSensorId = (cFeId0 - (cFeId0 % 2));

            // vectors to srore data
            std::vector<uint8_t> top_channel_data;
            std::vector<uint8_t> bottom_channel_data;
            size_t               top_offset = 0, bottom_offset = 0;
            std::vector<uint8_t> top_data_final(6);
            std::vector<uint8_t> bottom_data_final(6);

            // we have two hybrids (FE) per hybrid therefore we iterate a bit here
            // if we are
            uint32_t cIterRange     = ((cFeId0 % 2) == 0) ? 2 : 1; // now we also need to make sure that we starting from the right hybrid (0)
            auto     cFeIterCurrent = cFeIter;
            for(uint32_t i = 0; i < cIterRange; i++)
            {
                // check that we are still not at the end
                if(cFeIterCurrent == cOpticalReadout->end()) continue;
                // get the fe id
                uint32_t cFeIdCurrent = (*cFeIterCurrent)->getId();

                // parsing CBC data (CBC2 or CBC3)
                // !!!!!!!!!!! Two uint8_t words per uint16_t, that is why all values are encoded by two
                for(auto cCbc: *(*cFeIterCurrent))
                {
                    int cChipId = (int)cCbc->getId();
                    // adding this check here [sarah]
                    // std::string cCheck = pPh2Event->DataBitString( cCbc->getHybridId() , cCbc->getId() );
                    // if( cCheck.empty() )
                    //	continue;
                    const std::vector<uint32_t> cHitsVector = pPh2Event->GetHits((*cFeIterCurrent)->getId(), cCbc->getId());
                    fHitsCounter += pPh2Event->GetNHits((*cFeIterCurrent)->getId(), cCbc->getId());
                    for(auto hit: cHitsVector)
                    {
                        // LOG(INFO) << "FeIdCurrent: " << cFeIdCurrent << ", cbc: " << cChipId << ", hit id: " << hit;
                        if(hit % 2 == 1)
                        {
                            uint32_t hit_pos = (cChipId * NCHANNELS / 2) + (hit - 1) / 2;
                            if(cFeIdCurrent % 2 == 0) hit_pos = 1015 - hit_pos;
                            // top sensor
                            top_channel_data.resize(top_offset + 6);
                            // row
                            top_channel_data[top_offset + 0] = (hit_pos >> 0) & 0xFF;
                            top_channel_data[top_offset + 1] = (hit_pos >> 8) & 0xFF;
                            // column
                            top_channel_data[top_offset + 2] = ((1 - (cFeIdCurrent % 2)) >> 0) & 0xFF;
                            top_channel_data[top_offset + 3] = ((1 - (cFeIdCurrent % 2)) >> 8) & 0xFF;
                            // tot (always 1)
                            top_channel_data[top_offset + 4] = 1;
                            top_channel_data[top_offset + 5] = 0;
                            // offset
                            top_offset += 6;
                        }
                        else
                        {
                            uint32_t hit_pos = (cChipId * NCHANNELS / 2) + hit / 2;
                            if(cFeIdCurrent % 2 == 0) hit_pos = 1015 - hit_pos;
                            // bottom sensor
                            bottom_channel_data.resize(bottom_offset + 6);
                            // row
                            bottom_channel_data[bottom_offset + 0] = (hit_pos >> 0) & 0xFF;
                            bottom_channel_data[bottom_offset + 1] = (hit_pos >> 8) & 0xFF;
                            // column
                            bottom_channel_data[bottom_offset + 2] = ((1 - (cFeIdCurrent % 2)) >> 0) & 0xFF;
                            bottom_channel_data[bottom_offset + 3] = ((1 - (cFeIdCurrent % 2)) >> 8) & 0xFF;
                            // tot (always 1)
                            bottom_channel_data[bottom_offset + 4] = 1;
                            bottom_channel_data[bottom_offset + 5] = 0;
                            // offset
                            bottom_offset += 6;
                        }
                    } // end of hit loop
                }     // end of cCbc loop
                cFeIterCurrent++;
                cFeIter++;
            } // end fe within a hybrid loop loop

            //// top sensor
            // number of rows
            top_data_final[0] = (cNRows >> 0) & 0xFF;
            top_data_final[1] = (cNRows >> 8) & 0xFF;
            // number of rows
            top_data_final[2] = 2;
            top_data_final[3] = 0;
            // number
            uint32_t numhits_top = (top_channel_data.size()) / 6;
            top_data_final[4]    = (numhits_top >> 0) & 0xFF;
            top_data_final[5]    = (numhits_top >> 8) & 0xFF;
            // now append data
            top_data_final.insert(top_data_final.end(), top_channel_data.begin(), top_channel_data.end());
            // send
            pEudaqSubEvent->AddBlock(cSensorId, top_data_final);

            //// bottom sensor
            // number of rows
            bottom_data_final[0] = (cNRows >> 0) & 0xFF;
            bottom_data_final[1] = (cNRows >> 8) & 0xFF;
            // number of rows
            bottom_data_final[2] = 2;
            bottom_data_final[3] = 0;
            // number
            uint32_t numhits_bottom = (bottom_channel_data.size()) / 6;
            bottom_data_final[4]    = (numhits_bottom >> 0) & 0xFF;
            bottom_data_final[5]    = (numhits_bottom >> 8) & 0xFF;
            // now append data
            bottom_data_final.insert(bottom_data_final.end(), bottom_channel_data.begin(), bottom_channel_data.end());
            // send
            pEudaqSubEvent->AddBlock(cSensorId + 1, bottom_data_final);
        } // end of cFe loop
    }

    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            char name[100];
            auto cBxId = static_cast<const D19cCic2Event*>(pPh2Event)->BxId(cHybrid->getId());
            std::sprintf(name, "bx_ID_%02d", cHybrid->getId());
            pEudaqSubEvent->SetTag(name, (uint32_t)cBxId);
            auto cStatusBit = static_cast<const D19cCic2Event*>(pPh2Event)->Status(cHybrid->getId());
            std::sprintf(name, "status_%02d", cHybrid->getId());
            pEudaqSubEvent->SetTag(name, (uint32_t)cStatusBit);

            for(auto cCbc: *cHybrid)
            {
                char     name[100];
                uint32_t cHybridId = cHybrid->getHybridId();
                uint32_t cCbcId    = cCbc->getId();
                // auto cL1Id = static_cast<const D19cCic2Event*>(pPh2Event)->L1Id( cHybrid->getId(), cCbc->getCId() );
                std::sprintf(name, "pipeline_address_%02d_%02d", cHybridId, cCbcId);
                pEudaqSubEvent->SetTag(name, (uint32_t)pPh2Event->PipelineAddress(cHybridId, cCbcId));
                std::sprintf(name, "error_%02d_%02d", cHybridId, cCbcId);
                pEudaqSubEvent->SetTag(name, (uint32_t)pPh2Event->Error(cHybridId, cCbcId));
                // std::sprintf(name, "stubvec_size_%02d_%02d", cHybridId, cCbcId);
                // pEudaqSubEvent->SetTag(name, (uint32_t)pPh2Event->StubVector(cHybridId, cCbcId).size());
                // std::sprintf(name, "l1_counter__%02d_%02d", cHybridId, cCbcId);
                // pEudaqSubEvent->SetTag(name, (uint32_t)pPh2Event->GetEventCount(cHybridId, cCbcId));
                uint8_t cStubId = 0;
                for(auto cStub: pPh2Event->StubVector(cHybridId, cCbcId))
                {
                    std::sprintf(name, "stub_pos_%02d_%02d_%02d", cHybridId, cCbcId, cStubId);
                    pEudaqSubEvent->SetTag(name, (uint32_t)cStub.getPosition());
                    std::sprintf(name, "stub_bend_%02d_%02d_%02d", cHybridId, cCbcId, cStubId);
                    pEudaqSubEvent->SetTag(name, (uint32_t)cStub.getBend());

                    cStubId++;
                } // end of cStub loop
            }     // end of cCbc loop
        }         // end of cHybrid loop
    }
}

bool Eudaq2Producer::EventsPending()
{
    if(fConfigured)
    {
        if(fHandshakeEnabled)
        {
            for(auto cBoard: *fDetectorContainer)
            {
                BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
                if(theBoard->getBoardType() == BoardType::D19C)
                {
                    if(this->fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.readout_block.general.readout_req") > 0) { return true; } // end of if ReadBoardReg
                }                                                                                                                              // end of if BoardType
            }                                                                                                                                  // end of theBoard loop
        }
        else
        {
            return true;
        } // end of if fHandshakeEnabled

    } // end of if fConfigured
    return false;
}

// FIXME check me Sarah
void Eudaq2Producer::writeObjects()
{
    // this->SaveResults();
    // fResultFile->Flush();
}
#endif
