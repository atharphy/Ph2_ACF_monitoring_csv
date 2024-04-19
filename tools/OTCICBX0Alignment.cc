#include "tools/OTCICBX0Alignment.h"
#include "HWInterface/ExceptionHandler.h"
#include "HWInterface/FastCommandInterface.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWDescription/BeBoard.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTCICBX0Alignment::fCalibrationDescription = "Perform the BX0 alignment between FE chips and CIC. Should be done after the word alignment.";

OTCICBX0Alignment::OTCICBX0Alignment() : Tool() {}

OTCICBX0Alignment::~OTCICBX0Alignment() {}

void OTCICBX0Alignment::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "^BX0_DELAY$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "^EXT_BX0_DELAY$");
    fRegisterHelper->freeFrontEndRegister(FrontEndType::CIC2, "^BX0_ALIGN_CONFIG$");

#ifdef __USE_ROOT__ // to disable and anable ROOT by command
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTCICBX0Alignment.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTCICBX0Alignment::ConfigureCalibration()
{

}

void OTCICBX0Alignment::Running()
{
    LOG(INFO) << "Starting OTCICBX0Alignment measurement.";
    Initialise();
    BX0Alignment();
    LOG(INFO) << "Done with OTCICBX0Alignment.";
    Reset();
}

void OTCICBX0Alignment::Stop(void)
{
    LOG(INFO) << "Stopping OTCICBX0Alignment measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTCICBX0Alignment.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTCICBX0Alignment stopped.";
}

void OTCICBX0Alignment::Pause()
{

}


void OTCICBX0Alignment::Resume()
{

}


void OTCICBX0Alignment::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTCICBX0Alignment::BX0Alignment(uint32_t pWait_us)
{
    LOG(INFO) << BOLDBLUE << "Starting CIC automated BX0 alignment procedure .... " << RESET;
    std::string theQueryFunction = "skipSSAQuery";
    auto        theSkipSSAquery  = [](const ChipContainer* theReadoutChip) {
        if(static_cast<const ReadoutChip*>(theReadoutChip)->getFrontEndType() == FrontEndType::SSA2) return false;
        return true;
    };
    fDetectorContainer->addReadoutChipQueryFunction(theSkipSSAquery, theQueryFunction);

    DetectorDataContainer theBX0AlignmentDelayContainer;
    ContainerFactory::copyAndInitHybrid<uint16_t>(*fDetectorContainer, theBX0AlignmentDelayContainer);

    for(auto theBoard: *fDetectorContainer)
    {
        uint16_t maxAttempts = 1;
        uint16_t currentAttempt = 0;
        
            for(auto theOpticalGroup: *theBoard)
            {
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;


                    //Making sure to use an enabled FE                
                    auto theFEtoUse = theHybrid->getFirstObject(); //  *(theHybrid->begin());
                    if(theFEtoUse == nullptr)
                    {
                        LOG(INFO) << BOLDRED << "No FE suitable for BX0 alignment enabled on Board id " << +cCic->getBeBoardId() << " OpticalGroup id" << +cCic->getOpticalGroupId() << " Hybrid id " << +cCic->getHybridId()
                        << " --- Hybrid will be disabled" << RESET;
                        ExceptionHandler::getInstance()->disableHybrid(cCic->getBeBoardId(), cCic->getOpticalGroupId(), cCic->getHybridId());
                        return;
                    }
                    std::cout << " the FE to use is " << +theFEtoUse->getId() << std::endl;
                    auto theFECICmapping = fCicInterface->getMapping(cCic);
                    uint8_t theIndex = (theFEtoUse->getFrontEndType() == FrontEndType::MPA2)? theFEtoUse->getId() - 8: theFEtoUse->getId(); 
                    uint8_t theFEId = theFECICmapping[theIndex];
                    // configure word alignment pattern on FEs
                    std::vector<uint8_t> cAlignmentPatterns = fReadoutChipInterface->getBX0AlignmentPatterns();
                    uint8_t pLine = 0;
                    if(theOpticalGroup->getFrontEndType() == FrontEndType::OuterTracker2S) pLine = 4;
                    fCicInterface->PrepareForAutomatedBX0Alignment(cCic, cAlignmentPatterns, pLine, theFEId);
                    for(uint8_t cIndex = 0; cIndex < (uint8_t)cAlignmentPatterns.size(); cIndex += 1)
                    {
                         LOG(INFO) << BOLDBLUE << "Calibration pattern set on readout chip on stub line " << +cIndex << " set to " << std::bitset<8>(cAlignmentPatterns[cIndex]) << RESET;
                    }
                    
                    std::cout << " BXO alignment pattern for chip " << +theFEtoUse->getId() << std::endl;
                    fReadoutChipInterface->produceBX0AlignmentPattern(theFEtoUse); 
                

                        // LOG(INFO) << BOLDMAGENTA << " CHECK MPA OUTPUT! " << RESET;
                        // auto cInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());

                        // uint8_t numberOfBytesInSinglePacket = (static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10) ? 2 : 1;
                        // D19cDebugFWInterface* theDebugInterface = cInterface->getDebugInterface();
                        // fCicInterface->SelectOutput(cCic, false);
                        // fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", theHybrid->getId());
                        // fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
                        // for(uint phyPort=0; phyPort<10; ++phyPort)
                        // {
                        //     uint8_t registerValue = 0x10 + phyPort;
                        //     fCicInterface->WriteChipReg(cCic, "MUX_CTRL", registerValue);
                        //     for(size_t iteration = 0; iteration < 1; iteration++)
                        //     {

                        //         auto lineOutputVector = theDebugInterface->StubDebug(false, 4, false);
                        //         size_t cNlines = 4;
                        //         for(size_t line=0; line<cNlines; ++line)
                        //         {
                        //             LOG(INFO) << BOLDRED << "Line " << line << " -> " <<std::hex << getPatternPrintout(lineOutputVector[line], numberOfBytesInSinglePacket,true) << std::dec << RESET;
                        //         }
                        //     }

                        // }

                } //hybrids
            } //optical group
        
        // LOG(INFO) << BOLDRED << " Sending ONE ONLY Resync !" << RESET;     
        // fBeBoardInterface->ChipReSync(theBoard);

        while(currentAttempt < maxAttempts)
        {
            //Send Resync to all hybrids connected to one board at once
            // LOG(INFO) << BOLDRED << "NOT Sending Resync !" << RESET;
            // LOG(INFO) << BOLDRED << "NOT Sending Resync !" << RESET;
            // LOG(INFO) << BOLDRED << "NOT Sending Resync !" << RESET;       
            LOG(INFO) << BOLDMAGENTA << " Sending Resync !" << RESET;     
            auto cInterface          = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface());
            auto cFastCommandInterface = cInterface->getFastCommandInterface();
            uint8_t theNumberOfResyncs = 5;
            cFastCommandInterface->SendGlobalCounterResetResync(theNumberOfResyncs);
            // fBeBoardInterface->ChipReSync(theBoard);
            for(auto theOpticalGroup: *theBoard)
            {    
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    bool  cSuccessAlign          = fCicInterface->CheckAutomatedBX0Alignment(cCic);

                    // bool  cSuccessAlign          = fCicInterface->AutomatedBX0Alignment(cCic, cAlignmentPatterns);
                    auto& theBX0AlignmentValues = theBX0AlignmentDelayContainer.getObject(theBoard->getId())
                                                       ->getObject(theOpticalGroup->getId())
                                                       ->getObject(theHybrid->getId())
                                                       ->getSummary<uint16_t>();
                    theBX0AlignmentValues = fCicInterface->retrieveExternalBX0AlignmentValues(cCic);
                    cSuccessAlign          = cSuccessAlign && fCicInterface->ConfigureExternalBX0Alignment(cCic, theBX0AlignmentValues);
                    if(cSuccessAlign) { LOG(INFO) << BOLDBLUE << "Automated BX0 alignment procedure " << BOLDGREEN << " SUCCEEDED!" << RESET; }
                    else
                    {
                        LOG(INFO) << BOLDRED << "Automated BX0 alignment procedure " << BOLDRED << " FAILED!" << RESET;
                        LOG(INFO) << BOLDRED << "FAILED CIC BX0 alignment word on Board id " << +theBoard->getId() << " OpticalGroup id" << +theOpticalGroup->getId() << " Hybrid id"
                                  << +theHybrid->getId() << " --- Hybrid will be disabled" << RESET;
                        ExceptionHandler::getInstance()->disableHybrid(theBoard->getId(), theOpticalGroup->getId(), theHybrid->getId());
                        continue;
                    }
                    



                } // hybrids 
            } // optical group
            ++currentAttempt;            
        }
        for(auto theOpticalGroup: *theBoard)
        {    
                for(auto theHybrid: *theOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(theHybrid)->fCic;
                    fCicInterface->SetStaticBX0Alignment(cCic);
                }
        }
    }

#ifdef __USE_ROOT__
    fDQMHistogramOTCICBX0Alignment.fillBX0AlignmentDelay(theBX0AlignmentDelayContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theBX0AlignmentDelayContainerSerialization("OTCICBX0AlignmentBX0AlignmentDelay");
        theBX0AlignmentDelayContainerSerialization.streamByOpticalGroupContainer(fDQMStreamer, theBX0AlignmentDelayContainer);
    }
#endif

    fDetectorContainer->removeReadoutChipQueryFunction(theQueryFunction);
}