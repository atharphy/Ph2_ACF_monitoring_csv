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
    fDisableStubLogic = pDisableStubLogic;

    cWithCBC = false;
    cWithSSA = false;
    cWithMPA = false;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                if(!cWithCBC)
                {
                    cWithCBC = (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::CBC3; }) != cHybrid->end());
                }
                if(!cWithSSA)
                {
                    cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::SSA; }) != cHybrid->end());
                }
                if(!cWithMPA)
                {
                    cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::MPA; }) != cHybrid->end());
                }
            }
        }
    }
    if(cWithCBC) LOG(INFO) << BOLDBLUE << "PedeNoise with CBCs" << RESET;
    if(cWithSSA && !cWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs" << RESET;
    if(cWithMPA && !cWithSSA) LOG(INFO) << BOLDBLUE << "PedeNoise with MPAs" << RESET;
    if(cWithSSA && cWithMPA) LOG(INFO) << BOLDBLUE << "PedeNoise with SSAs+MPAs" << RESET;

    // ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    // cWithCBC                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    // cWithSSA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    // cWithMPA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);
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
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA);
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::SSA2);
    }
    if(cWithMPA)
    {
        MPAChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA);
        setChannelGroupHandler(theChannelGroupHandler, FrontEndType::MPA2);
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
    fMaxThreshold                = findValueInSettings<double>("PedeNoiseMaxThreshold", 1023);

    fNEventsPerBurst                  = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
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

    // for now.. force to use async mode here
    bool cForcePSasync = true;
    // event types
    fEventTypes.clear();
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.push_back(cBoard->getEventType());
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                if(!cWithSSA && !cWithMPA) continue;

                if(!cForcePSasync) continue;

                cBoard->setEventType(EventType::PSAS);
                // set all SSAs + MPAs to output data in async mode
                for(auto cROC: *cHybrid) { fReadoutChipInterface->WriteChipReg(cROC, "AnalogueAsync", 1); }
            }
        }
    }
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    // enable ASYNC mode for PS asics
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->setBoard(cBoard->getId());
        // auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
        // cInterface->SetPSCounterMode(cEnableFastCounterReadout);
        // cInterface->SetPSPairSelect(cEnablePairSelect);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                // set all SSAs + MPAs to output data in async mode
                for(auto cROC: *cHybrid)
                {
                    if(cROC->getFrontEndType() == FrontEndType::CBC3) continue;

                    // TBC - what about MPA here?
                    LOG(INFO) << BOLDBLUE << "Setting up for analogue async injection in SSA/MPAs" << RESET;
                    fReadoutChipInterface->WriteChipReg(cROC, "AnalogueAsync", 1);
                }
            }
        }
    }
}

void PedeNoise::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by PedeNoise" << RESET;
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
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
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                bool cIsPS    = (cWithSSA && cWithMPA) && cWithLpGBT;
                cWithPS       = cWithPS || cIsPS;
                LOG(INFO) << BOLDBLUE << "PedeNoise::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(DEBUG) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        // don't reconfigure the offsets .. whole point of this excercise
                        if(cMapItem.first.find("VCth") != std::string::npos) continue;
                        if(cMapItem.first.find("ThDAC") != std::string::npos) continue;
                        if(cMapItem.first.find("Bias_THDAC") != std::string::npos) continue;

                        LOG(DEBUG) << BOLDBLUE << "PedeNoise::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to " << cMapItem.second.fValue
                                   << RESET;
                        fReadoutChipInterface->WriteChipReg(cChip, cMapItem.first, cMapItem.second.fValue);
                    }
                }
            }
        }
    }
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        if(cWithPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
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
                for(auto cROC: *cHybrid)
                {
                    if(cROC->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - thus disabling Stub logic for pedestal and noise measurement." << RESET;
                        static_cast<CbcInterface*>(fReadoutChipInterface)->enableHipSuppression(cROC, false, true, 0);
                        fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth");
                        fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode");
                        // fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth", 0x23);
                        // fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode", 0x00);
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
                for(auto cROC: *cHybrid)
                {
                    RegisterVector cRegVec;
                    if(cROC->getFrontEndType() == FrontEndType::CBC3)
                    {
                        LOG(INFO) << BOLDBLUE << "Chip Type = CBC3 - re-enabling stub logic to original value!" << RESET;
                        cRegVec.push_back(
                            {"Pipe&StubInpSel&Ptwidth", fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>()});
                        cRegVec.push_back(
                            {"HIP&TestMode", fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>()});
                        fReadoutChipInterface->WriteChipMultReg(cROC, cRegVec);
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
    if(fPulseAmplitude != 0) { LOG(INFO) << BLUE << "Enabled test pulse. " << RESET; }
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
    this->enableTestPulse(fPulseAmplitude != 0);
    cStartValue = this->findPedestal(fPulseAmplitude == 0);
    if(fDisableStubLogic) disableStubLogic();
    LOG(INFO) << BLUE << "Sweep of S-curves will start at an average threshold of " << cStartValue << RESET;
    measureSCurves(cStartValue);
    // scanScurves();

    // if(fDisableStubLogic) reloadStubLogic();
    this->SetTestAllChannels(originalAllChannelFlag);
    // if(fPulseAmplitude != 0)
    // {
    //     this->enableTestPulse(false);
    //     if(cWithSSA)
    //         setSameGlobalDac("InjectedCharge", 0);
    //     else if(cWithMPA)
    //     {
    //         setSameGlobalDac("CalDAC0", 0);
    //         setSameGlobalDac("CalDAC1", 0);
    //         setSameGlobalDac("CalDAC2", 0);
    //         setSameGlobalDac("CalDAC3", 0);
    //         setSameGlobalDac("CalDAC4", 0);
    //         setSameGlobalDac("CalDAC5", 0);
    //         setSameGlobalDac("CalDAC6", 0);
    //     }
    //     else
    //         setSameGlobalDac("TestPulsePotNodeSel", 0);

    //     LOG(INFO) << BLUE << "Disabled test pulse. " << RESET;
    // }

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

void PedeNoise::Validate(uint32_t pNoiseStripThreshold, uint32_t pMultiple)
{
    LOG(INFO) << "Validation: Taking Data with " << fEventsPerPoint * pMultiple << " random triggers!";

    for(auto cBoard: *fDetectorContainer)
    {
        // increase threshold to supress noise
        setThresholdtoNSigma(cBoard, 5);
    }
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    bool originalAllChannelFlag = this->fAllChan;

    this->SetTestAllChannels(true);
    this->measureData(fEventsPerPoint * pMultiple);
    this->SetTestAllChannels(originalAllChannelFlag);
#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.fillValidationPlots(theOccupancyContainer);
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fDQMStreamerEnabled << std::endl;
#else
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fDQMStreamerEnabled << std::endl;
    auto theOccupancyStream = prepareHybridContainerStreamer<Occupancy, Occupancy, Occupancy>();
    // auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();

    LOG(INFO) << "6 ";
    for(auto board: theOccupancyContainer)
    {
        if(fDQMStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fDQMStreamer);
    }
#endif
    LOG(INFO) << "7 ";
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                // std::cout << __PRETTY_FUNCTION__ << " The Hybrid Occupancy = " <<
                // theOccupancyContainer.at(cBoard->getIndex())->at(cFe->getIndex())->getSummary<Occupancy,Occupancy>().fOccupancy
                // << std::endl;

                for(auto cROC: *cFe)
                {
                    RegisterVector cRegVec;
                    uint32_t       NCH = NCHANNELS;
                    if(cWithSSA) NCH = NSSACHANNELS;
                    if(cWithMPA) NCH = NMPACHANNELS;
                    for(uint32_t iChan = 0; iChan < NCH; iChan++)
                    {
                        // LOG (INFO) << RED << "Ch " << iChan << RESET ;
                        float occupancy =
                            theOccupancyContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cFe->getIndex())->at(cROC->getIndex())->getChannel<Occupancy>(iChan).fOccupancy;
                        if(occupancy > float(pNoiseStripThreshold * 0.001))
                        {
                            if(cWithCBC)
                            {
                                // char cRegName[11];
                                // sprintf(cRegName, "Channel%03d", iChan + 1);
                                std::string cRegName = "Channel" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0xFF});
                            }
                            if(cROC->getFrontEndType() == FrontEndType::SSA)
                            {
                                // char cRegName[17];
                                // sprintf(cRegName, "THTRIMMING_S%03d", iChan + 1);
                                std::string cRegName = "THTRIMMING_S" + (boost::format("%|03|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            if((cROC->getFrontEndType() == FrontEndType::MPA))
                            {
                                // char cRegName[12];
                                // sprintf(cRegName, "TrimDAC_P%04d", iChan + 1);
                                std::string cRegName = "TrimDAC_P" + (boost::format("%|04|") % (iChan + 1)).str();
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            LOG(INFO) << RED << "Found a noisy channel on ROC " << +cROC->getId() << " on Hybrid#" << +cFe->getId() << " Channel " << iChan << " with an occupancy of "
                                      << occupancy * 1e6 << "; threshold is " << pNoiseStripThreshold * 1e6 << " setting offset to " << +0xFF << RESET;
                        }
                        else
                            LOG(INFO) << BOLDGREEN << "ROC " << +cROC->getId() << " on Hybrid#" << +cFe->getId() << " Channel " << iChan << " with an occupancy of " << occupancy * 1e6
                                      << " number of hits is " << fEventsPerPoint * pMultiple * occupancy << "; threshold is " << pNoiseStripThreshold * 1e6 << " setting offset to " << +0xFF << RESET;
                    }

                    fReadoutChipInterface->WriteChipMultReg(cROC, cRegVec);
                }
            }
        }
        setThresholdtoNSigma(cBoard, 0);
    }
}

uint16_t PedeNoise::findPedestal(bool forceAllChannels)
{
    bool originalAllChannelFlag = this->fAllChan;
    if(forceAllChannels) this->SetTestAllChannels(true);

    // figure  out if you should normalize or not
    uint8_t cNormalizationOrig = getNormalization();
    uint8_t cNormalize         = 0;
    if(cWithCBC or (cWithSSA && !cWithMPA) or (cWithMPA && !cWithSSA)) { cNormalize = 1; }
    LOG(INFO) << BOLDBLUE << "normalization will be set to " << +cNormalize << RESET;
    setNormalization(cNormalize);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);
    this->bitWiseScan("Threshold", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(forceAllChannels) this->SetTestAllChannels(originalAllChannelFlag);

    float    cMean = 0.;
    uint32_t nCbc  = 0;

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cROC: *cFe)
                {
                    uint16_t tmpVthr = 0;
                    if(cROC->getFrontEndType() == FrontEndType::CBC3) tmpVthr = (static_cast<ReadoutChip*>(cROC)->getReg("VCth1") + (static_cast<ReadoutChip*>(cROC)->getReg("VCth2") << 8));
                    if(cROC->getFrontEndType() == FrontEndType::SSA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("Bias_THDAC");
                    if(cROC->getFrontEndType() == FrontEndType::MPA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("ThDAC0");

                    cMean += tmpVthr;
                    ++nCbc;
                }
            }
        }
    }

    cMean /= nCbc;

    LOG(INFO) << BOLDBLUE << "Found Pedestals to be around " << BOLDRED << cMean << RESET;
    setNormalization(cNormalizationOrig);
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
            for(auto cFe: *cOpticalGroup)
            {
                LOG(DEBUG) << BOLDYELLOW << "Initialising containers for Hybrid" << +cFe->getId() << RESET;
                auto& cSignThisFE   = cSignThisOG->at(cFe->getIndex());
                auto& cThThisFE     = cThThisOG->at(cFe->getIndex());
                auto& cCntThisFE    = cCntThisOG->at(cFe->getIndex());
                auto& cStatusThisFE = cStatusThisOG->at(cFe->getIndex());
                for(auto cROC: *cFe)
                {
                    auto  cThreshold                   = fReadoutChipInterface->ReadChipReg(cROC, "Threshold");
                    auto& cCntThisROC                  = cCntThisFE->at(cROC->getIndex());
                    auto& cSummary                     = cCntThisROC->getSummary<std::pair<int, int>>();
                    cSummary.first                     = (cROC->getFrontEndType() == FrontEndType::CBC3) ? 0 : cMaxOccupancy;
                    cSummary.second                    = 0;
                    auto& cThThisROC                   = cThThisFE->at(cROC->getIndex());
                    cThThisROC->getSummary<uint16_t>() = cThreshold;
                    auto& cSignThisROC                 = cSignThisFE->at(cROC->getIndex());
                    cSignThisROC->getSummary<int>()    = cInitialSign;
                    auto& cStatusThisROC               = cStatusThisFE->at(cROC->getIndex());
                    auto& cStatusSmry                  = cStatusThisROC->getSummary<uint8_t>();
                    cStatusSmry                        = 0;
                    LOG(DEBUG) << BOLDYELLOW << "Initialising containers for Chip" << +cROC->getId() << " - current threshold is " << cThThisROC->getSummary<uint16_t>() << " current limit is "
                               << cSummary.first << " current break count is " << cSummary.second << RESET;

                } // ROC
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
                for(auto cFe: *cOpticalGroup)
                {
                    auto& cSignThisFE   = cSignThisOG->at(cFe->getIndex());
                    auto& cThThisFE     = cThThisOG->at(cFe->getIndex());
                    auto& cCntThisFE    = cCntThisOG->at(cFe->getIndex());
                    auto& cStatusThisFE = cStatusThisOG->at(cFe->getIndex());
                    for(auto cROC: *cFe)
                    {
                        auto& cCntThisROC    = cCntThisFE->at(cROC->getIndex());
                        auto& cCntSummary    = cCntThisROC->getSummary<std::pair<int, int>>();
                        auto& cSignThisROC   = cSignThisFE->at(cROC->getIndex())->getSummary<int>();
                        auto& cStatusThisROC = cStatusThisFE->at(cROC->getIndex())->getSummary<uint8_t>();

                        auto&    cThThisROC = cThThisFE->at(cROC->getIndex())->getSummary<uint16_t>();
                        uint16_t cMaxValue  = (cROC->getFrontEndType() == FrontEndType::CBC3) ? (1 << 10) : (1 << 8);
                        cMaxValue           = cMaxValue - 1;

                        // switch sign and reset count once break count has been reached
                        bool cEndReached = (cStatusThisROC == 1) ? true : false;
                        if(cCntSummary.second == cMinBreakCount && !cEndReached)
                        {
                            LOG(INFO) << BOLDYELLOW << "\t\t.. ROC" << +cROC->getId() << " on hybrid" << +cFe->getId() << " break count reached, switching sign "
                                      << " .... current sign is " << cSignThisROC << " current limit is " << cCntSummary.first << " and current break count is " << cCntSummary.second << RESET;
                            cEndReached = (cROC->getFrontEndType() == FrontEndType::CBC3) ? (cCntSummary.first == 1) : (cCntSummary.first == 0);
                            if(cEndReached)
                            {
                                cStatusThisROC = 1;
                                LOG(INFO) << BOLDYELLOW << "\t\t.. Finished scan for ROC#" << +cROC->getId() << " on hybrid" << +cFe->getId() << RESET;
                            }
                            else
                            {
                                cSignThisROC       = -1 * cSignThisROC;
                                cCntSummary.second = 0;
                                cCntSummary.first  = (cCntSummary.first == 1) ? 1 - cCntSummary.first : 0;
                            }
                        }
                        // bool cBrkCntReached = (cCntSummary.second >= cMinBreakCount);
                        // bool cEndReached = (cROC->getFrontEndType() == FrontEndType::CBC3) ? ( cCntSummary.first == 1) : 0;
                        // if( cBrkCntReached && cEndReached) continue;

                        // set threshold
                        int  cTh             = cThThisROC + cSignThisROC * cStepSize;
                        bool cThLimitReached = (cTh <= 0 || cTh >= cMaxValue);
                        if(cThLimitReached) cTh = (cTh <= 0) ? 0 : cMaxValue;
                        // if( cROC->getFrontEndType() == FrontEndType::MPA ) cTh = 255;
                        cThThisROC = cTh;

                        if(cStepCounter % cPrintOutStep == 0)
                            LOG(INFO) << BOLDYELLOW << "Setting threshold on ROC" << +cROC->getId() << " on Hybrid" << +cROC->getHybridId() << " to " << cThThisROC << RESET;
                        fReadoutChipInterface->WriteChipReg(cROC, "Threshold", cThThisROC);
                        if(cStatusThisROC == 0) cNmodified++;
                    } // ROC
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
                    for(auto cFe: *cOpticalGroup)
                    {
                        auto& cDataContainerThisFE = cDataContainerThisOG->at(cFe->getIndex());
                        for(auto cROC: *cFe)
                        {
                            auto&                cDataContainerThisROC = cDataContainerThisFE->at(cROC->getIndex());
                            auto&                cSummary              = cDataContainerThisROC->getSummary<Occupancy, Occupancy>();
                            ChannelGroupHandler* cHandler;
                            if(cROC->getFrontEndType() == FrontEndType::MPA)
                                cHandler = new MPAChannelGroupHandler();
                            else
                                cHandler = new SSAChannelGroupHandler();
                            LOG(DEBUG) << BOLDYELLOW << " Normalizing assuming " << +getNReadbackEvents() << " events and " << cHandler->allChannelGroup()->getNumberOfEnabledChannels()
                                       << " enabled channels." << RESET;
                            cSummary.fOccupancy = 0;
                            for(uint16_t cChnl = 0; cChnl < cDataContainerThisROC->size(); cChnl++)
                            {
                                uint32_t cRow = cChnl % cHandler->allChannelGroup()->getNumberOfRows();
                                uint32_t cCol;
                                if(cHandler->allChannelGroup()->getNumberOfCols() == 0)
                                    cCol = 0;
                                else
                                    cCol = cChnl / cHandler->allChannelGroup()->getNumberOfRows();
                                if(cHandler->allChannelGroup()->isChannelEnabled(cRow, cCol))
                                {
                                    cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy /= getNReadbackEvents();
                                    cGlbOcc += cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy;
                                    if(cChnl < 10 || cChnl > 15 * 120 + 110)
                                        LOG(DEBUG) << BOLDBLUE << cChnl << " [ " << cRow << " , " << cCol << " ] " << cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy << RESET;
                                    cSummary.fOccupancy += cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy;
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
                for(auto cFe: *cOpticalGroup)
                {
                    auto& cCntThisFE = cCntThisOG->at(cFe->getIndex());
                    // auto& cSignThisFE = cSignThisOG->at(cFe->getIndex());
                    // auto& cThThisFE = cThThisOG->at(cFe->getIndex());
                    for(auto cROC: *cFe)
                    {
                        // update counters
                        auto& cCntThisROC  = cCntThisFE->at(cROC->getIndex());
                        auto& cCntSummary  = cCntThisROC->getSummary<std::pair<int, int>>();
                        auto& cOccThisChip = cROC->getSummary<Occupancy, Occupancy>().fOccupancy;
                        auto  cDifference  = std::fabs(std::min(cMaxOccupancy, cOccThisChip) - cCntSummary.first);
                        if(cStepCounter % cPrintOutStep == 0)
                            LOG(DEBUG) << BOLDBLUE << "\t.. Chip" << +cROC->getId() << " on Hybrid" << +cFe->getId() << " is " << cOccThisChip << " difference is " << cDifference
                                       << " current limit has been found " << cCntSummary.second << " times." << RESET;
                        int cIncrement = (std::fabs(cOccThisChip - cCntSummary.first) <= cLimit) ? 1 : 0;
                        cCntSummary.second += cIncrement;
                        // // update threshold
                        // auto& cThThisROC = cThThisFE->at(cROC->getIndex());
                        // auto& cSignThisROC = cSignThisFE->at(cROC->getIndex());
                        // cThThisROC->getSummary<uint16_t>() = cThThisROC->getSummary<uint16_t>() + cSignThisROC->getSummary<int>()*cStepSize;
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
    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    float    cMaxOccupancy  = 1.0;
    float    cLimit         = 0.05;
    int      cMinBreakCount = 10;
    uint16_t cValue         = pStartValue;
    uint16_t cMaxValue      = (1 << 10) - 1;
    // uint16_t cMinValue      = 0;
    if(cWithSSA) cMaxValue = (1 << 8) - 1;
    if(cWithMPA) cMaxValue = (1 << 8) - 1;
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

            theOccupancyContainer->normalizeAndAverageContainers(fDetectorContainer, fChannelGroupHandlerContainer, fEventsPerPoint);
            float globalOccupancy = theOccupancyContainer->getSummary<Occupancy, Occupancy>().fOccupancy;
#ifdef __USE_ROOT__
            if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cValue, *theOccupancyContainer);
#else
            if(fPlotSCurves)
            {
                auto theSCurveStreamer = prepareChannelContainerStreamer<Occupancy, uint16_t>("SCurve");
                theSCurveStreamer.setHeaderElement(cValue);
                for(auto board: *theOccupancyContainer)
                {
                    if(fDQMStreamerEnabled) theSCurveStreamer.streamAndSendBoard(board, fDQMStreamer);
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
            cLimitFound = (cValue == 0 || cValue >= cMaxValue) || (cLimitCounter >= cMinBreakCount);
            if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign.." << RESET; }

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
                            if(!fChannelGroupHandlerContainer->getObject(board->getId())
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
                        if(!fChannelGroupHandlerContainer->getObject(board->getId())
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
            //     for(auto cFe: *cOpticalGroup)
            //     {
            //         auto& cDataContainerThisFE = cDataContainerThisOG->at(cFe->getIndex());
            //         for(auto cROC: *cFe)
            //         {
            //             auto& cDataContainerThisROC = cDataContainerThisFE->at(cROC->getIndex());
            //             auto& cSummary = cDataContainerThisROC->getSummary<Occupancy,Occupancy>();
            //             ChannelGroupHandler* cHandler;
            //             if( cROC->getFrontEndType() == FrontEndType::MPA ) cHandler = new MPAChannelGroupHandler();
            //             else cHandler = new SSAChannelGroupHandler();
            //             LOG (DEBUG) << BOLDYELLOW << " Normalizing assuming " << +getNReadbackEvents()
            //                 << " events and " << cHandler->allChannelGroup()->getNumberOfEnabledChannels() << " enabled channels." << RESET;
            //             cSummary.fOccupancy = 0;
            //             for( uint16_t cChnl=0; cChnl < cDataContainerThisROC->size(); cChnl++)
            //             {
            //                 uint32_t cRow = cChnl%cHandler->allChannelGroup()->getNumberOfRows();
            //                 uint32_t cCol;
            //                 if( cHandler->allChannelGroup()->getNumberOfCols() == 0 ) cCol = 0;
            //                 else  cCol = cChnl/cHandler->allChannelGroup()->getNumberOfRows();
            //                 if(cHandler->allChannelGroup()->isChannelEnabled(cRow, cCol ))
            //                 {
            //                     cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy /= getNReadbackEvents();
            //                     cGlbOcc += cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy;
            //                     if( cChnl < 10 || cChnl > 15*120 + 110 ) LOG (DEBUG) << BOLDBLUE << cChnl << " [ " << cRow << " , " << cCol << " ] " <<
            //                     cDataContainerThisROC->getChannel<Occupancy>(cRow, cCol).fOccupancy << RESET; cSummary.fOccupancy+= cDataContainerThisROC->getChannel<Occupancy>(cRow,
            //                     cCol).fOccupancy; cNormGlblOcc++;
            //                 }
            //             }
            //             cSummary.fOccupancy = std::min(cMaxOccupancy, cSummary.fOccupancy/cHandler->allChannelGroup()->getNumberOfEnabledChannels());
            //         }
            //     }
            // }
        }
        else
            board->normalizeAndAverageContainers(fDetectorContainer->at(board->getIndex()), fChannelGroupHandlerContainer->getObject(board->getId()), 0);
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
        if(fDQMStreamerEnabled) { theThresholdAndNoiseStream.streamAndSendBoard(board, fDQMStreamer); }
    }
#endif
}

void PedeNoise::setThresholdtoNSigma(BoardContainer* board, uint32_t pNSigma)
{
    for(auto opticalGroup: *board)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                uint32_t cROCId = chip->getId();

                uint16_t cPedestal = round(fThresholdAndNoiseContainer->at(board->getIndex())
                                               ->at(opticalGroup->getIndex())
                                               ->at(hybrid->getIndex())
                                               ->at(chip->getIndex())
                                               ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                               .fThreshold);
                uint16_t cNoise    = round(fThresholdAndNoiseContainer->at(board->getIndex())
                                            ->at(opticalGroup->getIndex())
                                            ->at(hybrid->getIndex())
                                            ->at(chip->getIndex())
                                            ->getSummary<ThresholdAndNoise, ThresholdAndNoise>()
                                            .fNoise);
                int      cDiff     = -pNSigma * cNoise;
                uint16_t cValue    = cPedestal + cDiff;

                if(pNSigma > 0)
                    LOG(INFO) << "Changing Threshold on ROC " << +cROCId << " by " << cDiff << " to " << cPedestal + cDiff << " VCth units to supress noise!";
                else
                    LOG(INFO) << "Changing Threshold on ROC " << +cROCId << " back to the pedestal at " << +cPedestal;
                ThresholdVisitor cThresholdVisitor(fReadoutChipInterface, cValue);
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
