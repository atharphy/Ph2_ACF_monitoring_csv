#include "tools/OTalignBoardDataWord.h"
#include "HWDescription/BeBoard.h"
#include "HWDescription/Hybrid.h"
#include "HWDescription/OpticalGroup.h"
#include "HWInterface/D19cBackendAlignmentFWInterface.h"
#include "HWInterface/D19cDebugFWInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/ExceptionHandler.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTalignBoardDataWord::fCalibrationDescription = "Find bitslips in the FPGA to decode triggered data on to decode words";

OTalignBoardDataWord::OTalignBoardDataWord() : Tool() {}

OTalignBoardDataWord::~OTalignBoardDataWord() {}

void OTalignBoardDataWord::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeBoardRegister("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl");
    fRegisterHelper->freeBoardRegister("fc7_daq_ctrl.physical_interface_block.link[0-9A-B]_hybrid[01]_L1A_bitslip");
    fRegisterHelper->freeBoardRegister("fc7_daq_ctrl.physical_interface_block.link[0-9A-B]_hybrid[01]_stub_bitslip");

    fBroadcastAlignSetting = findValueInSettings<double>("OTalignBoardDataWord_BroadcastAlignSetting", 0);

    fNumberOfLines = fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS ? 7 : 6;

    // need to free bitslip when will be accessible
    // free the registers in case any
    std::vector<uint8_t> initialEmptyVector(fNumberOfLines, 0);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fBitSlipContainer, initialEmptyVector);
    ContainerFactory::copyAndInitHybrid<std::vector<uint8_t>>(*fDetectorContainer, fAlignmentRetryContainer, initialEmptyVector);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignBoardDataWord.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignBoardDataWord::ConfigureCalibration() {}

void OTalignBoardDataWord::Running()
{
    LOG(INFO) << "Starting OTalignBoardDataWord measurement.";
    Initialise();
    wordAlignBEdata();
    LOG(INFO) << "Done with OTalignBoardDataWord.";
    Reset();
}

void OTalignBoardDataWord::Stop(void)
{
    LOG(INFO) << "Stopping OTalignBoardDataWord measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTalignBoardDataWord.process();
#endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignBoardDataWord stopped.";
}

void OTalignBoardDataWord::Pause() {}

void OTalignBoardDataWord::Resume() {}

void OTalignBoardDataWord::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTalignBoardDataWord::wordAlignBEdata()
{
    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::wordAlignBEdata" << RESET;

    for(auto theBoard: *fDetectorContainer)
    {
        boardWordAlignment(theBoard);
        LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::wordAlignBEdata ... trying to readout L1 data.. " << RESET;
        ReadNEvents(theBoard, 10);
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTalignBoardDataWord.fillBitSlipValues(fBitSlipContainer);
    fDQMHistogramOTalignBoardDataWord.fillAlignmentRetryNumber(fAlignmentRetryContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theBitSlipContainerSerialization("OTalignBoardDataWordBitSlip");
        theBitSlipContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fBitSlipContainer);

        ContainerSerialization theAlignmentRetryContainerSerialization("OTalignBoardDataWordAlignmentRetry");
        theAlignmentRetryContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, fAlignmentRetryContainer);
    }
#endif
}

void OTalignBoardDataWord::boardWordAlignment(BeBoard* theBoard)
{

    std::string controlPhaseRegisterName = "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl";
    std::string statusPhaseRegisterName  = "fc7_daq_stat.physical_interface_block.phase_tuning_reply";

    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::boardWordAlignment for an OG " << RESET;

    // setting CIC to ouput pattern on all trigger lines
    for(auto theOpticalGroup: *theBoard)
    {
        for(auto theHybrid: *theOpticalGroup)
        {
            auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            fCicInterface->SelectOutput(cCic, true);
            fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        }
    }

    fBeBoardInterface->Stop(theBoard);
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 100});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", 3});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);
    fBeBoardInterface->ChipReSync(theBoard);
    fBeBoardInterface->Start(theBoard);

    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
    D19cBackendAlignmentFWInterface* theAlignerInterface = cInterface->getBackendAlignmentInterface();
    if(fBroadcastAlignSetting == 2)
        tryAllHybridAlignment(theAlignerInterface, theBoard);
    else
    {
       D19cDebugFWInterface*            theDebugInterface   = cInterface->getDebugInterface();
        for(auto theOpticalGroup: *theBoard)
        {
            // uint8_t hybdridShift = 27;
            // size_t numberOfStubLines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
            // for(auto theHybrid: *theOpticalGroup)
            // {
            //     auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
            //     fCicInterface->SelectOutput(cCic, true);
            //     fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);

            //     // broadcast
            //     std::string controlRegisterName = "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl";
            //     uint8_t     theHybridFWid       = theHybrid->getId() % 2 + 2 * theOpticalGroup->getId();
            //     theHybridFWid = 0x1F;
            //     uint32_t    configureCommand    = 0x7f21100 | (theHybridFWid << hybdridShift);
            //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, configureCommand);
            //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << configureCommand << std::dec << std::endl;
            //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, controlRegisterName) << std::dec << std::endl;

            //     uint32_t doWordAlignmentCommand = 0x7f50002 | (theHybridFWid << hybdridShift);
            //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, doWordAlignmentCommand);
            //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << doWordAlignmentCommand << std::dec << std::endl;
            //     usleep(100000);
            //     theHybridFWid       = theHybrid->getId() % 2 + 2 * theOpticalGroup->getId();
            //     for(uint32_t line = 0; line < numberOfStubLines + 1; ++line)
            //     {
            //         uint32_t readCommand = 0x10000 | (theHybridFWid << hybdridShift) | ((line) << 20);
            //         fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, readCommand);
            //         std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line " << line << " command 0x" << std::hex << readCommand << std::dec << std::endl;
            //         uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.phase_tuning_reply");
            //         std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line " << line << " reply 0x" << std::hex << readValue << std::dec << std::endl;
            //     }

            //     // one line at a time
            //     // std::string controlRegisterName = "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl";
            //     // uint8_t     theHybridFWid       = theHybrid->getId() % 2 + 2 * theOpticalGroup->getId();
            //     // for(uint32_t line = 0; line < numberOfStubLines; ++line)
            //     // {
            //     //     uint32_t    configureCommand    = 0x20100 | (theHybridFWid << hybdridShift) | ((line + 1) <<20);
            //     //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, configureCommand);
            //     //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << configureCommand << std::dec << std::endl;
            //     //     uint32_t doWordAlignmentCommand = 0x50002 | (theHybridFWid << hybdridShift) | ((line + 1) <<20);
            //     //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, doWordAlignmentCommand);
            //     //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << doWordAlignmentCommand << std::dec << std::endl;
            //     //     usleep(100000);
            //     //     uint32_t readCommand = 0x10000 | (theHybridFWid << hybdridShift) | ((line + 1) << 20);
            //     //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, readCommand);
            //     //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line " << line << " command 0x" << std::hex << readCommand << std::dec << std::endl;
            //     //     uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.phase_tuning_reply");
            //     //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line " << line << " reply 0x" << std::hex << readValue << std::dec << std::endl;
            //     // }
            // }

            bool cAligned = opticalGroupWordAlignment(theOpticalGroup, theAlignerInterface, theDebugInterface);
            if(!cAligned)
            {
                LOG(INFO) << BOLDRED << "Could not align stub word in OTalignBoardDataWord on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId()
                          << " --- OpticalGroup will be disabled" << RESET;
                ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
                continue;
            }
        } // optical groups connected to this  board
    }

    fBeBoardInterface->Stop(theBoard);

    auto getRegisterName = [](const std::string& type, size_t linkNumber, size_t hybridId)
    {
        std::stringstream registerNameStream;
        registerNameStream << std::hex << "fc7_daq_ctrl.physical_interface_block.link" << std::uppercase << linkNumber << "_hybrid" << hybridId << "_" << type << "_bitslip" << std::dec;
        return registerNameStream.str();
    };

    std::vector<std::pair<std::string, uint32_t>> alignedBitslipRegisters;
    for(size_t linkNumber = 0; linkNumber < 12; ++linkNumber)
    {
        for(size_t hybridId = 0; hybridId < 2; ++hybridId)
        {
            alignedBitslipRegisters.push_back({getRegisterName("stub", linkNumber, hybridId), 0xFFFFFFFF});
            alignedBitslipRegisters.push_back({getRegisterName("L1A", linkNumber, hybridId), 0xFFFFFFFF});
        }
    }

    // Reading all bitslip registers
    fBeBoardInterface->ReadBoardMultReg(theBoard, alignedBitslipRegisters);

    // Set MSB to 1 to use values from bitslip registers
    std::for_each(alignedBitslipRegisters.begin(),
                  alignedBitslipRegisters.end(),
                  [](std::pair<std::string, uint32_t>& registerNameAndValue) { registerNameAndValue.second = (registerNameAndValue.second | 0x80000000) + 4; });

    // Updating bitslip registers with MSB set to 1
    fBeBoardInterface->WriteBoardMultReg(theBoard, alignedBitslipRegisters);

    readBitslipRegs();
    
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Pre-reset" << std::endl;
    
    for(uint8_t line=0; line<7; ++line)
    {
        uint32_t value = (0x60010000 | (line << 20));
        fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, value);
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing = 0x" << std::hex << value << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusPhaseRegisterName) << std::dec << std::endl;
    }

    fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xfff50008);

    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] after-reset" << std::endl;
    for(uint8_t line=0; line<7; ++line)
    {
        uint32_t value = (0x60010000 | (line << 20));
        fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, value);
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing = 0x" << std::hex << value << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusPhaseRegisterName) << std::dec << std::endl;
    }

    fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xfff25100);
    
    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] after mode" << std::endl;
    for(uint8_t line=0; line<7; ++line)
    {
        uint32_t value = (0x60010000 | (line << 20));
        fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, value);
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing = 0x" << std::hex << value << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusPhaseRegisterName) << std::dec << std::endl;
    }

    fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xfff50004);

    std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] after manual" << std::endl;
    for(uint8_t line=0; line<7; ++line)
    {
        uint32_t value = (0x60010000 | (line << 20));
        fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, value);
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing = 0x" << std::hex << value << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusPhaseRegisterName) << std::dec << std::endl;
    }


}

bool OTalignBoardDataWord::opticalGroupWordAlignment(const OpticalGroup* theOpticalGroup, D19cBackendAlignmentFWInterface* theAlignerInterface, D19cDebugFWInterface* theDebugInterface)
{
    // align stub lines in the BE
    bool   isPSmodule = theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS;
    size_t cNlines    = isPSmodule ? 7 : 6;
    LOG(INFO) << BOLDMAGENTA << "OTalignBoardDataWord::wordAlignBEdata" << RESET;
    for(auto theHybrid: *theOpticalGroup)
    {
        if(fBroadcastAlignSetting == 1) { return tryAllLineAlignment(theAlignerInterface, theHybrid); }
        else
        {
            for(size_t lineId = 0; lineId < cNlines; lineId++)
            {
                std::string lineName = "L1";
                if(lineId != 0) lineName = "Stub line# " + std::to_string(lineId - 1);
                LOG(INFO) << BOLDMAGENTA << "Aligning " << lineName << " on Hybrid#" << +theHybrid->getId() << RESET;
                bool isLineAligned = tryLineAlignment(theAlignerInterface, theHybrid, lineId);

                if(!isLineAligned)
                {
                    if(skip2SkickOff(theHybrid->getId(), lineId, !isPSmodule)) continue;
                    return false;
                }
            }
        }
    }
    return true;
}

bool OTalignBoardDataWord::tryLineAlignment(D19cBackendAlignmentFWInterface* theAlignerInterface, Hybrid* theHybrid, uint8_t lineId)
{
    bool isLineAligned          = false;
    int  currentIterationNumber = 0;

    auto& theHybridBitSlipVector = fBitSlipContainer.getObject(theHybrid->getBeBoardId())->getObject(theHybrid->getOpticalGroupId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
    auto& theHybridAlignmentRetryVector =
        fAlignmentRetryContainer.getObject(theHybrid->getBeBoardId())->getObject(theHybrid->getOpticalGroupId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();

    while(!isLineAligned && currentIterationNumber < fMaxNumberOfIterations)
    {
        ++currentIterationNumber;
        AlignmentResult theAlignmentResult = theAlignerInterface->alignWord(theHybrid->getId(), lineId);
        isLineAligned                      = theAlignmentResult.fWordAlignmentSuccess;
        if(!isLineAligned)
        {
            LOG(INFO) << BOLDYELLOW << "Alignment on line " << +lineId << " failed, retrying " << fMaxNumberOfIterations - currentIterationNumber << " more times before giving up" << RESET;
            theHybridAlignmentRetryVector[lineId]++;
            continue;
        }
        theHybridBitSlipVector[lineId] = theAlignmentResult.fBitslip;
    }

    return isLineAligned;
}

bool OTalignBoardDataWord::tryAllLineAlignment(D19cBackendAlignmentFWInterface* theAlignerInterface, Hybrid* theHybrid)
{
    bool isHybridAligned        = false;
    int  currentIterationNumber = 0;
    bool isPSmodule             = fDetectorContainer->getObject(theHybrid->getBeBoardId())->getObject(theHybrid->getOpticalGroupId())->getFrontEndType() == FrontEndType::OuterTrackerPS;

    auto& theHybridBitSlipVector = fBitSlipContainer.getObject(theHybrid->getBeBoardId())->getObject(theHybrid->getOpticalGroupId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
    auto& theHybridAlignmentRetryVector =
        fAlignmentRetryContainer.getObject(theHybrid->getBeBoardId())->getObject(theHybrid->getOpticalGroupId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();

    while(!isHybridAligned && currentIterationNumber < fMaxNumberOfIterations)
    {
        ++currentIterationNumber;
        std::vector<AlignmentResult> theAlignmentVectorResult = theAlignerInterface->alignWordAllLines(theHybrid->getId(), fNumberOfLines);
        bool                         allLinesAligned          = true;
        for(uint8_t lineId = 0; lineId < fNumberOfLines; ++lineId)
        {
            if(skip2SkickOff(theHybrid->getId(), lineId, !isPSmodule)) continue;
            if(!theAlignmentVectorResult[lineId].fWordAlignmentSuccess)
            {
                allLinesAligned = false;
                break;
            }
        }
        if(!allLinesAligned)
        {
            LOG(INFO) << BOLDYELLOW << "Alignment failed, retrying " << fMaxNumberOfIterations - currentIterationNumber << " more times before giving up" << RESET;
            for(auto& retry: theHybridAlignmentRetryVector) ++retry;
        }
        else
        {
            isHybridAligned = true;
            for(uint8_t lineId = 0; lineId < fNumberOfLines; ++lineId) { theHybridBitSlipVector[lineId] = theAlignmentVectorResult[lineId].fBitslip; }
        }
    }

    return isHybridAligned;
}

bool OTalignBoardDataWord::tryAllHybridAlignment(D19cBackendAlignmentFWInterface* theAlignerInterface, BeBoard* theBoard)
{
    bool isPSmodule = theBoard->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS;

    for(uint16_t iteration = 0; iteration <= fMaxNumberOfIterations; ++iteration)
    {
        bool               allHybridAligned         = true;
        BoardDataContainer alignmentResultContainer = theAlignerInterface->alignWordAllHybrids(theBoard, fNumberOfLines);
        for(auto theOpticalGroup: alignmentResultContainer)
        {
            for(auto theHybrid: *theOpticalGroup)
            {
                auto& theHybridBitSlipVector = fBitSlipContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
                auto& theHybridAlignmentRetryVector =
                    fAlignmentRetryContainer.getObject(theBoard->getId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
                auto theAlignmentResultVector = theHybrid->getSummary<std::vector<AlignmentResult>>();
                for(uint8_t lineId = 0; lineId < fNumberOfLines; ++lineId)
                {
                    if(skip2SkickOff(theHybrid->getId(), lineId, !isPSmodule)) continue;
                    if(!theAlignmentResultVector[lineId].fWordAlignmentSuccess)
                    {
                        ++theHybridAlignmentRetryVector[lineId];
                        allHybridAligned = false;
                    }
                    else { theHybridBitSlipVector[lineId] = theAlignmentResultVector[lineId].fBitslip; }
                }
            }
        }
        if(allHybridAligned) return true;
    }

    return false;
}

bool OTalignBoardDataWord::skip2SkickOff(uint16_t hybridId, uint8_t lineId, bool is2Smodule)
{
    if(((hybridId % 2) == 0) && (lineId == 5) && is2Smodule)
    {
        LOG(INFO) << BOLDYELLOW << "Attention! ignoring alignment failure on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
        return true;
    }
    return false;
}

void OTalignBoardDataWord::disableUnalignedHybrid(Ph2_HwDescription::Hybrid* theHybrid)
{
    LOG(INFO) << BOLDRED << "Could not align stub word in OTalignBoardDataWord on Board id " << +theHybrid->getBeBoardId() << " OpticalGroup id " << +theHybrid->getOpticalGroupId() << " Hybrid id "
              << +theHybrid->getId() << " --- Hybrid will be disabled" << RESET;
    ExceptionHandler::getInstance()->disableHybrid(theHybrid->getBeBoardId(), theHybrid->getOpticalGroupId(), theHybrid->getId());
}
