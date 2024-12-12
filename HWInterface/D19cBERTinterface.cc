#include "HWInterface/D19cBERTinterface.h"
#include "HWInterface/RegManager.h"
#include <iostream>
#include <thread>
#include "Utils/Container.h"
#include "Utils/DataContainer.h"
#include "Utils/ContainerFactory.h"

using namespace Ph2_HwInterface;

bool BitErrorTestControl::fIsDebugModeActivated = false;
BitErrorTestControl::Mode BitErrorTestControl::fCurrentMode = BitErrorTestControl::Mode::None0;

uint32_t BitErrorTestControl::encodeCommand() const
{
    uint32_t theCommand = 0;

    theCommand |= ((fHybridId & 0x1F) << 27);
    theCommand |= ((fChipId & 0x7) << 24);
    theCommand |= ((fLineId & 0xF) << 20);
    theCommand |= ((static_cast<uint8_t>(fCommand) & 0xF) << 16);

    switch (fCommand)
    {
    case Command::Configure:
        if(fHybridId == 0x1F || fChipId == 0x7 || fLineId == 0xF)
        {
            theCommand |= ((fDebugMode ? 1 : 0) << 8);
            theCommand |= ((fCheckMode ? 1 : 0) << 7);
            fIsDebugModeActivated = fDebugMode;
        }
        theCommand |= ((fCounterReset ? 1 : 0) << 6);
        theCommand |= ((static_cast<uint8_t>(fCounterSelect) & 0x3) << 4);
        theCommand |= ((static_cast<uint8_t>(fMode) & 0x3) << 2);
        theCommand |= ((fCheckEnable ? 1 : 0) << 1);
        theCommand |= ((fReceiveEnable ? 1 : 0) << 0);
        fCurrentMode = fMode;
        break;

    case Command::SetCounterThreshold:
        theCommand |= ((fCounterThreshold & 0xFF) << 0);
        break;

    case Command::Execute:
        theCommand |= ((fErrorInjection ? 1 : 0) << 7);
        theCommand |= ((fDataLoad ? 1 : 0) << 2);
        break;

    case Command::ReadBERTfirstData:
        if(fIsDebugModeActivated)
        {
            theCommand |= ((fPackagePatternLSB & 0xFF) << 0);
        }
        break;
        
    case Command::ReadBERTsampledData:
        if(fIsDebugModeActivated)
        {
            theCommand |= ((fPackagePatternMSB & 0xFF) << 0);
        }
        break;
    
    default:
        break;
    }

    return theCommand;
}


void BitErrorTestControl::resetCommandBits()
{
    fCommand = Command::ReturnConfig;
    fDebugMode = false;
    fCheckMode = false;
    fCounterReset = false;
    fCounterSelect = CounterSelect::CounterLSB;
    fMode = Mode::None0;
    fCheckEnable = false;
    fReceiveEnable = false;
    fCounterThreshold = 0;
    fErrorInjection = false;
    fDataLoad = false;
    fPackagePatternLSB = 0;
    fPackagePatternMSB = 0;
}


void BitErrorTestReply::decodeReply(uint32_t reply, const BitErrorTestControl& theBitErrorTestControl)
{
    fHybridId = theBitErrorTestControl.fHybridId;
    fChipId = theBitErrorTestControl.fChipId;
    fLineId = theBitErrorTestControl.fLineId;

    auto checkAddress = [this](uint32_t reply, const std::string& caseName)
    {
        uint8_t theHybridId = (reply >> 27) & 0x1F;
        uint8_t theChipId = (reply >> 24) & 0x7;
        uint8_t theLineId = (reply >> 20) & 0xF;
        if(theHybridId != fHybridId || theChipId != fChipId || theLineId != fLineId)
        {
            std::string errorMessage = std::string(__PRETTY_FUNCTION__) + " case " + caseName + " requesting info for hybrid " + std::to_string(this->fHybridId) + " chip " + std::to_string(this->fChipId) + " line " + std::to_string(this->fLineId) + " but received hybrid " + std::to_string(theHybridId) + " chip " + std::to_string(theChipId) + " line " + std::to_string(theLineId);
            std::cerr << errorMessage << std::endl;
            throw std::runtime_error(errorMessage);
        }
    };


    switch (theBitErrorTestControl.fCommand)
    {
    case BitErrorTestControl::Command::ReturnConfig:
    {
        checkAddress(reply, "ReturnConfig");

        bool isFlagSet = ((reply >> 15) & 0x1) > 0;
        fMode = static_cast<BitErrorTestControl::Mode>((reply >> 2) & 0x3);
        BitErrorTestControl::fCurrentMode = fMode;
        switch (BitErrorTestControl::fCurrentMode)
        {
        case BitErrorTestControl::Mode::PRBS:
            fPRBScounterOverflow = isFlagSet;
            break;
            
        case BitErrorTestControl::Mode::LSFR:
            fLFSRcounterOverflow = isFlagSet;
            break;
        
        default:
            break;
        }
        fPRBScheckStateMachineStatus = (reply >> 12) & 0x3;
        fCheckMode = ((reply >> 7) & 0x1) > 0;
        fCounterReset = ((reply >> 6) & 0x1) > 0;
        fCounterSelect = (reply >> 4) & 0x3;
        fCheckEnable = ((reply >> 1) & 0x1) > 0;
        fReceiveEnable = ((reply >> 0) & 0x1) > 0;
        break;
    }

    case BitErrorTestControl::Command::ReturnCounterThreshold:
        checkAddress(reply, "ReturnCounterThreshold");
        fCounterThreshold = reply & 0xFF;
        break;
    
    case BitErrorTestControl::Command::ReadCounterData:
        switch (BitErrorTestControl::fCurrentMode)
        {
        case BitErrorTestControl::Mode::PRBS:
            fPRBScounterValue = reply;
            break;
            
        case BitErrorTestControl::Mode::LSFR:
            fLFSRcounterValue = reply;
            break;
        
        default:
            break;
        }
        break;
    
    case BitErrorTestControl::Command::ReadBERTfirstData:
        if(BitErrorTestControl::fIsDebugModeActivated)
        {
            switch (BitErrorTestControl::fCurrentMode)
            {
            case BitErrorTestControl::Mode::PRBS:
                fPRBSfirstData = reply;
                break;
                
            case BitErrorTestControl::Mode::LSFR:
                fLFSRfirstData = reply;
                break;
            
            default:
                break;
            }
        }
        break;

    case BitErrorTestControl::Command::ReadBERTsampledData:
        if(BitErrorTestControl::fIsDebugModeActivated)
        {
            switch (BitErrorTestControl::fCurrentMode)
            {
            case BitErrorTestControl::Mode::PRBS:
                fPRBSdata = reply;
                break;
                
            case BitErrorTestControl::Mode::LSFR:
                fLFSRdata = reply;
                break;
            
            default:
                break;
            }
        }
        break;
    
    default:
        break;
    }
}

D19cBERTinterface::D19cBERTinterface(RegManager* theRegManager)
: fTheRegManager(theRegManager)
{}

D19cBERTinterface::~D19cBERTinterface() {}

void D19cBERTinterface::writeCommand(BitErrorTestControl theBitErrorTestControl)
{
    uint32_t theCommand = theBitErrorTestControl.encodeCommand();
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_ctrl.physical_interface_block.bert_control 0x" << std::hex << theCommand << std::dec << std::endl;

    fTheRegManager->WriteReg("fc7_daq_ctrl.physical_interface_block.bert_control", theCommand);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void D19cBERTinterface::startBitErrorRateTest(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestControl.setDebugMode(true);
    theBitErrorTestControl.setCounterSelect(BitErrorTestControl::CounterSelect::BitErrorCounter);
    theBitErrorTestControl.setMode(BitErrorTestControl::Mode::PRBS);
    theBitErrorTestControl.setCheckEnable(true);
    theBitErrorTestControl.setReceiveEnable(true);
    writeCommand(theBitErrorTestControl);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::SetCounterThreshold);
    theBitErrorTestControl.setCounterThreshold(0x80);
    writeCommand(theBitErrorTestControl);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Execute);
    theBitErrorTestControl.setDataLoad(true);
    writeCommand(theBitErrorTestControl);
}


void D19cBERTinterface::stopBitErrorRateTest(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestControl.setCheckEnable(false);
    theBitErrorTestControl.setReceiveEnable(false);
    writeCommand(theBitErrorTestControl);
}

uint32_t D19cBERTinterface::getBitErrorCounters(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestControl.setMode(BitErrorTestControl::Mode::PRBS);
    writeCommand(theBitErrorTestControl);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::ReadCounterData);
    writeCommand(theBitErrorTestControl);

    BitErrorTestReply theBitErrorTestCounter = readReplay(theBitErrorTestControl);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::ReturnConfig);
    writeCommand(theBitErrorTestControl);

    BitErrorTestReply theBitErrorTestConfig = readReplay(theBitErrorTestControl);

    return theBitErrorTestConfig.getPRBScounterOverflow() ? 0xFFFFFFFF : theBitErrorTestCounter.getPRBScounterValue();

}

BitErrorTestReply D19cBERTinterface::readReplay(const BitErrorTestControl& theBitErrorTestControl)
{
    uint32_t reply = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.bert_stat");
    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_stat.physical_interface_block.bert_stat 0x" << std::hex << reply << std::dec << std::endl;

    BitErrorTestReply theBitErrorTestReply;
    theBitErrorTestReply.decodeReply(reply, theBitErrorTestControl);
    return theBitErrorTestReply;
}


BoardDataContainer D19cBERTinterface::runBERTonAllHybdrids(BoardContainer* theBoardContainer, uint8_t numberOfLines, uint32_t numberOfSeconds)
{
    
    BoardDataContainer theBERTcounterResult;
    std::vector<uint32_t> theInitialVector(numberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<uint32_t>>(*theBoardContainer, theBERTcounterResult, theInitialVector);

    startBitErrorRateTest(0x1F, 0xF);

    uint32_t sleepingStepSeconds = 10;
    while(numberOfSeconds >= sleepingStepSeconds)
    {
        LOG(INFO) << BOLDMAGENTA << "Sleeping for other " << numberOfSeconds << " seconds" << RESET;
        std::this_thread::sleep_for(std::chrono::seconds(sleepingStepSeconds));
        numberOfSeconds -= sleepingStepSeconds;
    }
    if(numberOfSeconds > 0)
    {
        LOG(INFO) << BOLDMAGENTA << "Sleeping for other " << numberOfSeconds << " seconds" << RESET;
        std::this_thread::sleep_for(std::chrono::seconds(numberOfSeconds));
    }

    stopBitErrorRateTest(0x1F, 0xF);

    for(auto theOpticalGroup: theBERTcounterResult)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theCounterVector = theHybrid->getSummary<std::vector<uint32_t>>();
            for(uint8_t line = 0; line < numberOfLines; ++line)
            {
                theCounterVector.at(line) = getBitErrorCounters(theHybrid->getId(), line);
            }
        }
    }

    return theBERTcounterResult;
}
