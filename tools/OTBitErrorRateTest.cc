#include "tools/OTBitErrorRateTest.h"
#include "HWInterface/D19cFWInterface.h"
#include "System/RegisterHelper.h"
#include "Utils/ContainerSerialization.h"

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

void OTBitErrorRateTest::ConfigureCalibration() {}

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

void OTBitErrorRateTest::Pause() {}

void OTBitErrorRateTest::Resume() {}

void OTBitErrorRateTest::Reset() { fRegisterHelper->restoreSnapshot(); }

void OTBitErrorRateTest::bitErrorRateTest()
{
    LOG(DEBUG) << "BIT ERROR RATE TEST" << std::endl;
    for(auto theBoard: *fDetectorContainer)
    {
        for(auto theOpticalGroup: *theBoard)
        {
            std::string phaseTuningControlRegisterName = "fc7_daq_ctrl.physical_interface_block.phase_tuning_ctrl";
            std::string theBertRegisterControl         = "fc7_daq_ctrl.physical_interface_block.bert_control";

            // configure lpGBT
            bool is10G    = static_cast<D19clpGBTInterface*>(flpGBTInterface)->GetChipRate(theOpticalGroup->flpGBT) == 10;
            auto theLpGBT = theOpticalGroup->flpGBT;
            // std::string lpGBTRegisterName = "EPRXPRBS0";
            // uint16_t theBertRegisterControl = 0;
            // flpGBTInterface->WriteChipReg(theLpGBT, lpGBTRegisterName, theBertRegisterControl);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX0Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX1Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX2Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX3Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX4Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX5Control", 0x5a);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRX6Control", 0x5a);

            uint16_t phase          = 0x7f;
            uint8_t  driverStrenght = 3;
            uint8_t  PS0delayValue  = phase & 0x3f;
            uint8_t  PS0configValue = ((phase >> 1) & 0x80) | (is10G ? 5 : 4) | (driverStrenght << 3);
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

            usleep(10000);

            flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0x00);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0x00);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0x00);
            flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x00);

            bool   allAligned = false;
            size_t attempt    = 0;

            while(!allAligned && attempt < 100)
            {
                usleep(100);
                if(flpGBTInterface->ReadChipReg(theLpGBT, "EPRX0Locked") == 0xf2 && // we observe F2. Originally we put 50
                   flpGBTInterface->ReadChipReg(theLpGBT, "EPRX1Locked") == 0xf2 && // we observe a2
                   flpGBTInterface->ReadChipReg(theLpGBT, "EPRX2Locked") == 0xf2 && flpGBTInterface->ReadChipReg(theLpGBT, "EPRX3Locked") == 0xf2 &&
                   flpGBTInterface->ReadChipReg(theLpGBT, "EPRX4Locked") == 0xf2 && flpGBTInterface->ReadChipReg(theLpGBT, "EPRX5Locked") == 0xf2 &&
                   flpGBTInterface->ReadChipReg(theLpGBT, "EPRX6Locked") == 0xf2)
                {
                    allAligned = true;
                    break;
                }
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "] attempt " << attempt << std::endl;
                ++attempt;
            }

            // debug: print status information regardless of exit status: the EPRX0Locked, EPRX0CurrentPhase10, EPRX0CurrentPhase32
            if(true)
            {
                uint16_t    value = 0;
                std::string reg   = "";
                // Locked Registers

                std::cout << std::hex; // print hex values
                reg   = "EPRX0Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX0CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX0CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX1Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX1CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX1CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX2Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX2CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX2CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX3Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX3CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX3CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX4Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX4CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX4CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX5Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX5CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX5CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                reg   = "EPRX6Locked";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX6CurrentPhase10";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;
                reg   = "EPRX6CurrentPhase32";
                value = flpGBTInterface->ReadChipReg(theLpGBT, reg);
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]: " << reg << " = " << value << std::endl;

                std::cout << std::dec;
            }

            auto lineOutputVector = static_cast<D19cFWInterface*>(fBeBoardInterface->getFirmwareInterface(fDetectorContainer->getFirstObject()))->StubDebug(true, 6, false);
            for(size_t lineIndex = 0; lineIndex < lineOutputVector.size(); ++lineIndex)
            {
                std::cout << "Line " << lineIndex << ": " << getPatternPrintout(lineOutputVector[lineIndex], 1, true) << std::endl;
            }

            // flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain10", 0x00);
            // flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain32", 0x00);
            // flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrain54", 0x00);
            // flpGBTInterface->WriteChipReg(theLpGBT, "EPRXTrainEc6", 0x00);

            if(!allAligned) //
            {
                std::cout << __PRETTY_FUNCTION__ << "[" << __LINE__ << "]"
                          << " not aligned, aborting" << std::endl;
                abort();
            }

            auto writeWithComment = [this, theBoard](const std::string& registerName, uint32_t registerValue, const std::string& comment)
            {
                std::cout << comment << std::endl;
                std::cout << "Writing register " << registerName << " value 0x" << std::hex << registerValue << std::dec << std::endl;
                this->fBeBoardInterface->WriteBoardReg(theBoard, registerName, registerValue);
                // auto readBack = this->fBeBoardInterface->ReadBoardReg(theBoard, registerName);
                // std::cout << "Reading back register " << registerName << " value 0x" << std::hex << readBack << std::dec << std::endl;
                // if(registerValue != readBack) std::cout << BOLDRED << "Readback does not match!!!" << RESET << std::endl;
                std::cout << std::endl;
            };

            auto readForAllLines =
                [this, theBoard, theOpticalGroup, &writeWithComment](
                    const std::string& controlRegisterName, uint32_t controlRegisterValue, const std::string& controlComment, const std::string& statusRegisterName, const std::string& statusComment)
            {
                for(int hybridId = 0; hybridId < 16; ++hybridId)
                // for(auto theHybrid: *theOpticalGroup)
                {
                    if(hybridId != 1) continue;
                    // uint8_t hybridId = theHybrid->getId();
                    for(uint32_t line = 0; line < 7; ++line)
                    {
                        std::string lineComment           = " for hybrid " + std::to_string(+hybridId) + " line " + std::to_string(line);
                        std::string theFullControlComment = controlComment + lineComment;
                        uint32_t    readCommand           = controlRegisterValue | (hybridId << 27) | ((line) << 20);
                        writeWithComment(controlRegisterName, readCommand, theFullControlComment);

                        uint32_t readValue = fBeBoardInterface->ReadBoardReg(theBoard, statusRegisterName);
                        std::cout << statusComment << lineComment << " = 0x" << std::hex << readValue << std::dec << std::endl;
                    }
                }
                std::cout << std::endl << std::endl << std::endl << std::endl << std::endl;
            };


            writeWithComment(theBertRegisterControl, 0xFFF20005, "Set mode = PRBS");

            // writeWithComment(theBertRegisterControl, 0xFFF3FEDE, "Set mode = PRBS");

            writeWithComment(theBertRegisterControl, 0xFFF50004, "Sample PRBS data – DATA_LD = 1");

            readForAllLines(theBertRegisterControl, 0x00000000, "Select PRBS status 0", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS status 0");

            readForAllLines(theBertRegisterControl, 0x00010000, "Select PRBS status 1", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS status 1");


            readForAllLines(theBertRegisterControl, 0x00060000, "Select PRBS FIRST DATA", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS FIRST DATA");


            readForAllLines(theBertRegisterControl, 0x00070000, "Select PRBS SAMPLED DATA", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS SAMPLED DATA");


            writeWithComment(theBertRegisterControl, 0xFFF30080, "Configure BERT – CNTR_THR = 0x80");

            writeWithComment(theBertRegisterControl, 0xFFF20035, "Configure BERT – CNTR_SEL = 11 (BER_CNT), MODE = 01 (PRBS), RX_EN = 1");

            std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Configure Word Alignment (WA)" << std::endl;

            // writeWithComment(phaseTuningControlRegisterName, 0xFFF44030, "Configure WA – SYNC_PATTERN = 0x4030");
            writeWithComment(phaseTuningControlRegisterName, 0xFFF4cea7, "Configure WA – SYNC_PATTERN = 0x4030");

            writeWithComment(phaseTuningControlRegisterName, 0xFFF50008, "Reset WA FSM – FSM_RST = 1");

            writeWithComment(phaseTuningControlRegisterName, 0xFFF20300, "Configure WA – MODE = 00 (auto), PRBS_EN = 1, SYNC_EN = 1");

            usleep(1000000);

            writeWithComment(phaseTuningControlRegisterName, 0xFFF50002, " Do Word Alignment – DO_WA = 1");

            for(int sleepSec = 0; sleepSec < 5; ++sleepSec)
            {
                std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] sleeping 1 sec..." << std::endl;
                usleep(1000000);
            }


            readForAllLines(phaseTuningControlRegisterName, 0x00000000, "Select phase tuning status", "fc7_daq_stat.physical_interface_block.phase_tuning_reply", "Phase tuning reply");


            std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Read WA Status (loop on hybrids and lines)" << std::endl;
            readForAllLines(phaseTuningControlRegisterName, 0x00010000, "Select phase tuning status", "fc7_daq_stat.physical_interface_block.phase_tuning_reply", "Phase tuning reply");

            usleep(1000000);

            std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Sample PRBS Data" << std::endl;

            // writeWithComment(theBertRegisterControl, 0x00050004, "Sample PRBS data – DATA_LD = 1");

            // std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Read PRBS First Data (loop on hybrids and lines)" << std::endl;

            // readForAllLines(theBertRegisterControl, 0x00060000, "Select PRBS FIRST DATA", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS FIRST DATA");

            // std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Read PRBS Sampled Data (loop on hybrids and lines" << std::endl;

            // readForAllLines(theBertRegisterControl, 0x00070000, "Select PRBS SAMPLED DATA", "fc7_daq_stat.physical_interface_block.bert_stat", "Read PRBS SAMPLED DATA");

            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Before PRBS start" << std::endl;
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Before PRBS start" << std::endl;
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Before PRBS start" << std::endl;
            std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Before PRBS start" << std::endl;
            
            readForAllLines(theBertRegisterControl, 0x00040000, "Select BER counter", "fc7_daq_stat.physical_interface_block.bert_stat", "Read BER counter");

            writeWithComment(theBertRegisterControl, 0xFFF20037, "Start PRBS Test");

            readForAllLines(theBertRegisterControl, 0x00040000, "Select BER counter", "fc7_daq_stat.physical_interface_block.bert_stat", "Read BER counter");

            uint32_t numberOfIterations = 5;
            std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Start BER Test with error injection (repeat " << numberOfIterations << " times)" << std::endl;

            for(uint32_t iteration = 0; iteration < numberOfIterations; ++iteration)
            {
                std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] !!!!!!!!!!! Start injecting errors - iteration " << iteration << std::endl;

                writeWithComment(theBertRegisterControl, 0xFFF200B7, "Configure BERT – CNTR_SEL = 11 (BER_CNT), MODE = 01 (PRBS), RX_EN = 1, CHK_EN = 1, INJ_ERR = 1");

                std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Stop injecting errors" << std::endl;

                writeWithComment(theBertRegisterControl, 0xFFF20037, "Configure BERT – CNTR_SEL = 11 (BER_CNT), MODE = 01 (PRBS), RX_EN = 1, CHK_EN = 1, INJ_ERR = 0");

                std::cout << __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Read BER counters (loop on hybrids and lines)" << std::endl;

                readForAllLines(theBertRegisterControl, 0x00040000, "Select BER counter", "fc7_daq_stat.physical_interface_block.bert_stat", "Read BER counter");
            }

            writeWithComment(theBertRegisterControl, 0x00020035, "Stop PRBS test");
        }
    }
}
