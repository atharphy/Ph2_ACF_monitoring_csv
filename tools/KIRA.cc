#include "KIRA.h"

#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/Occupancy.h"

KIRA::KIRA() : OTTool() { }
KIRA::~KIRA() { }

// fKiraClient->sendAndReceivePacket("KIRATriggerFrequency,ArduinoId:" + fKiraId + ",Frequency:124");
    /*
    KIRATriggerFrequency,ArduinoId:abc,Frequency:123
    KIRATrigger,ArduinoId:abc,Value:on/off
    KIRAPulseLength,ArduinoId:abc,PulseLength:123
    KIRADacLed,ArduinoId:abc,Value:high/low
    KIRALed,ArduinoId:abc,LED:led1,Value:on/off
    KIRALedIntensity,ArduinoId:abc,LED:led1,Intensity:123
    */
void KIRA::Initialise(int pKiraPort, std::string pKiraId)
{
    Prepare();
    SetName("KIRATest");

    fKiraClient = new TCPClient("127.0.0.1", pKiraPort);
    fKiraId = pKiraId;
    LOG(INFO) << BOLDYELLOW << "Trying to connect to the KIRA Server..." << RESET;
    if(!fKiraClient->connect(1))
    {
        LOG(ERROR) << BOLDRED << "Cannot connect to the KIRA Server, KIRA will need to be controlled manually" << RESET;
        delete fKiraClient;
        fKiraClient = nullptr;
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Connected to the KIRA Server!" << RESET;
    }

    // Switch all LEDs off
    for (int i = 0; i < 8; i++) 
    {
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(i) + ",Value:off"); 
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Top" + std::to_string(i) + ",Value:off"); 
    }
    LOG(INFO) << BOLDBLUE << "All KIRA LEDs switched off" << RESET;

    fRecycleBin.setDetectorContainer(fDetectorContainer);

    #ifdef __USE_ROOT__
    fDQMHistogrammer.book(fResultFile, *fDetectorContainer, fSettingsMap);
    #endif

    PrepareForExternal(fDetectorContainer->at(0));

}

void KIRA::PrepareForExternal(BeBoard* pBoard)
{
    LOG(INFO) << BOLDBLUE << "Prepare external triggers" << RESET;
    // configure trigger
    // make sure I am accepting all triggers
    BeBoardRegMap cRegMap = pBoard->getBeBoardRegMap();
    // trigger config
    uint32_t cTriggerMult    = cRegMap["fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity"];

    uint8_t                                       cTriggerSource = 5;
    std::vector<std::string>                      cFcmdRegs{"trigger_source", "triggers_to_accept"};
    std::vector<uint32_t>                         cFcmdRegVals{cTriggerSource, 0}; // fNevents};
    std::vector<uint32_t>                         cFcmdRegOrigVals(cFcmdRegs.size(), 0);
    std::vector<std::pair<std::string, uint32_t>> cRegVec;
    cRegVec.clear();
    for(size_t cIndx = 0; cIndx < cFcmdRegs.size(); cIndx++)
    {
        std::string cRegName    = "fc7_daq_cnfg.fast_command_block." + cFcmdRegs[cIndx];
        cFcmdRegOrigVals[cIndx] = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        cRegVec.push_back({cRegName, cFcmdRegVals[cIndx]});
    }
    cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", cTriggerMult});
    cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    // enable DIO5
    cRegVec.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x0});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.dio5_en", 0x1});
    cRegVec.push_back({"fc7_daq_cnfg.dio5_block.ch2.threshold", 0});
    fBeBoardInterface->WriteBoardMultReg(pBoard, cRegVec);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled", 0);

    // stop triggers
    fBeBoardInterface->Stop(pBoard);
    // update registers
    UpdateFromRegMap(pBoard);
    // send a ReSync
    fBeBoardInterface->ChipReSync(pBoard);
}

void KIRA::determineLatency()
{
    auto cBoard = fDetectorContainer->at(0);
    // initialize latency scan range 
    uint16_t cLatencyStart = findValueInSettings<double>("StartLatency", 85);
    uint16_t cLatencyRange = findValueInSettings<double>("LatencyRange", 15);
    uint16_t cLatencyLED = findValueInSettings<double>("KiraLatencyLed", 4);
    uint16_t cLatencyIntensity = findValueInSettings<double>("KiraLatencyIntensity", 31000);

    uint16_t cHitMaximum = 0;
    uint16_t cLatencySetting = 0;
     
    LOG(INFO) << "Determine correct latency setting for KIRA operation using bottom LED " << +cLatencyLED << RESET;
    fKiraClient->sendAndReceivePacket("KIRALedIntensity,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLatencyLED) + ",Intensity:" + std::to_string(cLatencyIntensity));
    fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLatencyLED) + ",Value:on");

    for (uint16_t cLat = cLatencyStart; cLat < cLatencyStart + cLatencyRange; cLat++) 
    {
        LOG(INFO) << BOLDBLUE << "Set Latency to " << cLat << RESET;
        setSameDacBeBoard(cBoard, "TriggerLatency", cLat);
        fBeBoardInterface->ChipReSync(cBoard);
        ReadNEvents(cBoard, fNevents);

        size_t   cTriggerMult = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        const std::vector<Event*>& cEvents              = this->GetEvents();
        // loop over triggers in the burst
        for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
        {
            // prepare container to hold hit information per chip
            DetectorDataContainer cHitContainer;
            ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cHitContainer);
            // zero hit container
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        cHitContainer.at(cBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<uint16_t>() = 0;
                    } // chip
                }     // hybrid
            }         // optical group

            // start at the beginning + trigger id in burst
            auto cEventIter  = cEvents.begin() + cTriggerId;
            fNReadbackEvents = cEvents.size();
            do
            {
                if(cEventIter >= cEvents.end()) break;
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                            // skip all chips that are not directly illuminated by the LED 
                            if(cHybrid->getIndex()%2 == 0 && cChip->getIndex() != 7-cLatencyLED) continue;
                            if(cHybrid->getIndex()%2 == 1 && cChip->getIndex() != cLatencyLED) continue;

                            auto cHits = (*cEventIter)->GetHits(cHybrid->getId(), cChip->getId());
                            if (cHits.size() != 0)
                                LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << "Chip#" << +cChip->getId() % 8 << " " << +cHits.size() << " hits." << RESET;
                            for(auto cHit: cHits)
                            {
                                // monitor only bottom sensor channels
                                if(cHit % 2 == 0)
                                {
                                    cHitContainer.at(cBoard->getIndex())
                                    ->at(cOpticalGroup->getIndex())
                                    ->at(cHybrid->getIndex())
                                    ->at(cChip->getIndex())
                                    ->getSummary<uint16_t>() += 1;
                                }
                            }
                        } // chip vector
                    }     // hybrid vector
                }         // optical group vector
                cEventIter += (1 + cTriggerMult);
            } while(cEventIter < cEvents.end()); 

            #ifdef __USE_ROOT__
                fDQMHistogrammer.fillLatencyPlots(cLat + cTriggerId, cTriggerId, cHitContainer, fNevents);
            #endif

            // Find earliest maximum latency bin (= largest latency setting with maximum entries)
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                        if(cHybrid->getIndex()%2 == 0 && cChip->getIndex() != 7-cLatencyLED) continue;
                        uint16_t cTmpHits = cHitContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>();
                        if (cTmpHits >= cHitMaximum)
                        {
                            cHitMaximum = cTmpHits;
                            cLatencySetting = cLat + cTriggerId; 
                        }
                    }
                }
            }
        }
    }

    LOG(INFO) << BOLDRED << "Found optimal latency to be " << cLatencySetting << RESET;
    LOG(INFO) << BOLDRED << "Set Latency to " << cLatencySetting << RESET;
    setSameDacBeBoard(cBoard, "TriggerLatency", cLatencySetting);
    fBeBoardInterface->ChipReSync(cBoard);
    // Switch LED off again 
    fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLatencyLED) + ",Value:off");
}

void KIRA::performKIRATest()
{
    LOG(INFO) << BOLDRED << "Starting KIRA Test" << RESET;
    auto cBoard = fDetectorContainer->at(0);
    uint16_t cIntensity = findValueInSettings<double>("KiraIntensity", 31000);

    for (uint16_t cLED = 0; cLED < 8; cLED++)
    {
        LOG(INFO) << BOLDYELLOW << "Test with bottom LED " << cLED << RESET;
        // Start with bottom LED 
        fKiraClient->sendAndReceivePacket("KIRALedIntensity,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLED) + ",Intensity:" + std::to_string(cIntensity));
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLED) + ",Value:on");
        
        ReadNEvents(cBoard, fNevents);

        size_t   cTriggerMult = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        const std::vector<Event*>& cEvents              = this->GetEvents();
        DetectorDataContainer cHitContainer = analyseEvents(cBoard, cEvents, 0, cLED);
        #ifdef __USE_ROOT__
            fDQMHistogrammer.fillBottomSensorPlots(cHitContainer, fNevents, cLED);
        #endif
        // Switch off LED
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Bot" + std::to_string(cLED) + ",Value:off");

        // Start top LED 
        fKiraClient->sendAndReceivePacket("KIRALedIntensity,ArduinoId:" + fKiraId + ",LED:Top" + std::to_string(cLED) + ",Intensity:" + std::to_string(int(29000+cLED*(1000/8.))));
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Top" + std::to_string(cLED) + ",Value:on");
        
        LOG(INFO) << BOLDYELLOW << "Test with top LED " << cLED << RESET;
        ReadNEvents(cBoard, fNevents);

        const std::vector<Event*>& cEvents2              = this->GetEvents();
        DetectorDataContainer cHitContainer2 = analyseEvents(cBoard, cEvents2, 1, cLED);
        #ifdef __USE_ROOT__
            fDQMHistogrammer.fillTopSensorPlots(cHitContainer2, fNevents, cLED);
        #endif
        // Switch off LED
        fKiraClient->sendAndReceivePacket("KIRALed,ArduinoId:" + fKiraId + ",LED:Top" + std::to_string(cLED) + ",Value:off");
    }
}

DetectorDataContainer KIRA::analyseEvents(BeBoard* pBoard, const std::vector<Event*>& pEvents, uint16_t pSensor, uint16_t pLED)
{
    // prepare container to hold hit information per chip
    DetectorDataContainer cHitContainer;
    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, float>>(*fDetectorContainer, cHitContainer);

    // start at the beginning + trigger id in burst
    auto cEventIter  = pEvents.begin();
    fNReadbackEvents = pEvents.size();
    do
    {
        if(cEventIter >= pEvents.end()) break;
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() != FrontEndType::CBC3) continue;
                    // skip all chips that are not directly illuminated by the LED 
                    if(cHybrid->getIndex()%2 == 0 && cChip->getIndex() != 7-pLED) continue;
                    if(cHybrid->getIndex()%2 == 1 && cChip->getIndex() != pLED) continue;

                    auto cHits = (*cEventIter)->GetHits(cHybrid->getId(), cChip->getId());
                    if (cHits.size() != 0)
                        LOG(DEBUG) << BOLDBLUE << "Event#" << (*cEventIter)->GetEventCount() << "Chip#" << +cChip->getId() % 8 << " " << +cHits.size() << " hits." << RESET;
                    for(auto cHit: cHits)
                    {
                        // Fill hits in Data Container for bottom sensor
                        if(cHit % 2 == pSensor)
                        {
                            cHitContainer.at(pBoard->getIndex())
                            ->at(cOpticalGroup->getIndex())
                            ->at(cHybrid->getIndex())
                            ->at(cChip->getIndex())
                            ->getSummary<GenericDataArray<VECSIZE, float>>()[int(cHit/2)] += 1; 
                        }
                    }
                } // chip vector
            }     // hybrid vector
        }         // optical group vector
        cEventIter += 1;
    } while(cEventIter < pEvents.end()); 
    return cHitContainer;
}

// void KIRA::

// void KIRA::IntensityCalibration() 
// {
//     // fKiraClient->sendAndReceivePacket("")
// }