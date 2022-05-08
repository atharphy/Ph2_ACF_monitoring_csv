#include "BeamTestCommissioning.h"

BeamTestCommissioning::BeamTestCommissioning() : OTTool() {}

BeamTestCommissioning::~BeamTestCommissioning() {}

void BeamTestCommissioning::Initialise()
{
  LOG(INFO) << BOLDYELLOW << "Initialising BeamTest Commissioning Tool" << RESET;
  Prepare();
  SetName("BeamTestCommissioning");

  //Find which readout chips are present
  fWithCBC = false, fWithSSA = false, fWithMPA = false;
  for(auto cBoard : *fDetectorContainer)
  {
    auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
    fWithCBC = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::CBC3) != cFrontEndTypes.end();
    fWithSSA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA) != cFrontEndTypes.end() || std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
    fWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end() || std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
    if(fWithCBC) LOG(INFO) << BOLDBLUE << "BeamTest Commissioning for 2S Module (CBC)" << RESET;
    else if(!fWithMPA && fWithSSA) LOG(INFO) << BOLDBLUE << "BeamTest Commissioning for PS Module (SSA only)" << RESET;
    else if(fWithMPA && !fWithSSA) LOG(INFO) << BOLDBLUE << "BeamTest Commissioning for PS Module (MPA only)" << RESET;
    else if(fWithMPA && fWithSSA) LOG(INFO) << BOLDBLUE << "BeamTest Commissioning for PS Module (MPA + SSA)" << RESET;
    else
    { 
      LOG(ERROR) << "BeamTest Commissioning not supporting the defined chip type" << RESET;
      throw std::runtime_error(std::string("BeamTest Commissioning not supporting the defined chip type")); 
    }
    
    for(auto cType : cFrontEndTypes)
    {
      if(cType == FrontEndType::CBC3)
      {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
      }
      else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
      {
        // SSAs
        SSAChannelGroupHandler theSSAChannelGroupHandler;
        theSSAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theSSAChannelGroupHandler, cType);
      }
      else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
      {
        // Now MPAs
        MPAChannelGroupHandler theMPAChannelGroupHandler;
        theMPAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theMPAChannelGroupHandler, cType);
      }
    }
  }

  //Retrieve some commissioning settings from XML
  fTPamplitude = findValueInSettings<double>("BTCommissioningTPamplitude");
  fTPdelay = findValueInSettings<double>("BTCommissioningTPdelay");
  fStripThreshold = findValueInSettings<double>("BTCommissioningStripThreshold");
  fPixelThreshold = findValueInSettings<double>("BTCommissioningPixelThreshold");
  fStartLatency = findValueInSettings<double>("BTCommissioningStartLatency", 0);
  fLatencyRange = findValueInSettings<double>("BTCommissioningLatencyRange", 0);
  fInjectionType = findValueInSettings<double>("BTCommissioningInjectionType");

  InitialiseContainers();
  InitialiseInjection();

// #ifdef __USE_ROOT__
//   fDQMHistogrammer.book(fResultFile, *fDetectorContainer, fSettingsMap);
// #endif
}

void BeamTestCommissioning::Running()
{
  Initialise();
  fSuccess = true;
  Reset();
}

void BeamTestCommissioning::Stop() {}

void BeamTestCommissioning::Pause() {}

void BeamTestCommissioning::Resume() {}

void BeamTestCommissioning::Finalise()
{
#ifdef __USE_ROOT__
  SaveResults();
#endif
}

void BeamTestCommissioning::InitialiseContainers()
{
  //Board trigger multiplicity containers
  ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, fTriggerMultiplicityContainer);
  //Latency scans containers
  ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fL1LatencyContainer);
  ContainerFactory::copyAndInitBoard<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, fStubLatencyContainer);
  //Latency scans count containers 
  ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint32_t>>(*fDetectorContainer, fL1LatencyHitCountContainer);
  ContainerFactory::copyAndInitBoard<GenericDataArray<VECSIZE, uint32_t>>(*fDetectorContainer, fStubLatencyStubCountContainer);
  //Hit and Stub count container
  ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fHitCountContainer);
  ContainerFactory::copyAndInitChip<uint32_t>(*fDetectorContainer, fStubCountContainer);
  //Optimal Latency containers
  ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, fOptimalL1LatencyContainer);
  ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, fOptimalStubLatencyContainer);

  for(auto cBoard : *fDetectorContainer)
  {
    auto cBoardIndex = cBoard->getIndex();
    auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>() = cTriggerMultiplicity;
    for(uint16_t cLatencyIndex = 0; cLatencyIndex < fLatencyRange; cLatencyIndex++)
    {
      fStubLatencyContainer.at(cBoardIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatencyIndex] = 0;
      fStubLatencyStubCountContainer.at(cBoardIndex)->getSummary<GenericDataArray<VECSIZE, uint32_t>>()[cLatencyIndex] = 0;
    }
    fOptimalStubLatencyContainer.at(cBoardIndex)->getSummary<uint16_t>() = 0;
    for(auto cOpticalGroup : *cBoard)
    {
      auto cOpticalGroupIndex = cOpticalGroup->getIndex();
      for(auto cHybrid : *cOpticalGroup)
      {
        auto cHybridIndex = cHybrid->getIndex();
        for(auto cChip : *cHybrid)
        {
          auto cChipIndex = cChip->getIndex();
          for(uint16_t cLatencyIndex = 0; cLatencyIndex < fLatencyRange; cLatencyIndex++)
          {
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMultiplicity + 1; cTriggerId++)
            { 
              fL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cLatencyIndex * (1 + cTriggerMultiplicity) - cTriggerId] = 0;
              fL1LatencyHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<GenericDataArray<VECSIZE, uint32_t>>()[cLatencyIndex * (1 + cTriggerMultiplicity) - cTriggerId] = 0;
            }
            fHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>() = 0;
            fStubCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>() = 0;
          }//Latency Index
          fOptimalL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint16_t>() = 0;
        }//Chip
      }//Hybrid
    }//OpticalGroup
  }//Board
}

void BeamTestCommissioning::InitialiseInjection()
{
  fInjections.clear();
  Ph2_HwInterface::Injection cInjection;
  cInjection.fRow    = 10; cInjection.fColumn = 2; fInjections.push_back(cInjection); // 0
  cInjection.fRow    = 20; cInjection.fColumn = 3; fInjections.push_back(cInjection); // 1
  cInjection.fRow    = 30; cInjection.fColumn = 4; fInjections.push_back(cInjection); // 2
  cInjection.fRow    = 40; cInjection.fColumn = 5; fInjections.push_back(cInjection); // 3
  cInjection.fRow    = 50; cInjection.fColumn = 6; fInjections.push_back(cInjection); // 4
}

void BeamTestCommissioning::SetInternalUserTrigger()
{
  LOG(INFO) << BOLDMAGENTA << "Trigger scheme set to : Internal User Trigger" << RESET;
  for(auto cBoard : *fDetectorContainer) ConfigureTriggerScheme(cBoard, true, false, false);
}

void BeamTestCommissioning::SetInternalTestPulseTrigger()
{
  LOG(INFO) << BOLDMAGENTA << "Trigger scheme set to : Internal Test Pulse Trigger" << RESET;
  for(auto cBoard : *fDetectorContainer) ConfigureTriggerScheme(cBoard, true, true, false);
}

void BeamTestCommissioning::SetExternalTestPulseTrigger()
{
  LOG(INFO) << BOLDMAGENTA << "Trigger scheme set to : External Test Pulse Trigger [No TLU]" << RESET;
  for(auto cBoard : *fDetectorContainer) ConfigureTriggerScheme(cBoard, false, true, false);
}

void BeamTestCommissioning::SetExternalTestPulseTriggerTLU()
{
  LOG(INFO) << BOLDMAGENTA << "Trigger scheme set to : External Test Pulse Trigger with TLU" << RESET;
  for(auto cBoard : *fDetectorContainer) ConfigureTriggerScheme(cBoard, false, true, true);
}

void BeamTestCommissioning::SetExternalBeamTriggerTLU()
{
  LOG(INFO) << BOLDMAGENTA << "Trigger scheme set to : External Beam Trigger with TLU" << RESET;
  for(auto cBoard : *fDetectorContainer) ConfigureTriggerScheme(cBoard, false, false, true);
}

void BeamTestCommissioning::ConfigureTriggerScheme(Ph2_HwDescription::BeBoard* pBoard, bool pInternal, bool pEnableTP, bool pEnableTLU)
{
  BeBoardRegMap cBoardRegMap = pBoard->getBeBoardRegMap();

  //Decide of trigger source
  uint8_t cTriggerSource = 0;
  if(pInternal)
  {
    if(pEnableTP) cTriggerSource = 6;
    else cTriggerSource = 3;
  }
  else 
  {
    if(pEnableTLU) cTriggerSource = 4;
    else
    {
      if(pEnableTP) cTriggerSource = 13;
      else cTriggerSource = 5;
    }
  }

  uint32_t cTriggerFrequency = (pInternal == true) ? cBoardRegMap["fc7_daq_cnfg.fast_command_block.user_trigger_frequency"] : 0;
  uint32_t cDelayAfterFastReset = cBoardRegMap["test_pulse.delay_after_fast_reset"];
  uint32_t cDelayAfterTestPulse = cBoardRegMap["test_pulse.delay_after_test_pulse"];
  uint32_t cDelayBeforeNextPulse = cBoardRegMap["test_pulse.delay_before_next_pulse"];
  uint32_t cEnableFastReset = cBoardRegMap["test_pulse.en_fast_reset"];
  uint32_t cNTriggersToAccept = 0; //all triggers
  uint32_t cEnableHandshake = 0;
  uint32_t cEnableDIO5 = (pInternal == true) ? false : true;

  std::vector<std::pair<std::string, uint32_t>> cBoardRegisters{{"fc7_daq_cnfg.fast_command_block.trigger_source", cTriggerSource},
                                                      {"fc7_daq_cnfg.readout_block.global.data_handshake_enable", cEnableHandshake},
                                                      {"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerFrequency},
                                                      {"test_pulse.delay_after_fast_reset", cDelayAfterFastReset},
                                                      {"test_pulse.delay_after_test_pulse", cDelayAfterTestPulse},
                                                      {"test_pulse.delay_before_next_pulse", cDelayBeforeNextPulse},
                                                      {"test_pulse.en_fast_reset", cEnableFastReset},
                                                      {"fc7_daq_cnfg.fast_command_block.triggers_to_accept", cNTriggersToAccept},
                                                      {"fc7_daq_cnfg.tlu_block.tlu_enabled", pEnableTLU},
                                                      {"fc7_daq_cnfg.dio5_block.dio5_en", cEnableDIO5},
                                                      {"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1}};

  fBeBoardInterface->WriteBoardMultReg(pBoard, cBoardRegisters);

  LOG(INFO) << YELLOW << "Trigger source = " << +fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.trigger_source") << RESET;
  LOG(INFO) << YELLOW << "Data handshake enabled = " << +fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.readout_block.global.data_handshake_enable") << RESET;
  LOG(INFO) << YELLOW << "Trigger frequency = " << +fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.fast_command_block.user_trigger_frequency") << RESET;
  LOG(INFO) << YELLOW << "Delay after fast reset = " << +fBeBoardInterface->ReadBoardReg(pBoard, "test_pulse.delay_after_fast_reset") << RESET;
  LOG(INFO) << YELLOW << "Delay after test pulse = " << +fBeBoardInterface->ReadBoardReg(pBoard, "test_pulse.delay_after_test_pulse") << RESET;
  LOG(INFO) << YELLOW << "Delay beofre nest pulse = " << +fBeBoardInterface->ReadBoardReg(pBoard, "test_pulse.delay_before_next_pulse") << RESET;
  LOG(INFO) << YELLOW << "Enable fast reset = " << +fBeBoardInterface->ReadBoardReg(pBoard, "test_pulse.en_fast_reset") << RESET;
  LOG(INFO) << YELLOW << "TLU enabled = " << +fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.tlu_block.tlu_enabled") << RESET;
  LOG(INFO) << YELLOW << "DIO5 enabled = " << +fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.dio5_block.dio5_en") << RESET;

  fBeBoardInterface->Stop(pBoard);
  fBeBoardInterface->ChipReSync(pBoard);
}

void BeamTestCommissioning::ExtractHitCounts(const std::vector<Event*> pEvents, size_t pTriggerId)
{
  LOG(DEBUG) << BOLDMAGENTA << "Extracting Hit and Stub countd" << RESET;
  for(auto cBoard : *fDetectorContainer)
  {
    auto cBoardIndex = cBoard->getIndex();
    auto cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
    auto cEventIter = pEvents.begin() + pTriggerId;
    size_t cEventCount = 0, cMaxEventCount = (fNevents >= 10000) ? 10000 : fNevents;
    do
    {
      if(cEventIter >= pEvents.end()) break;
      for(auto cOpticalGroup : *cBoard)
      {
        auto cOpticalGroupIndex = cOpticalGroup->getIndex();
        for(auto cHybrid : *cOpticalGroup)
        {
          auto cHybridIndex = cHybrid->getIndex();
          auto cHybridId = cHybrid->getId();
          auto cL1Status = static_cast<D19cCic2Event*>(*cEventIter)->L1Status(cHybridId);
          auto cBxId = (*cEventIter)->BxId(cHybridId);
          auto cStubStatus = static_cast<D19cCic2Event*>(*cEventIter)->Status(cHybridId);
          LOG(DEBUG) << BOLDYELLOW << "Event : " << (*cEventIter)->GetEventCount() << " -- Hybrid : " << +cHybridId << " -- BxId : " << +cBxId << " -- L1 Status : " << std::bitset<9>(cL1Status) << " -- Stub Status : " << std::bitset<8>(cStubStatus) << RESET;
          for(auto cChip : *cHybrid)
          {
            auto cChipIndex = cChip->getIndex();
            auto cChipId = cChip->getId();
            //Get hits
            auto cHits = (*cEventIter)->GetHits(cHybridId, cChipId);
            fHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>() += cHits.size();
            LOG(DEBUG) << BOLDBLUE << "Event : " << (*cEventIter)->GetEventCount() << " -- Hybrid : " << +cHybridId << " -- Chip : " << +cChipId << " -- Hit count : " << +cHits.size() << RESET;
          }
        }
      }
      cEventIter += (1 + cTriggerMultiplicity);
      cEventCount++;
    } while(cEventIter < pEvents.end() && cEventCount < cMaxEventCount);
  }
}

void BeamTestCommissioning::ExtractStubCounts(const std::vector<Event*> pEvents, size_t pTriggerId)
{
  LOG(DEBUG) << BOLDMAGENTA << "Extracting Hit and Stub countd" << RESET;
  for(auto cBoard : *fDetectorContainer)
  {
    auto cBoardIndex = cBoard->getIndex();
    auto cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
    auto cEventIter = pEvents.begin() + pTriggerId;
    size_t cEventCount = 0, cMaxEventCount = (fNevents >= 10000) ? 10000 : fNevents;

    do
    {
      if(cEventIter >= pEvents.end()) break;
      for(auto cOpticalGroup : *cBoard)
      {
        auto cOpticalGroupIndex = cOpticalGroup->getIndex();
        for(auto cHybrid : *cOpticalGroup)
        {
          auto cHybridIndex = cHybrid->getIndex();
          auto cHybridId = cHybrid->getId();
          auto cL1Status = static_cast<D19cCic2Event*>(*cEventIter)->L1Status(cHybridId);
          auto cBxId = (*cEventIter)->BxId(cHybridId);
          auto cStubStatus = static_cast<D19cCic2Event*>(*cEventIter)->Status(cHybridId);
          LOG(DEBUG) << BOLDYELLOW << "Event : " << (*cEventIter)->GetEventCount() << " -- Hybrid : " << +cHybridId << " -- BxId : " << +cBxId << " -- L1 Status : " << std::bitset<9>(cL1Status) << " -- Stub Status : " << std::bitset<8>(cStubStatus) << RESET;
          for(auto cChip : *cHybrid)
          {
            auto cChipIndex = cChip->getIndex();
            auto cChipId = cChip->getId();
            //Get stubs
            auto cStubs = (*cEventIter)->StubVector(cHybridId, cChipId);
            fStubCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>() += cStubs.size();
            LOG(DEBUG) << BOLDBLUE << "Event : " << (*cEventIter)->GetEventCount() << " -- Hybrid : " << +cHybridId << " -- Chip : " << +cChipId << " -- Stub count : " << +cStubs.size() << RESET;
          }
        }
      }
      cEventIter += (1 + cTriggerMultiplicity);
      cEventCount++;
    } while(cEventIter < pEvents.end() && cEventCount < cMaxEventCount);
  }
}

void BeamTestCommissioning::ScanL1Latency()
{
  LOG(INFO) << BOLDYELLOW << "Starting L1 Latency Scan" << RESET;
  for(auto cBoard: *fDetectorContainer)
  {
      setSameDacBeBoard(cBoard, "TriggerLatency", fStartLatency);
      fBeBoardInterface->ChipReSync(cBoard);
  }
  DetectorDataContainer cMaximumHitCountContainer;
  ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cMaximumHitCountContainer);
  bool cDone = false;
  uint16_t cL1LatencyStep = 0;
  do
  {
    for(auto cBoard : *fDetectorContainer)
    {
      auto cBoardIndex = cBoard->getIndex();
      auto& cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
      for(auto cOpticalGroup : *cBoard)
      {
        auto cOpticalGroupIndex = cOpticalGroup->getIndex();
        for(auto cHybrid : *cOpticalGroup)
        {
          auto cHybridIndex = cHybrid->getIndex();
          for(auto cChip : *cHybrid)
          {
            auto cChipIndex = cChip->getIndex();
            auto cL1Latency = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
            LOG(INFO) << BOLDMAGENTA << "Chip : " << +cChip->getId() << " -- L1 Latency : " << +cL1Latency << RESET;
            for(size_t cTriggerId = 0; cTriggerId < cTriggerMultiplicity + 1; cTriggerId++)
            {
              fL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cL1LatencyStep * (1 + cTriggerMultiplicity) - cTriggerId] = cL1Latency;
            }
            cMaximumHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>() = 0;
          }//Chip
        }//Hybrid
      }//OpticalGroup
    }//Board
    LOG(INFO) << BOLDBLUE << "L1 Latency step : " << +cL1LatencyStep << RESET;
    ContinousReadout();

    for(auto cBoard : *fDetectorContainer)
    {
      auto cBoardIndex = cBoard->getIndex();
      auto cBoardId = cBoard->getId();
      fBeBoardInterface->setBoard(cBoardId);
      const std::vector<Event*>& cEvents = GetEvents();
      auto& cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
      LOG(INFO) << BOLDMAGENTA << "Board : " << +cBoardId << " -- Event size : " << +cEvents.size() << RESET;

      for(size_t cTriggerId = 0; cTriggerId < cTriggerMultiplicity + 1; cTriggerId++)
      {
        ExtractHitCounts(cEvents, cTriggerId);
        for(auto cOpticalGroup : *cBoard)
        {
          auto cOpticalGroupIndex = cOpticalGroup->getIndex();
          for(auto cHybrid : *cOpticalGroup)
          {
            auto cHybridIndex = cHybrid->getIndex();
            auto cHybridId = cHybrid->getId();
            for(auto cChip : *cHybrid)
            {
              auto cChipIndex = cChip->getIndex();
              auto cChipId = cChip->getId();
              auto cL1Latency = fL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cL1LatencyStep * (1 + cTriggerMultiplicity) - cTriggerId];
              auto cCurrHitCount = fHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>();
              fL1LatencyHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cL1LatencyStep * (1 + cTriggerMultiplicity) - cTriggerId] = cCurrHitCount;
              auto& cMaxHitCount = cMaximumHitCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>();
              auto& cOptimalL1Latency = fOptimalL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint16_t>();
              if(cCurrHitCount > 0 && cCurrHitCount >= cMaxHitCount)
              {
                cOptimalL1Latency = cL1Latency;
                cMaxHitCount = cCurrHitCount;
                LOG(INFO) << BOLDGREEN << "\t Hybrid : " << +cHybridId << " -- Chip : " << +cChipId << " -- L1 Latency : " << +cOptimalL1Latency << " -- New maximum hit count : " << +cMaxHitCount << RESET;
              }
              else
              {
                LOG(INFO) << BOLDBLUE << "\t Hybrid : " << +cHybridId << " -- Chip : " << +cChipId << " -- L1 Latency : " << +cL1Latency << " -- Current hit count : " << +cCurrHitCount << RESET;
              }
            }//Chip
          }//Hybrid
        }//OpticalGroup
      }//Trigger Id
    }//Board
    // update trigger latency for next step
    bool cAllDone = true;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cTriggerMultiplicity = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
        setSameDacBeBoard(cBoard, "TriggerLatency", fStartLatency + (1 + cL1LatencyStep) * (1 + cTriggerMultiplicity));
        cAllDone = cAllDone && ((1 + cL1LatencyStep) * (1 + cTriggerMultiplicity) >= fLatencyRange);
        fBeBoardInterface->ChipReSync(cBoard);
    }
    cDone = cAllDone;
    cL1LatencyStep++;
  } while(!cDone);
  LOG(INFO) << BOLDYELLOW << "Done with L1 Latency Scan" << RESET;

  //Set optimal latency to all chips
  for(auto cBoard : *fDetectorContainer)
  {
    auto cBoardIndex = cBoard->getIndex();
    for(auto cOpticalGroup : *cBoard)
    {
      auto cOpticalGroupIndex = cOpticalGroup->getIndex();
      for(auto cHybrid : *cOpticalGroup)
      {
        auto cHybridIndex = cHybrid->getIndex();
        auto cHybridId = cHybrid->getId();
        for(auto cChip : *cHybrid)
        {
          auto cChipIndex = cChip->getIndex();
          auto cChipId = cChip->getId();
          auto cOptimalLatency = fOptimalL1LatencyContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint16_t>();
          LOG(INFO) << BOLDMAGENTA << "Hybrid : " << +cHybridId << " -- Chip : " << +cChipId << " -- Optimal L1 Latency" << +cOptimalLatency << RESET;
          fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cOptimalLatency);
        }
      }
    }
    fBeBoardInterface->ChipReSync(cBoard);
  }
// #ifdef __USE_ROOT__
//   fDQMHistogrammer.FillL1LatencyPlots(fL1LatencyContainer, fL1LatencyHitCountContainer);
// #endif
}

void BeamTestCommissioning::ScanStubLatency()
{
  LOG(INFO) << BOLDYELLOW << "Starting Stub Latency Scan" << RESET;
  DetectorDataContainer cBoardL1LatencyContainer;
  ContainerFactory::copyAndInitBoard<uint16_t>(*fDetectorContainer, cBoardL1LatencyContainer);
  for(auto cBoard : *fDetectorContainer)
  {
    auto cBoardIndex = cBoard->getIndex();
    cBoardL1LatencyContainer.at(cBoardIndex)->getSummary<uint16_t>() = 0;
    auto cBoardId = cBoard->getId();
    uint16_t cMinL1Latency = 0;
    uint16_t cMaxL1Latency = 0;
    std::vector<uint16_t> cL1LatencyBins(512, 0);
    for(auto cOpticalGroup : *cBoard)
    {
      for(auto cHybrid : *cOpticalGroup)
      {
        for(auto cChip : *cHybrid)
        {
          auto cType = cChip->getFrontEndType();
          if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2) continue;
          auto cL1Latency = fReadoutChipInterface->ReadChipReg(cChip, "TriggerLatency");
          //increment if valid latency
          if(cL1Latency > 0) cL1LatencyBins[cL1Latency]++;
          //extract min and max L2 latencies
          if(cMinL1Latency == 0) cMinL1Latency == cL1Latency;
          if(cMaxL1Latency == 0) cMaxL1Latency == cL1Latency;
          if(cL1Latency < cMinL1Latency) cMinL1Latency = cL1Latency;
          if(cL1Latency > cMaxL1Latency) cMaxL1Latency = cL1Latency;
        }
      }
    }
    auto cModeL1Latency= std::max_element(cL1LatencyBins.begin(), cL1LatencyBins.end()) - cL1LatencyBins.begin();
    LOG(INFO) << BOLDMAGENTA << "Board : " << +cBoardId << " -- Min L1 Latency : " << +cMinL1Latency << " [40MHz Clk cycles]" << RESET;
    LOG(INFO) << BOLDMAGENTA << "Board : " << +cBoardId << " -- Max L1 Latency : " << +cMaxL1Latency << " [40MHz Clk cycles]" << RESET;
    LOG(INFO) << BOLDMAGENTA << "Board : " << +cBoardId << " -- Mode L1 Latency : " << +cModeL1Latency << " [40MHz Clk cycles]" << RESET;
    cBoardL1LatencyContainer.at(cBoardIndex)->getSummary<uint16_t>() = cModeL1Latency;
  }

  //check if stub alignment has already been run
  bool cAlignmentRun = true;
  for(auto cBoard: *fDetectorContainer) { cAlignmentRun = cAlignmentRun && (cBoard->getStubOffset() != 0); }
  int cOffset = 0;
  //if no alignment has been run .. use scan range
  if(!cAlignmentRun)
  {
    auto     cSetting   = fSettingsMap.find("StubAlignmentScanStart");
    uint32_t cScanStart = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<uint32_t>(cSetting->second) : 100;
    cOffset             = cScanStart;
  }
  else
  {
    cOffset = (int)(fLatencyRange / 2.);
  }

  DetectorDataContainer cMaximumStubCountContainer;
  ContainerFactory::copyAndInitBoard<uint32_t>(*fDetectorContainer, cMaximumStubCountContainer);
  size_t cStubLatencyStep = 0;
  do
  {
    // set stub latency on all BE boards
    std::vector<bool> cSetBoardLatencyContainer(false);
    for(auto cBoard: *fDetectorContainer)
    {
        auto  cBoardIndex = cBoard->getIndex();
        auto cBoardId = cBoard->getId();
        cMaximumStubCountContainer.at(cBoardIndex)->getSummary<uint32_t>() = 0;
        auto cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
        int cStubLatency = cBoardL1LatencyContainer.at(cBoardIndex)->getSummary<uint16_t>() - (cOffset - cStubLatencyStep * (1 + cTriggerMultiplicity));
        if(cStubLatency < 0)
        {
            LOG(INFO) << BOLDRED << "Board : " << +cBoardId << " -- Stub Latency [NEGATIVE] : " << +cStubLatency << " -- Trigger multiplicity : " << cTriggerMultiplicity << RESET;;
            cSetBoardLatencyContainer.at(cBoardIndex) = false;
            continue;
        }
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cStubLatency);
        fStubLatencyContainer.at(cBoardIndex)->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cStubLatencyStep] = cStubLatency;
        LOG(INFO) << BOLDBLUE << "Board : " << +cBoardId << " -- Stub Latency :  " << +cStubLatency << " -- Stub Offset : " << +(cBoardL1LatencyContainer.at(cBoardIndex)->getSummary<uint16_t>() - cStubLatency) << RESET;
        cSetBoardLatencyContainer.at(cBoardIndex) = true;
    }
    ContinousReadout();

    for(auto cBoard : *fDetectorContainer)
    {
      auto cBoardIndex = cBoard->getIndex();
      auto cBoardId = cBoard->getId();
      if(cSetBoardLatencyContainer.at(cBoardIndex) == false) continue;
      fBeBoardInterface->setBoard(cBoardId);
      const std::vector<Event*>& cEvents = GetEvents();
      auto cTriggerMultiplicity = fTriggerMultiplicityContainer.at(cBoardIndex)->getSummary<uint32_t>();
      for(size_t cTriggerId = 0; cTriggerId < cTriggerMultiplicity + 1; cTriggerId++)
      {
        ExtractStubCounts(cEvents, cTriggerId);
        uint32_t cCurrStubCount = 0;
        auto& cMaxStubCount = cMaximumStubCountContainer.at(cBoardIndex)->getSummary<uint32_t>();
        auto& cOptimalStubLatency = fOptimalStubLatencyContainer.at(cBoardIndex)->getSummary<uint16_t>();
        for(auto cOpticalGroup : *cBoard)
        {
          auto cOpticalGroupIndex = cOpticalGroup->getIndex();
          for(auto cHybrid : *cOpticalGroup)
          {
            auto cHybridIndex = cHybrid->getIndex();
            for(auto cChip : *cHybrid)
            {
              auto cChipIndex = cChip->getIndex();
              cCurrStubCount += fStubCountContainer.at(cBoardIndex)->at(cOpticalGroupIndex)->at(cHybridIndex)->at(cChipIndex)->getSummary<uint32_t>();
            }//Chip
          }//Hybrid
        }//OpticalGroup
        fStubLatencyStubCountContainer.at(cBoardIndex)->getSummary<GenericDataArray<VECSIZE, uint32_t>>()[cStubLatencyStep] = cCurrStubCount;
        if(cCurrStubCount >= cMaxStubCount)
        {
          cMaxStubCount = cCurrStubCount;
          cOptimalStubLatency = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay");;
          LOG(INFO) << BOLDGREEN << "\t Board : " << +cBoardId << " -- Trigger : " << +cTriggerId <<  " -- Stub Latency : " << +cOptimalStubLatency << " -- New maximum stub count : " << +cMaxStubCount << RESET;
        }
        else
        {
          LOG(INFO) << BOLDBLUE << "\t Board : " << +cBoardId << " -- Trigger : " << +cTriggerId << " -- Stub Latency : " << +cOptimalStubLatency << " -- Current stub count : " << +cCurrStubCount << RESET;
        }
      }
    }
  } while(cStubLatencyStep < fLatencyRange);
  // configure optimal stub latency
  for(auto cBoard: *fDetectorContainer)
  {
      auto cBoardIndex = cBoard->getIndex();
      auto cBoardId = cBoard->getId();
      auto& cOptimalStubLatency = fOptimalStubLatencyContainer.at(cBoardIndex)->getSummary<uint32_t>();
      LOG(INFO) << BOLDMAGENTA << "Board : " << +cBoardId << " -- Optimal Stub Latency" << +cOptimalStubLatency << RESET;
      fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.readout_block.global.common_stubdata_delay", cOptimalStubLatency);
  }
// #ifdef __USE_ROOT__
//   fDQMHistogrammer.FillStubLatencyPlots(fStubLatencyContainer, fStubLatencyHitCountContainer);
// #endif
  LOG(INFO) << BOLDYELLOW << "Done with Stub Latency Scan" << RESET;
}
