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

void PedeNoise::cleanContainerMap()
{
    for(auto container: fSCurveOccupancyMap) fRecycleBin.free(container.second);
    fSCurveOccupancyMap.clear();
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
    cleanContainerMap();
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

    cWithCBC = false;
    cWithSSA = false;
    cWithMPA = false;
    std::vector<FrontEndType> cAllFrontEndTypes;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        cWithCBC            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::CBC3) != cFrontEndTypes.end();
        cWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        cWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }
    if(cWithCBC) LOG(INFO) << BOLDBLUE << "PedeNoise with CBCs" << RESET;
    if(cWithSSA && !cWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs" << RESET;
    if(cWithMPA && !cWithSSA) LOG(INFO) << BOLDBLUE << "PedeNoise with MPAs" << RESET;
    if(cWithSSA && cWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs+MPAs" << RESET;

    if(cWithCBC)
    {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
    }
    if(cWithSSA)
    {
        SSAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        // temporary
        setChannelGroupHandler(theChannelGroupHandler, cAllFrontEndTypes);
    }
    if(cWithMPA)
    {
        MPAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, cAllFrontEndTypes);
        // setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA);
        // setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA2);
    }
    initializeRecycleBin();

    fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fPlotSCurves                 = findValueInSettings<double>("PlotSCurves", 0);
    fFitSCurves                  = findValueInSettings<double>("FitSCurves", 0);
    fPulseAmplitude              = findValueInSettings<double>("PedeNoisePulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings<double>("Nevents", 10);
    fUseFixRange                 = findValueInSettings<double>("PedeNoiseUseFixRange", 0);
    fMinThreshold                = findValueInSettings<double>("PedeNoiseMinThreshold", 0);
    fMaxThreshold                = findValueInSettings<double>("PedeNoiseMaxThreshold", 0);
    // if you forget to use the PedeNoiseUseFixRange setting but instead declare
    // min and max threshold ... will still work
    if(!fUseFixRange && fMinThreshold != fMaxThreshold) { fUseFixRange = true; }

    fNEventsPerBurst = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : 0xffff;
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
        if(!cWithSSA && !cWithMPA) continue;
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
                LOG(INFO) << BOLDBLUE << "PedeNoise::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;

                for(auto cChip: *cHybrid)
                {
                    auto cModMap = cChip->GetModifiedRegisterMap();
                    LOG(INFO) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    std::vector<std::pair<std::string, uint16_t>> cRegList;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        // don't reconfigure the offsets .. whole point of this excercise
                        if(cMapItem.first.find("VCth") != std::string::npos) continue;
                        if(cMapItem.first.find("ThDAC") != std::string::npos) continue;
                        if(cMapItem.first.find("Bias_THDAC") != std::string::npos) continue;

                        LOG(INFO) << BOLDYELLOW << "PedestalEqualization::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
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
    uint16_t cStartValue = 0;
    if(cWithSSA) cStartValue = 40;
    if(cWithMPA) cStartValue = 50;
    bool originalAllChannelFlag = this->fAllChan;

    if(fPulseAmplitude != 0 && originalAllChannelFlag && cWithCBC)
    {
        this->SetTestAllChannels(false);
        LOG(INFO) << RED << "Cannot inject pulse for all channels, test in groups enabled. " << RESET;
    }

    // configure TP amplitude
    for(auto cBoard: *fDetectorContainer)
    {
        if(cWithSSA || cWithMPA)
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fPulseAmplitude);
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
    if(!fUseFixRange) { cStartValue = this->findPedestal(forceAllChannels); }
    else
    {
        cStartValue = (fMaxThreshold + fMinThreshold) / 2.;
    }
    if(fDisableStubLogic) disableStubLogic();
    LOG(INFO) << BLUE << "Sweep of S-curves will start at an average threshold of " << cStartValue << RESET;
    measureSCurves(cStartValue);
    // scanScurves();
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
    LOG(INFO) << "Validation: Taking Data with " << fNEventsToValidate << " random triggers!";

    for(auto cBoard: *fDetectorContainer)
    {
        // increase threshold to supress noise
        setThresholdtoNSigma(cBoard, 5);
    }
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    bool originalAllChannelFlag = this->fAllChan;

    LOG(INFO) << "Setting all channels";
    this->SetTestAllChannels(true);
    LOG(INFO) << "measuring with " << fNEventsToValidate << "events and " << fNEventsPerBurst << " per burst";
    this->measureData(fNEventsToValidate, fNEventsPerBurst);
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
                    RegisterVector cRegVec;
                    uint32_t       NCH = NCHANNELS;
                    if(cWithSSA) NCH = NSSACHANNELS;
                    if(cWithMPA) NCH = NMPACHANNELS;
                    for(uint32_t iChan = 0; iChan < NCH; iChan++)
                    {
                        // LOG (INFO) << RED << "Ch " << iChan << RESET ;
                        float occupancy =
                            theOccupancyContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getChannel<Occupancy>(iChan).fOccupancy;
                        if(occupancy > fMaskingThreshold && fMaskNoisyChannels)
                        {
                            if(cWithCBC)
                            {
                                // char cRegName[11];
                                // sprintf(cRegName, "Channel%03d", iChan + 1);
                                std::string cRegName = "Channel" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0xFF});
                            }
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                            {
                                // char cRegName[17];
                                // sprintf(cRegName, "THTRIMMING_S%03d", iChan + 1);
                                std::string cRegName = "THTRIMMING_S" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            if((cChip->getFrontEndType() == FrontEndType::MPA))
                            {
                                // char cRegName[12];
                                // sprintf(cRegName, "TrimDAC_P%04d", iChan + 1);
                                std::string cRegName = "TrimDAC_P" + (boost::format("%|04|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            LOG(INFO) << RED << "Found a noisy channel on Chip " << +cChip->getId() << " Channel " << iChan << " with an occupancy of " << occupancy << "; setting offset to " << +0xFF
                                      << RESET;
                        }
                        else
                            LOG(INFO) << BOLDGREEN << "Chip " << +cChip->getId() << " on Hybrid#" << +cHybrid->getId() << " Channel " << iChan << " with an occupancy of " << occupancy * 1e6
                                      << " number of hits is " << fNEventsToValidate * occupancy << "; threshold is " << fMaskingThreshold << " setting offset to " << +0xFF << RESET;
                    }

                    fReadoutChipInterface->WriteChipMultReg(cChip, cRegVec);
                }
            }
        }
    }
}

uint16_t PedeNoise::findPedestal(bool forceAllChannels)
{
    bool originalAllChannelFlag = this->fAllChan;
    if(forceAllChannels) this->SetTestAllChannels(true);

    // // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    // uint8_t cNormalize         = 0;
    // if(cWithCBC or (cWithSSA && !cWithMPA) or (cWithMPA && !cWithSSA)) { cNormalize = 1; }
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
                    if(cChip->getFrontEndType() == FrontEndType::MPA)
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
    if(cWithCBC || cWithSSA)
        LOG(INFO) << BOLDBLUE << "Found Pedestals on Strip ASICs to be around " << fMeanStrips << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Found Pedestals on Pixel ASICs to be around " << fMeanPixels << RESET;
    setNormalization(cNormalizationOrig);
    float cMean = (cWithCBC || cWithSSA) ? fMeanStrips : fMeanPixels;
    return cMean;
}
void PedeNoise::scanScurves()
{
    float                 cMaxOccupancy  = 1.;
    float                 cLimit         = 0.05;
    int                   cMinBreakCount = 15; // take from xml
    int                   cStepSize      = 1;  // take from xml
    int                   cInitialSign   = -1;
    int                   cPrintOutStep  = 10;
    DetectorDataContainer cCounts, cSigns, cThresholds, cStatus;

    // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    uint8_t cNormalize         = 0;

    if(cWithCBC or (cWithSSA && !cWithMPA) or (cWithMPA && !cWithSSA)) { cNormalize = 1; }
    LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    setNormalization(cNormalize);

    ContainerFactory::copyAndInitChip<std::pair<int, int>>(*fDetectorContainer, cCounts);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, cSigns);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cThresholds);
    ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, cStatus);
    LOG(INFO) << BOLDYELLOW << "Starting S-curve scan" << RESET;
    // intialize containers to start value
    for(auto cBoard: *fDetectorContainer)
    {
        LOG(DEBUG) << BOLDYELLOW << "Initialising containers on board" << +cBoard->getId() << RESET;
        auto cSignThisBrd   = cSigns.at(cBoard->getIndex());
        auto cThThisBrd     = cThresholds.at(cBoard->getIndex());
        auto cCntThisBrd    = cCounts.at(cBoard->getIndex());
        auto cStatusThisBrd = cStatus.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            LOG(DEBUG) << BOLDYELLOW << "Initialising containers for OG" << +cOpticalGroup->getId() << RESET;
            auto& cSignThisOG   = cSignThisBrd->at(cOpticalGroup->getIndex());
            auto& cThThisOG     = cThThisBrd->at(cOpticalGroup->getIndex());
            auto& cCntThisOG    = cCntThisBrd->at(cOpticalGroup->getIndex());
            auto& cStatusThisOG = cStatusThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(DEBUG) << BOLDYELLOW << "Initialising containers for Hybrid" << +cHybrid->getId() << RESET;
                auto& cSignThisHybrid   = cSignThisOG->at(cHybrid->getIndex());
                auto& cThThisHybrid     = cThThisOG->at(cHybrid->getIndex());
                auto& cCntThisHybrid    = cCntThisOG->at(cHybrid->getIndex());
                auto& cStatusThisHybrid = cStatusThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    auto  cThreshold                    = fReadoutChipInterface->ReadChipReg(cChip, "Threshold");
                    auto& cCntThisChip                  = cCntThisHybrid->at(cChip->getIndex());
                    auto& cSummary                      = cCntThisChip->getSummary<std::pair<int, int>>();
                    cSummary.first                      = (cChip->getFrontEndType() == FrontEndType::CBC3) ? 0 : cMaxOccupancy;
                    cSummary.second                     = 0;
                    auto& cThThisChip                   = cThThisHybrid->at(cChip->getIndex());
                    cThThisChip->getSummary<uint16_t>() = cThreshold;
                    auto& cSignThisChip                 = cSignThisHybrid->at(cChip->getIndex());
                    cSignThisChip->getSummary<int>()    = cInitialSign;
                    auto& cStatusThisChip               = cStatusThisHybrid->at(cChip->getIndex());
                    auto& cStatusSmry                   = cStatusThisChip->getSummary<uint8_t>();
                    cStatusSmry                         = 0;
                    LOG(DEBUG) << BOLDYELLOW << "Initialising containers for Chip" << +cChip->getId() << " - current threshold is " << cThThisChip->getSummary<uint16_t>() << " current limit is "
                               << cSummary.first << " current break count is " << cSummary.second << RESET;

                } // Chip
            }     // FE
        }         // OG
    }             // board

    bool   cContinueScan = true;
    size_t cStepCounter  = 0;
    do
    {
        // set threshold
        size_t cNmodified = 0;
        for(auto cBoard: *fDetectorContainer)
        {
            auto& cSignThisBrd   = cSigns.at(cBoard->getIndex());
            auto& cThThisBrd     = cThresholds.at(cBoard->getIndex());
            auto& cCntThisBrd    = cCounts.at(cBoard->getIndex());
            auto  cStatusThisBrd = cStatus.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cSignThisOG   = cSignThisBrd->at(cOpticalGroup->getIndex());
                auto& cThThisOG     = cThThisBrd->at(cOpticalGroup->getIndex());
                auto& cCntThisOG    = cCntThisBrd->at(cOpticalGroup->getIndex());
                auto& cStatusThisOG = cStatusThisBrd->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cSignThisHybrid   = cSignThisOG->at(cHybrid->getIndex());
                    auto& cThThisHybrid     = cThThisOG->at(cHybrid->getIndex());
                    auto& cCntThisHybrid    = cCntThisOG->at(cHybrid->getIndex());
                    auto& cStatusThisHybrid = cStatusThisOG->at(cHybrid->getIndex());
                    for(auto cChip: *cHybrid)
                    {
                        auto& cCntThisChip    = cCntThisHybrid->at(cChip->getIndex());
                        auto& cCntSummary     = cCntThisChip->getSummary<std::pair<int, int>>();
                        auto& cSignThisChip   = cSignThisHybrid->at(cChip->getIndex())->getSummary<int>();
                        auto& cStatusThisChip = cStatusThisHybrid->at(cChip->getIndex())->getSummary<uint8_t>();

                        auto&    cThThisChip = cThThisHybrid->at(cChip->getIndex())->getSummary<uint16_t>();
                        uint16_t cMaxValue   = (cChip->getFrontEndType() == FrontEndType::CBC3) ? (1 << 10) : (1 << 8);
                        cMaxValue            = cMaxValue - 1;

                        // switch sign and reset count once break count has been reached
                        bool cEndReached = (cStatusThisChip == 1) ? true : false;
                        if(cCntSummary.second == cMinBreakCount && !cEndReached)
                        {
                            LOG(INFO) << BOLDYELLOW << "\t\t.. Chip" << +cChip->getId() << " on hybrid" << +cHybrid->getId() << " break count reached, switching sign "
                                      << " .... current sign is " << cSignThisChip << " current limit is " << cCntSummary.first << " and current break count is " << cCntSummary.second << RESET;
                            cEndReached = (cChip->getFrontEndType() == FrontEndType::CBC3) ? (cCntSummary.first == 1) : (cCntSummary.first == 0);
                            if(cEndReached)
                            {
                                cStatusThisChip = 1;
                                LOG(INFO) << BOLDYELLOW << "\t\t.. Finished scan for Chip#" << +cChip->getId() << " on hybrid" << +cHybrid->getId() << RESET;
                            }
                            else
                            {
                                cSignThisChip      = -1 * cSignThisChip;
                                cCntSummary.second = 0;
                                cCntSummary.first  = (cCntSummary.first == 1) ? 1 - cCntSummary.first : 0;
                            }
                        }
                        // bool cBrkCntReached = (cCntSummary.second >= cMinBreakCount);
                        // bool cEndReached = (cChip->getFrontEndType() == FrontEndType::CBC3) ? ( cCntSummary.first == 1) : 0;
                        // if( cBrkCntReached && cEndReached) continue;

                        // set threshold
                        int  cTh             = cThThisChip + cSignThisChip * cStepSize;
                        bool cThLimitReached = (cTh <= 0 || cTh >= cMaxValue);
                        if(cThLimitReached) cTh = (cTh <= 0) ? 0 : cMaxValue;
                        // if( cChip->getFrontEndType() == FrontEndType::MPA ) cTh = 255;
                        cThThisChip = cTh;

                        if(cStepCounter % cPrintOutStep == 0)
                            LOG(INFO) << BOLDYELLOW << "Setting threshold on Chip" << +cChip->getId() << " on Hybrid" << +cChip->getHybridId() << " to " << cThThisChip << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, "Threshold", cThThisChip);
                        if(cStatusThisChip == 0) cNmodified++;
                    } // Chip
                }     // FE
            }         // OG
        }             // board
        // continue scan if at least one object isn't finished
        cContinueScan = (cNmodified > 0);
        if(!cContinueScan)
        {
            LOG(INFO) << BOLDMAGENTA << "Number of modified thresholds is " << +cNmodified << ".. will stop scan!" << RESET;
            continue;
        }

        // measure occupancy for all BeBoards
        DetectorDataContainer* cOccContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
        fDetectorDataContainer               = cOccContainer;
        fSCurveOccupancyMap[cStepCounter]    = cOccContainer;
        float  cGlbOcc                       = 0;
        size_t cNormGlblOcc                  = 0;
        for(auto cBoard: *fDetectorContainer)
        {
            measureBeBoardData(cBoard->getIndex(), fEventsPerPoint, fNEventsPerBurst);
            if(cNormalize == 0)
            {
                auto& cDataContainerThisBrd = fDetectorDataContainer->at(cBoard->getIndex());
                for(auto cOpticalGroup: *cBoard)
                {
                    auto& cDataContainerThisOG = cDataContainerThisBrd->at(cOpticalGroup->getIndex());
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        auto& cDataContainerThisHybrid = cDataContainerThisOG->at(cHybrid->getIndex());
                        for(auto cChip: *cHybrid)
                        {
                            auto&                cDataContainerThisChip = cDataContainerThisHybrid->at(cChip->getIndex());
                            auto&                cSummary               = cDataContainerThisChip->getSummary<Occupancy, Occupancy>();
                            ChannelGroupHandler* cHandler;
                            if(cChip->getFrontEndType() == FrontEndType::MPA)
                                cHandler = new MPAChannelGroupHandler();
                            else
                                cHandler = new SSAChannelGroupHandler();
                            LOG(DEBUG) << BOLDYELLOW << " Normalizing assuming " << +getNReadbackEvents() << " events and " << cHandler->allChannelGroup()->getNumberOfEnabledChannels()
                                       << " enabled channels." << RESET;
                            cSummary.fOccupancy = 0;
                            for(uint16_t cChnl = 0; cChnl < cDataContainerThisChip->size(); cChnl++)
                            {
                                uint32_t cRow = cChnl % cHandler->allChannelGroup()->getNumberOfRows();
                                uint32_t cCol;
                                if(cHandler->allChannelGroup()->getNumberOfCols() == 0)
                                    cCol = 0;
                                else
                                    cCol = cChnl / cHandler->allChannelGroup()->getNumberOfRows();
                                if(cHandler->allChannelGroup()->isChannelEnabled(cRow, cCol))
                                {
                                    cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy /= getNReadbackEvents();
                                    cGlbOcc += cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy;
                                    if(cChnl < 10 || cChnl > 15 * 120 + 110)
                                        LOG(DEBUG) << BOLDBLUE << cChnl << " [ " << cRow << " , " << cCol << " ] " << cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy << RESET;
                                    cSummary.fOccupancy += cDataContainerThisChip->getChannel<Occupancy>(cRow, cCol).fOccupancy;
                                    cNormGlblOcc++;
                                }
                            }
                            cSummary.fOccupancy = std::min(cMaxOccupancy, cSummary.fOccupancy / cHandler->allChannelGroup()->getNumberOfEnabledChannels());
                        }
                    }
                }
            }
        }
        if(cNormalize == 1)
            cGlbOcc = std::min(cMaxOccupancy, cOccContainer->getSummary<Occupancy, Occupancy>().fOccupancy);
        else
            cGlbOcc = cGlbOcc / cNormGlblOcc;

        if(cStepCounter % cPrintOutStep == 0) LOG(INFO) << BOLDBLUE << "PedeNoise [Step#" << +cStepCounter << "] global occupancy is " << cGlbOcc << RESET;

#ifdef __USE_ROOT__
        if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cThresholds, *cOccContainer);
#endif

        // now check if the occupancy has reached the limit
        for(auto cBoard: *fDetectorDataContainer)
        {
            auto& cCntThisBrd = cCounts.at(cBoard->getIndex());
            // auto& cThThisBrd = cThresholds.at(cBoard->getIndex());
            // auto& cSignThisBrd = cSigns.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cCntThisOG = cCntThisBrd->at(cOpticalGroup->getIndex());
                // auto& cSignThisOG = cSignThisBrd->at(cOpticalGroup->getIndex());
                // auto& cThThisOG = cThThisBrd->at(cOpticalGroup->getIndex());
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCntThisHybrid = cCntThisOG->at(cHybrid->getIndex());
                    // auto& cSignThisHybrid = cSignThisOG->at(cHybrid->getIndex());
                    // auto& cThThisHybrid = cThThisOG->at(cHybrid->getIndex());
                    for(auto cChip: *cHybrid)
                    {
                        // update counters
                        auto& cCntThisChip = cCntThisHybrid->at(cChip->getIndex());
                        auto& cCntSummary  = cCntThisChip->getSummary<std::pair<int, int>>();
                        auto& cOccThisChip = cChip->getSummary<Occupancy, Occupancy>().fOccupancy;
                        auto  cDifference  = std::fabs(std::min(cMaxOccupancy, cOccThisChip) - cCntSummary.first);
                        if(cStepCounter % cPrintOutStep == 0)
                            LOG(DEBUG) << BOLDBLUE << "\t.. Chip" << +cChip->getId() << " on Hybrid" << +cHybrid->getId() << " is " << cOccThisChip << " difference is " << cDifference
                                       << " current limit has been found " << cCntSummary.second << " times." << RESET;
                        int cIncrement = (std::fabs(cOccThisChip - cCntSummary.first) <= cLimit) ? 1 : 0;
                        cCntSummary.second += cIncrement;
                        // // update threshold
                        // auto& cThThisChip = cThThisHybrid->at(cChip->getIndex());
                        // auto& cSignThisChip = cSignThisHybrid->at(cChip->getIndex());
                        // cThThisChip->getSummary<uint16_t>() = cThThisChip->getSummary<uint16_t>() + cSignThisChip->getSummary<int>()*cStepSize;
                    } // chip
                }     // hybrid
            }         // OG
        }             // board
        // for the other case - I don't know what to do ask Fabio
        cStepCounter++;
    } while(cContinueScan); // && cStepCounter < 10);

    // return normalization back to original value
    setNormalization(cNormalizationOrig);
}
void PedeNoise::measureSCurves(uint16_t pStartValue)
{
    auto cChannels     = findValueInSettings<double>("NoiseMeasurementLimit", 1);
    auto cLowerLimitTh = findValueInSettings<double>("PedeNoiseMinThreshold", 0);
    auto cUpperLimitTh = findValueInSettings<double>("PedeNoiseMaxThreshold", 0);
    if(fUseFixRange) LOG(INFO) << BOLDYELLOW << "Scan should be between " << cLowerLimitTh << " and " << cUpperLimitTh << " DAC units" << RESET;

    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    float    cMaxOccupancy  = 1.0;
    float    cLimit         = cChannels / (100.);
    int      cMinBreakCount = 10;
    uint16_t cValue         = pStartValue;
    uint16_t cMaxValue      = (1 << 10) - 1;
    // uint16_t cMinValue      = 0;
    if(cWithSSA || cWithMPA) cMaxValue = (1 << 8) - 1;
    float              cFirstLimit = (cWithCBC) ? 0 : 1;
    std::vector<int>   cSigns{-1, 1};
    std::vector<float> cLimits{cFirstLimit, 1 - cFirstLimit};
    //(fDetectorContainer[0]->getBoardType() == BoardType::D19C)

    int cCounter = 0;
    for(auto cSign: cSigns)
    {
        bool firstlim      = false;
        bool cLimitFound   = false;
        int  cLimitCounter = 0;
        do
        {
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurveOccupancyMap[cValue]                  = theOccupancyContainer;
            this->setDacAndMeasureData("Threshold", cValue, fEventsPerPoint, fNEventsPerBurst);

            theOccupancyContainer->normalizeAndAverageContainers(fDetectorContainer, getChannelGroupHandlerContainer(), fEventsPerPoint);
            float globalOccupancy = theOccupancyContainer->getSummary<Occupancy, Occupancy>().fOccupancy;
#ifdef __USE_ROOT__
            if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cValue, *theOccupancyContainer);
#else
            if(fPlotSCurves)
            {
                auto theSCurveStreamer = prepareChannelContainerStreamer<Occupancy, uint16_t>("SCurve");
                theSCurveStreamer->setHeaderElement(cValue);
                for(auto board: *theOccupancyContainer)
                {
                    if(fDQMStreamerEnabled) theSCurveStreamer->streamAndSendBoard(board, fDQMStreamer);
                }
            }
#endif

            auto cDistanceFromTarget = std::fabs(std::min(globalOccupancy, cMaxOccupancy) - (cLimits[cCounter]));
            LOG(INFO) << BOLDMAGENTA << "Current value of threshold is  " << cValue << " Occupancy: " << std::setprecision(2) << std::fixed << globalOccupancy << "\t.. distance from target is "
                      << cDistanceFromTarget * 100 << "\t..Incrementing limit found counter "
                      << " -- current value is " << +cLimitCounter << RESET;
            if(cDistanceFromTarget <= cLimit || firstlim) // || globalOccupancy>1.0)
            {
                firstlim = true;
                // LOG(DEBUG) << BOLDMAGENTA << "\t\t....Incrementing limit found counter "
                //            << " -- current value is " << +cLimitCounter << RESET;
                cLimitCounter++;
            }

            cValue += cSign;
            if(!fUseFixRange)
            {
                cLimitFound = (cValue == 0 || cValue >= cMaxValue) || (cLimitCounter >= cMinBreakCount);
                if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign during auto scan .." << RESET; }
            }
            else
            {
                cLimitFound = (cSign < 0) ? (cValue == cLowerLimitTh) : (cValue == cUpperLimitTh);
                if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign because threshold limit was reached .." << RESET; }
            }

        } while(!cLimitFound);
        cCounter++;
        cValue = pStartValue + cSigns[cCounter];
    }
    // this->HttpServerProcess();
    LOG(DEBUG) << YELLOW << "Found minimal and maximal occupancy " << cMinBreakCount << " times, SCurves finished! " << RESET;
}
void PedeNoise::extractPedeNoise()
{
    fThresholdAndNoiseContainer = new DetectorDataContainer();
    ContainerFactory::copyAndInitStructure<ThresholdAndNoise>(*fDetectorContainer, *fThresholdAndNoiseContainer);
    uint16_t                                                     counter          = 0;
    std::map<uint16_t, DetectorDataContainer*>::reverse_iterator previousIterator = fSCurveOccupancyMap.rend();
    for(std::map<uint16_t, DetectorDataContainer*>::reverse_iterator mIt = fSCurveOccupancyMap.rbegin(); mIt != fSCurveOccupancyMap.rend(); ++mIt)
    {
        if(previousIterator == fSCurveOccupancyMap.rend())
        {
            previousIterator = mIt;
            continue;
        }
        if(fSCurveOccupancyMap.size() - 1 == counter) break;

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
                            float previousOccupancy = (previousIterator)
                                                          ->second->at(board->getIndex())
                                                          ->at(opticalGroup->getIndex())
                                                          ->at(hybrid->getIndex())
                                                          ->at(chip->getIndex())
                                                          ->getChannel<Occupancy>(iChannel)
                                                          .fOccupancy;
                            float currentOccupancy =
                                mIt->second->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getChannel<Occupancy>(iChannel).fOccupancy;
                            float binCenter = (mIt->first + (previousIterator)->first) / 2.;

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fThreshold += binCenter * (previousOccupancy - currentOccupancy);

                            fThresholdAndNoiseContainer->at(board->getIndex())
                                ->at(opticalGroup->getIndex())
                                ->at(hybrid->getIndex())
                                ->at(chip->getIndex())
                                ->getChannel<ThresholdAndNoise>(iChannel)
                                .fNoise += binCenter * binCenter * (previousOccupancy - currentOccupancy);

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

        previousIterator = mIt;
        ++counter;
    }

    // calculate the averages and ship
    // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    uint8_t cNormalize         = 0;

    if(cWithCBC or (cWithSSA && !cWithMPA) or (cWithMPA && !cWithSSA)) { cNormalize = 1; }
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

void PedeNoise::writeObjects()
{
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.process();
#endif
}

void PedeNoise::ConfigureCalibration() { CreateResultDirectory("Results/Run_PedeNoise"); }

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
