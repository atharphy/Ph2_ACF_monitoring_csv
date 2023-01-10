#include "PedeNoise.h"
#include "../HWDescription/Cbc.h"
#include "../HWDescription/SSA.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerStream.h"
#include "../Utils/EmptyContainer.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"
#include "../Utils/ThresholdAndNoise.h"
#include "boost/format.hpp"
#include <math.h>

#ifdef __USE_ROOT__
// static_assert(false,"use root is defined");
#include "../DQMUtils/DQMHistogramPedeNoise.h"
#endif

PedeNoise::PedeNoise() : Tool() {}

PedeNoise::~PedeNoise() { clearDataMembers(); }

void PedeNoise::cleanContainerVector()
{
    for(auto container: fSCurveStripOccupancyMap) fRecycleBin.free(container.second);
    for(auto container: fSCurvePixelOccupancyMap) fRecycleBin.free(container.second);
    fSCurveStripOccupancyMap.clear();
    fSCurvePixelOccupancyMap.clear();
}

void PedeNoise::clearDataMembers()
{
    return;
    // delete fBoardRegContainer;
    delete fThresholdAndNoiseContainer;
    if(fDisableStubLogic)
    {
        delete fStubLogicValue;
        delete fHIPCountValue;
    }
    cleanContainerVector();
}

void PedeNoise::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoardRegMap cRegMap      = cBoard->getBeBoardRegMap();
        uint32_t      cTriggerFreq = cRegMap["fc7_daq_cnfg.fast_command_block.user_trigger_frequency"];

        std::vector<std::pair<std::string, uint32_t>> cRegVec;
        cRegVec.clear();
        cRegVec.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", cTriggerFreq});
        cRegVec.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});
        fBeBoardInterface->WriteBoardMultReg(cBoard, cRegVec);
        LOG(INFO) << BOLDYELLOW << "Noise measured on BeBoard#" << +cBoard->getId() << " with a trigger rate of " << cTriggerFreq << "kHz." << RESET;
    }
    fDisableStubLogic = pDisableStubLogic;

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
        fWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }
    if(fWithCBC) LOG(INFO) << BOLDBLUE << "PedeNoise with CBCs" << RESET;
    if(fWithSSA && !fWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs" << RESET;
    if(fWithMPA && !fWithSSA) LOG(INFO) << BOLDBLUE << "PedeNoise with MPAs" << RESET;
    if(fWithSSA && fWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs+MPAs" << RESET;

    for(auto cFrontEndType: cAllFrontEndTypes)
    {
        if(cFrontEndType == FrontEndType::CBC3)
        {
            CBCChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler);
        }
        else if(cFrontEndType == FrontEndType::SSA || cFrontEndType == FrontEndType::SSA2)
        {
            SSAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
        else if(cFrontEndType == FrontEndType::MPA || cFrontEndType == FrontEndType::MPA2)
        {
            MPAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
    }

    initializeRecycleBin();

    fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fPlotSCurves                 = findValueInSettings<double>("PlotSCurves", 0);
    fFitSCurves                  = findValueInSettings<double>("FitSCurves", 0);
    fPulseAmplitude              = findValueInSettings<double>("PedeNoisePulseAmplitude", 0);
    fPulseAmplitudePix           = findValueInSettings<double>("PedeNoisePulseAmplitudePix", fPulseAmplitude);
    std::cout << +fPulseAmplitudePix << std::endl;
    fPedeNoiseLimit          = findValueInSettings<double>("PedeNoiseLimit", 10);
    fPedeNoiseMask           = findValueInSettings<double>("PedeNoiseMask", 0);
    fPedeNoiseMaskUntrimmed  = findValueInSettings<double>("PedeNoiseMaskUntrimmed", 0);
    fPedeNoiseUntrimmedLimit = findValueInSettings<double>("PedeNoiseUntrimmedLimit", 0.0);
    fEventsPerPoint          = findValueInSettings<double>("Nevents", 10);
    fUseFixRange             = findValueInSettings<double>("PedeNoiseUseFixRange", 0);
    fMinThreshold            = findValueInSettings<double>("PedeNoiseMinThreshold", 0);
    fMaxThreshold            = findValueInSettings<double>("PedeNoiseMaxThreshold", 0);
    fNeventsForValidation    = findValueInSettings<double>("NeventsForValidation", 10000);
    fMaskingThreshold        = findValueInSettings<double>("MaskingThreshold", 0.001);
    // if you forget to use the PedeNoiseUseFixRange setting but instead declare
    // min and max threshold ... will still work
    if(!fUseFixRange && fMinThreshold != fMaxThreshold) { fUseFixRange = true; }

    fNEventsPerBurst = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : fEventsPerPoint;
    // uint8_t cEnableFastCounterReadout = (uint8_t)findValueInSettings<double>("EnableFastCounterReadout", 0);
    // uint8_t cEnablePairSelect         = (uint8_t)findValueInSettings<double>("EnablePairSelect", 0);
    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;
    // LOG(INFO) << " Fast Counter Readout [PS] " << +cEnableFastCounterReadout << RESET;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);
    if(fFitSCurves) fPlotSCurves = true;

    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegNap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
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

    // for now.. force to use async mode here
    bool cForcePSasync = true;
    // event types
    fEventTypes.clear();
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
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PedeNoise::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by PedeNoise" << RESET;
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
                LOG(DEBUG) << BOLDBLUE << "PedeNoise::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;

                for(auto cChip: *cHybrid)
                {
                    auto cModMap = cChip->GetModifiedRegisterMap();
                    LOG(DEBUG) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    std::vector<std::pair<std::string, uint16_t>> cRegList;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        if(cMapItem.second.fValue == cValueInMemory) continue;
                        // don't reconfigure the offsets .. whole point of this excercise
                        if(cMapItem.first.find("VCth") != std::string::npos) continue;
                        if(cMapItem.first.find("ThDAC") != std::string::npos) continue;
                        if(cMapItem.first.find("Bias_THDAC") != std::string::npos) continue;
                        if(cMapItem.first.find("ENFLAGS") != std::string::npos) { cMapItem.second.fValue = (cMapItem.second.fValue & 0xfe) + (cValueInMemory & 0x1); };
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
}
void PedeNoise::disableStubLogic()
{
    fStubLogicValue = new DetectorDataContainer();
    fHIPCountValue  = new DetectorDataContainer();
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *fStubLogicValue);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, *fHIPCountValue);

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - thus disabling Stub logic for pedestal and noise measurement." << RESET;
                        static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(cChip, false, true, 0);
                        fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cChip), "Pipe&StubInpSel&Ptwidth");
                        fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cChip), "HIP&TestMode");
                        // fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), "Pipe&StubInpSel&Ptwidth", 0x23);
                        // fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cChip), "HIP&TestMode", 0x00);
                    }
                }
            }
        }
    }
}

void PedeNoise::reloadStubLogic()
{
    // re-enable stub logic

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    RegisterVector cRegVec;
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - re-enabling stub logic to original value!" << RESET;
                        cRegVec.push_back({"Pipe&StubInpSel&Ptwidth",
                                           fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()});
                        cRegVec.push_back(
                            {"HIP&TestMode", fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<uint16_t>()});
                        fReadoutChipInterface->WriteChipMultReg(cChip, cRegVec);
                    }
                }
            }
        }
    }
}

void PedeNoise::sweepSCurves()
{
    bool originalAllChannelFlag = this->fAllChan;

    if(fPulseAmplitude != 0 && originalAllChannelFlag && fWithCBC)
    {
        this->SetTestAllChannels(false);
        LOG(INFO) << RED << "Cannot inject pulse for all channels, test in groups enabled. " << RESET;
    }

    // configure TP amplitude
    for(auto cBoard: *fDetectorContainer)
    {
        if(fWithSSA || fWithMPA)
        {
            // Allow for different SSA and MPA injection amplitudes
            // setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fTestPulseAmplitude);
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();

                        if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitudePix);
                        else
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitude);
                    }
                }
            }
        }

        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fPulseAmplitude);
    }

    bool forceAllChannels = false;
    if(fPulseAmplitude != 0)
    {
        LOG(INFO) << BOLDYELLOW << "Enabled test pulse. " << RESET;
        this->enableTestPulse(true);
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "sweepSCurves without TP injection" << RESET;
        this->enableTestPulse(false);
        forceAllChannels = true;
    }

    uint16_t cStripStartValue = 0, cPixelStartValue = 0;
    if(!fUseFixRange)
    {
        this->findPedestal(forceAllChannels);
        cStripStartValue = fMeanStrips;
        cPixelStartValue = fMeanPixels;
    }
    else
    {
        cStripStartValue = (fMaxThreshold + fMinThreshold) / 2.;
        cPixelStartValue = (fMaxThreshold + fMinThreshold) / 2.;
    }
    if(fDisableStubLogic) disableStubLogic();
    if(fWithCBC || fWithSSA) LOG(INFO) << BLUE << "Sweep of Strip S-curves will start at an average threshold of " << cStripStartValue << RESET;
    if(fWithMPA) LOG(INFO) << MAGENTA << "Sweep of Pixel S-curves will start at an average threshold of " << cPixelStartValue << RESET;

    measureSCurves(cStripStartValue, cPixelStartValue);
    // if(fDisableStubLogic) reloadStubLogic();
    this->SetTestAllChannels(originalAllChannelFlag);
    LOG(INFO) << BOLDBLUE << "Finished sweeping SCurves..." << RESET;
    return;
}

void PedeNoise::measureNoise()
{
    LOG(INFO) << BOLDBLUE << "sweepSCurves" << RESET;
    sweepSCurves();
    LOG(INFO) << BOLDBLUE << "extractPedeNoise" << RESET;
    extractPedeNoise();
    LOG(INFO) << BOLDBLUE << "producePedeNoisePlots" << RESET;
    producePedeNoisePlots();
    LOG(INFO) << BOLDBLUE << "Done" << RESET;
}

void PedeNoise::Validate()
{
    LOG(INFO) << "Validation: Taking Data with " << fNeventsForValidation << " random triggers!";

    for(auto cBoard: *fDetectorContainer)
    {
        // increase threshold to supress noise
        setThresholdtoNSigma(cBoard, 5);
    }

    for(auto board: *fThresholdAndNoiseContainer) { maskNoisyChannels(board); }

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    bool originalAllChannelFlag = this->fAllChan;

    LOG(INFO) << "Setting all channels";
    this->SetTestAllChannels(true);
    LOG(INFO) << "measuring with " << fNeventsForValidation << " events and " << fNEventsPerBurst << " per burst";
    this->measureData(fNeventsForValidation, fNEventsPerBurst);
    LOG(INFO) << "setting al channels v2";
    this->SetTestAllChannels(originalAllChannelFlag);
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.fillValidationPlots(theOccupancyContainer);
#else
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    auto theOccupancyStream = prepareHybridContainerStreamer<Occupancy, Occupancy, Occupancy>();
    // auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();

    for(auto board: theOccupancyContainer)
    {
        if(fDQMStreamerEnabled) theOccupancyStream->streamAndSendBoard(board, fDQMStreamer);
    }
#endif
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // std::cout << __PRETTY_FUNCTION__ << " The Hybrid Occupancy = " <<
                // theOccupancyContainer.at(cBoard->getIndex())->at(cHybrid->getIndex())->getSummary<Occupancy,Occupancy>().fOccupancy
                // << std::endl;

                for(auto cChip: *cHybrid)
                {
                    auto           cType = cChip->getFrontEndType();
                    RegisterVector cRegVec;
                    uint32_t       NCH = NCHANNELS;
                    if(cType == FrontEndType::CBC3)
                        NCH = NCHANNELS;
                    else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                        NCH = NSSACHANNELS;
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                        NCH = NMPACHANNELS;
                    //
                    for(uint32_t iChan = 0; iChan < NCH; iChan++)
                    {
                        // LOG (INFO) << RED << "Ch " << iChan << RESET ;
                        float occupancy =
                            theOccupancyContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(iChan).fOccupancy;
                        if(occupancy > fMaskingThreshold)
                        {
                            std::string message = "Found a noisy channel on Chip " + getReadoutChipString(cBoard->getId(), cOpticalGroup->getId(), cHybrid->getId(), cChip->getId()) + " Channel " +
                                                  std::to_string(iChan) + " with an occupancy of " + std::to_string(occupancy) + "(>" + std::to_string(fMaskingThreshold) + ")";
                            if(fMaskNoisyChannels)
                            {
                                if(fWithCBC)
                                {
                                    // char cRegName[11];
                                    // sprintf(cRegName, "Channel%03d", iChan + 1);
                                    std::string cRegName = "Channel" + (boost::format("%|03|") % (iChan + 1)).str();
                                    cRegVec.push_back({cRegName, 0xFF});
                                }
                                if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                                {
                                    // char cRegName[17];
                                    // sprintf(cRegName, "THTRIMMING_S%03d", iChan + 1);
                                    std::string cRegName = "THTRIMMING_S" + (boost::format("%|03|") % (iChan + 1)).str();
                                    cRegVec.push_back({cRegName, 0x1F});
                                }
                                if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2)
                                {
                                    // char cRegName[12];
                                    // sprintf(cRegName, "TrimDAC_P%04d", iChan + 1);
                                    std::string cRegName = "TrimDAC_P" + (boost::format("%|04|") % (iChan + 1)).str();
                                    cRegVec.push_back({cRegName, 0x1F});
                                }
                                message += ";  setting offset to 255";
                            }
                            LOG(DEBUG) << RED << message << RESET;
                        }
                    }

                    fReadoutChipInterface->WriteChipMultReg(cChip, cRegVec);
                }
            }
        }
    }
}

void PedeNoise::findPedestal(bool forceAllChannels)
{
    bool originalAllChannelFlag = this->fAllChan;
    if(forceAllChannels) this->SetTestAllChannels(true);

    // // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    // uint8_t cNormalize         = 0;
    // if(fWithCBC or (fWithSSA && !fWithMPA) or (fWithMPA && !fWithSSA)) { cNormalize = 1; }
    // LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    // setNormalization(cNormalize);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    this->bitWiseScan("Threshold", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(forceAllChannels) this->SetTestAllChannels(originalAllChannelFlag);

    uint8_t cNStripChips = 0, cNPixelChips = 0;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    uint16_t tmpVthr = 0;
                    if(cChip->getFrontEndType() == FrontEndType::CBC3)
                    {
                        tmpVthr = (static_cast<ReadoutChip*>(cChip)->getReg("VCth1") + (static_cast<ReadoutChip*>(cChip)->getReg("VCth2") << 8));
                        fMeanStrips += tmpVthr;
                        cNStripChips++;
                    }
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        tmpVthr = static_cast<ReadoutChip*>(cChip)->getReg("Bias_THDAC");
                        fMeanStrips += tmpVthr;
                        cNStripChips++;
                    }
                    if(cChip->getFrontEndType() == FrontEndType::MPA || cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        tmpVthr = static_cast<ReadoutChip*>(cChip)->getReg("ThDAC0");
                        fMeanPixels += tmpVthr;
                        cNPixelChips++;
                    }
                }
            }
        }
    }
    fMeanStrips = (cNStripChips > 0) ? fMeanStrips / cNStripChips : 0xFF / 2;
    fMeanPixels = (cNPixelChips > 0) ? fMeanPixels / cNPixelChips : 0xFF / 2;
    if(fWithCBC || fWithSSA) LOG(INFO) << BOLDBLUE << "Found Pedestals on Strip ASICs to be around " << fMeanStrips << RESET;
    if(fWithMPA) LOG(INFO) << BOLDMAGENTA << "Found Pedestals on Pixel ASICs to be around " << fMeanPixels << RESET;
    setNormalization(cNormalizationOrig);
}

void PedeNoise::measureSCurves(uint16_t pStripStartValue, uint16_t pPixelStartValue)
{
    auto cChannels     = findValueInSettings<double>("NoiseMeasurementLimit", 1);
    auto cLowerLimitTh = findValueInSettings<double>("PedeNoiseMinThreshold", 0);
    auto cUpperLimitTh = findValueInSettings<double>("PedeNoiseMaxThreshold", 0);
    if(fUseFixRange) LOG(INFO) << BOLDYELLOW << "Scan should be between " << cLowerLimitTh << " and " << cUpperLimitTh << " DAC units" << RESET;

    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    float    cMaxOccupancy  = 1.0;
    float    cLimit         = cChannels / (100.);
    int      cMinBreakCount = 10;
    uint16_t cStripValue    = pStripStartValue;
    uint16_t cPixelValue    = pPixelStartValue;
    uint16_t cMaxValue      = (1 << 10) - 1;
    if(fWithSSA || fWithMPA) cMaxValue = (1 << 8) - 1;
    float              cFirstLimit = (fWithCBC) ? 0 : 1;
    std::vector<int>   cSigns{-1, 1};
    std::vector<float> cLimits{cFirstLimit, 1 - cFirstLimit};

    int cCounter = 0;
    for(auto cSign: cSigns)
    {
        bool cStripFirstLim = false, cPixelFirstLim = false;
        bool cLimitFound = false, cStripLimitFound = false, cPixelLimitFound = false;
        int  cStripLimitCounter = 0, cPixelLimitCounter = 0;
        do
        {
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurvePixelOccupancyMap[cPixelValue]        = theOccupancyContainer;
            fSCurveStripOccupancyMap[cStripValue]        = theOccupancyContainer;

            for(auto cBoard: *fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            auto cType = cChip->getFrontEndType();
                            if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cStripValue);
                            else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cPixelValue);
                        }
                    }
                }
            }
            this->measureData(fEventsPerPoint, fNEventsPerBurst);

            // Retrieve occupancy for strip and pixel chips
            theOccupancyContainer->normalizeAndAverageContainers(fDetectorContainer, getChannelGroupHandlerContainer(), fEventsPerPoint);
            float   cStripGlobalOccupancy = 0, cPixelGlobalOccupancy = 0;
            uint8_t cNStripChips = 0, cNPixelChips = 0;
            for(auto cBoard: *fDetectorContainer)
            {
                auto cBoardIdx = cBoard->getIndex();
                for(auto cOpticalGroup: *cBoard)
                {
                    auto cOpticalGroupIdx = cOpticalGroup->getIndex();
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto cHybridIdx = cHybrid->getIndex();
                        for(auto cChip: *cHybrid)
                        {
                            auto cChipIdx       = cChip->getIndex();
                            auto cType          = cChip->getFrontEndType();
                            auto cChipOccupancy = theOccupancyContainer->at(cBoardIdx)->at(cOpticalGroupIdx)->at(cHybridIdx)->at(cChipIdx)->getSummary<Occupancy, Occupancy>().fOccupancy;
                            if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                            {
                                cNStripChips++;
                                cStripGlobalOccupancy += cChipOccupancy;
                            }
                            else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                            {
                                cNPixelChips++;
                                cPixelGlobalOccupancy += cChipOccupancy;
                            }
                        }
                    }
                }
            }
            cStripGlobalOccupancy /= cNStripChips;
            cPixelGlobalOccupancy /= cNPixelChips;
#ifdef __USE_ROOT__
            if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cStripValue, cPixelValue, *theOccupancyContainer);
#else
            if(fPlotSCurves)
            {
                auto theSCurveStreamer = prepareChannelContainerStreamer<Occupancy, uint16_t, uint16_t>("SCurve");
                theSCurveStreamer->setHeaderElement<0>(cStripValue);
                theSCurveStreamer->setHeaderElement<1>(cPixelValue);
                for(auto board: *theOccupancyContainer)
                {
                    if(fDQMStreamerEnabled) theSCurveStreamer->streamAndSendBoard(board, fDQMStreamer);
                }
            }
#endif

            auto cStripDistanceFromTarget = std::fabs(std::min(cStripGlobalOccupancy, cMaxOccupancy) - (cLimits[cCounter]));
            auto cPixelDistanceFromTarget = std::fabs(std::min(cPixelGlobalOccupancy, cMaxOccupancy) - (cLimits[cCounter]));

            if(fWithCBC || fWithSSA)
            {
                LOG(INFO) << BOLDBLUE << "Strip Threshold =  " << +cStripValue << " -- Occupancy = " << std::setprecision(2) << std::fixed << +cStripGlobalOccupancy
                          << " -- Distance from target = " << cStripDistanceFromTarget * 100 << "\t..Incrementing limit found counter "
                          << " -- current value is " << +cStripLimitCounter << RESET;
            }
            if(fWithMPA)
            {
                LOG(INFO) << BOLDMAGENTA << "Pixel Threshold =  " << +cPixelValue << " -- Occupancy = " << std::setprecision(2) << std::fixed << +cPixelGlobalOccupancy
                          << " -- Distance from target = " << cPixelDistanceFromTarget * 100 << "\t..Incrementing limit found counter "
                          << " -- current value is " << +cPixelLimitCounter << RESET;
            }

            if(cStripDistanceFromTarget <= cLimit || cStripFirstLim) // || globalOccupancy>1.0)
            {
                cStripFirstLim = true;
                cStripLimitCounter++;
            }
            if(cPixelDistanceFromTarget <= cLimit || cPixelFirstLim) // || globalOccupancy>1.0)
            {
                cPixelFirstLim = true;
                cPixelLimitCounter++;
            }

            if(!cStripLimitFound) cStripValue += cSign;
            if(!cPixelLimitFound) cPixelValue += cSign;
            if(!fUseFixRange)
            {
                if(!fWithMPA && (fWithSSA || fWithCBC))
                {
                    cStripLimitFound = (cStripValue == 0 || cStripValue >= cMaxValue) || (cStripLimitCounter >= cMinBreakCount);
                    cLimitFound      = cStripLimitFound;
                }
                else if(fWithMPA && !fWithSSA)
                {
                    cPixelLimitFound = (cPixelValue == 0 || cPixelValue >= cMaxValue) || (cPixelLimitCounter >= cMinBreakCount);
                    cLimitFound      = cPixelLimitFound;
                }
                else if(fWithSSA && fWithMPA)
                {
                    cStripLimitFound = (cStripValue == 0 || cStripValue >= cMaxValue) || (cStripLimitCounter >= cMinBreakCount);
                    cPixelLimitFound = (cPixelValue == 0 || cPixelValue >= cMaxValue) || (cPixelLimitCounter >= cMinBreakCount);
                    cLimitFound      = cStripLimitFound && cPixelLimitFound;
                }
                if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign during auto scan .." << RESET; }
            }
            else
            {
                cStripLimitFound = (cSign < 0) ? (cStripValue == cLowerLimitTh) : (cStripValue == cUpperLimitTh);
                cPixelLimitFound = (cSign < 0) ? (cPixelValue == cLowerLimitTh) : (cPixelValue == cUpperLimitTh);

                if(!fWithMPA && (fWithSSA || fWithCBC))
                    cLimitFound = cStripLimitFound;
                else if(fWithMPA && !fWithSSA)
                    cLimitFound = cPixelLimitFound;
                else if(fWithSSA && fWithMPA)
                    cLimitFound = cStripLimitFound && cPixelLimitFound;

                if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign because threshold limit was reached .." << RESET; }
            }
        } while(!cLimitFound);
        cCounter++;
        cStripValue = pStripStartValue + cSigns[cCounter];
        cPixelValue = pPixelStartValue + cSigns[cCounter];
    }
    // this->HttpServerProcess();
    LOG(DEBUG) << YELLOW << "Found minimal and maximal occupancy " << cMinBreakCount << " times, SCurves finished! " << RESET;
}
void PedeNoise::extractPedeNoise()
{
    fThresholdAndNoiseContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *fThresholdAndNoiseContainer);

    std::map<uint16_t, DetectorDataContainer*>::reverse_iterator previousStripIterator = fSCurveStripOccupancyMap.rend();
    std::map<uint16_t, DetectorDataContainer*>::reverse_iterator previousPixelIterator = fSCurvePixelOccupancyMap.rend();

    uint16_t counter = 0;

    for(std::map<uint16_t, DetectorDataContainer*>::reverse_iterator mStripIt = fSCurveStripOccupancyMap.rbegin(), mPixelIt = fSCurvePixelOccupancyMap.rbegin();
        mStripIt != fSCurveStripOccupancyMap.rend(), mPixelIt != fSCurvePixelOccupancyMap.rend();
        ++mStripIt, ++mPixelIt)
    {
        if(fWithCBC || (!fWithMPA && fWithSSA))
        {
            if(previousStripIterator == fSCurveStripOccupancyMap.rend())
            {
                previousStripIterator = mStripIt;
                continue;
            }
            if(fSCurveStripOccupancyMap.size() - 1 == counter) break;
        }
        else if(fWithSSA && fWithMPA)
        {
            if(previousStripIterator == fSCurveStripOccupancyMap.rend() && previousPixelIterator == fSCurvePixelOccupancyMap.rend())
            {
                previousStripIterator = mStripIt;
                previousPixelIterator = mPixelIt;
                continue;
            }
            if((fSCurveStripOccupancyMap.size() - 1 == counter) && (fSCurvePixelOccupancyMap.size() - 1 == counter)) break;
        }
        else if(!fWithSSA && fWithMPA)
        {
            if(previousPixelIterator == fSCurvePixelOccupancyMap.rend())
            {
                previousPixelIterator = mPixelIt;
                continue;
            }
            if(fSCurvePixelOccupancyMap.size() - 1 == counter) break;
        }

        for(auto board: *fDetectorContainer)
        {
            for(auto opticalGroup: *board)
            {
                for(auto hybrid: *opticalGroup)
                {
                    for(auto chip: *hybrid)
                    {
                        for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                        {
                            if(!getChannelGroupHandlerContainer()
                                    ->getObject(board->getId())
                                    ->getObject(opticalGroup->getId())
                                    ->getObject(hybrid->getId())
                                    ->getObject(chip->getId())
                                    ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                    ->allChannelGroup()
                                    ->isChannelEnabled(iChannel))
                                continue;

                            float currentOccupancy = 0, previousOccupancy = 0, binCenter = 0;
                            auto  cType = chip->getFrontEndType();
                            if(cType == FrontEndType::CBC3 || cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                            {
                                previousOccupancy = (previousStripIterator)
                                                        ->second->at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getChannel<Occupancy>(iChannel)
                                                        .fOccupancy;
                                currentOccupancy =
                                    mStripIt->second->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getChannel<Occupancy>(iChannel).fOccupancy;
                                binCenter = (mStripIt->first + (previousStripIterator)->first) / 2.;
                            }
                            else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                            {
                                previousOccupancy = (previousPixelIterator)
                                                        ->second->at(board->getIndex())
                                                        ->at(opticalGroup->getIndex())
                                                        ->at(hybrid->getIndex())
                                                        ->at(chip->getIndex())
                                                        ->getChannel<Occupancy>(iChannel)
                                                        .fOccupancy;
                                currentOccupancy =
                                    mPixelIt->second->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getChannel<Occupancy>(iChannel).fOccupancy;
                                binCenter = (mPixelIt->first + (previousPixelIterator)->first) / 2.;
                                if(previousOccupancy > currentOccupancy) { continue; }
                            }

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fThreshold += binCenter * (previousOccupancy - currentOccupancy);

                            // if (iChannel>1800){
                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fNoise += binCenter * binCenter * (previousOccupancy - currentOccupancy); //}

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fThresholdError += previousOccupancy - currentOccupancy;
                        }
                    }
                }
            }
        }
        previousStripIterator = mStripIt;
        previousPixelIterator = mPixelIt;
        ++counter;
    }

    // calculate the averages and ship
    // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    uint8_t cNormalize         = 1;
    LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    setNormalization(cNormalize);

    for(auto board: *fThresholdAndNoiseContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto hybrid: *opticalGroup)
            {
                for(auto chip: *hybrid)
                {
                    for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                    {
                        if(!getChannelGroupHandlerContainer()
                                ->getObject(board->getId())
                                ->getObject(opticalGroup->getId())
                                ->getObject(hybrid->getId())
                                ->getObject(chip->getId())
                                ->getSummary<std::shared_ptr<ChannelGroupHandler>>()
                                ->allChannelGroup()
                                ->isChannelEnabled(iChannel))
                            continue;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise = sqrt(chip->getChannel<ThresholdAndNoise>(iChannel).fNoise - (chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold *
                                                                                                                                            chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold));

                        if(isnan(chip->getChannel<ThresholdAndNoise>(iChannel).fNoise) || isinf(chip->getChannel<ThresholdAndNoise>(iChannel).fNoise))
                        {
                            LOG(WARNING) << BOLDYELLOW << "Problem in deriving noise for Board " << board->getId() << " Optical Group " << opticalGroup->getId() << " Hybrid " << hybrid->getId()
                                         << " ReadoutChip " << chip->getId() << " Channel " << iChannel << ", forcing it to 0." << RESET;
                            chip->getChannel<ThresholdAndNoise>(iChannel).fNoise = 0.;
                        }
                        if(isnan(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold) || isinf(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold))
                        {
                            LOG(WARNING) << BOLDYELLOW << "Problem in deriving threshold for Board " << board->getId() << " Optical Group " << opticalGroup->getId() << " Hybrid " << hybrid->getId()
                                         << " ReadoutChip " << chip->getId() << " Channel " << iChannel << ", forcing it to 0." << RESET;
                            chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold = 0.;
                        }
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError = 1;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoiseError     = 1;
                    }
                }
            }
        }
        if(cNormalize == 0)
        {
            // auto& cDataContainerThisBrd = fDetectorDataContainer->at(cBoard->getIndex());
            // for(auto cOpticalGroup: *cBoard)
            // {
            //     auto& cDataContainerThisOG = cDataContainerThisBrd->at(cOpticalGroup->getIndex());
            //     for(auto cHybrid: *cOpticalGroup)
            //     {
            //         auto& cDataContainerThisHybrid = cDataContainerThisOG->at(cHybrid->getIndex());
            //         for(auto cChip: *cHybrid)
            //         {
            //             auto& cDataContainerThisChip = cDataContainerThisHybrid->at(cChip->getIndex());
            //             auto& cSummary = cDataContainerThisChip->getSummary<Occupancy,Occupancy>();
            //             ChannelGroupHandler* cHandler;
            //             if( cChip->getFrontEndType() == FrontEndType::MPA ) cHandler = new MPAChannelGroupHandler();
            //             else cHandler = new SSAChannelGroupHandler();
            //             LOG (DEBUG) << BOLDYELLOW << " Normalizing assuming " << +getNReadbackEvents()
            //                 << " events and " << cHandler->allChannelGroup()->getNumberOfEnabledChannels() << " enabled channels." << RESET;
            //             cSummary.fOccupancy = 0;
            //             for( uint16_t cChnl=0; cChnl < cDataContainerThisChip->size(); cChnl++)
            //             {
            //                 uint32_t cRow = cChnl%cHandler->allChannelGroup()->getNumberOfRows();
            //                 uint32_t cCol;
            //                 if( cHandler->allChannelGroup()->getNumberOfCols() == 0 ) cCol = 0;
            //                 else  cCol = cChnl/cHandler->allChannelGroup()->getNumberOfRows();
            //                 if(cHandler->allChannelGroup()->isChannelEnabled(cRow, cCol ))
            //                 {
            //                     cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy /= getNReadbackEvents();
            //                     cGlbOcc += cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy;
            //                     if( cChnl < 10 || cChnl > 15*120 + 110 ) LOG (DEBUG) << BOLDBLUE << cChnl << " [ " << cRow << " , " << cCol << " ] " <<
            //                     cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy << RESET; cSummary.fOccupancy+= cDataContainerThisChip->getChannel<Occupancy>(cRow,
            //                     cCol).fOccupancy; cNormGlblOcc++;
            //                 }
            //             }
            //             cSummary.fOccupancy = std::min(cMaxOccupancy, cSummary.fOccupancy/cHandler->allChannelGroup()->getNumberOfEnabledChannels());
            //         }
            //     }
            // }
        }
        else
            board->normalizeAndAverageContainers(fDetectorContainer->at(board->getIndex()), getChannelGroupHandlerContainer()->getObject(board->getId()), 0);
    }
    setNormalization(cNormalizationOrig);
}

void PedeNoise::producePedeNoisePlots()
{
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.fillPedestalAndNoisePlots(*fThresholdAndNoiseContainer);

    // Storing noise and pedestal average and RMS values on the summaryTree
    for(auto board: *fThresholdAndNoiseContainer)
    {
        for(auto opticalGroup: *board)
        {
            for(auto module: *opticalGroup)
            {
                for(auto chip: *module)
                {
                    fillSummaryTree("AvgNoiseSSA" + std::to_string(chip->getId()), chip->getSummary<ThresholdAndNoise>().fNoise);          // For GUI summaryTree
                    fillSummaryTree("StDvNoiseSSA" + std::to_string(chip->getId()), chip->getSummary<ThresholdAndNoise>().fNoiseError);    // For GUI summaryTree
                    fillSummaryTree("AvgPedeSSA" + std::to_string(chip->getId()), chip->getSummary<ThresholdAndNoise>().fThreshold);       // For GUI summaryTree
                    fillSummaryTree("StDvPedeSSA" + std::to_string(chip->getId()), chip->getSummary<ThresholdAndNoise>().fThresholdError); // For GUI summaryTree
                }
            }
        }
    }

#else
    auto theThresholdAndNoiseStream = prepareChannelContainerStreamer<ThresholdAndNoise>();
    for(auto board: *fThresholdAndNoiseContainer)
    {
        if(fDQMStreamerEnabled) { theThresholdAndNoiseStream->streamAndSendBoard(board, fDQMStreamer); }
    }
#endif
}

void PedeNoise::setThresholdtoNSigma(BoardContainer* board, float pNSigma)
{
    for(auto opticalGroup: *board)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                uint32_t cChipId = chip->getId();

                float cPedestal = fThresholdAndNoiseContainer->at(board->getIndex())
                                      ->at(opticalGroup->getIndex())
                                      ->at(hybrid->getIndex())
                                      ->at(chip->getIndex())
                                      ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                      .fThreshold;
                float cNoise = fThresholdAndNoiseContainer->at(board->getIndex())
                                   ->at(opticalGroup->getIndex())
                                   ->at(hybrid->getIndex())
                                   ->at(chip->getIndex())
                                   ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                   .fNoise;

                int      cDiff                  = -pNSigma * cNoise;
                uint16_t cThresholdWorkingPoint = round(cPedestal + cDiff);

                if(pNSigma > 0)
                    LOG(INFO) << "Changing Threshold on Chip " << +cChipId << " by " << cDiff << " to " << +cThresholdWorkingPoint << " VCth units to supress noise!"
                              << " NOISE IS " << cNoise << " SIGMA " << pNSigma;
                else
                {
                    LOG(INFO) << "Changing Threshold on Chip " << +cChipId << " back to the pedestal at " << +cPedestal;
                    cThresholdWorkingPoint = cPedestal;
                }
                fReadoutChipInterface->WriteChipReg(chip, "Threshold", cThresholdWorkingPoint);

                ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cThresholdWorkingPoint);
                static_cast<ReadoutChip*>(chip)->accept(cThresholdVisitor);
            }
        }
    }
}

void PedeNoise::maskNoisyChannels(BoardDataContainer* board)
{
    for(auto opticalGroup: *board)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                LOG(INFO) << BOLDYELLOW << chip->getId() << RESET;
                auto chipDC = fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex());
                // auto cType = chipDC->getFrontEndType();

                // uint32_t       NCH = NCHANNELS;
                // if(cType == FrontEndType::CBC3)
                //   NCH = NCHANNELS;
                // else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                //  NCH = NSSACHANNELS;
                // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                //  NCH = NMPACHANNELS;

                /*float fMean=1.0;
                        if(cType == FrontEndType::MPA or  cType == FrontEndType::MPA2)
                     fMean=2.7;
                        if(cType == FrontEndType::SSA or  cType == FrontEndType::SSA2)
                     fMean=4.2;*/
                auto     cOriginalMask = chipDC->getChipOriginalMask();
                uint32_t nMask         = 0;
                for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                {
                    float cPedestal = chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fThreshold;
                    // float cNoise = chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fNoise;
                    // LOG(INFO) << BOLDYELLOW << "CHECK "<<iChannel <<", "<<chip->getChannel<ThresholdAndNoise>(iChannel).fNoise<<" "<<fPedeNoiseLimit*fMean<<RESET;
                    // LOG(INFO) << BOLDYELLOW << "CHECK "<<iChannel <<", "<<std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold -cPedestal)<<" "<<fPedeNoiseUntrimmedLimit<<RESET;
                    if(fPedeNoiseMask and (chip->getChannel<ThresholdAndNoise>(iChannel).fNoise > fPedeNoiseLimit))
                    {
                        nMask += 1;
                        LOG(INFO) << BOLDYELLOW << "Masking Channel: " << iChannel << " with a noise of " << chip->getChannel<ThresholdAndNoise>(iChannel).fNoise << ", which is over the limit of "
                                  << fPedeNoiseLimit << RESET;
                        cOriginalMask->disableChannel(iChannel);
                    }
                    if(fPedeNoiseMaskUntrimmed and std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold - cPedestal) > fPedeNoiseUntrimmedLimit)
                    {
                        uint8_t thetrim = fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(chipDC), "TrimDAC_P" + std::to_string(iChannel + 1));

                        nMask += 1;
                        LOG(INFO) << BOLDYELLOW << "Masking Channel:  " << iChannel << " with a pedestal difference of "
                                  << std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold - cPedestal) << ", which is over the limit of " << fPedeNoiseUntrimmedLimit
                                  << " trimval: " << +thetrim << RESET;
                        cOriginalMask->disableChannel(iChannel);
                    }

                    // LOG(INFO) << BOLDYELLOW << "snorp SUMMARY TH "<<cPedestal <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "snorp SUMMARY NOI "<<cNoise <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "fPedeNoiseLimit "<<fPedeNoiseLimit<< " fPedeNoiseMask "<<fPedeNoiseMask <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "Noise "<<iChannel<< ": "<<chip->getChannel<ThresholdAndNoise>(iChannel).fNoise <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "Thresh "<<iChannel<< ": "<<chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold  <<RESET;
                }
                // fReadoutChipInterface->maskChannelGroup(chipDC,cOriginalMask);
                if(nMask > 0) LOG(INFO) << BOLDYELLOW << "PedeNoise masked " << nMask << " channels..." << RESET;
                fReadoutChipInterface->ConfigureChipOriginalMask(chipDC);
            }
        }
    }
}

void PedeNoise::writeObjects()
{
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.process();
#endif
}

void PedeNoise::ConfigureCalibration() {}

void PedeNoise::Running()
{
    LOG(INFO) << "Starting noise measurement";
    Initialise(true, true);
    // auto myFunction = [](const Ph2_HwDescription::ReadoutChip *theChip){
    //     std::cout<<"Using it"<<std::endl;
    //     return (theChip->getId()==0);
    //     };
    // HybridContainer::SetQueryFunction(myFunction);
    measureNoise();
    // HybridContainer::ResetQueryFunction();
    // Validate();
    LOG(INFO) << "Done with noise";
    Reset();
}

void PedeNoise::Stop()
{
    LOG(INFO) << "Stopping noise measurement";
    Reset();
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    clearDataMembers();
    LOG(INFO) << "Noise measurement stopped.";
}

void PedeNoise::Pause() {}

void PedeNoise::Resume() {}
