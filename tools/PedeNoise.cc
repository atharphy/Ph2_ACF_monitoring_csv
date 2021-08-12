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
    delete fThresholdAndNoiseContainer;
    delete fStubLogicValue;
    delete fHIPCountValue;
    cleanContainerMap();
}

void PedeNoise::Initialise(bool pAllChan, bool pDisableStubLogic)
{
    fDisableStubLogic = pDisableStubLogic;

    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));

    cWithCBC = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    cWithSSA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    if(cWithMPA) fChannelGroupHandler = new MPAChannelGroupHandler();

    initializeRecycleBin();

    fChannelGroupHandler->setChannelGroupParameters(16, 2);
    // For async only -- to fix
    if(cWithMPA or cWithSSA) fChannelGroupHandler->setChannelGroupParameters(16, 120);
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

    fNEventsPerBurst = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;

    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;

    this->SetSkipMaskedChannels(fSkipMaskedChannels);
    if(fFitSCurves) fPlotSCurves = true;

#ifdef __USE_ROOT__
    fDQMHistogramPedeNoise.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
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
                        fStubLogicValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth");
                        fHIPCountValue->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cROC->getIndex())->getSummary<uint16_t>() =
                            fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode");
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "Pipe&StubInpSel&Ptwidth", 0x23);
                        fReadoutChipInterface->WriteChipReg(static_cast<ReadoutChip*>(cROC), "HIP&TestMode", 0x00);
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
        if(cWithSSA)
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fPulseAmplitude);
        else if(cWithMPA)
        {
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC0", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC1", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC2", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC3", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC4", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC5", fPulseAmplitude);
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC6", fPulseAmplitude);
        }
        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fPulseAmplitude);
    }

    bool forceAllChannels = false;
    if(fPulseAmplitude != 0)
    {
        this->enableTestPulse(true);
        setFWTestPulse();
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    }
    else
    {
        this->enableTestPulse(false);
        forceAllChannels = true;
    }
    if(!fUseFixRange) { cStartValue = this->findPedestal(forceAllChannels); }
    else
    {
        cStartValue = (fMaxThreshold + fMinThreshold) / 2.;
    }

    if(fDisableStubLogic) disableStubLogic();
    // LOG (INFO) << BLUE <<  "SV " <<cStartValue<< RESET ;

    measureSCurves(cStartValue);

    if(fDisableStubLogic) reloadStubLogic();

    this->SetTestAllChannels(originalAllChannelFlag);
    if(fPulseAmplitude != 0)
    {
        this->enableTestPulse(false);
        if(cWithSSA) setSameGlobalDac("InjectedCharge", 0);
        if(cWithMPA)
        {
            setSameGlobalDac("CalDAC0", 0);
            setSameGlobalDac("CalDAC1", 0);
            setSameGlobalDac("CalDAC2", 0);
            setSameGlobalDac("CalDAC3", 0);
            setSameGlobalDac("CalDAC4", 0);
            setSameGlobalDac("CalDAC5", 0);
            setSameGlobalDac("CalDAC6", 0);
        }
        else
            setSameGlobalDac("TestPulsePotNodeSel", 0);

        LOG(INFO) << BLUE << "Disabled test pulse. " << RESET;
    }

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
                                char cRegName[11];
                                sprintf(cRegName, "Channel%03d", iChan + 1);
                                cRegVec.push_back({cRegName, 0xFF});
                            }
                            if(cWithSSA)
                            {
                                char cRegName[17];
                                sprintf(cRegName, "THTRIMMING_S%03d", iChan + 1);
                                cRegVec.push_back({cRegName, 0x1F});
                            }
                            if(cWithMPA)
                            {
                                char cRegName[12];
                                sprintf(cRegName, "TrimDAC_P%04d", iChan + 1);
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
    if(cWithCBC) this->bitWiseScan("VCth", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("Bias_THDAC", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithMPA) this->bitWiseScan("ThDAC_ALL", fEventsPerPoint, 0.56, fNEventsPerBurst);

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
void PedeNoise::measureSCurves(uint16_t pStartValue)
{
    // adding limit to define what all one and all zero actually mean.. avoid waiting forever during scan!
    int      cMinBreakCount = 5;
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
        bool cLimitFound   = false;
        int  cLimitCounter = 0;
        do
        {
            DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
            fDetectorDataContainer                       = theOccupancyContainer;
            fSCurveOccupancyMap[cValue]                  = theOccupancyContainer;
            if(cWithCBC) this->setDacAndMeasureData("VCth", cValue, fEventsPerPoint, fNEventsPerBurst);
            if(cWithSSA) this->setDacAndMeasureData("Bias_THDAC", cValue, fEventsPerPoint, fNEventsPerBurst);
            if(cWithMPA) this->setDacAndMeasureData("ThDAC_ALL", cValue, fEventsPerPoint, fNEventsPerBurst);
            // this->setDacAndMeasureData("VCth", cValue, fEventsPerPoint);

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

            LOG(INFO) << BOLDMAGENTA << "Current value of threshold is  " << cValue << " Occupancy: " << std::setprecision(2) << std::fixed << globalOccupancy << "\t.. "
                      << "Incrementing limit found counter "
                      << " -- current value is " << +cLimitCounter << RESET;
            if(!fUseFixRange)
            {
                auto cDistanceFromTarget = std::fabs(globalOccupancy - (cLimits[cCounter]));
                if(cDistanceFromTarget <= fLimit)
                {
                    LOG(DEBUG) << BOLDMAGENTA << "\t\t....Incrementing limit found counter "
                               << " -- current value is " << +cLimitCounter << RESET;
                    cLimitCounter++;
                }

                cValue += cSign;
                cLimitFound = (cValue <= 0 || cValue >= cMaxValue) || (cLimitCounter >= cMinBreakCount);
                if(cLimitFound && (cLimitCounter < cMinBreakCount)) { LOG(WARNING) << BOLDRED << "Running out of values to test without reaching the limit..." << RESET; }
                if(cLimitFound) { LOG(INFO) << BOLDYELLOW << "Switching sign.." << RESET; }
            }
            else
            {
                if(cSign == -1 && cValue == fMinThreshold) cLimitFound = true;
                if(cSign == 1 && cValue == fMaxThreshold) cLimitFound = true;
                cValue += cSign;
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
}

void PedeNoise::Stop()
{
    LOG(INFO) << "Stopping noise measurement";
    writeObjects();
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    clearDataMembers();
    LOG(INFO) << "Noise measurement stopped.";
}

void PedeNoise::Pause() {}

void PedeNoise::Resume() {}
