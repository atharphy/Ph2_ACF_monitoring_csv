/*!
  \file                  RD53FWInterface.h
  \brief                 RD53FWInterface to initialize and configure the FW
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53FWInterface.h"
#include "RD53Interface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
RD53FWInterface::RD53FWInterface(const char* pId, const char* pUri, const char* pAddressTable) : BeBoardFWInterface(pId, pUri, pAddressTable), fpgaConfig(nullptr), ddr3Offset(0), FWinfo(0) {}

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

void RD53FWInterface::ResetSequence(const std::string& refClockRate)
{
    LOG(INFO) << BOLDMAGENTA << "Resetting the backend board... it may take a while" << RESET;

    RD53FWInterface::TurnOffFMC();
    RD53FWInterface::TurnOnFMC();
    RD53FWInterface::ResetBoard();

    // ##############################
    // # Initialize clock generator #
    // ##############################
    RD53FWInterface::InitializeClockGenerator(refClockRate);

    // ###################################
    // # Reset optical link slow control #
    // ###################################
    RD53FWInterface::ResetOptoLinkSlowControl();

    // ######################
    // # Reset optical link #
    // ######################
    RD53FWInterface::ResetOptoLink();

    LOG(INFO) << BOLDMAGENTA << "Now you can start using the DAQ ... enjoy!" << RESET;
}

void RD53FWInterface::ConfigureBoard(const BeBoard* pBoard)
{
    // ########################
    // # Print firmware infos #
    // ########################
    uint32_t cVersionMajor = RegManager::ReadReg("user.stat_regs.usr_ver.usr_ver_major");
    uint32_t cVersionMinor = RegManager::ReadReg("user.stat_regs.usr_ver.usr_ver_minor");
    this->FWinfo           = ((cVersionMajor << RD53FWconstants::NBIT_FWVER) | cVersionMinor);

    uint32_t cFWyear    = RegManager::ReadReg("user.stat_regs.fw_date.year");
    uint32_t cFWmonth   = RegManager::ReadReg("user.stat_regs.fw_date.month");
    uint32_t cFWday     = RegManager::ReadReg("user.stat_regs.fw_date.day");
    uint32_t cFWhour    = RegManager::ReadReg("user.stat_regs.fw_date.hour");
    uint32_t cFWminute  = RegManager::ReadReg("user.stat_regs.fw_date.minute");
    uint32_t cFWseconds = RegManager::ReadReg("user.stat_regs.fw_date.seconds");

    uint32_t cLinkType   = RegManager::ReadReg("user.stat_regs.global_reg.link_type");
    uint32_t cOptSpeed   = RegManager::ReadReg("user.stat_regs.global_reg.optical_speed");
    uint32_t cFEtype     = RegManager::ReadReg("user.stat_regs.global_reg.front_end_type");
    uint32_t cL12FMCtype = RegManager::ReadReg("user.stat_regs.global_reg.fmc_l12_type");
    uint32_t cL08FMCtype = RegManager::ReadReg("user.stat_regs.global_reg.fmc_l8_type");

    LOG(INFO) << BOLDBLUE << "\t--> FW version : " << BOLDYELLOW << cVersionMajor << "." << cVersionMinor << BOLDBLUE << " -- Date (yy/mm/dd) : " << BOLDYELLOW << cFWyear << "/" << cFWmonth << "/"
              << cFWday << BOLDBLUE << " -- Time (hour:minute:sec) : " << BOLDYELLOW << cFWhour << ":" << cFWminute << ":" << cFWseconds << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Link type : " << BOLDYELLOW << (cLinkType == 0 ? "electrical" : "optical") << BOLDBLUE << " -- Optical speed : " << BOLDYELLOW
              << (cOptSpeed == 0 ? "10 Gbit/s" : "5 Gbit/s") << BOLDBLUE << " -- Frontend type : " << BOLDYELLOW << (cFEtype == 1 ? "RD53A" : "RD53B") << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> L12 FMC type : " << BOLDYELLOW << cL12FMCtype << BOLDBLUE << " -- L08 FMC type : " << BOLDYELLOW << cL08FMCtype << BOLDBLUE
              << " (1=KSU, 2=CERN, 3=DIO5, 4=OPTO, 5=FERMI, 7=NONE, 0=Unspecified)" << RESET;

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
            LOG(INFO) << BOLDBLUE << "\t--> " << it.first << ": 0x" << BOLDYELLOW << std::hex << std::uppercase << it.second << std::dec << " (" << it.second << ")" << RESET;
            if(it.first.find("HitOr_enable_l12") != std::string::npos)
                RD53FWInterface::localCfgFastCmd.enable_hitor = it.second;
            else if(it.first.find("ext_clk_en") != std::string::npos)
            {
                cfgDIO5.enable     = it.second;
                cfgDIO5.ch_out_en  = 0x0;
                cfgDIO5.ext_clk_en = it.second;
            }
            else
            {
                RD53FWInterface::localCfgFastCmd.trigger_source = static_cast<RD53FWInterface::TriggerSource>(it.second);
                if(static_cast<RD53FWInterface::TriggerSource>(it.second) == TriggerSource::External)
                {
                    LOG(INFO) << BOLDBLUE << "\t--> Trigger source was selected to be External" << RESET;
                    cfgDIO5.enable    = true;
                    cfgDIO5.ch_out_en = 0x0;
                }
                else if(static_cast<RD53FWInterface::TriggerSource>(it.second) == TriggerSource::TLU)
                {
                    LOG(INFO) << BOLDBLUE << "\t--> Trigger source was selected to be TLU" << RESET;
                    cfgDIO5.enable             = true;
                    cfgDIO5.ch_out_en          = 0x05;
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
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    // ################################
    // # Enabling hybrids and chips   #
    // # Hybrid_type hard coded in FW #
    // # 1 = single chip              #
    // # 2 = double chip hybrid       #
    // # 4 = quad chip hybrid         #
    // ################################
    this->singleChip     = RegManager::ReadReg("user.stat_regs.aurora_rx.Module_type") == 1;
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

    // ########################
    // # Read clock generator #
    // ########################
    RD53FWInterface::ReadClockGenerator();

    // #########################################
    // # Read optical link slow control status #
    // #########################################
    uint32_t txIsReady, rxIsReady;
    RD53FWInterface::StatusOptoLinkSlowControl(txIsReady, rxIsReady);

    // ###########################
    // # Print clock measurement #
    // ###########################
    uint32_t inputClk = RegManager::ReadReg("user.stat_regs.clkin_rate");
    uint32_t gtxClk   = RegManager::ReadReg("user.stat_regs.gtx_refclk_rate");
    LOG(INFO) << GREEN << std::fixed << std::setprecision(3) << "Input clock frequency (could be either internal or external, should be ~40 MHz): " << BOLDYELLOW << inputClk / 1000. << " MHz"
              << std::setprecision(-1) << RESET;
    LOG(INFO) << GREEN << std::fixed << std::setprecision(3) << "GTX receiver clock frequency (~160 MHz (~320 MHz) for electrical (optical) readout): " << BOLDYELLOW << gtxClk / 1000. << " MHz"
              << std::setprecision(-1) << RESET;

    // @TMP@
    RegManager::WriteReg("user.ctrl_regs.ctrl_cdr.cdr_addr", 0);
    uint32_t extCMDclk = RegManager::ReadReg("user.stat_regs.cdr_freq_mon");
    RegManager::WriteReg("user.ctrl_regs.ctrl_cdr.cdr_addr", 1);
    uint32_t extSERclk = RegManager::ReadReg("user.stat_regs.cdr_freq_mon");
    LOG(INFO) << GREEN << std::fixed << std::setprecision(3) << "External CMD clock frequency: " << BOLDYELLOW << extCMDclk / 1000. << " MHz" << std::setprecision(-1) << RESET;
    LOG(INFO) << GREEN << std::fixed << std::setprecision(3) << "External Serializer clock frequency: " << BOLDYELLOW << extSERclk / 1000. << " MHz" << std::setprecision(-1) << RESET;
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
            LOG(INFO) << BOLDBLUE << "\t--> " << it.first << ": 0x" << BOLDYELLOW << std::hex << std::uppercase << it.second << std::dec << " (" << it.second << ")" << RESET;
            cVecReg.push_back({it.first, it.second});
        }

    if(cVecReg.size() != 0)
    {
        RegManager::WriteStackReg(cVecReg);
        RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.load_config");
        RD53FWInterface::SendBoardCommand("user.ctrl_regs.ext_tlu_reg2.dio5_load_config");
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

    // ##########
    // # Header #
    // ##########
    commandList.emplace_back(bits::pack<6, 10, 16>(RD53FWconstants::HEADEAR_WRTCMD, (hybridId < 0 ? enabledHybrids : 1 << hybridId), n32bitWords));

    // ############
    // # Commands #
    // ############
    for(auto i = 1u; i < data.size(); i += 2) commandList.emplace_back(bits::pack<16, 16>(data[i - 1], data[i]));

    // If data.size() is not even, add a sync command
    if(data.size() % 2 != 0) commandList.emplace_back(bits::pack<16, 16>(data.back(), RD53CmdEncoder::SYNC));
}

void RD53FWInterface::SendChipCommandsPack(const std::vector<uint32_t>& commandList)
{
    int nAttempts = 0;

    // ############################
    // # Check write-command FIFO #
    // ############################
    while(((RegManager::ReadReg("user.stat_regs.slow_cmd.error_flag") || !RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_empty") || RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_full")) ==
           true) &&
          (nAttempts < RD53Shared::MAXATTEMPTS))
    {
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.error_flag") == true) LOG(ERROR) << BOLDRED << "Write-command FIFO error" << RESET;
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_empty") == false) LOG(ERROR) << BOLDRED << "Write-command FIFO not empty" << RESET;
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_full") == true) LOG(ERROR) << BOLDRED << "Write-command FIFO full" << RESET;

        nAttempts++;
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    }
    if(nAttempts == RD53Shared::MAXATTEMPTS)
        LOG(ERROR) << BOLDRED << "Error in the write-command FIFO, reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ")" << RESET;

    // ###############################
    // # Send command(s) to the chip #
    // ###############################
    RegManager::WriteBlockReg("user.ctrl_regs.Slow_cmd_fifo_din", commandList);
    RegManager::WriteStackReg({{"user.ctrl_regs.Slow_cmd.dispatch_packet", 1}, {"user.ctrl_regs.Slow_cmd.dispatch_packet", 0}});

    // ####################################
    // # Check if commands were dispached #
    // ####################################
    nAttempts = 0;
    while((RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_packet_dispatched") == false) && (nAttempts < RD53Shared::MAXATTEMPTS))
    {
        nAttempts++;
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    }
    if(nAttempts == RD53Shared::MAXATTEMPTS)
        LOG(ERROR) << BOLDRED << "Error while dispatching chip register program, reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ")" << RESET;

    // ############################
    // # Check write-command FIFO #
    // ############################
    while(((RegManager::ReadReg("user.stat_regs.slow_cmd.error_flag") || !RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_empty") || RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_full")) ==
           true) &&
          (nAttempts < RD53Shared::MAXATTEMPTS))
    {
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.error_flag") == true) LOG(ERROR) << BOLDRED << "Write-command FIFO error" << RESET;
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_empty") == false) LOG(ERROR) << BOLDRED << "Write-command FIFO not empty" << RESET;
        if(RegManager::ReadReg("user.stat_regs.slow_cmd.fifo_full") == true) LOG(ERROR) << BOLDRED << "Write-command FIFO full" << RESET;

        nAttempts++;
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    }
    if(nAttempts == RD53Shared::MAXATTEMPTS)
        LOG(ERROR) << BOLDRED << "Error in the write-command FIFO, reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ")" << RESET;
}

std::vector<std::pair<uint16_t, uint16_t>> RD53FWInterface::ReadChipRegisters(ReadoutChip* pChip)
{
    std::vector<std::pair<uint16_t, uint16_t>> regReadback;

    // #################################
    // # Compose chip-lane in readback #
    // #################################
    uint32_t chipLane = pChip->getHybridId();
    if(this->singleChip != true) chipLane = RD53FWconstants::NLANE_HYBRID * chipLane + static_cast<RD53*>(pChip)->getChipLane();

    // #####################
    // # Read the register #
    // #####################
    if(RegManager::ReadReg("user.stat_regs.Register_Rdback.fifo_full") == true) LOG(ERROR) << BOLDRED << "Read-command FIFO full" << RESET;

    while(RegManager::ReadReg("user.stat_regs.Register_Rdback.fifo_empty") == false)
    {
        uint32_t readBackData = RegManager::ReadReg("user.stat_regs.Register_Rdback_fifo");

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
    if(RegManager::ReadReg("user.stat_regs.global_reg.clk_gen_lock") == 1)
        LOG(INFO) << BOLDBLUE << "\t--> Clock generator is " << BOLDYELLOW << "locked" << RESET;
    else
        LOG(ERROR) << BOLDRED << "\t--> Clock generator is not locked" << RESET;

    // ############################
    // # Check I2C initialization #
    // ############################
    if(RegManager::ReadReg("user.stat_regs.global_reg.i2c_init") == 1)
        LOG(INFO) << BOLDBLUE << "\t--> I2C " << BOLDYELLOW << "initialized" << RESET;
    else
    {
        LOG(ERROR) << BOLDRED << "I2C not initialized" << RESET;
        uint32_t status = RegManager::ReadReg("user.stat_regs.global_reg.i2c_init_err");
        LOG(ERROR) << BOLDRED << "\t--> I2C initialization status: " << BOLDYELLOW << status << RESET;
    }

    if(RegManager::ReadReg("user.stat_regs.global_reg.i2c_acq_err") == 1) LOG(INFO) << GREEN << "I2C ack error during analog readout (for KSU FMC only)" << RESET;

    // ############################################################
    // # Check status registers associated wih fast command block #
    // ############################################################
    uint32_t fastCMDReg = RegManager::ReadReg("user.stat_regs.fast_cmd.trigger_source_o");
    LOG(INFO) << GREEN << "Fast command block trigger source: " << BOLDYELLOW << fastCMDReg << RESET << GREEN << " (1=IPBus, 2=Test-FSM, 3=TTC, 4=TLU, 5=External, 6=Hit-Or, 7=User-defined frequency)"
              << RESET;

    fastCMDReg = RegManager::ReadReg("user.stat_regs.fast_cmd.trigger_state");
    LOG(INFO) << GREEN << "Fast command block trigger state: " << BOLDYELLOW << fastCMDReg << RESET << GREEN << " (0=Idle, 2=Running)" << RESET;

    fastCMDReg = RegManager::ReadReg("user.stat_regs.fast_cmd.if_configured");
    LOG(INFO) << GREEN << "Fast command block check if configuraiton registers have been set: " << BOLDYELLOW << (fastCMDReg == true ? "configured" : "not configured") << RESET;

    fastCMDReg = RegManager::ReadReg("user.stat_regs.fast_cmd.error_code");
    LOG(INFO) << GREEN << "Fast command block error code (0=No error): " << BOLDYELLOW << fastCMDReg << RESET;

    // ###########################
    // # Check trigger registers #
    // ###########################
    uint32_t trigReg = RegManager::ReadReg("user.stat_regs.trigger_cntr");
    LOG(INFO) << GREEN << "Trigger counter: " << BOLDYELLOW << trigReg << RESET;

    // ##########################
    // # Check hybrid registers #
    // ##########################
    uint32_t hybrid = RegManager::ReadReg("user.stat_regs.aurora_rx.Module_type");
    LOG(INFO) << GREEN << "Hybrid type: " << BOLDYELLOW << hybrid << RESET << GREEN " (1=Single chip, 2=Double chip, 4=Quad chip)" << RESET;

    hybrid = RegManager::ReadReg("user.stat_regs.aurora_rx.Nb_of_modules");
    LOG(INFO) << GREEN << "Number of hybrids which can be potentially readout: " << BOLDYELLOW << hybrid << RESET;
}

bool RD53FWInterface::CheckChipCommunication(const BeBoard* pBoard)
{
    uint32_t chips_en;
    uint32_t channel_up;

    LOG(INFO) << GREEN << "Checking status communication RD53 --> FW" << RESET;

    // ###############################
    // # Check RD53 AURORA registers #
    // ###############################
    uint32_t auroraSpeed = RD53FWInterface::ReadoutSpeed();
    LOG(INFO) << BOLDBLUE << "\t--> Aurora speed: " << BOLDYELLOW << (auroraSpeed == 0 ? "1.28 Gbit/s" : "640 Mbit/s") << RESET;

    // ########################################
    // # Check communication with the chip(s) #
    // ########################################
    chips_en = RegManager::ReadReg("user.ctrl_regs.Chips_en");
    LOG(INFO) << BOLDBLUE << "\t--> Total number of required data lanes: " << BOLDYELLOW << RD53Shared::countBitsOne(chips_en) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<20>(chips_en)
              << RESET;

    channel_up = RegManager::ReadReg("user.stat_regs.aurora_rx_channel_up");
    LOG(INFO) << BOLDBLUE << "\t--> Total number of active data lanes:   " << BOLDYELLOW << RD53Shared::countBitsOne(channel_up) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<20>(channel_up)
              << RESET;

    if(chips_en & ~channel_up)
    {
        LOG(ERROR) << BOLDRED << "\t--> Some data lanes are enabled but inactive" << RESET;
        RD53FWInterface::InitHybridByHybrid(pBoard);
        return false;
    }
    else if(chips_en == 0)
        throw Exception("[RD53FWInterface::CheckChipCommunication] No data lane is enabled: aborting");
    else
        LOG(INFO) << BOLDBLUE << "\t--> All enabled data lanes are active" << RESET;

    return true;
}

uint32_t RD53FWInterface::ReadoutSpeed()
// ####################
// # 0  = 1.28 Gbit/s #
// # !0 = 640 Mbit/s  #
// ####################
{
    return RegManager::ReadReg("user.stat_regs.aurora_rx.speed");
}

void RD53FWInterface::InitHybridByHybrid(const BeBoard* pBoard)
{
    const unsigned int MAXSEQUENCES = 5; // @CONST@

    for(const auto cOpticalGroup: *pBoard)
        for(const auto cHybrid: *cOpticalGroup)
        {
            // #################################
            // # Check if all lanes are active #
            // #################################
            const uint32_t hybrid_id         = cHybrid->getId();
            const uint32_t chips_en_to_check = RD53FWInterface::GetHybridEnabledChips(cHybrid);

            // ################################
            // # Try different init sequences #
            // ################################
            bool lanes_up;
            for(unsigned int seq = 0; seq < MAXSEQUENCES; seq++)
            {
                LOG(INFO) << GREEN << "Trying initialization sequence number: " << BOLDYELLOW << seq << RESET;
                LOG(INFO) << BOLDBLUE << "\t--> Number of required data lanes for [board/opticalGroup/hybrid = " << BOLDYELLOW << pBoard->getId() << "/" << cOpticalGroup->getId() << "/" << hybrid_id
                          << BOLDBLUE << "]: " << BOLDYELLOW << RD53Shared::countBitsOne(chips_en_to_check) << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<20>(chips_en_to_check) << RESET;

                std::vector<uint16_t> initSequence = RD53FWInterface::GetInitSequence(this->singleChip == true ? 4 : seq);

                for(unsigned int i = 0; i < RD53Shared::MAXATTEMPTS; i++)
                {
                    RD53FWInterface::WriteChipCommand(initSequence, hybrid_id);
                    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

                    // #################################
                    // # Check if all lanes are active #
                    // #################################
                    lanes_up            = false;
                    uint32_t channel_up = RegManager::ReadReg("user.stat_regs.aurora_rx_channel_up");

                    LOG(INFO) << BOLDBLUE << "\t--> Total number of active data lanes for tentative n. " << BOLDYELLOW << i << BOLDBLUE << ": " << BOLDYELLOW << RD53Shared::countBitsOne(channel_up)
                              << BOLDBLUE << " i.e. " << BOLDYELLOW << std::bitset<20>(channel_up) << RESET;

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
        chips_en |= hyb_chips_en << (RD53FWconstants::NLANE_HYBRID * hybrid_id);
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

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
}

void RD53FWInterface::ResetBoard()
{
    // #######
    // # Set #
    // #######
    RegManager::WriteReg("user.ctrl_regs.reset_reg.aurora_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.aurora_pma_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.global_rst", 1);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.clk_gen_rst", 1);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.fmc_pll_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.cmd_rst", 1);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.i2c_rst", 1);

    // #########
    // # Reset #
    // #########
    RegManager::WriteReg("user.ctrl_regs.reset_reg.global_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.clk_gen_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.fmc_pll_rst", 1);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.cmd_rst", 0);

    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    RegManager::WriteReg("user.ctrl_regs.reset_reg.i2c_rst", 0);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.aurora_pma_rst", 1);
    RegManager::WriteReg("user.ctrl_regs.reset_reg.aurora_rst", 1);

    // ########
    // # DDR3 #
    // ########
    LOG(INFO) << YELLOW << "Waiting for DDR3 calibration..." << RESET;
    while(RegManager::ReadReg("user.stat_regs.readout1.ddr3_initial_calibration_done") == false) std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    LOG(INFO) << BOLDBLUE << "\t--> DDR3 calibration done" << RESET;
}

void RD53FWInterface::ResetFastCmdBlk()
{
    RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.ipb_reset");

    RegManager::WriteReg("user.ctrl_regs.fast_cmd_reg_1.ipb_fast_duration", RD53FWconstants::IPBUS_FASTDURATION);
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

uint32_t RD53FWInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    uint32_t nWordsInMemoryOld, nWordsInMemory = 0;

    // #############################################
    // # Wait for a stable number of words to read #
    // #############################################
    nWordsInMemory = RegManager::ReadReg("user.stat_regs.words_to_read");
    do
    {
        nWordsInMemoryOld = nWordsInMemory;
        std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
    } while(((nWordsInMemory = RegManager::ReadReg("user.stat_regs.words_to_read")) != nWordsInMemoryOld) && (pWait == true));
    // auto nTriggersReceived = RegManager::ReadReg("user.stat_regs.trigger_cntr");

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

    RD53FWInterface::WriteArbitraryRegister("user.ctrl_regs.fast_cmd_reg_3.triggers_to_accept", RD53FWInterface::localCfgFastCmd.n_triggers = pNEvents);

    do
    {
        nAttempts++;
        retry = false;
        pData.clear();

        // ####################
        // # Readout sequence #
        // ####################
        RD53FWInterface::Start();
        while(RegManager::ReadReg("user.stat_regs.trigger_cntr") < pNEvents * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration))
            std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::READOUTSLEEP));
        RD53FWInterface::ReadData(pBoard, false, pData, pWait);
        RD53FWInterface::Stop();

        // ##################
        // # Error checking #
        // ##################
        RD53Event::decodedEvents.clear();
        uint16_t status;
        RD53Event::DecodeEventsMultiThreads(pData, RD53Event::decodedEvents, status); // Decode events with multiple threads
        // RD53Event::DecodeEvents(pData, RD53Event::decodedEvents, {}, status);         // Decode events with a single thread
        // RD53Event::PrintEvents(RD53Event::decodedEvents, pData);                      // @TMP@
        if(RD53Event::EvtErrorHandler(status) == false)
        {
            retry = true;
            continue;
        }

        if(RD53Event::decodedEvents.size() != RD53FWInterface::localCfgFastCmd.n_triggers * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration))
        {
            LOG(ERROR) << BOLDRED << "Sent " << RD53FWInterface::localCfgFastCmd.n_triggers * (1 + RD53FWInterface::localCfgFastCmd.trigger_duration) << " triggers, but collected "
                       << RD53Event::decodedEvents.size() << " events" << BOLDYELLOW << " --> retry" << RESET;
            retry = true;
            continue;
        }

    } while((retry == true) && (nAttempts < RD53Shared::MAXATTEMPTS));

    if(retry == true)
    {
        LOG(ERROR) << BOLDRED << "Reached maximum number of attempts (" << BOLDYELLOW << +RD53Shared::MAXATTEMPTS << BOLDRED << ") without success" << RESET;
        pData.clear();
    }

    // #################
    // # Show progress #
    // #################
    RD53RunProgress::update(pData.size(), true);
}

void RD53FWInterface::SendBoardCommand(const std::string& cmd_reg)
{
    RegManager::WriteStackReg({{cmd_reg, 1}, {"user.ctrl_regs.fast_cmd_reg_1.cmd_strobe", 1}, {"user.ctrl_regs.fast_cmd_reg_1.cmd_strobe", 0}, {cmd_reg, 0}});
}

void RD53FWInterface::ConfigureFastCommands(const FastCommandsConfig* cfg)
{
    if(cfg == nullptr) cfg = &(RD53FWInterface::localCfgFastCmd);

    // @TMP@
    if(cfg->autozero_source == AutozeroSource::FastCMDFSM)
    {
        RD53FWInterface::WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, RD53Constants::GLOBAL_PULSE_ADDR, 1 << 15).getFrames(),
                                          -1); // Prepare GLOBAL_PULSE_RT to reset autozero level in SYNC FE
        RD53FWInterface::WriteChipCommand(RD53Cmd::GlobalPulse(RD53Constants::BROADCAST_CHIPID, 0x0004).getFrames(), -1);
        RD53FWInterface::WriteChipCommand(RD53Cmd::WrReg(RD53Constants::BROADCAST_CHIPID, RD53Constants::GLOBAL_PULSE_ADDR, 1 << 14).getFrames(),
                                          -1); // Prepare GLOBAL_PULSE_RT to acquire zero level in SYNC FE
    }

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
                               {"user.ctrl_regs.fast_cmd_reg_7.glb_pulse_data", (uint32_t)bits::pack<4, 1, 4, 1>(RD53Constants::BROADCAST_CHIPID, 0, 0x0008, 0)}});

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
// ##################################################################################
// # Finite state machine                                                           #
// ##################################################################################
// # Idle --> Init_Prime --> Auto_Zero --> ECR --> Inject --> Trigger --> Prime --| #
// #  ^                          ^                                                | #
// #  |                          |------------------------------------------------| #
// #  |                                                                           | #
// #  |---------------------------------------------------------------------------| #
// ##################################################################################
{
    const double  mainClock = 40e6; // @CONST@
    const uint8_t chipId    = RD53Constants::BROADCAST_CHIPID;
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

    // #############################
    // # Configuring FastCmd block #
    // #############################
    RD53FWInterface::localCfgFastCmd.n_triggers       = 0;
    RD53FWInterface::localCfgFastCmd.trigger_duration = nTRIGxEvent - 1;

    if(injType == INJtype::Digital)
    {
        // #######################################
        // # Configuration for digital injection #
        // #######################################
        RD53::CalCmd calcmd_first(1, 2, 10, 0, 0);
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.first_cal_data = calcmd_first.getCalCmd(chipId);
        RD53::CalCmd calcmd_second(0, 0, 2, 0, 0);
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
    }
    else
        LOG(ERROR) << BOLDRED << "Option not recognized " << BOLDYELLOW << injType << RESET;

    // @TMP@
    if(enableAutozero == true)
    {
        RD53FWInterface::localCfgFastCmd.autozero_source                   = AutozeroSource::FastCMDFSM;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.ecr_en               = true;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr      = 512;
        RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_autozero = 128;
    }

    LOG(INFO) << GREEN << "Internal trigger frequency (if enabled): " << BOLDYELLOW << std::fixed << std::setprecision(0)
              << mainClock / ((RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_ecr + 1) * 4 - 1 + (RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_inject + 1) * 4 + 7 +
                              (RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_trigger + 1) * 4 - 1 + (RD53FWInterface::localCfgFastCmd.fast_cmd_fsm.delay_after_prime + 1) * 4 + 7 +
                              RD53FWInterface::localCfgFastCmd.trigger_duration)
              << std::setprecision(-1) << " Hz" << RESET; // @TMP@
    RD53Shared::resetDefaultFloat();

    // ##############################
    // # Download the configuration #
    // ##############################
    RD53FWInterface::ConfigureFastCommands();
    RD53FWInterface::PrintFWstatus();
}

void RD53FWInterface::ConfigureDIO5(const DIO5Config* cfg)
{
    const uint8_t fiftyOhmEnable = 0x12; // @CONST@

    if(RegManager::ReadReg("user.stat_regs.fast_cmd.dio5_not_ready") == true) LOG(ERROR) << BOLDRED << "DIO5 not ready" << RESET;

    if(RegManager::ReadReg("user.stat_regs.fast_cmd.dio5_error") == true) LOG(ERROR) << BOLDRED << "DIO5 is in error" << RESET;

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
                               {"user.ctrl_regs.ext_tlu_reg2.ext_clk_en", (uint32_t)cfg->ext_clk_en}});

    RD53FWInterface::SendBoardCommand("user.ctrl_regs.ext_tlu_reg2.dio5_load_config");
}

// ###################################
// # Read/Write Status Optical Group #
// ###################################

void RD53FWInterface::ResetOptoLinkSlowControl()
{
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_tx_reset", 0x1}, {"user.ctrl_regs.lpgbt_1.ic_rx_reset", 0x1}});
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_tx_reset", 0x0}, {"user.ctrl_regs.lpgbt_1.ic_rx_reset", 0x0}});
}

void RD53FWInterface::StatusOptoLinkSlowControl(uint32_t& txIsReady, uint32_t& rxIsReady)
{
    txIsReady = RegManager::ReadReg("user.stat_regs.lpgbt_sc_1.tx_ready");
    rxIsReady = RegManager::ReadReg("user.stat_regs.lpgbt_sc_1.rx_empty");

    if(txIsReady == true)
        LOG(INFO) << GREEN << "Optical link tx slow control status: " << BOLDYELLOW << "ready" << RESET;
    else
        LOG(WARNING) << GREEN << "Optical link tx slow control status: " << BOLDRED << "not ready" << RESET;

    if(rxIsReady == true)
        LOG(INFO) << GREEN << "Optical link rx slow control status: " << BOLDYELLOW << "ready" << RESET;
    else
        LOG(WARNING) << GREEN << "Optical link rx slow control status: " << BOLDRED << "not ready" << RESET;
}

void RD53FWInterface::ResetOptoLink()
{
    RegManager::WriteReg("user.ctrl_regs.lpgbt_1.mgt_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    RegManager::WriteReg("user.ctrl_regs.lpgbt_1.mgt_reset", 0x0);
}

void RD53FWInterface::StatusOptoLink(uint32_t& txStatus, uint32_t& rxStatus, uint32_t& mgtStatus)
{
    txStatus  = RegManager::ReadReg("user.stat_regs.lpgbt_fpga.tx_ready");
    rxStatus  = RegManager::ReadReg("user.stat_regs.lpgbt_fpga.rx_ready");
    mgtStatus = RegManager::ReadReg("user.stat_regs.lpgbt_fpga.mgt_ready");

    LOG(INFO) << BOLDBLUE << "\t--> Optical link n. active LpGBT chip tx:  " << BOLDYELLOW << txStatus << BOLDBLUE << " i.e.: " << BOLDYELLOW << std::bitset<20>(txStatus) << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Optical link n. active LpGBT chip rx:  " << BOLDYELLOW << rxStatus << BOLDBLUE << " i.e.: " << BOLDYELLOW << std::bitset<20>(rxStatus) << RESET;
    LOG(INFO) << BOLDBLUE << "\t--> Optical link n. active LpGBT chip mgt: " << BOLDYELLOW << mgtStatus << BOLDBLUE << " i.e.: " << BOLDYELLOW << std::bitset<20>(mgtStatus) << RESET;
}

bool RD53FWInterface::WriteOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress, const uint32_t pData, const bool pVerifLoop)
{
    // OptoChip ID
    RD53FWInterface::selectLink(linkNumber);

    // Config
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_tx_fifo_din", pData}, {"user.ctrl_regs.lpgbt_1.ic_chip_addr_tx", LpGBTaddress}, {"user.ctrl_regs.lpgbt_2.ic_reg_addr_tx", pAddress}});

    // Perform operation
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_tx_fifo_wr_en", 0x1},
                               {"user.ctrl_regs.lpgbt_1.ic_tx_fifo_wr_en", 0x0},
                               {"user.ctrl_regs.lpgbt_1.ic_send_wr_cmd", 0x1},
                               {"user.ctrl_regs.lpgbt_1.ic_send_wr_cmd", 0x0}});

    if(pVerifLoop == true)
    {
        uint32_t cReadBack = RD53FWInterface::ReadOptoLinkRegister(linkNumber, LpGBTaddress, pAddress);
        if(cReadBack != pData)
        {
            LOG(ERROR) << BOLDRED << "[RD53FWInterface::WriteOpticalLinkRegiser] Register readback failure for register 0x" << BOLDYELLOW << std::hex << std::uppercase << pAddress << std::dec
                       << RESET;
            return false;
        }
    }

    return true;
}

uint32_t RD53FWInterface::ReadOptoLinkRegister(const uint32_t linkNumber, const uint16_t LpGBTaddress, const uint32_t pAddress)
{
    // OptoChip ID
    RD53FWInterface::selectLink(linkNumber);

    // Config
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_chip_addr_tx", LpGBTaddress}, {"user.ctrl_regs.lpgbt_2.ic_reg_addr_tx", pAddress}});

    // Perform operation
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_2.ic_nb_of_words_to_read", 0x1}, {"user.ctrl_regs.lpgbt_1.ic_send_rd_cmd", 0x1}, {"user.ctrl_regs.lpgbt_1.ic_send_rd_cmd", 0x0}});

    // Actual readback one word at a time
    RegManager::WriteStackReg({{"user.ctrl_regs.lpgbt_1.ic_rx_fifo_rd_en", 0x1}, {"user.ctrl_regs.lpgbt_1.ic_rx_fifo_rd_en", 0x0}});
    uint32_t cRead = RegManager::ReadReg("user.stat_regs.lpgbt_sc_1.rx_fifo_dout");

    return cRead;
}

void RD53FWInterface::PrintFrequencyLVDS()
{
    uint32_t LVDS = RegManager::ReadReg("user.stat_regs.gp_lvds_freq_mon");
    LOG(INFO) << GREEN << std::fixed << std::setprecision(3) << "LVDS frequency: " << BOLDYELLOW << LVDS / 1000. << " MHz" << std::setprecision(-1) << RESET;
}

void RD53FWInterface::PrintErrorsLVDS()
{
    uint32_t LVDSctrl  = RegManager::ReadReg("user.ctrl_regs.reg_gp_lvds.gp_lvds_ctrl");
    uint32_t LVDSerror = RegManager::ReadReg("user.stat_regs.global_reg.error_gp_lvds");
    if(LVDSctrl == 1)
    {
        if(LVDSerror == 0)
            LOG(INFO) << GREEN << "No errors in CMD data" << RESET;
        else
            LOG(WARNING) << RED << "Some errors CMD data: " << BOLDYELLOW << LVDSerror << RESET;
    }
    else if(LVDSctrl == 7)
    {
        if(LVDSerror == 0)
            LOG(INFO) << GREEN << "LVDS pattern GOOD" << RESET;
        else
            LOG(WARNING) << RED << "LVDS pattern WRONG" << RESET;
    }
}

void RD53FWInterface::selectLink(const uint8_t pLinkId, uint32_t pWait_ms) { RegManager::WriteReg("user.ctrl_regs.lpgbt_1.active_link", pLinkId); }

void RD53FWInterface::SelectBERcheckBitORFrame(const uint8_t bitORframe) { RegManager::WriteReg("user.ctrl_regs.PRBS_checker.error_cntr_sel", bitORframe); }

void RD53FWInterface::WriteArbitraryRegister(const std::string& regName, const uint32_t value, const BeBoard* pBoard, ReadoutChipInterface* pReadoutChipInterface, const bool doReset)
{
    RegManager::WriteReg(regName, value);
    RD53FWInterface::SendBoardCommand("user.ctrl_regs.fast_cmd_reg_1.load_config");

    if(doReset == true)
    {
        RD53FWInterface::ResetBoard();
        RD53FWInterface::ConfigureBoard(pBoard);
        static_cast<RD53Interface*>(pReadoutChipInterface)->InitRD53Downlink(pBoard);
    }
}

uint32_t RD53FWInterface::ReadArbitraryRegister(const std::string& regName) { return RegManager::ReadReg(regName); }

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
    if(fpgaConfig && fpgaConfig->getUploadingFpga() > 0) throw Exception("[RD53FWInterface::CheckIfUploading] This board is uploading an FPGA configuration");

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

void RD53FWInterface::InitializeClockGenerator(const std::string& refClockRate, bool doStoreInEEPROM)
// ############################
// # refClockRate = 160 [MHz] #
// # refClockRate = 320 [MHz] #
// ############################
{
    const uint32_t writeSPI(0x8FA38014);    // Write to SPI @CONST@
    const uint32_t writeEEPROM(0x8FA38014); // Write to EEPROM @CONST@
    uint32_t       SPIregSettings[] = {
        0xEB020320, // OUT0 --> This clock is not used, but it can be used as another GBT clock (160 MHz, LVDS, phase shift 0 deg)
        0xEB020321, // OUT1 --> GBT clock reference: 160 MHz, LVDS, phase shift 0 deg (0xEB820321: 320 MHz, LVDS, phase shift 0 deg)
        0xEB840302, // OUT2 --> DDR3 clock reference: 240 MHz, LVDS, phase shift 0 deg
        0xEB840303, // OUT3 --> Not used (240 MHz, LVDS, phase shift 0 deg)
        0xEB140334, // OUT4 --> Not used (40 MHz, LVDS, R4.1 = 1, ph4adjc = 0)
        0x10000E75, // Reference selection: 0x10000E75 primary reference, 0x10000EB5 secondary reference
        0x030E02E6, // VCO selection: 0xyyyyyyEy select VCO1 if CDCE reference is 40 MHz, 0xyyyyyyFy select VCO2 if CDCE reference is > 40 MHz
                    // VCO1, PS = 4, FD = 12, FB = 1, ChargePump 50 uA, Internal Filter, R6.20 = 0, AuxOut = enable, AuxOut = OUT2
        0xBD800DF7, // RC network parameters: C2 = 473.5 pF, R2 = 98.6 kOhm, C1 = 0 pF, C3 = 0 pF, R3 = 5 kOhm etc, SEL_DEL1 = 1, SEL_DEL2 = 1
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

    if(refClockRate == "160")
        SPIregSettings[1] = 0xEB020321;
    else if(refClockRate == "320")
        SPIregSettings[1] = 0xEB820321;
    else
        throw Exception("[RD53FWInterface::InitializeClockGenerator] CDCE reference clock rate not recognized");

    for(const auto value: SPIregSettings)
    {
        RegManager::WriteReg("system.spi.tx_data", value);
        RegManager::WriteReg("system.spi.command", writeSPI);

        RegManager::ReadReg("system.spi.rx_data"); // Dummy read
        RegManager::ReadReg("system.spi.rx_data"); // Dummy read
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
        RegManager::WriteReg("system.spi.tx_data", writeEEPROM);
        RegManager::WriteReg("system.spi.command", writeSPI);

        RegManager::ReadReg("system.spi.rx_data"); // Dummy read
        RegManager::ReadReg("system.spi.rx_data"); // Dummy read
    }
}

void RD53FWInterface::ReadClockGenerator()
{
    const uint32_t writeSPI(0x8FA38014);                                                       // Write to SPI @CONST@
    const uint32_t SPIreadCommands[] = {0x0E, 0x1E, 0x2E, 0x3E, 0x4E, 0x5E, 0x6E, 0x7E, 0x8E}; // @CONST@

    LOG(INFO) << GREEN << "Reading clock generator (CDCE62005) configuration" << RESET;
    for(const auto value: SPIreadCommands)
    {
        RegManager::WriteReg("system.spi.tx_data", value);
        RegManager::WriteReg("system.spi.command", writeSPI);

        RegManager::WriteReg("system.spi.tx_data", 0xAAAAAAAA); // Dummy write
        RegManager::WriteReg("system.spi.command", writeSPI);

        uint32_t          readback = RegManager::ReadReg("system.spi.rx_data");
        std::stringstream myString("");
        myString << std::right << std::setfill('0') << std::setw(8) << std::hex << std::uppercase << readback << std::dec;
        LOG(INFO) << BOLDBLUE << "\t--> SPI register content: 0x" << BOLDYELLOW << std::hex << std::uppercase << myString.str() << std::dec << RESET;
    }
}

// #################################################
// # FMC ADC measurements: temperature and voltage #
// #################################################

float RD53FWInterface::ReadHybridTemperature(int hybridId)
{
    RegManager::WriteReg("user.ctrl_regs.i2c_block.dp_addr", hybridId);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    uint32_t sensor1 = RegManager::ReadReg("user.stat_regs.i2c_block_1.NTC1");
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    uint32_t sensor2 = RegManager::ReadReg("user.stat_regs.i2c_block_1.NTC2");
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

    auto value = calcTemperature(sensor1, sensor2);
    LOG(INFO) << BOLDBLUE << "\t--> Hybrid temperature: " << BOLDYELLOW << std::setprecision(3) << value << BOLDBLUE << " C" << std::setprecision(-1) << RESET;

    return value;
}

float RD53FWInterface::ReadHybridVoltage(int hybridId)
{
    RegManager::WriteReg("user.ctrl_regs.i2c_block.dp_addr", hybridId);
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    uint32_t senseVDD = RegManager::ReadReg("user.stat_regs.i2c_block_2.vdd_sense");
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));
    uint32_t senseGND = RegManager::ReadReg("user.stat_regs.i2c_block_2.gnd_sense");
    std::this_thread::sleep_for(std::chrono::microseconds(RD53Shared::DEEPSLEEP));

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

// #######################
// # Bit Error Rate test #
// #######################

double RD53FWInterface::RunBERtest(bool given_time, double frames_or_time, uint16_t hybrid_id, uint16_t chip_id, uint8_t frontendSpeed)
// ####################
// # frontendSpeed    #
// # 1.28 Gbit/s  = 0 #
// # 640 Mbit/s   = 1 #
// # 320 Mbit/s   = 2 #
// ####################
{
    const uint32_t nBitInClkPeriod = 32. * std::pow(2, frontendSpeed); // Number of bits in the 40 MHz clock period
    const double   fps             = 1.28e9 / nBitInClkPeriod;         // Frames per second
    const int      n_prints        = 10;                               // Only an indication, the real number of printouts will be driven by the length of the time steps @CONST@
    double         frames2run;
    double         time2run;
    uint32_t       cntr_lo;
    uint32_t       cntr_hi;

    if(given_time == true)
    {
        time2run   = frames_or_time;
        frames2run = time2run * fps;
    }
    else
    {
        frames2run = frames_or_time;
        time2run   = frames2run / fps;
    }

    // Configure number of printouts and calculate the frequency of printouts
    double time_per_step = std::min(std::max(time2run / n_prints, 1.), 3600.); // The runtime of the PRBS test will have a precision of one step (at most 1h and at least 1s)

    WriteStackReg({{"user.ctrl_regs.PRBS_checker.module_addr", hybrid_id},
                   {"user.ctrl_regs.PRBS_checker.chip_address", chip_id},
                   {"user.ctrl_regs.PRBS_checker.reset_cntr", 1},
                   {"user.ctrl_regs.PRBS_checker.reset_cntr", 0}});

    // Set PRBS frames to run
    uint32_t lowFrames, highFrames;
    std::tie(highFrames, lowFrames) = bits::unpack<32, 32>(static_cast<long long>(frames2run));
    WriteStackReg({{"user.ctrl_regs.prbs_frames_to_run_low", lowFrames}, {"user.ctrl_regs.prbs_frames_to_run_high", highFrames}});
    RD53FWInterface::SendBoardCommand("user.ctrl_regs.PRBS_checker.load_config");

    // #########
    // # Start #
    // #########
    WriteStackReg({{"user.ctrl_regs.PRBS_checker.start_checker", 1}, {"user.ctrl_regs.PRBS_checker.start_checker", 0}});

    LOG(INFO) << BOLDGREEN << "===== BER run starting =====" << std::fixed << std::setprecision(0) << RESET;
    bool     run_done     = false;
    int      idx          = 1;
    uint64_t frameCounter = 0, nErrors = 0;
    while(run_done == false)
    {
        std::this_thread::sleep_for(std::chrono::seconds(static_cast<unsigned int>(time_per_step)));

        // Read frame counters to check progress
        cntr_lo = RegManager::ReadReg("user.stat_regs.prbs_frame_cntr_low");
        cntr_hi = RegManager::ReadReg("user.stat_regs.prbs_frame_cntr_high");
        if(bits::pack<32, 32>(cntr_hi, cntr_lo) == frameCounter)
        {
            LOG(ERROR) << BOLDRED << "BER test stopping because no clock was detected for this chip" << RESET;
            return -1;
        }
        frameCounter = bits::pack<32, 32>(cntr_hi, cntr_lo);
        nErrors      = RegManager::ReadReg("user.stat_regs.prbs_ber_cntr");

        double percent_done = frameCounter / frames2run * 100.;
        LOG(INFO) << GREEN << "I've been running for " << BOLDYELLOW << time_per_step * idx << RESET << GREEN << "s (" << BOLDYELLOW << percent_done << RESET << GREEN << "% done)" << RESET;
        LOG(INFO) << GREEN << "Current counter: " << BOLDYELLOW << nErrors << RESET << GREEN << " frames with error(s)" << RESET;
        if(given_time == true)
            run_done = (time_per_step * idx >= time2run);
        else
            run_done = (frameCounter >= frames2run);
        idx++;
    }
    LOG(INFO) << BOLDGREEN << "========= Finished =========" << RESET;

    // ########
    // # Stop #
    // ########
    WriteStackReg({{"user.ctrl_regs.PRBS_checker.stop_checker", 1}, {"user.ctrl_regs.PRBS_checker.stop_checker", 0}});

    // Read PRBS frame counter
    cntr_lo      = RegManager::ReadReg("user.stat_regs.prbs_frame_cntr_low");
    cntr_hi      = RegManager::ReadReg("user.stat_regs.prbs_frame_cntr_high");
    frameCounter = bits::pack<32, 32>(cntr_hi, cntr_lo);
    nErrors      = RegManager::ReadReg("user.stat_regs.prbs_ber_cntr");
    LOG(INFO) << BOLDGREEN << "===== BER test summary =====" << RESET;
    LOG(INFO) << GREEN << "Final number of PRBS frames sent: " << BOLDYELLOW << frameCounter << RESET;
    LOG(INFO) << GREEN << "Final counter: " << BOLDYELLOW << nErrors << RESET << GREEN << " frames with error(s)" << RESET;
    LOG(INFO) << GREEN << "Final Frame Error Rate: " << BOLDYELLOW << nErrors / time2run << RESET << GREEN << " frames/s (" << BOLDYELLOW << std::fixed << std::setprecision(3)
              << nErrors / frames2run * 100 << RESET << GREEN << "%)" << RESET;
    LOG(INFO) << BOLDGREEN << "====== End of summary ======" << RESET;

    return nErrors / frames2run;
}

} // namespace Ph2_HwInterface
