#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWDescription/Chip.h"
#include "HWInterface/RegManager.h"
#include "Utils/ConsoleColor.h"

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{

uint32_t PhaseTuningControl::encodeCommand() const
{
    bool newFW = false;
    uint32_t theCommand = 0;

    theCommand |= ((fHybridId & 0x1F ) << (newFW ? 27 : 28));
    theCommand |= ((fLineId & 0xF ) << 20);
    theCommand |= ((static_cast<uint8_t>(fCommand) & 0xF ) << 16);

    switch (fCommand)
    {
        case Command::Configure:
            theCommand |= ((static_cast<uint8_t>(fMode) & 0x3 ) << (newFW ? 13 : 12));
            switch (fMode)
            {
                case Mode::Auto:
                    theCommand |= ((fEnableL1A ? 1 : 0 ) << 11);
                    if(fIsOptical)
                    {
                        theCommand |= ((fEnableSync ? 1 : 0 ) << 8);
                        theCommand |= ((fEnablePRBS ? 1 : 0 ) << 9);
                        theCommand |= ((fEnableLFSR ? 1 : 0 ) << 10);
                        theCommand |= ((fEnableLCC ? 1 : 0 ) << 12);
                    }
                    break;

                case Mode::Slave:
                    if(!fIsOptical) theCommand |= ((fMasterLineId & 0xF) << 8);
                    break;

                case Mode::Manual:
                    theCommand |= ((fBitSlip & 0x1F ) << 0);
                    if(!fIsOptical) theCommand |= ((fDelay & 0x1F ) << 5);
                    break;
                
                default:
                    break;
            }
            break;

        case Command::SetPatternLength:
            if(!fIsOptical) theCommand |= ((fPatternLenght & 0xFF) << 0);
            break;

        case Command::SetSyncPattern:
            if(!fIsOptical) theCommand |= ((fSyncPattern & 0xFF) << 0);
            break;

        case Command::Align:
            if(!fIsOptical) theCommand |= ((fDoPhaseAlignment ? 1 : 0 ) << 0);
            theCommand |= ((fDoWordAlignment ? 1 : 0 ) << 1);
            theCommand |= ((fApplyManual ? 1 : 0 ) << 2);
            break;

        default:
            break;
    }

    return theCommand;
}

void PhaseTuningControl::resetCommandBits()
{
    fBitSlip         = 0;
    fDelay           = 0;
    fPatternLenght   = 0;
    fSyncPattern     = 0;
    fDoWordAlignment = false;
    fDoPhaseAlignment= false;
    fApplyManual     = false;
    fMasterLineId    = 0;
    fEnableSync      = false;
    fEnablePRBS      = false;
    fEnableLFSR      = false;
    fEnableL1A       = false;
    fEnableLCC       = false;
    fMode            = Mode::Auto;
    fCommand         = Command::ReturnConfig;
}

void PhaseTuningReply::decodeReply(uint32_t reply, const PhaseTuningControl& thePhaseTuningControl)
{
    bool isOptical = thePhaseTuningControl.fIsOptical;
    uint8_t expectedLineId = thePhaseTuningControl.fLineId;
    uint8_t receivedLineId = (reply >> 28) & 0xF;
    if(expectedLineId != receivedLineId)
    {
        std::string errorMessage = std::string(__PRETTY_FUNCTION__) + " requestion info for line " + std::to_string(expectedLineId) + " but received line " + std::to_string(receivedLineId);
        std::cerr << errorMessage << std::endl;
        throw std::runtime_error(errorMessage);
    }

    switch (thePhaseTuningControl.fCommand)
    {
        case PhaseTuningControl::Command::ReturnConfig:
            fBitSlip = (reply >> 0) & 0x1F;
            if(isOptical)
            {
                fMode    = static_cast<PhaseTuningControl::Mode>((reply >> 12) & 0x2);
            }
            else
            {
                fDelay = (reply >> 5) & 0xF;
                fMasterLineId =  (reply >> 9) & 0xF;
                fMode    = static_cast<PhaseTuningControl::Mode>((reply >> 13) & 0x2);
            }
            break;

        case PhaseTuningControl::Command::ReturnResult:
            if(isOptical)
            {
                fBitSlip = (reply >> 16) & 0x1F;
                fIsDone  = ((reply >> 15) & 0x1) ? true : false;
                fWordFSMstate = static_cast<WordFSMstate>((reply >> 8) & 0xF);
            }
            else
            {
                fPhaseFSMstate = static_cast<PhaseFSMstate>((reply >> 0) & 0xF);
                fWordFSMstate = static_cast<WordFSMstate>((reply >> 7) & 0xF);
                fIsDone  = ((reply >> 13) & 0x1) ? true : false;
                fBitSlip = (reply >> 14) & 0x1F;
                fDelay    = (reply >> 19) & 0x1F;
            }
            break;
        
        default:
            break;
    }
}

AlignmentResult::AlignmentResult(const PhaseTuningReply& thePhaseTuningReply)
{
    fDone = thePhaseTuningReply.getIsDone();
    fWordAlignmentSuccess = thePhaseTuningReply.getWordFSMstate() == PhaseTuningReply::WordFSMstate::TunedWORD;
    fPhaseAlignmentSuccess = thePhaseTuningReply.getPhaseFSMstate() == PhaseTuningReply::PhaseFSMstate::TunedPHASE;
    fDelay   = thePhaseTuningReply.getDelay();
    fBitslip = thePhaseTuningReply.getBitSlip();
    
    switch (thePhaseTuningReply.getWordFSMstate())
    {
        case PhaseTuningReply::WordFSMstate::IdleWordOrWaitIserdese:
            fWordAlignmentFSMstate = "IdleWordOrWaitIserdese";
            break;
        case PhaseTuningReply::WordFSMstate::WaitFrame:
            fWordAlignmentFSMstate = "WaitFrame";
            break;
        case PhaseTuningReply::WordFSMstate::ApplyBitslip:
            fWordAlignmentFSMstate = "ApplyBitslip";
            break;
        case PhaseTuningReply::WordFSMstate::WaitBitslip:
            fWordAlignmentFSMstate = "WaitBitslip";
            break;
        case PhaseTuningReply::WordFSMstate::PatternVerification:
            fWordAlignmentFSMstate = "PatternVerification";
            break;
        case PhaseTuningReply::WordFSMstate::FailedFrame:
            fWordAlignmentFSMstate = "FailedFrame";
            break;
        case PhaseTuningReply::WordFSMstate::FailedVerification:
            fWordAlignmentFSMstate = "FailedVerification";
            break;
        case PhaseTuningReply::WordFSMstate::TunedWORD:
            fWordAlignmentFSMstate = "TunedWORD";
            break;
        case PhaseTuningReply::WordFSMstate::Unknown:
            fWordAlignmentFSMstate = "Unknown";
            break;
        default:
            fWordAlignmentFSMstate = "NotDefined";
            break;
    }

    switch (thePhaseTuningReply.getPhaseFSMstate())
    {
        case PhaseTuningReply::PhaseFSMstate::IdlePHASE:
            fPhaseAlignmentFSMstate = "IdlePHASE";
            break;
        case PhaseTuningReply::PhaseFSMstate::ResetIDELAYE:
            fPhaseAlignmentFSMstate = "ResetIDELAYE";
            break;
        case PhaseTuningReply::PhaseFSMstate::WaitResetIDELAYE:
            fPhaseAlignmentFSMstate = "WaitResetIDELAYE";
            break;
        case PhaseTuningReply::PhaseFSMstate::ApplyInitialDelay:
            fPhaseAlignmentFSMstate = "ApplyInitialDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::CheckInitialDelay:
            fPhaseAlignmentFSMstate = "CheckInitialDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::InitialSampling:
            fPhaseAlignmentFSMstate = "InitialSampling";
            break;
        case PhaseTuningReply::PhaseFSMstate::ProcessInitialSampling:
            fPhaseAlignmentFSMstate = "ProcessInitialSampling";
            break;
        case PhaseTuningReply::PhaseFSMstate::ApplyDelay:
            fPhaseAlignmentFSMstate = "ApplyDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::CheckDelay:
            fPhaseAlignmentFSMstate = "CheckDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::Sampling:
            fPhaseAlignmentFSMstate = "Sampling";
            break;
        case PhaseTuningReply::PhaseFSMstate::ProcessSampling:
            fPhaseAlignmentFSMstate = "ProcessSampling";
            break;
        case PhaseTuningReply::PhaseFSMstate::WaitGoodDelay:
            fPhaseAlignmentFSMstate = "WaitGoodDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::FailedInitial:
            fPhaseAlignmentFSMstate = "FailedInitial";
            break;
        case PhaseTuningReply::PhaseFSMstate::FailedToApplyDelay:
            fPhaseAlignmentFSMstate = "FailedToApplyDelay";
            break;
        case PhaseTuningReply::PhaseFSMstate::TunedPHASE:
            fPhaseAlignmentFSMstate = "TunedPHASE";
            break;
        case PhaseTuningReply::PhaseFSMstate::Unknown:
            fPhaseAlignmentFSMstate = "Unknown";
            break;
        default:
            fWordAlignmentFSMstate = "NotDefined";
            break;
    }
}

D19cBackendAlignmentFWInterface::D19cBackendAlignmentFWInterface(RegManager* theRegManager) : fTheRegManager(theRegManager) {}

D19cBackendAlignmentFWInterface::~D19cBackendAlignmentFWInterface() {}

AlignmentResult D19cBackendAlignmentFWInterface::tunePhase(uint8_t hybridId, uint8_t lineId)
{
    LOG(WARNING) << BOLDYELLOW << "Attention!!! D19cBackendAlignmentFWInterface::tunePhase not tested since FEH hybrid testing not supported by main Ph2_ACF repository" << RESET;
    if(fIsOptical)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " does not support optical modules, aborting" << RESET;
        abort();
    }
    PhaseTuningControl thePhaseTuningControl(fIsOptical);
    thePhaseTuningControl.setHybridId(hybridId);
    thePhaseTuningControl.setLineId(lineId);

    // Configure command
    thePhaseTuningControl.setCommand(PhaseTuningControl::Command::Configure);
    thePhaseTuningControl.setEnableSync(true);
    thePhaseTuningControl.setMode(PhaseTuningControl::Mode::Auto);
    writeCommand(thePhaseTuningControl.encodeCommand());

    // align line
    thePhaseTuningControl.resetCommandBits();
    thePhaseTuningControl.setCommand(PhaseTuningControl::Command::Align);
    thePhaseTuningControl.setDoPhaseAlignment(true);
    writeCommand(thePhaseTuningControl.encodeCommand());

    return retrieveAlignmentResult(hybridId, lineId);
}

void D19cBackendAlignmentFWInterface::runWordAlignment(uint8_t hybridId, uint8_t lineId)
{
    if(!fIsOptical)
    {
        LOG(ERROR) << BOLDRED << __PRETTY_FUNCTION__ << " does not support not optical modules, aborting" << RESET;
        abort();
    }
    PhaseTuningControl thePhaseTuningControl(fIsOptical);
    thePhaseTuningControl.setHybridId(hybridId);
    thePhaseTuningControl.setLineId(lineId);

    // Configure command
    thePhaseTuningControl.setCommand(PhaseTuningControl::Command::Configure);
    thePhaseTuningControl.setEnableSync(true);
    thePhaseTuningControl.setMode(PhaseTuningControl::Mode::Auto);
    writeCommand(thePhaseTuningControl.encodeCommand());

    // align line
    thePhaseTuningControl.resetCommandBits();
    thePhaseTuningControl.setCommand(PhaseTuningControl::Command::Align);
    thePhaseTuningControl.setDoWordAlignment(true);
    writeCommand(thePhaseTuningControl.encodeCommand());
}

AlignmentResult D19cBackendAlignmentFWInterface::alignWord(uint8_t hybridId, uint8_t lineId)
{
    runWordAlignment(hybridId, lineId);
    return retrieveAlignmentResult(hybridId, lineId);
}

AlignmentResult D19cBackendAlignmentFWInterface::retrieveAlignmentResult(uint8_t hybridId, uint8_t lineId)
{
    PhaseTuningControl thePhaseTuningControl(fIsOptical);
    thePhaseTuningControl.setHybridId(hybridId);
    thePhaseTuningControl.setLineId(lineId);
    thePhaseTuningControl.setCommand(PhaseTuningControl::Command::ReturnResult);
    writeCommand(thePhaseTuningControl.encodeCommand());

    uint32_t reply = fTheRegManager->ReadReg(fPhaseTuningResultRegisterName);
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reply 0x" << std::hex << reply << std::dec << std::endl;
    PhaseTuningReply thePhaseTuningReply;
    thePhaseTuningReply.decodeReply(reply, thePhaseTuningControl);
    AlignmentResult theAlignmentResults(thePhaseTuningReply);

    LOG(INFO) << "\tHybrid:" << +hybridId << " Line: " << lineId;
    LOG(INFO) << "\t\t Done: " << std::boolalpha << +theAlignmentResults.fDone << ", PA FSM: " << BOLDGREEN << theAlignmentResults.fPhaseAlignmentFSMstate << RESET << ", WA FSM: " << BOLDGREEN
                << theAlignmentResults.fWordAlignmentFSMstate << RESET;
    LOG(INFO) << "\t\t Delay: " << +theAlignmentResults.fDelay << ", Bitslip: " << +theAlignmentResults.fBitslip;

    return theAlignmentResults;
}

void D19cBackendAlignmentFWInterface::writeCommand(uint32_t phaseTunerCommand)
{
    fTheRegManager->WriteReg(fPhaseTuningControlRegisterName, phaseTunerCommand);
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << phaseTunerCommand << std::dec << std::endl;
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

std::vector<AlignmentResult> D19cBackendAlignmentFWInterface::alignWordAllLines(uint8_t hybridId, uint8_t numberOfLines)
{
    runWordAlignment(hybridId, 0xF);

    std::vector<AlignmentResult> theAlignmentResultVector;

    for(uint8_t lineId = 0; lineId < numberOfLines; ++lineId) theAlignmentResultVector.emplace_back(retrieveAlignmentResult(hybridId, lineId));

    return theAlignmentResultVector;
}


}
