#include "tools/PixelAlive.h"
#include "HWDescription/Cbc.h"
#include "HWDescription/SSA.h"
#include "HWInterface/D19cFWInterface.h"
#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/Container.h"
#include "Utils/ContainerFactory.h"
#include "Utils/ContainerSerialization.h"
#include "Utils/EmptyContainer.h"
#include "Utils/MPAChannelGroupHandler.h"
#include "Utils/Occupancy.h"
#include "Utils/SSAChannelGroupHandler.h"
#include "Utils/ThresholdAndNoise.h"
#include "boost/format.hpp"
#include <math.h>

#ifdef __USE_ROOT__
// static_assert(false,"use root is defined");
#include "DQMUtils/DQMHistogramPixelAlive.h"
#endif

PixelAlive::PixelAlive() : Tool() {}

PixelAlive::~PixelAlive() { clearDataMembers(); }

void PixelAlive::cleanContainerVector()
{
}

void PixelAlive::clearDataMembers()
{
    return;
    if(fDisableStubLogic)
    {
        delete fStubLogicValue;
        delete fHIPCountValue;
    }
    cleanContainerVector();
}

//Does Setup before any data taking
void PixelAlive::Initialise()
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
//        LOG(INFO) << BOLDYELLOW << "Noise measured on BeBoard#" << +cBoard->getId() << " with a trigger rate of " << cTriggerFreq << "kHz." << RESET;
    }
    fDisableStubLogic = false;



    this->enableTestPulse(true); //For Testing Purposes

    fWithSSA = false;
    fWithMPA = false;
    std::vector<FrontEndType> cAllFrontEndTypes;
    for(auto cBoard: *fDetectorContainer)
    {
        auto cFrontEndTypes = cBoard->connectedFrontEndTypes();
        fWithSSA            = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::SSA2) != cFrontEndTypes.end();
        fWithMPA = std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA) != cFrontEndTypes.end() ||
                   std::find(cFrontEndTypes.begin(), cFrontEndTypes.end(), FrontEndType::MPA2) != cFrontEndTypes.end();
        for(auto cFrontEndType: cFrontEndTypes)
        {
            if(std::find(cAllFrontEndTypes.begin(), cAllFrontEndTypes.end(), cFrontEndType) == cAllFrontEndTypes.end()) cAllFrontEndTypes.push_back(cFrontEndType);
        }
    }
    if(fWithSSA && !fWithMPA) LOG(INFO) << BOLDBLUE << "PixelAlive with SSAs" << RESET;
    if(fWithMPA && !fWithSSA) LOG(INFO) << BOLDBLUE << "PixelAlive with MPAs" << RESET;
    if(fWithSSA && fWithMPA) LOG(INFO) << BOLDBLUE << "PixelAlive with SSAs+MPAs" << RESET;

    for(auto cFrontEndType: cAllFrontEndTypes)
    {
        if(cFrontEndType == FrontEndType::SSA || cFrontEndType == FrontEndType::SSA2)
        {
            SSAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
        else if(cFrontEndType == FrontEndType::MPA || cFrontEndType == FrontEndType::MPA2)
        {
            MPAChannelGroupHandler theChannelGroupHandler;
            theChannelGroupHandler.setChannelGroupParameters(1, NSSACHANNELS * NMPACOLS); // 16*2*8
            setChannelGroupHandler(theChannelGroupHandler, cFrontEndType);
        }
    }

    initializeRecycleBin();

//    fAllChan = pAllChan;

	//Reads in options from xml file, has corresponding stuff in header
    fSkipMaskedChannels          = findValueInSettings<double>("SkipMaskedChannels", 0); //after comma is default
    
    fPixelAliveMask          = findValueInSettings<double>("PixelAliveMask", 1); //Mask Pixels?
    fEventsPerPoint          = findValueInSettings<double>("PixelAliveNevents", 1000);
    fNEventsPerBurst = (fEventsPerPoint >= fMaxNevents) ? fMaxNevents : fEventsPerPoint;
    fPulseAmplitude_MPA      = findValueInSettings<double>("PixelAlivePulseAmplitude_MPA", 145);
    fPulseAmplitude_SSA      = findValueInSettings<double>("PixelAlivePulseAmplitude_SSA", 132);
    fPulseThreshold_MPA      = findValueInSettings<double>("PixelAliveThreshold_MPA", 225);
    fPulseThreshold_SSA      = findValueInSettings<double>("PixelAliveThreshold_SSA", 111);
    LOG(INFO) << "Parsed settings:";
    LOG(INFO) << " Nevents = " << fEventsPerPoint;
//    std::cout << __PRETTY_FUNCTION__ << " MPA PulseAmplitude " << +fPulseAmplitude_MPA << std::endl;
    std::cout << " MPA Injection Charge " << +fPulseAmplitude_MPA << std::endl;
    std::cout << " MPA Threshold " << +fPulseThreshold_MPA << std::endl;
    std::cout << " SSA Injection Charge " << +fPulseAmplitude_SSA << std::endl;
    std::cout << " SSA Threshold " << +fPulseThreshold_SSA << std::endl;
//    LOG(INFO) << " MPA Injection Charge = " << fPulseAmplitude_MPA << " Threshold = " << fPulseThreshold_MPA;
//    LOG(INFO) << " SSA Injection Charge = " << fPulseAmplitude_SSA << " Threshold = " << fPulseThreshold_SSA;


    // LOG(INFO) << " Fast Counter Readout [PS] " << +cEnableFastCounterReadout << RESET;

	//Saves original board config, so set back at the end
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegNap = fBoardRegContainer.getObject(cBoard->getId())->getSummary<BeBoardRegMap>();
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

    // for now.. force to use async mode here (does not track timing of hits), only used for testing
    bool cForcePSasync = true;
    // event types
    fEventTypes.clear();
    for(auto cBoard: *fDetectorContainer)
    {
        fEventTypes.push_back(cBoard->getEventType());
        if(!fWithSSA && !fWithMPA) continue;
        if(!cForcePSasync) continue;
        cBoard->setEventType(EventType::PSAS); //Sets up board to expect async input
        static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface())->InitializePSCounterFWInterface(cBoard);
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                for(auto cChip: *cHybrid) { fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 0); } //Tells chip to expect async
            }
        }
    }

//Once I have the histogram code working add this back in
#ifdef __USE_ROOT__
    LOG(INFO) << BOLDYELLOW << "DQMHistogramPixelAlive Book" << RESET;
    fDQMHistogramPixelAlive.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

//Resets all registers saved in initialize, will need to look this over
void PixelAlive::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by PixelAlive" << RESET;
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.getObject(cBoard->getId())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) { cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second)); }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);
        

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cHybrid: *cOpticalGroup)
            {
                LOG(INFO) << BOLDBLUE << "PixelAlive::Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;

                for(auto cChip: *cHybrid)
                {
		    // Set this back to what it was 
 		    fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 0);
		    
                    auto cModMap = cChip->GetModifiedRegisterMap();
                    LOG(INFO) << BOLDYELLOW << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    std::vector<std::pair<std::string, uint16_t>> cRegList;
                    for(auto cMapItem: cModMap)
                    {
                        auto cValueInMemory = cChip->getReg(cMapItem.first);
                        if(cMapItem.second.fValue == cValueInMemory) continue;
                        // don't reconfigure the offsets .. whole point of this excercise
                        if(cMapItem.first.find("ENFLAGS") != std::string::npos) { cMapItem.second.fValue = (cMapItem.second.fValue & 0xfe) + (cValueInMemory & 0x1); }; //This conserves pixel masking
//                        LOG(INFO) << BOLDYELLOW << "PixelAlive::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to " << cMapItem.second.fValue << RESET;
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

//Print Out Results and runs functions
void PixelAlive::measurePixels()
{
    LOG(INFO) << BOLDBLUE << "Measure Pixel Occupancy" << RESET;
    measureOccupancy();
//Commenting out plotting for now
//    LOG(INFO) << BOLDBLUE << "Produce PixelAlive Plots" << RESET;
//    producePixelAlivePlots();
    LOG(INFO) << BOLDBLUE << "Done" << RESET;

}



//Make own version of plotmaker
void PixelAlive::producePixelAlivePlots()
{
#ifdef __USE_ROOT__
//    fDQMHistogramPixelAlive.fillPedestalAndNoisePlots(*fThresholdAndNoiseContainer);


#endif
}


//Make own version of channel masking
void PixelAlive::maskNoisyChannels(BoardDataContainer* board)
{
/*
    for(auto opticalGroup: *board)
    {
        for(auto hybrid: *opticalGroup)
        {
            for(auto chip: *hybrid)
            {
                LOG(INFO) << BOLDYELLOW << chip->getId() << RESET;
                auto chipDC = fDetectorContainer->at(board->getId())->at(opticalGroup->getId())->at(hybrid->getId())->at(chip->getId());
                // auto cType = chipDC->getFrontEndType();

                // uint32_t       NCH = NCHANNELS;
                // if(cType == FrontEndType::CBC3)
                //   NCH = NCHANNELS;
                // else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                //  NCH = NSSACHANNELS;
                // else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                //  NCH = NMPACHANNELS;

//                float fMean=1.0;
//                        if(cType == FrontEndType::MPA or  cType == FrontEndType::MPA2)
//                     fMean=2.7;
//                        if(cType == FrontEndType::SSA or  cType == FrontEndType::SSA2)
//                     fMean=4.2;
                auto     cOriginalMask = chipDC->getChipOriginalMask();
//                uint32_t nMask         = 0;

                for(uint16_t iChannel = 0; iChannel < chip->size(); ++iChannel)
                {
//                    float cPedestal = chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fThreshold;
                    // float cNoise = chip->getSummary<ThresholdAndNoise, ThresholdAndNoise>().fNoise;
                    // LOG(INFO) << BOLDYELLOW << "CHECK "<<iChannel <<", "<<chip->getChannel<ThresholdAndNoise>(iChannel).fNoise<<" "<<fPedeNoiseLimit*fMean<<RESET;
                    // LOG(INFO) << BOLDYELLOW << "CHECK "<<iChannel <<", "<<std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold -cPedestal)<<" "<<fPedeNoiseUntrimmedLimit<<RESET;
                    if(fPedeNoiseMask and (chip->getChannel<ThresholdAndNoise>(iChannel).fNoise > fPedeNoiseLimit))
                    {
                        nMask += 1;
                        LOG(INFO) << BOLDYELLOW << "Masking Channel: " << iChannel << " with a noise of " << chip->getChannel<ThresholdAndNoise>(iChannel).fNoise << ", which is over the limit of "
                                  << fPedeNoiseLimit << RESET;
                        cOriginalMask->disableChannel(iChannel); //Make version of this for masking pixels
                    }
                    if(fPedeNoiseMaskUntrimmed and std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold - cPedestal) > fPedeNoiseUntrimmedLimit)
                    {
                        uint8_t thetrim = fReadoutChipInterface->ReadChipReg(static_cast<ReadoutChip*>(chipDC), "TrimDAC_P" + std::to_string(iChannel + 1));

                        nMask += 1;
                        LOG(INFO) << BOLDYELLOW << "Masking Channel:  " << iChannel << " with a pedestal difference of "
                                  << std::fabs(chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold - cPedestal) << ", which is over the limit of " << fPedeNoiseUntrimmedLimit
                                  << " trimval: " << +thetrim << RESET;
                        cOriginalMask->disableChannel(iChannel); //This is where the channel is disabled
                    }

                    // LOG(INFO) << BOLDYELLOW << "snorp SUMMARY TH "<<cPedestal <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "snorp SUMMARY NOI "<<cNoise <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "fPedeNoiseLimit "<<fPedeNoiseLimit<< " fPedeNoiseMask "<<fPedeNoiseMask <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "Noise "<<iChannel<< ": "<<chip->getChannel<ThresholdAndNoise>(iChannel).fNoise <<RESET;
                    // LOG(INFO) << BOLDYELLOW << "Thresh "<<iChannel<< ": "<<chip->getChannel<ThresholdAndNoise>(iChannel).fThreshold  <<RESET;
                }
                // fReadoutChipInterface->maskChannelGroup(chipDC,cOriginalMask);
                if(nMask > 0) LOG(INFO) << BOLDYELLOW << "PedeNoise masked " << nMask << " channels..." << RESET;
                fReadoutChipInterface->ConfigureChipOriginalMask(chipDC);
            }
 
       }
    }
*/
}

void PixelAlive::writeObjects()
{
#ifdef __USE_ROOT__
    LOG(INFO) << BOLDYELLOW << "DQMHistogramPixelAlive Process" << RESET;
    fDQMHistogramPixelAlive.process();
#endif
}


//Modify for PixelAlive to run functions (Initialize, measurePixels, Reset)

void PixelAlive::Run()
{
    LOG(INFO) << "Starting Pixel Occupancy Measurement";
    Initialise();
    measurePixels();
    LOG(INFO) << "Done With Occupancy Measurement";
    Reset();
}

//Keep without changing

void PixelAlive::Stoper()
{
    LOG(INFO) << "Stopping Pixel Occupancy Measurement";
    Reset();
    writeObjects();
    dumpConfigFiles(); //Will Dump configuration (pixel masking is only thing changed) into results directory, copy to MPA files if we want to use same settings
    SaveResults();
    closeFileHandler();
    clearDataMembers();
    LOG(INFO) << "Pixel Occupancy Measurement Stopped.";
}

//These need to be here
void PixelAlive::Pause() {}

void PixelAlive::Resume() {}

//Measure Pixel Occupancy
void PixelAlive::measureOccupancy()
{
    for(auto cBoard: *fDetectorContainer)
    {
        if(fWithSSA || fWithMPA)
        {
            // Allow for different SSA and MPA injection amplitudes
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType = cChip->getFrontEndType();

                        if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
			{
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitude_MPA);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fPulseThreshold_MPA);
			}
                        else
			{
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", fPulseAmplitude_SSA);
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", fPulseThreshold_SSA);
			}
                    }
                }
            }
        }

        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fPulseAmplitude_MPA);
    }



    auto epsilon     	    = findValueInSettings<double>("PixelEpsilon", 0.05); //Acceptable occupancy > 1 - epsilon

    //DetectorDataContainer* theOccupancyContainer = fRecycleBin.get(&ContainerFactory::copyAndInitStructure<Occupancy>, Occupancy());
    DetectorDataContainer theOccupancyContainer;
    fDetectorDataContainer = &theOccupancyContainer;
    ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

//    bool originalAllChannelFlag = this->fAllChan;
//    this->SetTestAllChannels(true);
    
    this->measureData(fEventsPerPoint, fNEventsPerBurst); //Sends pulses and measures occupancy

#ifdef __USE_ROOT__
    LOG(INFO) << BOLDMAGENTA << "Making DQMHistogramPixelAlive Plots" << RESET;
    fDQMHistogramPixelAlive.fillOccupancyPlots(theOccupancyContainer);
#else

    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PixelAliveValidation");
        theContainerSerialization.streamByHybridContainer(fDQMStreamer, theOccupancyContainer);
    }
#endif
    LOG(INFO) << "measuring with " << fEventsPerPoint << " events and " << fNEventsPerBurst << " per burst";
    LOG(INFO) << BOLDMAGENTA << "PixelAlive Epsilon: " << epsilon << RESET;


    uint32_t nMask_MPA = 0; //number of masked pixels (MPA)
    uint32_t nMask_SSA = 0; //number of masked pixels (SSA)
    
    for(auto cBoard: *fDetectorContainer)
    {
//	auto cBoardIdx = cBoard->getId();
        for(auto cOpticalGroup: *cBoard)
        {
//            auto cOpticalGroupIdx = cOpticalGroup->getId();
            for(auto cHybrid: *cOpticalGroup)
            {
//		auto cHybridIdx = cHybrid->getId();
                for(auto cChip: *cHybrid)
                {	//Add one more loop to loop through each channel within each chip
			   
		    LOG(INFO) << BLUE << "Pixel Alive Testing: Board(" << cBoard->getId() << ") Optical Group(" << cOpticalGroup->getId() << ") Hybrid(" << cHybrid->getId() << ") Chip(" << cChip->getId() << ")"<< RESET;
 		    fReadoutChipInterface->WriteChipReg(cChip, "AnalogueAsync", 0);
//                    auto cChipIdx       = cChip->getId();
		    auto cType          = cChip->getFrontEndType();
		    uint32_t NCH = NCHANNELS;
		    if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                        NCH = NSSACHANNELS;
                    else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                        NCH = NMPACHANNELS;

		    auto chipDC = fDetectorContainer->getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId());
		    auto cOriginalMask = chipDC->getChipOriginalMask();
		    
                    for(uint32_t iChan = 0; iChan < NCH; iChan++)
		    {
//			LOG (INFO) << RED << "Ch " << iChan << RESET ;
			float occupancy = theOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<Occupancy>(iChan).fOccupancy;
			
			if(occupancy > (1.0-epsilon))
			{
			 //   LOG(INFO) << BLUE << "Occupancy Acceptable" << RESET;
			}
			else
			{
			 //   LOG(INFO) << RED << "Occupancy Unaccptable" << RESET;
			    LOG(INFO) << RED << "Masking Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			    if(fPixelAliveMask)
			    {
			    	cOriginalMask->disableChannel(iChan); //Mask failing pixel
			    	if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                	    	    nMask_SSA++;
        	            	else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                	    	    nMask_MPA++;
			    }
			}
			if(occupancy >= 1.01)
			{
			    LOG(INFO) << RED << "Occupancy Above 1.0" << RESET;
			}
		    }

		    
		}
	    }
	}
    }
    if(fPixelAliveMask)
    {
    	LOG (INFO) << BOLDRED << "Masked " << nMask_SSA << " SSA Channels" << RESET ;
    	LOG (INFO) << BOLDRED << "Masked " << nMask_MPA << " MPA Channels" << RESET ;
    }
}

//Measure Pixel Occupancy In 2D Plots For Debugging
void PixelAlive::channelTest()
{
    LOG(INFO) << BOLDBLUE << "Performing Channel Test" << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        if(fWithSSA || fWithMPA)
        {
            //Initialize Threshold and Injection Charge to initial values
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        auto cType          = cChip->getFrontEndType();
		                if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 85);
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 80);
                        }
                        else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                        {
                            fReadoutChipInterface->WriteChipReg(cChip, "Threshold", 200);
                            fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", 95);
                        }
                    }
                }
            }
        }

        else
            setSameDacBeBoard(static_cast<BeBoard*>(cBoard), "TestPulsePotNodeSel", fPulseAmplitude_MPA);
    }


    LOG(INFO) << "measuring with " << fEventsPerPoint << " events and " << fNEventsPerBurst << " per burst";


    TH2F* ssa_chan10 = new TH2F("ssa_chan10", "SSA Channel 10 Occupancy in Threshold vs. Injection Charge", 50, 85, 135, 200, 30, 230);
    TH2F* ssa_chan20 = new TH2F("ssa_chan20", "SSA Channel 20 Occupancy in Threshold vs. Injection Charge", 50, 85, 135, 200, 30, 230);
    TH2F* ssa_chan40 = new TH2F("ssa_chan40", "SSA Channel 40 Occupancy in Threshold vs. Injection Charge", 50, 85, 135, 200, 30, 230);
    TH2F* ssa_chan80 = new TH2F("ssa_chan80", "SSA Channel 80 Occupancy in Threshold vs. Injection Charge", 50, 85, 135, 200, 30, 230);
    TH2F* ssa_chan110 = new TH2F("ssa_chan110", "SSA Channel 110 Occupancy in Threshold vs. Injection Charge", 50, 85, 135, 200, 30, 230);

    TH2F* mpa_chan119 = new TH2F("mpa_chan119", "MPA Channel 119 Occupancy in Threshold vs. Injection Charge", 50, 200, 250, 200, 45, 245);
    TH2F* mpa_chan241 = new TH2F("mpa_chan241", "MPA Channel 241 Occupancy in Threshold vs. Injection Charge", 50, 200, 250, 200, 45, 245);
    TH2F* mpa_chan350 = new TH2F("mpa_chan350", "MPA Channel 350 Occupancy in Threshold vs. Injection Charge", 50, 200, 250, 200, 45, 245);
    TH2F* mpa_chan601 = new TH2F("mpa_chan601", "MPA Channel 601 Occupancy in Threshold vs. Injection Charge", 50, 200, 250, 200, 45, 245);
    TH2F* mpa_chan1300 = new TH2F("mpa_chan1300", "MPA Channel 1300 Occupancy in Threshold vs. Injection Charge", 50, 200, 250, 200, 45, 245);


    ssa_chan10->GetZaxis()->SetRangeUser(0.0, 1.1);
    ssa_chan20->GetZaxis()->SetRangeUser(0.0, 1.1);
    ssa_chan40->GetZaxis()->SetRangeUser(0.0, 1.1);
    ssa_chan80->GetZaxis()->SetRangeUser(0.0, 1.1);
    ssa_chan110->GetZaxis()->SetRangeUser(0.0, 1.1);

    mpa_chan119->GetZaxis()->SetRangeUser(0.0, 1.1);
    mpa_chan241->GetZaxis()->SetRangeUser(0.0, 1.1);
    mpa_chan350->GetZaxis()->SetRangeUser(0.0, 1.1);
    mpa_chan601->GetZaxis()->SetRangeUser(0.0, 1.1);
    mpa_chan1300->GetZaxis()->SetRangeUser(0.0, 1.1);

    ssa_chan10->SetXTitle("Threshold");
    ssa_chan10->SetYTitle("Injection Charge");
    ssa_chan10->SetZTitle("Occupancy");
    ssa_chan20->SetXTitle("Threshold");
    ssa_chan20->SetYTitle("Injection Charge");
    ssa_chan20->SetZTitle("Occupancy");
    ssa_chan40->SetXTitle("Threshold");
    ssa_chan40->SetYTitle("Injection Charge");
    ssa_chan40->SetZTitle("Occupancy");
    ssa_chan80->SetXTitle("Threshold");
    ssa_chan80->SetYTitle("Injection Charge");
    ssa_chan80->SetZTitle("Occupancy");
    ssa_chan110->SetXTitle("Threshold");
    ssa_chan110->SetYTitle("Injection Charge");
    ssa_chan110->SetZTitle("Occupancy");

    mpa_chan119->SetXTitle("Threshold");
    mpa_chan119->SetYTitle("Injection Charge");
    mpa_chan119->SetZTitle("Occupancy");
    mpa_chan241->SetXTitle("Threshold");
    mpa_chan241->SetYTitle("Injection Charge");
    mpa_chan241->SetZTitle("Occupancy");
    mpa_chan350->SetXTitle("Threshold");
    mpa_chan350->SetYTitle("Injection Charge");
    mpa_chan350->SetZTitle("Occupancy");
    mpa_chan601->SetXTitle("Threshold");
    mpa_chan601->SetYTitle("Injection Charge");
    mpa_chan601->SetZTitle("Occupancy");
    mpa_chan1300->SetXTitle("Threshold");
    mpa_chan1300->SetYTitle("Injection Charge");
    mpa_chan1300->SetZTitle("Occupancy");
    
    for(int thresh = 0; thresh <= 50; thresh++)
    {
	int ssa_thresh = thresh + 85;
	int mpa_thresh = thresh + 200;
        //for(int inj = thresh; inj <= 200; inj++)	//min injection is threshold
        for(int inj = thresh-50; inj <= thresh+50; inj++)
        { 
	    int ssa_inj = ssa_thresh + inj - 5;
	    int mpa_inj = mpa_thresh + inj - 105;
            LOG (INFO) << MAGENTA << "SSA Threshold: " << ssa_thresh << " Injection Charge: " << ssa_inj << RESET ;
            LOG (INFO) << MAGENTA << "MPA Threshold: " << mpa_thresh << " Injection Charge: " << mpa_inj << RESET ;
            
            DetectorDataContainer theOccupancyContainer;
            fDetectorDataContainer = &theOccupancyContainer;
            ContainerFactory::copyAndInitStructure<Occupancy>(*fDetectorContainer, *fDetectorDataContainer);

            bool originalAllChannelFlag = this->fAllChan;
            this->SetTestAllChannels(true);
            this->measureData(fEventsPerPoint, fNEventsPerBurst); //Sends pulses and measures occupancy
            this->SetTestAllChannels(originalAllChannelFlag);
            
            for(auto cBoard: *fDetectorContainer)
            {
//	        auto cBoardIdx = cBoard->getId();
                for(auto cOpticalGroup: *cBoard)
                {
  //                  auto cOpticalGroupIdx = cOpticalGroup->getId();
                    for(auto cHybrid: *cOpticalGroup)
                    {
//		                auto cHybridIdx = cHybrid->getId();
                        for(auto cChip: *cHybrid)
                        {	//Add one more loop to loop through each channel within each chip
			   
                            auto cChipIdx       = cChip->getId();
		                    auto cType          = cChip->getFrontEndType();
		                    uint32_t NCH = NCHANNELS;
                            //Set Injection and Threshold for next round
		                    if((cType == FrontEndType::SSA || cType == FrontEndType::SSA2) && inj == thresh+50)
                            {
                                NCH = NSSACHANNELS;
                                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", ssa_thresh+1);
                                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", ssa_inj-99);
            			LOG (INFO) << MAGENTA << "Set New SSA Threshold: " << ssa_thresh+1 << " New Injection Charge: " << ssa_inj-99 << RESET ;
                            }
                            else if((cType == FrontEndType::MPA || cType == FrontEndType::MPA2) && inj == thresh+50)
                            {
                                NCH = NMPACHANNELS;
                                fReadoutChipInterface->WriteChipReg(cChip, "Threshold", mpa_thresh+1);
                                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", mpa_inj-99);
            			LOG (INFO) << MAGENTA << "Set New MPA Threshold: " << mpa_thresh+1 << " New Injection Charge: " << mpa_inj-99 << RESET ;
                            }
		                    else if(cType == FrontEndType::SSA || cType == FrontEndType::SSA2)
                            {
                                NCH = NSSACHANNELS;
                                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", ssa_inj+1);
                            }
                            else if(cType == FrontEndType::MPA || cType == FrontEndType::MPA2)
                            {
                                NCH = NMPACHANNELS;
                                fReadoutChipInterface->WriteChipReg(cChip, "InjectedCharge", mpa_inj+1);
                            }
                            for(uint32_t iChan = 0; iChan < NCH; iChan++)
		                    {
			                    float occupancy = theOccupancyContainer.getObject(cBoard->getId())->getObject(cOpticalGroup->getId())->getObject(cHybrid->getId())->getObject(cChip->getId())->getChannel<Occupancy>(iChan).fOccupancy;

			                    if(cChipIdx == 0 && iChan == 10) //ssa10
			                    {
			                        ssa_chan10->SetBinContent(ssa_thresh-84, ssa_inj-29, occupancy);
						
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 0 && iChan == 20) //ssa20
			                    {
			                        ssa_chan20->SetBinContent(ssa_thresh-84, ssa_inj-29, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 0 && iChan == 40) //ssa40
			                    {
			                        ssa_chan40->SetBinContent(ssa_thresh-84, ssa_inj-29, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 0 && iChan == 80) //ssa80
			                    {
			                        ssa_chan80->SetBinContent(ssa_thresh-84, ssa_inj-29, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 0 && iChan == 110) //ssa110
			                    {
			                        ssa_chan110->SetBinContent(ssa_thresh-84, ssa_inj-29, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 8 && iChan == 119) //mpa119
			                    {
			                        mpa_chan119->SetBinContent(mpa_thresh-199, mpa_inj-44, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 8 && iChan == 241) //mpa241
			                    {
			                        mpa_chan241->SetBinContent(mpa_thresh-199, mpa_inj-44, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 8 && iChan == 350) //mpa350
			                    {
			                        mpa_chan350->SetBinContent(mpa_thresh-199, mpa_inj-44, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 8 && iChan == 601) //mpa601
			                    {
			                        mpa_chan601->SetBinContent(mpa_thresh-199, mpa_inj-44, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
                                else if(cChipIdx == 8 && iChan == 1300) //mpa1300
			                    {
			                        mpa_chan1300->SetBinContent(mpa_thresh-199, mpa_inj-44, occupancy);
						LOG(INFO) << BLUE << "Chip(" << cChipIdx << ") Channel(" << iChan << ") Occupancy = " << occupancy << RESET;
			                    }
		                    }
		                }       
	                }
	            }
            }
        }
    }
    fResultFile->WriteObject(ssa_chan10, "ssa10_thesh_inj");
    fResultFile->WriteObject(ssa_chan20, "ssa20_thesh_inj");
    fResultFile->WriteObject(ssa_chan40, "ssa40_thesh_inj");
    fResultFile->WriteObject(ssa_chan80, "ssa80_thesh_inj");
    fResultFile->WriteObject(ssa_chan110, "ssa110_thesh_inj");
    fResultFile->WriteObject(mpa_chan119, "mpa119_thesh_inj");
    fResultFile->WriteObject(mpa_chan241, "mpa241_thesh_inj");
    fResultFile->WriteObject(mpa_chan350, "mpa350_thesh_inj");
    fResultFile->WriteObject(mpa_chan601, "mpa601_thesh_inj");
    fResultFile->WriteObject(mpa_chan1300, "mpa1300_thesh_inj");


//#ifdef __USE_ROOT__
//    if(fPlotSCurves) fDQMHistogramPedeNoise.fillSCurvePlots(cStripValue, cPixelValue, *theOccupancyContainer);
//#endif

    // this->HttpServerProcess();
//    LOG(DEBUG) << YELLOW << "Found minimal and maximal occupancy " << cMinBreakCount << " times, SCurves finished! " << RESET;
}
