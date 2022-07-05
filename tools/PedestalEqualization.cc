#include "PedestalEqualization.h"
#include "../HWDescription/ReadoutChip.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/DataContainer.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"

// initialize the static member

using namespace Ph2_System;
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

PedestalEqualization::PedestalEqualization() : Tool() {}

PedestalEqualization::~PedestalEqualization() {}

void PedestalEqualization::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fDisableStubLogic = pDisableStubLogic;

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    fWithCBC = false;
    fWithSSA = false;
    fWithMPA = false;
    std::vector<FrontEndType> cAllFrontEndTypes;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithCBC            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::CBC3) != cFrontEndTypes.end();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }
    if(fWithCBC) LOG(INFO) << BOLDBLUE << "PedestalEqualization with CBCs" << RESET;
    if(fWithSSA && !fWithMPA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with SSAs" << RESET;
    if(fWithMPA && !fWithSSA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with MPAs" << RESET;
    if(fWithSSA && fWithMPA) LOG(INFO) << BOLDBLUE << "PedestalEqualization with SSAs+MPAs" << RESET;

    if(fWithCBC)
    {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
    }
    if(fWithSSA && !fWithMPA)
    {
        SSAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        // temporary
        setChannelGroupHandler(theChannelGroupHandler, cAllFrontEndTypes);
    }
    if(!fWithSSA && fWithMPA)
    {
        MPAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, cAllFrontEndTypes);
        // setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA);
        // setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA2);
    }
    if(fWithSSA && fWithMPA)
    {
        // SSAs
        SSAChannelGroupHandler theSSAChannelGroupHandler;
        theSSAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theSSAChannelGroupHandler, FrontEndType::SSA);
        // Now MPAs
        MPAChannelGroupHandler theMPAChannelGroupHandler;
        theMPAChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theMPAChannelGroupHandler, FrontEndType::MPA);
    }

    this->fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fCheckLoop                   = findValueInSettings<double>("VerificationLoop", 1);
    fTestPulseAmplitude          = findValueInSettings<double>("PedestalEqualizationPulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings<double>("Nevents", 10);
    fNEventsPerBurst             = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    fOccupancyAtPedestal         = findValueInSettings<double>("PedestalEqualizationOccupancy", 0.56);
    uint8_t cDefTargetOffset     = (fWithCBC) ? 0x7F : 0xF;
    fTargetOffset                = findValueInSettings<double>("PedestalEqualizationTargetOffset", cDefTargetOffset);
    // uint8_t cEnableFastCounterReadout = (uint8_t)findValueInSettings<double>("EnableFastCounterReadout", 0);
    // uint8_t cEnablePairSelect         = (uint8_t)findValueInSettings<double>("EnablePairSelect", 0);

    LOG(INFO) << BOLDBLUE << "PedestalEqualization::Initialise Occupancy at pedestal is " << fOccupancyAtPedestal << " target offset is " << +fTargetOffset << RESET;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // event types
    fEventTypes.clear();
    bool cForcePSasync = true;
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.push_back(cBoard->getEventType());
        if(!fWithSSA && !fWithMPA) continue;
        if(!cForcePSasync) continue;
        cBoard->setEventType(EventType::PSAS);
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->InitializePSCounterFWInterface(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 1); }
            }
        }
    }

    // make sure register tracking is on
    for(auto board: *fDetectorContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    chip->setRegisterTracking(1);
                    chip->ClearModifiedRegisterMap();
                }
            }
        }
    }

    if(fDisableStubLogic)
    {
        // ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fStubLogicCointainer);
        // ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fHIPCountCointainer);

        for(auto board: *fDetectorContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(chip);
                        // if it is a CBC3, disable the stub logic for this procedure
                        if(theChip->getFrontEndType() == FrontEndType::CBC3)
                        {
                            LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - thus disabling Stub logic for offset tuning for CBC " << +chip->getId() << RESET;
                            // fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() =
                            //     fReadoutChipInterface->ReadChipReg(theChip, "Pipe&StubInpSel&Ptwidth");
                            // uint8_t value = fReadoutChipInterface->ReadChipReg(theChip, "HIP&TestMode");
                            // fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() = value;
                            static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(theChip, false, true, 0);
                        }
                    }
                }
            }
        }
    }

    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << "	Nevents = " << fEventsPerPoint;
    LOG(INFO) << "	TestPulseAmplitude = " << int(fTestPulseAmplitude);
    LOG(INFO) << "  Target Vcth determined algorithmically for Chip";
    LOG(INFO) << "  Target Offset = 0x" << std::hex << +fTargetOffset;
}
void PedestalEqualization::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by PedestalEqualization" << RESET;
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) { cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second)); }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    auto cModMap = cChip->GetModifiedRegisterMap();
                    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    std::vector<std::pair<std::string, uint16_t>> cRegList;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        // don't reconfigure the offsets .. whole point of this excercise
                        if(cMapItem.first.find("Channel") != std::string::npos) continue;
                        if(cMapItem.first.find("TrimDAC") != std::string::npos) continue;
                        if(cMapItem.first.find("THTRIMMING") != std::string::npos) continue;

                        LOG(DEBUG) << BOLDYELLOW << "PedestalEqualization::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
                        cRegList.push_back(std::make_pair(cMapItem.first, cMapItem.second.fValue));
                    }
                    fReadoutChipInterface->WriteChipMultReg(cChip, cRegList, false);
                    // don't track registers + clear mod reg map
                    cChip->setRegisterTracking(0);
                    cChip->ClearModifiedRegisterMap();
                }
            }
        }
    }
    resetPointers();

    // size_t cIndx = 0;
    // for(auto cBoard: *fDetectorContainer)
    // {
    //     if(fEventTypes[cIndx] == EventType::PSAS) continue;
    //     cBoard->setEventType(fEventTypes[cIndx]);
    //     for(auto cOpticalGroup: *cBoard)
    //     {
    //         for(auto cHybrid: *cOpticalGroup)
    //         {
    //             auto cType    = FrontEndType::SSA;
    //             bool fWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
    //             cType         = FrontEndType::MPA;
    //             bool fWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
    //             if(!fWithSSA && !fWithMPA) continue;

    //             for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "ReadoutMode", 0); }
    //         }
    //     }
    // }
}
void PedestalEqualization::FindVplus()
{
    // original tool flags
    bool    originalAllChannelFlag = this->fAllChan;
    uint8_t cNormalizationOrig     = getNormalization();

    // figure  out if you should normalize or not
    uint cNormalize = 1;
    LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    setNormalization(cNormalize);

    if(fTestPulse)
    {
        this->enableTestPulse(true);
        for(auto cBoard: *fDetectorContainer)
        {
            if(fWithSSA or fWithMPA)
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fTestPulseAmplitude);
            else
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fTestPulseAmplitude);
        }
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    }
    else
        this->enableTestPulse(false);

    LOG(INFO) << BOLDBLUE << "Setting threshold trim registers to mid-range value...0x" << std::hex << +fTargetOffset << std::dec << RESET;
    if(fWithCBC)
        setSameLocalDac("ChannelOffset", fTargetOffset);
    else
        setSameLocalDac("ThresholdTrim", fTargetOffset);

    LOG(INFO) << BOLDBLUE << "Finding threshold at which to equalize offsets - searching for threshold where <Occupancy>/Chip is " << fOccupancyAtPedestal << RESET;
    uint8_t cTargetVcth = 0x0;
    setSameDac("Threshold", cTargetVcth);
    this->SetTestAllChannels(true);
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    this->bitWiseScan("Threshold", fEventsPerPoint, fOccupancyAtPedestal, fNEventsPerBurst);

    // dumpConfigFiles();

    // LOG(INFO) << BOLDBLUE << "Setting threshold trim registers to max value..." << RESET;
    if(fWithCBC)
        setSameLocalDac("ChannelOffset", 0xFF);
    else
        setSameLocalDac("ThresholdTrim", 0x1F);

    // store thresholds
    DetectorDataContainer theVcthContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theVcthContainer);

    float cMeanStripsValue = 0., cMeanPixelsValue = 0.;
    float cNStripChips = 0., cNPixelChips = 0.;
    for(auto board: theVcthContainer) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                // nChip += hybrid->size();
                for(auto chip: *hybrid) // for on chip - begin
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));
                    uint16_t     tmpVthr = 0;
                    auto         cType   = theChip->getFrontEndType();
                    if(cType == FrontEndType::CBC3) tmpVthr = (theChip->getReg("VCth1") + (theChip->getReg("VCth2") << 8));
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2) tmpVthr = theChip->getReg("Bias_THDAC");
                    if(cType == FrontEndType::MPA) tmpVthr = theChip->getReg("ThDAC0");
                    chip->getSummary<uint16_t>() = tmpVthr;
                    LOG(INFO) << GREEN << "VCth value for BeBoard " << +board->getId() << " OpticalGroup " << +opticalGroup->getId() << " Hybrid " << +hybrid->getId() << " Chip " << +chip->getId()
                              << " = " << tmpVthr << RESET;
                    uint32_t ENCHAN  = theChip->getChipOriginalMask()->getNumberOfEnabledChannels();
                    uint32_t TOTCHAN = chip->size();
                    LOG(DEBUG) << GREEN << "NCHANNELS " << ENCHAN << " TOTCHAN " << TOTCHAN << RESET;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::CBC3)
                    {
                        cNStripChips += float(ENCHAN) / float(TOTCHAN);
                        cMeanStripsValue += tmpVthr * (float(ENCHAN) / float(TOTCHAN));
                        LOG(DEBUG) << "MeanStripsValue : " << +cMeanStripsValue << " -- NStripChips : " << +cNStripChips << RESET;
                    }
                    else if(cType == FrontEndType::MPA)
                    {
                        cNPixelChips += float(ENCHAN) / float(TOTCHAN);
                        cMeanPixelsValue += tmpVthr * (float(ENCHAN) / float(TOTCHAN));
                    }

                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on board - end

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.fillVplusPlots(theVcthContainer);
#else
    auto theVCthStream = prepareHybridContainerStreamer<EmptyContainer, uint16_t, EmptyContainer>();
    for(auto board: theVcthContainer)
    {
        if(fDQMStreamerEnabled) theVCthStream->streamAndSendBoard(board, fDQMStreamer);
    }
#endif

    fStripTargetVcth = uint16_t(cMeanStripsValue / cNStripChips);
    fPixelTargetVcth = uint16_t(cMeanPixelsValue / cNPixelChips);
    if(fUseMean)
    {
        if(fWithCBC || fWithSSA) LOG(INFO) << BOLDBLUE << "Mean VCth value of all strip chips is " << fStripTargetVcth << " - using as TargetVcth value for all strip chips!" << RESET;
        if(fWithMPA) LOG(INFO) << BOLDBLUE << "Mean VCth value of all pixel chips is " << fPixelTargetVcth << " - using as TargetVcth value for all pixel chips!" << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();
                        if(cType == FrontEndType::SSA || cType == FrontEndType::CBC3) { fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fStripTargetVcth); }
                        else if(cType == FrontEndType::MPA)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fPixelTargetVcth);
                        }
                    }
                }
            }
        }
    }
    this->SetTestAllChannels(originalAllChannelFlag);
    setNormalization(cNormalizationOrig);
}

void PedestalEqualization::FindOffsets()
{
    // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    uint8_t cNormalize         = 1;
    LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    setNormalization(cNormalize);

    float cOccupancyAtPedestal = fOccupancyAtPedestal;
    LOG(INFO) << BOLDBLUE << "Finding offsets..." << RESET;
    // just to be sure, configure the correct VCth and VPlus values

    uint32_t NCH = NCHANNELS;
    if(fUseMean)
    {
        if(fWithCBC || fWithSSA) LOG(INFO) << BOLDBLUE << "Mean VCth value of all strip chips is " << fStripTargetVcth << " - using as TargetVcth value for all strip chips!" << RESET;
        if(fWithMPA) LOG(INFO) << BOLDBLUE << "Mean VCth value of all pixel chips is " << fPixelTargetVcth << " - using as TargetVcth value for all pixel chips!" << RESET;
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();
                        if(cType == FrontEndType::SSA || cType == FrontEndType::CBC3) { fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fStripTargetVcth); }
                        else if(cType == FrontEndType::MPA)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fPixelTargetVcth);
                        }
                    }
                }
            }
        }
    }
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    if(fWithCBC) this->bitWiseScan("ChannelOffset", fEventsPerPoint, cOccupancyAtPedestal, fNEventsPerBurst);
    if(fWithSSA or fWithMPA) this->bitWiseScan("ThresholdTrim", fEventsPerPoint, cOccupancyAtPedestal, fNEventsPerBurst);
    dumpConfigFiles();
    DetectorDataContainer theOffsetsCointainer;
    ContainerFactory::copyAndInitChannel<uint8_t>(*fDetectorContainer, theOffsetsCointainer);

    for(auto board: theOffsetsCointainer) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                for(auto chip: *hybrid) // for on chip - begin
                {
                    // if(fDisableStubLogic and fWithCBC)
                    // {
                    //     ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));

                    //     uint8_t stubLogicValue = fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                    //     fReadoutChipInterface->WriteChipReg(theChip, "Pipe&StubInpSel&Ptwidth", stubLogicValue);

                    //     uint8_t HIPCountValue = fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                    //     fReadoutChipInterface->WriteChipReg(theChip, "HIP&TestMode", HIPCountValue);
                    // }

                    unsigned int channelNumber = 1;
                    int          cMeanOffset   = 0;
                    ReadoutChip* roc           = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));
                    auto         cType         = roc->getFrontEndType();
                    for(auto& channel: *chip->getChannelContainer<uint8_t>()) // for on channel - begin
                    {
                        char charRegName[20];
                        if(cType == FrontEndType::CBC3) sprintf(charRegName, "Channel%03d", channelNumber++);
                        if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2) sprintf(charRegName, "THTRIMMING_S%d", channelNumber++);
                        if(cType == FrontEndType::MPA) sprintf(charRegName, "TrimDAC_P%d", channelNumber++);
                        std::string cRegName = charRegName;
                        channel              = roc->getReg(cRegName);
                        LOG(DEBUG) << BOLDGREEN << "Offset set to " << +channel << RESET;
                        cMeanOffset += channel;
                    }
                    if(cType == FrontEndType::MPA) NCH = NMPACHANNELS;
                    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2) NCH = NSSACHANNELS;

                    LOG(INFO) << BOLDRED << "Mean offset on Chip" << +chip->getId() << " is : " << (cMeanOffset) / (double)NCH << " Vcth units." << RESET;
                } // for on chip - end
            }     // for on hybrid - end
        }         // for on opticalGroup - end
    }             // for on board - end
#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.fillOccupancyPlots(theOccupancyContainer);
    fDQMHistogramPedestalEqualization.fillOffsetPlots(theOffsetsCointainer);
#else
    auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();
    for(auto board: theOccupancyContainer)
    {
        if(fDQMStreamerEnabled) theOccupancyStream->streamAndSendBoard(board, fDQMStreamer);
    }

    auto theOffsetStream = prepareChannelContainerStreamer<uint8_t>();
    for(auto board: theOffsetsCointainer)
    {
        if(fDQMStreamerEnabled) theOffsetStream->streamAndSendBoard(board, fDQMStreamer);
    }
#endif

    setNormalization(cNormalizationOrig);
}

void PedestalEqualization::writeObjects()
{
    this->SaveResults();
#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.process();
#endif
}

// State machine control functions

void PedestalEqualization::ConfigureCalibration() { CreateResultDirectory("Results/Run_PedestalEqualization"); }

void PedestalEqualization::Running()
{
    LOG(INFO) << "Starting Pedestal Equalization";
    Initialise(true, true);
    FindVplus();
    FindOffsets();
    LOG(INFO) << "Done with Pedestal Equalization";
    Reset();
}

void PedestalEqualization::Stop()
{
    LOG(INFO) << "Stopping Pedestal Equalization.";
    writeObjects();
    dumpConfigFiles();
    closeFileHandler();
    LOG(INFO) << "Pedestal Equalization stopped.";
    Reset();
}

void PedestalEqualization::Pause() {}

void PedestalEqualization::Resume() {}
