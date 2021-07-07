#include "PSHybridTester.h"
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

// initialize the static member

PSHybridTester::PSHybridTester() : Tool() {}

PSHybridTester::~PSHybridTester() {}

void PSHybridTester::Initialise()
{
#ifdef __TCUSB__
    LOG(INFO) << BOLDBLUE << "Selecting antenna channel to "
              << " disable all charge injection" << RESET;
    TC_PSFE cTC_PSFE;
    cTC_PSFE.antenna_fc7(uint16_t(513), TC_PSFE::ant_channel::NONE);
#endif
}
void PSHybridTester::MPATest(uint32_t pPattern)
{
    for(auto cBoard: *fDetectorContainer) { this->MPATest(cBoard, pPattern); }
}
void PSHybridTester::SSAOutputsPogoScope(BeBoard* pBoard, bool pTrigger)
{
    uint32_t cNtriggers = this->findValueInSettings<double>("PSHybridDebugDuration");
    if(pTrigger)
        LOG(INFO) << BOLDBLUE << "Going to send " << +cNtriggers << " triggers to debug L1 SSA output " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Going to capture for " << +cNtriggers * 10 << " ms to debug stub SSA output " << RESET;

    // pair id
    for(uint8_t cPairId = 0; cPairId < 2; cPairId++)
    {
        uint8_t cAlignmentPattern = (cPairId == 0) ? 0x05 : 0x01;
        // first I would like to align the lines in the back-end
        if(!pTrigger)
        {
            bool cAligned = true;
            for(uint8_t cLineId = 1; cLineId < 8; cLineId++)
            {
                cAligned = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PhaseTuning(pBoard, 0, cPairId, cLineId, cAlignmentPattern, 8);
                if(!cAligned) LOG(INFO) << BOLDRED << "Alignment failed on line " << +cLineId << RESET;
            }
            // if aligned then try and scope
            LOG(INFO) << "SLVS debug [stub lines] : Chip " << +cPairId << RESET;
        }
        else
        {
            LOG(INFO) << BOLDBLUE << "SLVS debug [L1 line] : Chip " << +cPairId << RESET;
        }
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", cPairId);
        if(pTrigger)
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->L1ADebug(false);
        else
        {
            static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 7);
        }
    }
}

void PSHybridTester::SSAOutputsPogoDebug(BeBoard* pBoard, bool pTrigger)
{
    uint32_t cNtriggers = this->findValueInSettings<double>("PSHybridDebugDuration");
    if(pTrigger)
        LOG(INFO) << BOLDBLUE << "Going to send " << +cNtriggers << " triggers to debug L1 SSA output " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Going to capture for " << +cNtriggers * 10 << " ms to debug stub SSA output " << RESET;

    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_input", 0xFFFFFFFF);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_input", 1);
    // send N triggers
    uint8_t cTriggerCounter = 0;
    do
    {
        if(pTrigger) fBeBoardInterface->ChipTrigger(pBoard);
        // fBeBoardInterface->ChipTestPulse(pBoard);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        cTriggerCounter++;
    } while(cTriggerCounter < cNtriggers);
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
    auto cDebugDone = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.input_lines_debug_done");
    do
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        cDebugDone = fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_stat.physical_interface_block.input_lines_debug_done");
    } while(cDebugDone != 0xFFFFFFFF);
    LOG(INFO) << BOLDBLUE << "Input lines debug done: 0x" << std::hex << cDebugDone << std::dec << RESET;
    auto cMapIterator = fInputDebugMap.begin();
    do
    {
        auto cRegisterName = cMapIterator->first;
        // only print out registers that are of interest
        bool cPrintMapItem = (cRegisterName.find("ssa") != std::string::npos);
        if(!cPrintMapItem)
        {
            cMapIterator++;
            continue;
        }

        if(pTrigger)
            cPrintMapItem = cPrintMapItem && (cRegisterName.find("l1") != std::string::npos);
        else
            cPrintMapItem = cPrintMapItem && (cRegisterName.find("trig") != std::string::npos);

        cPrintMapItem = cPrintMapItem || (cRegisterName.find("clk") != std::string::npos);
        cPrintMapItem = cPrintMapItem || (cRegisterName.find("fcmd") != std::string::npos);

        if(cPrintMapItem)
        {
            char cRegName[80];
            sprintf(cRegName, "fc7_daq_stat.physical_interface_block.debug_blk_counter%2d", cMapIterator->second);
            auto cResult = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
            LOG(INFO) << BOLDBLUE << "Register is " << cRegisterName << " counter address is " << +cMapIterator->second << " counter value is " << cResult << RESET;
        }
        cMapIterator++;
    } while(cMapIterator != fInputDebugMap.end());
}
void PSHybridTester::SSAPairSelect(BeBoard* pBoard, const std::string& SSAPairSel)
{
    try
    {
        auto BitPattern = fSSAPairSelMap.at(SSAPairSel);
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select", BitPattern);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        auto cRegister = this->fBeBoardInterface->ReadBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.multiplexing_bp.ssa_pair_select");
        LOG(INFO) << BLUE << "SSA pair " << SSAPairSel << " is selected register value is " << std::bitset<4>(cRegister) << RESET;
    }
    catch(const std::out_of_range& e)
    {
        LOG(ERROR) << BOLDRED << "Invalid SSA pair select option: " << SSAPairSel << RESET;
        LOG(INFO) << BLUE << "Possible options are: "
                  << "01, 12, 23, 34, 45, 56, 67" << RESET;
    }
}
void PSHybridTester::SSATestStubOutput(const std::string& cSSAPairSel)
{
    for(auto cBoard: *fDetectorContainer) { this->SSATestStubOutput(cBoard, cSSAPairSel); }
}
void PSHybridTester::SSATestL1Output(const std::string& cSSAPairSel)
{
    for(auto cBoard: *fDetectorContainer) { this->SSATestL1Output(cBoard, cSSAPairSel); }
}
void PSHybridTester::SelectCIC(bool pSelect)
{
#ifdef __TCUSB__
    TC_PSFE cTC_PSFE;
    // enable CIC in mode of front-end hybrid
    if(pSelect)
        cTC_PSFE.mode_control(TC_PSFE::mode::CIC_IN);
    else
        cTC_PSFE.mode_control(TC_PSFE::mode::SSA_OUT);
#endif
}
void PSHybridTester::MPATest(BeBoard* pBoard, uint32_t pPattern)
{
    // enable CIC mux - phy port 0
    for(uint8_t cPhyPort = 0; cPhyPort < 12; cPhyPort++)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                fCicInterface->SelectMux(cCic, cPhyPort);
            } // hybrid
        }     // hybrid
        // check output
        fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->StubDebug(true, 4);
    }
}
void PSHybridTester::SSATestStubOutput(BeBoard* pBoard, const std::string& cSSAPairSel)
{
    this->SelectCIC(false);
    this->SSAPairSelect(pBoard, cSSAPairSel);
    // now cycle through chips one at a time ..
    // and configure chips to output a fixed pattern
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                uint8_t cPattern = (cReadoutChip->getId() % 2 == 0) ? 0x01 : 0x05;

                LOG(INFO) << BOLDBLUE << "Chip " << +cReadoutChip->getId() << " configured to output " << std::bitset<8>(cPattern) << " on SLVS output" << RESET;

                // make sure SSA is configured to output a test pattern on SLVS out
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput", 1);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern0", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern1", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern2", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern3", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern4", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern5", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern6", cPattern);
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "OutPattern7/FIFOconfig", cPattern);
            } // chip
        }     // hybrid
    }         // opticalGroup
    // now capture output on pogo sockets
    // this->SSAOutputsPogoDebug(pBoard, false);
    this->SSAOutputsPogoScope(pBoard, false);
}
void PSHybridTester::SSATestL1Output(BeBoard* pBoard, const std::string& cSSAPairSel)
{
    // select SSA pair
    this->SSAPairSelect(pBoard, cSSAPairSel);
    // now cycle through chips one at a time ..
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;
                /*
               // make sure SSA is configured to inject digital hits in one strip
               // first make sure all strips are set to output exactly 0
               fReadoutChipInterface->WriteChipReg(cReadoutChip, "CalibrationPattern", 0x00);
               // set readout mode to 'frames'
               //fReadoutChipInterface->WriteChipReg(cReadoutChip, "EnableSLVSTestOutput",0);
               uint8_t cPattern=0x01;//0,1
               if( cReadoutChip->getId() == 0 || cReadoutChip->getId() == 4 ) //2,3
                 cPattern = 0x01;
               else if( cReadoutChip->getId() == 1 || cReadoutChip->getId() == 5 ) //2,3
                 cPattern = 0x05;
               else if( cReadoutChip->getId() == 2 || cReadoutChip->getId() == 6 )//4,5
                 cPattern = 0x15;
               else if( cReadoutChip->getId() == 3 || cReadoutChip->getId() == 7 )//6,7
                 cPattern = 0x55;
               // then that strip 10 is otputting
               fReadoutChipInterface->WriteChipReg(cReadoutChip, "CalibrationPatternS10", cPattern);
               LOG (INFO) << BOLDBLUE << "Chip "
                   << +cReadoutChip->getId()
                   << " configured to output "
                   << std::bitset<8>(cPattern)
                   << " on L1 data output"
                   << RESET;
       */
            } // chip
        }     // hybrid
    }         // opticalGroup
    // now capture output on pogo sockets
    // this->SSAOutputsPogoDebug(pBoard, true);
    this->SSAOutputsPogoScope(pBoard, true);
}
void PSHybridTester::SetHybridVoltage()
{
#ifdef __TCUSB__
    TC_PSFE cTC_PSFE;
    // cTC_PSFE.set_voltage(cTC_PSFE._1100mV,cTC_PSFE._1250mV);
    cTC_PSFE.set_voltage(cTC_PSFE._1050mV, cTC_PSFE._1250mV);
#endif
}
void PSHybridTester::ReadHybridVoltage(const std::string& pVoltageName)
{
#ifdef __TCUSB__
    auto cMapIterator = fHybridVoltageMap.find(pVoltageName);
    if(cMapIterator != fHybridVoltageMap.end())
    {
        auto&              cMeasurement = cMapIterator->second;
        TC_PSFE            cTC_PSFE;
        std::vector<float> cMeasurements(fNreadings, 0.);
        for(int cIndex = 0; cIndex < fNreadings; cIndex++)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
            cTC_PSFE.adc_get(cMeasurement, cMeasurements[cIndex]);
            LOG(DEBUG) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                       << " reading from test card  : " << cMeasurements[cIndex] << " mV." << RESET;
        }
        fVoltageMeasurement = this->getStats(cMeasurements);
    }
#endif
}
void PSHybridTester::ReadHybridCurrent(const std::string& pVoltageName)
{
#ifdef __TCUSB__
    // auto cMapIterator = fHybridCurrentMap.find(pVoltageName);
    // if( cMapIterator != fHybridCurrentMap.end() )
    // {
    //     auto& cMeasurement = cMapIterator->second;
    TC_PSFE            cTC_PSFE;
    std::vector<float> cMeasurements(fNreadings, 0.);
    for(int cIndex = 0; cIndex < fNreadings; cIndex++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
        cTC_PSFE.adc_get(TC_PSFE::measurement::ISEN_1V, cMeasurements[cIndex]);
        LOG(INFO) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                  << " reading from test card 1V  : " << cMeasurements[cIndex] << " mA." << RESET;
    }

    std::vector<float> cMeasurements1(fNreadings, 0.);
    for(int cIndex = 0; cIndex < fNreadings; cIndex++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(fVoltageMeasurementWait_ms));
        cTC_PSFE.adc_get(TC_PSFE::measurement::ISEN_1V25, cMeasurements1[cIndex]);
        LOG(INFO) << BOLDBLUE << "\t\t..After waiting for " << (cIndex + 1) * 1e-3 * fVoltageMeasurementWait_ms << " seconds ..."
                  << " reading from test card  1V25 : " << cMeasurements1[cIndex] << " mA." << RESET;
    }
    // fCurrentMeasurement = this->getStats(cMeasurements);
    //}
#endif
}
void PSHybridTester::CheckHybridCurrents()
{
    ReadHybridCurrent("Hybrid1V00");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 1V00 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;

    // ReadHybridCurrent("Hybrid1V25");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 1V25 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;

    // ReadHybridCurrent("Hybrid3V30");
    // LOG (INFO) << BOLDBLUE << "Current consumption on 3V30 : "
    //     << fCurrentMeasurement.first << " mA on average "
    //     << fCurrentMeasurement.second << " mA rms. " << RESET;
}
void PSHybridTester::CheckHybridVoltages()
{
    ReadHybridVoltage("TestCardGround");
    LOG(INFO) << BOLDBLUE << "Test card ground : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    ReadHybridVoltage("PanasonicGround");
    LOG(INFO) << BOLDBLUE << "Panasonic connector ground : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    ReadHybridVoltage("Hybrid3V3");
    LOG(INFO) << BOLDBLUE << "Hybrid 3V30 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    ReadHybridVoltage("Hybrid1V00");
    LOG(INFO) << BOLDBLUE << "Hybrid 1V00 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

    ReadHybridVoltage("Hybrid1V25");
    LOG(INFO) << BOLDBLUE << "Hybrid 1V25 : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;
    if(fVoltageMeasurement.first * 1e-3 >= PSHYBRIDMAXV) { throw std::runtime_error(std::string("Exceeded maximum voltage of 1V25 of PS FEH")); }
}
void PSHybridTester::ReadSSABias(BeBoard* pBoard, const std::string& pBiasName)
{
    // now cycle through chips one at a time ..
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            // set AMUX on all SSAs to highZ
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1);
            }

            ReadHybridVoltage("ADC");
            LOG(INFO) << BOLDBLUE << "[AmuxHigh] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

            // then select bias
            for(auto cReadoutChip: *cHybrid)
            {
                // add check for SSA
                if(cReadoutChip->getFrontEndType() != FrontEndType::SSA) continue;

                LOG(INFO) << BOLDBLUE << "Selecting TestBias on SSA" << +cReadoutChip->getId() << RESET;
                // select bias
                fReadoutChipInterface->WriteChipReg(cReadoutChip, pBiasName, 1);
                ReadHybridVoltage("ADC");
                LOG(INFO) << BOLDBLUE << "[ " << pBiasName << " ] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;

                // back to high Z
                fReadoutChipInterface->WriteChipReg(cReadoutChip, "AmuxHigh", 1);
                ReadHybridVoltage("ADC");
                LOG(INFO) << BOLDBLUE << "[AmuxHigh] ADC reading : " << fVoltageMeasurement.first << " mV on average " << fVoltageMeasurement.second << " mV rms. " << RESET;
            }
        } // hybrid
    }     // board
}
void PSHybridTester::CheckFastCommands(BeBoard* pBoard, const std::string& pFastCommand, uint8_t pDuration)
{
    LOG(DEBUG) << BOLDBLUE << "Sending " << pFastCommand << RESET;
    if(pFastCommand == "ReSync") { this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 16) | (pDuration << 28)); }
    else if(pFastCommand == "Trigger" || pFastCommand == "OpenShutter")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 18) | (pDuration << 28));
    }
    else if(pFastCommand == "TestPulse")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 17) | (pDuration << 28));
    }
    else if(pFastCommand == "BC0" || pFastCommand == "CloseShutter")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (pDuration << 28));
    }
    else if(pFastCommand == "ReSync&BC0" || pFastCommand == "StartReadout")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 16) | (pDuration << 28));
    }
    else if(pFastCommand == "Trigger&BC0" || pFastCommand == "ClearCounters")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 18) | (pDuration << 28));
    }
    else if(pFastCommand == "TestPulse&BC0")
    {
        this->fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.fast_command_block.control", (1 << 19) | (1 << 17) | (pDuration << 28));
    }
}
void PSHybridTester::CheckHybridInputs(BeBoard* pBoard, std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    uint32_t             cRegisterValue = 0;
    std::vector<uint8_t> cIndices(0);
    for(auto cInput: pInputs)
    {
        auto cMapIterator = fInputDebugMap.find(cInput);
        if(cMapIterator != fInputDebugMap.end())
        {
            auto& cIndex   = cMapIterator->second;
            cRegisterValue = cRegisterValue | (1 << cIndex);
            cIndices.push_back(cIndex);
        }
    }
    // select input lines
    LOG(INFO) << BOLDBLUE << "Configuring debug register : " << std::bitset<32>(cRegisterValue) << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_input", cRegisterValue);
    // start
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_input", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // stop
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_input", 1);
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    // check counters
    pCounters.clear();
    pCounters.resize(cIndices.size());
    for(auto cIndex: cIndices)
    {
        char cBuffer[20];
        sprintf(cBuffer, "debug_blk_counter%02d", (cIndex & 0x63)); // max value can be 99, to avoid GCC 8 warning
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}
void PSHybridTester::CheckHybridInputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckHybridInputs(cBoard, pInputs, pCounters); }
}
void PSHybridTester::SelectAntennaPosition(const std::string& pPosition, uint16_t pPotentiometer)
{
#ifdef __TCUSB__
    std::map<std::string, TC_PSFE::ant_channel> cAntennaControl = {
        {"Disable", TC_PSFE::ant_channel::NONE}, {"EvenChannels", TC_PSFE::ant_channel::_1}, {"OddChannels", TC_PSFE::ant_channel::_2}, {"Enable", TC_PSFE::ant_channel::ALL}};

    auto cMapIterator = cAntennaControl.find(pPosition);
    if(cMapIterator != cAntennaControl.end())
    {
        auto& cChannel = cMapIterator->second;
        LOG(INFO) << BOLDBLUE << "Selecting antenna channel to "
                  << " inject charge in [ " << pPosition << " ] position. This is switch position " << +cChannel << RESET;
        TC_PSFE cTC_PSFE;
        cTC_PSFE.antenna_fc7(pPotentiometer, cChannel);
    }
#endif
}

void PSHybridTester::CheckHybridOutputs(BeBoard* pBoard, std::vector<std::string> pOutputs, std::vector<uint32_t>& pCounters)
{
    uint32_t             cRegisterValue = 0;
    std::vector<uint8_t> cIndices(0);
    for(auto cInput: pOutputs)
    {
        auto cMapIterator = fOutputDebugMap.find(cInput);
        if(cMapIterator != fOutputDebugMap.end())
        {
            auto& cIndex   = cMapIterator->second;
            cRegisterValue = cRegisterValue | (1 << cIndex);
            cIndices.push_back(cIndex);
        }
    }
    // select input lines
    LOG(INFO) << BOLDBLUE << "Configuring debug register : " << std::bitset<32>(cRegisterValue) << RESET;
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_cnfg.physical_interface_block.debug_blk_output", cRegisterValue);
    // start
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.start_output", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // stop
    fBeBoardInterface->WriteBoardReg(pBoard, "fc7_daq_ctrl.physical_interface_block.debug_blk.stop_output", 1);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
    // check counters
    pCounters.clear();
    pCounters.resize(cIndices.size());
    for(auto cIndex: cIndices)
    {
        char cBuffer[20];
        sprintf(cBuffer, "debug_blk_counter%02d", (cIndex & 0x63)); // max value can be 99, to avoid GCC 8 warning
        std::string cRegName = cBuffer;
        uint32_t    cCounter = fBeBoardInterface->ReadBoardReg(pBoard, cRegName);
        pCounters.push_back(cCounter);
    }
}
void PSHybridTester::ReadSSABias(const std::string& pBiasName)
{
#ifdef __TCUSB__
    LOG(INFO) << BOLDBLUE << "TC USB built." << RESET;
    for(auto cBoard: *fDetectorContainer) { this->ReadSSABias(cBoard, pBiasName); }
#else
    LOG(INFO) << BOLDRED << "TC USB not built .. check that you have the lib installed!" << RESET;
#endif
}

void PSHybridTester::CheckHybridOutputs(std::vector<std::string> pInputs, std::vector<uint32_t>& pCounters)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckHybridOutputs(cBoard, pInputs, pCounters); }
}
void PSHybridTester::CheckFastCommands(const std::string& pFastCommand, uint8_t pDuartion)
{
    for(auto cBoard: *fDetectorContainer) { this->CheckFastCommands(cBoard, pFastCommand, pDuartion); }
}
// State machine control functions
void PSHybridTester::Running()
{
    LOG(INFO) << BOLDBLUE << "Starting PS Hybrid tester" << RESET;
    Initialise();
}

void PSHybridTester::Stop()
{
    LOG(INFO) << BOLDBLUE << "Stopping PS Hybrid tester" << RESET;
    // writeObjects();
    dumpConfigFiles();
    Destroy();
}

void PSHybridTester::Pause() {}

void PSHybridTester::Resume() {}
