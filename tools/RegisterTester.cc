#include "RegisterTester.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

RegisterTester::RegisterTester() : Tool() { fNBadRegisters = 0; }

// D'tor
RegisterTester::~RegisterTester() {}

void RegisterTester::Initialise()
{
    // this is needed if you're going to use groups anywhere
    initializeRecycleBin();
    
    // read back original masks
    const auto cTimeStart = std::chrono::system_clock::now();
    fStartTime            = std::chrono::duration_cast<std::chrono::seconds>(cTimeStart.time_since_epoch()).count();
    LOG(INFO) << BOLDMAGENTA << "RegisterTester::Initialise at " << fStartTime << " s from epoch." << RESET;


    // clear map of modified registers
    fReadoutChipInterface->ClearModifiedRegisterMap();
    bool cIsPS = false;
    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            bool cWithLpGBT = (cOpticalGroup->flpGBT != nullptr);
            for(auto cHybrid: *cOpticalGroup)
            {
                if(cIsPS) continue;

                auto cType    = FrontEndType::SSA;
                bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cType         = FrontEndType::MPA;
                bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                cIsPS         = (cWithSSA && cWithMPA) && cWithLpGBT;
            }
        }
    }
    if(cIsPS) static_cast<PSInterface*>(fReadoutChipInterface)->ResetModifiedRegisterMap();
}

void RegisterTester::RegisterTest()
{
    // 0, increasing page order 
    // 1, decreasing page order 
    // 2, don't sort  
    uint8_t cSortOrder=0;
    size_t  cLimitPerPage=1;
    for( size_t cAttempt = 0; cAttempt < 10; cAttempt++ )
    {
       // first I want to record the register map for this map 
       // retreive original settings for all chips and all back-end boards
        DetectorDataContainer cRegListContainer;
        ContainerFactory::copyAndInitChip<Registers>(*fDetectorContainer, cRegListContainer);
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        Registers&       cRegList     = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        const ChipRegMap& cOriginalMap = cChip->getRegMap();
                        for( auto cMapItem : cOriginalMap )
                        {
                            cRegList.push_back( std::make_pair(cMapItem.first, cMapItem.second ) );
                        }
                        // registers sorted by ... all on page 0 then all on page 1 
                        if(cSortOrder == 0) std::sort(cRegList.begin(), cRegList.end(), customLessThanPage); // all to page0 then page 1
                        if(cSortOrder == 1) std::sort(cRegList.begin(), cRegList.end(), customGreaterThanPage); // all to page1 then page 0
                    }
                }
            }
        }// board loop to save record of registers 

        // so here .. I need to send a hard reset to the ROCs 
        for(auto cBoard: *fDetectorContainer)
        {
            auto cWithLpGBT = fReadoutChipInterface->lpGBTCheck(cBoard);
            if( !cWithLpGBT ){ fBeBoardInterface->ChipReset(cBoard); continue; } 
            for(auto cOpticalGroup: *cBoard)
            {
                auto& clpGBT = cOpticalGroup->flpGBT;
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto cType        = FrontEndType::CBC3;
                    bool cWithCBC = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cType         = FrontEndType::SSA;
                    bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cType         = FrontEndType::MPA;
                    bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());

                    if( cWithCBC ) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cHybrid->getId()%2 );
                    if( cWithSSA ) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetSSA(clpGBT, cHybrid->getId()%2 );
                    if( cWithMPA ) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, cHybrid->getId()%2 );

                }
            }
        }// board loop to send hard reset 

        // container to store default register values after a hard reset 
        DetectorDataContainer cDefRegListContainer;
        ContainerFactory::copyAndInitChip<Registers>(*fDetectorContainer, cDefRegListContainer);
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        Registers&       cOriginaList  = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        Registers&       cList          = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        // set page to that of the first register in the map  
                        //if( cChip->getFrontEndType() == FrontEndType::CBC3 ) static_cast<CbcInterface*>(fReadoutChipInterface)->ConfigurePage( cChip, 1);//(*cOriginalMap.begin()).second.fPage );  
                        for( auto cListItem : cOriginaList )
                        {
                            auto cRegItem = cListItem.second; 
                            cRegItem.fValue  = fReadoutChipInterface->ReadChipReg(cChip, cListItem.first); 
                            cList.push_back( std::make_pair(cListItem.first, cRegItem) ); 
                            // LOG (INFO) << BOLDMAGENTA << "\t... Register " << cListItem.first << " default value after a hard reset is 0x" 
                            //     << std::hex << +cRegItem.fValue << std::dec 
                            //     << " value after configuration should be 0x" << std::hex << cListItem.second.fValue << std::dec 
                            //     << RESET;
                        }//map
                     }//chip
                }//hybrid
            }//OG
        }// board loop to save record of registers 


        // now .. try and re-write original values after config and look for mis-matches 
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        LOG (INFO) << BOLDMAGENTA << "Test#" << +cAttempt << " I2C registers on ROC" << +cChip->getId() << RESET;
                        Registers&       cList  = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        Registers        cOriginalList; 
                        size_t           cNPage0Regs, cNPage1Regs = 0;
                        cNPage0Regs = cNPage1Regs;
                        for( auto cItem : cList )
                        {
                            bool cPushBack = ( cItem.second.fPage == 0 && cNPage0Regs < cLimitPerPage ) || ( cItem.second.fPage == 1 && cNPage1Regs < cLimitPerPage ); 
                            if( !cPushBack ) continue; 
                            
                            cOriginalList.push_back(cItem); 
                            if( cItem.second.fPage == 0 ) cNPage0Regs++; 
                            if( cItem.second.fPage == 1 ) cNPage1Regs++; 
                        }
                        Registers&       cCurrentList   = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        Registers cSensitiveRegisters; cSensitiveRegisters.clear();
                        std::vector<std::string> cRegistersChecked; cRegistersChecked.clear();
                        for( auto cConfigItem : cOriginalList ) 
                        {
                            // auto cWithLpGBT = fReadoutChipInterface->lpGBTCheck(cBoard);
                            // auto& clpGBT = cOpticalGroup->flpGBT;
                            // send a hard reset every time 
                            // if( !cWithLpGBT ) fBeBoardInterface->ChipReset(cBoard); 
                            // else if( cChip->getFrontEndType() == FrontEndType::CBC3 )  static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cHybrid->getId()%2 );
                            // skip read-only registers 
                            if(cChip->getFrontEndType() == FrontEndType::CBC3 && (cConfigItem.first.find("Bandgap") != std::string::npos || cConfigItem.first.find("ChipIDFuse") != std::string::npos)) continue;
                            // need to think of something smarter than this
                            if(cChip->getFrontEndType() == FrontEndType::CBC3 && (cConfigItem.first.find("FeCtrl&TrgLat2") != std::string::npos )) continue;
                
                            // so .. first thing I want to do is write the test value to this register 
                            // first make sure chips local map is  updated 
                            cChip->setReg(cConfigItem.first, cConfigItem.second.fValue, cConfigItem.second.fPrmptCfg, cConfigItem.second.fStatusReg);
                            // auto cReg = cChip->getRegItem(cConfigItem.first);
                            // LOG (INFO) << BOLDMAGENTA << "\t..Register " << cConfigItem.first << " writing 0x" << std::hex << +cConfigItem.second.fValue << std::dec 
                            //     << " value in memory map is 0x" << std::hex << +cReg.fValue << std::dec 
                            //     << RESET;
                            fReadoutChipInterface->WriteChipReg(cChip,cConfigItem.first, cConfigItem.second.fValue);
                            size_t cMatches=0;
                            for( auto cItem : cCurrentList ) // loop over what I think the current values are 
                            {
                                // skip checking a register against itself 
                                if(cItem.first  == cConfigItem.first ) continue; 

                                // compare the value read back from the chip against what is expected  
                                auto cReadBack = fReadoutChipInterface->ReadChipReg(cChip, cItem.first);
                                auto cValue    = cItem.second.fValue; 
                                if( std::find(cRegistersChecked.begin(), cRegistersChecked.end(), cItem.first) != cRegistersChecked.end() ){ 
                                   auto cName        = cItem.first;
                                   auto cIter        = std::find_if(cOriginalList.begin(), cOriginalList.end(), [&cName](Register x) { return x.first == cName; } );
                                   cValue = (*cIter).second.fValue; 
                                }

                                bool cCorrupted = cReadBack != cValue; 
                                if( cCorrupted )
                                { 
                                    std::string cRegName = cItem.first; 
                                    if( std::find_if(cSensitiveRegisters.begin(), cSensitiveRegisters.end(), [&cRegName](Register x) { return x.first == cRegName; }) != cSensitiveRegisters.end() )
                                        cSensitiveRegisters.push_back( cItem );
                                    //if( cSensitiveRegister.find(cItem.first) == cSensitiveRegister.end() ) cSensitiveRegister[cItem.first] = cItem.second; 
                                    LOG (INFO) << BOLDRED << "\t\t...Mismatch in I2C register " << cItem.first 
                                        << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec 
                                        << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec 
                                        << RESET;
                                }
                                else 
                                {
                                    // LOG (INFO) << BOLDGREEN << "\t\t...Match in I2C register " << cItem.first 
                                    //     << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec 
                                    //     << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec 
                                    //     << RESET;
                                    cMatches++;
                                }
                                    
                            }// loop over current values and compare what I read back against what I have stored in memory 
                            cRegistersChecked.push_back( cConfigItem.first );
                            float cMatchedPerc = cMatches/(float)(cCurrentList.size() - 1); 
                            LOG (INFO) << BOLDMAGENTA << " When writing register " << cConfigItem.first 
                                << " on page " << +cConfigItem.second.fPage
                                << " found read-back matched fraction from other registers to be "
                                << 100*cMatchedPerc << " percent."
                                << RESET; 
                        }//register write loop 
                    }//chip 
                }//hybrid
            }//OG
        }//board loop to run test 
    }
}
void RegisterTester::TestRegisters()
{
    // two bit patterns to rest registers with
    uint8_t cFirstBitPattern  = 0xAA;
    uint8_t cSecondBitPattern = 0x55;

    std::ofstream report;
    report.open(fDirectoryName + "/TestReport.txt", std::ofstream::out | std::ofstream::app);
    char line[240];

    for(auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    auto   cMap   = cChip->getRegMap();

                    for(const auto& cReg: cMap)
                    {
                        if(!fReadoutChipInterface->WriteChipReg(cChip, cReg.first, cFirstBitPattern, true))
                        {
                            sprintf(line, "# Writing 0x%.2x to CBC Register %s FAILED.\n", cFirstBitPattern, (cReg.first).c_str());
                            LOG(INFO) << BOLDRED << line << RESET;
                            report << line;
                            fBadRegisters[cChip->getId()].insert(cReg.first);
                            fNBadRegisters++;
                        }

                        // sleep for 100 ns between register writes
                        std::this_thread::sleep_for(std::chrono::nanoseconds(100));

                        if(!fReadoutChipInterface->WriteChipReg(cChip, cReg.first, cSecondBitPattern, true))
                        {
                            sprintf(line, "# Writing 0x%.2x to CBC Register %s FAILED.\n", cSecondBitPattern, (cReg.first).c_str());
                            LOG(INFO) << BOLDRED << line << RESET;
                            report << line;
                            fBadRegisters[cChip->getId()].insert(cReg.first);
                            fNBadRegisters++;
                        }

                        // sleep for 100 ns between register writes
                        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
                    }

                    fBeBoardInterface->ChipReSync(cBoard);
                }
            }
        }
    }

    report.close();
}

// Reload CBC registers from file found in directory.
// If no directory is given use the default files for the different operational modes found in Ph2_ACF/settings
void RegisterTester::ReconfigureRegisters(std::string pDirectoryName)
{
    for(auto cBoard: *fDetectorContainer)
    {
        fBeBoardInterface->ChipReset(cBoard);

        for(auto cOpticalGroup: *cBoard)
        {
            for(auto cFe: *cOpticalGroup)
            {
                for(auto cChip: *cFe)
                {
                    std::string  pRegFile;

                    if(pDirectoryName.empty())
                        pRegFile = "settings/CbcFiles/Cbc_default_electron.txt";
                    else
                    {
                        char buffer[120];
                        sprintf(buffer, "%s/FE%dCBC%d.txt", pDirectoryName.c_str(), cFe->getId(), cChip->getId());
                        pRegFile = buffer;
                    }

                    cChip->loadfRegMap(pRegFile);
                    fReadoutChipInterface->ConfigureChip(cChip);
                    LOG(INFO) << GREEN << "\t\t Successfully (re)configured ROC" << int(cChip->getId()) << "'s regsiters from " << pRegFile << " ." << RESET;
                }
            }
        }

        fBeBoardInterface->ChipReSync(cBoard);
    }
}
void RegisterTester::PrintTestReport()
{
    std::ofstream report(fDirectoryName + "/registers_test.txt"); // Creates a file in the current directory
    PrintTestResults(report);
    report.close();
}
void RegisterTester::PrintTestResults(std::ostream& os)
{
    os << "Testing Chip Registers one-by-one with complimentary bit-patterns (0xAA, 0x55)" << std::endl;

    for(const auto& cCbc: fBadRegisters)
    {
        os << "Malfunctioning Registers on Chip " << cCbc.first << " : " << std::endl;

        for(const auto& cReg: cCbc.second) os << cReg << std::endl;
    }

    LOG(INFO) << BOLDBLUE << "Channels diagnosis report written to: " + fDirectoryName + "/registers_test.txt" << RESET;
}

bool RegisterTester::PassedTest()
{
    bool passFlag = ((int)(fNBadRegisters) == 0) ? true : false;
    return passFlag;
}


//
void RegisterTester::writeObjects()
{
    this->SaveResults();
#ifdef __USE_ROOT__
    fResultFile->Flush();
#endif
}
// State machine control functions
void RegisterTester::Running() { Initialise(); }

void RegisterTester::Stop()
{
    this->SaveResults();
#ifdef __USE_ROOT__
    fResultFile->Flush();
#endif

    SaveResults();
#ifdef __USE_ROOT__
    CloseResultFile();
#endif
    Destroy();
}

void RegisterTester::Pause() {}

void RegisterTester::Resume() {}
