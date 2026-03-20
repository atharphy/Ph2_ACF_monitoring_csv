#include "tools/OTTimeCorrelations.h"
#include "HWInterface/D19cFWInterface.h"
#include "HWInterface/D19cL1ReadoutInterface.h"
#include "HWInterface/D19cTriggerInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/StartInfo.h"
#include "Utils/Utilities.h"
#include <boost/math/distributions/normal.hpp>
#include <iomanip>
#include <sstream>

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

std::string OTTimeCorrelations::fCalibrationDescription = "Make time correlation plots for list of settings";

OTTimeCorrelations::OTTimeCorrelations() : Tool() {}

OTTimeCorrelations::~OTTimeCorrelations() {}

void OTTimeCorrelations::SetThresholds(float numberOfSigma)
{
    // Logic adapted from OTPhysics::SetThresholds
    float theStripSigma;
    float thePixelSigma;
    if(numberOfSigma == 0)
    {
        uint16_t theMaximumChannelNumber  = MAXCICCLUSTERS / 2;
        float    theStripAllowedOccupancy = float(theMaximumChannelNumber) / (NSSACHANNELS * NCHIPS_OT);
        float    thePixelAllowedOccupancy = float(theMaximumChannelNumber) / (NSSACHANNELS * NMPAROWS * NCHIPS_OT);

        boost::math::normal gaus(0, 1);
        theStripSigma = quantile(complement(gaus, theStripAllowedOccupancy));
        thePixelSigma = quantile(complement(gaus, thePixelAllowedOccupancy));
    }
    else
    {
        theStripSigma = numberOfSigma;
        thePixelSigma = numberOfSigma;
    }
    LOG(INFO) << "Setting thresholds with StripSigma: " << theStripSigma << " PixelSigma: " << thePixelSigma;

    for(auto pBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *pBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid)
                {
                    float theSigma     = (cChip->getFrontEndType() == FrontEndType::SSA2) ? theStripSigma : thePixelSigma;
                    float theThreshold = cChip->getAveragePedestal() + cChip->getAverageNoise() * theSigma;
                    fReadoutChipInterface->WriteChipReg(cChip, "Threshold", std::round(theThreshold));
                }
            }
        }
    }
}

void OTTimeCorrelations::ConfigureCalibration()
{
    LOG(INFO) << "[OTTimeCorrelations::ConfigureCalibration()] Retrieving parameters from settings";
    // Retrieve parameters from settings
    fNeventsConf = this->findValueInSettings<double>("OTTimeCorrelations_Nevents", 1000);
    LOG(INFO) << "Read fNeventsConf from config file: " << fNeventsConf;

    theThresholdSigma       = convertStringToFloatList(this->findValueInSettings<std::string>("OTTimeCorrelations_ThresholdSigma", "3.0, 3.0"));
    theNTriggerPerBurst     = convertStringToFloatList(this->findValueInSettings<std::string>("OTTimeCorrelations_NTriggersPerBurst", "4, 4"));
    theDelayBetweenTriggers = convertStringToFloatList(this->findValueInSettings<std::string>("OTTimeCorrelations_DelayBetweenTriggers", "0, 1"));
    theAverageFrequency     = convertStringToFloatList(this->findValueInSettings<std::string>("OTTimeCorrelations_AverageFrequency", "400, 400"));

    fSaveRawData = this->findValueInSettings<double>("OTTimeCorrelations_SaveRawData", 1);

    // check if the sizes match: if they do not, don't run
    if((theThresholdSigma.size() != theNTriggerPerBurst.size()) || (theThresholdSigma.size() != theDelayBetweenTriggers.size()) || (theThresholdSigma.size() != theAverageFrequency.size()))
    {
        LOG(ERROR) << "Mismatch in sizes of settings vectors! Please check the configuration.";
        throw std::runtime_error("Mismatch in sizes of settings vectors");
    }
}

void OTTimeCorrelations::Running()
{
    LOG(INFO) << "[OTTimeCorrelations::Running] Starting";

    if(fSaveRawData == true)
    {
        char      runString[7];
        const int theRunNumber = Tool::fRunNumber;
        sprintf(runString, "%06d", theRunNumber);
        this->addFileHandler(fDirectoryName + "/run_" + runString + ".raw", 'w');
        this->initializeWriteFileHandler();
    }

    // Reset readout
    for(auto theBoard: *fDetectorContainer)
    {
        auto theFWInterface      = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getObject((theBoard)->getId())));
        auto theReadoutInterface = theFWInterface->getL1ReadoutInterface();
        theReadoutInterface->ResetReadout();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Resync chips
    for(const auto cBoard: *fDetectorContainer) static_cast<D19cFWInterface*>(this->fBeBoardFWMap[static_cast<BeBoard*>(cBoard)->getId()])->ChipReSync();

    StartInfo theStartInfo;
    theStartInfo.setRunNumber(fRunNumber);
    SystemController::Start(theStartInfo);

    DetectorDataContainer theStripModuleHitContainer;
    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint32_t, MAXCICCHANNELS * 2 + 1>>(*fDetectorContainer, theStripModuleHitContainer);
    DetectorDataContainer thePixelModuleHitContainer;
    ContainerFactory::copyAndInitOpticalGroup<GenericDataArray<uint32_t, MAXCICCHANNELS * 2 + 1>>(*fDetectorContainer, thePixelModuleHitContainer);

    fTotalDataSize = 0;

    OTTimeCorrelationStripData theStripData;
    OTTimeCorrelationPixelData thePixelData;
    ChipErrorData              chipErrors;

    for(size_t iteration = 0; iteration < theDelayBetweenTriggers.size(); ++iteration)
    {
        // iterate over settings
        LOG(INFO) << BOLDYELLOW << " Starting iteration " << iteration << RESET;
        OTTimeCorrelationStripData::reset();
        OTTimeCorrelationPixelData::reset();
        ChipErrorData::reset();
        OTTimeCorrelationConfig::reset();

        setIterationSettings(iteration);

        uint32_t collectedEvents     = 0;
        uint32_t iter_ev_error_count = 0;
        uint32_t iter_tot_ev         = 0;
        uint32_t triggerPerBurst     = OTTimeCorrelationConfig::getTriggerPerBurst();
        uint32_t readSize            = std::floor(65535 / triggerPerBurst);
        uint32_t triggerDelay        = OTTimeCorrelationConfig::getTriggerDelay();

        if(triggerDelay > 0) { readSize = readSize * triggerPerBurst; }

        LOG(INFO) << "  collectedEvents: " << collectedEvents << "  readSize: " << readSize << "  triggerDelay: " << triggerDelay << "  fNevents: " << fNevents;

        // loop over events
        for(auto theBoard: *fDetectorContainer)
        {
            // start iteration data taking: loop to read events in batches until we reach fNeventsConf
            while(collectedEvents < fNeventsConf)
            {
                uint32_t nToRead;
                if(triggerDelay > 0) { nToRead = std::min(readSize, fNeventsConf - collectedEvents); }
                else { nToRead = std::min(readSize, fNevents - collectedEvents / triggerPerBurst); }

                if(nToRead == 0) { break; }
                ReadNEvents(theBoard, nToRead);

                // Process all collected events
                const std::vector<Event*>& events = GetEvents();
                collectedEvents += events.size();
                LOG(INFO) << "Collected events: " << collectedEvents << " / " << fNeventsConf;
                int ev_error_count = 0;
                int tot_ev         = 0;
                for(auto opticalGroup: *theBoard)
                {
                    // loop over events
                    for(const auto& event: events)
                    {
                        // decode the event and fill the custom structure
                        auto             ev = static_cast<D19cCic2Event*>(event);
                        std::vector<int> stripsOn;
                        std::vector<int> pixelsOn;

                        uint32_t cStripModuleHits = 0;
                        uint32_t cPixelModuleHits = 0;
                        for(auto hybrid: *opticalGroup)
                        {
                            for(auto chip: *hybrid)
                            {
                                tot_ev += 1;
                                if(ev->IsL1ErrorSet(hybrid->getId(), chip->getId()) != 0)
                                {
                                    ev_error_count += 1;
                                    if(chip->getFrontEndType() == FrontEndType::SSA2) { ChipErrorData::ssaErrors[(int)chip->getId() % 8] += 1; }
                                    else { ChipErrorData::mpaErrors[(int)chip->getId() % 8] += 1; }
                                }

                                auto     hits       = ev->GetHits(hybrid->getId(), chip->getId());
                                uint32_t cEventHits = hits.size();
                                if(chip->getFrontEndType() == FrontEndType::SSA2)
                                {
                                    for(auto& hit: hits)
                                    {
                                        auto second      = hit.second;
                                        int  stripNumber = (hybrid->getId() % 2) * 960 + chip->getId() * 120 + second;
                                        stripsOn.push_back(stripNumber);
                                    }
                                    cStripModuleHits += cEventHits;
                                }
                                else if(chip->getFrontEndType() == FrontEndType::MPA2)
                                {
                                    for(auto& hit: hits)
                                    {
                                        auto second      = hit.second;
                                        int  pixelNumber = (hybrid->getId() % 2) * 960 + (chip->getId() % 8) * 120 + second;
                                        pixelsOn.push_back(pixelNumber);
                                    }
                                    cPixelModuleHits += cEventHits;
                                }
                            }
                        }

                        auto bunchId = ev->GetBunch();

                        theStripData.stripData = stripsOn; // vector containing hit strip numbers

                        pixelsOn.erase(std::unique(pixelsOn.begin(), pixelsOn.end()), pixelsOn.end()); // remove duplicates

                        thePixelData.pixelData = pixelsOn; // vector containing hit pixel numbers

                        // update BX in the data structures
                        OTTimeCorrelationConfig::updateBX(bunchId);

                        OTTimeCorrelationStripData::update();

                        OTTimeCorrelationStripData::compute_same_tcorr();
                        OTTimeCorrelationStripData::compute_min_hits();
                        OTTimeCorrelationStripData::compute_3D_corr();

                        OTTimeCorrelationPixelData::update();

                        OTTimeCorrelationPixelData::compute_same_tcorr();
                        OTTimeCorrelationPixelData::compute_min_hits();
                        OTTimeCorrelationPixelData::compute_3D_corr();

                        theStripModuleHitContainer.getObject(theBoard->getId())
                            ->getObject(opticalGroup->getId())
                            ->getSummary<GenericDataArray<uint32_t, MAXCICCHANNELS * 2 + 1>>()
                            .at(cStripModuleHits) += 1;

                        thePixelModuleHitContainer.getObject(theBoard->getId())
                            ->getObject(opticalGroup->getId())
                            ->getSummary<GenericDataArray<uint32_t, MAXCICCHANNELS * 2 + 1>>()
                            .at(cPixelModuleHits) += 1;

#ifdef __USE_ROOT__
                        fDQMHistogramOTTimeCorrelation.fillSSAData(theStripData, triggerPerBurst, triggerDelay, iterationSettingsName);
                        fDQMHistogramOTTimeCorrelation.fillMPAData(thePixelData, triggerPerBurst, triggerDelay, iterationSettingsName);
                        fDQMHistogramOTTimeCorrelation.fillErrorHist(chipErrors, triggerPerBurst, triggerDelay, iterationSettingsName);
                        fDQMHistogramOTTimeCorrelation.fill2DhistSlices(theStripData, thePixelData, triggerPerBurst, triggerDelay, iterationSettingsName);
                        // fDQMHistogramOTTimeCorrelation.fill3Dhistograms(theStripData, thePixelData, triggerPerBurst, triggerDelay, iterationSettingsName);
#endif
                        // LOG(INFO) << "----------------------------------------";
                    } // end of event loop
                    iter_ev_error_count += ev_error_count;
                    iter_tot_ev += tot_ev;
                }
            } // end of data taking while for this setting
        } // end of board loop
#ifdef __USE_ROOT__
        fDQMHistogramOTTimeCorrelation.reportTimingStats();
        fDQMHistogramOTTimeCorrelation.fillModuleHitPlots(theStripModuleHitContainer, true, iterationSettingsName);
        fDQMHistogramOTTimeCorrelation.fillModuleHitPlots(thePixelModuleHitContainer, false, iterationSettingsName);
#endif
        LOG(INFO) << "Iteration summary - chips with errors: " << iter_ev_error_count << "/" << iter_tot_ev << " = " << (iter_tot_ev > 0 ? (float)iter_ev_error_count / iter_tot_ev * 100 : 0) << "%";
        if(OTTimeCorrelationConfig::getTriggerPerBurst() > 1)
        {
            LOG(INFO) << "Iteration summary - bursts with wrong size: " << OTTimeCorrelationConfig::mismatchedBursts << "/" << OTTimeCorrelationConfig::totBursts << " = "
                      << (OTTimeCorrelationConfig::totBursts > 0 ? (float)OTTimeCorrelationConfig::mismatchedBursts / OTTimeCorrelationConfig::totBursts * 100 : 0) << "%";
        }
    } // end of iteration setting loop
    Stop(); // end of acquisition
}

void OTTimeCorrelations::Stop()
{
    LOG(INFO) << "[OTTimeCorrelations::Stop] Stopping";
    Tool::Stop();
}

void OTTimeCorrelations::SetTriggerSource(uint8_t pTriggerSource)
{
    // Set trigger source in the board
    std::vector<std::pair<std::string, uint32_t>> boardRegisterVector;
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", pTriggerSource});
    for(auto theBoard: *fDetectorContainer) { fBeBoardInterface->WriteBoardMultReg(theBoard, boardRegisterVector); }

    LOG(INFO) << "Trigger source set to " << +pTriggerSource;
}

void OTTimeCorrelations::setIterationSettings(size_t iteration)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<std::pair<std::string, uint32_t>> boardRegisterVector;
    // set the appropriate values based on the config
    if(theNTriggerPerBurst.at(iteration) > 1)
    {
        OTTimeCorrelationConfig::setN((size_t)theNTriggerPerBurst.at(iteration) / 2);
        if(theDelayBetweenTriggers.at(iteration) > 0)
        {
            LOG(INFO) << BOLDYELLOW << "Version: triggers in burst with variable delay" << RESET;
            // compute delay between pullses based on avg freq
            auto delay = (40000 * theNTriggerPerBurst.at(iteration)) / theAverageFrequency.at(iteration) - theNTriggerPerBurst.at(iteration) * (theDelayBetweenTriggers.at(iteration) + 1); // not sure
            LOG(INFO) << "Delay before next pulse: " << delay << " AverageFrequency in BX: " << 40000 / theAverageFrequency.at(iteration);
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.delay_before_next_pulse", delay});
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.consecutive_delay_between_trigger", theDelayBetweenTriggers.at(iteration)});
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.test_pulse.consecutive_triggers_per_burst", theNTriggerPerBurst.at(iteration) - 2});
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", 0});
            SetTriggerSource(8);
            fNevents = fNeventsConf;
            OTTimeCorrelationConfig::setTriggerDelay(theDelayBetweenTriggers.at(iteration));
        }
        else
        {
            LOG(INFO) << BOLDYELLOW << "Version: consecutive triggers in burst" << RESET;
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", theNTriggerPerBurst.at(iteration) - 1});
            boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", theAverageFrequency.at(iteration) / theNTriggerPerBurst.at(iteration)});
            SetTriggerSource(3);
            fNevents = fNeventsConf / theNTriggerPerBurst.at(iteration);
            OTTimeCorrelationConfig::setTriggerDelay(0); // they can only be consecutive
            LOG(INFO) << "Delay before next pulse: " << 40000 * theNTriggerPerBurst.at(iteration) / theAverageFrequency.at(iteration)
                      << " AverageFrequency in BX: " << 40000 / theAverageFrequency.at(iteration);
        }
    }
    else
    {
        LOG(INFO) << BOLDYELLOW << "Version: single trigger" << RESET;
        boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.misc.trigger_multiplicity", theNTriggerPerBurst.at(iteration) - 1});
        boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.user_trigger_frequency", theAverageFrequency.at(iteration)});
        SetTriggerSource(3);
        fNevents = fNeventsConf;
        OTTimeCorrelationConfig::setN(100);
    }

    // boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.trigger_source", theTriggerSource});
    boardRegisterVector.push_back({"fc7_daq_cnfg.fast_command_block.triggers_to_accept", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.tlu_block.tlu_enabled", 0});
    boardRegisterVector.push_back({"fc7_daq_cnfg.readout_block.global.data_handshake_enable", 0});
    boardRegisterVector.push_back({"fc7_daq_ctrl.fast_command_block.control.load_config", 0x1});

    SetThresholds(theThresholdSigma.at(iteration));

    for(auto theBoard: *fDetectorContainer) { fBeBoardInterface->WriteBoardMultReg(theBoard, boardRegisterVector); }

    OTTimeCorrelationConfig::setTriggerPerBurst(theNTriggerPerBurst.at(iteration));

    LOG(INFO) << "Configuring OTTimeCorrelations with Nevents: " << fNevents << "  ThresholdSigma: " << theThresholdSigma.at(iteration)
              << "  NTriggerPerBurst: " << int(theNTriggerPerBurst.at(iteration)) << "  DelayBetweenTriggers: " << int(theDelayBetweenTriggers.at(iteration))
              << "  AverageFrequency: " << theAverageFrequency.at(iteration);

    std::ostringstream s_name_stream;
    s_name_stream << "s" << std::fixed << std::setprecision(1) << theThresholdSigma.at(iteration);
    std::string s_name = s_name_stream.str();
    std::replace(s_name.begin(), s_name.end(), '.', 'p');
    std::ostringstream iteration_name_stream;
    iteration_name_stream << "_tBurst" << int(theNTriggerPerBurst.at(iteration)) << "_tDel" << int(theDelayBetweenTriggers.at(iteration)) << "_rAvg" << int(theAverageFrequency.at(iteration)) << "_"
                          << s_name;
    iterationSettingsName = iteration_name_stream.str();

#ifdef __USE_ROOT__
    fDQMHistogramOTTimeCorrelation.book(fResultFile, *fDetectorContainer, fSettingsMap, iterationSettingsName, theThresholdSigma.at(iteration));
#endif
}
