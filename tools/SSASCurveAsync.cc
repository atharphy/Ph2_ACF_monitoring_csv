/*!
  \file                  SSASCurveAsync.cc
  \brief                 Implementaion of SCurve scan
  \author                kevin Nash
  \version               1.0
  \date                  28/06/18
  Support:               email to knash@gmail.com
*/

#include "SSASCurveAsync.h"
#include "../Utils/Occupancy.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

SSASCurve::SSASCurve() : Tool() {}

SSASCurve::~SSASCurve() {}

void SSASCurve::Initialise(void)
{
    StartTHDAC = this->findValueInSettings("StartTHDAC");
    StopTHDAC  = this->findValueInSettings("StopTHDAC");
    NMsec      = this->findValueInSettings("NMsec");
    NMpulse    = this->findValueInSettings("NMpulse");
    Res        = this->findValueInSettings("Res");
    Nlvl       = this->findValueInSettings("Nlvl");
    Mrms       = this->findValueInSettings("Mrms");
    Vfac       = this->findValueInSettings("Vfac");
    SyncDebug  = this->findValueInSettings("SyncDebug");

    TestPulsePotentiometer = this->findValueInSettings("TestPulsePotentiometer");
#ifdef __USE_ROOT__
    fDQMHistogramSSASCurveAsync.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}
void SSASCurve::run(void)
{
    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));

    cWithSSA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA);
    cWithMPA = (cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);

    DetectorDataContainer theHitContainer;
    ContainerFactory::copyAndInitChannel<std::pair<std::array<uint32_t, 2>, float>>(*fDetectorContainer, theHitContainer);

    for(auto cBoard: theHitContainer)
    {
        BeBoard* theBeBoard = static_cast<BeBoard*>(fDetectorContainer->at(cBoard->getIndex()));

        if(cWithSSA)
        {
            if(SyncDebug)
                LOG(INFO) << BOLDBLUE << "SYNC DEBUG!" << RESET;
            else
                theBeBoard->setEventType(EventType::SSAAS);
        }
        if(cWithMPA)
        {
            if(SyncDebug)
                LOG(INFO) << BOLDBLUE << "SYNC DEBUG!" << RESET;
            else
                theBeBoard->setEventType(EventType::MPAAS);
        }

        float rms  = 999.0;
        float vfac = Vfac;
        LOG(INFO) << BOLDBLUE << "VFAC " << vfac << RESET;
        LOG(INFO) << BOLDBLUE << "Mrms " << Mrms << RESET;

        this->enableTestPulse(true);
        setFWTestPulse();
        // hard-code values for TP trigger
        // these really should just be in the xml..
        uint16_t cDelayAfterFastReset = 50;
        uint16_t cDelayAfterTP        = 200;
        uint16_t cDelayBeforeNextTP   = 1000;
        uint8_t  cEnableFastReset     = 0;
        uint8_t  cEnableTP            = 1;
        uint8_t  cEnableL1A           = 1;
        // why both?!
        // I think this is trying to
        // uint16_t cNtriggers=NMpulse;
        // uint16_t cTriggerRate=1000;
        // uint8_t cSource=6;
        // uint8_t cStubsMask=0;
        // uint8_t cStubLatency=0;
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTriggerFSM( cNtriggers,
        // cTriggerRate,cSource, cStubsMask, cStubLatency);
        // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->ConfigureTestPulseFSM(cDelayAfterFastReset
        // 	,cDelayAfterTP,cDelayBeforeNextTP
        // 	,cEnableFastReset,cEnableTP,1cEnableL1A);
        std::vector<std::pair<std::string, uint32_t>> cVecReg;
        // configure trigger
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", NMpulse});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_fast_reset", cDelayAfterFastReset});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_after_test_pulse", cDelayAfterTP});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", cDelayBeforeNextTP});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_fast_reset", cEnableFastReset});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_test_pulse", cEnableTP});
        cVecReg.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.en_l1a", cEnableL1A});
        fBeBoardInterface->WriteBoardMultReg(theBeBoard, cVecReg);

        float wroffset = 0.0;
        float Nuntoff  = 0.0;
        // until RMS is .. something?
        while(rms > Mrms)
        {
            float Nunt = 0.0;
            LOG(INFO) << BOLDBLUE << "Running Scurve..." << RESET;

            // std::vector<float> cur;
            // std::vector<float> goal;
            float mean   = 0.0;
            float Nstrip = 0.0;
            float Nmeans = 0.0;

            bool runpulse = false;

            if(NMpulse > 0) runpulse = true;
            if(cWithSSA)
            {
                LOG(INFO) << BOLDBLUE << "withSSA" << RESET;
                if(runpulse) setSameDacBeBoard(theBeBoard, "Bias_CALDAC", TestPulsePotentiometer);
            }
            if(cWithMPA)
            {
                LOG(INFO) << BOLDBLUE << "withMPA" << RESET;
                if(runpulse)
                {
                    setSameDacBeBoard(theBeBoard, "CalDAC0", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC1", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC2", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC3", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC4", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC5", TestPulsePotentiometer);
                    setSameDacBeBoard(theBeBoard, "CalDAC6", TestPulsePotentiometer);
                }
            }

            for(size_t thd = StartTHDAC; thd <= StopTHDAC; thd++)
            {
                Nstrip = 0.0;

                LOG(INFO) << BOLDRED << "THDAC " << thd << RESET;

                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex()));
                            if(cWithSSA) { this->fReadoutChipInterface->WriteChipReg(theChip, "Bias_THDAC", thd); }
                            if(cWithMPA)
                            {
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC0", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC1", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC2", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC3", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC4", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC5", thd);
                                this->fReadoutChipInterface->WriteChipReg(theChip, "ThDAC6", thd);
                            }
                            unsigned int channelNumber = 0;
                            channelNumber++;
                        } // ROC
                    }     // hybrid
                }         // module

                // static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->PS_Clear_counters();
                // if (SyncDebug)
                // 	ReadNEvents( theBeBoard, NMpulse );
                // else
                // 	ReadASEvent( theBeBoard, NMsec,NMpulse,false,false );
                // now ReadNEvents takes care of all of this
                // for both MPA and SSA
                this->ReadNEvents(theBeBoard, NMpulse);
                const std::vector<Event*>& cEvents = this->GetEvents(theBeBoard);

                for(auto opticalGroup: *cBoard)
                {
                    for(auto hybrid: *opticalGroup) // for on hybrid - begin
                    {
                        for(auto cROC: *hybrid) // for on chip - begin
                        {
                            for(auto cEvent: cEvents)
                            {
                                auto         hits          = cEvent->GetHits(hybrid->getId(), cROC->getId());
                                unsigned int channelNumber = 0;
                                unsigned int curh          = 0;
                                for(auto& channel: *cROC->getChannelContainer<std::pair<std::array<uint32_t, 2>, float>>())
                                {
                                    Nstrip += 1.0;
                                    curh = hits[channelNumber];
                                    // now hit vector contains number
                                    // if (SyncDebug)
                                    // {
                                    // 	curh=0;
                                    // 	for(auto cev: eventVector)
                                    // 	{
                                    // 		curh+=cev->DataBit ( hybrid->getId(), cSSA->getId(), channelNumber);
                                    // 		//LOG (INFO) << BOLDRED << curh<< RESET;
                                    // 	}
                                    // }
                                    // else curh=hits[channelNumber];
                                    if((channel.first[1] > Nlvl) && (curh < channel.first[1] / 2) && (channel.first[0] > channel.first[1] / 2))
                                    {
                                        channel.second = (float(thd) * float(curh) + float(thd - 1) * float(channel.first[0])) / (float(curh) + float(channel.first[0]));
                                        globalmax      = std::max(globalmax, channel.second);
                                    }

                                    if(runpulse)
                                        channel.first[1] = NMpulse;
                                    else
                                        channel.first[1] = std::max(channel.first[1], curh);
                                    // LOG (INFO) << BOLDRED << channel.first[1]<< RESET;
                                    channel.first[0] = curh;
                                    channelNumber++;
                                } // chnl
                            }     // events
                        }         // ROC
                    }             // hybrid
                }                 // module
#ifdef __USE_ROOT__
                fDQMHistogramSSASCurveAsync.fillSSASCurveAsyncPlots(theHitContainer, thd);
#endif
                //
            } // th-scan

            rms = 0.0;

            for(auto opticalGroup: *cBoard)
            {
                for(auto hybrid: *opticalGroup) // for on hybrid - begin
                {
                    for(auto cSSA: *hybrid) // for on chip - begin
                    {
                        for(auto& channel: *cSSA->getChannelContainer<std::pair<std::array<uint32_t, 2>, float>>())
                        {
                            mean += channel.second;
                            Nmeans += 1.0;
                        } // chn;
                    }     // ROC
                }         // hybrid
            }             // module

            mean /= Nmeans;

            std::string prestr = "";
            if(cWithSSA) prestr = "THTRIMMING_S";
            if(cWithMPA) prestr = "TrimDAC_P";

            float writeave = 0.0;
            for(auto opticalGroup: *cBoard)
            {
                for(auto hybrid: *opticalGroup) // for on hybrid - begin
                {
                    for(auto cROC: *hybrid) // for on chip - begin
                    {
                        uint32_t istrip = 1;
                        for(auto& channel: *cROC->getChannelContainer<std::pair<std::array<uint32_t, 2>, float>>()) // for on channel -
                                                                                                                    // begin
                        {
                            ReadoutChip* theChip = static_cast<ReadoutChip*>(fDetectorContainer->at(cBoard->getIndex())->at(opticalGroup->getIndex())->at(hybrid->getIndex())->at(cROC->getIndex()));
                            rms += (channel.second - mean) * (channel.second - mean);

                            int32_t cr      = fReadoutChipInterface->ReadChipReg(theChip, prestr + std::to_string(istrip));
                            float   floatTh = cr;
                            if(channel.second > 1.0) floatTh = float(cr) - vfac * (float(channel.second) - mean) + wroffset;

                            uint32_t THtowrite = uint32_t(std::roundf(floatTh));
                            THtowrite          = std::min(THtowrite, uint32_t(31));
                            THtowrite          = std::max(THtowrite, uint32_t(0));
                            writeave += THtowrite;
                            if(THtowrite == 0 || THtowrite == 31) LOG(INFO) << BOLDRED << "Warning, thdac at limit. Chip " << theChip->getId() << ",Strip " << istrip << ",TH " << THtowrite << RESET;
                            if(THtowrite == 0) Nunt += 1.0;
                            if(THtowrite == 31) Nunt -= 1.0;

                            fReadoutChipInterface->WriteChipReg(theChip, prestr + std::to_string(istrip), THtowrite);
                            istrip += 1;
                        } // chnl
                    }     // ROC
                }         // hybrid
            }             // mdule
            // prev = cur;
            // prevgoal=goal;
            LOG(INFO) << BOLDBLUE << "Done" << RESET;
            rms /= float(Nstrip);
            rms = std::sqrt(rms);
            if(std::abs(Nunt) > 0.0)
                Nuntoff += Nunt / std::abs(Nunt);
            else
                Nuntoff += 0;
            wroffset = 15.0 - writeave / float(Nstrip) + (Nuntoff);
            LOG(INFO) << BOLDRED << "RMS: " << rms << RESET;
            LOG(INFO) << BOLDRED << "MEAN: " << mean << RESET;
            LOG(INFO) << BOLDRED << "WRITEMEAN: " << writeave / float(Nstrip) << RESET;
            LOG(INFO) << BOLDRED << "MAX: " << globalmax << RESET;
            LOG(INFO) << BOLDRED << "NSTRIP: " << Nstrip << RESET;
            LOG(INFO) << BOLDRED << "OFF: " << Nuntoff << RESET;
        } // rms
    }     // board
}
void SSASCurve::writeObjects(void)
{
#ifdef __USE_ROOT__
    fDQMHistogramSSASCurveAsync.process();
#endif
    SaveResults();
    closeFileHandler();
}
