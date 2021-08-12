#include "OpenFinder.h"
#include "CBCChannelGroupHandler.h"
#include "ContainerFactory.h"
#include "DataContainer.h"
#include "Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OpenFinder::OpenFinder() : PSHybridTester()
{
    fParameters.fAntennaTriggerSource = 7;
    fParameters.antennaDelay          = 50;
    fParameters.potentiometer         = this->findValueInSettings<double>("AntennaPotentiometer");
    fParameters.nTriggers             = this->findValueInSettings<double>("Nevents");
}

OpenFinder::~OpenFinder() {}
void OpenFinder::Reset()
{
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());

        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                LOG(INFO) << BOLDBLUE << "Resetting all registers on readout chips connected to FEhybrid#" << (cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip) cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
    resetPointers();
}
void OpenFinder::Initialise(Parameters pParameters)
{
    fChannelGroupHandler = new CBCChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);

    // Read some settings from the map
    auto cSetting       = fSettingsMap.find("Nevents");
    fEventsPerPoint     = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 100;
    cSetting            = fSettingsMap.find("TestPulseAmplitude");
    fTestPulseAmplitude = (cSetting != std::end(fSettingsMap)) ? boost::any_cast<double>(cSetting->second) : 0;
    // Set fTestPulse based on the test pulse amplitude
    fTestPulse = (fTestPulseAmplitude & 0x1);
    // Import the rest of parameters from the user settings
    fParameters = pParameters;

    // set the antenna switch min and max values
    int cAntennaSwitchMinValue = (fParameters.antennaGroup > 0) ? fParameters.antennaGroup : 1;
    int cAntennaSwitchMaxValue = (fParameters.antennaGroup > 0) ? (fParameters.antennaGroup + 1) : 5;

    // prepare container
    ContainerFactory::copyAndInitStructure<ChannelList>(*fDetectorContainer, fOpens);

    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitStructure<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitStructure<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    ContainerFactory::copyAndInitStructure<ScanSummaries>(*fDetectorContainer, fInTimeOccupancy);
    for(auto cBoard: *fDetectorContainer)
    {
        fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>() = static_cast<BeBoard*>(cBoard)->getBeBoardRegMap();
        auto& cRegMapThisBoard                                                 = fRegMapContainer.at(cBoard->getIndex());
        auto& cOpens                                                           = fOpens.at(cBoard->getIndex());
        auto& cOccupancy                                                       = fInTimeOccupancy.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cOpensOpticalGroup      = cOpens->at(cOpticalGroup->getIndex());
            auto& cOccupancyOpticalGroup  = cOccupancy->at(cOpticalGroup->getIndex());
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());

            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cOpensHybrid      = cOpensOpticalGroup->at(cHybrid->getIndex());
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                auto& cOccupancyHybrid  = cOccupancyOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    cOpensHybrid->at(cChip->getIndex())->getSummary<ChannelList>().clear();
                    cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>() = static_cast<ReadoutChip*>(cChip)->getRegMap();
                    auto& cThisOcc                                                     = cOccupancyHybrid->at(cChip->getIndex())->getSummary<ScanSummaries>();
                    for(int cAntennaPosition = cAntennaSwitchMinValue; cAntennaPosition < cAntennaSwitchMaxValue; cAntennaPosition++)
                    {
                        ScanSummary cSummary;
                        cSummary.first  = 0;
                        cSummary.second = 0;
                        cThisOcc.push_back(cSummary);
                    }
                }
            }
        }
    }
}
// Antenna map generator by Sarah (used to be in Tools.cc)
// TO-DO - generalize for other hybrids
// I think this is really the only thing that needs to change between
// 2S and PS
OpenFinder::antennaChannelsMap OpenFinder::returnAntennaMap()
{
    antennaChannelsMap cAntennaMap;
    for(int cAntennaSwitch = 1; cAntennaSwitch < 5; cAntennaSwitch++)
    {
        std::vector<int> cOffsets(2);
        if((cAntennaSwitch - 1) % 2 == 0)
        {
            cOffsets[0] = 0 + (cAntennaSwitch > 2);
            cOffsets[1] = 2 + (cAntennaSwitch > 2);
        }
        else
        {
            cOffsets[0] = 2 + (cAntennaSwitch > 2);
            cOffsets[1] = 0 + (cAntennaSwitch > 2);
        }
        cbcChannelsMap cTmpMap;
        for(int cCbc = 0; cCbc < 8; cCbc++)
        {
            int           cOffset = cOffsets[(cCbc % 2)];
            channelVector cTmpList;
            cTmpList.clear();
            for(int cChannel = cOffset; cChannel < 254; cChannel += 4) { cTmpList.push_back(cChannel); }
            cTmpMap.emplace(cCbc, cTmpList);
        }
        cAntennaMap.emplace(cAntennaSwitch, cTmpMap);
    }
    return cAntennaMap;
}

bool OpenFinder::FindLatency(BeBoard* pBoard, std::vector<uint16_t> pLatencies)
{
    LOG(INFO) << BOLDBLUE << "Scanning latency to find charge injected by antenna in time ..." << RESET;
    // Preparing the antenna map, the list of opens and the hit counter
    auto  cAntennaMap            = returnAntennaMap();
    int   cAntennaSwitchMinValue = (fParameters.antennaGroup > 0) ? fParameters.antennaGroup : 1;
    auto  cBeBoard               = static_cast<BeBoard*>(pBoard);
    auto& cSummaryThisBoard      = fInTimeOccupancy.at(pBoard->getIndex());
    auto  cSearchAntennaMap      = cAntennaMap.find(fAntennaPosition);
    // scan latency and record optimal latency
    for(auto cLatency: pLatencies)
    {
        setSameDacBeBoard(cBeBoard, "TriggerLatency", cLatency);
        fBeBoardInterface->ChipReSync(cBeBoard); // NEED THIS! ??
        LOG(DEBUG) << BOLDBLUE << "L1A latency set to " << +cLatency << RESET;
        this->ReadNEvents(cBeBoard, fEventsPerPoint);
        const std::vector<Event*>& cEvents = this->GetEvents();
        for(auto cOpticalGroup: *pBoard)
        {
            auto& cSummaryThisOpticalGroup = cSummaryThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cSummaryThisHybrid = cSummaryThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cSummaryThisChip = cSummaryThisHybrid->at(cChip->getIndex());
                    auto& cSummary         = cSummaryThisChip->getSummary<ScanSummaries>()[fAntennaPosition - cAntennaSwitchMinValue];

                    auto     cConnectedChannels = cSearchAntennaMap->second.find((int)cChip->getId())->second;
                    uint32_t cOccupancy         = 0;
                    for(auto cEvent: cEvents)
                    {
                        auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                        for(auto cHit: cHits)
                        {
                            if(std::find(cConnectedChannels.begin(), cConnectedChannels.end(), cHit) != cConnectedChannels.end()) { cOccupancy++; }
                        }
                    }
                    float cEventOccupancy = cOccupancy / static_cast<float>(fEventsPerPoint * cConnectedChannels.size());
                    if(cEventOccupancy >= cSummary.second)
                    {
                        cSummary.first  = cLatency;
                        cSummary.second = cEventOccupancy;
                    }
                    LOG(DEBUG) << BOLDBLUE << "On average " << cEventOccupancy * 100 << " of events readout from chip " << +cChip->getId() << " contain a hit." << RESET;
                }
            }
        }
    }

    // set optimal latency for each chip
    bool cFailed = false;
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cSummaryThisOpticalGroup = cSummaryThisBoard->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cSummaryThisHybrid = cSummaryThisOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                auto& cSummaryThisChip = cSummaryThisHybrid->at(cChip->getIndex())->getSummary<ScanSummaries>()[fAntennaPosition - cAntennaSwitchMinValue];
                auto  cReadoutChip     = static_cast<ReadoutChip*>(cChip);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "TriggerLatency", cSummaryThisChip.first);
                LOG(INFO) << BOLDBLUE << "Optimal latency "
                          << " for chip " << +cChip->getId() << " was " << cSummaryThisChip.first << " hit occupancy " << cSummaryThisChip.second << RESET;

                if(cSummaryThisChip.second == 0)
                {
                    LOG(INFO) << BOLDRED << "FAILED to find optimal latency "
                              << " for chip " << +cChip->getId() << " hit occupancy " << cSummaryThisChip.second << RESET;
                    cFailed = (cFailed || true);
                }
            }
        }
    }
    return !cFailed;
}

void OpenFinder::CountOpens(BeBoard* pBoard)
{
    DetectorDataContainer cMeasurement;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, cMeasurement);

    // Preparing the antenna map, the list of opens and the hit counter
    auto cAntennaMap       = returnAntennaMap();
    auto cBeBoard          = static_cast<BeBoard*>(pBoard);
    auto cSearchAntennaMap = cAntennaMap.find(fAntennaPosition);
    // scan latency and record optimal latency
    this->ReadNEvents(cBeBoard, fEventsPerPoint);
    const std::vector<Event*>& cEvents = this->GetEvents();

    auto& cOpens            = fOpens.at(pBoard->getIndex());
    auto& cSummaryThisBoard = cMeasurement.at(pBoard->getIndex());
    for(auto cOpticalGroup: *pBoard)
    {
        auto& cOpensThisOpticalGroup   = cOpens->at(cOpticalGroup->getIndex());
        auto& cSummaryThisOpticalGroup = cSummaryThisBoard->at(cOpticalGroup->getIndex());
        for(auto cHybrid: *cOpticalGroup)
        {
            auto& cOpensThisHybrid   = cOpensThisOpticalGroup->at(cHybrid->getIndex());
            auto& cSummaryThisHybrid = cSummaryThisOpticalGroup->at(cHybrid->getIndex());
            for(auto cChip: *cHybrid)
            {
                auto  cConnectedChannels = cSearchAntennaMap->second.find((int)cChip->getId())->second;
                auto& cSummaryThisChip   = cSummaryThisHybrid->at(cChip->getIndex());
                for(auto cEvent: cEvents)
                {
                    auto cHits = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                    for(auto cConnectedChannel: cConnectedChannels)
                    {
                        if(std::find(cHits.begin(), cHits.end(), cConnectedChannel) == cHits.end()) { cSummaryThisChip->getChannelContainer<Occupancy>()->at(cConnectedChannel).fOccupancy += 1; }
                    }
                }

                auto& cOpensThisChip = cOpensThisHybrid->at(cChip->getIndex())->getSummary<ChannelList>();
                for(auto cConnectedChannel: cConnectedChannels)
                {
                    if(cSummaryThisChip->getChannelContainer<Occupancy>()->at(cConnectedChannel).fOccupancy > THRESHOLD_OPEN * fEventsPerPoint)
                    {
                        cOpensThisChip.push_back(cConnectedChannel);
                        LOG(DEBUG) << BOLDRED << "Possible open found.."
                                   << " readout chip " << +cChip->getId() << " channel " << +cConnectedChannel << RESET;
                    }
                }
            }
        }
    }
}

#ifdef __USE_ROOT__
void OpenFinder::Print()
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cOpens = fOpens.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            // create TTree for opens: opensTree
            auto OpensTree = new TTree("Opens", "Open channels in the hybrid");
            // create int for all opens count
            int32_t totalOpens = 0;
            // create variables for TTree branches
            int              nCBC = -1;
            std::vector<int> openChannels;
            // create branches
            OpensTree->Branch("CBC", &nCBC);
            OpensTree->Branch("Channels", &openChannels);

            auto& cOpensThisOpticalGroup = cOpens->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cOpensThisHybrid = cOpensThisOpticalGroup->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto& cOpensThisChip = cOpensThisHybrid->at(cChip->getIndex())->getSummary<ChannelList>();

                    // empty openChannels vector
                    openChannels.clear();

                    if(cOpensThisChip.size() == 0)
                    {
                        LOG(INFO) << BOLDGREEN << "No opens found "
                                  << "on readout chip " << +cChip->getId() << " hybrid " << +cHybrid->getId() << RESET;
                    }
                    else
                    {
                        LOG(INFO) << BOLDRED << "Found " << +cOpensThisChip.size() << " opens in readout chip" << +cChip->getId() << " on FE hybrid " << +cHybrid->getId() << RESET;
                        // add number of opens to total opens counts
                        totalOpens += cOpensThisChip.size();
                    }
                    for(auto cOpenChannel: cOpensThisChip)
                    {
                        LOG(DEBUG) << BOLDRED << "Possible open found.."
                                   << " readout chip " << +cChip->getId() << " channel " << +cOpenChannel << RESET;
                        // store opens on opensChannels vector
                        openChannels.push_back(cOpenChannel);
                    }
                    // store chip opens on OpensTree
                    nCBC = cChip->getId() + 1;
                    OpensTree->Fill();
                }
            }
            // fillSummaryTree( "nOpens", totalOpens );
            fResultFile->cd();
            OpensTree->Write();
            if(totalOpens == 0) { gDirectory->Delete("Opens;*"); }
        }
    }
}
#endif

void OpenFinder::FindOpens2S()
{
#ifdef __ANTENNA__
    // The main antenna object is needed here
    // Antenna cAntenna;
    Antenna cAntenna = Antenna(fParameters.UsbId.c_str());
    // Trigger source for the antenna
    cAntenna.SelectTriggerSource(fParameters.fAntennaTriggerSource);
    // Configure SPI (again?) and the clock
    cAntenna.ConfigureClockGenerator(CLOCK_SLAVE, 8, 0);
    // Configure bias for antenna pull-up
    cAntenna.ConfigureDigitalPotentiometer(POTENTIOMETER_SLAVE, fParameters.potentiometer);
    // Configure communication with analogue switch
    cAntenna.ConfigureAnalogueSwitch(SWITCH_SLAVE);
    // set the antenna switch min and max values
    int cAntennaSwitchMinValue = (fParameters.antennaGroup > 0) ? fParameters.antennaGroup : 1;
    int cAntennaSwitchMaxValue = (fParameters.antennaGroup > 0) ? (fParameters.antennaGroup + 1) : 5;
    LOG(INFO) << BOLDBLUE << "Will switch antenna between chanels " << +cAntennaSwitchMinValue << " and  " << cAntennaSwitchMaxValue << RESET;
    // Set the antenna delay and compute the corresponding latency start and stop
    uint16_t cTriggerRate = 10;
    static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureAntennaFSM(fEventsPerPoint, cTriggerRate, fParameters.antennaDelay);

    uint16_t cStart = fParameters.antennaDelay - 1;
    uint16_t cStop  = fParameters.antennaDelay + (fParameters.latencyRange) + 1;
    LOG(INFO) << BOLDBLUE << "Antenna delay set to " << +fParameters.antennaDelay << " .. will scan L1 latency between " << +cStart << " and " << +cStop << RESET;
    // Loop over the antenna groups
    cAntenna.TurnOnAnalogSwitchChannel(9);

    // Latency range based on step 1
    std::vector<DetectorDataContainer*> cContainerVector;
    std::vector<uint16_t>               cListOfLatencies;
    for(int cLatency = cStart; cLatency < cStop; ++cLatency)
    {
        cListOfLatencies.push_back(cLatency);
        cContainerVector.emplace_back(new DetectorDataContainer());
        ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *cContainerVector.back());
    }

    for(fAntennaPosition = cAntennaSwitchMinValue; fAntennaPosition < cAntennaSwitchMaxValue; fAntennaPosition++)
    {
        LOG(INFO) << BOLDBLUE << "Looking for opens using antenna channel " << +fAntennaPosition << RESET;
        // Switching the antenna to the correct group
        cAntenna.TurnOnAnalogSwitchChannel(fAntennaPosition);

        for(auto cBoard: *fDetectorContainer)
        {
            auto cBeBoard = static_cast<BeBoard*>(cBoard);
            bool cSuccess = this->FindLatency(cBeBoard, cListOfLatencies);
            if(cSuccess)
                this->CountOpens(cBeBoard);
            else
                exit(FAILED_LATENCY);
        }

        // de-select all channels
        cAntenna.TurnOnAnalogSwitchChannel(9);
    }
#endif
}
void OpenFinder::SelectAntennaPosition(const std::string& cPosition)
{
#ifdef __TCUSB__
    auto cMapIterator = fAntennaControl.find(cPosition);
    if(cMapIterator != fAntennaControl.end())
    {
        auto& cChannel = cMapIterator->second;
        LOG(INFO) << BOLDBLUE << "Selecting antenna channel to "
                  << " inject charge in [ " << cPosition << " ] position. This is switch position " << +cChannel << RESET;
        TC_PSFE cTC_PSFE;
        cTC_PSFE.antenna_fc7(fParameters.potentiometer, cChannel);
        // measure level using on-board ADC
        std::vector<float> cMeasurements(3, 0.);
        for(int cIndex = 0; cIndex < 3; cIndex++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            cTC_PSFE.adc_get(TC_PSFE::measurement::ANT_PULL, cMeasurements[cIndex]);
        }
        auto cMeasurement = this->getStats(cMeasurements);
        LOG(INFO) << BOLDBLUE << "Antenna Pull-up Measurement : " << cMeasurement.first << " mV on average " << cMeasurement.second << " mV rms. " << RESET;
    }
#endif
}
void OpenFinder::FindOpensPS()
{
    fParameters.potentiometer = this->findValueInSettings<double>("AntennaPotentiometer");
    fParameters.nTriggers     = this->findValueInSettings<double>("Nevents");

    LOG(INFO) << BOLDBLUE << "Checking for opens in PS hybrid "
              << " antenna potentiometer will be set to 0x" << std::hex << fParameters.potentiometer << std::dec << " units."
              << "Going to ask for " << +fParameters.nTriggers << " events." << RESET;
    LOG(INFO) << BOLDBLUE << "Hyrbid voltages BEFORE selecting antenna position" << RESET;
    SelectAntennaPosition("Disable");

    // make sure that async mode is selected
    // that antenna source is 10
    // and set thresholds
    uint16_t cThreshold = this->findValueInSettings<double>("ThresholdForOpens");
    for(auto cBoard: *fDetectorContainer)
    {
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 0});
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 1});
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                    {
                        fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1);
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 0);
                    }
                }
            }
        }
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
    }

    // PS antenna connected to either even or odd channels
    std::vector<uint8_t> cPositions{0, 1};
    for(auto cPosition: cPositions)
    {
        // select antenna position
        SelectAntennaPosition((cPosition == 0) ? "EvenChannels" : "OddChannels");

        // check counters
        for(auto cBoard: *fDetectorContainer)
        {
            BeBoard* cBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));
            // cBeBoard->setEventType(EventType::SSAAS);

            this->ReadNEvents(cBeBoard, fParameters.nTriggers);
            std::stringstream          outp;
            const std::vector<Event*>& cEvents     = this->GetEvents();
            bool                       cOpensFound = false;
            for(auto cEvent: cEvents)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() != FrontEndType::SSA) continue;
                            // if( cChip->getId() != 0 )
                            //  continue;

                            LOG(INFO) << BOLDBLUE << "SSA#" << +cChip->getId() << RESET;
                            // auto cNhits = cEvent->GetNHits( cHybrid->getId(), cChip->getId());
                            auto cHitVector = cEvent->GetHits(cHybrid->getId(), cChip->getId());
                            for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
                            {
                                if(iChannel % 2 != cPosition)
                                {
                                    LOG(DEBUG) << BOLDMAGENTA << "\t\t... "
                                               << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits " << RESET;
                                    continue;
                                }
                                else
                                {
                                    LOG(DEBUG) << BOLDBLUE << "\t... "
                                               << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits " << RESET;
                                    if(cHitVector[iChannel] <= (1.0 - THRESHOLD_OPEN) * fParameters.nTriggers)
                                    {
                                        cOpensFound = true;
                                        LOG(INFO) << BOLDRED << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                  << +fParameters.nTriggers << " were expected." << RESET;
                                    }
                                    else
                                    {
                                        LOG(DEBUG) << BOLDGREEN << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                   << +fParameters.nTriggers << " were expected." << RESET;
                                    }
                                }
                            } // chnl
                        }     // chip
                    }         // hybrid
                }             // opticalGroup
            }
            if(!cOpensFound) LOG(INFO) << BOLDGREEN << "No opens found on this hybrid." << RESET;
        }
        // disable
        SelectAntennaPosition("Disable");
    }

    // std::this_thread::sleep_for (std::chrono::milliseconds (10000) );
    fParameters.potentiometer = 512;
    SelectAntennaPosition("Disable");
    // check counters
}
void OpenFinder::FindOpens() {}
