#include "OTTool.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;


#include "../Utils/ContainerFactory.h"

OTTool::OTTool() : Tool()
{
    fBoardRegContainer.reset();
    fBrdRegsToPerserve.clear();
    fROCRegsToPerserve.reset();
    fSuccess = false;
    fMyName = "OTTool";
}

OTTool::~OTTool() {}
// Reset register on BeBoard + ROCs 
void OTTool::Reset()
{
    LOG(INFO) << BOLDGREEN << "Resetting registers touched  by " << fMyName << RESET;
    // set everything back to original values .. like I wasn't here
    bool cWithPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << fMyName << ":Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap)
        {
        	// skip registers that I should perserve for this board 
            if( std::find( fBrdRegsToPerserve.begin(), fBrdRegsToPerserve.end() , cReg.first ) != fBrdRegsToPerserve.end() ){ 
                LOG (INFO) << BOLDBLUE << "Will not reconfigure " << cReg.first << RESET;
                continue;
            }
            cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second));
        }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);

        auto& cROCRegsToPreserveThisBrd = fROCRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {

        	auto& cROCRegsToPreserveThisOG = cROCRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                bool cIsPS    = (cWithSSA && cWithMPA) && cWithLpGBT;
                cWithPS       = cWithPS || cIsPS;
                LOG(INFO) << BOLDBLUE << fMyName << ":Resetting all registers on readout chips connected to FEhybrid#" << +(cHybrid->getId()) << " back to their original values..." << RESET;
                auto& cROCRegsToPreserveThisHybrd = cROCRegsToPreserveThisOG->at(cHybrid->getIndex());
            	for(auto cChip: *cHybrid)
                {
                	auto& cROCRegsToPreserveThisROC = cROCRegsToPreserveThisHybrd->at(cChip->getIndex());
                	auto& cRegsToPerserve = cROCRegsToPreserveThisROC->getSummary<std::vector<std::string>>(); 
                	if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->UpdateModifiedRegisterMap(cChip);
                    auto cModMap = fReadoutChipInterface->GetModifiedRegisterMap(cChip);
                    LOG(INFO) << BOLDBLUE << "Chip#" << +cChip->getId() << " map of modified registers contains " << cModMap.size() << " items." << RESET;
                    for(auto cMapItem: cModMap)
                    {
                    	// skip registers that I should perserve for this ROC 
                        if( std::find( cRegsToPerserve.begin(), cRegsToPerserve.end() , cMapItem.first ) != cRegsToPerserve.end() ) continue;
        				
        				auto cValueInMemory = cChip->getReg(cMapItem.first);
                        LOG(INFO) << BOLDBLUE << fMyName << "::Resetting Register " << cMapItem.first << " on Chip#" << +cChip->getId() << " from " << cValueInMemory << " to "
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

// Initialization function
void OTTool::Prepare()
{
    // retreive original settings for all chips and all back-end boards
    fBoardRegContainer.reset();
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        auto&                cBoardRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        const BeBoardRegMap& cOrigRegMap  = static_cast<const BeBoard*>(cBoard)->getBeBoardRegMap();
        cBoardRegMap.insert(cOrigRegMap.begin(), cOrigRegMap.end());
    }

    // check sparsification 
    for(auto cBoard: *fDetectorContainer)
    {
        bool cSparsified = (fBeBoardInterface->ReadBoardReg(cBoard, "fc7_daq_cnfg.physical_interface_block.cic.2s_sparsified_enable") == 1);
        cBoard->setSparsification(cSparsified);
    }    

    // clear map of modified registers
    // probably this should be a container per board 
    // since we should be able to mix different types of boards 
    // but need to decide if this is per board/OG/hybrid 
    // TO-DO 
    fWithCIC = 0;
    fWithLpGBT = 0 ; 
    fWithSSA   = 0 ; 
    fWithMPA   = 0 ; 
    fWithCBC   = 0 ; 
    if(fReadoutChipInterface != nullptr)
    {
        fReadoutChipInterface->ClearModifiedRegisterMap();
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                fWithLpGBT = (cOpticalGroup->flpGBT != nullptr) ? 1 : 0 ;
                fWithCIC        = fWithLpGBT;
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto& cCic = static_cast<OuterTrackerHybrid*>(cHybrid)->fCic;
                    fWithCIC   = fWithCIC || (cCic != nullptr);
                    fWithSSA = fWithSSA || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::SSA; }) != cHybrid->end()) ? 1 : 0 ;
                    fWithMPA = fWithMPA || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::MPA; }) != cHybrid->end()) ? 1 : 0 ;
                    fWithCBC = fWithCBC || (std::find_if(cHybrid->begin(), cHybrid->end(), [](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == FrontEndType::CBC3; }) != cHybrid->end()) ? 1 : 0 ;
                }
            }
        }
        bool cIsPS = (fWithSSA && fWithMPA) && fWithLpGBT;
        if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
    }

    // prepare list of ROC registers to perserve 
    fDetectorDataContainer = &fROCRegsToPerserve;
    ContainerFactory::copyAndInitChip<std::vector<std::string>>(*fDetectorContainer, *fDetectorDataContainer);

    // retreive number of events from settings file
    fNevents = findValueInSettings("Nevents", 10);

    fSuccess = false;
}

// configure print-out options
void OTTool::ConfigurePrintout(PrintConfig pCnfg)
{
    fPrintConfig.fVerbose=pCnfg.fVerbose;
    fPrintConfig.fPrintEvery=pCnfg.fPrintEvery;
}
// set list of board registers to perserve
void OTTool::SetBrdRegstoPerserve(std::vector<std::string> pListOfRegs)
{
    fBrdRegsToPerserve.clear();
    for (const auto& cRegName : pListOfRegs){ 
        LOG (INFO) << BOLDBLUE << "Adding " << cRegName << " to list of Brd Regs to perserve..." << RESET;
        fBrdRegsToPerserve.push_back(cRegName);
    }
}
// set list of ROC registers to perserve
void OTTool::SetROCRegstoPerserve(FrontEndType pType, std::vector<std::string> pListOfRegs)
{
    for(auto cBoard: *fDetectorContainer)
    {
        auto& cROCRegsToPreserveThisBrd = fROCRegsToPerserve.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {

            auto& cROCRegsToPreserveThisOG = cROCRegsToPreserveThisBrd->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cROCRegsToPreserveThisHybrd = cROCRegsToPreserveThisOG->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid)
                {
                    if( cChip->getFrontEndType() != pType ) continue;

                    auto& cROCRegsToPreserveThisROC = cROCRegsToPreserveThisHybrd->at(cChip->getIndex());
                    auto& cRegsToPerserve = cROCRegsToPreserveThisROC->getSummary<std::vector<std::string>>();
                    cRegsToPerserve.clear();
                    for (const auto& cRegName : pListOfRegs) cRegsToPerserve.push_back(cRegName);
                }//ROCs
            }//Hybrds
        }//OGs
    }//brd
}

// read data from file 
void OTTool::ReadDataFromFile(std::string pRawFileName)
{
    this->addFileHandler(pRawFileName, 'r');
    std::vector<uint32_t> cData;
    this->readFile(cData);
    LOG(INFO) << BOLDBLUE << "BeamTestCheck2S::ReadDataFromFile Read back " << +cData.size() << " 32-bit words from the .raw file : " << pRawFileName << RESET;
    for(auto cBoard: *fDetectorContainer)
    {
        size_t cNevents = 1;
        DecodeData(cBoard, cData, cNevents, fBeBoardInterface->getBoardType(cBoard));
        // const std::vector<Event*>& cEvents = GetEvents ();
        LOG(INFO) << BOLDBLUE << "BeamTestCheck2S::ReadDataFromFile decoded back " << +cNevents << " events from the .raw file [BeBoard#" << +cBoard->getId() << "]" << RESET;
        if( fPrintConfig.fVerbose ) PrintData(cBoard);
    }
}

// poll board from data 
// next TO-DO - make each board run in a separate thread 
void OTTool::ContinousReadout()
{
    // for now this is is done in sequence.. in principle we should do this in parralel for all boards
    for(auto cBoard: *fDetectorContainer)
    {
        ContinousReadout(cBoard);
    }
}

// print data 
void   OTTool::PrintData(BeBoard* pBoard)
{
    const std::vector<Event*>& cEvents  = GetEvents();
    LOG (INFO) << BOLDBLUE << "Printing events from FC7.. collected : " << +cEvents.size() << " events." << RESET;
    for(auto& cEvent: cEvents) 
    {
        EventPrintout(pBoard, cEvent);              
    }
}

// continuous readout 
void OTTool::ContinousReadout(BeBoard* pBoard)
{
    std::vector<uint32_t> cCompleteData(0);
    fBeBoardInterface->Start(pBoard);
    size_t   cCounter = 0;
    uint32_t cNevents = 0;
    bool     cBreak   = false;
    bool     cWait    = false;
    do
    {
        std::this_thread::sleep_for(std::chrono::microseconds(fReadoutPause));
        std::vector<uint32_t> cData(0);
        cNevents += ReadData(pBoard, cData, cWait);
        if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
        auto cTriggerCounter = fBeBoardInterface->getFirmwareInterface()->ReadReg("fc7_daq_stat.fast_command_block.trigger_in_counter");
        if(cCounter % 100 == 0)
            LOG(INFO) << BOLDMAGENTA << "BeamTestCheck2S continuousReadout loop ... " << +cTriggerCounter << " triggers received and " << +cNevents << " events readout so far... " << RESET;
        cCounter++;
        cBreak = (cNevents >= fNevents);
    } while(!cBreak);
    fBeBoardInterface->Stop(pBoard);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    std::vector<uint32_t> cData(0);
    cNevents += ReadData(pBoard, cData, false);
    if(cData.size() != 0) std::move(cData.begin(), cData.end(), std::back_inserter(cCompleteData));
    DecodeData(pBoard, cCompleteData, cNevents, fBeBoardInterface->getBoardType(pBoard));
    LOG(INFO) << BOLDYELLOW << "BeamTestCheck2S::ContinousReadout readout " << cNevents << " when " << fNevents << " were requested." << RESET;
}

// event print-out 
void   OTTool::EventPrintout(BeBoard* pBoard, Event* pEvent)
{
    auto cSparsified  = pBoard->getSparsification();
    if( cSparsified ) LOG (DEBUG) << BOLDBLUE << "Checking with internal - sparisified data" << RESET;
    else LOG (DEBUG) << BOLDBLUE << "Checking with internal - un-sparisified data" << RESET;

    if( pEvent->GetEventCount()%fPrintConfig.fPrintEvery != 0 ) return;
    std::stringstream cEvntHeader; 
    cEvntHeader << "Event#" << +pEvent->GetEventCount() ; 
    for(auto cOpticalGroup: *pBoard)
    {
        for(auto cHybrid: *cOpticalGroup)
        {
            auto cL1IdCIC      = static_cast<D19cCic2Event*>(pEvent)->L1Id(cHybrid->getId(), 0);
            auto cL1Status     = static_cast<D19cCic2Event*>(pEvent)->L1Status(cHybrid->getId());
            auto cBxId         = (pEvent)->BxId(cHybrid->getId());
            auto cStubStat     = static_cast<D19cCic2Event*>(pEvent)->Status(cHybrid->getId());
            std::stringstream cOut;
            if( cStubStat != 0x00 || cL1Status != 0x00 ) 
                cOut << BOLDRED  << cEvntHeader.str()
                    << " L1Id " << +cL1IdCIC 
                    << " Stub status is " << std::bitset<8>(cStubStat) 
                    << " L1 status [FEs] is " << std::bitset<8>(cL1Status) 
                    << " L1 status [CIC] is " << std::bitset<1>(cL1Status&0x1) 
                    << " BxId is " << +cBxId ;
            else 
                cOut << BOLDGREEN  << cEvntHeader.str()
                    << " L1Id " << +cL1IdCIC 
                    << " Stub status is " << std::bitset<8>(cStubStat) 
                    << " L1 status [FEs] is " << std::bitset<8>(cL1Status) 
                    << " L1 status [CIC] is " << std::bitset<1>(cL1Status&0x1) 
                    << " BxId is " << +cBxId ;
            for( auto cChip : *cHybrid )
            {
                if(!cSparsified) break;
                auto   cClusters    = (pEvent)->getClusters(cHybrid->getId(), cChip->getId());
                cOut << BOLDBLUE << "\t..ROC#" << +cChip->getId() << " has " << +cClusters.size() << " clusters." << RESET;
            }
            LOG (INFO) << cOut.str() << RESET;
        }
    }   
}




