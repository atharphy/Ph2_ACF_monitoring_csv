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
    std::vector<std::string> cRegsToSkip{"Bandgap","ChipIDFuse","FeCtrl&TrgLat2"};
    size_t  cAttempts = 1;
    uint8_t cTestFlavor = 0; 
    // first just test page toggle 
    for(size_t cAttempt = 0; cAttempt < cAttempts; cAttempt++)
    {

        std::vector<uint8_t> cPages{1,0,1};
        LOG(INFO) << BOLDMAGENTA << "Test" << +cTestFlavor << " attempt#" << +cAttempt << " I2C registers .... just going to toggle the page without writing..." << RESET;
        uint8_t cSortOrder=0;       
        //uint8_t cFirstPage=(cSortOrder==0)? 0 : 1; 

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
                        Registers&        cRegList = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        const ChipRegMap& cOriginalMap = cChip->getRegMap();
                        for(auto cMapItem: cOriginalMap) { 
                            if( std::find( cRegsToSkip.begin(), cRegsToSkip.end(), cMapItem.first ) != cRegsToSkip.end() ) continue; 
                            LOG (DEBUG) << BOLDMAGENTA << "Will configure register " << cMapItem.first 
                                << " on page " << +cMapItem.second.fPage 
                                << " with address " << +cMapItem.second.fAddress 
                                << " with value 0x" << std::hex << +cMapItem.second.fValue  << std::dec
                                << RESET;
                            cRegList.push_back(std::make_pair(cMapItem.first, cMapItem.second)); 
                        }
                        // registers sorted by ... all on page 0 then all on page 1
                        if(cSortOrder == 0) std::sort(cRegList.begin(), cRegList.end(), customLessThanPage);    // all to page0 then page 1
                        if(cSortOrder == 1) std::sort(cRegList.begin(), cRegList.end(), customGreaterThanPage); // all to page1 then page 0
                    }
                }
            }
        } // board loop to save record of registers

        // so here .. I need to send a hard reset to the ROCs
        for(auto cBoard: *fDetectorContainer)
        {
            auto cWithLpGBT = fReadoutChipInterface->lpGBTCheck(cBoard);
            if(!cWithLpGBT)
            {
                fBeBoardInterface->ChipReset(cBoard);
                continue;
            }
            for(auto cOpticalGroup: *cBoard)
            {
                auto& clpGBT = cOpticalGroup->flpGBT;
                for(auto cHybrid: *cOpticalGroup)
                {
                    auto cType    = FrontEndType::CBC3;
                    bool cWithCBC = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cType         = FrontEndType::SSA;
                    bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
                    cType         = FrontEndType::MPA;
                    bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());

                    if(cWithCBC) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cHybrid->getId() % 2);
                    if(cWithSSA) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetSSA(clpGBT, cHybrid->getId() % 2);
                    if(cWithMPA) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, cHybrid->getId() % 2);
                }
            }
        } // board loop to send hard reset

        // reset page map 
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
                        static_cast<CbcInterface*>(fReadoutChipInterface)->resetPageMap(); 
                    } // chip
                } // hybrid
            } // OG
        } 

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
                        Registers& cOriginaList = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        Registers& cList        = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        // set page to that of the first register in the map
                        // if( cChip->getFrontEndType() == FrontEndType::CBC3 ) static_cast<CbcInterface*>(fReadoutChipInterface)->ConfigurePage( cChip, 1);//(*cOriginalMap.begin()).second.fPage );
                        for(auto cListItem: cOriginaList)
                        {
                            if( std::find( cRegsToSkip.begin(), cRegsToSkip.end(), cListItem.first ) != cRegsToSkip.end() ) continue; 
                            
                            auto cRegItem   = cListItem.second;
                            cRegItem.fValue = fReadoutChipInterface->ReadChipReg(cChip, cListItem.first);
                            cList.push_back(std::make_pair(cListItem.first, cRegItem));
                            LOG (DEBUG) << BOLDMAGENTA << "Default value after a hard reset of register " << cListItem.first << " is 0x"
                                << std::hex << +cRegItem.fValue << std::dec
                                << " value after configuration should be 0x" << std::hex << cListItem.second.fValue << std::dec
                                << RESET;
                        } // map
                    }     // chip
                }         // hybrid
            }             // OG
        }                 // board loop to save record of registers

        
        // now .. try and change page after config and look for mis-matches
        for(auto cBoard: *fDetectorContainer)
        {
            for(auto cOpticalGroup: *cBoard)
            {
                for(auto cHybrid: *cOpticalGroup)
                {
                    for(auto cChip: *cHybrid)
                    {
                        LOG (INFO) << BOLDMAGENTA << "Chip#" << +cChip->getId() << " on hybrid" << +cHybrid->getId() << RESET;
                        Registers& cExpectedLst = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
                        Registers  cSensitiveRegisters;
                        cSensitiveRegisters.clear();
                        std::vector<int> cPageToggles(0);
                        
                        uint8_t cPreviousPage = static_cast<CbcInterface*>(fReadoutChipInterface)->GetLastPage(cChip);
                        for(auto cPage : cPages )
                        {
                            LOG (INFO) << BOLDMAGENTA << "Going to select page " << +cPage
                                << " - previous page was " << +cPreviousPage << RESET;

                            static_cast<CbcInterface*>(fReadoutChipInterface)->ConfigurePage( cChip, cPage );
                            // check for mismatches
                            size_t cMatches = 0;
                            for(auto& cItem: cExpectedLst) // loop over what I think the current values are
                            {
                                // compare the value read back from the chip against what is expected
                                LOG (DEBUG) << BOLDMAGENTA << "\t... Reading back value from register " << cItem.first << " on page " << +cItem.second.fPage << " with value 0x" 
                                    << std::hex << +cItem.second.fValue << std::dec << RESET;
                                auto cReadBack = fReadoutChipInterface->ReadChipReg(cChip, cItem.first);
                                auto cValue    = cItem.second.fValue;

                                bool cCorrupted = cReadBack != cValue;
                                if(cCorrupted)
                                {
                                    std::string cRegName = cItem.first;
                                    if(std::find_if(cSensitiveRegisters.begin(), cSensitiveRegisters.end(), [&cRegName](Register x) { return x.first == cRegName; }) == cSensitiveRegisters.end())
                                    {
                                        cSensitiveRegisters.push_back(cItem);
                                        cPageToggles.push_back( cPreviousPage - cPage );
                                    }
                                    LOG (INFO) << BOLDRED << "\t\t\t..When switching from page " << +cPreviousPage
                                            << " to page " << +cPage
                                            << "\t\t...Mismatch in I2C register " << cItem.first << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec
                                            << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec << RESET;
                                    // if register value does not match
                                    // re-write 
                                    fReadoutChipInterface->WriteChipReg(cChip, cRegName, cItem.second.fValue);
                                }
                                else
                                {
                                    LOG (DEBUG) << BOLDGREEN << "\t\t...Match in I2C register " << cItem.first
                                        << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec
                                        << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec
                                        << RESET;
                                    cMatches++;
                                }

                            } // loop over current values and compare what I read back against what I have stored in memory
                            float cMatchedPerc = cMatches / (float)(cExpectedLst.size() );
                            if( cMatches == cExpectedLst.size() )
                                LOG(INFO) << BOLDGREEN << "Found read-back matched fraction from other registers to be " << 100 * cMatchedPerc << " percent. Found " 
                                  << +cSensitiveRegisters.size() << " sensitive registers." <<  RESET;
                            else
                                LOG(INFO) << BOLDRED << "Found read-back matched fraction from other registers to be " << 100 * cMatchedPerc << " percent. Found " 
                                  << +cSensitiveRegisters.size() << " sensitive registers." <<  RESET;
                            
                            cPreviousPage = cPage;
                        } // register write loop
                        std::sort(cSensitiveRegisters.begin(), cSensitiveRegisters.end(), customGreaterThanAddress);
                        for( auto cSensitiveRegister : cSensitiveRegisters ) 
                        {
                            LOG (INFO) << BOLDRED << "Sensitive register " << cSensitiveRegister.first << " on page " << +cSensitiveRegister.second.fPage << RESET;
                        }
                    }     // chip
                }         // hybrid
            }             // OG
        }                 // board loop to run test
        
    }
    cTestFlavor++;

    // // 0, increasing page order
    // // 1, decreasing page order
    // // 2, don't sort
    // size_t  cLimitPerPage = 2;
   // for(uint8_t cSortOrder = 0; cSortOrder < 2; cSortOrder++)
    // {
    //     for(size_t cAttempt = 0; cAttempt < cAttempts; cAttempt++)
    //     {
    //         LOG(INFO) << BOLDMAGENTA << "Test#" << +cAttempt << " I2C registers .... sort oder is " << +cSortOrder << RESET;
                            
    //         // first I want to record the register map for this map
    //         // retreive original settings for all chips and all back-end boards
    //         DetectorDataContainer cRegListContainer;
    //         ContainerFactory::copyAndInitChip<Registers>(*fDetectorContainer, cRegListContainer);
    //         for(auto cBoard: *fDetectorContainer)
    //         {
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         Registers&        cRegList = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
    //                         const ChipRegMap& cOriginalMap = cChip->getRegMap();
    //                         for(auto cMapItem: cOriginalMap) { 
    //                             if( std::find( cRegsToSkip.begin(), cRegsToSkip.end(), cMapItem.first ) != cRegsToSkip.end() ) continue; 
    //                             LOG (DEBUG) << BOLDMAGENTA << "Will configure register " << cMapItem.first 
    //                                 << " on page " << +cMapItem.second.fPage 
    //                                 << " with address " << +cMapItem.second.fAddress 
    //                                 << " with value 0x" << std::hex << +cMapItem.second.fValue  << std::dec
    //                                 << RESET;
    //                             cRegList.push_back(std::make_pair(cMapItem.first, cMapItem.second)); 
    //                         }
    //                         // registers sorted by ... all on page 0 then all on page 1
    //                         if(cSortOrder == 0) std::sort(cRegList.begin(), cRegList.end(), customLessThanPage);    // all to page0 then page 1
    //                         if(cSortOrder == 1) std::sort(cRegList.begin(), cRegList.end(), customGreaterThanPage); // all to page1 then page 0
    //                     }
    //                 }
    //             }
    //         } // board loop to save record of registers

    //         // so here .. I need to send a hard reset to the ROCs
    //         for(auto cBoard: *fDetectorContainer)
    //         {
    //             auto cWithLpGBT = fReadoutChipInterface->lpGBTCheck(cBoard);
    //             if(!cWithLpGBT)
    //             {
    //                 fBeBoardInterface->ChipReset(cBoard);
    //                 continue;
    //             }
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 auto& clpGBT = cOpticalGroup->flpGBT;
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     auto cType    = FrontEndType::CBC3;
    //                     bool cWithCBC = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
    //                     cType         = FrontEndType::SSA;
    //                     bool cWithSSA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());
    //                     cType         = FrontEndType::MPA;
    //                     bool cWithMPA = (std::find_if(cHybrid->begin(), cHybrid->end(), [&cType](Ph2_HwDescription::Chip* x) { return x->getFrontEndType() == cType; }) != cHybrid->end());

    //                     if(cWithCBC) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetCBC(clpGBT, cHybrid->getId() % 2);
    //                     if(cWithSSA) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetSSA(clpGBT, cHybrid->getId() % 2);
    //                     if(cWithMPA) static_cast<D19clpGBTInterface*>(flpGBTInterface)->resetMPA(clpGBT, cHybrid->getId() % 2);
    //                 }
    //             }
    //         } // board loop to send hard reset

    //         // reset page map 
    //         for(auto cBoard: *fDetectorContainer)
    //         {
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         if( cChip->getFrontEndType() != FrontEndType::CBC3 ) continue;
    //                         static_cast<CbcInterface*>(fReadoutChipInterface)->resetPageMap(); 
    //                     } // chip
    //                 } // hybrid
    //             } // OG
    //         } 

    //         // container to store default register values after a hard reset
    //         DetectorDataContainer cDefRegListContainer;
    //         ContainerFactory::copyAndInitChip<Registers>(*fDetectorContainer, cDefRegListContainer);
    //         for(auto cBoard: *fDetectorContainer)
    //         {
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         Registers& cOriginaList = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
    //                         Registers& cList        = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
    //                         // set page to that of the first register in the map
    //                         // if( cChip->getFrontEndType() == FrontEndType::CBC3 ) static_cast<CbcInterface*>(fReadoutChipInterface)->ConfigurePage( cChip, 1);//(*cOriginalMap.begin()).second.fPage );
    //                         for(auto cListItem: cOriginaList)
    //                         {
    //                             if( std::find( cRegsToSkip.begin(), cRegsToSkip.end(), cListItem.first ) != cRegsToSkip.end() ) continue; 
                                
    //                             auto cRegItem   = cListItem.second;
    //                             cRegItem.fValue = fReadoutChipInterface->ReadChipReg(cChip, cListItem.first);
    //                             cList.push_back(std::make_pair(cListItem.first, cRegItem));
    //                             LOG (DEBUG) << BOLDMAGENTA << "Default value after a hard reset of register " << cListItem.first << " is 0x"
    //                                 << std::hex << +cRegItem.fValue << std::dec
    //                                 << " value after configuration should be 0x" << std::hex << cListItem.second.fValue << std::dec
    //                                 << RESET;
    //                         } // map
    //                     }     // chip
    //                 }         // hybrid
    //             }             // OG
    //         }                 // board loop to save record of registers

    //         //continue;

    //         // now .. try and re-write original values after config and look for mis-matches
    //         for(auto cBoard: *fDetectorContainer)
    //         {
    //             for(auto cOpticalGroup: *cBoard)
    //             {
    //                 for(auto cHybrid: *cOpticalGroup)
    //                 {
    //                     for(auto cChip: *cHybrid)
    //                     {
    //                         LOG (INFO) << BOLDMAGENTA << "Chip#" << +cChip->getId() << RESET;
    //                         Registers& cListToCnfig = cRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
    //                         Registers  cCnfgList;
    //                         size_t     cNPage0Regs, cNPage1Regs = 0;
    //                         cNPage0Regs = cNPage1Regs;
    //                         for(auto cItem: cListToCnfig)
    //                         {
    //                             if( std::find( cRegsToSkip.begin(), cRegsToSkip.end(), cItem.first ) != cRegsToSkip.end() ) continue; 
                                
    //                             bool cPushBack = (cItem.second.fPage == 0 && cNPage0Regs < cLimitPerPage) || (cItem.second.fPage == 1 && cNPage1Regs < cLimitPerPage);
    //                             if(!cPushBack) continue;

    //                             cCnfgList.push_back(cItem);
    //                             if(cItem.second.fPage == 0) cNPage0Regs++;
    //                             if(cItem.second.fPage == 1) cNPage1Regs++;
    //                         }
                            
    //                         LOG (DEBUG) << BOLDMAGENTA << "Going to test using " << +cNPage0Regs << " register(s) on Page0 and " << +cNPage1Regs << " register(s) on Page1 " << RESET;
    //                         Registers& cExpectedLst = cDefRegListContainer.at(cBoard->getIndex())->at(cOpticalGroup->getIndex())->at(cHybrid->getIndex())->at(cChip->getIndex())->getSummary<Registers>();
    //                         Registers  cSensitiveRegisters;
    //                         cSensitiveRegisters.clear();
    //                         std::vector<int> cPageToggles(0);
    //                         std::vector<std::string> cRegistersChecked;
    //                         cRegistersChecked.clear();
                            
    //                         uint8_t cPage = static_cast<CbcInterface*>(fReadoutChipInterface)->GetLastPage(cChip);
    //                         uint8_t cPreviousPage; 
    //                         for(auto cConfigItem: cCnfgList)
    //                         {
    //                             cPreviousPage = cPage;
    //                             cPage = cConfigItem.second.fPage;
    //                             LOG (DEBUG) << BOLDMAGENTA << "Going to configure register " << cConfigItem.first 
    //                                 << " on page " << +cConfigItem.second.fPage 
    //                                 << " with address " << +cConfigItem.second.fAddress 
    //                                 << " with value 0x" << std::hex << +cConfigItem.second.fValue  << std::dec
    //                                 << " page toggle from Page " << +(cPreviousPage) << " to page " << +cPage
    //                                 << RESET;

    //                             // so .. first thing I want to do is write the test value to this register
    //                             // first make sure chips local map is  updated
    //                             cChip->setReg(cConfigItem.first, cConfigItem.second.fValue, cConfigItem.second.fPrmptCfg, cConfigItem.second.fStatusReg);
    //                             // write register 
    //                             fReadoutChipInterface->WriteChipReg(cChip, cConfigItem.first, cConfigItem.second.fValue);
    //                             // check for mismatches
    //                             size_t cMatches = 0;
    //                             for(auto& cItem: cExpectedLst) // loop over what I think the current values are
    //                             {
    //                                 // skip checking a register against itself
    //                                 // but update expected value in current list 
    //                                 if(cItem.first == cConfigItem.first){ 
    //                                     cItem.second = cConfigItem.second;
    //                                     continue;
    //                                 }

    //                                 // compare the value read back from the chip against what is expected
    //                                 LOG (DEBUG) << BOLDMAGENTA << "\t... Reading back value from register " << cItem.first << " on page " << +cItem.second.fPage << " with value 0x" 
    //                                     << std::hex << +cItem.second.fValue << std::dec << RESET;
    //                                 auto cReadBack = fReadoutChipInterface->ReadChipReg(cChip, cItem.first);
    //                                 auto cValue    = cItem.second.fValue;

    //                                 bool cCorrupted = cReadBack != cValue;
    //                                 if(cCorrupted)
    //                                 {
    //                                     std::string cRegName = cItem.first;
    //                                     if(std::find_if(cSensitiveRegisters.begin(), cSensitiveRegisters.end(), [&cRegName](Register x) { return x.first == cRegName; }) == cSensitiveRegisters.end())
    //                                     {
    //                                         cSensitiveRegisters.push_back(cItem);
    //                                         cPageToggles.push_back( cPreviousPage - cPage );
    //                                     }
    //                                     LOG (INFO) << BOLDRED << "When configuring register " << cConfigItem.first 
    //                                             << " on page " << +cConfigItem.second.fPage 
    //                                             << " with address " << +cConfigItem.second.fAddress 
    //                                             << " with value 0x" << std::hex << +cConfigItem.second.fValue  << std::dec
    //                                             << " page toggle from Page " << +(cPreviousPage) << " to page " << +cPage
    //                                             << "\t\t...Mismatch in I2C register " << cItem.first << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec
    //                                             << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec << RESET;
    //                                     // if register value does not match
    //                                     // re-write 
    //                                     fReadoutChipInterface->WriteChipReg(cChip, cRegName, cItem.second.fValue);
    //                                 }
    //                                 else
    //                                 {
    //                                     LOG (DEBUG) << BOLDGREEN << "\t\t...Match in I2C register " << cItem.first
    //                                         << " value stored in map is 0x" << std::hex << +cItem.second.fValue << std::dec
    //                                         << " value read-back from chip is 0x" << std::hex << +cReadBack << std::dec
    //                                         << RESET;
    //                                     cMatches++;
    //                                 }

    //                             } // loop over current values and compare what I read back against what I have stored in memory
    //                             cRegistersChecked.push_back(cConfigItem.first);
    //                             float cMatchedPerc = cMatches / (float)(cExpectedLst.size() - 1);
    //                             if( cMatches != (cExpectedLst.size()-1) )
    //                                 LOG(INFO) << BOLDRED << " When writing register " << cConfigItem.first << " on page " << +cConfigItem.second.fPage
    //                                   << " found read-back matched fraction from other registers to be " << 100 * cMatchedPerc << " percent. Found " 
    //                                   << +cSensitiveRegisters.size() << " sensitive registers."
    //                                   << RESET;
    //                             else
    //                                 LOG(DEBUG) << BOLDGREEN << " When writing register " << cConfigItem.first << " on page " << +cConfigItem.second.fPage
    //                                   << " found read-back matched fraction from other registers to be " << 100 * cMatchedPerc << " percent. Found " 
    //                                   << +cSensitiveRegisters.size() << " sensitive registers."
    //                                   << RESET;
    //                         } // register write loop
    //                         std::sort(cSensitiveRegisters.begin(), cSensitiveRegisters.end(), customGreaterThanAddress);
    //                         for( auto cSensitiveRegister : cSensitiveRegisters ) 
    //                         {
    //                             LOG (INFO) << BOLDRED << "Sensitive register " << cSensitiveRegister.first << " on page " << +cSensitiveRegister.second.fPage << RESET;
    //                         }
    //                     }     // chip
    //                 }         // hybrid
    //             }             // OG
    //         }                 // board loop to run test
    //     }
    // } 
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
                    auto cMap = cChip->getRegMap();

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
                    std::string pRegFile;

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
