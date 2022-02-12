#include "D19cL1ReadoutInterface.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
    
D19cL1ReadoutInterface::D19cL1ReadoutInterface(const std::string& pId, const std::string& pUri, const std::string& pAddressTable) : L1ReadoutInterface(pId, pUri, pAddressTable) {
    // handshake should always be off for this readout mode
    fHandshake=ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
}
D19cL1ReadoutInterface::D19cL1ReadoutInterface(const std::string& puHalConfigFileName, uint32_t pBoardId) : L1ReadoutInterface(puHalConfigFileName, pBoardId) {
    // handshake should always be off for this readout mode
    fHandshake=ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
}
D19cL1ReadoutInterface::~D19cL1ReadoutInterface() {}

bool D19cL1ReadoutInterface::ResetReadout()
{
    LOG (INFO) << BOLDBLUE << "D19cL1ReadoutInterface Resetting readout..." << RESET;
    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x1);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
    WriteReg("fc7_daq_ctrl.readout_block.control.readout_reset", 0x0);
    std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));

    LOG(INFO) << BOLDBLUE << "Reseting DDR3 " << RESET;
    fDDR3Offset     = 0;
    auto cDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    while(!cDDR3Calibrated)
    {
        LOG(DEBUG) << "Waiting for DDR3 to finish initial calibration";
        std::this_thread::sleep_for(std::chrono::microseconds(fWait_us));
        cDDR3Calibrated = (ReadReg("fc7_daq_stat.ddr3_block.init_calib_done") == 1);
    }
    return cDDR3Calibrated;
}

void D19cL1ReadoutInterface::CountFwEvents()
{
    fNReadoutEvents = 0;
    if(fData.size() == 0) return ;

    std::vector<uint32_t> cValidData(0);
    cValidData.clear();
    auto   cEventIterator = fData.begin();
    bool   cFoundEmpty    = false;
    size_t cOffset        = 0;
    size_t cCorr          = 0;
    do
    {
        // check event header
        uint32_t cFirstWord = *cEventIterator;
        uint32_t cHeader    = ((0xFFFF << 16) & cFirstWord) >> 16;
        if(cHeader != 0xFFFF)
        {
            if(!cFoundEmpty && cFirstWord == 0)
            {
                cFoundEmpty = true;
                cCorr       = cOffset; // how far from start
            }
            LOG(INFO) << BOLDMAGENTA << "Event header " << std::bitset<16>(cHeader) << " not EXPECTED" << RESET;
            cEventIterator = fData.end();
        }
        else
        {
            uint32_t cEventSize = (0x0000FFFF & (*cEventIterator)) * 4; // event size is given in 128 bit words
            // uint32_t cDummyCount = (0xFF & (*(cEventIterator + 1))) * 4;
            // LOG(DEBUG) << BOLDMAGENTA << "Valid event header .. copying over "
            //           << " event is made up of " << +cEventSize << " 32 bit words "
            //           << " of which " << +cDummyCount << " are dummy words." << RESET;
            // for(size_t cIndx = 0; cIndx < cEventSize; cIndx++) LOG(DEBUG) << BOLDYELLOW << "\t..." << std::bitset<32>(*(cEventIterator + cIndx)) << RESET;
            std::copy(fData.begin() + cOffset, fData.begin() + cOffset + cEventSize, std::back_inserter(cValidData));
            cEventIterator += cEventSize;
            cOffset += cEventSize;
            fNReadoutEvents++;
        }
    } while(cEventIterator < fData.end());
    cCorr = (cFoundEmpty) ? fData.size() - cCorr : cCorr;
    fData.clear();
    // LOG (INFO) << BOLDMAGENTA << "Original data vector has " << cOriginalDataSize
    //     << " 32-bit words..  found empty word in position "
    //     << cCorr
    //     << " from end of vector"
    //     << RESET;
    // adjust offset to first empty slot in DDR3
    if(cFoundEmpty)
    {
        LOG(INFO) << BOLDMAGENTA << "Found an empty event .. all 0s .. resetting DDR3 offset" << RESET;
        // fDDR3Offset = (cFoundEmpty) ? cFoundEmpty - cOffset : fDDR3Offset;
    }
    if(cValidData.size() == 0) return ;

    std::move(cValidData.begin(), cValidData.end(), std::back_inserter(fData));
    LOG(INFO) << BOLDMAGENTA << "Returning a data vector with " << +fData.size() << " valid 32 bit words which are " << +fNReadoutEvents << " events." << RESET;
}

bool D19cL1ReadoutInterface::WaitForReadout()
{
    bool cFailed     = true;
    auto cNWords     = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    auto cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
    auto cStartTime = std::chrono::high_resolution_clock::now(), cEndTime = cStartTime;
    auto cDuration = std::chrono::duration_cast<std::chrono::microseconds>(cEndTime - cStartTime).count();
    if(!fWaitForReadoutReq) // wait until words in the readout have stopped inreasing 
    {
        // LOG (INFO) << BOLDMAGENTA << "D19cL1ReadoutInterface::WaitForReadout Now checking words from the FC7" << RESET;
        uint32_t cNWordsPrev    = cNWords;
        bool     cStopIncrement = false;
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
            cNWords        = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
            cStopIncrement = (cNWords == cNWordsPrev);
            cEndTime = std::chrono::high_resolution_clock::now();
            cDuration     = std::chrono::duration_cast<std::chrono::microseconds>(cEndTime - cStartTime).count();
        
        } while(!cStopIncrement && cDuration < fTimeout_us);
        cFailed = ( cNWords == 0 );
        if( cFailed ) LOG (INFO) << BOLDRED << "D19cL1ReadoutInterface::WaitForReadout no words in the readout .." << RESET; 
    }
    else // send triggers until the readout request flag is '1'
    {
        // try this
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us*10));
            cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            cEndTime = std::chrono::high_resolution_clock::now();
            cDuration     = std::chrono::duration_cast<std::chrono::microseconds>(cEndTime - cStartTime).count();
        } while(cReadoutReq == 0  && cDuration < fTimeout_us); 
        // fails if either one of these is true
        cFailed = ( cReadoutReq == 0);
        if( cFailed ) LOG (INFO) << BOLDRED << "D19cL1ReadoutInterface::WaitForReadout ReadoutReq not cleared.." << RESET; 
    }
    return !cFailed;
}
bool D19cL1ReadoutInterface::WaitForNTriggers()
{
    fTriggerInterface->ResetTriggerFSM();
    // wait for trigger state machine to send all triggers 
    auto cTriggerSource = this->ReadReg("fc7_daq_cnfg.fast_command_block.trigger_source"); // trigger source
    LOG (INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::WaitForNTriggers After resetting trigger FSM.. trigger source is " << cTriggerSource << RESET; 
    LOG(INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::WaitForNTriggers Running Trigger FSM ..." << RESET;
    return fTriggerInterface->RunTriggerFSM();  
}

bool D19cL1ReadoutInterface::WaitForData()
{
    return true;
}
void D19cL1ReadoutInterface::FillData()
{
    fHandshake=ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
    fData.clear();
    uint32_t cNWords  = ReadReg("fc7_daq_stat.readout_block.general.words_cnt");
    if(fHandshake == 0x1)
    {
        size_t cCounter    = 0;
        auto   cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
        do
        {
            std::this_thread::sleep_for(std::chrono::microseconds(fWait_us * 10));
            // if(cCounter % 10 == 0) 
            LOG(INFO) << BOLDRED << "D19cFWInterface::GetData ReadoutReq is " << +cReadoutReq << RESET;
            cReadoutReq = ReadReg("fc7_daq_stat.readout_block.general.readout_req");
            cCounter++;
        } while(cReadoutReq == 0 && cCounter < 100);
        if(cReadoutReq == 0) { LOG(INFO) << BOLDRED << "Readout request 0 [i.e words missing in the readout] ... " << RESET; }
        else
            LOG(DEBUG) << BOLDGREEN << "ReadoutReq fullfilled.... " << RESET;
    }
    else
        LOG(DEBUG) << BOLDBLUE << "Data handshake not enabled" << RESET;
    
    if( cNWords == 0 ) return;
    
    fData = ReadBlockRegOffset("fc7_daq_ddr3", cNWords, fDDR3Offset);
    LOG(INFO) << BOLDGREEN << +cNWords << " words read back from DDR3 memory " << RESET;
    fDDR3Offset += cNWords;
    LOG(DEBUG) << BOLDGREEN << "\t... " << +fDDR3Offset << " current offset in DDR3 " << RESET;
    
    // figure out how many events I've got
    // LOG (INFO) << BOLDMAGENTA << "D19cFWInterface::GetData " << +pData.size() << " words in the readout." << RESET;
    // cNEvents = this->CountFwEvents(pBoard, pData);
    // if(cNEvents == 0) LOG(INFO) << BOLDMAGENTA << "Read back " << +pData.size() << " valid words with " << +cNWords << " in the readout." << RESET;
    // LOG(INFO) << BOLDBLUE << "D19cL1ReadoutInterface has received ... " << +fNEvents << " ... events from DDR3.."
                // << " data size is " << +fData.size() << " 32 bit words." << RESET;
}
bool D19cL1ReadoutInterface::ReadEvents(const Ph2_HwDescription::BeBoard* pBoard)
{
    LOG (INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::ReadEvents " << fNEvents <<  RESET;
    fHandshake = 1; 
    auto cOriginalHandshakeMode     = ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
    WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable",fHandshake);
    auto cHandshake =  ReadReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable");
    // reset readout 
    ResetReadout();
    // write number of triggers to accept
    // in the handshake mode offset is cleared after each handshake
    auto     cMultiplicity  = ReadReg("fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
    fNEvents = fNEvents*(cMultiplicity+1);
    fTriggerInterface->SetNTriggersToAccept(fNEvents);
    LOG(INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::ReadNEvents asking for " << fNEvents << " events handshake mode is currently " << cHandshake << RESET;
    if( WaitForNTriggers() ) // sure that all triggers have been sent 
    {
        if( WaitForReadout() ) // sure that readout has finished 
        {
            LOG (INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::ReadEvents triggers succesfully sent" << RESET;
            FillData();
            LOG (INFO) << BOLDYELLOW << "D19cL1ReadoutInterface::ReadEvents filled data vector with " << fData.size() << " 32-bit words" << RESET;
            CountFwEvents(); 
            WriteReg("fc7_daq_cnfg.readout_block.global.data_handshake_enable",cOriginalHandshakeMode);
            return (fData.size() > 0 );
        }
    }
    return false;
}

} // namespace Ph2_HwInterface