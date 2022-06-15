#if defined(__TCUSB__) && defined(__USE_ROOT__) && defined(__ANTENNA__)
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
    CBCChannelGroupHandler theChannelGroupHandler;
    theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
    setChannelGroupHandler(theChannelGroupHandler);
    // fChannelGroupHandler = new CBCChannelGroupHandler();
    // fChannelGroupHandler->setChannelGroupParameters(16, 2);

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
        setSameDacBeBoard(static_cast<BeBoard*>(cBeBoard), "TriggerLatency", cLatency);
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
                auto& cOpensThisHybrid = cOpensThisOpticalGroup->at(cOpticalGroup->getIndex());
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
            fillSummaryTree("nOpens", totalOpens);
            fResultFile->cd();
            OpensTree->Write();
            if(totalOpens == 0) { gDirectory->Delete("Opens;*"); }
        }
    }
}

void OpenFinder::FindOpens2S()
{
    #if defined(__ANTENNA__)
    // The main antenna object is needed here
    // Antenna cAntenna;
    // Antenna cAntenna = Antenna(fParameters.UsbId.c_str());
    Antenna cAntenna = Antenna();
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
void OpenFinder::SelectAntennaPosition(const std::string& cPosition, uint16_t potentiometer)
{
    if(potentiometer != 0) fParameters.potentiometer = potentiometer;
#if defined(__TCUSB__)
    auto cMapIterator = fAntennaControl.find(cPosition);
    if(cMapIterator != fAntennaControl.end())
    {
        auto& cChannel = cMapIterator->second;
        LOG(INFO) << BOLDBLUE << "Selecting antenna channel to "
                  << " inject charge in [ " << cPosition << " ] position. This is switch position " << +cChannel << RESET;
        TC_PSFE cTC_PSFE;
        cTC_PSFE.antenna_fc7(fParameters.potentiometer, cChannel);

        LOG(INFO) << "Antenna set" << RESET;

        float measurement;
        cTC_PSFE.adc_get(TC_PSFE::measurement::ANT_PULL, measurement);
        LOG(INFO) << BOLDBLUE << "Antenna Pull-up Measurement : " << measurement << " mV." << RESET;
        if(cPosition == "Disable") ReadAntennaVoltage();
    }
#endif
}

void OpenFinder::FindOpensPS()
{
#if defined(__TCUSB__)
    float   measurement;
    TC_PSFE cTC_PSFE;
    cTC_PSFE.adc_get(TC_PSFE::measurement::_3V3, measurement);
    LOG(INFO) << "3V3 -> " << +measurement << RESET;

    uint16_t antennaPullupLowEnd  = this->findValueInSettings<double>("AntennaPotentiometerLowEnd");
    uint16_t antennaPullupHighEnd = this->findValueInSettings<double>("AntennaPotentiometerHighEnd");
    fParameters.nTriggers         = this->findValueInSettings<double>("Nevents");

    std::vector<TH2F*> fOccupancyHistVect;
    // uint16_t antennaPullupLowEnd = 540;
    // uint16_t antennaPullupHighEnd = 700;
    uint16_t antennaPullup = (antennaPullupHighEnd - antennaPullupLowEnd) / 2 + antennaPullupLowEnd;
    // fParameters.potentiometer = antennaPullup;

    LOG(DEBUG) << BOLDBLUE << "Checking for opens in PS hybrid "
               << " antenna potentiometer will be set to 0x" << std::hex << fParameters.potentiometer << std::dec << " units."
               << "Going to ask for " << +fParameters.nTriggers << " events." << RESET;
    LOG(INFO) << BOLDBLUE << "Hybrid voltages BEFORE selecting antenna position" << RESET;
    SelectAntennaPosition("Disable");

    std::vector<uint16_t> finalAntennaOdd;
    std::vector<uint16_t> finalAntennaEven;
    // uint16_t              finalAntenna = 0;

    // int    crosstalk_channels     = 0;
    int    high_outliers_channels = 0;
    double occupancy_avg          = 0;
    // double occupancy_crosstk = 0;

    std::vector<bool> AntennaOdd_set;
    std::vector<bool> AntennaEven_set;
    bool              antenna_set = false;
    // make sure that async mode is selected
    // that antenna source is 10
    // and set thresholds
    uint16_t cThreshold = this->findValueInSettings<double>("ThresholdForOpens");
    for(auto cBoard: *fDetectorContainer)
    {
        // make sure async mode is enabled
        setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "AnalogueAsync", 1);
        // first .. set injection amplitude to 0
        setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", 0);
        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 0});
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 1});

        // std::vector<std::pair<std::string, uint32_t>> cRegVec;
        // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 10});
        // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.cal_pulse", 0});
        // cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.ps_async_en.antenna", 1});
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        double cPedeMean   = getSummaryParameter(Form("AvgPedeSSA%d", cChip->getId()));
                        double cPedeStdDev = getSummaryParameter(Form("StDvPedeSSA%d", cChip->getId()));
                        LOG(INFO) << "Mean:" << cPedeMean << RESET;
                        LOG(INFO) << "StdDev:" << cPedeStdDev << RESET;

                        if(cPedeMean != -1.0)
                        {
                            if(cPedeMean <= cPedeStdDev*2)
                            {
                                cThreshold = (uint16_t)(cPedeMean);
                            }
                            else{
                                if((cPedeMean + 3 * cPedeStdDev) <= 255) { cThreshold = (uint16_t)(cPedeMean + 3 * cPedeStdDev); }
                            }
                            LOG(INFO) << BOLDBLUE << "Threshold  " << cThreshold << RESET;
                        }
                        else
                        {
                            cThreshold = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                        }
                        // cThreshold = 12;
                        std::string tmpParameter = "thresholdForOpens_" + std::to_string(cChip->getId());
#if defined(__USE_ROOT__)
                        fillSummaryTree(tmpParameter, cThreshold);
#endif

                        // std::string cHistName  = Form("AntennaOccupancy_Even_%d", cChip->getId());
                        // if ( gROOT->FindObject(cHistName.c_str()) != nullptr )
                        //     cHistName  = Form("%s_%s", cHistName.c_str(), "II" );
                        // std::string cHistTitle = Form("Occupancy in Even Channels in Chip %d", cChip->getId());
                        // TH2F*       fOccupancyHistEven =
                        //     new TH2F(cHistName.c_str(), cHistTitle.c_str(), (antennaPullupHighEnd - antennaPullupLowEnd), antennaPullupLowEnd -0.5, antennaPullupHighEnd+0.5, 120, -0.5,
                        //     120+0.5);
                        // cHistName  = Form("AntennaOccupancy_Odd_%d", cChip->getId());
                        // if ( gROOT->FindObject(cHistName.c_str()) != nullptr )
                        //     cHistName  = Form("%s_%s", cHistName.c_str(), "II" );
                        // cHistTitle = Form("Occupancy in Odd Channels in Chip %d", cChip->getId());
                        // TH2F* fOccupancyHistOdd =
                        //     new TH2F(cHistName.c_str(), cHistTitle.c_str(), (antennaPullupHighEnd - antennaPullupLowEnd), antennaPullupLowEnd-0.5, antennaPullupHighEnd+0.5, 120, -0.5, 120+0.5);

                        // fOccupancyHistVect.push_back(fOccupancyHistEven);
                        // fOccupancyHistVect.push_back(fOccupancyHistOdd);

                        finalAntennaEven.push_back(0);
                        finalAntennaOdd.push_back(0);

                        fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1);
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThreshold);
                        fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 0);
                        // fReadoutChipInterface->WriteChipReg(cChip, "SAMPLINGMODE", 0);
                    }
                }
            }
        }
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
    }
    // fBeBoardInterface->WriteBoardMultReg (cBoard, cRegVec);

    // For DEBUG. Sweep antenna value
    //   TH1I* occupancyHist = new TH1I("OccupHist", "occupancy histogram for stddev", 60, 0, 60);
    // while(true)
    // {
    //     antennaPullup = 750;
    //   for(antennaPullup = antennaPullupLowEnd; antennaPullup < antennaPullupHighEnd; antennaPullup += 4)
    //   {
    //       std::vector<uint8_t> cPositions{0, 1};
    //       for(auto cPosition: cPositions)
    //       {
    //           //   if(cPosition == 1)
    //           //   continue;
    //           LOG(INFO) << " Antenna pull up : " << +antennaPullup << RESET;

    //           std::string chn = (cPosition == 0) ? "even" : "odd";

    //           //   std::string cHistName  = Form("OccupHist_%d", cChip->getId());
    //           //   TH1I* occupancyHist = new TH1I( cHistName.c_str(), "occupancy histogram for stddev", 60, 0, 60);

    //           fParameters.potentiometer = antennaPullup;
    //           // select antenna position
    //           SelectAntennaPosition((cPosition == 0) ? "EvenChannels" : "OddChannels");

    //           for(auto cBoard: *fDetectorContainer)
    //           {
    //               for(auto cOpticalGroup: *cBoard)
    //               {
    //                   for(auto cHybrid: *cOpticalGroup)
    //                   {
    //                       for(auto cChip: *cHybrid)
    //                       {
    //                           if(cChip->getFrontEndType() == FrontEndType::SSA)
    //                           {
    //                               //   TH2F* fOccupancyHist = fOccupancyHistVect[cChip->getIndex()+cPosition];

    //                               //   fOccupancyHist->Clear();

    //                               BeBoard* cBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));
    //                               this->ReadNEvents(cBeBoard, fParameters.nTriggers);
    //                               const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
    //                               for(auto cEvent: cEvents)
    //                               {
    //                                   auto cNhits     = cEvent->GetNHits(cHybrid->getId(), cChip->getId());
    //                                   auto cHitVector = cEvent->GetHits(cHybrid->getId(), cChip->getId());

    //                                   for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
    //                                   {
    //                                       if(iChannel % 2 == cPosition)
    //                                       {
    //                                           occupancy_avg += cHitVector[iChannel];
    //                                           fOccupancyHistVect[2 * cChip->getIndex() + cPosition]->Fill(antennaPullup, iChannel, cHitVector[iChannel]);
    //                                           if(cHitVector[iChannel] >= fParameters.nTriggers)
    //                                           {
    //                                               high_outliers_channels++;
    //                                               LOG(DEBUG) << "High outlier " << +iChannel << RESET;
    //                                           }
    //                                       }
    //                                   } // chnl
    //                               }
    //                               //   LOG(INFO) << "Std dev for " << chn << " channels of chip " << +cChip->getId() << ": " << +occupancyHist->GetStdDev() << RESET;
    //                               //   fOccupancyHist->Fill(antennaPullup, occupancy_avg, occupancyHist->GetStdDev());
    //                               //   fOccupancyHist->Fill(antennaPullup, 2*occupancy_avg/cChip->size());
    //                           }
    //                       }
    //                   }
    //               }
    //           }
    //       }
    //       //   }
    //   }

    //  for (int i=0; i+1<int(fOccupancyHistVect.size()); i+=2) {
    //     auto fOccupancyHistEven = fOccupancyHistVect[i];
    //     auto fOccupancyHistOdd = fOccupancyHistVect[i+1];
    //     fOccupancyHistEven->Draw();
    //     fOccupancyHistOdd->Draw();
    //     // fOccupancyHistEven->Write();
    //     // fOccupancyHistOdd->Write();
    //  }

    //   return;

    LOG(INFO) << BOLDMAGENTA << "Finding the optimal values for the antenna potentiometer..." << RESET;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        std::vector<uint8_t> cPositions{0, 1};
                        for(auto cPosition_index: cPositions)
                        {
                            // uint8_t cPosition = 1-cPosition_index;
                            uint8_t cPosition = cPosition_index;
                            antenna_set       = false;
                            for(int i = 0; i < 2 && !antenna_set; i++)
                            {
                                std::string chn = (cPosition == 0) ? "even" : "odd";

                                LOG(INFO) << BOLDMAGENTA << "Finding optimal antenna value for the " << chn << " channels of chip " << +cChip->getId() << RESET;

                                antennaPullupLowEnd  = this->findValueInSettings<double>("AntennaPotentiometerLowEnd");
                                antennaPullupHighEnd = this->findValueInSettings<double>("AntennaPotentiometerHighEnd");

                                // BINARY SEARCH
                                uint8_t nTries = 0;
                                while(!antenna_set)
                                {
                                    nTries++;
                                    if(antennaPullupHighEnd < antennaPullupLowEnd)
                                    {
                                        LOG(INFO) << BOLDRED << "Could not find a valid antenna value for " << chn << " channels of chip " << +cChip->getId() << "!!" << RESET;
                                        break;
                                    }

                                    antennaPullup = (antennaPullupHighEnd + antennaPullupLowEnd) / 2;

                                    fParameters.potentiometer = antennaPullup;
                                    // select antenna position
                                    SelectAntennaPosition((cPosition == 0) ? "EvenChannels" : "OddChannels");
                                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                                    // check counters
                                    BeBoard* cBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));
                                    this->ReadNEvents(cBeBoard, fParameters.nTriggers);
                                    const std::vector<Event*>& cEvents = this->GetEvents();
                                    // const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
                                    for(auto cEvent: cEvents)
                                    {
                                        // auto cNhits     = cEvent->GetNHits(cHybrid->getId(), cChip->getId());
                                        auto cHitVector = cEvent->GetHits(cHybrid->getId(), cChip->getId());

                                        for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
                                        {
                                            if(iChannel % 2 == cPosition)
                                            {
                                                occupancy_avg += cHitVector[iChannel];
                                                if(cHitVector[iChannel] >= fParameters.nTriggers)
                                                {
                                                    high_outliers_channels++;
                                                    LOG(DEBUG) << "High outlier " << +iChannel << RESET;
                                                }
                                            }
                                        } // chnl

                                        // Find the average ocupancy of the channels
                                        occupancy_avg = occupancy_avg / (cChip->size() / 2 * fParameters.nTriggers);
                                        LOG(INFO) << "Occupancy on " << chn << " channels of chip " << +cChip->getId() << " for antenna value " << fParameters.potentiometer << ": " << BOLDBLUE
                                                  << occupancy_avg << RESET;

                                        if(occupancy_avg >= 0.90 && occupancy_avg <= 0.99)
                                        {
                                            antenna_set = true;
                                            LOG(INFO) << BOLDGREEN << "Antenna value for " << chn << " channels of chip " << +cChip->getId() << " set to " << antennaPullup << RESET;
                                            if(cPosition == 0)
                                                finalAntennaEven[cChip->getIndex()] = antennaPullup;
                                            else
                                                finalAntennaOdd[cChip->getIndex()] = antennaPullup;
                                        }
                                        else if(occupancy_avg < 0.90)
                                        {
                                            antennaPullupLowEnd = antennaPullup + 1;
                                        }
                                        else if(occupancy_avg > 0.99)
                                        {
                                            antennaPullupHighEnd = antennaPullup - 1;
                                        }
                                        if (nTries > 50) //Check if we want to keep this
                                        {
                                            LOG(INFO) << BOLDRED << "Could not find a valid antenna value for " << chn << " channels of chip " << +cChip->getId() << "!!" << RESET;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    LOG(INFO) << BOLDBLUE << "Antenna values set, running to open finder." << RESET;

#if defined(__USE_ROOT__)
    fResultFile->cd();
    TString               fOpensTreeParameter = "";
    std::vector<uint16_t> fOpensTreeValue     = {};
    TTree*                fOpensTree          = new TTree("opensTree", "Opens in hybrid");
    fOpensTree->Branch("Chip", &fOpensTreeParameter);
    fOpensTree->Branch("Value", &fOpensTreeValue);
#endif

    bool        cOpensFound = false;
    std::string Channels    = "";

    std::vector<uint8_t> cPositions{0, 1};
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    for(auto cPosition: cPositions)
                    {
                        bool retry     = true;
                        int  occupancy = 0;
                        for(int i = 0; i < 2 && retry; i++)
                        {
                            retry = false;
                            // Get Antenna value for chip and channels
                            if(cPosition == 0)
                            {
                                if(finalAntennaEven[cChip->getIndex()] > 0)
                                {
                                    //   if (cChip->getId() == 6) {
                                    //     fParameters.potentiometer = 600;
                                    //     LOG(INFO) << "Potentiometer value: " << 600 << RESET;
                                    //   }
                                    //   else {
                                    fParameters.potentiometer = finalAntennaEven[cChip->getIndex()] + 5;
                                    LOG(INFO) << "Potentiometer value: " << finalAntennaEven[cChip->getIndex()] << RESET;
                                    ;
                                    // }
                                    Channels = "Even";
                                }
                                else
                                {
                                    LOG(INFO) << BOLDRED << "Could not set antenna value for the even channels of this chip ( chip " << +cChip->getId() << " ), skipping..." << RESET;
                                    continue;
                                }
                            }
                            else
                            {
                                if(finalAntennaOdd[cChip->getIndex()] > 0)
                                {
                                    // if (cChip->getId() == 1) {
                                    // fParameters.potentiometer = 781;
                                    // LOG(INFO) << "Potentiometer value: " << 781 << RESET;
                                    // }
                                    // else {
                                    fParameters.potentiometer = finalAntennaOdd[cChip->getIndex()] + 5;
                                    LOG(INFO) << "Potentiometer value: " << finalAntennaOdd[cChip->getIndex()];
                                    // }
                                    Channels = "Odd";
                                }
                                else
                                {
                                    LOG(INFO) << BOLDRED << "Could not set antenna value for the odd channels of this chip ( chip " << +cChip->getId() << " ), skipping..." << RESET;
                                    continue;
                                }
                            }
                            // FillSummaryTree(Form("Antenna_",cChip->GetId(),Channels), finalAntennaEven[cChip->getIndex()] );

                            // select antenna position
                            SelectAntennaPosition((cPosition == 0) ? "EvenChannels" : "OddChannels");

                            // check counters
                            BeBoard* cBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));
                            // cBeBoard->setEventType(EventType::SSAAS);

                            this->ReadNEvents(cBeBoard, fParameters.nTriggers);
                            const std::vector<Event*>& cEvents = this->GetEvents();
                            // const std::vector<Event*>& cEvents = this->GetEvents(cBeBoard);
                            cOpensFound = false;
                            std::vector<uint16_t> opens;
                            for(auto cEvent: cEvents)
                            {
                                LOG(INFO) << BOLDBLUE << "SSA#" << +cChip->getId() << RESET;
                                // auto cNhits     = cEvent->GetNHits(cHybrid->getId(), cChip->getId());
                                auto cHitVector = cEvent->GetHits(cHybrid->getId(), cChip->getId());

                                std::string tmpParameter = "";

                                for(uint32_t iChannel = 0; iChannel < cChip->size(); ++iChannel)
                                {
                                    occupancy += cHitVector[iChannel];
                                    // LOG(INFO) << "Channels" << +cHitVector[iChannel] << RESET;
                                    if(iChannel % 2 != cPosition)
                                    {
                                        if(cHitVector[iChannel] >= (THRESHOLD_OPEN)*fParameters.nTriggers)
                                            LOG(DEBUG) << BOLDBLUE << "\t\t... "
                                                       << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits " << RESET;
                                        continue;
                                    }
                                    else // If channel in injected channels:
                                    {
                                        LOG(DEBUG) << BOLDBLUE << "\t... "
                                                   << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits " << RESET;
                                        if(cHitVector[iChannel] <= (THRESHOLD_OPEN * 2.5) * fParameters.nTriggers)
                                        {
                                            cOpensFound = true;
                                            LOG(INFO) << BOLDRED << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                      << +fParameters.nTriggers << " were expected." << RESET;
                                            opens.push_back(iChannel);
                                        }
                                        else if(cHitVector[iChannel] > (1.0) * fParameters.nTriggers)
                                        {
                                            LOG(INFO) << BOLDBLUE << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                      << +fParameters.nTriggers << " were expected." << RESET;
                                        }
                                        else if(cHitVector[iChannel] <= (1.0 - THRESHOLD_OPEN*2.5) * fParameters.nTriggers)
                                            LOG(INFO) << BOLDYELLOW << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                      << +fParameters.nTriggers << " were expected." << RESET;
                                        else
                                        {
                                            LOG(INFO) << BOLDGREEN << "Chip " << +cChip->getId() << " strip " << +iChannel << " detected " << +cHitVector[iChannel] << " hits when at most "
                                                      << +fParameters.nTriggers << " were expected." << RESET;
                                        }
                                    }
                                } // chnl

                                tmpParameter = "";
                                tmpParameter = "opens_" + std::to_string(cChip->getId()) + "_" + Channels;
#if defined(__USE_ROOT__)
                                fillSummaryTree(tmpParameter, opens.size());
                                if(true)
                                {
                                    fResultFile->cd();
                                    fOpensTreeParameter.Clear();
                                    fOpensTreeParameter = "Chip_" + std::to_string(cChip->getId());
                                    fOpensTreeValue     = opens;
                                    fOpensTree->Fill();
                                }
#endif
                            }

                            if(!cOpensFound)
                                LOG(INFO) << BOLDGREEN << "No opens found on the " << Channels << " channels of chip " << +cChip->getId() << RESET;
                            else
                                LOG(INFO) << BOLDRED << +opens.size() << " opens found on the " << Channels << " channels of chip " << +cChip->getId() << RESET;
                            // disable
                            fParameters.potentiometer = 512;
                            SelectAntennaPosition("Disable");

                            LOG(INFO) << "Avg ccupancy on ALL channels is " << +occupancy / cChip->size() << RESET;
                        }
                    }
                }
            }
        }
    }
#endif
}
void OpenFinder::FindOpens() {}
#endif
