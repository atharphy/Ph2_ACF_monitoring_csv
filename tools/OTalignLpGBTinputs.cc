#include "tools/OTalignLpGBTinputs.h"
#include "HWInterface/ExceptionHandler.h"
#include "System/RegisterHelper.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

OTalignLpGBTinputs::OTalignLpGBTinputs() : Tool() {}

OTalignLpGBTinputs::~OTalignLpGBTinputs() {}

void OTalignLpGBTinputs::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::LpGBT, "^EPRX\\d{2}ChnCntr$");

    for(const auto cBoard: *fDetectorContainer)
    {
        // force trigger source to be internal triggers
        LOG(INFO) << BOLDYELLOW << "Forcing trigger source to internal triggers" << RESET;
        fBeBoardInterface->WriteBoardReg(cBoard, "fc7_daq_cnfg.fast_command_block.trigger_source", 3);
    }

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTalignLpGBTinputs.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTalignLpGBTinputs::ConfigureCalibration() {}

void OTalignLpGBTinputs::AlignLpGBTInputs()
{
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            LOG(INFO) << BOLDYELLOW << "LinkAlignmentOT::AlignLpGBTInputs ..." << RESET;
            auto cBoardId   = theOpticalGroup->getBeBoardId();
            auto cBoardIter = std::find_if(fDetectorContainer->begin(), fDetectorContainer->end(), [&cBoardId](Ph2_HwDescription::BeBoard* x) { return x->getId() == cBoardId; });
            // stop triggers to make sure that there are no L1 packets from the CIC
            fBeBoardInterface->Stop((*cBoardIter));

            LOG(INFO) << BOLDMAGENTA << "Aligning CIC-lpGBT data on OpticalGroup#" << +theOpticalGroup->getId() << RESET;
            auto& clpGBT = theOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;

            // configure CICs to output alignment pattern on stub lines
            std::vector<uint8_t> cFeEnableRegs(0);
            for(auto cHybrid: *theOpticalGroup)
            {
                auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                // disable alignment output
                fCicInterface->SelectOutput(cCic, true);
                cFeEnableRegs.push_back(fCicInterface->ReadChipReg(cCic, "FE_ENABLE"));
                fCicInterface->EnableFEs(cCic, {0, 1, 2, 3, 4, 5, 6, 7}, false);
            }
            std::vector<uint8_t> cEportGroups;
            std::vector<uint8_t> cEportChnls;
            for(auto cHybrid: *theOpticalGroup)
            {
                std::vector<uint8_t> cGroups;
                std::vector<uint8_t> cChannels;
                if(theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S)
                {
                    if(cHybrid->getId() % 2 == 0)
                    {
                        cGroups   = {0, 4, 4, 5, 5, 6};
                        cChannels = {0, 0, 2, 0, 2, 0};
                    }
                    else
                    {
                        cGroups   = {0, 1, 1, 2, 2, 3};
                        cChannels = {2, 0, 2, 0, 2, 2};
                    }
                }
                else // PS
                {
                    if(cHybrid->getId() % 2 == 0)
                    {
                        cGroups   = {4, 4, 5, 5, 6, 6, 0};
                        cChannels = {2, 0, 2, 0, 2, 0, 0};
                    }
                    else
                    {
                        cGroups   = {0, 1, 1, 2, 2, 3, 3};
                        cChannels = {2, 0, 2, 0, 2, 0, 2};
                    }
                }
                for(auto cGrp: cGroups) cEportGroups.push_back(cGrp);
                for(auto cChnl: cChannels) cEportChnls.push_back(cChnl);
            }
            auto cMode = flpGBTInterface->PhaseAlignRx(clpGBT, cEportGroups, cEportChnls);
            if(cMode == 15)
            {
                LOG(INFO) << BOLDRED << "FAILED to align LpGBT inputs on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId() << " --- OpticalGroup will be disabled"
                          << RESET;
                ExceptionHandler::getInstance()->disableOpticalGroup(theBoard->getId(), theOpticalGroup->getId());
                continue;
            }
            for(size_t cIndx = 0; cIndx < cEportGroups.size(); cIndx++) { flpGBTInterface->ConfigureRxPhase(clpGBT, cEportGroups[cIndx], cEportChnls[cIndx], cMode); }
        }
    }
}

void OTalignLpGBTinputs::Running()
{
    LOG(INFO) << "Starting OTalignLpGBTinputs measurement.";
    Initialise();
    AlignLpGBTInputs();
    LOG(INFO) << "Done with OTalignLpGBTinputs.";
    Reset();
}

void OTalignLpGBTinputs::Stop(void)
{
    LOG(INFO) << "Stopping OTalignLpGBTinputs measurement.";
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: processing the histograms
    fDQMHistogramOTalignLpGBTinputs.process();
#endif
    dumpConfigFiles();
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTalignLpGBTinputs stopped.";
}

void OTalignLpGBTinputs::Pause() {}

void OTalignLpGBTinputs::Resume() {}

void OTalignLpGBTinputs::Reset() { fRegisterHelper->restoreSnapshot(); }
