#include "tools/OTBitErrorRateTest.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"
#include "HWInterface/D19cFWInterface.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

std::string OTBitErrorRateTest::fCalibrationDescription = "Insert brief calibration description here";

OTBitErrorRateTest::OTBitErrorRateTest() : Tool() {}

OTBitErrorRateTest::~OTBitErrorRateTest() {}

void OTBitErrorRateTest::Initialise(void)
{
    fRegisterHelper->takeSnapshot();
    // free the registers in case any

#ifdef __USE_ROOT__ 
    // Calibration is not running on the SoC: plots are booked during initialization
    fDQMHistogramOTBitErrorRateTest.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void OTBitErrorRateTest::ConfigureCalibration()
{

}

void OTBitErrorRateTest::Running()
{
    LOG(INFO) << "Starting OTBitErrorRateTest measurement.";
    Initialise();
    bitErrorRateTest();
    LOG(INFO) << "Done with OTBitErrorRateTest.";
    Reset();
}

void OTBitErrorRateTest::Stop(void)
{
    LOG(INFO) << "Stopping OTBitErrorRateTest measurement.";
    #ifdef __USE_ROOT__
        // Calibration is not running on the SoC: processing the histograms
        fDQMHistogramOTBitErrorRateTest.process();
    #endif
    SaveResults();
    closeFileHandler();
    LOG(INFO) << "OTBitErrorRateTest stopped.";
}

void OTBitErrorRateTest::Pause()
{

}


void OTBitErrorRateTest::Resume()
{

}


void OTBitErrorRateTest::Reset()
{
    fRegisterHelper->restoreSnapshot();
}

void OTBitErrorRateTest::bitErrorRateTest()
{
    LOG(DEBUG)<<"BIT ERROR RATE TEST"<<std::endl;
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {

            // if(true) /// DEBUG print bitslip
            // {
            //     for(std::string regName : {"link5_hybrid0_stub_bitslip","link5_hybrid1_stub_bitslip","link5_hybrid0_L1A_bitslip","link5_hybrid1_L1A_bitslip"} )
            //     {
            //         std::string fullName = "fc7_daq_ctrl.physical_interface_block." + regName;

            //         uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, fullName);
            //         std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "]"<<"BIT SLIP "<< regName << " reply 0x" << std::hex << readValue << std::dec << std::endl;

            //     }
            // }

            // Alignment
            std::string controlPhaseRegisterName = "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl";
            std::string controlBertRegisterName  = "fc7_daq_ctrl.physical_interface_block.bert_control";
            std::string statusPhaseRegisterName  = "fc7_daq_stat.physical_interface_block.phase_tuning_reply";
            std::string statusBertRegisterName   = "fc7_daq_stat.physical_interface_block.bert_stat";
            
            fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xFFF50008); // reset of phase tuning FSM
            usleep(100);
            fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, 0xFFF20039); // Rx enable and select error counter
            usleep(100);
            fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, 0xFFF30080); // set threshold
            usleep(100);
            fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xFFF20300); // configure for PRBS and sync enable on phase tuning FSM
            usleep(100);

            for(uint8_t line=0; line<7; ++line)
            {
                uint32_t command = (0x40060000 | (line << 20));
                fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, command);
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command = 0x" << std::hex << command << std::dec << std::endl;
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reply   = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusBertRegisterName) << std::dec << std::endl;
            }

            for(uint8_t line=0; line<7; ++line)
            {
                uint32_t value = (0x40010000 | (line << 20));
                fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, value);
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] writing = 0x" << std::hex << value << std::dec << std::endl;
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reading = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusBertRegisterName) << std::dec << std::endl;
            }

            bool allAligned = prepareLpGBTforBERT(theOpticalGroup);
            if(!allAligned)
            {
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]" << " not aligned, aborting" << std::endl;
                abort();
            }

            for(uint8_t line=0; line<7; ++line)
            {
                uint32_t command = (0x40060000 | (line << 20));
                fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, command);
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command = 0x" << std::hex << command << std::dec << std::endl;
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reply    = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusBertRegisterName) << std::dec << std::endl;
            }

            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] sending 0xFFF50004" << std::endl;
            
            fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, 0xFFF50004);

            for(uint8_t line=0; line<7; ++line)
            {
                uint32_t command = (0x40070000 | (line << 20));
                fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, command);
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] command = 0x" << std::hex << command << std::dec << std::endl;
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] reply   = 0x" << std::hex << fBeBoardInterface->ReadBoardReg(theBoard, statusBertRegisterName) << std::dec << std::endl;
            }

            auto theFWInterface = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(theBoard));
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.hybrid_select", 8);
            fBeBoardInterface->WriteBoardReg(fDetectorContainer->getObject(theOpticalGroup->getBeBoardId()), "fc7_daq_cnfg.physical_interface_block.slvs_debug.chip_select", 0);
            auto lineOutputVector        = theFWInterface->StubDebug(true, 6, false);
            for(auto line : lineOutputVector) LOG(INFO) << BOLDRED << "Stub data received    " << getPatternPrintout(line, 1, true) << RESET;

            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] sending 0xFFF20002" << std::endl;

            fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, 0xFFF20002);

            usleep(1000000);

            for(uint8_t line=0; line<7; ++line)
            {
                fBeBoardInterface->WriteBoardReg(theBoard, controlPhaseRegisterName, (0x40010000 | (line << 20)));
                uint32_t theReply = fBeBoardInterface->ReadBoardReg(theBoard, statusPhaseRegisterName);
                std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] line " << +line << " reply = 0x" << std::hex << theReply << std::dec << std::endl;
            }

            //
            // Read status register 1, DEBUG only link 5
            LOG(INFO)<<BOLDBLUE<<" STATUS REGISTER LINK 5"<<RESET;
            for(std::string regName : {"link5_hybrid0_stub_bitslip","link5_hybrid1_stub_bitslip","link5_hybrid0_L1A_bitslip","link5_hybrid1_L1A_bitslip"} )
            {
                    std::string fullName = "fc7_daq_ctrl.physical_interface_block." + regName;

                    uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, fullName);
                    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "]"<<"BIT SLIP "<< regName << " reply 0x" << std::hex << readValue << std::dec << std::endl;

            }

            usleep(10000);


            //usleep(10000);
            for(size_t line=0; line<7; ++line)
                {
                    uint32_t readRegisterValue = (line << 20) | 0x40040000; // 
                    fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, readRegisterValue);

                    uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.bert_stat");
                    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "]"<<"BERT_ERRORCOUNTER hybrid "<<0<<" line " << line << " reply 0x" << std::hex << readValue << std::dec << std::endl;
                }

            for(size_t line=0; line<7; ++line)
                {
                    uint32_t readRegisterValue = (line << 20) | 0x48040000; // 
                    fBeBoardInterface->WriteBoardReg(theBoard, controlBertRegisterName, readRegisterValue);

                    uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, "fc7_daq_stat.physical_interface_block.bert_stat");
                    std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "]"<<"BERT_ERRORCOUNTER hybrid "<<1<<" line " << line << " reply 0x" << std::hex << readValue << std::dec << std::endl;
                }


            usleep(10000);

        }
    }
}

bool OTBitErrorRateTest::prepareLpGBTforBERT(Ph2_HwDescription::OpticalGroup* theOpticalGroup)
{
    bool is10G = static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10;
    auto theLpGBT = theOpticalGroup->flpGBT;

    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource1", 0x09);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource2", 0x09);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource3", 0x09);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource4", 0x01);

    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource1", 0x24);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource2", 0x24);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource3", 0x24);
    // flpGBTInterface->WriteChipReg(theLpGBT, "ULDataSource4", 0x04);

    // flpGBTInterface->WriteChipReg(theLpGBT, "DPDataPattern0", 0xEA);
    // flpGBTInterface->WriteChipReg(theLpGBT, "DPDataPattern1", 0xEA);
    // flpGBTInterface->WriteChipReg(theLpGBT, "DPDataPattern2", 0xEA);
    // flpGBTInterface->WriteChipReg(theLpGBT, "DPDataPattern3", 0xEA);

    
    uint16_t value=0; std::string reg = "";
    std::cout<<std::hex; // print hex values
    reg = "EPRX0Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX0CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX0CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX1Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX1CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX1CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX2Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX2CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX2CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX3Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX3CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX3CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX4Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX4CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX4CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX5Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX5CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX5CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

    reg = "EPRX6Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX6CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
    reg = "EPRX6CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        std::cout<<std::dec;

    // std::string lpGBTRegisterName = "EPRXPRBS0";
    // uint16_t theRegisterName = 0;
    // flpGBTInterface->WriteChipReg(theLpGBT, lpGBTRegisterName, theRegisterName);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX0Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX1Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX2Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX3Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX4Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX5Control", 0x5a);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRX6Control", 0x5a);

    uint16_t phase = 0x2a;
    uint8_t  driverStrenght = 3;
    uint8_t PS0delayValue = phase & 0xff;
    uint8_t PS0configValue = ((phase >> 1) & 0x80) | (is10G ? 5 : 4) | (driverStrenght << 3);
    flpGBTInterface->WriteChipReg(theLpGBT, "PS0Config", PS0configValue);
    flpGBTInterface->WriteChipReg(theLpGBT, "PS0Delay", PS0delayValue);

    // Default values
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS3", 0x15);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS2", 0x55);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS1", 0x55);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS0", 0x55);

    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0x55);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0x55);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0x55);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x05);

    usleep(100);

    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x00);


    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS3", 0x1a);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS2", 0xaa);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS1", 0xaa);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXPRBS0", 0xaa);
        
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0xaa);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0xaa);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0xaa);
    //flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x0a);

    bool allAligned = false;
    size_t attempt = 0;
    
    while(!allAligned && attempt < 100)
    {
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX0Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX0Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX1Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX1Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX2Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX2Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX3Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX3Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX4Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX4Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX5Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX5Locked") << std::dec << std::endl;
        std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] EPRX6Locked = 0x" << std::hex << flpGBTInterface->ReadChipReg(theLpGBT, "EPRX6Locked") << std::dec << std::endl;
    
        usleep(100);
        if(  flpGBTInterface->ReadChipReg(theLpGBT, "EPRX0Locked") == 0xf2 &&   // we observe F2. Originally we put 50
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX1Locked") == 0xf2 &&  // we observe a2
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX2Locked") == 0xf2 && 
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX3Locked") == 0xf2 && 
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX4Locked") == 0xf2 && 
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX5Locked") == 0xf2 && 
                flpGBTInterface->ReadChipReg(theLpGBT, "EPRX6Locked") == 0xf2)
        {
            allAligned = true;
            break;
        }
        std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] attempt " << attempt << std::endl;
        ++attempt;
    }
    
    // debug: print status information regardless of exit status: the EPRX0Locked, EPRX0CurrentPhase10, EPRX0CurrentPhase32
    if (true){
        uint16_t value=0; std::string reg = "";
        // Locked Registers

        std::cout<<std::hex; // print hex values
        reg = "EPRX0Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX0CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX0CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        reg = "EPRX1Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX1CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX1CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        
        reg = "EPRX2Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX2CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX2CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        reg = "EPRX3Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX3CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX3CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        reg = "EPRX4Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX4CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX4CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        reg = "EPRX5Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX5CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX5CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        
        reg = "EPRX6Locked"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX6CurrentPhase10"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;
        reg = "EPRX6CurrentPhase32"; value = flpGBTInterface->ReadChipReg(theLpGBT, reg); std::cout<< __PRETTY_FUNCTION__ <<"["<<__LINE__<<"]: "<< reg <<" = " << value <<std::endl;

        std::cout<<std::dec;
    }

    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0x00);
    flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x00);



    return allAligned;
}
