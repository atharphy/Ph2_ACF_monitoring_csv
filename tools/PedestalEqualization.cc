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
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));

    cWithCBC = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    cWithSSA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    if(cWithCBC) fChannelGroupHandler = new CBCChannelGroupHandler();
    if(cWithSSA) fChannelGroupHandler = new SSAChannelGroupHandler();
    if(cWithMPA) fChannelGroupHandler = new MPAChannelGroupHandler();
    fChannelGroupHandler->setChannelGroupParameters(16, 2);
    // For async only -- to fix
    if(cWithMPA) fChannelGroupHandler->setChannelGroupParameters(16, 120);

    this->fAllChan = pAllChan;

    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0);
    fMaskChannelsFromOtherGroups = findValueInSettings<double>("MaskChannelsFromOtherGroups", 1);
    fCheckLoop                   = findValueInSettings<double>("VerificationLoop", 1);
    fTestPulseAmplitude          = findValueInSettings<double>("PedestalEqualizationPulseAmplitude", 0);
    fEventsPerPoint              = findValueInSettings<double>("Nevents", 10);
    fNEventsPerBurst             = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : -1;
    fTargetOffset                = 0x7F;
    if(cWithSSA or cWithMPA) fTargetOffset = 0xF;

    fTargetVcth = 0x0;
    this->SetSkipMaskedChannels(fSkipMaskedChannels);

    if(fTestPulseAmplitude == 0)
        fTestPulse = 0;
    else
        fTestPulse = 1;
        // LOG (INFO) << BLUE <<  "fTestPulse " <<fTestPulse<< RESET ;

#ifdef __USE_ROOT__
    fDQMHistogramPedestalEqualization.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    if(fDisableStubLogic)
    {
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fStubLogicCointainer);
        ContainerFactory::copyAndInitChip<uint8_t>(*fDetectorContainer, fHIPCountCointainer);

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
                            fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() =
                                fReadoutChipInterface->ReadChipReg(theChip, "Pipe&StubInpSel&Ptwidth");

                            uint8_t value = fReadoutChipInterface->ReadChipReg(theChip, "HIP&TestMode");
                            fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>() = value;
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
    LOG(INFO) << "  Target Vcth determined algorithmically for ROC";
    LOG(INFO) << "  Target Offset fixed to half range (0x80) for ROC";
}

void PedestalEqualization::FindVplus()
{
    if(fTestPulse)
    {
        this->enableTestPulse(true);
        setFWTestPulse();
        for(auto cBoard: *fDetectorContainer)
        {
            if(cWithSSA)
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "InjectedCharge", fTestPulseAmplitude);
            else if(cWithMPA)
            {
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC0", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC1", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC2", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC3", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC4", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC5", fTestPulseAmplitude);
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "CalDAC6", fTestPulseAmplitude);
            }
            else
                setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fTestPulseAmplitude);
        }
        LOG(INFO) << BLUE << "Enabled test pulse. " << RESET;
    }
    else
        this->enableTestPulse(false);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    LOG(INFO) << BOLDBLUE << "Identifying optimal Vplus for ROC..." << RESET;
    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);
    if(cWithMPA) setSameDac("ThDAC_ALL", fTargetVcth);
    bool originalAllChannelFlag = this->fAllChan;
    this->SetTestAllChannels(true);
    if(cWithCBC) setSameLocalDac("ChannelOffset", fTargetOffset);
    if(cWithSSA) setSameLocalDac("ThresholdTrim", fTargetOffset);
    if(cWithMPA) setSameLocalDac("ThresholdTrim", fTargetOffset);
    if(cWithCBC) this->bitWiseScan("VCth", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("Bias_THDAC", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithMPA) this->bitWiseScan("ThDAC_ALL", fEventsPerPoint, 0.56, fNEventsPerBurst);
    dumpConfigFiles();

    if(cWithCBC) setSameLocalDac("ChannelOffset", 0xFF);
    if(cWithSSA) setSameLocalDac("ThresholdTrim", 0xFF);
    if(cWithMPA) setSameLocalDac("ThresholdTrim", 0xFF);

    DetectorDataContainer theVcthContainer;
    ContainerFactory::copyAndInitChip<uint16_t>(*fDetectorContainer, theVcthContainer);

    float    cMeanValue = 0.;
    uint32_t nCbc       = 0;

    for(auto board: theVcthContainer) // for on boards - begin
    {
        for(auto opticalGroup: *board) // for on opticalGroup - begin
        {
            for(auto hybrid: *opticalGroup) // for on hybrid - begin
            {
                nCbc += hybrid->size();
                for(auto chip: *hybrid) // for on chip - begin
                {
                    ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));
                    uint16_t     tmpVthr = 0;
                    if(cWithCBC) tmpVthr = (theChip->getReg("VCth1") + (theChip->getReg("VCth2") << 8));
                    if(cWithSSA) tmpVthr = theChip->getReg("Bias_THDAC");
                    if(cWithMPA) tmpVthr = theChip->getReg("ThDAC0");

                    chip->getSummary<uint16_t>() = tmpVthr;

                    LOG(INFO) << GREEN << "VCth value for BeBoard " << +board->getId() << " OpticalGroup " << +opticalGroup->getId() << " Hybrid " << +hybrid->getId() << " ROC " << +chip->getId()
                              << " = " << tmpVthr << RESET;
                    cMeanValue += tmpVthr;
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
        if(fStreamerEnabled) theVCthStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif

    fTargetVcth = uint16_t(cMeanValue / nCbc);

    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);
    if(cWithMPA) setSameDac("ThDAC_ALL", fTargetVcth);

    LOG(INFO) << BOLDBLUE << "Mean VCth value of all chips is " << fTargetVcth << " - using as TargetVcth value for all chips!" << RESET;
    this->SetTestAllChannels(originalAllChannelFlag);
}

void PedestalEqualization::FindOffsets()
{
    LOG(INFO) << BOLDBLUE << "Finding offsets..." << RESET;
    // just to be sure, configure the correct VCth and VPlus values

    uint32_t NCH = NCHANNELS;
    if(cWithSSA) NCH = NSSACHANNELS;
    if(cWithMPA) NCH = NMPACHANNELS;

    if(cWithCBC) setSameDac("VCth", fTargetVcth);
    if(cWithSSA) setSameDac("Bias_THDAC", fTargetVcth);
    if(cWithMPA) setSameDac("ThDAC_ALL", fTargetVcth);

    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

    if(cWithCBC) this->bitWiseScan("ChannelOffset", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithSSA) this->bitWiseScan("ThresholdTrim", fEventsPerPoint, 0.56, fNEventsPerBurst);
    if(cWithMPA) this->bitWiseScan("ThresholdTrim", fEventsPerPoint, 0.56, fNEventsPerBurst);
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
                    if(fDisableStubLogic and cWithCBC)
                    {
                        ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()));

                        uint8_t stubLogicValue = fStubLogicCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                        fReadoutChipInterface->WriteChipReg(theChip, "Pipe&StubInpSel&Ptwidth", stubLogicValue);

                        uint8_t HIPCountValue = fHIPCountCointainer.at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex())->getSummary<uint8_t>();
                        fReadoutChipInterface->WriteChipReg(theChip, "HIP&TestMode", HIPCountValue);
                    }

                    unsigned int channelNumber = 1;
                    int          cMeanOffset   = 0;

                    for(auto& channel: *chip->getChannelContainer<uint8_t>()) // for on channel - begin
                    {
                        char charRegName[20];

                        if(cWithCBC) sprintf(charRegName, "Channel%03d", channelNumber++);
                        if(cWithSSA) sprintf(charRegName, "THTRIMMING_S%d", channelNumber++);
                        if(cWithMPA) sprintf(charRegName, "TrimDAC_P%d", channelNumber++);
                        std::string cRegName = charRegName;
                        channel = static_cast<ReadoutChip*>(fDetectorContainer->at(board->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(chip->getIndex()))->getReg(cRegName);
                        cMeanOffset += channel;
                    }

                    LOG(INFO) << BOLDRED << "Mean offset on ROC" << +chip->getId() << " is : " << (cMeanOffset) / (double)NCH << " Vcth units." << RESET;
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
        if(fStreamerEnabled) theOccupancyStream.streamAndSendBoard(board, fNetworkStreamer);
    }

    auto theOffsetStream = prepareChannelContainerStreamer<uint8_t>();
    for(auto board: theOffsetsCointainer)
    {
        if(fStreamerEnabled) theOffsetStream.streamAndSendBoard(board, fNetworkStreamer);
    }
#endif

    // a add write original register ;
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
}

void PedestalEqualization::Stop()
{
    LOG(INFO) << "Stopping Pedestal Equalization.";
    writeObjects();
    dumpConfigFiles();
    closeFileHandler();
    LOG(INFO) << "Pedestal Equalization stopped.";
}

void PedestalEqualization::Pause() {}

void PedestalEqualization::Resume() {}
