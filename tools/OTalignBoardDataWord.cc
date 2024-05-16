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
        stubAndL1WordAlignment(theBoard);
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

void OTalignBoardDataWord::stubAndL1WordAlignment(BeBoard* theBoard)
{
    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::stubAndL1WordAlignment for an OG " << RESET;
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cDebugFWInterface*            theDebugInterface   = cInterface->getDebugInterface();
    D19cBackendAlignmentFWInterface* theAlignerInterface = cInterface->getBackendAlignmentInterface();
    theAlignerInterface->InitializeConfiguration();
    theAlignerInterface->InitializeAlignerObject();
    fBeBoardInterface->Stop(theBoard);

    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::stubAndL1WordAlignment after debug interface " << RESET;

    fBeBoardInterface->WriteBoardReg(theBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 3);
    for(auto theOpticalGroup: *theBoard)
    {
        bool cAligned = stubWordAlignment(theOpticalGroup, theAlignerInterface, theDebugInterface);
        if(!cAligned)
        {
            LOG(INFO) << BOLDRED << "Could not align stub word in OTalignBoardDataWord on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId()
                      << " --- OpticalGroup will be disabled" << RESET;
            ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
            continue;
        }
    } // optical groups connected to this  board

    // Set board trigger configuration for L1 alignment
    std::vector<std::pair<std::string, uint32_t>> cVecReg;
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.backpressure_enable", 0});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", 100});
    cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
    cVecReg.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
    cVecReg.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0x0});
    cVecReg.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0x1});
    fBeBoardInterface->WriteBoardMultReg(theBoard, cVecReg);

    for(auto theOpticalGroup: *theBoard)
    {
        bool cAligned = L1WordAlignment(theOpticalGroup, theAlignerInterface, theDebugInterface);
        if(!cAligned)
        {
            LOG(INFO) << BOLDRED << "Could not align stub word in OTalignBoardDataWord on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId()
                      << " --- OpticalGroup will be disabled" << RESET;
            ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
            continue;
        }
    } // optical groups connected to this  board
}

bool OTalignBoardDataWord::stubWordAlignment(const OpticalGroup* theOpticalGroup, D19cBackendAlignmentFWInterface* theAlignerInterface, D19cDebugFWInterface* theDebugInterface)
{
    // align stub lines in the BE
    size_t cNlines = (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTrackerPS) ? 6 : 5;
    LOG(INFO) << BOLDMAGENTA << "OTalignBoardDataWord::wordAlignBEdata ... word alignment on " << +cNlines << "/6 lines stub from CIC.." << RESET;
    for(auto theHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
        fCicInterface->SelectOutput(cCic, true);
        fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
        auto& theHybridBeBitSlip = fBitSlipContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
        auto& theHybridAlignmentRetry =
            fAlignmentRetryContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();

        for(size_t cLineId = 1; cLineId <= cNlines; cLineId++)
        {
            LOG(INFO) << BOLDMAGENTA << "Aligning Stub line#" << +cLineId << " on Hybrid#" << +theHybrid->getId() << RESET;
            AlignerObject theAlignerObject;
            theAlignerObject.fHybrid  = theHybrid->getId();
            theAlignerObject.fChip    = 0;
            theAlignerObject.fLine    = cLineId;
            theAlignerObject.fOptical = 1;
            LineConfiguration theLineConfiguration;
            theLineConfiguration.fPattern       = 0xEA;
            theLineConfiguration.fPatternPeriod = 8;
            bool isLineAligned                  = tryLineAlignment(theAlignerInterface, cLineId, theAlignerObject, theLineConfiguration, theHybridBeBitSlip, theHybridAlignmentRetry);

            if(!isLineAligned)
            {
                if(((theHybrid->getId() % 2) == 0) & ((cLineId - 1) == 4) & (theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S))
                {
                    LOG(INFO) << BOLDYELLOW << "Attention! ignoring alignment failure on right hybrid CIC line 4 due to bug in kickoff SEH!" << RESET;
                    continue;
                } // CIC_OUT_4_R will always fail for kick-off SEH, ignore here to keep allowing noise measurements
                return false;
            }
        }
    }
    return true;
}

bool OTalignBoardDataWord::L1WordAlignment(const OpticalGroup* theOpticalGroup, D19cBackendAlignmentFWInterface* theAlignerInterface, D19cDebugFWInterface* theDebugInterface)
{
    LOG(INFO) << BOLDYELLOW << "OTalignBoardDataWord::L1WordAlignment " << RESET;

    auto theBoard = fDetectorContainer->getObject(theOpticalGroup->getBeBoardId());
    bool cSuccess = true;

    // configure triggers
    // make sure you're only sending one trigger at a time

    LOG(INFO) << BOLDBLUE << "Aligning the back-end to properly decode L1A data coming from the front-end objects." << RESET;
    fBeBoardInterface->ChipReSync(theBoard);
    fBeBoardInterface->Start(theBoard);
    // back-end tuning on l1 lines
    auto& clpGBT   = theOpticalGroup->flpGBT;
    bool  cOptical = clpGBT != nullptr;

    for(auto theHybrid: *theOpticalGroup)
    {
        auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
        if(cCic == nullptr)
        {
            LOG(INFO) << BOLDYELLOW << " No CIC to use for L1 Word alignment..." << RESET;
            continue;
        }

        auto& theHybridBeBitSlip = fBitSlipContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();
        auto& theHybridAlignmentRetry =
            fAlignmentRetryContainer.getObject(theOpticalGroup->getBeBoardId())->getObject(theOpticalGroup->getId())->getObject(theHybrid->getId())->getSummary<std::vector<uint8_t>>();

        int     cChipId = cCic->getId();
        uint8_t cLineId = 0;
        LOG(INFO) << BOLDBLUE << "Performing word alignment [in the back-end] to prepare for receiving CIC L1A data ...: FE " << +theHybrid->getId() << " Chip" << +cChipId << RESET;
        uint16_t cPattern = 0xFE;

        // configure pattern
        LOG(INFO) << BOLDBLUE << "OTalignBoardDataWord::L1WordAlignment for CIC data" << RESET;
        AlignerObject theAlignerObject;
        theAlignerObject.fHybrid  = theHybrid->getId();
        theAlignerObject.fChip    = 0;
        theAlignerObject.fLine    = cLineId;
        theAlignerObject.fOptical = cOptical ? 1 : 0;
        LineConfiguration theLineConfiguration;
        theLineConfiguration.fPattern    = cPattern;
        theLineConfiguration.fMode       = 0;
        theLineConfiguration.fEnableL1   = 0;
        theLineConfiguration.fMasterLine = 0;

        uint16_t cPatternLength = 40;
        LOG(INFO) << BOLDYELLOW << "Trying to align data with pattern length " << +cPatternLength << RESET;
        theLineConfiguration.fPatternPeriod = cPatternLength;
        cSuccess                            = cSuccess && tryLineAlignment(theAlignerInterface, cLineId, theAlignerObject, theLineConfiguration, theHybridBeBitSlip, theHybridAlignmentRetry);
    }
    fBeBoardInterface->Stop(theBoard);

    return cSuccess;
}

void OTalignBoardDataWord::manuallyConfigureLine(const Chip* pChip, uint8_t pLineId, uint8_t pPhase, uint8_t pBitslip)
{
    auto cBoardId   = pChip->getBeBoardId();
    auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
    fBeBoardInterface->setBoard((*cBoardIter)->getId());
    auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

    D19cBackendAlignmentFWInterface* theAlignerInterface = cInterface->getBackendAlignmentInterface();
    theAlignerInterface->InitializeConfiguration();
    theAlignerInterface->InitializeAlignerObject();

    AlignerObject theAlignerObject;
    theAlignerObject.fHybrid = pChip->getHybridId();
    theAlignerObject.fChip   = pChip->getId();
    theAlignerObject.fLine   = pLineId;
    LineConfiguration theLineConfiguration;
    theLineConfiguration.fMode       = 2;
    theLineConfiguration.fDelay      = pPhase;
    theLineConfiguration.fBitslip    = pBitslip;
    theLineConfiguration.fEnableL1   = 0;
    theLineConfiguration.fMasterLine = 0;
    theAlignerInterface->ManuallyConfigureLine(theAlignerObject, theLineConfiguration);
}

bool OTalignBoardDataWord::tryLineAlignment(D19cBackendAlignmentFWInterface* theAlignerInterface,
                                            uint8_t                          lineId,
                                            AlignerObject&                   theAlignerObject,
                                            LineConfiguration&               theLineConfiguration,
                                            std::vector<uint8_t>&            theHybridBitSlipVector,
                                            std::vector<uint8_t>&            theHybridAlignmentRetryVector)
{
    bool isLineAligned          = false;
    int  maxNumberOfIterations  = 10;
    int  currentIterationNumber = 0;
    while(!isLineAligned && currentIterationNumber < maxNumberOfIterations)
    {
        ++currentIterationNumber;
        theAlignerInterface->AlignWord(theAlignerObject, theLineConfiguration, true);
        isLineAligned = theAlignerInterface->IsLineWordAligned();
        if(!isLineAligned)
        {
            LOG(INFO) << BOLDYELLOW << "Alignment on line " << +lineId << " failed, retrying " << maxNumberOfIterations - currentIterationNumber << " more times before giving up" << RESET;
            theHybridAlignmentRetryVector[lineId]++;
        }
    }
    theHybridBitSlipVector[lineId] = theAlignerInterface->GetLineConfiguration().fBitslip;

    return isLineAligned;
}