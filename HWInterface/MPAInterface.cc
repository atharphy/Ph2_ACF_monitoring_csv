/*
        FileName :                     MPAInterface.cc
        Content :                      User Interface to the MPAs
        Programmer :                   K. nash, M. Haranko, D. Ceresa
        Version :                      1.0
        Date of creation :             5/01/18
 */

#include "MPAInterface.h"
#include "../Utils/ConsoleColor.h"
#include <typeinfo>

#define DEV_FLAG 0

using namespace Ph2_HwDescription;

namespace Ph2_HwInterface
{
MPAInterface::MPAInterface(const BeBoardFWMap& pBoardMap) : ReadoutChipInterface(pBoardMap) {
}
MPAInterface::~MPAInterface() {}
void MPAInterface::LinkLpGBT(D19clpGBTInterface* pLpGBTInterface, lpGBT* pLpGBT)
{
    flpGBTInterface = pLpGBTInterface;
    flpGBT          = pLpGBT;
}


uint16_t MPAInterface::ReadChipReg(Chip* pMPA, const std::string& pRegNode)
{
    setBoard(pMPA->getBeBoardId());
    std::vector<uint32_t> cVecReq;
    ChipRegItem           cRegItem;
    if(pRegNode.find("CounterStrip") != std::string::npos)
    {
        int cChannel = 0;
        sscanf(pRegNode.c_str(), "CounterStrip%d", &cChannel);
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0901 + cChannel;
        cRegItem.fValue        = 0;
        uint8_t cRPLSB         = this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
        cRegItem.fPage         = 0x00;
        cRegItem.fAddress      = 0x0801 + cChannel;
        uint8_t  cRPMSB        = this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
        uint16_t cCounterValue = (cRPMSB << 8) | cRPLSB;
        LOG(DEBUG) << BOLDBLUE << "Counter MSB is 0x" << std::bitset<8>(cRPMSB) << " Counter LSB is 0x" << std::bitset<8>(cRPLSB) << " Counter value is " << std::hex << +cCounterValue << std::dec
                   << RESET;
        return cCounterValue;
    }
    else if(pRegNode == "ErrorL1") 
    {
        return this->readPeri(pMPA, pRegNode);
    }
    else if(pRegNode == "Threshold")
    {
        return this->ReadChipReg(pMPA, "ThDAC0");
    }
    else
    {
        cRegItem = pMPA->getRegItem(pRegNode);
        return this->ReadReg(pMPA, cRegItem.fAddress) & 0xFF;
    }
}
uint16_t MPAInterface::ReadReg(Chip* pChip, uint16_t pRegisterAddress, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    ChipRegItem cRegItem;
    cRegItem.fPage    = 0x00;
    cRegItem.fAddress = pRegisterAddress;
    cRegItem.fValue   = 0;
    if(flpGBTInterface == nullptr)
    {
        bool                  cFailed = false;
        bool                  cRead;
        std::vector<uint32_t> cVecReq;
        fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVecReq, true, false);
        fBoardFW->ReadChipBlockReg(cVecReq);
        uint8_t cSSAId;
        fBoardFW->DecodeReg(cRegItem, cSSAId, cVecReq[0], cRead, cFailed);
    }
    else
    {
        // FIXME the FeId is hard coded for now, need to get the FeId info here
        cRegItem.fValue = flpGBTInterface->mpaRead(flpGBT, pChip->getHybridId(), pChip->getId(), pRegisterAddress);
    }
    return cRegItem.fValue & 0xFF;
}


bool MPAInterface::configPixel(Chip* pChip, std::string cReg, int pPixelNum , uint8_t pValue, bool pVerifLoop)
{
    auto cRowCol = static_cast<MPA*>(pChip)->PNlocal(pPixelNum);
    int cPixNum = pPixelNum - 1; 
    uint32_t cRow = (pPixelNum == 0 ) ? 0 : 1 + cPixNum/120 ;
    uint32_t cColumn = (pPixelNum == 0 ) ? 0 : 1 + cPixNum%120 ;
    uint8_t cRegAddress = PIXEL_CONFIG_TABLE.find(cReg)->second;
    uint16_t cAddress = this->regPixel( pChip , cRegAddress , cRow, cColumn ); 
    LOG (INFO) << BOLDBLUE << "PXL#" << +pPixelNum << " register is row " << +cRow << " column " << +cColumn 
            << " [built-in MPA row " << +cRowCol.first << " col " << +cRowCol.second 
            << " ] register 0x" << std::hex << cAddress << std::dec 
            << " value to write is 0x" << std::hex << +pValue << std::dec
            << RESET;

    // if global register don't readback 
    if (cRow == 0 || cColumn == 0 ) LOG (DEBUG) << BOLDMAGENTA << "GLOBAL PXL REG" << RESET;
    pVerifLoop = (cRow == 0 || cColumn == 0 ) ? false : pVerifLoop; 
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
}
uint16_t MPAInterface::readPixel(Chip* pChip, std::string cReg, int pPixelNum)
{
    int cPixNum = pPixelNum - 1; 
    uint32_t cRow = (pPixelNum == 0 ) ? 0 : 1 + cPixNum/120 ;
    uint32_t cColumn = (pPixelNum == 0 ) ? 0 : 1 + cPixNum%120 ;
    uint8_t cRegAddress = PIXEL_CONFIG_TABLE.find(cReg)->second;
    uint16_t cAddress = this->regPixel( pChip , cRegAddress , cRow, cColumn ); 
    LOG (DEBUG) << BOLDBLUE << "PXL#" << +pPixelNum << " register is row " << +cRow << " column " << +cColumn 
            << " register 0x" << std::hex << cAddress << std::dec 
            << RESET;
    return MPAInterface::ReadReg(pChip, cAddress);
}
bool MPAInterface::maskPixel(Chip* pChip, int pPixelNum , uint8_t pMask, bool pVerifLoop) 
{
    // pixel num starts from 1 [0 == global]
    auto cRegValue = this->readPixel(pChip,"PixelEnable", pPixelNum);
    uint8_t cNewValue = (cRegValue&0xFE) | (1-pMask);
    return this->configPixel(pChip, "PixelEnable", pPixelNum, cNewValue, pVerifLoop );
}
bool MPAInterface::maskRowCol(Chip* pChip, int pRow , int pColumn, uint8_t pMask, bool pVerifLoop) 
{
    // row and col num starts from 1 [0 == global]
    int cPixNum = 1 + (pColumn-1)*120 + (pRow-1);//starting from 1 
    return this->maskPixel(pChip, cPixNum, pMask, pVerifLoop);
}
bool MPAInterface::configRow(Chip* pChip, std::string cReg, int pRow , uint8_t pValue, bool pVerifLoop)
{
    uint8_t cRegAddress = ROW_CONFIG_TABLE.find(cReg)->second;  
    // if global register don't readback 
    pVerifLoop = (pRow == 0 ) ? false : pVerifLoop; 
    uint16_t cAddress =  this->regRow( pChip , cRegAddress , pRow);
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
} 
bool MPAInterface::configPeri(Chip* pChip, std::string cReg, uint8_t pValue, bool pVerifLoop)
{
    LOG (INFO) << BOLDRED << PERI_CONFIG_TABLE.size() << " items in peri map." << RESET;
    for( auto cMapItem : PERI_CONFIG_TABLE )
        LOG (INFO) << cMapItem.first << " " << +cMapItem.second << RESET;
    uint8_t cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second;  
    uint16_t cAddress =  this->regPeri(pChip, cRegAddress ); 
    return MPAInterface::WriteReg(pChip, cAddress, pValue, pVerifLoop);
}
uint16_t MPAInterface::readPeri(Chip* pChip, std::string cReg)
{
    uint8_t cRegAddress = (PERI_CONFIG_TABLE.find(cReg))->second;  
    uint16_t cAddress =  this->regPeri(pChip, cRegAddress ); 
    LOG (INFO) << BOLDBLUE << "Reading peri register 0x" << std::hex << cAddress << std::dec 
            << RESET;
    return MPAInterface::ReadReg(pChip, cAddress);
}



uint16_t MPAInterface::regPixel(Chip* pChip, int pBaseRegister, int pRow , int pColumn) 
{
    uint16_t cRegAddress =  ((pRow << 11) | ( pBaseRegister << 7 ) | pColumn);
    return cRegAddress;
}
uint16_t MPAInterface::regPeri(Chip* pChip, int pBaseRegister ) 
{
    uint16_t cRegAddress =  ((0x11 << 11) | ( 0x0 << 8 ) | pBaseRegister);
    return cRegAddress;
}
uint16_t MPAInterface::regRow(Chip* pChip, int pBaseRegister, int pRow ) 
{
    uint16_t cRegAddress =  ((pRow << 11) | ( pBaseRegister << 7 ) | 0x79);
    return cRegAddress;
}


bool MPAInterface::WriteChipReg(Chip* pMPA, const std::string& pRegName, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());
    //need to or success
    if (pRegName=="ThDAC_ALL")
    {
        this->Set_threshold(pMPA, pValue);
        return true;
    }
    else if(pRegName == "DigitalPattern" )
    {
        bool cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x03);
        bool cConfigPattern = WriteChipSingleReg(pMPA, "LFSR_data", pValue);
        return cReadoutMode && cConfigPattern;
    }
    else if(pRegName == "DigitalSync" ) 
    {
        bool cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x00);
        bool cEnableDigital = true;
        // //bool cReadoutMode = configPeri(pMPA, "ReadoutMode", 0x00);
        // uint8_t cPixelMask=1; 
        // uint8_t cPolarity=1;
        // uint8_t cEnEdgeBR=1;
        // uint8_t cEnLvlBr=0;
        // uint8_t cEnCount=0;
        // uint8_t cDigCal=pValue; 
        // uint8_t cAnaCal=0; 
        // uint8_t cBrClk=0;
        // uint8_t cRegValue = (cEnEdgeBR << 2 ) | (cPolarity << 1 ) | cPixelMask; 
        // cRegValue = cRegValue | (  (cDigCal << 5 ) | (cEnCount << 4 ) | (cEnLvlBr << 3) ) ; 
        // cRegValue = cRegValue | (  (cBrClk << 7 ) | (cAnaCal << 6 ) ) ; 
        // if( pValue == 1 )
        //     LOG (INFO) << BOLDBLUE << "Enabling digital injection on MPA by setting register ENFLAGS_ALL to 0x" 
        //         << std::hex << +cRegValue << std::dec << RESET;
        // else
        //     LOG (INFO) << BOLDBLUE << "Disabling digital injection on MPA by setting register ENFLAGS_ALL to 0x" 
        //         << std::hex << +cRegValue << std::dec << RESET;
            
        // bool    cEnableDigital = WriteChipSingleReg(pMPA, "ENFLAGS_ALL", cRegValue, false);
        // LOG (INFO) << BOLDBLUE << "Enabling readout of L1 data on MPA by setting register ReadoutMode to 0x" 
        //         << std::hex << +(0) << std::dec << RESET;
        return cEnableDigital && cReadoutMode;
    }
    else if(pRegName == "AnalogueAsync")
    {
        //readout mode 1 -- ASYNC counter
        bool cReadoutMode       = configPeri(pMPA, "ReadoutMode", 0x01);

        uint8_t cPixelMask=1; 
        uint8_t cPolarity=1;
        uint8_t cEnEdgeBR=1;
        uint8_t cEnLvlBr=0;
        uint8_t cEnCount=pValue;
        uint8_t cDigCal=0; 
        uint8_t cAnaCal=pValue; 
        uint8_t cBrClk=0;
        uint8_t cRegValue = (cEnEdgeBR << 2 ) | (cPolarity << 1 ) | cPixelMask; 
        cRegValue = cRegValue | (  (cDigCal << 5 ) | (cEnCount << 4 ) | (cEnLvlBr << 3) ) ; 
        cRegValue = cRegValue | (  (cBrClk << 7 ) | (cAnaCal << 6 ) ) ; 
        if( pValue == 1 )
            LOG (INFO) << BOLDBLUE << "Enabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" 
                << std::hex << +cRegValue << std::dec << RESET;
        else
            LOG (INFO) << BOLDBLUE << "Disabling analogue injection on MPA by setting register ENFLAGS_ALL to 0x" 
                << std::hex << +cRegValue << std::dec << RESET;
        bool    cEnableAnalogue = this->configPixel(pMPA, "PixelEnable" , 0 , cRegValue, pVerifLoop);
        // mask pixel 1 
        // {
        //     this->maskPixel(pMPA,1,1, pVerifLoop); 
        // }
        if( pValue == 1 )
            LOG (INFO) << BOLDBLUE << "Enabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" 
                << std::hex << +pValue << std::dec << RESET;
        else
            LOG (INFO) << BOLDBLUE << "Disabling readout of I2C counters on MPA by setting register ReadoutMode to 0x" 
                << std::hex << +pValue << std::dec << RESET;
        return cEnableAnalogue && cReadoutMode;
    }

    else if(pRegName == "Threshold" or pRegName == "Bias_THDAC" )
    {
        LOG(INFO) << BOLDBLUE << "Setting "
                  << " bias thresh to " << +pValue << " on MPA" << +pMPA->getId() << RESET;
        Set_threshold(pMPA,  pValue);
        return true;
    }

    else if(pRegName == "InjectedCharge")
    {
        LOG(INFO) << BOLDBLUE << "Setting "
                  << " bias calDac to " << +pValue << " on MPA" << +pMPA->getId() << RESET;

        Set_calibration( pMPA, pValue);

        return true;
    }
    else
    {
        return this->WriteChipSingleReg(pMPA, pRegName, pValue, pVerifLoop);
    }

}



bool MPAInterface::WriteChipSingleReg(Chip* pChip, const std::string& pRegNode, uint16_t pValue, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    bool        cSuccess = true;
    ChipRegItem cRegItem = pChip->getRegItem(pRegNode);
    cRegItem.fValue      = pValue & 0xFF;
    if(flpGBTInterface == nullptr)
    {
        std::vector<uint32_t> cVec;
        fBoardFW->EncodeReg(cRegItem, pChip->getHybridId(), pChip->getId(), cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
        flpGBT->setBeBoardId(pChip->getBeBoardId());
        cSuccess = flpGBTInterface->mpaWrite(flpGBT, pChip->getHybridId(), pChip->getId(), cRegItem.fAddress, cRegItem.fValue, pVerifLoop);
    }
    if(cSuccess && flpGBTInterface == nullptr ) // check is done in lpGBTInterface for opto
    {
        pChip->setReg(pRegNode, pValue);
        if(pVerifLoop)
        {
            if(pRegNode != "ENFLAGS" && pRegNode != "DigCalibPattern_L" && pRegNode != "DigCalibPattern_H")
            {
                uint16_t cReadBack = ReadChipReg(pChip, pRegNode);
                if(cReadBack != pValue)
                {
                    LOG(INFO) << BOLDRED << "Read back value from " << pRegNode << BOLDBLUE << " at I2C address " << std::hex << cRegItem.fAddress << std::dec << " not equal to write value of "
                              << std::hex << +cRegItem.fValue << std::dec << RESET;
                    return false;
                }
            }
        }
    }
#ifdef COUNT_FLAG
    fRegisterCount++;
    fTransactionCount++;
#endif
    return cSuccess;
}

bool MPAInterface::WriteChipMultReg(Chip* pMPA, const std::vector<std::pair<std::string, uint16_t>>& pVecReq, bool pVerifLoop)
{
    // first, identify the correct BeBoardFWInterface
    setBoard(pMPA->getBeBoardId());

    std::vector<uint32_t> cVec;

    // Deal with the ChipRegItems and encode them
    ChipRegItem cRegItem;

    for(const auto& cReg: pVecReq)
    {
        if(cReg.second > 0xFF)
        {
            LOG(ERROR) << "MPA register are 8 bits, impossible to write " << cReg.second << " on registed " << cReg.first;
            continue;
        }

        // HACK! take out
        this->WriteChipReg(pMPA, cReg.first, cReg.second, pVerifLoop);

#ifdef COUNT_FLAG
        fRegisterCount++;
#endif
    }

    // write the registers, the answer will be in the same cVec
    // the number of times the write operation has been attempted is given by cWriteAttempts
    // uint8_t cWriteAttempts = 0 ;

    // HACK! put back in
    // bool cSuccess = fBoardFW->WriteChipBlockReg (  cVec, cWriteAttempts, pVerifLoop );
    bool cSuccess = true;

#ifdef COUNT_FLAG
    fTransactionCount++;
#endif

    // if the transaction is successfull, update the HWDescription object
    if(cSuccess)
    {
        for(const auto& cReg: pVecReq)
        {
            cRegItem = pMPA->getRegItem(cReg.first);
            pMPA->setReg(cReg.first, cReg.second);
        }
    }

    return cSuccess;
}

bool MPAInterface::WriteChipAllLocalReg(ReadoutChip* pMPA, const std::string& dacName, ChipContainer& localRegValues, bool pVerifLoop)
{
    setBoard(pMPA->getBeBoardId());
    assert(localRegValues.size() == pMPA->getNumberOfChannels());
    std::string dacTemplate;

    if(dacName == "TrimDAC_P" or dacName == "ThresholdTrim")
        {
        if (pMPA->getFrontEndType()== FrontEndType::MPA)  dacTemplate = "TrimDAC_P%d";
        else if (pMPA->getFrontEndType()== FrontEndType::SSA)  dacTemplate = "THTRIMMING_S%d";
        }

    else
        LOG(ERROR) << "Error, DAC " << dacName << " is not a Local DAC";

    std::vector<std::pair<std::string, uint16_t>> cRegVec;
    ChannelGroup<NMPACHANNELS, 1>                 channelToEnable;
    std::vector<uint32_t>                         cVec;
    cVec.clear();
    bool cSuccess = true;


    for(uint16_t iChannel = 0; iChannel < pMPA->getNumberOfChannels(); ++iChannel)
    {
        char dacName1[20];
        sprintf(dacName1, dacTemplate.c_str(), 1 + iChannel);
        LOG(DEBUG) << BOLDBLUE << "Setting register " << dacName1 << " to " << (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F) << RESET;
        cSuccess = cSuccess && this->WriteChipReg(pMPA, dacName1, (localRegValues.getChannel<uint16_t>(iChannel) & 0x1F), pVerifLoop);
    }
    return cSuccess;

}



bool MPAInterface::ConfigureChip(Chip* pMPA, bool pVerifLoop, uint32_t pBlockSize)
{
    LOG(INFO) << BOLDBLUE << "MPACONFIG" << RESET;
    setBoard(pMPA->getBeBoardId());
    std::vector<uint32_t> cVec;
    ChipRegMap            cMPARegMap = pMPA->getRegMap();
    // for some reason this makes block write work
    // otherwise need to configure one by one which
    // takes forever
    std::map<uint16_t, ChipRegItem> cMap;
    cMap.clear();
    for(auto& cRegInMap: cMPARegMap) { 
//SPEEDUP, TEMPORARY
    if((cRegInMap.first.find("_P") != std::string::npos)  and (cRegInMap.first.find("TrimDAC") == std::string::npos) ) continue;

    //LOG(INFO) << BOLDBLUE << cRegInMap.first<< RESET; 
    cMap[cRegInMap.second.fAddress] = cRegInMap.second; }
    std::vector<std::pair<uint16_t, uint16_t>> cRegs;
    for(auto& cRegItem: cMap)
    {
        LOG(DEBUG) << BOLDBLUE << "Register map for MPA contains a register with address " << std::hex << +cRegItem.second.fAddress << std::dec << RESET;
        std::pair<uint16_t, uint16_t> cReg;
        cReg.first  = cRegItem.second.fAddress;
        cReg.second = cRegItem.second.fValue;
        cRegs.push_back(cReg);
    } // loop over map
    return this->WriteRegs(pMPA, cRegs, pVerifLoop);
}



bool MPAInterface::WriteRegs(Chip* pChip, const std::vector<std::pair<uint16_t, uint16_t>> pRegs, bool pVerifLoop)
{
    LOG (INFO) << BOLDRED << "Be#" << +pChip->getBeBoardId() << RESET;
    setBoard(pChip->getBeBoardId());
    bool cSuccess = true;
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
            fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVec, pVerifLoop, true);
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
        int cCount=0;
        flpGBT->setBeBoardId(pChip->getBeBoardId());
        for(const auto& cReg: pRegs)
        {
            if( cCount%100 == 0 ) LOG(DEBUG) << BOLDBLUE << "Writing MPA register with address 0x" << std::hex << +cReg.first << std::dec << RESET;
            cSuccess = flpGBTInterface->mpaWrite(flpGBT, pChip->getHybridId(), pChip->getId(), cReg.first, cReg.second, pVerifLoop);
            if(!cSuccess) continue;
            #ifdef COUNT_FLAG
                fRegisterCount++;
            #endif
            cCount++;
        }
    }
    return cSuccess;
}

bool MPAInterface::WriteReg(Chip* pChip, uint16_t pRegisterAddress, uint16_t pRegisterValue, bool pVerifLoop)
{
    bool cSuccess = false;
    setBoard(pChip->getBeBoardId());
    // write
    if(flpGBTInterface == nullptr)
    {
        std::vector<uint32_t> cVec;
        ChipRegItem           cRegItem;
        cRegItem.fPage    = 0x00;
        cRegItem.fAddress = pRegisterAddress;
        cRegItem.fValue   = pRegisterValue & 0xFF;
        fBoardFW->EncodeReg(cRegItem, pChip->getId(), pChip->getId(), cVec, pVerifLoop, true);
        uint8_t cWriteAttempts = 0;
        cSuccess               = fBoardFW->WriteChipBlockReg(cVec, cWriteAttempts, pVerifLoop);
    }
    else
    {
       flpGBT->setBeBoardId(pChip->getBeBoardId());
       LOG (INFO) << BOLDBLUE << "Writing MPA register 0x" 
        << std::hex << +pRegisterAddress << std::dec 
        << " on back-end board " << +flpGBT->getBeBoardId() 
        << " MPA#" << +pChip->getId() 
        << " on FE#" << +pChip->getHybridId() 
        << " register value is " << +pRegisterValue 
        << RESET;
       cSuccess = flpGBTInterface->mpaWrite(flpGBT, pChip->getHybridId(), pChip->getId(), pRegisterAddress, pRegisterValue, pVerifLoop);
    }
    return cSuccess;
}


void MPAInterface::setFileHandler(FileHandler* pHandler)
{
    setBoard(0);
    fBoardFW->setFileHandler(pHandler);
}

// These are not currently used but can encode pix registers
void MPAInterface::Pix_write(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel, uint32_t data)
{
    uint8_t cWriteAttempts = 0;

    ChipRegItem rowreg = cRegItem;
    rowreg.fAddress    = ((row & 0x0001f) << 11) | ((cRegItem.fAddress & 0x000f) << 7) | (pixel & 0xfffffff);
    rowreg.fValue      = data;
    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    fBoardFW->EncodeReg(rowreg, cMPA->getHybridId(), cMPA->getId(), cVecReq, false, true);
    fBoardFW->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
}

uint32_t MPAInterface::Pix_read(ReadoutChip* cMPA, ChipRegItem cRegItem, uint32_t row, uint32_t pixel)
{
    uint8_t  cWriteAttempts = 0;
    uint32_t rep;

    std::vector<uint32_t> cVecReq;
    cVecReq.clear();
    fBoardFW->EncodeReg(cRegItem, cMPA->getHybridId(), cMPA->getId(), cVecReq, false, false);
    fBoardFW->WriteChipBlockReg(cVecReq, cWriteAttempts, false);
    std::chrono::milliseconds cShort(1);

    rep = this->ReadChipReg(cMPA, "fc7_daq_ctrl.command_processor_block.i2c.mpa_MPA_i2c_reply.data");

    return rep;
}

Stubs MPAInterface::Format_stubs(std::vector<std::vector<uint8_t>> rawstubs)
{
    int   j     = 0;
    int   cycle = 0;
    Stubs formstubs;
    for(int i = 0; i < 39; i++)
    {
        if((rawstubs[0][i] & 0x80) == 128)
        {
            j = i + 1;
            formstubs.pos.push_back(std::vector<uint8_t>(5, 0));
            formstubs.row.push_back(std::vector<uint8_t>(5, 0));
            formstubs.cur.push_back(std::vector<uint8_t>(5, 0));

            formstubs.nst.push_back(((rawstubs[1][i] & 0x80) >> 5) | ((rawstubs[2][i] & 0x80) >> 6) | ((rawstubs[3][i] & 0x80) >> 7));
            formstubs.pos[cycle][0] = ((rawstubs[4][i] & 0x80) << 0) | ((rawstubs[0][i] & 0x40) << 0) | ((rawstubs[1][i] & 0x40) >> 1) | ((rawstubs[2][i] & 0x40) >> 2) |
                                      ((rawstubs[3][i] & 0x40) >> 3) | ((rawstubs[4][i] & 0x40) >> 4) | ((rawstubs[0][i] & 0x20) >> 4) | ((rawstubs[1][i] & 0x20) >> 5);
            formstubs.pos[cycle][1] = ((rawstubs[4][i] & 0x10) << 3) | ((rawstubs[0][i] & 0x8) << 3) | ((rawstubs[1][i] & 0x8) << 2) | ((rawstubs[2][i] & 0x8) << 1) | ((rawstubs[3][i] & 0x8) << 0) |
                                      ((rawstubs[4][i] & 0x8) >> 1) | ((rawstubs[0][i] & 0x4) >> 1) | ((rawstubs[1][i] & 0x4) >> 2);
            formstubs.pos[cycle][2] = ((rawstubs[4][i] & 0x2) << 6) | ((rawstubs[0][i] & 0x1) << 6) | ((rawstubs[1][i] & 0x1) << 5) | ((rawstubs[2][i] & 0x1) << 4) | ((rawstubs[3][i] & 0x1) << 3) |
                                      ((rawstubs[4][i] & 0x1) << 3) | ((rawstubs[1][j] & 0x80) >> 6) | ((rawstubs[2][j] & 0x80) >> 7);
            formstubs.pos[cycle][3] = ((rawstubs[0][j] & 0x20) << 2) | ((rawstubs[1][j] & 0x20) << 1) | ((rawstubs[2][j] & 0x20) << 0) | ((rawstubs[3][j] & 0x20) >> 1) |
                                      ((rawstubs[4][j] & 0x20) >> 2) | ((rawstubs[0][j] & 0x10) >> 2) | ((rawstubs[1][j] & 0x10) >> 3) | ((rawstubs[2][j] & 0x10) >> 4);
            formstubs.pos[cycle][4] = ((rawstubs[0][j] & 0x4) << 5) | ((rawstubs[1][j] & 0x4) << 4) | ((rawstubs[2][j] & 0x4) << 3) | ((rawstubs[3][j] & 0x4) << 2) | ((rawstubs[4][j] & 0x4) << 1) |
                                      ((rawstubs[0][j] & 0x2) << 1) | ((rawstubs[1][j] & 0x2) << 0) | ((rawstubs[2][j] & 0x2) >> 1);
            formstubs.row[cycle][0] = ((rawstubs[0][i] & 0x10) >> 1) | ((rawstubs[1][i] & 0x10) >> 2) | ((rawstubs[2][i] & 0x10) >> 3) | ((rawstubs[3][i] & 0x10) >> 4);
            formstubs.row[cycle][1] = ((rawstubs[0][i] & 0x2) << 2) | ((rawstubs[1][i] & 0x2) << 1) | ((rawstubs[2][i] & 0x2) << 0) | ((rawstubs[3][i] & 0x2) >> 1);
            formstubs.row[cycle][2] = ((rawstubs[1][j] & 0x40) >> 3) | ((rawstubs[2][j] & 0x40) >> 4) | ((rawstubs[3][j] & 0x40) >> 5) | ((rawstubs[4][j] & 0x40) >> 6);
            formstubs.row[cycle][3] = ((rawstubs[1][j] & 0x8) >> 0) | ((rawstubs[2][j] & 0x8) >> 1) | ((rawstubs[3][j] & 0x8) >> 2) | ((rawstubs[4][j] & 0x8) >> 3);
            formstubs.row[cycle][4] = ((rawstubs[1][j] & 0x1) << 3) | ((rawstubs[2][j] & 0x1) << 2) | ((rawstubs[3][j] & 0x1) << 1) | ((rawstubs[4][j] & 0x1) << 0);
            formstubs.cur[cycle][0] = ((rawstubs[2][i] & 0x20) >> 3) | ((rawstubs[3][i] & 0x20) >> 4) | ((rawstubs[4][i] & 0x20) >> 5);
            formstubs.cur[cycle][1] = ((rawstubs[2][i] & 0x4) >> 0) | ((rawstubs[3][i] & 0x4) >> 1) | ((rawstubs[4][i] & 0x4) >> 2);
            formstubs.cur[cycle][2] = ((rawstubs[3][j] & 0x80) >> 5) | ((rawstubs[4][j] & 0x80) >> 6) | ((rawstubs[0][j] & 0x40) >> 6);
            formstubs.cur[cycle][3] = ((rawstubs[3][j] & 0x10) >> 2) | ((rawstubs[4][j] & 0x10) >> 3) | ((rawstubs[0][j] & 0x8) >> 3);
            formstubs.cur[cycle][4] = ((rawstubs[3][j] & 0x2) << 1) | ((rawstubs[4][j] & 0x2) >> 0) | ((rawstubs[0][j] & 0x1) >> 0);
            // std::cout<<"RS1 "<<+formstubs.pos[cycle][0]<<std::endl;
            // std::cout<<"RS2 "<<+formstubs.pos[cycle][1]<<std::endl;
            // std::cout<<"RS3 "<<+formstubs.pos[cycle][2]<<std::endl;
            // std::cout<<"RS01"<<+rawstubs[1][i]<<std::endl; std::cout<<"RS4 "<<+formstubs.pos[cycle][3]<<std::endl;
            cycle += 1;
        }
    }
    return formstubs;
}

L1data MPAInterface::Format_l1(std::vector<uint8_t> rawl1, bool verbose)
{
    bool    found = false;
    uint8_t header, error(0), L1_ID, strip_counter, pixel_counter;
    L1data  formL1data;

    std::vector<uint16_t> strip_data, pixel_data;
    uint16_t              curdata;

    for(int i = 1; i < 200; i++)
    {
        if((rawl1[i] == 255) & (rawl1[i - 1] == 255) & (!found))
        {
            header        = rawl1[i - 1] << 11 | rawl1[i - 1] << 3 | ((rawl1[i + 1] & 0xE0) >> 5);
            error         = ((rawl1[i + 1] & 0x18) >> 3);
            L1_ID         = ((rawl1[i + 1] & 0x7) << 6) | ((rawl1[i + 2] & 0xFC) >> 2);
            strip_counter = ((rawl1[i + 2] & 0x1) << 4) | ((rawl1[i + 3] & 0xF0) >> 4);
            pixel_counter = ((rawl1[i + 3] & 0xF) << 1) | ((rawl1[i + 4] & 0x80) >> 7);

            uint8_t wordl = 11, counter = 0;
            bool    curbit;
            uint8_t bitmask = 0x80;
            for(int j = 4; j < 50; j++)
            {
                for(int k = 0; k < 8; k++)
                {
                    curbit = (rawl1[i + j] & (bitmask >> k));
                    counter += 1;
                    curdata += (curbit << (wordl - counter));
                    if(counter == wordl)
                    {
                        if(wordl == 11)
                            strip_data.push_back(curdata);
                        else
                            pixel_data.push_back(curdata);
                        if(strip_counter == strip_data.size()) wordl = 14;
                        curdata = 0;
                        counter = 0;
                    }
                }
            }
            found = true;
        }
    }
    if(found)
    {
        formL1data.strip_counter = strip_counter;
        formL1data.pixel_counter = pixel_counter;
        if(verbose)
        {
            std::cout << "Header: " << std::bitset<8>(header) << std::endl;
            std::cout << "Error: " << std::bitset<8>(error) << std::endl;
            std::cout << "L1 ID: " << L1_ID << std::endl;
            std::cout << "Strip counter: " << strip_counter << std::endl;
            std::cout << "Pixel counter: " << pixel_counter << std::endl;
            std::cout << "Strip data:" << std::endl;
        }

        for(auto& sdata: strip_data)
        {
            formL1data.pos_strip.push_back((sdata & 0x7F0) >> 4);
            formL1data.width_strip.push_back((sdata & 0xE) >> 1);
            formL1data.MIP.push_back((sdata & 0x1));

            if(verbose) std::cout << "\tPosition: " << formL1data.pos_strip.back() << "\n\tWidth: " << formL1data.width_strip.back() << "\n\tMIP: " << formL1data.MIP.back() << std::endl;
        }
        if(verbose) std::cout << "Pixel data:" << std::endl;

        for(auto& pdata: pixel_data)
        {
            formL1data.pos_pixel.push_back((pdata & 0x3F80) >> 7);
            formL1data.width_pixel.push_back((pdata & 0x70) >> 4);
            formL1data.Z.push_back((pdata & 0xF) + 1);

            if(verbose) std::cout << "\tPosition: " << formL1data.pos_pixel.back() << "\n\tWidth: " << formL1data.width_pixel.back() << "\n\tRow Number: " << formL1data.Z.back() << std::endl;
        }

        return formL1data;
    }
    else
        std::cout << "Header not found!" << std::endl;

    return formL1data;
}

void MPAInterface::Activate_async(Chip* pMPA) { this->WriteChipReg(pMPA, "ReadoutMode", 0x1); }

void MPAInterface::Activate_sync(Chip* pMPA) { this->WriteChipReg(pMPA, "ReadoutMode", 0x0); }

void MPAInterface::Activate_pp(Chip* pMPA) { this->WriteChipReg(pMPA, "ECM", 0x81); }

void MPAInterface::Activate_ss(Chip* pMPA) { this->WriteChipReg(pMPA, "ECM", 0x41); }

void MPAInterface::Activate_ps(Chip* pMPA, uint8_t win) { this->WriteChipReg(pMPA, "ECM", win); }

void MPAInterface::Pix_Smode(ReadoutChip* pMPA, uint32_t p, std::string smode = "edge")
{
    uint32_t smodewrite = 0x0;
    if(smode == "edge") smodewrite = 0x0;
    if(smode == "level") smodewrite = 0x1;
    if(smode == "or") smodewrite = 0x2;
    if(smode == "xor") smodewrite = 0x3;
    this->WriteChipReg(pMPA, "ModeSel_P" + std::to_string(p + 1), smodewrite);
}

void MPAInterface::Enable_pix_BRcal(ReadoutChip* pMPA, uint32_t p, std::string polarity, std::string smode)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 1, EnLevelBR = 0, Encount = 0, DigCal = 0, AnCal = 0, BRclk = 0;

    if(polarity == "rise")
        Polarity = 1;
    else if(polarity == "fall")
        Polarity = 0;
    else
    {
        std::cout << "bad pol option" << std::endl;
        return;
    }
    if(smode == "level")
    {
        Pix_Smode(pMPA, p, "level");
        EnEdgeBR = 0, EnLevelBR = 1, Encount = 1, AnCal = 1;
    }
    else if(smode == "edge")
    {
        Pix_Smode(pMPA, p, "edge");
        EnEdgeBR = 1, EnLevelBR = 0, Encount = 1, AnCal = 1;
    }
    else
    {
        std::cout << "bad edge option" << std::endl;
        return;
    }
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_counter(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 0, EnLevelBR = 0, Encount = 1, DigCal = 0, AnCal = 1, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_sync(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 1, Polarity = 1, EnEdgeBR = 0, EnLevelBR = 0, Encount = 1, DigCal = 0, AnCal = 1, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Disable_pixel(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 0, Polarity = 0, EnEdgeBR = 0, EnLevelBR = 0, Encount = 0, DigCal = 0, AnCal = 0, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Enable_pix_digi(ReadoutChip* pMPA, uint32_t p)
{
    uint32_t PixelMask = 0, Polarity = 0, EnEdgeBR = 0, EnLevelBR = 0, Encount = 0, DigCal = 1, AnCal = 0, BRclk = 0;
    Pix_Set_enable(pMPA, p, PixelMask, Polarity, EnEdgeBR, EnLevelBR, Encount, DigCal, AnCal, BRclk);
}

void MPAInterface::Pix_Set_enable(ReadoutChip* pMPA,
                                  uint32_t     p,
                                  uint32_t     PixelMask = 1,
                                  uint32_t     Polarity  = 1,
                                  uint32_t     EnEdgeBR  = 1,
                                  uint32_t     EnLevelBR = 0,
                                  uint32_t     Encount   = 0,
                                  uint32_t     DigCal    = 0,
                                  uint32_t     AnCal     = 0,
                                  uint32_t     BRclk     = 0)
{
    uint32_t comboword = (PixelMask) + (Polarity << 1) + (EnEdgeBR << 2) + (EnLevelBR << 3) + (Encount << 4) + (DigCal << 5) + (AnCal << 6) + (BRclk << 7);
    this->WriteChipReg(pMPA, "ENFLAGS_P" + std::to_string(p + 1), comboword);
}

void MPAInterface::Set_calibration(Chip* pMPA, uint32_t cal)
{
        this->WriteChipReg(pMPA, "CalDAC0", cal);
        this->WriteChipReg(pMPA, "CalDAC1", cal);
        this->WriteChipReg(pMPA, "CalDAC2", cal);
        this->WriteChipReg(pMPA, "CalDAC3", cal);
        this->WriteChipReg(pMPA, "CalDAC4", cal);
        this->WriteChipReg(pMPA, "CalDAC5", cal);
        this->WriteChipReg(pMPA, "CalDAC6", cal);
}

void MPAInterface::Set_threshold(Chip* pMPA, uint32_t th)
{
    setBoard(pMPA->getBeBoardId());

    this->WriteChipReg(pMPA, "ThDAC0", th);
    this->WriteChipReg(pMPA, "ThDAC1", th);
    this->WriteChipReg(pMPA, "ThDAC2", th);
    this->WriteChipReg(pMPA, "ThDAC3", th);
    this->WriteChipReg(pMPA, "ThDAC4", th);
    this->WriteChipReg(pMPA, "ThDAC5", th);
    this->WriteChipReg(pMPA, "ThDAC6", th);

}

void MPAInterface::ReadASEvent(ReadoutChip* pMPA, std::vector<uint32_t>& pData, std::pair<uint32_t, uint32_t> pSRange)
{
    if(pSRange == std::pair<uint32_t, uint32_t>{0, 0}) pSRange = std::pair<uint32_t, uint32_t>{1, pMPA->getNumberOfChannels()};
    for(uint32_t i = pSRange.first; i <= pSRange.second; i++)
    {
        uint8_t cRP1 = this->ReadChipReg(pMPA, "ReadCounter_LSB_P" + std::to_string(i));
        uint8_t cRP2 = this->ReadChipReg(pMPA, "ReadCounter_MSB_P" + std::to_string(i));

        pData.push_back((cRP2 * 256) + cRP1);
        // std::cout<<i<<" "<<(cRP2*256) + cRP1<<std::endl;
    }
}

bool MPAInterface::enableInjection(ReadoutChip* pChip, bool inject, bool pVerifLoop)
{
    setBoard(pChip->getBeBoardId());
    // if sync

    // uint32_t enwrite=1;
    // if(inject) enwrite=17;

    uint32_t enwrite = 0x17;
    if(inject) enwrite = 0x53;
    this->WriteChipReg(pChip, "ENFLAGS_ALL", enwrite);
    return true;
}

uint32_t MPAInterface::ReadData(BeBoard* pBoard, bool pBreakTrigger, std::vector<uint32_t>& pData, bool pWait)
{
    setBoard(0);
    return fBoardFW->ReadData(pBoard, pBreakTrigger, pData, pWait);
}

void MPAInterface::Cleardata()
{
    setBoard(0);
    // fBoardFW->Cleardata( );
}

} // namespace Ph2_HwInterface
