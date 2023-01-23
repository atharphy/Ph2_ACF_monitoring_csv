#include "PhaseScan.h"

#include "../HWDescription/Cbc.h"
#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/GenericDataArray.h"
#include "../Utils/MPAChannelGroupHandler.h"
#include "../Utils/Occupancy.h"
#include "../Utils/SSAChannelGroupHandler.h"

PhaseScan::PhaseScan() : Tool() {}

PhaseScan::~PhaseScan() {}

void PhaseScan::Initialize()
{
    // check sparsification
    for(auto cBoard: *fDetectorContainer)
    {
        bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
        cBoard->setSparsification(cSparsified);
    }

    ReadoutChip* cFirstReadoutChip = static_cast<ReadoutChip*>(fDetectorContainer->at(0)->at(0)->at(0)->at(0));
    bool         cWithCBC          = (cFirstReadoutChip->getFrontEndType() == FrontEndType::CBC3);
    bool         cWithPS           = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA || cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA);
    bool         cWithPSv2         = (cFirstReadoutChip->getFrontEndType() == FrontEndType::SSA2 || cFirstReadoutChip->getFrontEndType() == FrontEndType::MPA2);

    if(cWithCBC)
    {
        CBCChannelGroupHandler theChannelGroupHandler;
        theChannelGroupHandler.setChannelGroupParameters(16, 2); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandler);
    }
    else if(cWithPS)
    {
        MPAChannelGroupHandler theChannelGroupHandlerMPA;
        theChannelGroupHandlerMPA.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandlerMPA, FrontEndType::MPA);

        SSAChannelGroupHandler theChannelGroupHandlerSSA;
        theChannelGroupHandlerSSA.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandlerSSA, FrontEndType::SSA);
    }
    else if(cWithPSv2)
    {
        MPAChannelGroupHandler theChannelGroupHandlerMPA;
        theChannelGroupHandlerMPA.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandlerMPA, FrontEndType::MPA2);

        SSAChannelGroupHandler theChannelGroupHandlerSSA;
        theChannelGroupHandlerSSA.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
        setChannelGroupHandler(theChannelGroupHandlerSSA, FrontEndType::SSA2);
    }

    initializeRecycleBin();

    fPhaseStartLatency = findValueInSettings<double>("PhaseStartLatency", 1);
    fPhaseLatencyRange = findValueInSettings<double>("PhaseLatencyRange", 1);
    fStartPhase        = findValueInSettings<double>("StartPhase", 0);
    fPhaseRange        = findValueInSettings<double>("PhaseRange", 7);
    std::cout << "Going to read " << fNevents << " events" << std::endl;

#ifdef __USE_ROOT__
    fDQMHistogramPhaseScan.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif

    LOG(INFO) << "Histograms and Settings initialised.";
}

void PhaseScan::ScanPhase()
{
    uint32_t cDeltaLat = fPhaseStartLatency;
    do
    {
        uint32_t cPhaseLat = fStartPhase;
        do
        {
            for(auto cBoard: *fDetectorContainer)
            {
                for(auto cOpticalGroup: *cBoard)
                {
                    for(auto cHybrid: *cOpticalGroup)
                    {
                        for(auto cChip: *cHybrid)
                        {
                            if(cChip->getFrontEndType() == FrontEndType::SSA)
                                fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cDeltaLat - 1);
                            else if(cChip->getFrontEndType() == FrontEndType::SSA2)
                                fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cDeltaLat + 1);
                            else
                                fReadoutChipInterface->WriteChipReg(cChip, "TriggerLatency", cDeltaLat);
                            fReadoutChipInterface->WriteChipReg(cChip, "PhaseShift", cPhaseLat);
                        }
                    }
                }
                fBeBoardInterface->ChipReSync(cBoard);
                this->ReadNEvents(cBoard, fNevents);
                const std::vector<Event*>& cEvents      = this->GetEvents();
                size_t                     cTriggerMult = fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity");
                uint32_t                   NPclus       = 0;
                uint32_t                   NSclus       = 0;

                for(size_t cTriggerId = 0; cTriggerId < cTriggerMult + 1; cTriggerId++)
                {
                    DetectorDataContainer cHitContainer;
                    ContainerFactory::copyAndInitChip<GenericDataArray<VECSIZE, uint16_t>>(*fDetectorContainer, cHitContainer);

                    // std::cout<<"cTriggerId "<<+cTriggerId<<std::endl;
                    auto cEventIter = cEvents.begin() + cTriggerId;
                    do
                    {
                        uint8_t cTDCVal = (*cEventIter)->GetTDC();
                        for(auto cOpticalGroup: *cBoard)
                        {
                            for(auto cHybrid: *cOpticalGroup)
                            {
                                for(auto cChip: *cHybrid)
                                {
                                    std::vector<PCluster> cPclstrs = static_cast<D19cCic2Event*>((*cEventIter))->GetPixelClusters(cHybrid->getId(), cChip->getId());
                                    std::vector<SCluster> cSclstrs = static_cast<D19cCic2Event*>((*cEventIter))->GetStripClusters(cHybrid->getId(), cChip->getId());

                                    // cTotalHitsS0 += cPclstrs.size();
                                    // cTotalHitsS1 += cSclstrs.size();
                                    if(cChip->getFrontEndType() == FrontEndType::MPA2)
                                    {
                                        for(auto& cPclstr: cPclstrs)
                                        {
                                            for(uint8_t cId = 0; cId < (1 + cPclstr.fWidth); cId++)
                                            {
                                                cHitContainer.at(cBoard->getIndex())
                                                    ->at(cOpticalGroup->getIndex())
                                                    ->at(cHybrid->getIndex())
                                                    ->at(cChip->getIndex())
                                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal] += 1;
                                                NPclus += 1;
                                            }
                                        }
                                    }
                                    if(cChip->getFrontEndType() == FrontEndType::SSA2)
                                    {
                                        for(auto& cSclstr: cSclstrs)
                                        {
                                            for(uint8_t cId = 0; cId < (1 + cSclstr.fWidth); cId++)
                                            {
                                                cHitContainer.at(cBoard->getIndex())
                                                    ->at(cOpticalGroup->getIndex())
                                                    ->at(cHybrid->getIndex())
                                                    ->at(cChip->getIndex())
                                                    ->getSummary<GenericDataArray<VECSIZE, uint16_t>>()[cTDCVal] += 1;
                                                NSclus += 1;
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        cEventIter += (1 + cTriggerMult);
                    } while(cEventIter < cEvents.end());
                    fDQMHistogramPhaseScan.fillPhasePlots(cDeltaLat, cPhaseLat, cHitContainer);
                    if(NPclus + NSclus > 0)
                    {
                        LOG(INFO) << "Latency: " << cDeltaLat << " SamplingPhase: " << cPhaseLat << RESET;
                        LOG(INFO) << "Found NPclus: " << NPclus << " NSclus: " << NSclus << RESET;
                    }
                }
            }
            cPhaseLat += 1;
        } while(cPhaseLat < (fStartPhase + fPhaseRange));
        cDeltaLat += 1;

    } while(cDeltaLat < (fPhaseStartLatency + fPhaseLatencyRange));
}
void PhaseScan::updateHists(std::string pHistName, bool pFinal)
{
    for(auto& cCanvas: fCanvasMap)
    {
        // maybe need to declare temporary pointers outside the if condition?
        if(pHistName == "hybrid_latency")
        {
            cCanvas.second->cd();
            TH1F* cTmpHist = dynamic_cast<TH1F*>(getHist(static_cast<Ph2_HwDescription::Hybrid*>(cCanvas.first), pHistName));
            cTmpHist->DrawCopy();
            cCanvas.second->Update();
        }
    }

    this->HttpServerProcess();
}
void PhaseScan::writeObjects()
{
#ifdef __USE_ROOT__
    fDQMHistogramPhaseScan.process();
#endif
}

void PhaseScan::Running()
{
    LOG(INFO) << "Starting Latency Scan";

    Initialize();
    PhaseScan();
    // StubLatencyScan();
    // MeasureTriggerTDC();
    LOG(INFO) << "Done with Latency Scan";
}

void PhaseScan::Stop()
{
    LOG(INFO) << "Stopping Latency Scan.";
    writeObjects();
    dumpConfigFiles();
    closeFileHandler();
    LOG(INFO) << "Latency Scan stopped.";
}

void PhaseScan::Pause() {}

void PhaseScan::Resume() {}
