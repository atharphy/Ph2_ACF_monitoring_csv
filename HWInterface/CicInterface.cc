/*

        FileName :                     CicInterface.cc
        Content :                      User Interface to the Cics
        Version :                      1.0
        Date of creation :             10/07/14

 */

#include "CicInterface.h"
#include "BeBoardFWInterface.h"
#include "D19cFWInterface.h"
#include "D19clpGBTInterface.h"
#include "ReadoutChipInterface.h"

#define DEV_FLAG 0
// #define COUNT_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
CicInterface::CicInterface(const BeBoardFWMap& pBoardMap) : ChipInterface(pBoardMap)
{
    fPhaseValues.clear();
    fFeStates.clear(); // 8 FEs
    for(size_t cIndex = 0; cIndex < 8; cIndex++) fFeStates.push_back(0);
    for(size_t cIndex = 0; cIndex < 8; cIndex++) fPhaseValues.push_back(0);

    fRegisterWrites=0;
    fPortStates.clear(); // 12 ports
    fWriteErrorMap.clear(); 
    fReadBackErrorMap.clear();
    fMap.clear();

    for(size_t cIndex = 0; cIndex < 12; cIndex++) fPortStates.push_back(0);

    for(size_t cInput = 0; cInput < 4; cInput++)
    {
        std::vector<uint8_t> cPhyPorts(12, 0);
        fPhaseTaps.push_back(cPhyPorts);
    }
}

CicInterface::~CicInterface() {}

//#FIXME temporary fix to use 1/2 PS skeleton
void CicInterface::LinkLpGBT(D19clpGBTInterface* pLpGBTInterface, lpGBT* pLpGBT)
{
    flpGBTInterface = pLpGBTInterface;
    flpGBT          = pLpGBT;
}
bool CicInterface::runVerification(Ph2_HwDescription::Chip* pChip, uint8_t pValue, std::string pRegName)
{
    auto cRegItem = pChip->getRegItem( pRegName );
    uint32_t cValue = pValue; 
    // only check against map if this is 
    // a register than *can* be read back
    // from 
    bool cSuccess = ( ( cRegItem.fStatusReg == 0x01 ) ? true : (cRegItem.fValue == cValue )) ;
    if( cSuccess  )
        LOG(DEBUG) << BOLDGREEN << "\t...[DEBUG] Have written 0x" << std::hex << +cRegItem.fValue << std::dec 
            << " CIC register with address 0x" << std::hex << +cRegItem.fAddress << std::dec 
            << " value read back is 0x"  << std::hex << +cValue << std::dec 
            << RESET;
    else if( !cSuccess )
        LOG(DEBUG) << BOLDRED << "\t...[DEBUG] Have written 0x" << std::hex << +cRegItem.fValue << std::dec 
            << " CIC register with address 0x" << std::hex << +cRegItem.fAddress << std::dec 
            << " value read back is 0x"  << std::hex << +cValue << std::dec 
            << RESET;
    return cSuccess;
} 

std::pair<int,float>   CicInterface::getWRattempts()
{
    float cReWR=0; 
    float cN=0; 
    for(auto cIter : fReWrMap)
    {  
        if(cIter.second !=0 ){ cN++; cReWR+= cIter.second;}
    } 
    float cMean =  (cN==0)? 0 : cReWR/cN;
    return std::make_pair(cN, cMean);
}
std::pair<float,float> CicInterface::getMinMaxWRattempts()
{
    float cReWRmin=0; 
    float cReWRmax=0; 
    for(auto cIter : fReWrMap)
    { 
        if(cIter.second !=0 && cReWRmin == 0 ) cReWRmin = cIter.second; 
        else 
        {
            if( cIter.second < cReWRmin) cReWRmin = cIter.second;
        }
        if(cIter.second > cReWRmax ) cReWRmax = cIter.second; 
    }
    return std::make_pair(cReWRmin, cReWRmax);
}
void CicInterface::printErrorSummary()
{
    LOG (INFO) << BOLDRED << "CIC total write error count : " << +fWriteErrors 
        << " out of a total of " << +fRegisterWrites << " writes"
        << RESET;

    LOG (INFO) << BOLDRED << "CIC total read-back error count : " << +fReadBackErrors 
        << " out of a total of " << +fRegisterWrites << " writes"
        << RESET;
        
}
void CicInterface::CheckConfig( Chip* pChip )
{
    LOG (DEBUG) << BOLDMAGENTA << "Running verification loop for CicInterface::WriteRegs" << RESET;
    fReadBackErrors=0;
    for(const auto& cMapItem: fMap)
    {
        auto cRegItem = pChip->getRegItem( cMapItem.second );
        bool cRetry = true;
        uint32_t cValue = fBoardFW->ReadFERegister(pChip, cRegItem.fAddress, cRetry);
        bool cSuccess = this->runVerification(pChip, cValue, cMapItem.second );
        if( !cSuccess )
        {
            LOG (INFO) << BOLDRED << "Readback error for CIC register 0x"
                << std::hex << +cRegItem.fAddress << std::dec 
                << " have written " << +cRegItem.fValue
                << " and have read back " << cValue << RESET;
            
            auto cIter = fReadBackErrorMap.find(cRegItem.fAddress);
            if( cIter == fReadBackErrorMap.end() ) fReadBackErrorMap[cRegItem.fAddress]=1;
            else fReadBackErrorMap[cRegItem.fAddress]=fReadBackErrorMap[cRegItem.fAddress]+1;
            fReadBackErrors++;
        }
    }
}
bool CicInterface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint8_t, uint8_t>> pRegs, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    bool cSuccess = true;
    bool cRetry=true;
    if(flpGBTInterface == nullptr)
    {
        std::vector<uint32_t> cVec;
        cVec.clear();
        for(const auto& cReg: pRegs)
        {
            ChipRegItem cRegItem;
            cRegItem.fPage    = 0x00;
            cRegItem.fAddress = cReg.first;
            cRegItem.fValue   = cReg.second & 0xFF;
            //fBoardFW->EncodeReg(cRegItem, pCic->getFeId(), pCic->getChipId(), cVec, pVerifLoop, true);
            //fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId(), cVec, pVerifLoop, true);
            fBoardFW->EncodeReg(cRegItem, pChip, cVec, pVerifLoop, true);
            #ifdef COUNT_FLAG
                        fRegisterCount++;
            #endif
        }
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
#ifdef COUNT_FLAG
        fTransactionCount++;
#endif
    }
    else
    {
        cSuccess = true;
        LOG (DEBUG) << BOLDMAGENTA << "Writing registers CicInterface::WriteRegs" << RESET;
        size_t cCount=0;
        std::vector<uint8_t> pSuccesses(pRegs.size(), 1);
        size_t cWritesCounter = fRegisterWrites; 
        size_t cWriteErrCounter = fWriteErrors;  
        size_t cReadBackCounter = fReadBackErrors; 
        
        for(const auto& cReg: pRegs)
        {
        
            auto cRegItem = pChip->getRegItem( fMap[cReg.first] );
            cRegItem.fPage    = 0x00;
            cRegItem.fAddress = cReg.first;
            cRegItem.fValue   = cReg.second & 0xFF;
            // update register map
            pChip->setReg( fMap[cReg.first] , cRegItem.fValue, cRegItem.fPrmptCfg , cRegItem.fStatusReg);
            //cSuccess = flpGBTInterface->cicWrite(flpGBT, pChip->getHybridId(), cReg.first, cReg.second, cRetry);
            cRetry=true; 
            bool cVerify=false;
            cSuccess = fBoardFW->WriteFERegister(pChip, cReg.first, cReg.second, cRetry, cVerify);
            auto cStatus = (static_cast<D19cFWInterface*>(fBoardFW))->getI2Cstatus(); 
            if( !cSuccess && fRetryI2C )
            {
                // keep trying 
                uint8_t cWriteAttempt=0; 
                LOG (DEBUG) << BOLDRED << "Write error for CIC register 0x" 
                        << std::hex << +cReg.first << std::dec  
                        << " I2C status is " << +cStatus
                        << RESET;  
                do
                {
                    auto cIter = fReWMap.find(cReg.first);
                    if( cIter == fReWMap.end() ) fReWMap[cReg.first]=1;
                    else fReWMap[cReg.first]=fReWMap[cReg.first]+1;
                    fReW++;

                    LOG (DEBUG) << BOLDRED << "\t.. attempt#" 
                        << +cWriteAttempt
                        << RESET;
                    //cSuccess = flpGBTInterface->cicWrite(flpGBT, pChip->getHybridId(), cReg.first, cReg.second, cRetry);
                    cSuccess = fBoardFW->WriteFERegister(pChip, cReg.first, cReg.second, cRetry);
                    cWriteAttempt++;
                }while(!cSuccess && cWriteAttempt < fMaxI2CAttempts);
            }
            if( !cSuccess )
            {
                LOG (INFO) << BOLDRED << "Write error for CIC register 0x"
                    << std::hex << +cReg.first << std::dec 
                    << " I2C status is " << std::bitset<8>(cStatus)
                    << RESET; 
                fI2CStatus.push_back(cStatus);
                auto cIter = fWriteErrorMap.find(cReg.first);
                if( cIter == fWriteErrorMap.end() ) fWriteErrorMap[cReg.first]=1;
                else fWriteErrorMap[cReg.first]=fWriteErrorMap[cReg.first]+1;
                fWriteErrors++;
            }
            fRegisterWrites++;
            pSuccesses[cCount] = (cSuccess) ? 1 : 0 ;     
            cCount++;
#ifdef COUNT_FLAG
            fRegisterCount++;
#endif
        }

        if ( pVerifLoop )
        {
            cCount=0;
            LOG (DEBUG) << BOLDMAGENTA << "Running verification loop for CicInterface::WriteRegs" << RESET;
            for(const auto& cReg: pRegs)
            {
                //uint32_t cValue = flpGBTInterface->cicRead(flpGBT, pChip->getHybridId(), cReg.first);
                uint32_t cValue = fBoardFW->ReadFERegister(pChip, cReg.first);
                cSuccess = ( pSuccesses[cCount]  == 1 ) ? this->runVerification(pChip, cValue, fMap[cReg.first]) : true;
                if( !cSuccess && fRetryI2C )
                {
                    // keep trying 
                    uint8_t cWriteAttempt=0; 
                    LOG (DEBUG) << BOLDRED << "Readback error for CIC register 0x" 
                        << std::hex << +cReg.first << std::dec  
                        << RESET;
                    do
                    {
                        LOG (DEBUG) << BOLDRED << "\t.. attempt#" 
                            << +cWriteAttempt
                            << RESET;
                        //cSuccess = flpGBTInterface->cicWrite(flpGBT, pChip->getHybridId(), cReg.first, cReg.second, cRetry);
                        cSuccess = fBoardFW->WriteFERegister(pChip, cReg.first, cReg.second, cRetry);
                        if( cSuccess )
                        {
                            uint32_t cValue = fBoardFW->ReadFERegister(pChip, cReg.first);
                            //uint32_t cValue = flpGBTInterface->cicRead(flpGBT, pChip->getHybridId(), cReg.first);
                            cSuccess = ( pSuccesses[cCount]  == 1 ) ? this->runVerification(pChip, cValue, fMap[cReg.first]) : true;
                            if( !cSuccess )
                            {
                                auto cIter = fReWrMap.find(cReg.first);
                                if( cIter == fReWrMap.end() ) fReWrMap[cReg.first]=1;
                                else fReWrMap[cReg.first]=fReWrMap[cReg.first]+1;
                                fReWR++;
                            }//update WR map 
                        }
                        else
                        {
                            auto cIter = fReWMap.find(cReg.first);
                            if( cIter == fReWMap.end() ) fReWMap[cReg.first]=1;
                            else fReWMap[cReg.first]=fReWMap[cReg.first]+1;
                            fReW++;
                        }// update W map
                        cWriteAttempt++;
                    }while(!cSuccess && cWriteAttempt < fMaxI2CAttempts);
                }
                // only log if the write failed 
                if( !cSuccess )
                {
                    auto cRegItem = pChip->getRegItem( fMap[cReg.first]  );
                    LOG (INFO) << BOLDRED << "Readback error for CIC register 0x"
                        << std::hex << +cReg.first << std::dec 
                        << " have written " << +cRegItem.fValue
                        << " and have read back " << cValue << RESET;
                    
                    auto cIter = fReadBackErrorMap.find(cReg.first);
                    if( cIter == fReadBackErrorMap.end() ) fReadBackErrorMap[cReg.first]=1;
                    else fReadBackErrorMap[cReg.first]=fReadBackErrorMap[cReg.first]+1;
                    fReadBackErrors++;
                }
                pSuccesses[cCount] = ( pSuccesses[cCount] == 1 && cSuccess ) ? 1 : 0 ;  
                cCount++;
            }
        }

        // check sum 
        auto cSum = std::accumulate(pSuccesses.begin(), pSuccesses.end(), 0.0);
        cSuccess = (cSum == pRegs.size() );
        if( cSuccess )
            LOG (INFO) << BOLDGREEN << "Register write successfull for CIC#" << +pChip->getId() << RESET;
        else 
            LOG (INFO) << BOLDRED << "Register write faile for CIC#" << +pChip->getId() << RESET;
        fAttemptedWrites = fRegisterWrites-cWritesCounter; 
        fSuccRegisterWrites = fAttemptedWrites - (fWriteErrors - cWriteErrCounter); 
        fSuccRegisterRbs  = fSuccRegisterWrites - (fReadBackErrors - cReadBackCounter);
        LOG (DEBUG) << BOLDRED << "Register write failed for CIC#" << +pChip->getId() 
            << +fSuccRegisterWrites << " in " << fAttemptedWrites << " attempted and "
            << " of those " << +fSuccRegisterRbs << " were read back correctly"
            //<< " found " << +(fWriteErrors-cWriteErrCounter) << " write errors in " << cAttemptedWrites << " attempts "
            //<< " and " << +(fReadBackErrors-cReadBackCounter) << " read-back errors found in those " << (fRegisterWrites-cWritesCounter-fWriteErrors+cWriteErrCounter) << " successfull writes"
            << RESET;
    }
    return cSuccess;
}


bool CicInterface::ConfigureChip(Chip* pCic, bool pVerifLoop, uint32_t pBlockSize)
{
    setBoard(pCic->getBeBoardId());
    std::vector<uint32_t> cVec;
    
    ChipRegMap            cCicRegMap = pCic->getRegMap();
    // get register map
    LOG (INFO) << BOLDMAGENTA << "Setting up CIC maps.." << RESET;
    fMap.clear();
    for(auto& cRegItem: cCicRegMap) { 
        fMap[cRegItem.second.fAddress] = cRegItem.first; 
    }
    //
    std::vector<std::pair<uint8_t, uint8_t>> cRegs;
    cRegs.clear();
    for(auto& cMapItem: fMap)
    {
        ChipRegItem& cItem = cCicRegMap[cMapItem.second]; 
        // create a register 
        std::pair<uint8_t, uint8_t> cReg;
        cReg.first  = cMapItem.first; 
        cReg.second = cItem.fValue; 
        //cReg.second = cCicRegMap[cMapItem.second].fValue;
        //cReg.second  = cRegItem.second.fValue;
        cRegs.push_back(cReg);
        // LOG(INFO) << BOLDBLUE << "Register map for CIC contains a register with address " << std::hex << +cReg.first << std::dec 
        //     << " register value " << std::hex << +cReg.second << std::dec 
        //     << RESET;
    }
    LOG (INFO) << BOLDMAGENTA << "Configuring CIC" << RESET;
    return this->WriteRegs(pCic, cRegs, pVerifLoop);
}

bool CicInterface::WriteReg(Chip* pChip, uint8_t pRegisterAddress, uint8_t pRegisterValue, bool pVerifLoop)
{
    // LOG (INFO) << BOLDMAGENTA << "CicInterface::WriteReg trying to write to register 0x" 
    //     << std::hex << +pRegisterAddress << std::dec 
    //     << RESET;

    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    auto cRegItem = pChip->getRegItem( fMap[pRegisterAddress] );
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = pRegisterValue & 0xFF;
    // update register map
    pChip->setReg( fMap[pRegisterAddress] , cRegItem.fValue, cRegItem.fPrmptCfg , cRegItem.fStatusReg);
    // write
    if(flpGBTInterface == nullptr)
    {
        std::vector<uint32_t> cVec;
        // LOG (INFO) << BOLDMAGENTA << "CicInterface::WriteReg(address) Register 0x" 
        //     << std::hex << +cRegItem.fAddress << std::dec << RESET;
        //fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId(), cVec, pVerifLoop, true);
        fBoardFW->EncodeReg(cRegItem, pChip, cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
        // write register 
        bool cRetry=true;
        // LOG (INFO) << BOLDMAGENTA << "Writing registers CicInterface::WriteReg via lpGBT" << RESET;
        //cSuccess = flpGBTInterface->cicWrite(flpGBT, pChip->getHybridId(), pRegisterAddress, pRegisterValue, cRetry);
        cSuccess = fBoardFW->WriteFERegister(pChip, pRegisterAddress, pRegisterValue, cRetry);
            
        fRegisterWrites++;
        // check write 
        if( !cSuccess )
        {
            auto cIter = fWriteErrorMap.find(pRegisterAddress);
            if( cIter == fWriteErrorMap.end() ) fWriteErrorMap[pRegisterAddress]=1;
            else fWriteErrorMap[pRegisterAddress]=fWriteErrorMap[pRegisterAddress]+1;
        }
        
        // check readback
        if ( pVerifLoop && cSuccess )
        {
            LOG (DEBUG) << BOLDMAGENTA << "Running verification loop for CicInterface::WriteReg" << RESET;
            //uint32_t cValue = flpGBTInterface->cicRead(flpGBT, pChip->getHybridId(), pRegisterAddress);
            // try this N times 
            size_t cReadAttempt=0; 
            do
            {
                uint32_t cValue = fBoardFW->ReadFERegister(pChip, pRegisterAddress);
                cSuccess = this->runVerification(pChip, cValue, fMap[pRegisterAddress]);
                cReadAttempt++;
            }while(!cSuccess && cReadAttempt < 10 );

            if( !cSuccess )
            {
                LOG (INFO) << BOLDRED << "Failed to read-back correct value from CIC register 0x"
                    << std::hex << +pRegisterAddress << std::dec 
                    << RESET;
                auto cIter = fReadBackErrorMap.find(pRegisterAddress);
                if( cIter == fReadBackErrorMap.end() ) fReadBackErrorMap[pRegisterAddress]=1;
                else fReadBackErrorMap[pRegisterAddress]=fReadBackErrorMap[pRegisterAddress]+1;
            }
        }
         
    }
    return cSuccess;
}
bool CicInterface::WriteChipReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    if( pRegNode.empty() ) { LOG (INFO) << BOLDRED << "CicInterface::WriteChipReg trying to write to empty register .." << RESET; return false; }
    //LOG (INFO) << BOLDMAGENTA << "CicInterface::WriteChipReg trying to write to register " << pRegNode << RESET;

    std::vector<uint32_t> cVec;
    ChipRegItem           cRegItem = pChip->getRegItem(pRegNode);
    cRegItem.fValue                = pValue;
    bool cSuccess                  = this->WriteReg(pChip, cRegItem.fAddress, cRegItem.fValue, pVerifLoop);
    return cSuccess;
}

bool     CicInterface::WriteChipMultReg(Chip* pChip, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop) { return true; }
uint16_t CicInterface::ReadChipReg(Chip* pChip, const std::string& pRegNode)
{
    setBoard(pChip->getBeBoardId());
    // LOG (INFO) << BOLDMAGENTA << "CicInterface::ReadChipReg(string) Register "
    //     << pRegNode << RESET;

    ChipRegMap            cRegMap = pChip->getRegMap();
    if( cRegMap.find(pRegNode) == cRegMap.end() ){ LOG (INFO) << BOLDRED << "Could not find CIC register " << pRegNode << RESET;}
    
    ChipRegItem cRegItem = pChip->getRegItem(pRegNode);
    return this->ReadChipRegItem(pChip, cRegItem).second;
}
std::pair<bool, uint16_t> CicInterface::ReadChipRegItem(Chip* pChip, ChipRegItem pRegItem)
{
    setBoard(pChip->getBeBoardId());
    if(flpGBTInterface == nullptr)
    {
        // LOG (INFO) << BOLDMAGENTA << "CicInterface::ReadChipReg(ChipRegItem) Register 0x" 
        //     << std::hex << +pRegItem.fAddress << std::dec << RESET;
        std::vector<uint32_t> cVecReq;
        fBoardFW->EncodeReg(pRegItem, pChip, cVecReq, true, false);
        //fBoardFW->EncodeReg(pRegItem, pChip->getHybridId(), pChip->getId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        // bools to find the values of failed and read
        bool    cFailed = false;
        bool    cRead;
        uint8_t cChipId;
        fBoardFW->DecodeReg(pRegItem, cChipId, cVecReq[0], cRead, cFailed);
        return std::make_pair(!cFailed, pRegItem.fValue);
    }
    else
    {
        // LOG (INFO) << BOLDMAGENTA << "CicInterface::ReadChipReg(ChipRegItem) via lpGBT Register 0x" 
        //     << std::hex << +pRegItem.fAddress << std::dec << RESET;
           
        //auto cValue = flpGBTInterface->cicRead(flpGBT, pChip->getHybridId(), pRegItem.fAddress);
        uint32_t cValue = fBoardFW->ReadFERegister(pChip, pRegItem.fAddress);
                
        return std::make_pair(true, cValue);
    }
}
bool CicInterface::CheckReSync(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAD : 0xA6;

    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC requires a ReSync." << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = cRegAddress;
    cRegItem.fStatusReg                   = 0x01;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);

    LOG(DEBUG) << BOLDBLUE << "Read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    if(!cReadBack.first) return false;

    bool cResyncNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x8) >> 3);
    if(!cResyncNeeded)
    {
        LOG(INFO) << BOLDBLUE << "....... No ReSync needed" << RESET;
        return true;
    }

    LOG(INFO) << BOLDBLUE << "....... ReSync needed - sending one now ...... " << RESET;
    // if readback worked and the CIC says it needs a Resync then send
    // a resync
    fBoardFW->ChipReSync();
    // check if CIC still needs one
    cReadBack = this->ReadChipRegItem(pChip, cRegItem);
    LOG(DEBUG) << BOLDBLUE << "After ReSync... read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    cResyncNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x8) >> 3);
    if(!cReadBack.first || cResyncNeeded)
    {
        LOG(INFO) << BOLDRED << "..............FAILED" << BOLDBLUE << " cleared RESYNC request" << RESET;
        return false;
    }
    else
    {
        LOG(INFO) << BOLDGREEN << "..............SUCCESSFULLY" << BOLDBLUE << " cleared  RESYNC request" << RESET;
        return true;
    }
}
bool CicInterface::CheckFastCommandLock(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAE : 0xA6;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC fast command decoder locked" << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = cRegAddress;
    cRegItem.fStatusReg                   = 0x01;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
    if(!cReadBack.first) return false;
    LOG(DEBUG) << BOLDBLUE << "Read back value of " << std::bitset<5>(cReadBack.second) << " from RO status register" << RESET;
    bool cLocked = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x10) >> 4);
    if(cLocked)
        return this->CheckReSync(pChip);
    else
        return cLocked;
}
// configure alignment patterns on CIC
bool CicInterface::ConfigureAlignmentPatterns(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns)
{
    bool cSuccess = true;
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    for(uint8_t cIndex = 0; cIndex < (uint8_t)pAlignmentPatterns.size(); cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "CALIB_PATTERN%d", cIndex);
        std::string cRegName(cBuffer, sizeof(cBuffer));
        cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, pAlignmentPatterns[cIndex]);
        if(cSuccess) { LOG(DEBUG) << BOLDBLUE << "Calibration pattern [for word alignment] on stub line " << +cIndex << " set to " << std::bitset<8>(pAlignmentPatterns[cIndex]) << RESET; }
    }
    return cSuccess;
}
// manually set Bx0 alignment
bool CicInterface::ManualBx0Alignment(Chip* pChip, uint8_t pBx0delay)
{
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_BX0_DELAY" : "BX0_ALIGN_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x7F) | (0x01 << 7));
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Manually settomg BX0 delay value in CIC on FE" << +pChip->getHybridId() << " to " << +pBx0delay << " clock cycles." << RESET;
    bool cSuccess = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;
    cSuccess = cSuccess && this->WriteChipReg(pChip, "EXT_BX0_DELAY", pBx0delay);
    return cSuccess;
}
// run automated Bx0 alignment
bool CicInterface::ConfigureBx0Alignment(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns, uint8_t pFEId, uint8_t pLineId)
{
    std::vector<uint8_t> cFeMapping{3, 2, 1, 0, 4, 5, 6, 7}; // FE --> FE CIC
    setBoard(pChip->getBeBoardId());
    LOG(DEBUG) << BOLDBLUE << "Running automated word alignment in CIC on FE" << +pChip->getHybridId() << RESET;
    LOG(DEBUG) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    bool cSuccess = ConfigureAlignmentPatterns(pChip, pAlignmentPatterns);

    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_BX0_DELAY" : "BX0_ALIGN_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x7F) | (0x00 << 7));
    cSuccess              = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "BX0_ALIGNMENT_FE" : "BX0_ALIGN_CONFIG";
    cRegValue = this->ReadChipReg(pChip, cRegName);
    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pFEId : ((cRegValue & 0xC7) | (cFeMapping[pFEId] << 3));
    cSuccess  = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "BX0_ALIGNMENT_LINE" : "BX0_ALIGN_CONFIG";
    cRegValue = this->ReadChipReg(pChip, cRegName);
    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pLineId : ((cRegValue & 0xF8) | (pLineId << 0));
    cSuccess  = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    fBoardFW->ChipReSync();
    return cSuccess;
}
bool CicInterface::AutoBx0Alignment(Chip* pChip, uint8_t pStatus)
{
    // make sure auto WA request is 0
    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_WA_REQUEST" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 0));
    bool        cSuccess   = this->WriteChipReg(pChip, cRegName, cToggleOff);
    if(!cSuccess) return cSuccess;

    cRegName        = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_BX0_ALIGNMENT_REQUEST" : "BX0_ALIGN_CONFIG";
    cRegValue       = this->ReadChipReg(pChip, cRegName);
    uint16_t cValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0xF8) | (pStatus << 6));
    cSuccess        = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess) return cSuccess;

    return cSuccess;
}
std::pair<bool, uint8_t> CicInterface::CheckBx0Alignment(Chip* pChip)
{
    uint8_t cDelay = 0;
    setBoard(pChip->getBeBoardId());

    bool cSuccess = this->AutoBx0Alignment(pChip, 0);
    if(!cSuccess) return std::make_pair(cSuccess, cDelay);
    // remember to send a resync
    fBoardFW->ChipReSync();

    // check status
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x84 : 0xA6;
    cRegItem.fStatusReg                 = 0x01;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
    if(!cReadBack.first)
    {
        LOG(ERROR) << BOLDRED << "Read-back failed!" << RESET;
        return std::make_pair(false, cDelay);
    }
    uint16_t cReadBackValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second) : ((cReadBack.second & 0x02) >> 1);
    cSuccess                = (cReadBackValue == 1);

    // read back delay
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAF : 0xA7;
    cRegItem.fStatusReg                 = 0x01;
    cReadBack         = this->ReadChipRegItem(pChip, cRegItem);
    if(cReadBack.first)
        cDelay = cReadBack.second;
    else
        cSuccess = false;
    return std::make_pair(cSuccess, cDelay);
}
// run automated word alignment
// assumes FEs have been configured to output alignment pattern
bool CicInterface::AutomatedWordAlignment(Chip* pChip, std::vector<uint8_t> pAlignmentPatterns, int pWait_ms)
{
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Running automated word alignment in CIC on FE" << +pChip->getHybridId() << RESET;
    LOG(INFO) << BOLDBLUE << "Configuring word alignment patterns on CIC" << RESET;
    bool cSuccess = ConfigureAlignmentPatterns(pChip, pAlignmentPatterns);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot configure patterns on CIC.." << RESET;
        exit(0);
    }

    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_WA_DELAY" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 1));
    uint16_t    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 1));

    cSuccess = this->WriteChipReg(pChip, cRegName, cToggleOn);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot disable external word alignment value on CIC.." << RESET;
        exit(0);
    }

    cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "AUTO_WA_REQUEST" : "MISC_CTRL";
    cRegValue  = this->ReadChipReg(pChip, cRegName);
    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 0));
    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 0));
    cSuccess   = this->WriteChipReg(pChip, cRegName, cToggleOn);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot send external word alignment request to CIC.." << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDBLUE << "Running automated word alignment .... " << RESET;
    // check if word alingment is done
    bool    cDone          = false;
    uint8_t cMaxIterations = (pWait_ms / 100);
    uint8_t cIteration     = 0;
    bool    cStop          = false;
    do
    {
        // check status
        ChipRegItem cRegItem;
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x83 : 0xA6;
        cRegItem.fStatusReg                 = 0x01;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
        if(!cReadBack.first)
        {
            LOG(INFO) << BOLDBLUE << "Readback failed.." << RESET;
            cDone = false;
            exit(0);
        }
        cDone = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : ((cReadBack.second & 0x01) == 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if(cIteration % 10 == 0) LOG(INFO) << BOLDBLUE << "\t....Iteration " << +cIteration << " ... : " << cDone << RESET;
        // stop either if done or if the maximum number of iterations
        // has been exceeded
        cStop = cDone || (cIteration > cMaxIterations);
        cIteration += 1;
    } while(!cStop);
    if(!cDone) { return cDone; }
    
    LOG(INFO) << BOLDBLUE << "Requesting CIC to stop automated word alignment..." << RESET;
    cSuccess = this->WriteChipReg(pChip, cRegName, cToggleOff);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDRED << "Cannot disable automated Word alignment request.." << RESET;
        exit(0);
    }

    if( cSuccess )
    {
        ConfigureExternalWordAlignment(pChip);
    }

    return cSuccess;
}
bool CicInterface::ResetDLL(Chip* pChip, uint16_t pWait_ms)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Resetting DLL in CIC" << RESET;
    // apply a channel reset
    LOG(INFO) << BOLDBLUE << "\t.... Enabling RESET on DLL" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "scDllResetReq%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0xFF);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC DLL reset" << RESET;
            exit(0);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    // release channel reset
    LOG(INFO) << BOLDBLUE << "\t... Disabling RESET on DLL" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[14];
        sprintf(cBuffer, "scDllResetReq%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC DLL reset" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
// check DLL lock in CIC
bool CicInterface::CheckDLL(Chip* pChip)
{
    uint16_t cRegAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x5A : 0x9A;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking DLL lock in CIC" << RESET;
    ChipRegItem           cRegItem;
    std::vector<uint16_t> cValues(2);
    for(int cIndex = 0; cIndex < (int)cValues.size(); cIndex += 1)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cRegAddress + cIndex;
        cRegItem.fStatusReg                 = 0x01;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
        if(cReadBack.first) cValues[cIndex] = cReadBack.second & 0xFF;
        LOG(DEBUG) << BOLDBLUE << "Lock" << cIndex << " -- " << cValues[cIndex] << RESET;
    }
    uint16_t cLock   = (cValues[1] << 8) | cValues[0];
    bool     cLocked = (cLock == 0xFFF);
    return cLocked;
}
bool CicInterface::SetAutomaticPhaseAlignment(Chip* pChip, bool pAuto)
{
    setBoard(pChip->getBeBoardId());
    if(pAuto)
        LOG(INFO) << BOLDBLUE << "Configuring CIC to use automatic phase aligner..." << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Configuring CIC to use static phase aligner..." << RESET;
    // set phase aligner in static mode
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "scTrackMode" : "PHY_PORT_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pAuto : ((cRegValue & 0x07) | (pAuto << 3));
    bool        cSuccess  = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess)
    {
        LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
        exit(0);
    }
    if( pAuto )
    {
        this->ResetPhaseAligner(pChip);
    }
    return cSuccess;
}
bool CicInterface::PhaseAlignerPorts(Chip* pChip, uint8_t pState)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    if(pState == 1)
        LOG(INFO) << BOLDGREEN << "Enabling " << BOLDBLUE << " all CIC phase aligner input..." << RESET;
    else
        LOG(INFO) << BOLDRED << "Disabling " << BOLDBLUE << " all CIC phase aligner input..." << RESET;
    for(uint8_t cIndex = 0; cIndex < 6; cIndex += 1)
    {
        char cBuffer[13];
        sprintf(cBuffer, "scEnableLine%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, (pState == 1) ? 0xFF : 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error conifguring CIC" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
bool CicInterface::ResetPhaseAligner(Chip* pChip, uint16_t pWait_ms)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Resetting CIC phase aligner..." << RESET;
    // apply a channel reset
    LOG(INFO) << BOLDBLUE << "\t.... Enabling RESET on all phase aligner inputs" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[16];
        sprintf(cBuffer, "scResetChannels%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0xFF);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC phase aligner reset" << RESET;
            exit(0);
        }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(pWait_ms));
    this->CheckPhaseAlignerLock(pChip, 0x00);
    // release channel reset
    LOG(INFO) << BOLDBLUE << "\t... Disabling RESET on all phase aligner inputs" << RESET;
    for(uint8_t cIndex = 0; cIndex < 2; cIndex += 1)
    {
        char cBuffer[16];
        sprintf(cBuffer, "scResetChannels%.1d", cIndex);
        std::string cRegName = std::string(cBuffer, sizeof(cBuffer));
        cSuccess             = this->WriteChipReg(pChip, cRegName, 0x00);
        if(!cSuccess)
        {
            LOG(ERROR) << BOLDRED << "Error setting CIC phase aligner reset" << RESET;
            exit(0);
        }
    }
    return cSuccess;
}
// FIX ME
// bool CicInterface::SetStaticPhaseAlignment(Chip* pChip, uint8_t pReadoutChipId, uint8_t pLineId, uint8_t pPhase)
// {
//     std::string          cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
//     auto                 cFeType    = this->ReadChipReg(pChip, cRegName);
//     bool                 c2S        = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cFeType == 0) : ((cFeType & 0x01) == 0);
//     std::vector<uint8_t> cFeMapping = c2S ? fFeMapping2S : fFeMappingPSR;
//     if(!c2S) cFeMapping = (pChip->getId() % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;

//     bool    cL1Selected = (pLineId == 0);
//     uint8_t cPort       = 0;
//     uint8_t cInput      = 0;
//     if(cL1Selected)
//     {
//         // uint8_t cChipId_onyHybrid = std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), cFeCounter));
//         uint8_t cChipId_forCic = cFeMapping[pReadoutChipId]; // std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), pReadoutChipId));
//         LOG(DEBUG) << BOLDBLUE << "Modifying phase alignment value for readout chip " << +pReadoutChipId << " [ in CIC mapping this is FE" << +cChipId_forCic << " ]" << RESET;
//         cInput = cChipId_forCic & 0x3;
//         cPort  = 10 + ((cChipId_forCic & 0x4) >> 2);
//         LOG(DEBUG) << BOLDBLUE << "Will modifiy phase aligner value for phy port " << +cPort << " input " << +cInput << RESET;
//     }
//     // I assume this has already been done
//     // bool cSuccess = SetAutomaticPhaseAlignment(pChip, false);
//     bool cSuccess = true;
//     setBoard(pChip->getBeBoardId());
//     if(cSuccess)
//     {
//         ChipRegItem cRegItem;
//         uint8_t     cIndex = std::floor(cPort / 2.);
//         char        cBuffer[17];
//         sprintf(cBuffer, "scPhaseSelectB%di%d", cInput, cIndex);
//         std::string cRegName(cBuffer, sizeof(cBuffer));
//         uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
//         uint8_t     cMask     = ~(0xF << ((cPort & 0x1))) & 0xFF;
//         uint16_t    cValue    = (cRegValue & cMask) | pPhase;
//         LOG(DEBUG) << BOLDBLUE << "Will modifiy register " << cBuffer << " which was set to " << std::bitset<8>(cRegValue) << " -- new value is " << std::bitset<8>(cValue) << RESET;
//         cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
//     }
//     if(!cSuccess)
//     {
//         LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
//         exit(0);
//     }
//     return cSuccess;
// }
// FIX ME
bool CicInterface::SetStaticPhaseAlignment(Chip* pChip)
{
    bool cSuccess = SetAutomaticPhaseAlignment(pChip, false);
    if(!cSuccess) return cSuccess; 

    cSuccess =this->SetOptimalTaps(pChip);
    return cSuccess; 
    // setBoard(pChip->getBeBoardId());
    // if(cSuccess)
    // {
    //     ChipRegItem cRegItem;
    //     for(uint8_t cInput = 0; cInput < 4; cInput += 1)
    //     {
    //         uint8_t cPhyPort = 0;
    //         for(uint8_t cIndex = 0; cIndex < 6; cIndex += 1)
    //         {
    //             char cBuffer[17];
    //             sprintf(cBuffer, "scPhaseSelectB%di%d", cInput, cIndex);
    //             uint8_t     cValue = (pPhaseTaps[cInput][cPhyPort + 1] << 4) | (pPhaseTaps[cInput][cPhyPort]);
    //             std::string cRegName(cBuffer, sizeof(cBuffer));
    //             LOG(DEBUG) << BOLDBLUE << "Input" << +cInput << " : " << +pPhaseTaps[cInput][cPhyPort + 1] << " " << +pPhaseTaps[cInput][cPhyPort] << " " << cRegName << "  ---> "
    //                        << std::bitset<8>(cValue) << RESET;
    //             cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
    //             cPhyPort += 2;
    //         }
    //     }
    // }
    // if(!cSuccess)
    // {
    //     LOG(ERROR) << BOLDRED << "Error configuring CIC" << RESET;
    //     exit(0);
    // }
    // return cSuccess;
}

bool CicInterface::ConfigureExternalWordAlignment(Chip* pChip)
{
    UpdateExternalWordAlignmentValues(pChip);
    size_t cCounter=0; 
    uint8_t cValue=0x00;
    int cIndx = 0; 
    bool cSuccess = true;
    for( size_t cFeId = 0; cFeId < 8; cFeId++)
    {
        for(size_t cLine=0; cLine < 5 ; cLine++)
        {
            cValue = cValue | ( ( fWordAlignmentVals[cFeId][cLine] & 0xF ) << (cCounter%2)*4 );
            if( (1+cCounter)%2 == 0  ) 
            {  
                char cBuffer[14];  
                sprintf(cBuffer, "EXT_WA_DELAY%.2d", cIndx);
                std::string cRegName(cBuffer, sizeof(cBuffer));
                // LOG(INFO) << BOLDBLUE << "\t..Setting static word alignment in register " 
                //     << cBuffer
                //     << " to " 
                //     << +cValue 
                //     << RESET;
                cSuccess = cSuccess && this->WriteChipReg(pChip, cRegName, cValue);
                cValue = 0x00; 
                cIndx++;
            }
            cCounter++;
        }
    }
    return cSuccess;
}
bool CicInterface::SetStaticWordAlignment(Chip* pChip, uint8_t pValue)
{
    if( pValue == 0 )
        LOG(INFO) << BOLDBLUE << "Configuring word alignment in CIC#"
            << +pChip->getId()
            << " to use external values" << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Configuring word alignment in CIC#"
            << +pChip->getId()
            << " to use internal values" << RESET;

    if( pValue == 1 ) 
        if( !this->ConfigureExternalWordAlignment(pChip) ) return false;

    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "USE_EXT_WA_DELAY" : "MISC_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOn = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : ((cRegValue & 0x1D) | (0x1 << 1));
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : ((cRegValue & 0x1D) | (0x0 << 1));

    uint8_t cValue = ( pValue == 0 ) ? cToggleOff : cToggleOn; 
    bool cSuccess = this->WriteChipReg(pChip, cRegName, cValue);
    return cSuccess;
}
std::vector<std::vector<uint8_t>> CicInterface::GetWordAlignmentValues(Chip* pChip)
{
    //UpdateExternalWordAlignmentValues(pChip);
    return fWordAlignmentVals;
}
void CicInterface::UpdateExternalWordAlignmentValues(Chip* pChip)
{
    setBoard(pChip->getBeBoardId());
    // 5 lines per FE ... 8 FEs per CIC
    fWordAlignmentVals.clear();
    std::vector<std::vector<uint8_t>> cWordAlignmentValues(8, std::vector<uint8_t>(5, 0));
    for( size_t cIndx=0; cIndx<8; cIndx++)
    {
        std::vector<uint8_t> cTmp(5,0);
        fWordAlignmentVals.push_back( cTmp );
    }
    uint8_t                           cLineCounter = 0;
    uint8_t                           cFECounter   = 0;
    ChipRegItem                       cRegItem;
    bool                              cSuccess     = true;
    uint16_t                          cBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x85 : 0xA8;
    for(uint8_t cIndex = 0; cIndex < 20; cIndex += 1)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cBaseAddress + cIndex;
        cRegItem.fStatusReg                 = 0x01;
        std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
        cSuccess                            = cSuccess && cReadBack.first;
        if(cSuccess)
        {
            LOG(DEBUG) << BOLDBLUE << "Word alignment value found to be " << std::bitset<8>(cReadBack.second) << RESET;
            for(uint8_t cNibble = 0; cNibble < 2; cNibble += 1)
            {
                uint8_t cWordAlignment                         = (cReadBack.second & (0xF << cNibble * 4)) >> 4 * cNibble;
                cWordAlignmentValues[cFECounter][cLineCounter] = cWordAlignment;
                fWordAlignmentVals[cFECounter][cLineCounter] = cWordAlignment;
                LOG(DEBUG) << BOLDBLUE << "Word alignment for FE" << +cFECounter << " Line" << +cLineCounter << " value found to be " << +cWordAlignment << RESET;
                cLineCounter += 1;
                if(cLineCounter > 4)
                {
                    cLineCounter = 0;
                    cFECounter += 1;
                }
            }
        }
    }
}

bool CicInterface::SetOptimalTap(Chip* pChip, uint8_t pPhyPort, uint8_t pPhyPortChannel, int pOffset)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    uint16_t    cBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x20 : 0x25;
    uint16_t    cBaseReg     = cBaseAddress + pPhyPortChannel * 6;

    bool cVerifloop = true;
    // 4 bits per phyPorts --> 12 phy ports --> 48 bits --> 6 registers
    uint8_t cRegOffset = (pPhyPort * 4 / 8);
    uint8_t cBitShift  = (pPhyPort % 2) * 4;

    cRegItem.fPage     = 0x00;
    cRegItem.fAddress  = cBaseReg + cRegOffset;
    cRegItem.fStatusReg                 = 0x01;

    uint8_t cRegMask   = (0xF << cBitShift); //
    cRegMask           = ~(cRegMask);
    uint16_t cRegValue = this->ReadChipRegItem(pChip, cRegItem).second;
    int      cPhaseTap = int(fPhaseTaps[pPhyPortChannel][pPhyPort]) + pOffset;
    if(cPhaseTap < 0 || cPhaseTap > 0xF){ 
        LOG(DEBUG) << BOLDRED << "FAILED TO modify optimal tap for PhyPort" << +pPhyPort << " PhyPortChannel " << +pPhyPortChannel << " .. wanted to set a phase tap of " 
            << +cPhaseTap
            << " when original was "
            << +fPhaseTaps[pPhyPortChannel][pPhyPort]
            << RESET;
        cPhaseTap =  int(fPhaseTaps[pPhyPortChannel][pPhyPort]);
    }// revert to optimal if offset makes no sense
    uint8_t cValue = (cRegValue & cRegMask) | (cPhaseTap << cBitShift);

    // LOG(INFO) << BOLDGREEN << "Setting optimal tap for PhyPort" << +pPhyPort << " PhyPortChannel " << +pPhyPortChannel << " Register mask is 0x" << std::hex << +cRegMask << std::dec << " Register 0x"
    //            << std::hex << +(cBaseReg + cRegOffset) << std::dec << " BitOffset " << +cBitShift << " to 0x" << std::hex << +cValue << std::dec << " to set a phase tap of "
    //            << +cPhaseTap 
    //            << " original register value is 0x" << std::hex << +cRegValue << std::dec 
    //            << " - modified register so that phase value is " << +(fPhaseTaps[pPhyPortChannel][pPhyPort]) 
    //            << RESET;
    return WriteReg(pChip, cRegItem.fAddress, cValue, cVerifloop);
}
bool CicInterface::SetOptimalTaps(Chip* pChip, int pOffset)
{
    bool cSuccess = true;
    for(uint8_t cPhyPortChannel = 0; cPhyPortChannel < 4; cPhyPortChannel++)
    {
        LOG(DEBUG) << BOLDBLUE << "Phy port input channel " << +cPhyPortChannel << RESET;
        for(uint8_t cPhyPort = 0; cPhyPort < 12; cPhyPort++)
        {
            LOG(DEBUG) << BOLDBLUE << "\t.. phy port " << +cPhyPort << RESET;
            cSuccess = cSuccess && this->SetOptimalTap(pChip, cPhyPort, cPhyPortChannel, pOffset);
        }
    }
    return cSuccess;
}
uint8_t CicInterface::GetOptimalTap(Chip* pChip, uint8_t pPhyPort, uint8_t pPhyPortChannel)
{
    uint8_t cPhaseTap = 0;
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    bool        cSuccess     = true;
    uint16_t    cBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x40 : 0x80;
    uint16_t    cBaseReg     = cBaseAddress + pPhyPortChannel * 6;
    // 4 bits per phyPorts --> 12 phy ports --> 48 bits --> 6 registers
    uint8_t cRegOffset = (pPhyPort * 4 / 8);
    uint8_t cBitOffset = (pPhyPort % 2);

    cRegItem.fPage                      = 0x00;
    cRegItem.fAddress                   = cBaseReg + cRegOffset;
    cRegItem.fStatusReg                 = 0x01;

    std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
    LOG(DEBUG) << BOLDBLUE << "Reading optimal tap for PhyPort" << +pPhyPort << " PhyPortChannel " << +pPhyPortChannel << " Register 0x" << std::hex << +(cBaseReg + cRegOffset) << std::dec
               << " BitOffset " << +cBitOffset << RESET;
    if(cReadBack.first)
    {
        cSuccess                              = cSuccess && cReadBack.first;
        cPhaseTap                             = (cReadBack.second & (0xF << (cBitOffset * 4))) >> (cBitOffset * 4);
        LOG(DEBUG) << BOLDBLUE << "Reading optimal tap for PhyPort" << +pPhyPort << " PhyPortChannel " << +pPhyPortChannel << " Optimal tap " << +cPhaseTap << RESET;
    }
    return cPhaseTap;
}
bool CicInterface::ReadOptimalTap(Chip* pChip, uint8_t pPhyPortChannel, std::vector<std::vector<uint8_t>>& pPhaseTaps)
{
    bool cSuccess = true;
    LOG(DEBUG) << BOLDBLUE << "Reading optimal tap found by Auto phase aligner lock in CIC for channel number " << +pPhyPortChannel << RESET;
    // 12 phy ports
    // 4 inputs per phy port
    for(int cPhyPort = 0; cPhyPort < 12; cPhyPort++) { pPhaseTaps[pPhyPortChannel][cPhyPort] = this->GetOptimalTap(pChip, cPhyPort, pPhyPortChannel); }
    return cSuccess;
}
std::vector<std::vector<uint8_t>> CicInterface::GetOptimalTaps(Chip* pChip)
{
    // 4 channels per phyPort ... 12 phyPorts per CIC
    std::vector<std::vector<uint8_t>> cPhaseTaps(4, std::vector<uint8_t>(12, 0));
    for(uint8_t cPhyPortChannel = 0; cPhyPortChannel < 4; cPhyPortChannel += 1) { this->ReadOptimalTap(pChip, cPhyPortChannel, cPhaseTaps); }
    return cPhaseTaps;
}
std::vector<uint8_t> CicInterface::GetOptimalTaps(Chip* pChip, uint8_t pFeId)
{
    std::string          cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
    auto                 cFeType    = this->ReadChipReg(pChip, cRegName);
    bool                 c2S        = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cFeType == 0) : ((cFeType & 0x01) == 0);
    std::vector<uint8_t> cFeMapping = c2S ? fFeMapping2S : fFeMappingPSR;
    if(!c2S) cFeMapping = (pChip->getId() % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
    else if(fWith8CBC3) cFeMapping = fFeMapping8BC3;


    // 6 outputs fer FE
    uint32_t cPhaseTapsThisFe = (uint32_t)(std::bitset<24>(fPhaseValues[cFeMapping[pFeId]]).to_ulong());
    LOG(DEBUG) << BOLDBLUE << "Optimal Taps for FE [ internal counter : " << +cFeMapping[pFeId] << " position on hybrid : " << +pFeId << " ] "
               << " is " << std::bitset<24>(cPhaseTapsThisFe) << RESET;

    std::vector<uint8_t> cPhaseTaps(6, 0);
    for(size_t cIndx = 0; cIndx < cPhaseTaps.size(); cIndx++)
    {
        cPhaseTaps[cIndx] = (cPhaseTapsThisFe & (0xF << cIndx * 4)) >> (cIndx * 4);
        LOG(DEBUG) << BOLDBLUE << "Optimal Tap for FE [ internal counter : " << +cFeMapping[pFeId] << " position on hybrid : " << +pFeId << " ] , line# " << +cIndx << " is " << +cPhaseTaps[cIndx]
                   << " taps." << RESET;
    }
    return cPhaseTaps;
}
bool CicInterface::CheckPhaseAlignerLock(Chip* pChip, uint8_t pCheckValue)
{
    // first .. get enabled FEs
    setBoard(pChip->getBeBoardId());
    std::string cRegName    = "FE_ENABLE";
    uint8_t     cEnabledFEs = this->ReadChipReg(pChip, cRegName);
    LOG(INFO) << BOLDMAGENTA << "FE_Enable Register set to " << +cEnabledFEs << RESET;

    uint16_t cRegBaseAddress = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x5C : 0xA0;
    LOG(DEBUG) << BOLDBLUE << "Checking Auto phase aligner lock in CIC." << RESET;
    ChipRegItem cRegItem;
    bool        cLocked = true;

    size_t cPortCounter       = 0;
    size_t cInputCounter      = 0;
    size_t cFeCounter         = 0;
    size_t cInputLineCounter  = 0;
    size_t cCounter           = 0;
    size_t cNStubLines        = 5;
    size_t cL1Line            = 5;
    bool   cLastStubLineFound = false;

    cRegName                        = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
    auto                 cFeType    = this->ReadChipReg(pChip, cRegName);
    bool                 c2S        = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cFeType == 0) : ((cFeType & 0x01) == 0);
    std::vector<uint8_t> cFeMapping = c2S ? fFeMapping2S : fFeMappingPSR;
    if(!c2S) cFeMapping = (pChip->getId() % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
    else if(fWith8CBC3) cFeMapping = fFeMapping8BC3;


    // read back phase alignment on stub lines
    for(int cIndex = 0; cIndex < 6; cIndex++)
    {
        cRegItem.fPage                      = 0x00;
        cRegItem.fAddress                   = cRegBaseAddress + cIndex;
        cRegItem.fStatusReg                 = 0x01;

        std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
        LOG(INFO) << BOLDBLUE << "Lock on input " << cIndex << " -- " << std::bitset<8>(cReadBack.second) << RESET;

        for(size_t cBitIndex = 0; cBitIndex < 8; cBitIndex++)
        {
            auto cAligned            = (cReadBack.second & (0x1 << cBitIndex)) >> cBitIndex;
            cInputCounter            = (cBitIndex & 0x3);
            cInputLineCounter        = (cIndex < 5) ? (cCounter % cNStubLines) : cL1Line;
            cLastStubLineFound       = cLastStubLineFound || (cFeCounter == 7 && cInputLineCounter == 4);
            cFeCounter               = (cLastStubLineFound) ? cBitIndex : cFeCounter;
            auto     cPhaseTap       = this->GetOptimalTap(pChip, cPortCounter, cInputCounter);
            uint32_t cPhaseValue     = (cInputLineCounter == 0) ? cPhaseTap : (fPhaseValues[cFeCounter].to_ulong() | (cPhaseTap << cInputLineCounter * 4));
            
            // and save to register
            if(cAligned == 0x1)
            { 
                fPhaseValues[cFeCounter] = std::bitset<24>(cPhaseValue);
                fPhaseTaps[cInputCounter][cPortCounter] = cPhaseTap;
                this->SetOptimalTap(pChip, cPortCounter, cInputCounter);
                if( cPortCounter < 10 )
                    LOG(DEBUG) << BOLDGREEN << "\t.. PhyPort#" << +cPortCounter << " input#" << (+cInputCounter) << " FE#" << +cFeCounter << " StubLine#" << +cInputLineCounter << " -- Alignment value is "
                        << +cAligned << " phase tap value is " << +cPhaseTap << " stored value is " << std::bitset<24>(fPhaseValues[cFeCounter]) << RESET;
                else
                    LOG(DEBUG) << BOLDGREEN << "\t.. PhyPort#" << +cPortCounter << " input#" << (+cInputCounter) << " FE#" << +cFeCounter << " L1 Line -- Alignment value is "
                        << +cAligned << " phase tap value is " << +cPhaseTap << " stored value is " << std::bitset<24>(fPhaseValues[cFeCounter]) << RESET;
            }
            else
                if( cPortCounter < 10 )
                    LOG(DEBUG) << BOLDRED << "\t.. PhyPort#" << +cPortCounter << " input#" << (+cInputCounter) << " FE#" << +cFeCounter << " StubLine#" << +cInputLineCounter << " -- Alignment value is "
                        << +cAligned << " phase tap value is " << +cPhaseTap << " stored value is " << std::bitset<24>(fPhaseValues[cFeCounter]) << RESET;
                else
                    LOG(DEBUG) << BOLDRED << "\t.. PhyPort#" << +cPortCounter << " input#" << (+cInputCounter) << " FE#" << +cFeCounter << " L1 Line -- Alignment value is "
                        << +cAligned << " phase tap value is " << +cPhaseTap << " stored value is " << std::bitset<24>(fPhaseValues[cFeCounter]) << RESET;
                
            fPortStates[cPortCounter][cInputCounter] = std::bitset<8>(cReadBack.second)[cBitIndex];
            fFeStates[cFeCounter][cInputLineCounter] = std::bitset<8>(cReadBack.second)[cBitIndex];

            cPortCounter += (cBitIndex == 3) || (cBitIndex == 7);
            cFeCounter = (!cLastStubLineFound) ? (cFeCounter + (((1 + cCounter) % cNStubLines == 0) ? 1 : 0)) : cBitIndex;
            cCounter++;
        }
    } // each register stores information from 4 phyport inputs
    // 1 bit for each of the 4 channels of the 12 PHYPorts

    cPortCounter = 0;
    for(cFeCounter = 0; cFeCounter < 8; cFeCounter++)
    {
        auto cEnableBit = (cEnabledFEs & (0x1 << cFeCounter)) >> cFeCounter;
        // only check enabled FEs
        if(cEnableBit != 1) continue;

        uint8_t cChipId_onyHybrid = std::distance(cFeMapping.begin(), std::find(cFeMapping.begin(), cFeMapping.end(), cFeCounter));
        auto    cCheckValue       = (pCheckValue & (0x1 << cFeCounter)) >> cFeCounter;
        for(cInputLineCounter = 0; cInputLineCounter < (1 + cNStubLines); cInputLineCounter++)
        {
            LOG(DEBUG) << BOLDYELLOW << "FE [CIC internal counter : " << +cFeCounter << " , position on hybrid : " << +cChipId_onyHybrid << " Line#" << +cInputLineCounter << " alignment value "
                       << +fFeStates[cFeCounter][cInputLineCounter] << RESET;
            cLocked = cLocked & (fFeStates[cFeCounter][cInputLineCounter] == cCheckValue);
        }
        if(cLocked)
            LOG(DEBUG) << BOLDGREEN << "PhyPort lock FE[ CIC internal counter : " << +cFeCounter << " , position on hybrid : " << +cChipId_onyHybrid << "] : " << BOLDMAGENTA
                      << +fFeStates[cFeCounter][cNStubLines] << BOLDBLUE << " [L1 lines] " << BOLDGREEN << std::bitset<5>(fFeStates[cFeCounter].to_ulong() & 0x1F) << " [Stub lines 0 -- 4]" << RESET;
        else
            LOG(DEBUG) << BOLDRED << "PhyPort lock FE[ CIC internal counter : " << +cFeCounter << " , position on hybrid : " << +cChipId_onyHybrid << "] : " << BOLDMAGENTA
                      << +fFeStates[cFeCounter][cNStubLines] << BOLDBLUE << " [L1 lines] " << BOLDRED << std::bitset<5>(fFeStates[cFeCounter].to_ulong() & 0x1F) << " [Stub lines 0 -- 4]" << RESET;
    }
    if(cLocked)
        LOG(INFO) << BOLDGREEN << "SUCCESSFULL " << BOLDBLUE << " lock on all phase aligner lines.." << RESET;
    else
        LOG(INFO) << BOLDRED << "FAILED " << BOLDBLUE << " to lock on all phase aligner lines.." << RESET;
    return cLocked;
}
bool CicInterface::SoftReset(Chip* pChip, uint32_t cWait_ms)
{
    setBoard(pChip->getBeBoardId());

    std::string cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "SOFT_RESET" : "MISC_CTRL";
    uint16_t    cRegValue  = this->ReadChipReg(pChip, cRegName);
    uint16_t    cToggleOn  = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x01 : (cRegValue & 0x0F) | (0x1 << 4);
    uint16_t    cToggleOff = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0x00 : (cRegValue & 0x0F) | (0x0 << 4);

    LOG(INFO) << BOLDBLUE << "Setting register " << cRegName << " to " << std::bitset<5>(cToggleOn) << " to toggle ON soft reset." << RESET;
    if(!this->WriteChipReg(pChip, cRegName, cToggleOn)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(cWait_ms));

    LOG(INFO) << BOLDBLUE << "Setting register " << cRegName << " to " << std::bitset<5>(cToggleOff) << " to toggle OFF soft reset." << RESET;
    if(!this->WriteChipReg(pChip, cRegName, cToggleOff)) return false;
    std::this_thread::sleep_for(std::chrono::milliseconds(cWait_ms));
    return true;
}
bool CicInterface::SelectOutput(Chip* pChip, bool pFixedPattern)
{
    setBoard(pChip->getBeBoardId());
    if(pFixedPattern)
        LOG(INFO) << BOLDBLUE << "Want to configure CIC to output fixed pattern on all lines... " << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Want to configure CIC to output data from readout chips... " << RESET;

    // enable output pattern from CIC
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "OUTPUT_PATTERN_ENABLE" : "MISC_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? static_cast<uint8_t>(pFixedPattern) : ((cRegValue & 0x1B) | (static_cast<uint8_t>(pFixedPattern) << 2));

    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    cRegValue = this->ReadChipReg(pChip, cRegName);
    LOG(INFO) << BOLDBLUE << "CIC output pattern configured by setting " << cRegName << " to " << std::bitset<8>(cRegValue) << RESET;
    return true;
}
bool CicInterface::SetSparsification(Chip* pChip, uint8_t pEnable)
{
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBC_SPARSIFICATION_SEL" : "FE_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pEnable : (cRegValue & 0x2F) | (pEnable << 4);
    return this->WriteChipReg(pChip, cRegName, cValue);
}
bool CicInterface::EnableFEs(Chip* pChip, std::vector<uint8_t> pFeIds, bool pEnable)
{
    setBoard(pChip->getBeBoardId());

    //  read type of CIC to figure out which mapping to use
    std::string          cRegName   = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
    auto                 cFeType    = this->ReadChipReg(pChip, cRegName);
    bool                 c2S        = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cFeType == 0) : ((cFeType & 0x01) == 0);
    std::vector<uint8_t> cFeMapping = c2S ? fFeMapping2S : fFeMappingPSR;
    if(!c2S) cFeMapping = (pChip->getId() % 2 == 0) ? fFeMappingPSR : fFeMappingPSL;
    else if(fWith8CBC3) cFeMapping = fFeMapping8BC3;

    // read enable register
    cRegName        = "FE_ENABLE";
    uint16_t cValue = this->ReadChipReg(pChip, cRegName);
    LOG (INFO) << BOLDMAGENTA << "FE_ENABLE register set to 0x" << std::hex  << +cValue << std::dec << RESET;
    for(auto pFeId: pFeIds)
    {
        uint8_t cChipId_forCic = cFeMapping[pFeId]; // std::distance(fFeMapping.begin(), std::find(fFeMapping.begin(), fFeMapping.end(), pFeId));
        uint8_t cMask          = ~(0x1 << cChipId_forCic) & 0xFF;
        LOG(INFO) << BOLDMAGENTA << "For ROC [Hybrid Id " << +pFeId << "] CIC FE#" << +cChipId_forCic << " mask is " << std::bitset<8>(cMask) << RESET;
        cValue = (cValue & cMask) | (static_cast<uint8_t>(pEnable) << cChipId_forCic);
    }
    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    LOG(INFO) << BOLDBLUE << "Setting FE enable register [" << cRegName << "] to " << std::bitset<8>(cValue) << RESET;
    return true;
}
bool CicInterface::SelectMode(Chip* pChip, uint8_t pMode)
{
    setBoard(pChip->getBeBoardId());

    LOG(INFO) << BOLDBLUE << "Want to configure CIC mode : " << +pMode << RESET;
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "CBCMPA_SEL" : "FE_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pMode : ((cRegValue & 0x3E) | pMode);
    if(pMode == 0) // for CBC mode .. always320 MHz and without last line
        cValue = (cValue & 0x35) | (pMode << 1) | (pMode << 3);

    if(!this->WriteChipReg(pChip, cRegName, cValue)) return false;

    cRegValue = this->ReadChipReg(pChip, cRegName);
    LOG(INFO) << BOLDBLUE << "Register " << cRegName << " set to " << std::bitset<6>(cRegValue) << " to select CIC Mode " << +pMode << RESET;
    return true;
}
// bool CicInterface::PhaseAlignerStatus(Chip* pChip, std::vector<std::vector<uint8_t>>& pPhaseTaps )
// {
//     bool cLocked=this->CheckPhaseAlignerLock(pChip);
//     // 4 channels per phyPort ... 12 phyPorts per CIC
//     // 8 FEs per CIC .... 6 SLVS lines per FE
//     // read back phase aligner values
//     pPhaseTaps = fCicInterface->GetOptimalTaps( pChip);
//     return cLocked;
// }
bool CicInterface::CheckSoftReset(Chip* pChip)
{
    setBoard(pChip->getBeBoardId());
    LOG(INFO) << BOLDBLUE << "Checking if CIC requires a Soft reset." << RESET;
    ChipRegItem cRegItem;
    cRegItem.fPage                      = 0x00;
    cRegItem.fStatusReg                 = 0x01; 
    cRegItem.fAddress                   = (pChip->getFrontEndType() == FrontEndType::CIC) ? 0xAC : 0xA6;
    std::pair<bool, uint16_t> cReadBack = this->ReadChipRegItem(pChip, cRegItem);
    if(!cReadBack.first)
    {
        LOG(ERROR) << BOLDRED << "Read back failed!" << RESET;
        exit(0);
    }
    bool cSoftResetNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : (((cReadBack.second & 0x04) >> 2) == 1);
    // if readback worked and the CIC says it needs a Resync then send
    // a resync
    if(cSoftResetNeeded)
    {
        LOG(INFO) << BOLDBLUE << "....... Reset  needed - sending one now ...... " << RESET;
        SoftReset(pChip);
    }
    // check if CIC still needs one
    cReadBack        = this->ReadChipRegItem(pChip, cRegItem);
    cSoftResetNeeded = (pChip->getFrontEndType() == FrontEndType::CIC) ? (cReadBack.second == 1) : (((cReadBack.second & 0x04) >> 2) == 1);
    if(!cReadBack.first || cSoftResetNeeded)
        return false;
    else
        return true;
}
bool CicInterface::SelectMux(Chip* pChip, uint8_t pPhyPort)
{
    // first .. enable bypass of logic
    if(!this->ControlMux(pChip, 1)) return false;

    // then select phy port
    setBoard(pChip->getBeBoardId());
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "ctrlTestMux" : "MUX_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pPhyPort : (cRegValue & 0x10) | pPhyPort;
    LOG(INFO) << BOLDBLUE << "Selecting phyPort [0-11]: " << +pPhyPort << " by setting register to 0x" << std::hex << +cValue << std::dec << RESET;
    return this->WriteChipReg(pChip, cRegName, cValue);
}
bool CicInterface::ControlMux(Chip* pChip, uint8_t pEnable)
{
    setBoard(pChip->getBeBoardId());
    std::string cRegName  = (pChip->getFrontEndType() == FrontEndType::CIC) ? "enableMux" : "MUX_CTRL";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    uint16_t    cValue    = (pChip->getFrontEndType() == FrontEndType::CIC) ? pEnable : (cRegValue & 0xF) | (pEnable << 4);
    if(pEnable == 1)
        LOG(INFO) << BOLDBLUE << " Enabling CIC MUX .. so bypassing CIC logic " << RESET;
    else
        LOG(INFO) << BOLDBLUE << " Disabling CIC MUX .. so activating CIC logic  " << RESET;

    return this->WriteChipReg(pChip, cRegName, cValue);
}
// start-up sequence for CIC [everything that does not require interaction
// with the BE or the other readout ASICs on the chip
bool CicInterface::StartUp(Chip* pChip, uint8_t pDriveStrength)
{
    std::string cOut = ".... Starting CIC start-up ........ on hybrid " + std::to_string(pChip->getHybridId());
    if(pChip->getFrontEndType() == FrontEndType::CIC)
        cOut += " for CIC1.";
    else
        cOut += " for CIC2.";
    LOG(INFO) << BOLDBLUE << cOut << RESET;

    bool cSuccess = this->CheckSoftReset(pChip);
    // if( !cSuccess )
    // {
    //     LOG (INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " clear SOFT reset request in CIC...
    //     " << RESET; exit(0);
    // }
    // bool cSuccess = this->SoftReset(pChip);

    bool cClkTermination = true;
    bool cRxTermination  = true;
    //(pChip->getFrontEndType() == FrontEndType::CIC ) ? true : false ;// true, false -- this needs to be false for the
    // crate set-up .. how to fix this?!?!
    std::string cRegName  = "SLVS_PADS_CONFIG";
    uint16_t    cRegValue = this->ReadChipReg(pChip, cRegName);
    auto        cIterator = fTxDriveStrength.find(pDriveStrength);
    if(cIterator != fTxDriveStrength.end())
    {
        auto cValue = (cRxTermination << 4) | (cClkTermination << 3) | cIterator->second;
        cSuccess    = this->WriteChipReg(pChip, "SLVS_PADS_CONFIG", cValue);
        LOG(INFO) << BOLDBLUE << "Configuring drive strength on CIC output pads: 0x" << std::hex << +cValue << std::dec << RESET;
        if(!cSuccess)
        {
            LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " configure drive strength on CIC output pads." << RESET;
            exit(0);
        }
        cRegValue = this->ReadChipReg(pChip, cRegName);
        LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " configured drive strength on CIC output pads: 0x" << std::hex << +cRegValue << std::dec << RESET;
    }

    // reset DLL for each of the 12 phy ports
    cSuccess = this->ResetDLL(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " Reset DLL in CIC " << RESET;
        exit(0);
    }
    // checking DLL lock
    cSuccess = this->CheckDLL(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " LOCK DLL in CIC  " << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDBLUE << "DLL in CIC " << BOLDGREEN << " LOCKED." << RESET;

    // // figure out which FEs have been enabled
    // // so we can return to this state after
    // // the reset
    // cRegName            = "FE_ENABLE";
    // uint16_t cEnableReg = this->ReadChipReg(pChip, cRegName);
    // LOG(INFO) << BOLDMAGENTA << "Enable chip register, before phase aligner reset, set to " << std::bitset<8>(+cEnableReg) << RESET;
    // // disable all FEs
    // this->WriteChipReg(pChip, cRegName, 0x00);
    // // reset
    // this->ResetPhaseAligner(pChip, 200);
    // // enable original FEs
    // this->WriteChipReg(pChip, cRegName, cEnableReg);
    // cEnableReg = this->ReadChipReg(pChip, cRegName);
    // LOG(INFO) << BOLDMAGENTA << "Enable chip register, after phase aligner reset, set to " << std::bitset<8>(+cEnableReg) << RESET;

    // set phase aligner to static mode 
    bool cAutoAlign=false;
    cSuccess = this->SetAutomaticPhaseAlignment(pChip, cAutoAlign);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " set automatic phase aligner in CIC... " << RESET;
        exit(0);
    }


    // select fast command edge
    bool cNegEdge = true;// was false for PS - need to check 
    if(cNegEdge)
        LOG(INFO) << BOLDBLUE << "Configuring fast command block in CIC to lock on falling edge." << RESET;
    else
        LOG(INFO) << BOLDBLUE << "Configuring fast command block in CIC to lock on rising edge." << RESET;
    cRegName        = (pChip->getFrontEndType() == FrontEndType::CIC) ? "FC_ON_NEG_EDGE" : "MISC_CTRL";
    cRegValue       = this->ReadChipReg(pChip, cRegName);
    uint16_t cValue = (pChip->getFrontEndType() == FrontEndType::CIC) ? cNegEdge : (cRegValue & 0x17) | (cNegEdge << 3);
    cSuccess        = this->WriteChipReg(pChip, cRegName, cValue);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " select FC edge in CIC  " << RESET;
        exit(0);
    }

    // check fast command lock
    cSuccess = this->CheckFastCommandLock(pChip);
    if(!cSuccess)
    {
        LOG(INFO) << BOLDBLUE << "Could " << BOLDRED << " NOT " << BOLDBLUE << " lock FC decoder in CIC  " << RESET;
        exit(0);
    }
    LOG(INFO) << BOLDGREEN << "SUCCESSFULLY " << BOLDBLUE << " configured fast command block in CIC." << RESET;

    cSuccess = this->CheckReSync(pChip);
    LOG(INFO) << BOLDGREEN << ".... Completed CIC start-up ........ " << RESET;
    return cSuccess;
}
} // namespace Ph2_HwInterface
