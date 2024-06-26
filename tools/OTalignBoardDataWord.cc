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
    fRegisterHelper->freeBoardRegister("fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl");

    fAlignLinesInBroadcast = findValueInSettings<double>("OTalignBoardDataWord_AlignLinesInBroadcast", 0) > 0 ? true : false;

    // need to free bitslip when will be accessible
    // free the registers in case any
    size_t               numberOfLines = (fDetectorContainer->getFirstObject()->getFirstObject()->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 7 : 6;
    std::vector<uint8_t> initialEmptyVector(numberOfLines, 0);
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
    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::boardWordAlignment for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));

    D19cDebugFWInterface*            theDebugInterface   = cInterface->getDebugInterface();
    D19cBackendAlignmentFWInterface* theAlignerInterface = cInterface->getBackendAlignmentInterface();
    fBeBoardInterface->Stop(theBoard);
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 100});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);
    fBeBoardInterface->ChipReSync(theBoard);
    fBeBoardInterface->Start(theBoard);

    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::boardWordAlignment after debug interface " << RESET;

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 3);
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
        //     uint32_t    configureCommand    = 0x7f21100 | (theHybridFWid << hybdridShift);
        //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, configureCommand);
        //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << configureCommand << std::dec << std::endl;
        //     uint32_t doWordAlignmentCommand = 0x7f50002 | (theHybridFWid << hybdridShift);
        //     fBeBoardInterface->WriteBoardReg(theBoard, controlRegisterName, doWordAlignmentCommand);
        //     std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command 0x" << std::hex << doWordAlignmentCommand << std::dec << std::endl;
        //     usleep(100000);
        //     for(uint32_t line = 0; line < numberOfStubLines; ++line)
        //     {
        //         uint32_t readCommand = 0x10000 | (theHybridFWid << hybdridShift) | ((line + 1) << 20);
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

    fBeBoardInterface->Stop(theBoard);

    // readBitslipRegs();

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
                  [](std::pair<std::string, uint32_t>& registerNameAndValue) { registerNameAndValue.second = registerNameAndValue.second | 0x80000000; });

    // Updating bitslip registers with MSB set to 1
    fBeBoardInterface->WriteBoardMultReg(theBoard, alignedBitslipRegisters);

    // readBitslipRegs();
}

bool OTalignBoardDataWord::opticalGroupWordAlignment(const OpticalGroup* theOpticalGroup, D19cBackendAlignmentFWInterface* theAlignerInterface, D19cDebugFWInterface* theDebugInterface)
{
    // align stub lines in the BE
    bool   isPSmodule = theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS;
    size_t cNlines    = isPSmodule ? 7 : 6;
    LOG(INFO) << BOLDMAGENTA << "OTalignBoardDataWord::wordAlignBEdata" << RESET;
    for(auto theHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
        fCicInterface->SelectOutput(cCic, true);
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        auto& theHybridBeBitSlip = fBitSlipContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
        auto& theHybridAlignmentRetry =
            fAlignmentRetryContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();

        if(fAlignLinesInBroadcast) { return tryAllLineAlignment(theAlignerInterface, theHybrid->getId(), isPSmodule, theHybridBeBitSlip, theHybridAlignmentRetry); }
        else
        {
            for(size_t cLineId = 0; cLineId < cNlines; cLineId++)
            {
                std::string lineName = "L1";
                if(cLineId != 0) lineName = "Stub line# " + std::to_string(cLineId - 1);
                LOG(INFO) << BOLDMAGENTA << "Aligning " << lineName << " on Hybrid#" << +theHybrid->getId() << RESET;
                bool isLineAligned = tryLineAlignment(theAlignerInterface, theHybrid->getId(), cLineId, theHybridBeBitSlip, theHybridAlignmentRetry);

                if(!isLineAligned)
                {
                    if(((theHybrid->getId() % 2) == 0) && (cLineId == 5) && !isPSmodule)
                    {
                        LOG(INFO) << BOLDYELLOW << "Attention! ignoring alignment failure on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                        continue;
                    } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
                    return false;
                }
            }
        }
    }
    return true;
}

bool OTalignBoardDataWord::tryLineAlignment(D19cBackendAlignmentFWInterface* theAlignerInterface,
                                            uint16_t                         hybridId,
                                            uint8_t                          lineId,
                                            std::vector<uint8_t>&            theHybridBitSlipVector,
                                            std::vector<uint8_t>&            theHybridAlignmentRetryVector)
{
    bool isLineAligned          = false;
    int  maxNumberOfIterations  = 10;
    int  currentIterationNumber = 0;
    while(!isLineAligned && currentIterationNumber < maxNumberOfIterations)
    {
        ++currentIterationNumber;
        AlignmentResult theAlignmentResult = theAlignerInterface->alignWord(hybridId, lineId);
        isLineAligned                      = theAlignmentResult.fWordAlignmentSuccess;
        if(!isLineAligned)
        {
            LOG(INFO) << BOLDYELLOW << "Alignment on line " << +lineId << " failed, retrying " << maxNumberOfIterations - currentIterationNumber << " more times before giving up" << RESET;
            theHybridAlignmentRetryVector[lineId]++;
            continue;
        }
        theHybridBitSlipVector[lineId] = theAlignmentResult.fBitslip;
    }

    return isLineAligned;
}

bool OTalignBoardDataWord::tryAllLineAlignment(Ph2_HwInterface::D19cBackendAlignmentFWInterface* theAlignerInterface,
                                               uint16_t                                          hybridId,
                                               bool                                              isPSmodule,
                                               std::vector<uint8_t>&                             theHybridBitSlipVector,
                                               std::vector<uint8_t>&                             theHybridAlignmentRetryVector)
{
    bool    isHybridAligned        = false;
    int     maxNumberOfIterations  = 10;
    int     currentIterationNumber = 0;
    uint8_t numberOfLines          = isPSmodule ? 7 : 6;
    while(!isHybridAligned && currentIterationNumber < maxNumberOfIterations)
    {
        ++currentIterationNumber;
        std::vector<AlignmentResult> theAlignmentVectorResult = theAlignerInterface->alignWordAllLines(hybridId, numberOfLines);
        bool                         allLinesAligned          = true;
        for(uint8_t lineId = 0; lineId < numberOfLines; ++lineId)
        {
            if(((hybridId % 2) == 0) && (lineId == 5) && !isPSmodule)
            {
                LOG(INFO) << BOLDYELLOW << "Attention! ignoring alignment failure on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                continue;
            } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
            if(!theAlignmentVectorResult[lineId].fWordAlignmentSuccess)
            {
                allLinesAligned = false;
                break;
            }
        }
        if(!allLinesAligned)
        {
            LOG(INFO) << BOLDYELLOW << "Alignment failed, retrying " << maxNumberOfIterations - currentIterationNumber << " more times before giving up" << RESET;
            for(auto& retry: theHybridAlignmentRetryVector) ++retry;
        }
        else
        {
            isHybridAligned = true;
            for(uint8_t lineId = 0; lineId < numberOfLines; ++lineId) { theHybridBitSlipVector[lineId] = theAlignmentVectorResult[lineId].fBitslip; }
        }
    }

    return isHybridAligned;
}