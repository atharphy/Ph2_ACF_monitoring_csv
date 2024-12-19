#include "HWInterface/D19cBERTinterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "HWInterface/RegManager.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/DataContainer.h"
#include "Utils/GenericDataArray.h"
#include <cmath>
#include <iostream>
#include <thread>

using namespace Ph2_HwInterface;

bool                               BitErrorTestControl::fCurrentCheckMode     = false;
bool                               BitErrorTestControl::fIsDebugModeActivated = false;
BitErrorTestControl::Mode          BitErrorTestControl::fCurrentMode          = BitErrorTestControl::Mode::None0;
BitErrorTestControl::CounterSelect BitErrorTestControl::fCurrentCounterSelect = BitErrorTestControl::CounterSelect::FrameCounterLSB;

uint32_t BitErrorTestControl::encodeCommand() const
{
    uint32_t theCommand = 0;

    theCommand |= ((fHybridId & 0x1F) << 27);
    theCommand |= ((fChipId & 0x7) << 24);
    theCommand |= ((fLineId & 0xF) << 20);
    theCommand |= ((static_cast<uint8_t>(fCommand) & 0xF) << 16);

    switch(fCommand)
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
        fCurrentMode          = fMode;
        fCurrentCounterSelect = fCounterSelect;
        fCurrentCheckMode     = fCheckMode;
        break;

    case Command::SetCounterThreshold: theCommand |= ((fCounterThreshold & 0xFF) << 0); break;

    case Command::ErrorInject:
        theCommand |= ((fErrorInjection ? 1 : 0) << 7);
        theCommand |= ((fDataLoad ? 1 : 0) << 2);
        break;

    case Command::ReadBERTfirstData:
        if(fIsDebugModeActivated) { theCommand |= ((fPackagePatternLSB & 0xFF) << 0); }
        break;

    case Command::ReadBERTsampledData:
        if(fIsDebugModeActivated) { theCommand |= ((fPackagePatternMSB & 0xFF) << 0); }
        break;

    default: break;
    }

    return theCommand;
}

void BitErrorTestControl::getLine(const BitErrorTestControl& theBitErrorTestReply)
{
    fHybridId = theBitErrorTestReply.fHybridId;
    fChipId   = theBitErrorTestReply.fChipId;
    fLineId   = theBitErrorTestReply.fLineId;
}

void BitErrorTestControl::resetCommandBits()
{
    fCommand           = Command::ReturnConfig;
    fDebugMode         = false;
    fCheckMode         = false;
    fCounterReset      = false;
    fCounterSelect     = CounterSelect::FrameCounterLSB;
    fMode              = Mode::None0;
    fCheckEnable       = false;
    fReceiveEnable     = false;
    fCounterThreshold  = 0;
    fErrorInjection    = false;
    fDataLoad          = false;
    fPackagePatternLSB = 0;
    fPackagePatternMSB = 0;
}

void BitErrorTestReply::decodeReply(uint32_t reply, const BitErrorTestControl& theBitErrorTestControl)
{
    fHybridId = theBitErrorTestControl.fHybridId;
    fChipId   = theBitErrorTestControl.fChipId;
    fLineId   = theBitErrorTestControl.fLineId;

    auto checkAddress = [this](uint32_t reply, const std::string& caseName)
    {
        uint8_t theHybridId = (reply >> 27) & 0x1F;
        uint8_t theChipId   = (reply >> 24) & 0x7;
        uint8_t theLineId   = (reply >> 20) & 0xF;
        if(theHybridId != fHybridId || theChipId != fChipId || theLineId != fLineId)
        {
            std::string errorMessage = std::string(__PRETTY_FUNCTION__) + " case " + caseName + " requesting info for hybrid " + std::to_string(this->fHybridId) + " chip " +
                                       std::to_string(this->fChipId) + " line " + std::to_string(this->fLineId) + " but received hybrid " + std::to_string(theHybridId) + " chip " +
                                       std::to_string(theChipId) + " line " + std::to_string(theLineId);
            std::cerr << errorMessage << std::endl;
            throw std::runtime_error(errorMessage);
        }
    };

    switch(theBitErrorTestControl.fCommand)
    {
    case BitErrorTestControl::Command::ReturnConfig:
    {
        checkAddress(reply, "ReturnConfig");

        bool isFlagSet                    = ((reply >> 15) & 0x1) > 0;
        fMode                             = static_cast<BitErrorTestControl::Mode>((reply >> 2) & 0x3);
        BitErrorTestControl::fCurrentMode = fMode;
        switch(BitErrorTestControl::fCurrentMode)
        {
        case BitErrorTestControl::Mode::PRBS: fPRBScounterOverflow = isFlagSet; break;

        case BitErrorTestControl::Mode::LSFR: fLFSRcounterOverflow = isFlagSet; break;

        default: break;
        }
        fPRBScheckStateMachineStatus = (reply >> 12) & 0x3;
        fCheckMode                   = ((reply >> 7) & 0x1) > 0;
        fCounterReset                = ((reply >> 6) & 0x1) > 0;
        fCounterSelect               = (reply >> 4) & 0x3;
        fCheckEnable                 = ((reply >> 1) & 0x1) > 0;
        fReceiveEnable               = ((reply >> 0) & 0x1) > 0;
        break;
    }

    case BitErrorTestControl::Command::ReturnCounterThreshold:
        checkAddress(reply, "ReturnCounterThreshold");
        fCounterThreshold = reply & 0xFF;
        break;

    case BitErrorTestControl::Command::ReadCounterData:
        switch(BitErrorTestControl::fCurrentMode)
        {
        case BitErrorTestControl::Mode::PRBS:
            if(BitErrorTestControl::fCurrentCheckMode)
            {
                switch(BitErrorTestControl::fCurrentCounterSelect)
                {
                case BitErrorTestControl::CounterSelect::FrameCounterLSB: fFrameCounterLSB = reply; break;
                case BitErrorTestControl::CounterSelect::FrameCounterMSB: fFrameCounterMSB = reply; break;
                case BitErrorTestControl::CounterSelect::FrameErrorCounter: fPRBSframeCounterValueEmulator = reply; break;
                case BitErrorTestControl::CounterSelect::BitErrorCounter: fPRBSbitCounterValueEmulator = reply; break;
                default: break;
                }
            }
            else
            {
                switch(BitErrorTestControl::fCurrentCounterSelect)
                {
                case BitErrorTestControl::CounterSelect::FrameCounterLSB: fFrameCounterLSB = reply; break;
                case BitErrorTestControl::CounterSelect::FrameCounterMSB: fFrameCounterMSB = reply; break;
                case BitErrorTestControl::CounterSelect::FrameErrorCounter: fPRBSframeCounterValuePredictNext = reply; break;
                case BitErrorTestControl::CounterSelect::BitErrorCounter: fPRBSbitCounterValuePredictNext = reply; break;
                default: break;
                }
            }
            break;

        case BitErrorTestControl::Mode::LSFR:
            // not handled by the FW at the moment
            break;

        default: break;
        }
        break;

    case BitErrorTestControl::Command::ReadBERTfirstData:
        if(!BitErrorTestControl::fIsDebugModeActivated)
        {
            switch(BitErrorTestControl::fCurrentMode)
            {
            case BitErrorTestControl::Mode::PRBS: fPRBSfirstData = reply; break;

            case BitErrorTestControl::Mode::LSFR: fLFSRfirstData = reply; break;

            default: break;
            }
        }
        break;

    case BitErrorTestControl::Command::ReadBERTsampledData:
        if(BitErrorTestControl::fIsDebugModeActivated)
        {
            switch(BitErrorTestControl::fCurrentMode)
            {
            case BitErrorTestControl::Mode::PRBS: fPRBSdata = reply; break;

            case BitErrorTestControl::Mode::LSFR: fLFSRdata = reply; break;

            default: break;
            }
        }
        break;

    default: break;
    }
}

D19cBERTinterface::D19cBERTinterface(RegManager* theRegManager) : fTheRegManager(theRegManager) {}

D19cBERTinterface::~D19cBERTinterface() {}

void D19cBERTinterface::writeCommand(BitErrorTestControl theBitErrorTestControl)
{
    uint32_t theCommand = theBitErrorTestControl.encodeCommand();
    // std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_ctrl.physical_interface_block.bert_control 0x" << std::hex << theCommand << std::dec << std::endl;

    fTheRegManager->WriteReg("fc7_daq_ctrl.physical_interface_block.bert_control", theCommand);
    std::this_thread::sleep_for(std::chrono::microseconds(100));
}

void D19cBERTinterface::startBitErrorRateTest(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestConfigure;
    theBitErrorTestConfigure.setHybridId(hybridId);
    theBitErrorTestConfigure.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestConfigure.setLineId(lineId);

    theBitErrorTestConfigure.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestConfigure.setCounterSelect(BitErrorTestControl::CounterSelect::BitErrorCounter);
    theBitErrorTestConfigure.setMode(BitErrorTestControl::Mode::PRBS);
    theBitErrorTestConfigure.setReceiveEnable(true);
    theBitErrorTestConfigure.setCheckMode(true);
    writeCommand(theBitErrorTestConfigure);

    BitErrorTestControl theBitErrorTestSetThreshold;
    theBitErrorTestSetThreshold.getLine(theBitErrorTestConfigure);
    theBitErrorTestSetThreshold.resetCommandBits();
    theBitErrorTestSetThreshold.setCommand(BitErrorTestControl::Command::SetCounterThreshold);
    theBitErrorTestSetThreshold.setCounterThreshold(0x80);
    writeCommand(theBitErrorTestSetThreshold);

    // Add first pattern check on command 6 (first data)
    theBitErrorTestConfigure.setCheckEnable(true);
    writeCommand(theBitErrorTestConfigure);
}

void D19cBERTinterface::stopBitErrorRateTest(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestControl.setCounterSelect(BitErrorTestControl::CounterSelect::BitErrorCounter);
    theBitErrorTestControl.setMode(BitErrorTestControl::Mode::PRBS);
    theBitErrorTestControl.setReceiveEnable(true);
    theBitErrorTestControl.setCheckMode(true);
    writeCommand(theBitErrorTestControl);
}

void D19cBERTinterface::haltBitErrorRateTest(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::Configure);
    theBitErrorTestControl.setCounterSelect(BitErrorTestControl::CounterSelect::BitErrorCounter);
    theBitErrorTestControl.setMode(BitErrorTestControl::Mode::PRBS);
    theBitErrorTestControl.setCheckMode(true);
    writeCommand(theBitErrorTestControl);
}

uint32_t D19cBERTinterface::getBitErrorCounters(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(0);
    theBitErrorTestControl.setLineId(lineId);
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::ReadCounterData);
    writeCommand(theBitErrorTestControl);

    BitErrorTestReply theBitErrorTestCounter = readReplay(theBitErrorTestControl);

    return theBitErrorTestCounter.getPRBSbitCounterValueEmulator();
}

void D19cBERTinterface::selectFrameCounters(bool isMSB)
{
    BitErrorTestControl theSetFrameCounterControl;
    theSetFrameCounterControl.setHybridId(0x1F);
    theSetFrameCounterControl.setChipId(0x7);
    theSetFrameCounterControl.setLineId(0xF);

    theSetFrameCounterControl.setCommand(BitErrorTestControl::Command::Configure);
    theSetFrameCounterControl.setCounterSelect(BitErrorTestControl::CounterSelect::BitErrorCounter);
    theSetFrameCounterControl.setMode(BitErrorTestControl::Mode::PRBS);
    theSetFrameCounterControl.setReceiveEnable(true);
    theSetFrameCounterControl.setCheckMode(true);
    if(isMSB)
        theSetFrameCounterControl.setCounterSelect(BitErrorTestControl::CounterSelect::FrameCounterMSB);
    else
        theSetFrameCounterControl.setCounterSelect(BitErrorTestControl::CounterSelect::FrameCounterLSB);
    writeCommand(theSetFrameCounterControl);
}

uint64_t D19cBERTinterface::getFrameCounters(uint8_t hybridId, uint8_t lineId, bool isMSB)
{
    BitErrorTestControl theReadDataControl;
    theReadDataControl.setHybridId(hybridId);
    theReadDataControl.setChipId(0);
    theReadDataControl.setLineId(lineId);
    theReadDataControl.setCommand(BitErrorTestControl::Command::ReadCounterData);
    writeCommand(theReadDataControl);

    BitErrorTestReply theFrameLSBcounter = readReplay(theReadDataControl);

    return isMSB ? theFrameLSBcounter.getFrameCounterMSB() : theFrameLSBcounter.getFrameCounterLSB();
}

uint32_t D19cBERTinterface::getFirstData(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(0);
    theBitErrorTestControl.setLineId(lineId);

    theBitErrorTestControl.resetCommandBits();
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::ReadBERTfirstData);
    writeCommand(theBitErrorTestControl);

    BitErrorTestReply theBitErrorTestCounter = readReplay(theBitErrorTestControl);

    return theBitErrorTestCounter.getPRBSfirstData();
}

BitErrorTestReply D19cBERTinterface::readReplay(const BitErrorTestControl& theBitErrorTestControl)
{
    uint32_t reply = fTheRegManager->ReadReg("fc7_daq_stat.physical_interface_block.bert_stat");
    // std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] fc7_daq_stat.physical_interface_block.bert_stat 0x" << std::hex << reply << std::dec << std::endl;

    BitErrorTestReply theBitErrorTestReply;
    theBitErrorTestReply.decodeReply(reply, theBitErrorTestControl);
    return theBitErrorTestReply;
}

void D19cBERTinterface::injectError(uint8_t hybridId, uint8_t lineId)
{
    BitErrorTestControl theBitErrorTestControl;
    theBitErrorTestControl.setHybridId(hybridId);
    theBitErrorTestControl.setChipId(lineId == 0xF ? 0x7 : 0x0);
    theBitErrorTestControl.setLineId(lineId);
    theBitErrorTestControl.setCommand(BitErrorTestControl::Command::ErrorInject);
    theBitErrorTestControl.setErrorInjection(true);
    writeCommand(theBitErrorTestControl);
}

BoardDataContainer D19cBERTinterface::runBERTonAllHybdrids(BoardContainer* theBoardContainer, uint8_t numberOfLines, bool is10Gmodule, float numberOfMatchedBits)
{
    uint8_t hybridId = 0x1F;
    uint8_t lineId   = 0xF;

    float dataRate = 3.2E8;
    if(is10Gmodule) dataRate *= 2.;

    float    extimatedWait = numberOfMatchedBits / dataRate + 0.5;
    uint32_t waitInSec     = ceil(extimatedWait);

    BoardDataContainer                         theBERTcounterResult;
    std::vector<GenericDataArray<uint64_t, 2>> theInitialVector(numberOfLines);
    ContainerFactory::copyAndInitHybrid<std::vector<GenericDataArray<uint64_t, 2>>>(*theBoardContainer, theBERTcounterResult, theInitialVector);

    startBitErrorRateTest(hybridId, lineId);

    for(auto theOpticalGroup: theBERTcounterResult)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            bool failedToFindPatternStart = false;
            for(uint8_t line = 0; line < numberOfLines; ++line)
            {
                uint16_t iteration     = 0;
                uint16_t maxIterations = 10;
                while(iteration < maxIterations)
                {
                    auto firstData = getFirstData(theHybrid->getId(), line) >> 16;
                    if(firstData == BERT_ALIGNMENT_PATTERN) break;
                    ++iteration;
                }
                if(iteration >= maxIterations)
                {
                    failedToFindPatternStart = true;
                    LOG(ERROR) << ERROR_FORMAT << "Failed to find BERT start pattern on line " << +line << " after " << maxIterations << " iterations" << RESET;
                }
            }
            if(failedToFindPatternStart)
            {
                LOG(INFO) << BOLDRED << "Cannot find BERT start pattern for OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id " << +theHybrid->getId() << " --- Hybrid will be disabled"
                          << RESET;
                ExceptionHandler::getInstance()->disableHybrid(theBoardContainer->getId(), theOpticalGroup->getId(), theHybrid->getId());
            }
        }
    }

    // injectError(hybridId, lineId);

    uint32_t sleepingStepSeconds = 10;
    while(waitInSec >= sleepingStepSeconds)
    {
        LOG(INFO) << BOLDMAGENTA << "Sleeping for other " << waitInSec << " seconds" << RESET;
        std::this_thread::sleep_for(std::chrono::seconds(sleepingStepSeconds));
        waitInSec -= sleepingStepSeconds;
    }
    if(waitInSec > 0)
    {
        LOG(INFO) << BOLDMAGENTA << "Sleeping for other " << waitInSec << " seconds" << RESET;
        std::this_thread::sleep_for(std::chrono::seconds(waitInSec));
    }

    stopBitErrorRateTest(hybridId, lineId);

    for(auto theOpticalGroup: theBERTcounterResult)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theCounterVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
            for(uint8_t line = 0; line < numberOfLines; ++line)
            {
                float BERTcounter               = getBitErrorCounters(theHybrid->getId(), line);
                theCounterVector.at(line).at(1) = BERTcounter;
            }
        }
    }

    selectFrameCounters(true);

    for(auto theOpticalGroup: theBERTcounterResult)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& theCounterVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
            for(uint8_t line = 0; line < numberOfLines; ++line) { theCounterVector.at(line).at(0) = (getFrameCounters(theHybrid->getId(), line, true) << 32); }
        }
    }

    selectFrameCounters(false);

    for(auto theOpticalGroup: theBERTcounterResult)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            bool  missingFrames    = false;
            auto& theCounterVector = theHybrid->getSummary<std::vector<GenericDataArray<uint64_t, 2>>>();
            for(uint8_t line = 0; line < numberOfLines; ++line)
            {
                theCounterVector.at(line).at(0) |= getFrameCounters(theHybrid->getId(), line, false);
                float totalBitCounter = theCounterVector.at(line).at(0) * 8.;
                if(totalBitCounter < numberOfMatchedBits)
                {
                    missingFrames = true;
                    LOG(ERROR) << ERROR_FORMAT << "Number of checked bits " << totalBitCounter << " is less then the expected number " << numberOfMatchedBits << " for line " << +line << RESET;
                }
            }

            if(missingFrames)
            {
                LOG(INFO) << BOLDRED << "Failed to run BERT on OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id " << +theHybrid->getId() << " --- Hybrid will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableHybrid(theBoardContainer->getId(), theOpticalGroup->getId(), theHybrid->getId());
            }
        }
    }

    haltBitErrorRateTest(hybridId, lineId);

    return theBERTcounterResult;
}
