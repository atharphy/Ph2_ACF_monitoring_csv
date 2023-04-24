
#include "tools/PSBiasCal.h"

#include "Utils/CBCChannelGroupHandler.h"
#include "Utils/ContainerFactory.h"

#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramPSBiasCal.h"
#endif
using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;
using namespace Ph2_System;

PSBiasCal::PSBiasCal() : Tool() { fRegMapContainer.reset(); }

PSBiasCal::~PSBiasCal() {}
void PSBiasCal::Reset()
{
    // set everything back to original values .. like I wasn't here
    for(auto cBoard: *fDetectorContainer)
    {
        BeBoard* theBoard = static_cast<BeBoard*>(cBoard);
        LOG(INFO) << BOLDBLUE << "Resetting all registers on back-end board " << +cBoard->getId() << RESET;
        auto&                                         cBeRegMap = fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>();
        std::vector<std::pair<std::string, uint32_t>> cVecBeBoardRegs;
        cVecBeBoardRegs.clear();
        for(auto cReg: cBeRegMap) { cVecBeBoardRegs.push_back(make_pair(cReg.first, cReg.second)); }
        fBeBoardInterface->WriteBoardMultReg(theBoard, cVecBeBoardRegs);
        auto& cRegMapThisBoard = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalGroup: *cBoard)
        {
            auto& cRegMapThisOpticalGroup = cRegMapThisBoard->at(cOpticalGroup->getIndex());
            for(auto cHybrid: *cOpticalGroup)
            {
                auto& cRegMapThisHybrid = cRegMapThisOpticalGroup->at(cHybrid->getIndex());
                LOG(INFO) << BOLDBLUE << "Resetting all registers on readout chips connected to FEhybrid#" << (cHybrid->getId()) << " back to their original values..." << RESET;
                for(auto cChip: *cHybrid)
                {
                    auto&                                         cRegMapThisChip = cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>();
                    std::vector<std::pair<std::string, uint16_t>> cVecRegisters;
                    cVecRegisters.clear();
                    for(auto cReg: cRegMapThisChip)
                    {
                        if(cChip->getFrontEndType() == FrontEndType::MPA)
                        {
                            if(false) { LOG(INFO) << BOLDMAGENTA << "\t...Will NOT set " << cReg.first << " back to original value. " << RESET; }
                        }
                        else
                        {
                            cVecRegisters.push_back(make_pair(cReg.first, cReg.second.fValue));
                        }
                    }
                    fReadoutChipInterface->WriteChipMultReg(static_cast<ReadoutChip*>(cChip), cVecRegisters);
                }
            }
        }
    }
    resetPointers();
}

void PSBiasCal::Initialise()
{
    LOG(INFO) << BOLDRED << "INIT " << RESET;
    fSuccess = false;
    // retreive original settings for all chips and all back-end boards
    ContainerFactory::copyAndInitChip<ChipRegMap>(*fDetectorContainer, fRegMapContainer);
    ContainerFactory::copyAndInitBoard<BeBoardRegMap>(*fDetectorContainer, fBoardRegContainer);
    for(auto cBoard: *fDetectorContainer)
    {
        fBoardRegContainer.at(cBoard->getIndex())->getSummary<BeBoardRegMap>() = static_cast<BeBoard*>(cBoard)->getBeBoardRegMap();
        auto& cRegMapThisBoard                                                 = fRegMapContainer.at(cBoard->getIndex());
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                auto& cRegMapThisHybrid = cRegMapThisBoard->at(cOpticalReadout->getIndex())->at(cHybrid->getIndex());
                for(auto cChip: *cHybrid) { cRegMapThisHybrid->at(cChip->getIndex())->getSummary<ChipRegMap>() = static_cast<ReadoutChip*>(cChip)->getRegMap(); }
            }
        }
    }
#ifdef __USE_ROOT__
    fDQMHistogramPSBiasCal.book(fResultFile, *fDetectorContainer, fSettingsMap);
#endif
}

void PSBiasCal::CalibrateADC()
{
    // Need to port this line to hybrid loop instead
    std::string cMonitor = "right";

    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalGroup: *cBoard)
        {
            auto& clpGBT = cOpticalGroup->flpGBT;
            if(clpGBT == nullptr) continue;
            // cMonitor="left";
            // enable voltage
            std::vector<std::string> cADCs_VoltageMonitors{"ADC1", "ADC2", "ADC6", "ADC7", "VDD"};
            std::vector<std::string> cADCs_Names{"1V_Monitor", "12V_Monitor", "1V25_Monitor", "2V55_Monitor"};
            std::vector<std::string> cModuleSide{"left", "left", "right", "left", "internal"};
            std::vector<float>       cADCs_Refs{1.0, 12, 0.645 * 1.25, 2.55, 1.25 * 0.42};
            // std::vector<float>       cADCs_Refs{1.0, 12, 0.645 * 1.146  , 2.55, 1.25*0.42};
            // use Vddd as reference
            // use P1V25 as reference
            size_t cIndx = 4;
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->WriteChipReg(clpGBT, "ADCMon", (1 << 4));
            // find correction
            std::vector<float>   cVals(10, 0);
            uint8_t              cEnableVref = 1;
            std::string          cADCsel     = cADCs_VoltageMonitors[cIndx];
            std::vector<uint8_t> cRefPoints{0, 0x05, 0x10, 0x20, 0x3F};
            std::vector<float>   cMeasurements(0);
            std::vector<float>   cSlopes(0);
            for(auto cRef: cRefPoints)
            {
                static_cast<D19clpGBTInterface*>(flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, cRef);
                for(size_t cM = 0; cM < cVals.size(); cM++)
                {
                    cVals[cM] = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, cADCsel) * CONVERSION_FACTOR;
                    // LOG (INFO) << BOLDBLUE << "EXP " <<cADCs_Refs[cIndx] << RESET;
                    // LOG (INFO) << BOLDBLUE << "ADCVAL " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, cADCsel) << " CONVERSION_FACTOR " <<CONVERSION_FACTOR<< RESET;
                }
                float cMean         = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                float cDifference_V = (cADCs_Refs[cIndx] - cMean);
                LOG(INFO) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT "
                          << " correction applied is " << +cRef << " reading [mean] is " << +cMean * 1e3 << " milli-volts."
                          << "\t...Difference between expected and measured "
                          << " values is " << cDifference_V * 1e3 << " milli-volts." << RESET;

                cMeasurements.push_back(cDifference_V);
                if(cMeasurements.size() > 1)
                {
                    for(int cI = cMeasurements.size() - 2; cI >= 0; cI--)
                    {
                        float cSlope = (cMeasurements[cMeasurements.size() - 1] - cMeasurements[cI]) / (cRefPoints[cMeasurements.size() - 1] - cRefPoints[cI]);
                        LOG(DEBUG) << BOLDBLUE << "Index " << +(cMeasurements.size() - 1) << " -- index " << cI << " slope is " << cSlope << RESET;
                        cSlopes.push_back(cSlope);
                    }
                }
            }
            float cMeanSlope = std::accumulate(cSlopes.begin(), cSlopes.end(), 0.) / cSlopes.size();
            float cIntcpt    = cMeasurements[0];
            int   cCorr      = std::min(std::floor(-1.0 * cIntcpt / cMeanSlope), 63.);
            LOG(DEBUG) << BOLDBLUE << "Mean slope is " << cMeanSlope << " , intercept is " << cIntcpt << " correction is " << cCorr << RESET;
            LOG(INFO) << BOLDBLUE << "CONVERSION_FACTOR " << CONVERSION_FACTOR << RESET;

            // apply correction and check
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->ConfigureVref(clpGBT, cEnableVref, (uint8_t)cCorr);
            for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, cADCsel) * CONVERSION_FACTOR; }
            // turn off ADC mon
            static_cast<D19clpGBTInterface*>(flpGBTInterface)->WriteChipReg(clpGBT, "ADCMon", 0x00);

            float cMean         = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
            float cDifference_V = std::fabs(cADCs_Refs[cIndx] - cMean);
            LOG(INFO) << BOLDBLUE << "ADC_" << cADCsel << " reading from lpGBT " << +cMean * 1e3 << " milli-volts."
                      << "\t...Difference between expected and measured "
                      << " values is " << cDifference_V * 1e3 << " milli-volts." << RESET;

            for(size_t cIndx = 0; cIndx < cADCs_VoltageMonitors.size(); cIndx++)
            {
                std::string cADCsel = cADCs_VoltageMonitors[cIndx];
                if(cModuleSide[cIndx].find(cMonitor) == std::string::npos) continue;

                for(size_t cM = 0; cM < cVals.size(); cM++) { cVals[cM] = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, cADCsel) * CONVERSION_FACTOR; }
                float cMean = std::accumulate(cVals.begin(), cVals.end(), 0.) / cVals.size();
                // float cStartUpMontior = cMean;
                LOG(INFO) << BOLDBLUE << "ADC_ " << cADCs_Names[cIndx] << " reading from lpGBT " << +cMean * 1e3 << " milli-volts. This is monitored via the " << cModuleSide[cIndx]
                          << " side of the module" << RESET;
            } // read all monitors
        }     // configure lpGBT
    }
}

float PSBiasCal::MeasureVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring) //, float VBGexpected, float VREFexpected, float VREFmin, float VREFmax)
{
    float VBGexpected, VREFexpected, VREFmin, VREFmax, VREFprecision;
    float ADC_GND = 0;
    float ADCMAX = 4095;
    float ADC_VBG = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        VBGexpected   = MPA2_VBG_EXPECTED;
        VREFexpected  = MPA2_VREF_EXPECTED;
        VREFmin       = MPA2_VREF_MIN;
        VREFmax       = MPA2_VREF_MAX;
        VREFprecision = MPA2_ADC_PRECISION;
        ADC_GND = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));
        ADC_VBG = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
    }
    else
    {
        VBGexpected   = SSA2_VBG_EXPECTED;
        VREFexpected  = SSA2_VREF_EXPECTED;
        VREFmin       = SSA2_VREF_MIN;
        VREFmax       = SSA2_VREF_MAX;
        VREFprecision = SSA2_ADC_PRECISION;
        ADC_GND = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));
        ADC_VBG = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
    }


    float slope = VBGexpected/(ADC_VBG-ADC_GND); // In this case it is the one measured on the module - GND measured on module. In the future it will be given.
    float offset = - ADC_GND*slope;

    std::cout << " offset " << offset << " slope "<< slope <<" ADC_VBG " << ADC_VBG << " ADC_GND " << ADC_GND <<  std::endl;

    float VREFobtained = ADCMAX*slope + offset;
    LOG(INFO) << BOLDRED << "for DAC_val "<< +static_cast<ChipInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), VREFstring) << " VREF val extrapolated: " << VREFobtained << " Expected val: " << VREFexpected  << RESET;

    std::cout << " VREFobtained " << VREFobtained << " VREFmax " << VREFmax << " VREFmin " << VREFmin << " (VREFobtained - VREFexpected) " << (VREFobtained - VREFexpected) << " VREFprecision " << VREFprecision << std::endl;

    if (VREFobtained > VREFmax || VREFobtained < VREFmin || abs(VREFobtained - VREFexpected)>VREFprecision)
    {
        LOG(INFO) << BOLDRED << " Need to calibrate VREF! VREF is outside of allowed range!!" << RESET;
        VREFobtained = CalibrateVREF(cChip,VBGstring, VREFstring, VBGexpected, VREFexpected);
        LOG(INFO) << BOLDRED << "for new DAC_val " << +static_cast<ChipInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), VREFstring)<< " New VREF val: " << VREFobtained << " Expected val: " << VREFexpected  << RESET;
        //FIXMEEE Get values again to update the slope!!
    }
    return VREFobtained;
}


float PSBiasCal::CalibrateVREF(Ph2_HwDescription::Chip* cChip, std::string VBGstring, std::string VREFstring, float VBGexpected, float VREFexpected)
{

    bool cSuccess = false;
    uint8_t    DAC_val = 0; 
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        DAC_val = cChip->pChipFuseID.ADCRef();
    else  
        DAC_val = static_cast<ChipInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), VREFstring);
    std::cout << " DAC_val " << +DAC_val << std::endl;
    uint8_t    DAC_new_val = 0;
    float ADC_GND = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        ADC_GND = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));
        std::cout << " GND " << ADC_GND << " from Kevin " << uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->measureGnd(static_cast<ReadoutChip*>(cChip)))) << std::endl;
    }    
    else
        ADC_GND = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));

    float ADCMAX = 4095;
    float ADCLSB = VREFexpected / (ADCMAX - ADC_GND); // slope estimated from VREF
    float offset = -ADC_GND*ADCLSB;
    float ADC_VBG = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        ADC_VBG = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
        std::cout << " VBG " << ADC_VBG << " from Kevin " << uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->measureBg(static_cast<ReadoutChip*>(cChip)))) << std::endl;
    }    

    else
        ADC_VBG = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));

    float targetADCVBG = ( VBGexpected- offset)/ ADCLSB;
    std::cout << " offset " << offset << " targetADCVBG "<< targetADCVBG <<" ADC_VBG " << ADC_VBG << " ADCLSB " << ADCLSB << " ADC_GND "<< ADC_GND << std::endl;
    // write VREF DAC with value 0 (minimum)
    // FIXME HERE FOR MPA2!!!!
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), 0);
    else
        fReadoutChipInterface->WriteChipReg(cChip, VREFstring, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    std::cout << " fReadoutChipInterface->WriteChipReg(cChip, VREFstring, 0); "<<std::endl;
    uint32_t off_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        std::cout << " MPA2 "<< std::endl;
        off_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),VBGstring)));
        std::cout << " off_val " << off_val << std::endl;
    }
    else
        off_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),VBGstring)));
    std::cout << " now set the DAC value to its maximum value " << std::endl;
    // now set the DAC value to its maximum value
    uint32_t DAC_max =  0x1F;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), DAC_max);
    else
        fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_max);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t max_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        max_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),VBGstring)));
    else
        max_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),VBGstring)));
    std::cout << " max_val " << max_val << " off_val "<< off_val << " float(DAC_max) "<< float(DAC_max) <<std::endl;
    float LSB = (float(off_val) - float(max_val)) / float(DAC_max);
    LOG(INFO) << BOLDMAGENTA << VREFstring << " LSB " << LSB << " default DAC val " << +DAC_val << RESET;
    std::cout<< " ADC_VBG "<< ADC_VBG <<  " targetADCVBG " << targetADCVBG << " diff "<< abs(ADC_VBG - targetADCVBG) << " LSB "<< LSB << std::endl;
    //std::cout<< " ADC_VBG "<< ADC_VBG << " targetADCVBG " << targetADCVBG << " diff "<< abs(ADC_VBG - targetADCVBG) << " LSB "<< LSB << std::endl;

    while(!cSuccess)
    {

        if (abs(ADC_VBG - targetADCVBG) < LSB )
        //if (abs(ADC_VBG - targetADCVBG) < LSB )
        {
            cSuccess = true;
            if(cChip->getFrontEndType() == FrontEndType::MPA2)
                static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), DAC_val);
            else
                fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_val);
            std::cout << " VREF calibrated" << std::endl;
            break;
        }
        std::cout << " DAC_val " << +DAC_val << std::endl;
        int sign = 0;
        if (targetADCVBG > ADC_VBG ) sign = -1;
        else sign = 1;
        uint8_t steps = uint8_t(std::round(abs(float(targetADCVBG)  - float(ADC_VBG) ) / LSB));
        std::cout << " (float(targetADCVBG) - float(ADC_VBG) ) / LSB "<< +steps <<std::endl;
        if ((DAC_val + sign*steps) > 31)
            DAC_new_val = 31;
        else if (((DAC_val + sign*steps) < 0))
            DAC_new_val = 0;
        else     
            DAC_new_val     = DAC_val +sign*steps;
        DAC_val = DAC_new_val;
        // std::cout << " (float(targetADCVBG) - float(ADC_VBG) ) / LSB "<< (float(targetADCVBG) - float(ADC_VBG) ) / LSB <<std::endl;
        // DAC_val     = DAC_val - uint32_t(std::round((float(targetADCVBG) - float(ADC_VBG) ) / LSB));
        std::cout << " DAC_new_val " << +DAC_new_val << std::endl;
        // DAC_new_val = std::min(uint8_t(31), DAC_val);
        // std::cout << " DAC_val "<< +DAC_val << " DAC_new_val "<< +DAC_new_val << std::endl;
        fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_new_val);
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        if(cChip->getFrontEndType() == FrontEndType::MPA2)
            ADC_VBG = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
        else
            ADC_VBG = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
        LOG(INFO) << BOLDRED << " VREF at " << +DAC_new_val << " is ADC_VBG " << ADC_VBG << " target " <<targetADCVBG  << RESET;

        bool checkadj = true;
        if(checkadj) // This checks if the linear extrapolation finds the best value.
        {
            bool     searching = true;
            uint32_t niter     = 0;
            while(searching)
            {
                uint8_t DAC_down_val;
                std::cout << " DAC_new_val - 1 " << +uint8_t(DAC_new_val - 1) <<std::endl;
                DAC_down_val = std::max(uint8_t(0), uint8_t(DAC_new_val - 1));

                LOG(INFO) << BOLDRED << "DAC_down_val "<<+DAC_down_val << RESET;
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), DAC_down_val);
                else
                    fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_down_val);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));

                uint32_t new_val_down = 0;
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    new_val_down = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
                else
                    new_val_down = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
                
                uint8_t DAC_up_val = std::min(uint8_t(31), uint8_t(DAC_new_val + 1));
                LOG(INFO) << BOLDRED << "DAC_up_val "<<+DAC_up_val << RESET;

                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), DAC_up_val);
                else
                    fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_up_val);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));

                uint32_t new_val_up = 0;
                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    new_val_up = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
                else
                    new_val_up = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));

                
                LOG(INFO) << BOLDRED << "Down " << new_val_down  << " Up " << new_val_up  << RESET;

                float expdiff     = std::fabs(targetADCVBG - ADC_VBG );
                float expdiffdown = std::fabs(targetADCVBG - new_val_down );
                float expdiffup   = std::fabs(targetADCVBG - new_val_up);

                if((expdiffdown < expdiff) or (expdiffup < expdiff))
                {
                    LOG(INFO) << BOLDRED << "Bad extrapolation in PSBiasCal: expdiffdown:" << expdiffdown << ", expdiffup:" << expdiffup << ", expdiff:" << expdiff << ", iteration:" << niter << RESET;
                    if((expdiffdown < expdiff))
                    {
                        DAC_val = DAC_down_val;
                        DAC_new_val = DAC_new_val - 1;
                    }
                    if((expdiffup < expdiff))
                    {
                        DAC_val = DAC_up_val;
                        DAC_new_val = DAC_new_val + 1;
                    }
                }
                else
                {
                    LOG(INFO) << BOLDRED << "Correct extrapolation in PSBiasCal , iteration:" << niter << RESET;
                    if(cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        usleep(100000);
                        LOG(INFO) << BOLDRED << " Register VBG gives ADC " << RESET;
                        LOG(INFO) << BOLDRED << +static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring) << RESET;
                    }
                    searching = false;
                }
                LOG(INFO) << BOLDRED << "Writing corr DAC val " << +DAC_val << RESET;

                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(static_cast<ReadoutChip*>(cChip), DAC_val);
                else
                    fReadoutChipInterface->WriteChipReg(cChip, VREFstring, DAC_val);
                std::this_thread::sleep_for(std::chrono::milliseconds(2));

                if(cChip->getFrontEndType() == FrontEndType::MPA2)
                    ADC_VBG = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
                else
                    ADC_VBG = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), VBGstring)));
    
                niter += 1;
            }
        }

    
        cSuccess=true;

    }

    float newVBG = ADC_VBG*ADCLSB+offset;
    LOG(INFO) << BOLDRED << "New VBG val: " << newVBG << " Expected val: " << VBGexpected << "+/-" << LSB*ADCLSB << RESET;
    float slope = (VBGexpected)/(ADC_VBG-ADC_GND);
    offset = -ADC_GND*slope;
    float VREFobtained = ADCMAX*slope+offset;
    LOG(INFO) << BOLDRED << "New VREF val: " << VREFobtained << " Expected val: " << VREFexpected << "+/-" << LSB*ADCLSB << RESET;


    return VREFobtained;
}



uint32_t PSBiasCal::CalibrateChipBias(Chip* cChip, Chip* clpGBT, uint32_t point, uint32_t block, uint32_t DAC_val, float exp_val, float gnd_corr, std::string dac_str, float VREFmeasured)
{
    // float VREF_LPGBT        = 1.0;
    // float cConversionFactor = VREF_LPGBT / 1024.;
    // float cConversionFactor = CONVERSION_FACTOR;

    uint32_t    DAC_new_val = 0;
    std::string DAC;
    uint8_t     regIndex = 0;
    float       ADCLSB   = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2) { ADCLSB = (static_cast<MPA2Interface*>(fReadoutChipInterface)->calculateADCLSB(cChip)); }
    else if(cChip->getFrontEndType() == FrontEndType::SSA2)
    {
        ADCLSB = (static_cast<SSA2Interface*>(fReadoutChipInterface)->CalculateADCLSB(cChip, VREFmeasured));
    }
    else
    {
        LOG(ERROR) << BOLDRED << "Calibration procedure unknown for this chip type - aborting." << RESET;
        std::runtime_error(std::string("PSBiasCal::CalibrateChipBias: Error, procedure implemented only for MPA2 & SSA2 at this time. Abort."));
    }

    if(cChip->getFrontEndType() == FrontEndType::MPA or cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        std::vector<std::string> nameDAC{"A", "B", "C", "D", "E", "ThDAC", "CalDAC"};
        std::vector<uint32_t>    iDAC{0, 1, 2, 3, 4, 5, 6};

        uint32_t shift = iDAC[point];
        if(cChip->getFrontEndType() == FrontEndType::MPA2)
        {
            // LOG(INFO) << BOLDRED << "ADCControl: " <<+fReadoutChipInterface->ReadChipReg(cChip, "ADCcontrol")<< RESET;

            // LOG(INFO) << BOLDRED << "ADCControl: " <<+fReadoutChipInterface->ReadChipReg(cChip, "ADCcontrol")<< RESET;
            static_cast<MPA2Interface*>(fReadoutChipInterface)->selectBlock(cChip, block + 1, shift);
        }
        if(cChip->getFrontEndType() == FrontEndType::MPA)
        {
            fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", 0x1 << block);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            fReadoutChipInterface->WriteChipReg(cChip, "TEST" + std::to_string(block), 0x1 << shift);
        }

        DAC = nameDAC[point] + std::to_string(block);
    }

    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
    {
        std::vector<std::string> nameDAC{"Bias_D5BFEED", "Bias_D5PREAMP", "Bias_D5TDR", "Bias_D5ALLV", "Bias_D5ALLI", "Bias_D5DAC8"};
        std::vector<uint8_t>     ADCcontrolIndex{1, 2, 3, 4, 5, 10};
        std::vector<uint32_t>    iDAC{0, 1, 2, 3, 4, 9};
        uint32_t                 shift = iDAC[point];

        uint32_t MtoWr = (1 << shift);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", MtoWr & 0xff);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", (MtoWr >> 8) & 0xff);
        DAC      = nameDAC[point];
        regIndex = ADCcontrolIndex[point];
    }
    // write DAC (ie one of the registers) with value 0 (minimum)
    fReadoutChipInterface->WriteChipReg(cChip, DAC, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    LOG(INFO) << BOLDRED << "DAC:" << DAC << RESET;

    uint32_t off_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        off_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip))); // dac_str not needed here
    else
        off_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

    LOG(INFO) << BOLDRED << " value for DAC " << DAC << " at 0 is off_val " << off_val << RESET;

    // now set the DAC value to its default value and get the nominal value
    fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_val);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    LOG(INFO) << BOLDRED << "NOM" << RESET;
    uint32_t act_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        act_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)));
    else
        act_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

    LOG(INFO) << BOLDRED << " value for DAC " << DAC << " at " << DAC_val << " is act_val " << unsigned(act_val) << RESET;

    // for (uint8_t ival=0;ival<32;ival++)
    // {
    //     LOG(INFO) << BOLDRED << "NOM " <<+ival<< RESET;
    //     fReadoutChipInterface->WriteChipReg(cChip, DAC, ival);
    //     std::this_thread::sleep_for(std::chrono::milliseconds(2));
    //     LOG(INFO) << BOLDRED << static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)<< RESET;
    // }

    float LSB = (float(act_val) - float(off_val)) / float(DAC_val);
    LOG(INFO) << BOLDRED << DAC << " LSB " << LSB << RESET;
    float ADC_GND = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        ADC_GND = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));
        std::cout << " GND " << ADC_GND << " from Kevin " << uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->measureGnd(static_cast<ReadoutChip*>(cChip)))) << std::endl;
    }    
    else
        ADC_GND = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip),"GND")));

    float exp_val_conv = 0.0;
    exp_val_conv       = exp_val / ADCLSB +ADC_GND; // converted from volts to ADC
    LOG(INFO) << BOLDRED << "exp_val_conv " << exp_val_conv << RESET;

    uint32_t offsetval = 0;
    DAC_new_val        = DAC_val - uint32_t(std::round((float(act_val) - float(exp_val_conv) - float(gnd_corr)) / LSB)) + offsetval;
    LOG(INFO) << BOLDRED << "DAC_new_val " << DAC_new_val << RESET;

    uint32_t DAC_nom_val;

    DAC_nom_val = std::max(uint32_t(0), DAC_new_val);
    DAC_nom_val = std::min(uint32_t(31), DAC_new_val);

    // LOG(INFO) << BOLDRED << "DAC "<<DAC<< RESET;
    // LOG(INFO) << BOLDRED << "exp_val " << exp_val << " exp_val_conv " << exp_val_conv << " act_val "<< act_val<<" LSB "<<LSB<< RESET;

    // LOG(INFO) << BOLDRED << "Read " << act_val << " with GND offset " << gnd_corr << " Writing " << DAC_nom_val << " to   " << RESET;
    // LOG(INFO) << BOLDRED << "Writing approximate DAC val " << DAC_nom_val << RESET;

    // now writing the DAC to the some new value DAC_nom_val estimated above (not clear how)
    fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_nom_val);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));

    uint32_t new_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
        new_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)));
    else
        new_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

    LOG(INFO) << BOLDRED << " value for DAC " << DAC << "at " << DAC_nom_val << " is new_val " << new_val << RESET;

    bool checkadj = true;
    if(checkadj) // This checks if the linear extrapolation finds the best value.
    {
        bool     searching = true;
        uint32_t niter     = 0;
        while(searching)
        {
            uint32_t DAC_down_val;

            DAC_down_val = std::max(uint32_t(0), DAC_new_val - 1);
            DAC_down_val = std::min(uint32_t(31), DAC_new_val - 1);

            // LOG(INFO) << BOLDRED << "DAC_down_val "<<DAC_down_val << RESET;
            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_down_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            uint32_t new_val_down = 0;
            if(cChip->getFrontEndType() == FrontEndType::MPA2)
                new_val_down = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)));
            else
                new_val_down = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

            uint32_t DAC_up_val;
            DAC_up_val = std::max(uint32_t(0), DAC_new_val + 1);
            DAC_up_val = std::min(uint32_t(31), DAC_new_val + 1);
            // LOG(INFO) << BOLDRED << "DAC_up_val "<<DAC_up_val << RESET;

            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_up_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            uint32_t new_val_up = 0;
            if(cChip->getFrontEndType() == FrontEndType::MPA2)
                new_val_up = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)));
            else
                new_val_up = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

            // LOG(INFO) << BOLDRED << "Down " << new_val_down - gnd_corr << " Up " << new_val_up - gnd_corr << RESET;

            float expdiff     = std::fabs(exp_val_conv - (new_val - gnd_corr));
            float expdiffdown = std::fabs(exp_val_conv - (new_val_down - gnd_corr));
            float expdiffup   = std::fabs(exp_val_conv - (new_val_up - gnd_corr));

            if((expdiffdown < expdiff) or (expdiffup < expdiff))
            {
                LOG(INFO) << BOLDRED << "Bad extrapolation in PSBiasCal: expdiffdown:" << expdiffdown << ", expdiffup:" << expdiffup << ", expdiff:" << expdiff << ", iteration:" << niter << RESET;
                if((expdiffdown < expdiff))
                {
                    DAC_nom_val = DAC_down_val;
                    DAC_new_val = DAC_new_val - 1;
                }
                if((expdiffup < expdiff))
                {
                    DAC_nom_val = DAC_up_val;
                    DAC_new_val = DAC_new_val + 1;
                }
            }
            else
            {
                LOG(INFO) << BOLDRED << "Correct extrapolation in PSBiasCal , iteration:" << niter << RESET;
                if(cChip->getFrontEndType() == FrontEndType::SSA2)
                {
                    usleep(100000);
                    LOG(INFO) << BOLDRED << " Register " << DAC << " corresponding to index " << unsigned(regIndex) << " gives ADC " << RESET;
                    LOG(INFO) << BOLDRED << +static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex) << RESET;
                }
                searching = false;
            }
            LOG(INFO) << BOLDRED << "Writing corr DAC val " << DAC_nom_val << RESET;

            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_nom_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));

            new_val = 0;
            if(cChip->getFrontEndType() == FrontEndType::MPA2)
                new_val = uint32_t(std::round(static_cast<MPA2Interface*>(fReadoutChipInterface)->ADCMeasure(cChip)));
            else
                new_val = uint32_t(std::round(static_cast<SSA2Interface*>(fReadoutChipInterface)->ReadADC(static_cast<ReadoutChip*>(cChip), regIndex)));

            niter += 1;
        }
    }

    LOG(INFO) << BOLDRED << "New DAC val: " << (new_val - gnd_corr) << " Expected val: " << exp_val_conv << "+/-" << LSB << RESET;

    return DAC_nom_val;
}

void PSBiasCal::DisableTest(Chip* cChip)
{
    if(cChip->getFrontEndType() == FrontEndType::MPA2)
    {
        LOG(INFO) << BOLDRED << "MPA2 Disable " << RESET;
        static_cast<MPA2Interface*>(fReadoutChipInterface)->selectBlock(cChip, 0);
    }

    else if(cChip->getFrontEndType() == FrontEndType::MPA)
    {
        LOG(INFO) << BOLDRED << "MPA Disable " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", 0x0);
        for(int iblock = 0; iblock < 7; iblock++)
        {
            std::string test = "TEST" + std::to_string(iblock);
            fReadoutChipInterface->WriteChipReg(cChip, test, (0x0));
        }
    }
    else if(cChip->getFrontEndType() == FrontEndType::SSA)
    {
        LOG(INFO) << BOLDRED << "SSA Disable " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", 0x0);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", 0x0);
    }
    else if(cChip->getFrontEndType() == FrontEndType::SSA2)
    {
        LOG(INFO) << BOLDRED << "SSA2 Disable " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", 0x0);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", 0x0);
    }
}

float PSBiasCal::MeasureGnd(Chip* cChip, Chip* clpGBT, std::string dac_str)
{
    float gnd_val = 0.0;

    if(cChip->getFrontEndType() == FrontEndType::MPA2) { gnd_val = static_cast<MPA2Interface*>(fReadoutChipInterface)->measureGnd(cChip); }
    else if(cChip->getFrontEndType() == FrontEndType::MPA)
    {
        std::vector<float> data(7, 0);
        for(int iblock = 0; iblock < 7; iblock++)
        {
            std::string test = "TEST" + std::to_string(iblock);
            fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", (0x1 << iblock));
            fReadoutChipInterface->WriteChipReg(cChip, test, (0x1 << 7));

            data[iblock] = float(static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str));
        }

        float avggnd = accumulate(data.begin(), data.end(), 0.0) / data.size();

        gnd_val = avggnd;
    }
    else if(cChip->getFrontEndType() == FrontEndType::SSA)
    {
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", (1 << 11) & 0xff);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", (1 << 11) >> 8);
        gnd_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str);
    }
    else if(cChip->getFrontEndType() == FrontEndType::SSA2)
    {
        gnd_val = static_cast<SSA2Interface*>(fReadoutChipInterface)->MeasureGND(cChip);
    }
    LOG(INFO) << BOLDRED << "gndval " << gnd_val << RESET;
    return gnd_val;
}

void PSBiasCal::CalibrateBias()
{
    LOG(INFO) << BOLDRED << "Disable all outputs... " << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid) { DisableTest(cChip); } // chip
            }
        }
    }
    LOG(INFO) << BOLDRED << "Starting Cal" << RESET;
    DetectorDataContainer theVREFDACContainer;
    ContainerFactory::copyAndInitChip<std::pair<uint32_t, float>>(*fDetectorContainer, theVREFDACContainer);

    for(const auto cBoard: *fDetectorContainer)
    {
        for(auto cOpticalReadout: *cBoard)
        {
            for(auto cHybrid: *cOpticalReadout)
            {
                for(auto cChip: *cHybrid)
                {
                    std::string dac_str = "ADC3";
                    if(cHybrid->getId() == 1) dac_str = "ADC0";
                    float VREFmeasured = 0;
                    std::string VREFstring = "";
                    uint32_t VREFdac = 0;
                    float gndval = MeasureGnd(cChip, cOpticalReadout->flpGBT, dac_str);
                    if(cChip->getFrontEndType() == FrontEndType::SSA || cChip->getFrontEndType() == FrontEndType::SSA2)
                    {
                        LOG(INFO) << BOLDRED << "SSA " << +(cChip->getId()) << " Hyb " << +(cHybrid->getId()) << RESET;
                        LOG(INFO) << BOLDRED << "ground is " << gndval << RESET;

                        LOG(INFO) << BOLDMAGENTA << " Calibrate VREF "<< RESET;

                        // VREFmeasured = CalibrateVREF(cChip,"VBG","ADC_VREF",SSA2_VBG_EXPECTED,SSA2_VREF_EXPECTED);
                        VREFstring = "ADC_VREF";
                        VREFmeasured = MeasureVREF(cChip,"VBG",VREFstring); //,SSA2_VBG_EXPECTED,SSA2_VREF_EXPECTED,SSA2_VREF_MIN, SSA2_VREF_MAX);
                        VREFdac = static_cast<ChipInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), VREFstring);
                        std::cout << "VREFdac "<< VREFdac << " VREFmeasured "<< VREFmeasured << std::endl;


                        std::vector<uint32_t> DAC_val{0xF, 0xF, 0xF, 0xF, 0xF, 0xF};
                        std::vector<float>    exp_val{0.082, 0.082, 0.115, 0.082, 0.082, 0.086};
                        for(int ipoint = 0; ipoint <= 5; ipoint++)
                        {
                            LOG(INFO) << BOLDRED << "SSA point " << ipoint << RESET;
                            LOG(DEBUG) << BOLDRED << CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, 0, DAC_val[ipoint], exp_val[ipoint], gndval, dac_str, VREFmeasured) << RESET;
                        }
                        // LOG(DEBUG) << BOLDRED << " done with above SSA" << RESET;
                    }

                    else if(cChip->getFrontEndType() == FrontEndType::MPA or cChip->getFrontEndType() == FrontEndType::MPA2)
                    {
                        std::vector<uint32_t> DAC_val{0xF, 0xF, 0xF, 0xF, 0xF};           //, 0xFF, 0xFF};
                        std::vector<float>    exp_val{0.082, 0.082, 0.108, 0.082, 0.082}; //, 1.0, 1.0};
                        static_cast<MPA2Interface*>(fReadoutChipInterface)->loadVref(cChip);
                        //CalibrateVREF(cChip,gndval,"bandgap","vref",MPA2_VBG_EXPECTED,MPA2_VREF_EXPECTED);
                        VREFstring = "vref";
                        VREFmeasured = MeasureVREF(cChip,"bandgap",VREFstring); //,MPA2_VBG_EXPECTED,MPA2_VREF_EXPECTED,MPA2_VREF_MIN, MPA2_VREF_MAX);
                        VREFdac = static_cast<ChipInterface*>(fReadoutChipInterface)->ReadChipReg(static_cast<ReadoutChip*>(cChip), VREFstring);
                        std::cout << "VREFdac "<< VREFdac << " VREFmeasured "<< VREFmeasured << std::endl;

                        for(int ipoint = 0; ipoint < 5; ipoint++)
                        {
                            for(int iblock = 0; iblock < 7; iblock++)
                            {
                                LOG(INFO) << BOLDRED << "MPA " << ipoint << "," << iblock << RESET;
                                CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, iblock, DAC_val[ipoint], exp_val[ipoint], gndval, dac_str);
                            }
                        }
                    }
                    std::cout << " ------ setting VREF DAC in DACContainter "<< VREFdac <<std::endl;
                    theVREFDACContainer.getObject(cChip->getBeBoardId())->getObject(cChip->getOpticalGroupId())->getObject(cChip->getHybridId())->getObject(cChip->getId())->getSummary<std::pair<uint32_t, float>>().first= VREFdac;
                    theVREFDACContainer.getObject(cChip->getBeBoardId())->getObject(cChip->getOpticalGroupId())->getObject(cChip->getHybridId())->getObject(cChip->getId())->getSummary<std::pair<uint32_t, float>>().second= VREFmeasured;


                    DisableTest(cChip);
                }

            } // chip
        }     // hybrid
    }         // optica]l group
#ifdef __USE_ROOT__
    fDQMHistogramPSBiasCal.fillDACPlots(theVREFDACContainer);
#else
    if(fDQMStreamerEnabled)
    {
        ContainerSerialization theContainerSerialization("PSBiasCalVrefDac");
        theContainerSerialization.streamByBoardContainer(fDQMStreamer, theVREFDACContainer);
    }
#endif
}
void PSBiasCal::writeObjects() 
{
#ifdef __USE_ROOT__
    fDQMHistogramPSBiasCal.process();
#endif
}
// State machine control functions
void PSBiasCal::Running() {}

void PSBiasCal::Stop()
{
    dumpConfigFiles();

    // Destroy();
}

void PSBiasCal::Pause() {}

void PSBiasCal::Resume() {}
