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
    //delete fBoardRegContainer;
    delete fThresholdAndNoiseContainer;
    if( fDisableStubLogic ) 
    {
        delete fStubLogicValue;
        delete fHIPCountValue;
    }
    cleanContainerMap();
}

void PedeNoise::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fDisableStubLogic = pDisableStubLogic;

    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    cWithCBC                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    cWithSSA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA                       = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    if(cWithMPA) fChannelGroupHandler = new MPAChannelGroupHandler();

    initializeRecycleBin();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
    // For async only -- to fix
    if(cWithMPA or cWithSSA) fChannelGroupHandler->setChannelGroupParameters(120, 16);
    fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings("MaskChannelsFromOtherGroups", 1);
    fPlotSCurves                 = findValueInSettings("PlotSCurves", 0);
    fFitSCurves                  = findValueInSettings("FitSCurves", 0);
    fPulseAmplitude              = findValueInSettings("PedeNoisePulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings("Nevents", 10);
    fNEventsPerBurst             = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    LOG(INFO) << BOLDRED << "I8" << RESET;
    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;
    LOG(INFO) << " Number of enabled channels " << fChannelGroupHandler->allChannelGroup()->getNumberOfEnabledChannels() << " " << +fChannelGroupHandler->allChannelGroup()->areAllChannelsEnabled() << RESET;

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
                for(auto cROC: *cHybrid)
                {
                    fReadoutChipInterface->WriteChipReg(cROC, "AnalogueAsync", 1);
                }
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
    bool cWithPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
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
                    LOG(INFO) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        // don't reconfigure the offsets .. whole point of this excercise 
                        if( cMapItem.first.find("VCth") != std::string::npos ) continue;
                        if( cMapItem.first.find("ThDAC") != std::string::npos ) continue; 
                        if( cMapItem.first.find("Bias_THDAC") != std::string::npos ) continue; 

                        LOG(INFO) << BOLDBLUE << "PedeNoise::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
                                   << cMapItem.second.fValue << RESET;
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
                        //fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth", 0x23);
                        //fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode", 0x00);
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
    if(fPulseAmplitude != 0)
    {
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
        setFWTestPulse();
    }
    this->enableTestPulse(fPulseAmplitude != 0);
    cStartValue = this->findPedestal(fPulseAmplitude == 0 );
    
    if(fDisableStubLogic) disableStubLogic();
    LOG (INFO) << BLUE <<  "Sweep of S-curves will start at an average threshold of " <<cStartValue<< RESET ;
    measureSCurves(cStartValue);
    //scanScurves();


    //if(fDisableStubLogic) reloadStubLogic();
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
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
    // std::cout << __PRETTY_FUNCTION__ << "__USE_ROOT__Is stream enabled: " << fStreamerEnabled << std::endl;
#else
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    std::cout << __PRETTY_FUNCTION__ << "Is stream enabled: " << fStreamerEnabled << std::endl;
    auto theOccupancyStream = prepareHybridContainerStreamer<Occupancy, Occupancy, Occupancy>();
    // auto theOccupancyStream = prepareChannelContainerStreamer<Occupancy>();

    LOG(INFO) << "6 ";
    for(auto board: theOccupancyContainer)
    {
        if(fStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fNetworkStreamer);
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
                            LOG(INFO) << RED << "Found a noisy channel on ROC " << +cROC->getId() << " Channel " << iChan << " with an occupancy of " << occupancy << "; setting offset to " << +0xFF
                                      << RESET;
                        }
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
                    if(cWithCBC) tmpVthr = (static_cast<ReadoutChip*>(cROC)->getReg("VCth1") + (static_cast<ReadoutChip*>(cROC)->getReg("VCth2") << 8));
                    if(cWithSSA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("Bias_THDAC");
                    if(cWithMPA) tmpVthr = static_cast<ReadoutChip*>(cROC)->getReg("ThDAC0");

                    cMean += tmpVthr;
                    ++nCbc;
                }
            }
        }
    }

    cMean /= nCbc;

    LOG(INFO) << BOLDBLUE << "Found Pedestals to be around " << BOLDRED << cMean << RESET;

    return cMean;
}
void PedeNoise::scanScurves()
{
    float    cMaxOccupancy  = 4.; 
    float    cLimit         = 0.01;
    int      cMinBreakCount =  5;//take from xml
    int      cStepSize      =  1;//take from xml 
    int      cInitialSign   = -1;
    DetectorDataContainer cCounts, cSigns, cThresholds , cStatus ;

    ContainerFactory::copyAndInitChip<std::pair<int,int>>(*fDetectorContainer, cCounts);
    ContainerFactory::copyAndInitChip<int>(*fDetectorContainer, cSigns);
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, cThresholds);
    ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, cStatus);
    LOG (INFO) << BOLDYELLOW << "Starting S-curve scan" << RESET;
    // intialize containers to start value
    for(auto cBoard: *fDetectorContainer)
    {
        LOG (DEBUG) << BOLDYELLOW << "Initialising containers on board" << +cBoard->getId() << RESET;
        auto cSignThisBrd = cSigns.at(cBoard->getIndex());
        auto cThThisBrd = cThresholds.at(cBoard->getIndex());
        auto cCntThisBrd = cCounts.at(cBoard->getIndex());
        auto cStatusThisBrd = cStatus.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            LOG (DEBUG) << BOLDYELLOW << "Initialising containers for OG" << +cOpticalGroup->getId() << RESET;
            auto& cSignThisOG = cSignThisBrd->at(cOpticalGroup->getIndex());
            auto& cThThisOG = cThThisBrd->at(cOpticalGroup->getIndex());
            auto& cCntThisOG = cCntThisBrd->at(cOpticalGroup->getIndex());
            auto& cStatusThisOG = cStatusThisBrd->at(cOpticalGroup->getIndex());
            for(auto cFe: *cOpticalGroup)
            {
                LOG (DEBUG) << BOLDYELLOW << "Initialising containers for Hybrid" << +cFe->getId() << RESET;
                auto& cSignThisFE = cSignThisOG->at(cFe->getIndex());
                auto& cThThisFE = cThThisOG->at(cFe->getIndex());
                auto& cCntThisFE = cCntThisOG->at(cFe->getIndex());
                auto& cStatusThisFE = cStatusThisOG->at(cFe->getIndex());
                for(auto cROC: *cFe)
                {
                    auto cThreshold = fReadoutChipInterface->ReadChipReg(cROC,"Threshold");
                    auto& cCntThisROC = cCntThisFE->at(cROC->getIndex());
                    auto& cSummary = cCntThisROC->getSummary<std::pair<int,int>>();
                    cSummary.first = (cROC->getFrontEndType() == FrontEndType::CBC3 ) ? 0 : cMaxOccupancy; 
                    cSummary.second = 0;
                    auto& cThThisROC = cThThisFE->at(cROC->getIndex());
                    cThThisROC->getSummary<uint16_t>()=cThreshold;
                    auto& cSignThisROC = cSignThisFE->at(cROC->getIndex());
                    cSignThisROC->getSummary<int>()=cInitialSign;
                    auto& cStatusThisROC = cStatusThisFE->at(cROC->getIndex());
                    auto& cStatusSmry = cStatusThisROC->getSummary<uint8_t>();
                    cStatusSmry=0; 
                    LOG (DEBUG) << BOLDYELLOW << "Initialising containers for Chip" << +cROC->getId() 
                        << " - current threshold is " << cThThisROC->getSummary<uint16_t>() 
                        << " current limit is " << cSummary.first 
                        << " current break count is " << cSummary.second
                        << RESET;

                    
                }//ROC
            }//FE
        }//OG
    }//board 

    bool cContinueScan=true;
    size_t cStepCounter=0;
    do
    {
        // set threshold 
        size_t cNmodified=0; 
        for(auto cBoard: *fDetectorContainer)
        {
            auto& cSignThisBrd = cSigns.at(cBoard->getIndex());
            auto& cThThisBrd = cThresholds.at(cBoard->getIndex());
            auto& cCntThisBrd = cCounts.at(cBoard->getIndex());
            auto cStatusThisBrd = cStatus.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cSignThisOG = cSignThisBrd->at(cOpticalGroup->getIndex());
                auto& cThThisOG = cThThisBrd->at(cOpticalGroup->getIndex());
                auto& cCntThisOG = cCntThisBrd->at(cOpticalGroup->getIndex());
                auto& cStatusThisOG = cStatusThisBrd->at(cOpticalGroup->getIndex());
                for(auto cFe: *cOpticalGroup)
                {
                    auto& cSignThisFE = cSignThisOG->at(cFe->getIndex());
                    auto& cThThisFE = cThThisOG->at(cFe->getIndex());
                    auto& cCntThisFE = cCntThisOG->at(cFe->getIndex());
                    auto& cStatusThisFE = cStatusThisOG->at(cFe->getIndex());
                    for(auto cROC: *cFe)
                    {
                        auto& cCntThisROC = cCntThisFE->at(cROC->getIndex());
                        auto& cCntSummary = cCntThisROC->getSummary<std::pair<int,int>>();
                        auto& cSignThisROC = cSignThisFE->at(cROC->getIndex())->getSummary<int>();
                        auto& cStatusThisROC = cStatusThisFE->at(cROC->getIndex())->getSummary<uint8_t>();
                
                        auto cThThisROC = cThThisFE->at(cROC->getIndex())->getSummary<uint16_t>();
                        uint16_t cMaxValue = (cROC->getFrontEndType() == FrontEndType::CBC3) ? ( 1 << 10 ) : (1 << 8); 
                        cMaxValue = cMaxValue - 1; 
                        bool cThLimitReached = (cThThisROC <= 0 || cThThisROC >= cMaxValue); 
                        if( cThLimitReached ) continue;

                        // switch sign and reset count once break count has been reached 
                        bool cEndReached = false;
                        if( cCntSummary.second == cMinBreakCount )
                        { 
                            LOG (INFO) << BOLDYELLOW << "\t\t.. ROC" << +cROC->getId() << " on hybrid" << +cFe->getId() 
                                << " break count reached, switching sign " 
                                << " .... current sign is " << cSignThisROC
                                << " current limit is " << cCntSummary.first 
                                << " and current break count is " << cCntSummary.second
                                << RESET;
                            cEndReached = (cROC->getFrontEndType() == FrontEndType::CBC3) ? ( cCntSummary.first == 1) : ( cCntSummary.first == 0); 
                            if( cEndReached )
                            {
                                cStatusThisROC = 1; 
                                LOG (INFO) << BOLDYELLOW << "\t\t.. Finished scan for ROC#"  << +cROC->getId() << " on hybrid" << +cFe->getId()  << RESET;
                            }
                            else
                            {
                                cSignThisROC = -1*cSignThisROC;
                                cCntSummary.second =0;
                                cCntSummary.first = (cCntSummary.first == 1 ) ? 1-cCntSummary.first : 0;
                            }
                        }
                        if( cEndReached ) continue; 
                        //bool cBrkCntReached = (cCntSummary.second >= cMinBreakCount);
                        //bool cEndReached = (cROC->getFrontEndType() == FrontEndType::CBC3) ? ( cCntSummary.first == 1) : 0; 
                        //if( cBrkCntReached && cEndReached) continue;

                        // set threshold 
                        uint16_t cTh = cThThisROC + cSignThisROC*cStepSize;
                        LOG (DEBUG) << BOLDYELLOW << "Setting threshold on ROC" << +cROC->getId() << " on Hybrid" << +cROC->getHybridId() 
                            << " to " << cTh << RESET;
                        fReadoutChipInterface->WriteChipReg(cROC,"Threshold", cTh);
                        if( cStatusThisROC == 0 ) cNmodified++;
                    }//ROC
                }//FE
            }//OG
        }//board 
        // continue scan if at least one object isn't finished 
        cContinueScan = (cNmodified>0);
        if( !cContinueScan ) { LOG (INFO) << BOLDMAGENTA << "Number of modified thresholds is " << +cNmodified << ".. will stop scan!" << RESET; continue;}
    
        // measure occupancy for all BeBoards
        DetectorDataContainer* cOccContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
        fDetectorDataContainer                       = cOccContainer;
        for( auto cBoard :*fDetectorContainer ) 
        {
            measureBeBoardData(cBoard->getIndex(), fEventsPerPoint, fNEventsPerBurst);
        }
        float cGlbOcc = std::min( cMaxOccupancy, cOccContainer->getSummary<Occupancy, Occupancy>().fOccupancy );
        if( cStepCounter%2 == 0 ) LOG (INFO) << BOLDBLUE << "PedeNoise [Step#" << +cStepCounter << "] global occupancy is " << cGlbOcc << RESET;

        // now check if the occupancy has reached the limit 
        // and update threshold 
        for( auto cBoard : *fDetectorDataContainer )
        {
            auto& cCntThisBrd = cCounts.at(cBoard->getIndex());
            auto& cThThisBrd = cThresholds.at(cBoard->getIndex());
            auto& cSignThisBrd = cSigns.at(cBoard->getIndex());
            for(auto cOpticalGroup: *cBoard)
            {
                auto& cCntThisOG = cCntThisBrd->at(cOpticalGroup->getIndex());
                auto& cSignThisOG = cSignThisBrd->at(cOpticalGroup->getIndex());
                auto& cThThisOG = cThThisBrd->at(cOpticalGroup->getIndex());
                for(auto cFe: *cOpticalGroup)
                {
                    auto& cCntThisFE = cCntThisOG->at(cFe->getIndex());
                    auto& cSignThisFE = cSignThisOG->at(cFe->getIndex());
                    auto& cThThisFE = cThThisOG->at(cFe->getIndex());
                    for(auto cROC: *cFe)
                    {
                        // update counters 
                        auto& cCntThisROC = cCntThisFE->at(cROC->getIndex());
                        auto& cCntSummary = cCntThisROC->getSummary<std::pair<int,int>>();
                        auto& cOccThisChip = cROC->getSummary<Occupancy, Occupancy>().fOccupancy;
                        auto  cDifference =  std::fabs( std::min(cMaxOccupancy,cOccThisChip)-cCntSummary.first);
                        LOG (INFO) << BOLDBLUE << "\t.. Chip" << +cROC->getId() << " on Hybrid" << +cFe->getId() 
                            << " is " << cOccThisChip 
                            << " difference is " << cDifference 
                            << " current limit has been found " << cCntSummary.second << " times."
                            << RESET;
                        int cIncrement = ( std::fabs(cOccThisChip-cCntSummary.first) <= cLimit )  ? 1 : 0;
                        cCntSummary.second += cIncrement;
                        // update threshold 
                        auto& cThThisROC = cThThisFE->at(cROC->getIndex());
                        auto& cSignThisROC = cSignThisFE->at(cROC->getIndex());
                        cThThisROC->getSummary<uint16_t>() = cThThisROC->getSummary<uint16_t>() + cSignThisROC->getSummary<int>()*cStepSize;
                    }//chip
                }// hybrid
            }//OG
        }// board 

        #ifdef __USE_ROOT__
            if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cThresholds, *cOccContainer);
        #endif
        // for the other case - I don't know what to do ask Fabio 
        cStepCounter++;
    }while(cContinueScan);

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
        do {
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurveOccupancyMap[cValue]                  = theOccupancyContainer;
            this->setDacAndMeasureData("Threshold", cValue, fEventsPerPoint, fNEventsPerBurst);

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
                    if(fStreamerEnabled) theSCurveStreamer.streamAndSendBoard(board, fNetworkStreamer);
                }
            }
#endif

            auto cDistanceFromTarget = std::fabs( std::min(globalOccupancy,cMaxOccupancy) - (cLimits[cCounter]));
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
                            if(!fChannelGroupHandler->allChannelGroup()->isChannelEnabled(iChannel)) continue;
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
                        if(!fChannelGroupHandler->allChannelGroup()->isChannelEnabled(iChannel)) continue;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise /= chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoise = sqrt(chip->getChannel<ThresholdAndNoise>(iChannel).fNoise - (chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold *
                                                                                                                                            chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold));
                        chip->getChannel<ThresholdAndNoise>(iChannel).fThresholdError = 1;
                        chip->getChannel<ThresholdAndNoise>(iChannel).fNoiseError     = 1;
                    }
                }
            }
        }
        board->normalizeAndAverageContainers(fDetectorContainer->at(board->getIndex()), fChannelGroupHandler->allChannelGroup(), 0);
    }
}

void PedeNoise::producePedeNoisePlots()
{
#ifdef __USE_ROOT__
    if(!fFitSCurves) fDQMHistogramPedeNoise.fillPedestalAndNoisePlots(*fThresholdAndNoiseContainer);
#else
    auto theThresholdAndNoiseStream = prepareChannelContainerStreamer<ThresholdAndNoise>();
    for(auto board: *fThresholdAndNoiseContainer)
    {
        if(fStreamerEnabled) { theThresholdAndNoiseStream.streamAndSendBoard(board, fNetworkStreamer); }
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
