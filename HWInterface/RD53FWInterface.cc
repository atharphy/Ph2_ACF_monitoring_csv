/*!
  \file                  RD53FWInterface.h
  \brief                 RD53FWInterface initialize and configure the FW
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53FWInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
RD53FWInterface::RD53FWInterface(const char* pId, const char* pUri, const char* pAddressTable) : BeBoardFWInterface(pId, pUri, pAddressTable), fpgaConfig(nullptr), ddr3Offset(0) {}

void RD53FWInterface::setFileHandler(FileHandler* pHandler)
{
    if(pHandler != nullptr)
    {
        this->fFileHandler = pHandler;
        this->fSaveToFile  = true;
    }
    else
        LOG(ERROR) << BOLDRED << "NULL FileHandler" << RESET;
}

uint32_t RD53FWInterface::getBoardInfo()
{
    uint32_t cVersionMajor = ReadReg("user.stat_regs.usr_ver.usr_ver_major");
    uint32_t cVersionMinor = ReadReg("user.stat_regs.usr_ver.usr_ver_minor");
    uint32_t cVersionWord  = ((cVersionMajor << NBIT_FWVER) | cVersionMinor);
    return cVersionWord;
}

void RD53FWInterface::ResetSequence()
{
    LOG(INFO) << BOLDMAGENTA << "Resetting the backend board... it may take a while" << RESET;

    RD53FWInterface::TurnOffFMC();
    RD53FWInterface::TurnOnFMC();
    RD53FWInterface::ResetBoard();

    LOG(INFO) << BOLDMAGENTA << "Now you can start using the DAQ ... enjoy!" << RESET;
}

void RD53FWInterface::ConfigureBoard(const BeBoard* pBoard)
{
    // ########################
    // # Print firmware infos #
    // ########################
    uint32_t cVersionMajor = ReadReg("user.stat_regs.usr_ver.usr_ver_major");
    uint32_t cVersionMinor = ReadReg("user.stat_regs.usr_ver.usr_ver_minor");

    uint32_t cFWyear    = ReadReg("user.stat_regs.fw_date.year");
    uint32_t cFWmonth   = ReadReg("user.stat_regs.fw_date.month");
    uint32_t cFWday     = ReadReg("user.stat_regs.fw_date.day");
    uint32_t cFWhour    = ReadReg("user.stat_regs.fw_date.hour");
    uint32_t cFWminute  = ReadReg("user.stat_regs.fw_date.minute");
    uint32_t cFWseconds = ReadReg("user.stat_regs.fw_date.seconds");

    LOG(INFO) << BOLDBLUE << "\t--> FW version : " << BOLDYELLOW << cVersionMajor << "." << cVersionMinor << BOLDBLUE << " -- date (yy/mm/dd) : " << BOLDYELLOW << cFWyear << "/" << cFWmonth << "/"
              << cFWday << BOLDBLUE << " -- time (hour:minute:sec) : " << BOLDYELLOW << cFWhour << ":" << cFWminute << ":" << cFWseconds << RESET;

    std::stringstream myString;
    RD53FWInterface::ChipReset();
    RD53FWInterface::ChipReSync();
    RD53FWInterface::ResetFastCmdBlk();
    RD53FWInterface::ResetSlowCmdBlk();
    RD53FWInterface::ResetReadoutBlk();

    // ###############################################
    // # FW register initialization from config file #
    // ###############################################
    RD53FWInterface::DIO5Config                   cfgDIO5;
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    LOG(INFO) << GREEN << "Initializing DIO5:" << RESET;
    for(const auto& it: pBoard->getBeBoardRegMap())
        if((it.first.find("ext_clk_en") != std::string::npos) || (it.first.find("HitOr_enable_l12") != std::string::npos) || (it.first.find("trigger_source") != std::string::npos))
        {
            LOG(INFO) << BOLDBLUE << "\t--> " << it.first << ": " << BOLDYELLOW << std::hex << "0x" << it.second << std::dec << " (" << it.second << ")" << RESET;
            if(it.first.find("HitOr_enable_l12") != std::string::npos)
                RD53FWInterface::localCfgFastCmd.enable_hitor = it.second;
            else if(it.first.find("ext_clk_en") != std::string::npos)
            {
                cfgDIO5.enable     = it.second;
                cfgDIO5.ch_out_en  = 0x00;
                cfgDIO5.ext_clk_en = it.second;
            }
            else
            {
                RD53FWInterface::localCfgFastCmd.trigger_source = static_cast<RD53FWInterface::TriggerSource>(it.second);
                if(static_cast<RD53FWInterface::TriggerSource>(it.second) == TriggerSource::External)
                {
                    cfgDIO5.enable    = true;
                    cfgDIO5.ch_out_en = 0x00;
                }
                else if(static_cast<RD53FWInterface::TriggerSource>(it.second) == TriggerSource::TLU)
                {
                    cfgDIO5.enable             = true;
                    cfgDIO5.ch_out_en          = 0x04;
                    cfgDIO5.tlu_en             = true;
                    cfgDIO5.tlu_handshake_mode = 0x02;
                }
            }
        }

    // ##################
    // # Configure DIO5 #
    // ##################
    RD53FWInterface::ConfigureDIO5(&cfgDIO5);
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
    usleep(DEEPSLEEP);

    // ##############################
    // # Initialize clock generator #
    // ##############################
    LOG(INFO) << GREEN << "Initializing clock generator (CDCE62005)..." << RESET;
    RD53FWInterface::InitializeClockGenerator();
    RD53FWInterface::ReadClockGenerator();
    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;

    // ################################
    // # Enabling hybrids and chips   #
    // # Hybrid_type hard coded in FW #
    // # 1 = single chip              #
    // # 2 = double chip hybrid       #
    // # 4 = quad chip hybrid         #
    // ################################
    this->singleChip     = ReadReg("user.stat_regs.aurora_rx.Module_type") == 1;
    this->enabledHybrids = 0;
    uint32_t chips_en    = 0;
    for(const auto cOpticalGroup: *pBoard)
        for(const auto cHybrid: *cOpticalGroup)
        {
            this->enabledHybrids |= 1 << cHybrid->getId();
            chips_en |= RD53FWInterface::GetHybridEnabledChips(cHybrid);
        }
    cVecReg.push_back({"user.ctrl_regs.Hybrids_en", this->enabledHybrids});
    cVecReg.push_back({"user.ctrl_regs.Chips_en", chips_en});
    if(cVecReg.size() != 0) RegManager::WriteStackReg(cVecReg);

    // ###########################
    // # Print clock measurement #
    // ###########################
    uint32_t inputClk = ReadReg("user.stat_regs.clkin_rate");
    uint32_t gtxClk   = ReadReg("user.stat_regs.gtx_refclk_rate");
    LOG(INFO) << GREEN << "Input clock frequency (could be either internal or external, should be ~40 MHz): " << BOLDYELLOW << inputClk / 1000. << " MHz" << RESET;
    LOG(INFO) << GREEN << "GTX receiver clock frequency (should be ~160 MHz): " << BOLDYELLOW << gtxClk / 1000. << " MHz" << RESET;

    // ##############################
    // # AURORA lock on data stream #
    // ##############################
    if(RD53FWInterface::CheckChipCommunication() == false)
    {
        RD53FWInterface::InitHybridByHybrid(pBoard);
        RD53FWInterface::CheckChipCommunication();
    }
}

void RD53FWInterface::ConfigureFromXML(const BeBoard* pBoard)
{
    // ###############################################
    // # FW register initialization from config file #
    // ###############################################
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    LOG(INFO) << GREEN << "Initializing board's registers:" << RESET;

    for(const auto& it: pBoard->getBeBoardRegMap())
        if((it.first.find("ext_clk_en") == std::string::npos) && (it.first.find("trigger_source") == std::string::npos))
        {
            LOG(INFO) << BOLDBLUE << "\t--> " << it.first << ": " << BOLDYELLOW << std::hex << "0x" << it.second << std::dec << " (" << it.second << ")" << RESET;
            cVecReg.push_back({it.first, it.second});
        }

    if(cVecReg.size() != 0)
    {
        RegManager::WriteStackReg(cVecReg);
        RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.load_config");
    }

    LOG(INFO) << BOLDBLUE << "\t--> Done" << RESET;
}

void RD53FWInterface::WriteChipCommand(const std::vector<uint16_t>& data, int hybridId)
{
    std::vector<uint32_t> commandList;

    RD53FWInterface::ComposeAndPackChipCommands(data, hybridId, commandList);
    RD53FWInterface::SendChipCommandsPack(commandList);
}

void RD53FWInterface::ComposeAndPackChipCommands(const std::vector<uint16_t>& data, int hybridId, std::vector<uint32_t>& commandList)
{
    const size_t n32bitWords = (data.size() / 2) + (data.size() % 2);

    // #####################
    // # Check if all good #
    // #####################
    if(ReadReg("user.stat_regs.slow_cmd.error_flag") == true) LOG(ERROR) << BOLDRED << "Write-command FIFO error" << RESET;
    if(ReadReg("user.stat_regs.slow_cmd.fifo_empty") == false) LOG(ERROR) << BOLDRED << "Write-command FIFO not empty" << RESET;

    // ##########
    // # Header #
    // ##########
    commandList.emplace_back(bits::pack<6, 10, 4, 12>(HEADEAR_WRTCMD, (hybridId < 0 ? enabledHybrids : 1 << hybridId), 0, n32bitWords));

    // ############
    // # Commands #
    // ############
    for(auto i = 1u; i < data.size(); i += 2) commandList.emplace_back(bits::pack<16, 16>(data[i - 1], data[i]));

    // If data.size() is not even, add a sync command
    if(data.size() % 2 != 0) commandList.emplace_back(bits::pack<16, 16>(data.back(), RD53CmdEncoder::SYNC));
}

void RD53FWInterface::SendChipCommandsPack(const std::vector<uint32_t>& commandList)
{
    int  nAttempts = 0;
    bool retry;

    // ###############################
    // # Send command(s) to the chip #
    // ###############################
    RegManager::WriteBlockReg("user.ctrl_regs.Slow_cmd_fifo_din", commandList);
    RegManager::WriteStackReg({{"user.ctrl_regs.Slow_cmd.dispatch_packet", 1}, {"user.ctrl_regs.Slow_cmd.dispatch_packet", 0}});

    // ####################################
    // # Check if commands were dispached #
    // ####################################
    while(((retry = !ReadReg("user.stat_regs.slow_cmd.fifo_packet_dispatched")) == true) && (nAttempts < MAXATTEMPTS))
    {
        nAttempts++;
        usleep(READOUTSLEEP);
    }
    if(retry == true) LOG(ERROR) << BOLDRED << "Error while dispatching chip register program, reached maximum number of attempts (" << BOLDYELLOW << MAXATTEMPTS << BOLDRED << ")" << RESET;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53FWInterface::ReadChipRegisters(ReadoutChip* pChip)
{
    std::vector<std::pair<uint16_t, uint16_t>> regReadback;

    // #################################
    // # Compose chip-lane in readback #
    // #################################
    uint32_t chipLane = pChip->getHybridId();
    if(this->singleChip != true) chipLane = NLANE_HYBRID * chipLane + static_cast<RD53*>(pChip)->getChipLane();

    // #####################
    // # Read the register #
    // #####################
    if(ReadReg("user.stat_regs.Register_Rdback.fifo_full") == true) LOG(ERROR) << BOLDRED << "Read-command FIFO full" << RESET;

    while(ReadReg("user.stat_regs.Register_Rdback.fifo_empty") == false)
    {
        uint32_t readBackData = ReadReg("user.stat_regs.Register_Rdback_fifo");

        uint16_t lane, address, value;
        std::tie(lane, address, value) = bits::unpack<6, 10, 16>(readBackData);

        if(lane == chipLane) regReadback.emplace_back(address, value);
    }

    if(regReadback.size() == 0) LOG(ERROR) << BOLDRED << "Read-command FIFO empty" << RESET;

    return regReadback;
}

void RD53FWInterface::PrintFWstatus()
{
    LOG(INFO) << GREEN << "Checking firmware status:" << RESET;

    // #################################
    // # Check clock generator locking #
    // #################################
    if(ReadReg("user.stat_regs.global_reg.clk_gen_lock") == 1)
        LOG(INFO) << BOLDBLUE << "\t--> Clock generator is " << BOLDYELLOW << "locked" << RESET;
    else
        LOG(ERROR) << BOLDRED << "\t--> Clock generator is not locked" << RESET;

    // ############################
    // # Check I2C initialization #
    // ############################
    if(ReadReg("user.stat_regs.global_reg.i2c_init") == 1)
        LOG(INFO) << BOLDBLUE << "\t--> I2C " << BOLDYELLOW << "initialized" << RESET;
    else
    {
        LOG(ERROR) << BOLDRED << "I2C not initialized" << RESET;
        unsigned int status = ReadReg("user.stat_regs.global_reg.i2c_init_err");
        LOG(ERROR) << BOLDRED << "\t--> I2C initialization status: " << BOLDYELLOW << status << RESET;
    }

    if(ReadReg("user.stat_regs.global_reg.i2c_acq_err") == 1) LOG(INFO) << GREEN << "I2C ack error during analog readout (for KSU FMC only)" << RESET;

    // ############################################################
    // # Check status registers associated wih fast command block #
    // ############################################################
    unsigned int fastCMDReg = ReadReg("user.stat_regs.fast_cmd.trigger_source_o");
    LOG(INFO) << GREEN << "Fast command block trigger source: " << BOLDYELLOW << fastCMDReg << RESET << GREEN << " (1=IPBus, 2=Test-FSM, 3=TTC, 4=TLU, 5=External, 6=Hit-Or, 7=User-defined frequency)"
              << RESET;

    fastCMDReg = ReadReg("user.stat_regs.fast_cmd.trigger_state");
    LOG(INFO) << GREEN << "Fast command block trigger state: " << BOLDYELLOW << fastCMDReg << RESET << GREEN << " (0=idle, 2=running)" << RESET;

    fastCMDReg = ReadReg("user.stat_regs.fast_cmd.if_configured");
    LOG(INFO) << GREEN << "Fast command block check if configuraiton registers have been set: " << BOLDYELLOW << (fastCMDReg == true ? "configured" : "not configured") << RESET;

    fastCMDReg = ReadReg("user.stat_regs.fast_cmd.error_code");
    LOG(INFO) << GREEN << "Fast command block error code (0=no error): " << BOLDYELLOW << fastCMDReg << RESET;

    // ###########################
    // # Check trigger registers #
    // ###########################
    unsigned int trigReg = ReadReg("user.stat_regs.trigger_cntr");
    LOG(INFO) << GREEN << "Trigger counter: " << BOLDYELLOW << trigReg << RESET;

    // ##########################
    // # Check hybrid registers #
    // ##########################
    unsigned int hybrid = ReadReg("user.stat_regs.aurora_rx.Module_type");
    LOG(INFO) << GREEN << "Hybrid type: " << BOLDYELLOW << hybrid << RESET << GREEN " (1=single chip, 2=double chip, 4=quad chip)" << RESET;

    hybrid = ReadReg("user.stat_regs.aurora_rx.Nb_of_modules");
    LOG(INFO) << GREEN << "Number of hybrids which can be potentially readout: " << BOLDYELLOW << hybrid << RESET;
}

bool RD53FWInterface::CheckChipCommunication()
{
    LOG(INFO) << GREEN << "Checking status communication FW <----> RD53" << RESET;

    // ###############################
    // # Check RD53 AURORA registers #
    // ###############################
    auroraSpeed = ReadReg("user.stat_regs.aurora_rx.speed");
    LOG(INFO) << BOLDBLUE << "\t--> Aurora speed: " << BOLDYELLOW << (auroraSpeed == 0 ? "1.28 Gbps" : "640 Mbps") << RESET;

    // ########################################
    // # Check communication with the chip(s) #
    // ########################################
    unsigned int chips_en = ReadReg("user.ctrl_regs.Chips_en");
    LOG(INFO) << BOLDBLUE << "\t--> Number of required data lanes: " << BOLDYELLOW << RD53Shared::countBitsOne(chips_en) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<12>(chips_en) << RESET;

    unsigned int channel_up = ReadReg("user.stat_regs.aurora_rx_channel_up");
    LOG(INFO) << BOLDBLUE << "\t--> Number of active data lanes:   " << BOLDYELLOW << RD53Shared::countBitsOne(channel_up) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<12>(channel_up)
              << RESET;

    if(chips_en & ~channel_up)
    {
        LOG(ERROR) << BOLDRED << "\t--> Some data lanes are enabled but inactive" << RESET;
        return false;
    }
    else if(chips_en == 0)
    {
        LOG(ERROR) << BOLDRED << "\t--> No data lane is enabled: aborting" << RESET;
        exit(EXIT_FAILURE);
    }

    LOG(INFO) << BOLDBLUE << "\t--> All enabled data lanes are active" << RESET;
    return true;
}

void RD53FWInterface::InitHybridByHybrid(const BeBoard* pBoard)
{
    const unsigned int MAXSEQUENCES = 5;

    for(const auto cOpticalGroup: *pBoard)
        for(const auto cHybrid: *cOpticalGroup)
        {
            // #################################
            // # Check if all lanes are active #
            // #################################
            const uint32_t hybrid_id         = cHybrid->getId();
            const uint32_t chips_en_to_check = RD53FWInterface::GetHybridEnabledChips(cHybrid);
            const uint32_t channel_up        = ReadReg("user.stat_regs.aurora_rx_channel_up");

            if((channel_up & chips_en_to_check) == chips_en_to_check)
            {
                LOG(INFO) << GREEN << "Board/OpticalGroup/Hybrid [" << BOLDYELLOW << pBoard->getId() << "/" << cOpticalGroup->getId() << "/" << hybrid_id << RESET << GREEN << "] already locked"
                          << RESET;
                continue;
            }

            // ################################
            // # Try different init sequences #
            // ################################
            bool lanes_up;
            for(unsigned int seq = 0; seq < MAXSEQUENCES; seq++)
            {
                LOG(INFO) << GREEN << "Trying initialization sequence number: " << BOLDYELLOW << seq << RESET;
                LOG(INFO) << BOLDBLUE << "\t--> Number of required data lanes for [board/opticalGroup/hybrid = " << BOLDYELLOW << pBoard->getId() << "/" << cOpticalGroup->getId() << "/" << hybrid_id
                          << BOLDBLUE << "]: " << BOLDYELLOW << RD53Shared::countBitsOne(chips_en_to_check) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<12>(chips_en_to_check) << RESET;

                std::vector<uint16_t> initSequence = RD53FWInterface::GetInitSequence(this->singleChip == true ? 4 : seq);

                for(unsigned int i = 0; i < MAXATTEMPTS; i++)
                {
                    // ###########################
                    // # Set proper AURORA speed #
                    // ###########################
                    if(auroraSpeed == 0)
                        RD53FWInterface::WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, RD53Constants::CDRCONFIG_ADDR, RD53Constants::CDRCONFIG_1Gbit).getFrames(), -1);
                    else
                        RD53FWInterface::WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, RD53Constants::CDRCONFIG_ADDR, RD53Constants::CDRCONFIG_640Mbit).getFrames(), -1);

                    RD53FWInterface::WriteChipCommand(initSequence, hybrid_id);
                    usleep(DEEPSLEEP);

                    // #################################
                    // # Check if all lanes are active #
                    // #################################
                    lanes_up            = false;
                    uint32_t channel_up = ReadReg("user.stat_regs.aurora_rx_channel_up");

                    LOG(INFO) << BOLDBLUE << "\t--> Number of active data lanes for tentative n. " << BOLDYELLOW << i << BOLDBLUE << ": " << BOLDYELLOW << RD53Shared::countBitsOne(channel_up)
                              << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<12>(channel_up) << RESET;

                    if((channel_up & chips_en_to_check) == chips_en_to_check)
                    {
                        LOG(INFO) << GREEN << "Board/OpticalGroup/Hybrid [" << BOLDYELLOW << pBoard->getId() << "/" << cOpticalGroup->getId() << "/" << hybrid_id << RESET << GREEN
                                  << "] locked with sequence " << BOLDYELLOW << seq << RESET << GREEN << " on tentative n. " << BOLDYELLOW << i << RESET;
                        lanes_up = true;
                        break;
                    }
                }

                if(lanes_up == true) break;
            }

            if(lanes_up == false) LOG(ERROR) << BOLDRED << "Not all data lanes are active for hybrid: " << BOLDYELLOW << hybrid_id << RESET;
        }
}

std::vector<uint16_t> RD53FWInterface::GetInitSequence(const unsigned int type)
{
    std::vector<uint16_t> initSequence;

    switch(type)
    {
    case 0:                                                                    // Okay for all (3 TBPX, 1 TEPX hybridss so far)
        for(unsigned int i = 0; i < 500; i++) initSequence.push_back(0x0000);  // 0000 0000
        for(unsigned int i = 0; i < 2000; i++) initSequence.push_back(0xCCCC); // 1100 1100
        break;
    case 1:                                                                    // Seen to be good for some TBPX hybrids
        for(unsigned int i = 0; i < 1000; i++) initSequence.push_back(0xFFFF); // 1111 1111
        for(unsigned int i = 0; i < 500; i++) initSequence.push_back(0x3333);  // 0011 0011
        break;

    case 2:                                                                    // Seen to be good for TEPX hybrid
        for(unsigned int i = 0; i < 500; i++) initSequence.push_back(0x0000);  // 0000 0000
        for(unsigned int i = 0; i < 1000; i++) initSequence.push_back(0x3333); // 0011 0011
        break;

    case 3:                                                                    // Seen to be good for TEPX hybrid
        for(unsigned int i = 0; i < 1000; i++) initSequence.push_back(0x0F0F); // 0011 0011
        break;

    case 4:                                                                    // Default for single chips (Doesn't work well with hybrids)
        for(unsigned int i = 0; i < 1000; i++) initSequence.push_back(0x0000); // 0000 0000
        break;

    default:                                                                   // Case 0 -> seems to be work with all
        for(unsigned int i = 0; i < 500; i++) initSequence.push_back(0x0000);  // 0000 0000
        for(unsigned int i = 0; i < 2000; i++) initSequence.push_back(0xCCCC); // 1100 1100
        break;
    }

    return initSequence;
}

uint32_t RD53FWInterface::GetHybridEnabledChips(const Hybrid* pHybrid)
{
    const uint32_t hybrid_id = pHybrid->getId();
    uint32_t       chips_en  = 0;

    if(this->singleChip == true)
        chips_en = 1 << hybrid_id;
    else
    {
        uint32_t hyb_chips_en = 0;
        for(const auto cChip: *pHybrid)
        {
            uint32_t chip_lane = static_cast<RD53*>(cChip)->getChipLane();
            hyb_chips_en |= 1 << chip_lane;
        }
        chips_en |= hyb_chips_en << (NLANE_HYBRID * hybrid_id);
    }

    return chips_en;
}

void RD53FWInterface::Start()
{
    RD53FWInterface::ChipReset();
    RD53FWInterface::ChipReSync();
    RD53FWInterface::ResetReadoutBlk();

    RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.start_trigger");
}

void RD53FWInterface::Stop() { RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.stop_trigger"); }

void RD53FWInterface::Pause() { RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.stop_trigger"); }

void RD53FWInterface::Resume() { RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.start_trigger"); }

void RD53FWInterface::TurnOffFMC() { RegManager::WriteStackReg({{"system.ctrl_2.fmc_pg_c2m", 0}, {"system.ctrl_2.fmc_l8_pwr_en", 0}, {"system.ctrl_2.fmc_l12_pwr_en", 0}}); }

void RD53FWInterface::TurnOnFMC()
{
    RegManager::WriteStackReg({{"system.ctrl_2.fmc_l12_pwr_en", 1}, {"system.ctrl_2.fmc_l8_pwr_en", 1}, {"system.ctrl_2.fmc_pg_c2m", 1}});

    usleep(DEEPSLEEP);
}

void RD53FWInterface::ResetBoard()
{
    // #######
    // # Set #
    // #######
    WriteReg("user.ctrl_regs.reset_reg.aurora_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.aurora_pma_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.global_rst", 1);
    WriteReg("user.ctrl_regs.reset_reg.clk_gen_rst", 1);
    WriteReg("user.ctrl_regs.reset_reg.fmc_pll_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.cmd_rst", 1);
    WriteReg("user.ctrl_regs.reset_reg.i2c_rst", 1);

    // #########
    // # Reset #
    // #########
    WriteReg("user.ctrl_regs.reset_reg.global_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.clk_gen_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.fmc_pll_rst", 1);
    WriteReg("user.ctrl_regs.reset_reg.cmd_rst", 0);

    usleep(DEEPSLEEP);

    WriteReg("user.ctrl_regs.reset_reg.i2c_rst", 0);
    WriteReg("user.ctrl_regs.reset_reg.aurora_pma_rst", 1);
    WriteReg("user.ctrl_regs.reset_reg.aurora_rst", 1);

    // ########
    // # DDR3 #
    // ########
    LOG(INFO) << YELLOW << "Waiting for DDR3 calibration..." << RESET;
    while(ReadReg("user.stat_regs.readout1.ddr3_initial_calibration_done") == false) usleep(DEEPSLEEP);

    LOG(INFO) << BOLDBLUE << "\t--> DDR3 calibration done" << RESET;
}

void RD53FWInterface::ResetFastCmdBlk()
{
    RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.ipb_reset");

    WriteReg("user.ctrl_regs.fast_cmd_reg_1.ipb_fast_duration", IPBUS_FASTDURATION);
}

void RD53FWInterface::ResetSlowCmdBlk()
{
    RegManager::WriteStackReg(
        {{"user.ctrl_regs.Slow_cmd.fifo_reset", 1}, {"user.ctrl_regs.Slow_cmd.fifo_reset", 0}, {"user.ctrl_regs.Register_RdBack.fifo_reset", 1}, {"user.ctrl_regs.Register_RdBack.fifo_reset", 0}});
}

void RD53FWInterface::ResetReadoutBlk()
{
    ddr3Offset = 0;
    RegManager::WriteStackReg({{"user.ctrl_regs.reset_reg.readout_block_rst", 1}, {"user.ctrl_regs.reset_reg.readout_block_rst", 0}});
}

void RD53FWInterface::ChipReset()
{
    RegManager::WriteStackReg(
        {{"user.ctrl_regs.reset_reg.scc_rst", 1}, {"user.ctrl_regs.reset_reg.scc_rst", 0}, {"user.ctrl_regs.fast_cmd_reg_1.ipb_ecr", 1}, {"user.ctrl_regs.fast_cmd_reg_1.ipb_ecr", 0}});
}

void RD53FWInterface::ChipReSync() { RegManager::WriteStackReg({{"user.ctrl_regs.fast_cmd_reg_1.ipb_bcr", 1}, {"user.ctrl_regs.fast_cmd_reg_1.ipb_bcr", 0}}); }

void RD53FWInterface::PrintEvents(const std::vector<RD53FWInterface::Event>& events, const std::vector<uint32_t>& pData)
{
    // ##################
    // # Print raw data #
    // ##################
    if(pData.size() != 0)
        for(auto j = 0u; j < pData.size(); j++)
        {
            if(j % NWORDS_DDR3 == 0) std::cout << std::dec << j << ":\t";
            std::cout << std::hex << std::setfill('0') << std::setw(8) << pData[j] << "\t";
            if(j % NWORDS_DDR3 == NWORDS_DDR3 - 1) std::cout << std::endl;
        }

    // ######################
    // # Print decoded data #
    // ######################
    for(auto i = 0u; i < events.size(); i++)
    {
        auto& evt = events[i];
        LOG(INFO) << BOLDGREEN << "===========================" << RESET;
        LOG(INFO) << BOLDGREEN << "EVENT           = " << i << RESET;
        LOG(INFO) << BOLDGREEN << "block_size      = " << evt.block_size << RESET;
        LOG(INFO) << BOLDGREEN << "tlu_trigger_id  = " << evt.tlu_trigger_id << RESET;
        LOG(INFO) << BOLDGREEN << "data_format_ver = " << evt.data_format_ver << RESET;
        LOG(INFO) << BOLDGREEN << "tdc             = " << evt.tdc << RESET;
        LOG(INFO) << BOLDGREEN << "l1a_counter     = " << evt.l1a_counter << RESET;
        LOG(INFO) << BOLDGREEN << "bx_counter      = " << evt.bx_counter << RESET;

        for(auto j = 0u; j < evt.chip_events.size(); j++)
        {
            LOG(INFO) << CYAN << "------- Chip Header -------" << RESET;
            LOG(INFO) << CYAN << "error_code      = " << evt.chip_frames[j].error_code << RESET;
            LOG(INFO) << CYAN << "hybrid_id       = " << evt.chip_frames[j].hybrid_id << RESET;
            LOG(INFO) << CYAN << "chip_lane       = " << evt.chip_frames[j].chip_lane << RESET;
            LOG(INFO) << CYAN << "l1a_data_size   = " << evt.chip_frames[j].l1a_data_size << RESET;
            LOG(INFO) << CYAN << "chip_type       = " << evt.chip_frames[j].chip_type << RESET;
            LOG(INFO) << CYAN << "frame_delay     = " << evt.chip_frames[j].frame_delay << RESET;

            LOG(INFO) << CYAN << "trigger_id      = " << evt.chip_events[j].trigger_id << RESET;
            LOG(INFO) << CYAN << "trigger_tag     = " << evt.chip_events[j].trigger_tag << RESET;
            LOG(INFO) << CYAN << "bc_id           = " << evt.chip_events[j].bc_id << RESET;

            LOG(INFO) << BOLDYELLOW << "--- Hit Data (" << evt.chip_events[j].hit_data.size() << " hits) ---" << RESET;

            for(const auto& hit: evt.chip_events[j].hit_data)
            {
                LOG(INFO) << BOLDYELLOW << "Column: " << std::setw(3) << hit.col << std::setw(-1) << ", Row: " << std::setw(3) << hit.row << std::setw(-1) << ", ToT: " << std::setw(3) << +hit.tot
                          << std::setw(-1) << RESET;
            }
        }
    }
}

bool RD53FWInterface::EvtErrorHandler(uint16_t status)
{
    bool isGood = true;

    if(status & RD53FWEvtEncoder::EVSIZE)
    {
        LOG(ERROR) << BOLDRED << "Invalid event size " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::EMPTY)
    {
        LOG(ERROR) << BOLDRED << "No data collected " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::NOHEADER)
    {
        LOG(ERROR) << BOLDRED << "No event headear found in data " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::INCOMPLETE)
    {
        LOG(ERROR) << BOLDRED << "Incomplete event header " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::L1A)
    {
        LOG(ERROR) << BOLDRED << "L1A counter mismatch " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::FWERR)
    {
        LOG(ERROR) << BOLDRED << "Firmware error " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::FRSIZE)
    {
        LOG(ERROR) << BOLDRED << "Invalid frame size " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53FWEvtEncoder::MISSCHIP)
    {
        LOG(ERROR) << BOLDRED << "Chip data are missing " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPHEAD)
    {
        LOG(ERROR) << BOLDRED << "Invalid chip header " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPPIX)
    {
        LOG(ERROR) << BOLDRED << "Invalid pixel row or column " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    if(status & RD53EvtEncoder::CHIPNOHIT)
    {
        LOG(ERROR) << BOLDRED << " Hit data are missing " << BOLDYELLOW << "--> retry" << std::setfill(' ') << std::setw(8) << "" << RESET;
        isGood = false;
    }

    return isGood;
}

uint32_t RD53FWInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    uint32_t nWordsInMemoryOld, nWordsInMemory = 0;

    // #############################################
    // # Wait for a stable number of words to read #
    // #############################################
    nWordsInMemory = ReadReg("user.stat_regs.words_to_read");
    do
    {
        nWordsInMemoryOld = nWordsInMemory;
        usleep(READOUTSLEEP);
    } while(((nWordsInMemory = ReadReg("user.stat_regs.words_to_read")) != nWordsInMemoryOld) && (pWait == true));
    // auto nTriggersReceived = ReadReg("user.stat_regs.trigger_cntr");

    // #############
    // # Read DDR3 #
    // #############
    std::vector<uint32_t> values = ReadBlockRegOffset("ddr3.fc7_daq_ddr3", nWordsInMemory, ddr3Offset);
    ddr3Offset += nWordsInMemory;
    pData.insert(pData.end(), values.begin(), values.end());

    if((this->fSaveToFile == true) && (pData.size() != 0)) this->fFileHandler->setData(pData);
    return pData.size();
}

void RD53FWInterface::ReadNEvents(BeBoard* pBoard, uint32_t pNEvents, std::vector<uint32_t>& pData, bool pWait)
{
    bool retry;
    int  nAttempts = 0;

    RD53FWInterface::localCfgFastCmd.n_triggers = pNEvents;
    RD53FWInterface::ConfigureFastCommands();

    do
    {
        nAttempts++;
        retry = false;
        pData.clear();

        // ####################
        // # Readout sequence #
        // ####################
        RD53FWInterface::Start();
        while(ReadReg("user.stat_regs.trigger_cntr") < pNEvents * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration)) usleep(READOUTSLEEP);
        RD53FWInterface::ReadData(pBoard, false, pData, pWait);
        RD53FWInterface::Stop();

        // ##################
        // # Error checking #
        // ##################
        decodedEvents.clear();
        uint16_t status = RD53FWInterface::DecodeEventsMultiThreads(pData, decodedEvents); // Decode events with multiple threads
        // uint16_t status = RD53FWInterface::DecodeEvents(pData, decodedEvents, {});         // Decode events with a single thread
        // RD53FWInterface::PrintEvents(decodedEvents, pData); // @TMP@
        if(RD53FWInterface::EvtErrorHandler(status) == false)
        {
            retry = true;
            continue;
        }

        if(decodedEvents.size() != RD53FWInterface::localCfgFastCmd.n_triggers * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration))
        {
            LOG(ERROR) << BOLDRED << "Sent " << RD53FWInterface::localCfgFastCmd.n_triggers * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration) << " triggers, but collected "
                       << decodedEvents.size() << " events" << BOLDYELLOW << " --> retry" << RESET;
            retry = true;
            continue;
        }

    } while((retry == true) && (nAttempts < MAXATTEMPTS));

    if(retry == true)
    {
        LOG(ERROR) << BOLDRED << "Reached maximum number of attempts (" << BOLDYELLOW << MAXATTEMPTS << BOLDRED << ") without success" << RESET;
        pData.clear();
    }

    // #################
    // # Show progress #
    // #################
    RD53RunProgress::update(pData.size(), true);
}
/*
// ##########################################
// # Use of OpenMP (compiler flag -fopenmp) #
// ##########################################
uint16_t RD53FWInterface::DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events)
{
    // ######################
    // # Consistency checks #
    // ######################
    if(data.size() == 0) return RD53FWEvtEncoder::EMPTY;

    uint16_t evtStatus = RD53FWEvtEncoder::GOOD;

    std::vector<size_t> eventStart;
    size_t              i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
        }
        else
            i++;
    if(eventStart.size() == 0) return RD53FWEvtEncoder::NOHEADER;
    const auto nEvents = ceil(static_cast<double>(eventStart.size()) / omp_get_max_threads());
    eventStart.push_back(data.size());

    // ######################
    // # Unpack data vector #
    // ######################
#pragma omp parallel
    {
        std::vector<RD53FWInterface::Event> vecEvents;
        std::vector<size_t>                 vecEventStart;

        if(eventStart.begin() + nEvents * omp_get_thread_num() < eventStart.end())
        {
            auto firstEvent = eventStart.begin() + nEvents * omp_get_thread_num();
            auto lastEvent  = firstEvent + nEvents + 1 < eventStart.end() ? firstEvent + nEvents + 1 : eventStart.end();
            std::move(firstEvent, lastEvent, std::back_inserter(vecEventStart));

            uint16_t status = RD53FWInterface::DecodeEvents(data, vecEvents, vecEventStart);

#pragma omp atomic
            evtStatus |= status;

            // #####################
            // # Pack event vector #
            // #####################
#pragma omp critical
            std::move(vecEvents.begin(), vecEvents.end(), std::back_inserter(events));
        }
    }

    return evtStatus;
}
*/
uint16_t RD53FWInterface::DecodeEventsMultiThreads(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events)
{
    // ######################
    // # Consistency checks #
    // ######################
    if(data.size() == 0) return RD53FWEvtEncoder::EMPTY;

    std::atomic<uint16_t> evtStatus;
    evtStatus.store(RD53FWEvtEncoder::GOOD);

    std::vector<std::vector<RD53FWInterface::Event>> vecEvents(RD53Shared::NTHREADS);
    std::vector<std::thread>                         vecThrDecoders(RD53Shared::NTHREADS);
    std::vector<std::vector<size_t>>                 vecEventStart(RD53Shared::NTHREADS);

    std::vector<size_t> eventStart;
    size_t              i = 0u;
    while(i < data.size())
        if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
        {
            eventStart.push_back(i);
            i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
        }
        else
            i++;
    if(eventStart.size() == 0) return RD53FWEvtEncoder::NOHEADER;
    const auto nEvents = ceil(static_cast<double>(eventStart.size()) / RD53Shared::NTHREADS);
    eventStart.push_back(data.size());

    // ######################
    // # Unpack data vector #
    // ######################
    for(i = 0u; i < RD53Shared::NTHREADS - 1; i++)
    {
        auto firstEvent = eventStart.begin() + nEvents * i;
        if(firstEvent + nEvents + 1 > eventStart.end() - 1) break;
        auto lastEvent = firstEvent + nEvents + 1;
        std::move(firstEvent, lastEvent, std::back_inserter(vecEventStart[i]));

        vecThrDecoders[i] = std::thread(&RD53FWInterface::DecodeEventsWrapper, std::ref(data), std::ref(vecEvents[i]), std::ref(vecEventStart[i]), std::ref(evtStatus));
    }

    auto firstEvent = eventStart.begin() + nEvents * i;
    auto lastEvent  = eventStart.end();
    std::move(firstEvent, lastEvent, std::back_inserter(vecEventStart[i]));

    evtStatus |= RD53FWInterface::DecodeEvents(data, vecEvents[i], vecEventStart[i]);

    // ################
    // # Join threads #
    // ################
    for(auto& thr: vecThrDecoders)
        if(thr.joinable() == true) thr.join();

    // #####################
    // # Pack event vector #
    // #####################
    for(auto i = 0u; i < RD53Shared::NTHREADS; i++) std::move(vecEvents[i].begin(), vecEvents[i].end(), std::back_inserter(events));

    return evtStatus;
}

void RD53FWInterface::DecodeEventsWrapper(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events, const std::vector<size_t>& eventStart, std::atomic<uint16_t>& evtStatus)
{
    evtStatus |= RD53FWInterface::DecodeEvents(data, events, eventStart);
}

uint16_t RD53FWInterface::DecodeEvents(const std::vector<uint32_t>& data, std::vector<RD53FWInterface::Event>& events, const std::vector<size_t>& eventStartExt)
{
    std::vector<size_t>  eventStartLocal;
    std::vector<size_t>& refEventStart = const_cast<std::vector<size_t>&>(eventStartExt);
    uint16_t             evtStatus     = RD53FWEvtEncoder::GOOD;
    const size_t         maxL1Counter  = RD53Shared::setBits(RD53EvtEncoder::NBIT_TRIGID) + 1;

    // ######################
    // # Consistency checks #
    // ######################
    if(data.size() == 0) return RD53FWEvtEncoder::EMPTY;

    if(eventStartExt.size() == 0)
    {
        size_t i = 0u;
        while(i < data.size())
            if(data[i] >> RD53FWEvtEncoder::NBIT_BLOCKSIZE == RD53FWEvtEncoder::EVT_HEADER)
            {
                eventStartLocal.push_back(i);
                i += RD53FWEvtEncoder::EVT_HEADER_SIZE;
            }
            else
                i++;
        if(eventStartLocal.size() == 0) return RD53FWEvtEncoder::NOHEADER;
        eventStartLocal.push_back(data.size());
        refEventStart = eventStartLocal;
    }

    events.reserve(events.size() + refEventStart.size() - 1);

    for(auto i = 0u; i < refEventStart.size() - 1; i++)
    {
        const auto start = refEventStart[i];
        const auto end   = refEventStart[i + 1];

        events.emplace_back(&data[start], end - start);
        if(events.back().evtStatus != RD53FWEvtEncoder::GOOD)
            evtStatus |= events.back().evtStatus;
        else
        {
            for(auto j = 0u; j < events.back().chip_events.size(); j++)
                if(events.back().l1a_counter % maxL1Counter != events.back().chip_events[j].trigger_id) evtStatus |= RD53FWEvtEncoder::L1A;
        }
    }

    return evtStatus;
}

// ########################
// # Event Implementation #
// ########################
void RD53FWInterface::Event::addBoardInfo2Events(const BeBoard* pBoard, std::vector<RD53FWInterface::Event>& decodedEvents)
{
    for(auto& evt: decodedEvents)
        for(auto& chip_frame: evt.chip_frames)
        {
            int chip_id = RD53FWInterface::Event::lane2chipId(pBoard, 0, chip_frame.hybrid_id, chip_frame.chip_lane);
            if(chip_id != -1) chip_frame.chip_id = chip_id;
        }
}

void RD53FWInterface::Event::fillDataContainer(BoardDataContainer* boardContainer, const ChannelGroupBase* cTestChannelGroup)
{
    bool   vectorRequired = boardContainer->at(0)->at(0)->at(0)->isSummaryContainerType<Summary<GenericDataVector, OccupancyAndPh>>();
    size_t chipIndx;

    for(const auto& cOpticalGroup: *boardContainer)
        for(const auto& cHybrid: *cOpticalGroup)
            for(const auto& cChip: *cHybrid)
                if((evtStatus == RD53FWEvtEncoder::GOOD) && (RD53FWInterface::Event::isHittedChip(cHybrid->getId(), cChip->getId(), chipIndx) == true))
                {
                    if(vectorRequired == true)
                    {
                        cChip->getSummary<GenericDataVector, OccupancyAndPh>().data1.push_back(chip_events[chipIndx].bc_id);
                        cChip->getSummary<GenericDataVector, OccupancyAndPh>().data2.push_back(chip_events[chipIndx].trigger_id);
                    }

                    for(const auto& hit: chip_events[chipIndx].hit_data)
                    {
                        cChip->getChannel<OccupancyAndPh>(hit.row + Ph2_HwDescription::RD53::nRows * (hit.col)).fOccupancy++;
                        cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).fPh += float(hit.tot);
                        cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).fPhError += float(hit.tot * hit.tot);
                        if(cTestChannelGroup->isChannelEnabled(hit.row, hit.col) == false) cChip->getChannel<OccupancyAndPh>(hit.row, hit.col).readoutError = true;
                    }
                }
}

bool RD53FWInterface::Event::isHittedChip(uint8_t hybrid_id, uint8_t chip_id, size_t& chipIndx) const
{
    auto it = chip_frames.begin();
    it      = std::find_if(
        it, chip_frames.end(), [&](const ChipFrame& frame) { return ((frame.hybrid_id == hybrid_id) && (frame.chip_id == chip_id) && (chip_events[it - chip_frames.begin()].hit_data.size() != 0)); });

    if(it == chip_frames.end()) return false;
    chipIndx = it - chip_frames.begin();
    return true;
}

int RD53FWInterface::Event::lane2chipId(const BeBoard* pBoard, uint16_t optGroup_id, uint16_t hybrid_id, uint16_t chip_lane)
{
    // #############################
    // # Translate lane to chip ID #
    // #############################
    if(pBoard != nullptr)
    {
        auto opticalGroup = std::find_if(pBoard->begin(), pBoard->end(), [&](OpticalGroupContainer* cOpticalGroup) { return cOpticalGroup->getId() == optGroup_id; });
        if(opticalGroup != pBoard->end())
        {
            auto hybrid = std::find_if((*opticalGroup)->begin(), (*opticalGroup)->end(), [&](HybridContainer* cHybrid) { return cHybrid->getId() == hybrid_id; });
            if(hybrid != (*opticalGroup)->end())
            {
                auto it = std::find_if((*hybrid)->begin(), (*hybrid)->end(), [&](ChipContainer* pChip) { return static_cast<RD53*>(pChip)->getChipLane() == chip_lane; });
                if(it != (*hybrid)->end()) return (*it)->getId();
            }
        }
    }
    return -1; // Chip not found
}

RD53FWInterface::Event::Event(const uint32_t* data, size_t n)
{
    evtStatus = RD53FWEvtEncoder::GOOD;

    // ######################
    // # Consistency checks #
    // ######################
    if(n < RD53FWEvtEncoder::EVT_HEADER_SIZE)
    {
        evtStatus = RD53FWEvtEncoder::INCOMPLETE;
        return;
    }

    std::tie(block_size) = bits::unpack<RD53FWEvtEncoder::NBIT_BLOCKSIZE>(data[0]);
    if(block_size * NWORDS_DDR3 != n)
    {
        evtStatus = RD53FWEvtEncoder::EVSIZE;
        return;
    }

    // #######################
    // # Decode event header #
    // #######################
    bool dummy_size;
    std::tie(tlu_trigger_id, data_format_ver, dummy_size) = bits::unpack<RD53FWEvtEncoder::NBIT_TRIGID, RD53FWEvtEncoder::NBIT_FMTVER, RD53FWEvtEncoder::NBIT_DUMMY>(data[1]);
    std::tie(tdc, l1a_counter)                            = bits::unpack<RD53FWEvtEncoder::NBIT_TDC, RD53FWEvtEncoder::NBIT_L1ACNT>(data[2]);
    bx_counter                                            = data[3];

    // ############################
    // # Search for frame lengths #
    // ############################
    std::vector<size_t> event_sizes;
    size_t              index = 4;
    while(index < n - dummy_size * NWORDS_DDR3)
    {
        if(data[index] >> (RD53FWEvtEncoder::NBIT_ERR + RD53FWEvtEncoder::NBIT_HYBRID + RD53FWEvtEncoder::NBIT_CHIPID + RD53FWEvtEncoder::NBIT_L1ASIZE) != RD53FWEvtEncoder::FRAME_HEADER)
        {
            evtStatus |= RD53FWEvtEncoder::FRSIZE;
            return;
        }
        size_t size = (data[index] & ((1 << RD53FWEvtEncoder::NBIT_L1ASIZE) - 1)) * NWORDS_DDR3;
        event_sizes.push_back(size);
        index += size;
    }

    if(index != n - dummy_size * NWORDS_DDR3)
    {
        evtStatus |= RD53FWEvtEncoder::MISSCHIP;
        return;
    }

    // ##############################
    // # Decode frame and chip data #
    // ##############################
    chip_frames.reserve(event_sizes.size());
    chip_events.reserve(event_sizes.size());
    index = 4;
    for(auto size: event_sizes)
    {
        chip_frames.emplace_back(data[index], data[index + 1]);

        if(chip_frames.back().error_code != 0)
        {
            evtStatus |= RD53FWEvtEncoder::FWERR;
            chip_frames.clear();
            chip_events.clear();
            return;
        }

        chip_events.emplace_back(&data[index + 2], size - 2);

        if(chip_events.back().evtStatus != RD53EvtEncoder::CHIPGOOD) evtStatus |= chip_events.back().evtStatus;

        index += size;
    }
}

RD53FWInterface::ChipFrame::ChipFrame(const uint32_t data0, const uint32_t data1)
{
    std::tie(error_code, hybrid_id, chip_lane, l1a_data_size) =
        bits::unpack<RD53FWEvtEncoder::NBIT_ERR, RD53FWEvtEncoder::NBIT_HYBRID, RD53FWEvtEncoder::NBIT_CHIPID, RD53FWEvtEncoder::NBIT_L1ASIZE>(data0);
    std::tie(chip_type, frame_delay) = bits::unpack<RD53FWEvtEncoder::NBIT_CHIPTYPE, RD53FWEvtEncoder::NBIT_DELAY>(data1);
}
// ########################

void RD53FWInterface::SendBoardCommand(const std::string& cmd_reg)
{
    RegManager::WriteStackReg({{cmd_reg, 1}, {"user.ctrl_regs.fast_cmd_reg_1.cmd_strobe", 1}, {"user.ctrl_regs.fast_cmd_reg_1.cmd_strobe", 0}, {cmd_reg, 0}});
}

void RD53FWInterface::ConfigureFastCommands(const FastCommandsConfig* cfg)
{
    if(cfg == nullptr) cfg = &(RD53FWInterface::localCfgFastCmd);

    if(cfg->autozero_source == AutozeroSource::FastCMDFSM)
        WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, 44, 1 << 14).getFrames(), -1); // @TMP@ : GLOBAL_PULSE_RT = "Acquire Zero level in SYNC FE"

    // ##################################
    // # Configuring fast command block #
    // ##################################
    RegManager::WriteStackReg({// ############################
                               // # General data for trigger #
                               // ############################
                               {"user.ctrl_regs.fast_cmd_reg_2.trigger_source", (uint32_t)cfg->trigger_source},
                               {"user.ctrl_regs.fast_cmd_reg_2.backpressure_en", (uint32_t)cfg->backpressure_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.init_ecr_en", (uint32_t)cfg->initial_ecr_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.veto_en", (uint32_t)cfg->veto_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.ext_trig_delay", (uint32_t)cfg->ext_trigger_delay},
                               {"user.ctrl_regs.fast_cmd_reg_2.trigger_duration", (uint32_t)cfg->trigger_duration},
                               {"user.ctrl_regs.fast_cmd_reg_2.HitOr_enable_l12", (uint32_t)cfg->enable_hitor},
                               {"user.ctrl_regs.fast_cmd_reg_3.triggers_to_accept", (uint32_t)cfg->n_triggers},

                               // ##############################
                               // # Fast command configuration #
                               // ##############################
                               {"user.ctrl_regs.fast_cmd_reg_2.tp_fsm_ecr_en", (uint32_t)cfg->fast_cmd_fsm.ecr_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.tp_fsm_test_pulse_en", (uint32_t)cfg->fast_cmd_fsm.first_cal_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.tp_fsm_inject_pulse_en", (uint32_t)cfg->fast_cmd_fsm.second_cal_en},
                               {"user.ctrl_regs.fast_cmd_reg_2.tp_fsm_trigger_en", (uint32_t)(cfg->enable_hitor != 0 ? 0 : cfg->fast_cmd_fsm.trigger_en)},

                               {"user.ctrl_regs.fast_cmd_reg_6.delay_after_init_prime", (uint32_t)cfg->fast_cmd_fsm.delay_after_first_prime},
                               {"user.ctrl_regs.fast_cmd_reg_7.delay_after_ecr", (uint32_t)cfg->fast_cmd_fsm.delay_after_ecr},
                               {"user.ctrl_regs.fast_cmd_reg_6.delay_after_autozero", (uint32_t)cfg->fast_cmd_fsm.delay_after_autozero}, // @TMP@
                               {"user.ctrl_regs.fast_cmd_reg_4.cal_data_prime", (uint32_t)cfg->fast_cmd_fsm.first_cal_data},
                               {"user.ctrl_regs.fast_cmd_reg_4.delay_after_prime_pulse", (uint32_t)cfg->fast_cmd_fsm.delay_after_prime},
                               {"user.ctrl_regs.fast_cmd_reg_5.cal_data_inject", (uint32_t)cfg->fast_cmd_fsm.second_cal_data},
                               {"user.ctrl_regs.fast_cmd_reg_5.delay_after_inject_pulse", (uint32_t)cfg->fast_cmd_fsm.delay_after_inject},
                               {"user.ctrl_regs.fast_cmd_reg_6.delay_before_next_pulse", (uint32_t)cfg->fast_cmd_fsm.delay_after_trigger},

                               // ################################
                               // # @TMP@ Autozero configuration #
                               // ################################
                               {"user.ctrl_regs.fast_cmd_reg_2.autozero_source", (uint32_t)cfg->autozero_source},
                               {"user.ctrl_regs.fast_cmd_reg_7.glb_pulse_data", (uint32_t)bits::pack<4, 1, 4, 1>(RD53Constants::BROADCAST_CHIPID, 0, 8, 0)}});

    RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.load_config");
}

void RD53FWInterface::SetAndConfigureFastCommands(const BeBoard* pBoard,
                                                  const uint32_t nTRIGxEvent,
                                                  const size_t   injType,
                                                  const uint32_t injLatency,
                                                  const uint32_t nClkDelays,
                                                  const bool     enableAutozero)
// ############################
// # injType == 0 --> None    #
// # injType == 1 --> Analog  #
// # injType == 2 --> Digital #
// ############################
{
    const double FSMperiod = 1. / 10e6; // Referred to 10 MHz clock
    enum INJtype
    {
        None,
        Analog,
        Digital
    };
    enum INJdelay
    {
        AfterInjectCal = 32,
        BeforePrimeCal = 8,
        Loop           = 460
    };

    uint8_t chipId = RD53Constants::BROADCAST_CHIPID;

    // #############################
    // # Configuring FastCmd block #
    // #############################
    RD53FWInterface::localCfgFastCmd.n_triggers       = 0;
    RD53FWInterface::localCfgFastCmd.trigger_duration = ((injType == INJtype::None) && (RD53FWInterface::localCfgFastCmd.trigger_source == TriggerSource::FastCMDFSM) ? 0 : nTRIGxEvent - 1);

    if(injType == INJtype::Digital)
    {
        // #######################################
        // # Configuration for digital injection #
        // #######################################
        RD53::CalCmd calcmd_first(1, 2, 10, 0, 0);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_data = calcmd_first.getCalCmd(chipId);
        RD53::CalCmd calcmd_second(0, 0, 0, 0, 0);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_data = calcmd_second.getCalCmd(chipId);

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_first_prime = (nClkDelays == 0 ? (uint32_t)INJdelay::Loop : nClkDelays);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr         = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_inject      = (injLatency == 0 ? (uint32_t)INJdelay::AfterInjectCal : injLatency);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger     = INJdelay::BeforePrimeCal;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_prime       = (nClkDelays == 0 ? (uint32_t)INJdelay::Loop : nClkDelays);

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_en  = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_en = false;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.trigger_en    = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.ecr_en        = false;
    }
    else if(injType == INJtype::Analog)
    {
        // ######################################
        // # Configuration for analog injection #
        // ######################################
        RD53::CalCmd calcmd_first(1, 0, 0, 0, 0);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_data = calcmd_first.getCalCmd(chipId);
        RD53::CalCmd calcmd_second(0, 0, 1, 0, 0);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_data = calcmd_second.getCalCmd(chipId);

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_first_prime = (nClkDelays == 0 ? (uint32_t)INJdelay::Loop : nClkDelays);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr         = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_inject      = (injLatency == 0 ? (uint32_t)INJdelay::AfterInjectCal : injLatency);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger     = INJdelay::BeforePrimeCal;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_prime       = (nClkDelays == 0 ? (uint32_t)INJdelay::Loop : nClkDelays);

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_en  = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_en = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.trigger_en    = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.ecr_en        = false;

        // @TMP@
        if(enableAutozero == true)
        {
            RD53FWInterface::localCfgFastCmd.autozero_source                   = AutozeroSource::FastCMDFSM;
            RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.ecr_en               = true;
            RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr      = 512;
            RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_autozero = 128;
        }
    }
    else if(injType == INJtype::None)
    {
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_data  = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_data = 0;

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_first_prime = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr         = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_inject      = 0;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger     = (nClkDelays == 0 ? (uint32_t)INJdelay::Loop : nClkDelays);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_prime       = 0;

        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_en  = false;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.second_cal_en = false;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.trigger_en    = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.ecr_en        = false;

        // @TMP@
        if(enableAutozero == true)
        {
            RD53FWInterface::localCfgFastCmd.autozero_source                   = AutozeroSource::FastCMDFSM;
            RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_autozero = RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger;
            RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger  = 0;
        }
    }
    else
        LOG(ERROR) << BOLDRED << "Option not recognized " << injType << RESET;

    LOG(INFO) << GREEN << "Internal trigger frequency (if enabled): " << BOLDYELLOW << std::fixed << std::setprecision(0)
              << 1. / (FSMperiod * (RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr + RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_inject +
                                    RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger + RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_prime))
              << std::setprecision(-1) << " Hz" << RESET;
    RD53Shared::resetDefaultFloat();

    // ##############################
    // # Download the configuration #
    // ##############################
    RD53FWInterface::ConfigureFastCommands();
    RD53FWInterface::PrintFWstatus();
}

void RD53FWInterface::ConfigureDIO5(const DIO5Config* cfg)
{
    const uint8_t fiftyOhmEnable = 0x12;

    if(ReadReg("user.stat_regs.stat_dio5.dio5_not_ready") == true) LOG(ERROR) << BOLDRED << "DIO5 not ready" << RESET;

    if(ReadReg("user.stat_regs.stat_dio5.dio5_error") == true) LOG(ERROR) << BOLDRED << "DIO5 is in error" << RESET;

    RegManager::WriteStackReg({{"user.ctrl_regs.ext_tlu_reg1.dio5_en", (uint32_t)cfg->enable},
                               {"user.ctrl_regs.ext_tlu_reg1.dio5_ch_out_en", (uint32_t)cfg->ch_out_en},
                               {"user.ctrl_regs.ext_tlu_reg1.dio5_term_50ohm_en", (uint32_t)fiftyOhmEnable},
                               {"user.ctrl_regs.ext_tlu_reg1.dio5_ch1_thr", (uint32_t)cfg->ch1_thr},
                               {"user.ctrl_regs.ext_tlu_reg1.dio5_ch2_thr", (uint32_t)cfg->ch2_thr},
                               {"user.ctrl_regs.ext_tlu_reg2.dio5_ch3_thr", (uint32_t)cfg->ch3_thr},
                               {"user.ctrl_regs.ext_tlu_reg2.dio5_ch4_thr", (uint32_t)cfg->ch4_thr},
                               {"user.ctrl_regs.ext_tlu_reg2.dio5_ch5_thr", (uint32_t)cfg->ch5_thr},
                               {"user.ctrl_regs.ext_tlu_reg2.tlu_en", (uint32_t)cfg->tlu_en},
                               {"user.ctrl_regs.ext_tlu_reg2.tlu_handshake_mode", (uint32_t)cfg->tlu_handshake_mode},
                               {"user.ctrl_regs.ext_tlu_reg2.ext_clk_en", (uint32_t)cfg->ext_clk_en},

                               {"user.ctrl_regs.ext_tlu_reg2.dio5_load_config", 1},
                               {"user.ctrl_regs.ext_tlu_reg2.dio5_load_config", 0}});
}

// ############################
// # Read/Write Optical Group #
// ############################
void RD53FWInterface::StatusOptoLink(Chip* pChip, uint32_t& isReady, uint32_t& isFIFOempty)
{
    isReady     = ReadReg("user.stat_regs.lpgbt_1.ic_tx_ready");
    isFIFOempty = ReadReg("user.stat_regs.lpgbt_1.ic_rx_empty");
}

void RD53FWInterface::ResetOptoLink(Chip* pChip)
{
    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link.ic_tx_reset", 0x01}, {"user.ctrl_regs.Optical_link.ic_rx_reset", 0x01}});

    usleep(DEEPSLEEP);

    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link.ic_tx_reset", 0x00}, {"user.ctrl_regs.Optical_link.ic_rx_reset", 0x00}});
}

bool RD53FWInterface::WriteOptoLinkRegister(Chip* pChip, uint32_t pAddress, uint32_t pData, bool pVerifLoop)
{
    const uint8_t lpGBTAddress = 0x70;

    // Config
    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link_cnfg1.ic_tx_fifo_din", pData},
                               {"user.ctrl_regs.Optical_link_cnfg1.ic_chip_addr_tx", lpGBTAddress},
                               {"user.ctrl_regs.Optical_link_cnfg2.ic_reg_addr_tx", pAddress}});

    // Perform operation
    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link.ic_tx_fifo_wr_en", 0x01},
                               {"user.ctrl_regs.Optical_link.ic_tx_fifo_wr_en", 0x00},
                               {"user.ctrl_regs.Optical_link.ic_send_wr_cmd", 0x01},
                               {"user.ctrl_regs.Optical_link.ic_send_wr_cmd", 0x00}});

    if(pVerifLoop == true)
    {
        uint32_t cReadBack = RD53FWInterface::ReadOptoLinkRegister(pChip, pAddress);
        if(cReadBack != pData)
        {
            LOG(ERROR) << BOLDRED << "[RD53FWInterface::WriteOpticalLinkRegiser] Register readback failure for register 0x" << BOLDYELLOW << std::hex << pAddress << std::dec << RESET;
            return false;
        }
    }

    return true;
}

uint32_t RD53FWInterface::ReadOptoLinkRegister(Chip* pChip, uint32_t pAddress)
{
    const uint8_t lpGBTAddress = 0x70;

    // Config
    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link_cnfg1.ic_chip_addr_tx", lpGBTAddress},
                               {"user.ctrl_regs.Optical_link_cnfg2.ic_reg_addr_tx", pAddress},
                               {"user.ctrl_regs.Optical_link_cnfg2.ic_nb_of_words_to_read_tx", 0x01}});

    // Perform operation
    RegManager::WriteStackReg(
        {{"user.ctrl_regs.Optical_link_cnfg2.ic_nb_of_words_to_read_tx", 0x01}, {"user.ctrl_regs.Optical_link.ic_send_rd_cmd", 0x01}, {"user.ctrl_regs.Optical_link.ic_send_rd_cmd", 0x00}});

    // Actual readback one word at a time
    uint32_t chipAddrRx  = ReadReg("user.stat_regs.lpgbt_1.ic_chip_addr_rx"); // Should be the same as lpGBTAddress
    uint32_t regAddrRx   = ReadReg("user.stat_regs.lpgbt_2.ic_reg_addr_rx");
    uint32_t nWords2Read = ReadReg("user.stat_regs.lpgbt_2.ic_nb_of_words_rx");
    RegManager::WriteStackReg({{"user.ctrl_regs.Optical_link.ic_rx_fifo_rd_en", 0x01}, {"user.ctrl_regs.Optical_link.ic_rx_fifo_rd_en", 0x00}});
    uint32_t cRead         = ReadReg("user.stat_regs.lpgbt_1.ic_rx_fifo_dout");
    uint32_t isRxFIFOempty = ReadReg("user.stat_regs.lpgbt_1.ic_rx_empty");

    LOG(INFO) << GREEN << std::hex << "Chip address rx = 0x" << BOLDYELLOW << chipAddrRx << RESET << GREEN << ". Reg address rx = 0x" << BOLDYELLOW << regAddrRx << RESET << GREEN
              << ". Nb of words received = 0x" << BOLDYELLOW << nWords2Read << RESET << GREEN << ". FIFO readback data = 0x" << BOLDYELLOW << cRead << RESET << GREEN << ". FIFO empty flag = 0x"
              << BOLDYELLOW << isRxFIFOempty << std::dec << RESET;

    return cRead;
}

// ###########################################
// # Member functions to handle the firmware #
// ###########################################
void RD53FWInterface::FlashProm(const std::string& strConfig, const char* fileName)
{
    CheckIfUploading();
    fpgaConfig->runUpload(strConfig, fileName);
}

void RD53FWInterface::JumpToFpgaConfig(const std::string& strConfig)
{
    CheckIfUploading();
    fpgaConfig->jumpToImage(strConfig);
}

void RD53FWInterface::DownloadFpgaConfig(const std::string& strConfig, const std::string& strDest)
{
    CheckIfUploading();
    fpgaConfig->runDownload(strConfig, strDest.c_str());
}

std::vector<std::string> RD53FWInterface::getFpgaConfigList()
{
    CheckIfUploading();
    return fpgaConfig->getFirmwareImageNames();
}

void RD53FWInterface::DeleteFpgaConfig(const std::string& strId)
{
    CheckIfUploading();
    fpgaConfig->deleteFirmwareImage(strId);
}

void RD53FWInterface::CheckIfUploading()
{
    if(fpgaConfig && fpgaConfig->getUploadingFpga() > 0) throw Exception("This board is uploading an FPGA configuration");

    if(!fpgaConfig) fpgaConfig = new D19cFpgaConfig(this);
}

void RD53FWInterface::RebootBoard()
{
    if(!fpgaConfig) fpgaConfig = new D19cFpgaConfig(this);
    fpgaConfig->resetBoard();
}

const FpgaConfig* RD53FWInterface::GetConfiguringFpga() { return (const FpgaConfig*)fpgaConfig; }

// ###################
// # Clock generator #
// ###################
void RD53FWInterface::InitializeClockGenerator(bool doStoreInEEPROM)
{
    const uint32_t writeSPI(0x8FA38014);    // Write to SPI
    const uint32_t writeEEPROM(0x8FA38014); // Write to EEPROM
    const uint32_t SPIregSettings[] = {
        0xEB020320, // OUT0 --> This clock is not used, but it can be used as another GBT clock (160 MHz, LVDS, phase
                    // shift 0 deg)
        0xEB020321, // OUT1 --> GBT clock reference: 160 MHz, LVDS, phase shift 0 deg (0xEB820321: 320 MHz, LVDS, phase
                    // shift 0 deg)
        0xEB840302, // OUT2 --> DDR3 clock reference: 240 MHz, LVDS, phase shift 0 deg
        0xEB840303, // OUT3 --> Not used (240 MHz, LVDS, phase shift 0 deg)
        0xEB140334, // OUT4 --> Not used (40 MHz, LVDS, R4.1 = 1, ph4adjc = 0)
        0x10000E75, // Reference selection: 0x10000E75 primary reference, 0x10000EB5 secondary reference
        0x030E02E6, // VCO selection: 0xyyyyyyEy select VCO1 if CDCE reference is 40 MHz, 0xyyyyyyFy select VCO2 if CDCE
                    // reference is > 40 MHz
        // VCO1, PS = 4, FD = 12, FB = 1, ChargePump 50 uA, Internal Filter, R6.20 = 0, AuxOut = enable, AuxOut = OUT2
        0xBD800DF7, // RC network parameters: C2 = 473.5 pF, R2 = 98.6 kOhm, C1 = 0 pF, C3 = 0 pF, R3 = 5 kOhm etc,
                    // SEL_DEL2 = 1, SEL_DEL1 = 1
        0x80001808  // Sync command configuration
    };

    // 0xyy8403yy --> 240 MHz, LVDS, phase shift   0 deg
    // 0xyy8407yy --> 240 MHz, LVDS, phase shift  90 deg
    // 0xyy840Byy --> 240 MHz, LVDS, phase shift 180 deg
    // 0xyy840Fyy --> 240 MHz, LVDS, phase shift 270 deg

    // 0xyy1403yy --> 040 MHz
    // 0xyy0403yy --> 120 MHz
    // 0xyy0203yy --> 160 MHz
    // 0xyy8403yy --> 240 MHz
    // 0xyy8203yy --> 320 MHz
    // 0xyy8003yy --> 480 MHz

    for(const auto value: SPIregSettings)
    {
        WriteReg("system.spi.tx_data", value);
        WriteReg("system.spi.command", writeSPI);

        ReadReg("system.spi.rx_data"); // Dummy read
        ReadReg("system.spi.rx_data"); // Dummy read
    }

    // ############################################################################
    // # Load new settings otherwise CDCE uses whatever was in EEPROM at power up #
    // ############################################################################
    RegManager::WriteStackReg({{"system.ctrl.cdce_sync", 0}, {"system.ctrl.cdce_sync", 1}});

    // #########################
    // # Save config in EEPROM #
    // #########################
    if(doStoreInEEPROM == true)
    {
        WriteReg("system.spi.tx_data", writeEEPROM);
        WriteReg("system.spi.command", writeSPI);

        ReadReg("system.spi.rx_data"); // Dummy read
        ReadReg("system.spi.rx_data"); // Dummy read
    }
}

void RD53FWInterface::ReadClockGenerator()
{
    const uint32_t writeSPI(0x8FA38014); // Write to SPI
    const uint32_t SPIreadCommands[] = {0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0x8E};

    for(const auto value: SPIreadCommands)
    {
        WriteReg("system.spi.tx_data", value);
        WriteReg("system.spi.command", writeSPI);

        WriteReg("system.spi.tx_data", 0xAAAAAAAA); // Dummy write
        WriteReg("system.spi.command", writeSPI);

        uint32_t          readback = ReadReg("system.spi.rx_data");
        std::stringstream myString("");
        myString << std::right << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << readback << std::dec;
        LOG(INFO) << BOLDBLUE << "\t--> SPI register content: " << BOLDYELLOW << "0x" << myString.str() << RESET;
    }
}

// ########################################
// # Vector containing the decoded events #
// ########################################
std::vector<RD53FWInterface::Event> RD53FWInterface::decodedEvents;

// ################################################
// # I2C block for programming peripheral devices #
// ################################################
bool RD53FWInterface::I2cCmdAckWait(unsigned int nAttempts)
{
    const uint16_t I2CcmdAckGOOD = 0x01;
    uint16_t       status        = 0x02; // 0x02 = I2CcmdAckBAD
    uint16_t       cLoop         = 0;

    while(++cLoop < nAttempts)
    {
        status = ReadReg("user.stat_regs.global_reg.i2c_acq_err");
        if(status == I2CcmdAckGOOD) return true;
        usleep(DEEPSLEEP);
    }

    return false;
}

void RD53FWInterface::WriteI2C(std::vector<uint32_t>& data)
{
    const uint16_t I2CwriteREQ = 0x01;

    WriteReg("ctrl.board.i2c_req", 0); // Disable
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_reset", 1);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_reset", 0);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_fifo_rx_dsel", 1);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_req", I2CwriteREQ);
    usleep(DEEPSLEEP);

    /* bool outcome = */ RegManager::WriteBlockReg("ctrl.board.i2c_fifo_tx", data);
    usleep(DEEPSLEEP);

    if(I2cCmdAckWait(20) == false) throw Exception("[RD53FWInterface::WriteI2C] I2C transaction error");

    WriteReg("ctrl.board.i2c_req", 0); // Disable
    usleep(DEEPSLEEP);
}

void RD53FWInterface::ReadI2C(std::vector<uint32_t>& data)
{
    const uint16_t I2CreadREQ = 0x03;

    WriteReg("ctrl.board.i2c_req", 0); // Disable
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_reset", 1);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_reset", 0);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_fifo_rx_dsel", 1);
    usleep(DEEPSLEEP);
    WriteReg("ctrl.board.i2c_req", I2CreadREQ);
    usleep(DEEPSLEEP);

    uint32_t sizeI2Cfifo = ReadReg("stat.board.i2c_fifo_rx_dcnt");
    usleep(DEEPSLEEP);

    int size2read = 0;
    if(sizeI2Cfifo > data.size())
    {
        size2read = data.size();
        LOG(WARNING) << BOLDRED << "[RD53FWInterface::ReadI2C] I2C FIFO contains more data than the vector size" << RESET;
    }
    else
        size2read = sizeI2Cfifo;

    data = RegManager::ReadBlockReg("ctrl.board.i2c_fifo_rx", size2read);
    usleep(DEEPSLEEP);

    if(RD53FWInterface::I2cCmdAckWait(20) == false) throw Exception("[RD53FWInterface::ReadI2C] I2C transaction error");

    WriteReg("ctrl.board.i2c_req", 0); // Disable
}

void RD53FWInterface::ConfigureClockSi5324()
{
    // ###################################################
    // # The Si5324 chip generates the clock for the GTX #
    // ###################################################

    uint8_t start_wr = 0x90;
    uint8_t stop_wr  = 0x50;
    // uint8_t stop_rd_nack   = 0x68;
    // uint8_t rd_incr        = 0x20;
    uint8_t wr_incr = 0x10;

    uint8_t enable_i2cmux  = 1;
    uint8_t disable_i2cmux = 0;

    uint8_t i2cmux_addr_wr = 0xe8;
    // uint8_t i2cmux_addr_rd = 0xe9;

    uint8_t si5324_pos     = 7;
    uint8_t si5324_addr_wr = 0xd0;
    // uint8_t si5324_addr_rd = 0xd1;

    uint32_t              word;
    std::vector<uint32_t> data;

    // #############
    // # Frequency #
    // #############
    const int N1_HS  = 0;
    const int NC1_LS = 19;
    const int N2_HS  = 1;
    const int N2_LS  = 511;
    const int N32    = 31;

    // ############################################
    // # Program Si5324 for 160 MHz precise clock #
    // ############################################
    std::vector<std::pair<uint8_t, uint8_t>> si5324Program;
    si5324Program.push_back({0x00, 0x54}); // Free running mode = 1, CKOUT_ALWAYS_ON = 0
    si5324Program.push_back({0x0B, 0x41}); // Disable CLKIN1
    si5324Program.push_back({0x06, 0x0F}); // Disable CKOUT2 (SFOUT2_REG=001), set CKOUT1 to LVDS (SFOUT1_REG=111)
    si5324Program.push_back({0x15, 0xFE}); // CKSEL_PIN = 0
    si5324Program.push_back({0x03, 0x55}); // CKIN2 selected, SQ_ICAL = 1

    si5324Program.push_back({0x02, 0x22});

    si5324Program.push_back({0x19, N1_HS << 5});
    si5324Program.push_back({0x1F, NC1_LS >> 16});
    si5324Program.push_back({0x20, NC1_LS >> 8});
    si5324Program.push_back({0x21, NC1_LS});
    si5324Program.push_back({0x28, (N2_HS << 5) | (N2_LS >> 16)});
    si5324Program.push_back({0x29, N2_LS >> 8});
    si5324Program.push_back({0x2A, N2_LS});
    si5324Program.push_back({0x2E, N32 >> 16});
    si5324Program.push_back({0x2F, N32 >> 8});
    si5324Program.push_back({0x30, N32});

    si5324Program.push_back({0x89, 0x01}); // FASTLOCK = 1
    si5324Program.push_back({0x88, 0x40}); // ICAL = 1
    // ###########################################

    word = (i2cmux_addr_wr << 8) | start_wr;
    data.push_back(word);
    word = (enable_i2cmux << si5324_pos) << 8 | stop_wr;
    data.push_back(word);

    for(auto i = 0u; i < si5324Program.size(); i++)
    {
        word = (si5324_addr_wr << 8) | start_wr;
        data.push_back(word);
        word = (si5324Program[i].first << 8) | wr_incr;
        data.push_back(word);
        word = (si5324Program[i].second << 8) | stop_wr;
        data.push_back(word);
    }

    word = (i2cmux_addr_wr << 8) | start_wr;
    data.push_back(word);
    word = (disable_i2cmux << si5324_pos) << 8 | stop_wr;
    data.push_back(word);

    RD53FWInterface::WriteI2C(data);
}

// #################################################
// # FMC ADC measurements: temperature and voltage #
// #################################################

float RD53FWInterface::ReadHybridTemperature(int hybridId)
{
    WriteReg("user.ctrl_regs.i2c_block.dp_addr", hybridId);
    usleep(DEEPSLEEP);
    uint32_t sensor1 = ReadReg("user.stat_regs.i2c_block_1.NTC1");
    usleep(DEEPSLEEP);
    uint32_t sensor2 = ReadReg("user.stat_regs.i2c_block_1.NTC2");
    usleep(DEEPSLEEP);

    auto value = calcTemperature(sensor1, sensor2);
    LOG(INFO) << BOLDBLUE << "\t--> Hybrid temperature: " << BOLDYELLOW << std::setprecision(3) << value << BOLDBLUE << " C" << std::setprecision(-1) << RESET;

    return value;
}

float RD53FWInterface::ReadHybridVoltage(int hybridId)
{
    WriteReg("user.ctrl_regs.i2c_block.dp_addr", hybridId);
    usleep(DEEPSLEEP);
    uint32_t senseVDD = ReadReg("user.stat_regs.i2c_block_2.vdd_sense");
    usleep(DEEPSLEEP);
    uint32_t senseGND = ReadReg("user.stat_regs.i2c_block_2.gnd_sense");
    usleep(DEEPSLEEP);

    auto value = calcVoltage(senseVDD, senseGND);
    LOG(INFO) << BOLDBLUE << "\t--> Hybrid voltage: " << BOLDYELLOW << std::setprecision(3) << value << BOLDBLUE << " V (corresponds to half VOUT_dig_ShuLDO of the chip)" << std::setprecision(-1)
              << RESET;

    return value;
}

float RD53FWInterface::calcTemperature(uint32_t sensor1, uint32_t sensor2, int beta)
{
    // #####################
    // # Natural constants #
    // #####################
    const float T0C  = 273.15; // [Kelvin]
    const float T25C = 298.15; // [Kelvin]
    const float R25C = 10;     // [kOhm]
    // For precise T measurements we should have individual -beta- for each temperature sensor
    // i.e. NTC thermistors = Negative Temperature Coefficient, measured in Kelvin

    // ##################################
    // # Voltage divider circuit on FMC #
    // ##################################
    const float Rdivider = 39.2; // [kOhm]
    const float Vdivider = 2.5;  // [V]

    // ###################
    // # Voltage per LSB #
    // ###################
    const float  safetyMargin       = 0.9;
    const float  minimumTemperature = -35;                                               // [Celsius]
    const size_t numberOfBits       = 11;                                                // Related to the ADC on the FMC
    const float  VrefADC            = 2.048;                                             // FMC's ADC refence voltage [V]
    const float  ADC_LSB            = VrefADC / (RD53Shared::setBits(numberOfBits) + 1); // [V/ADC]

    // #####################
    // # Calculate voltage #
    // #####################
    float voltage = (sensor1 - sensor2) * ADC_LSB;
    if((voltage > ((RD53Shared::setBits(numberOfBits) + 1.) * safetyMargin * ADC_LSB)) || (voltage >= Vdivider))
    {
        LOG(WARNING) << BOLDRED << "\t\t--> Thermistor measurement in saturation: either very cold or floating (voltage = " << BOLDYELLOW << std::setprecision(3) << voltage << std::setprecision(-1)
                     << BOLDRED << ")" << RESET;
        return minimumTemperature;
    }

    // ###############################################
    // # Calculate temperature with NTC Beta formula #
    // ###############################################
    float resistance  = voltage * Rdivider / (Vdivider - voltage);              // [kOhm]
    float temperature = 1. / (1. / T25C + log(resistance / R25C) / beta) - T0C; // [Celsius]

    return temperature;
}

float RD53FWInterface::calcVoltage(uint32_t senseVDD, uint32_t senseGND)
{
    // ##################################
    // # Voltage divider circuit on FMC #
    // ##################################
    const float R1divider      = 196;  // [kOhm]
    const float R2divider      = 39.2; // [kOhm]
    const float VdividerFactor = (R1divider + R2divider) / R2divider;

    // ###################
    // # Voltage per LSB #
    // ###################
    const size_t numberOfBits = 11;                                                 // Related to the ADC on the FMC
    const float  VrefADC      = 2.048;                                              // FMC's ADC refence voltage [V]
    const float  ADC_LSB      = VrefADC / (RD53Shared::setBits(numberOfBits) + 1.); // [V/ADC]

    // #####################
    // # Calculate voltage #
    // #####################
    float voltage = (senseVDD - senseGND) * ADC_LSB * VdividerFactor;
    if(voltage < ADC_LSB * VdividerFactor)
        LOG(WARNING) << BOLDRED << "\t\t--> Very low voltage: either floating VDD sense-line or FMC not powered (voltage = " << BOLDYELLOW << std::setprecision(3) << voltage << std::setprecision(-1)
                     << BOLDRED << ")" << RESET;
    else if(voltage > VrefADC * VdividerFactor)
        LOG(WARNING) << BOLDRED << "\t\t--> Measured voltage below reference: senseVDD = " << BOLDYELLOW << senseVDD << BOLDRED << "; senseGND = " << BOLDYELLOW << senseGND << RESET;

    return voltage;
}

// ##############################
// # Pseudo Random Bit Sequence #
// ##############################
bool RD53FWInterface::RunPRBStest(bool given_time, unsigned long long frames_or_time, uint16_t hybrid_id, uint16_t chip_id)
{
    const int          fps        = 3.5E7;
    const int          n_prints   = 10; // Only an indication, the real number of printouts will be driven by the length of the time steps
    unsigned long long frames2run = 0;
    unsigned           time2run   = 0;

    if(given_time == true)
    {
        time2run   = frames_or_time;
        frames2run = (unsigned long long)time2run * fps;
        LOG(INFO) << GREEN << "Running " << BOLDYELLOW << time2run << RESET << GREEN << "s will send about " << BOLDYELLOW << frames2run << RESET << GREEN << " frames" << RESET;
    }
    else
    {
        frames2run = frames_or_time;
        time2run   = (unsigned)frames2run / fps;
        LOG(INFO) << GREEN << "Running " << BOLDYELLOW << frames2run << RESET << GREEN << " frames will take about " << BOLDYELLOW << time2run << RESET << GREEN << "s" << RESET;
    }

    // Configure number of printouts and calculate the frequency of printouts
    unsigned time_per_step =
        std::min(std::max((unsigned)time2run / n_prints, (unsigned)1), (unsigned)3600); // The runtime of the PRBS test will have a precision of one step (at most 1h and at least 1s)

    WriteStackReg({{"user.ctrl_regs.PRBS_checker.module_addr", hybrid_id},
                   {"user.ctrl_regs.PRBS_checker.chip_address", chip_id},
                   {"user.ctrl_regs.PRBS_checker.reset_cntr", 1},
                   {"user.ctrl_regs.PRBS_checker.reset_cntr", 0}});

    // Set PRBS frames to run
    uint32_t lowFrames, highFrames;
    std::tie(highFrames, lowFrames) = bits::unpack<32, 32>(frames2run);
    WriteStackReg({{"user.ctrl_regs.prbs_frames_to_run_low", lowFrames},
                   {"user.ctrl_regs.prbs_frames_to_run_high", highFrames},
                   {"user.ctrl_regs.PRBS_checker.load_config", 1},
                   {"user.ctrl_regs.PRBS_checker.load_config", 0}});

    // Start PRBS
    WriteStackReg({{"user.ctrl_regs.PRBS_checker.start_checker", 1}, {"user.ctrl_regs.PRBS_checker.start_checker", 0}});

    bool run_done = false;
    int  idx      = 0;
    LOG(INFO) << BOLDGREEN << "===== PRBS run starting =====" << RESET;
    while(run_done == false)
    {
        // Sleep for a given time until the next printout
        sleep(time_per_step);

        // Read frame counters to check progress
        uint32_t cntr_lo       = ReadReg("user.stat_regs.prbs_frame_cntr_low");
        uint32_t cntr_hi       = ReadReg("user.stat_regs.prbs_frame_cntr_high");
        auto     current_frame = bits::pack<32, 32>(cntr_hi, cntr_lo);

        // Print progress and intermediate BER information
        float percent_done = (float)current_frame / frames2run * 100;
        LOG(INFO) << GREEN << "I've been running for " << BOLDYELLOW << (unsigned)time_per_step * (idx + 1) << RESET << GREEN << "s (" << BOLDYELLOW << std::setprecision(0) << percent_done << RESET
                  << GREEN << "% done)" << RESET;
        LOG(INFO) << GREEN << "Current BER counter: " << BOLDYELLOW << ReadReg("user.stat_regs.prbs_ber_cntr") << RESET;
        if(given_time == true)
            run_done = ((unsigned)time_per_step * (idx + 1) >= time2run);
        else
            run_done = (current_frame >= frames2run);
        idx++;
    }
    LOG(INFO) << BOLDGREEN << "===== Run finished =====" << RESET;

    WriteStackReg({{"user.ctrl_regs.PRBS_checker.stop_checker", 1}, {"user.ctrl_regs.PRBS_checker.stop_checker", 0}});

    // Read PRBS frame counter
    uint32_t PRBScntrLO   = ReadReg("user.stat_regs.prbs_frame_cntr_low");
    uint32_t PRBScntrHI   = ReadReg("user.stat_regs.prbs_frame_cntr_high");
    auto     frameCounter = bits::pack<32, 32>(PRBScntrHI, PRBScntrLO);
    LOG(INFO) << BOLDGREEN << "===== PRBS test summary =====" << RESET;
    LOG(INFO) << GREEN << "Final number of PRBS frames sent: " << BOLDYELLOW << frameCounter << RESET;

    // Read PRBS BER counter
    LOG(INFO) << GREEN << "Final BER counter: " << BOLDYELLOW << ReadReg("user.stat_regs.prbs_ber_cntr") << RESET;
    LOG(INFO) << BOLDGREEN << "===== End of summary =====" << RESET;

    return !ReadReg("user.stat_regs.prbs_ber_cntr");
}

} // namespace Ph2_HwInterface
