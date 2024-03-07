/*!
  \file                  RD53BMuxReader.cc
  \brief                 
  \author                
  \version               1.0
  \date                  
  Support:               none
*/

#include "RD53BMuxReader.h"
#include "Utils/ContainerSerialization.h"

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void RD53BMuxReader::ConfigureCalibration()
{
    LOG(INFO) << GREEN << "[RD53BMuxReader::ConfigureCalibration]" << RESET;
}

void RD53BMuxReader::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[RD53BMuxReader::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    RD53BMuxReader::run();
    //RD53BMuxReader::analyze();
    //RD53BMuxReader::sendData();
}

void RD53BMuxReader::sendData()
{
}

void RD53BMuxReader::Stop()
{
    LOG(INFO) << GREEN << "[RD53BMuxReader::Stop] Stopping" << RESET;

    Tool::Stop();

    RD53BMuxReader::draw();
    //this->closeFileHandler();

    RD53RunProgress::reset();
}

void RD53BMuxReader::localConfigure(const std::string& histoFileName, int currentRun)
{

    LOG(INFO) << GREEN << "[RD53BMuxReader::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

}

void RD53BMuxReader::run()
{

    auto chipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);
    
    CalibBase::prepareChipQueryForEnDis("chipSubset");//  ??
    std::vector<std::string> muxlist = {"Iref","NTC_VOLT",
        "ANA_IN_CURR", "ANA_SHUNT_CURR", "DIG_IN_CURR", "DIG_SHUNT_CURR", 
        "VIND","VINA", "VDDD","VDDA", "VOFS", "Vref_ADC","VrefA", "Vref_CORE", "Vref_PRE", 
        "ANA_GND_0","ANA_GND_1","ANA_GND_2","ANA_GND_3"};

        for(const auto cBoard: *fDetectorContainer)
            for(const auto cOpticalGroup: *cBoard)
                for(const auto cHybrid: *cOpticalGroup)
                    for(const auto cChip: *cHybrid)
                    {
                      
                        LOG(INFO) << GREEN << "[RD53BMuxReader::run]  board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/"
                                  << cHybrid->getId() << "/" << +cChip->getId() << RESET;

                        /* too verbose, spits out an extra message that can't be silenced
                        float vddd = chipInterface->ReadChipMonitor(cChip, "VDDD") * 2;
                        LOG(INFO) << BOLDBLUE << "VDDD = " << std::setprecision(3) << BOLDYELLOW << vddd << " V" << RESET;
                        */

                         chipInterface->WriteChipReg(cChip, "DAC_NTC", 500);

                        // raw ADC: for a list of "observables" see RD53BInterface::getADCobservable  in HWInterface/RD53BInterface.cc
                        for(const auto & mux : muxlist){
                            std::vector<uint32_t> adc;
                            for(unsigned int n = 0; n < 20; n++){
                                adc.emplace_back(chipInterface->ReadChipADC(cChip, mux)); 
                            }
                            LOG(INFO) << BOLDBLUE << std::setw(20) << mux  << " ADC = "  << BOLDYELLOW
                                << std::fixed << std::setw(5) << adc[ 0]
                                << std::fixed << std::setw(5) << adc[ 1]
                                << std::fixed << std::setw(5) << adc[ 2]
                                << std::fixed << std::setw(5) << adc[ 3]
                                << std::fixed << std::setw(5) << adc[ 4]
                                << std::fixed << std::setw(5) << adc[ 5]
                                << std::fixed << std::setw(5) << adc[ 6]
                                << std::fixed << std::setw(5) << adc[ 7]
                                << std::fixed << std::setw(5) << adc[ 8]
                                << std::fixed << std::setw(5) << adc[ 9]
                                << std::fixed << std::setw(5) << adc[10]
                                << std::fixed << std::setw(5) << adc[11]
                                << std::fixed << std::setw(5) << adc[12]
                                << std::fixed << std::setw(5) << adc[13]
                                << std::fixed << std::setw(5) << adc[14]
                                << std::fixed << std::setw(5) << adc[15]
                                << std::fixed << std::setw(5) << adc[16]
                                << std::fixed << std::setw(5) << adc[17]
                                << std::fixed << std::setw(5) << adc[18]
                                << std::fixed << std::setw(5) << adc[19]
                               <<  RESET;
                        }

                        /* cant't do the following, all private, thank you, no simple way to loop over all of them */
                        //uint32_t VMUXcode = 38;
                        //auto const alue = RD53Interface::measureVoltageCurrent(pChip, observable, isCurrentNotVoltage);
                        //auto const adc = chipInterface->measureADC(cChip, 0x1000 + VMUXcode); //13 bits: bit 12 enable, bits 6:11 I-Mon, bits 0:5 V-Mon
                        //LOG(INFO) << BOLDBLUE << "VMUX " << BOLDYELLOW << "[" << VMUXcode << "]"
                        //          << BOLDBLUE << " = " << adc << RESET;

                       //Marino Style
                        /*
                        int VMUXcode = 38; // VDDD table 28 (p58)
                        chipInterface->WriteChipReg(cChip, "MonitorEnable", 1); //Choose MUX entry
                        chipInterface->WriteChipReg(cChip, "VMonitor", VMUXcode);
                        chipInterface->SendGlobalPulse(cChip, {"ADCStartOfConversion"}); //ADC start conversion
                        auto const adc = chipInterface->ReadChipReg(cChip, "MonitoringDataADC");
                        //auto const adc = measureADC(cChip, 38);
                        LOG(INFO) << BOLDBLUE << "VMUX " << BOLDYELLOW << "[" << VMUXcode << "]"
                                  << BOLDBLUE << " = " << adc << RESET;
                        */
                    }

        // ##################
        // # Reset sequence #
        // ##################
        LOG(INFO) << "RD53BMuxReader resetting/reconfiguring board" <<  RESET;   
        for(const auto cBoard: *fDetectorContainer)
        {
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ResetBoard();
            static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureBoard(cBoard);
            this->ConfigureIT(cBoard);
            this->ConfigureFrontendIT(cBoard);
        }

    LOG(INFO) << "RD53BMuxReader more resetting" <<  RESET;   

    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
     LOG(INFO) << "RD53BMuxReader done" <<  RESET;   
}

void RD53BMuxReader::draw(bool saveData)
{
 LOG(INFO) << GREEN << "[RD53BMuxReader::draw]" << RESET;
}

void RD53BMuxReader::analyze()
{
 LOG(INFO) << GREEN << "[RD53BMuxReader::analyze]" << RESET;
}

void RD53BMuxReader::fillHisto()
{
 LOG(INFO) << GREEN << "[RD53BMuxReader::fillHisto]" << RESET;
}

