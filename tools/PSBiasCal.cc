
#include "PSBiasCal.h"

#include "../Utils/CBCChannelGroupHandler.h"
#include "../Utils/ContainerFactory.h"

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

uint32_t PSBiasCal::CalibrateChipBias(Chip* cChip, Chip* clpGBT, uint32_t point, uint32_t block, uint32_t DAC_val, float exp_val, float gnd_corr, std::string dac_str)
{
    // float VREF_LPGBT        = 1.0;
    // float cConversionFactor = VREF_LPGBT / 1024.;
    float cConversionFactor = CONVERSION_FACTOR;

    uint32_t    DAC_new_val = 0;
    std::string DAC;
    if(cChip->getFrontEndType() == FrontEndType::MPA)
    {
        std::vector<std::string> nameDAC{"A", "B", "C", "D", "E", "ThDAC", "CalDAC"};
        std::vector<uint32_t>    iDAC{0, 1, 2, 3, 4, 5, 6};
        uint32_t                 shift = iDAC[point];
        fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", 0x1 << block);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fReadoutChipInterface->WriteChipReg(cChip, "TEST" + std::to_string(block), 0x1 << shift);
        DAC = nameDAC[point] + std::to_string(block);
    }
    if(cChip->getFrontEndType() == FrontEndType::SSA)
    {
        std::vector<std::string> nameDAC{"Bias_D5BFEED", "Bias_D5PREAMP", "Bias_D5TDR", "Bias_D5ALLV", "Bias_D5ALLI", "Bias_D5DAC8"};
        std::vector<uint32_t>    iDAC{0, 1, 2, 3, 4, 9};
        uint32_t                 shift = iDAC[point];

        uint32_t MtoWr = (1 << shift);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", MtoWr & 0xff);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", (MtoWr >> 8) & 0xff);
        DAC = nameDAC[point];
    }

    fReadoutChipInterface->WriteChipReg(cChip, DAC, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t off_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str);

    fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_val);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    uint32_t act_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str);
    // LOG(INFO) << BOLDRED <<"act_val "<<act_val<<RESET;
    float    LSB          = (float(act_val) - float(off_val)) / float(DAC_val);
    float    exp_val_conv = exp_val / cConversionFactor;
    uint32_t offsetval    = 0;
    DAC_new_val           = DAC_val - uint32_t(std::round(((float(act_val) - float(exp_val_conv) - float(gnd_corr)) / LSB))) + offsetval;

    uint32_t DAC_nom_val;

    DAC_nom_val = std::max(uint32_t(0), DAC_new_val);
    DAC_nom_val = std::min(uint32_t(31), DAC_new_val);

    LOG(INFO) << BOLDRED << "Read " << act_val << " with GND offset " << gnd_corr << " Writing " << DAC_nom_val << " to   " << RESET;
    LOG(INFO) << BOLDRED << "Writing approximate DAC val " << DAC_nom_val << RESET;

    fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_nom_val);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    float new_val = float(static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str));
    // See if closest within 1 LSB TODO: Need a correction to be sure if fails

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

            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_down_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            float new_val_down = float(static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str));

            uint32_t DAC_up_val;

            DAC_up_val = std::max(uint32_t(0), DAC_new_val + 1);
            DAC_up_val = std::min(uint32_t(31), DAC_new_val + 1);

            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_up_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            float new_val_up = float(static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str));
            LOG(INFO) << BOLDRED << "Down " << new_val_down - gnd_corr << " Up " << new_val_up - gnd_corr << RESET;

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
                searching = false;
            LOG(INFO) << BOLDRED << "Writing corr DAC val " << DAC_nom_val << RESET;

            fReadoutChipInterface->WriteChipReg(cChip, DAC, DAC_nom_val);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            new_val = float(static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str));
            niter += 1;
        }
    }

    LOG(INFO) << BOLDRED << "New DAC val: " << (new_val - gnd_corr) << " Expected val: " << exp_val_conv << "+/-" << LSB << RESET;

    return DAC_nom_val;
}

void PSBiasCal::DisableTest(Chip* cChip)
{
    if(cChip->getFrontEndType() == FrontEndType::MPA)
    {
        LOG(INFO) << BOLDRED << "MPADisable " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", 0x0);
        for(int iblock = 0; iblock < 7; iblock++)
        {
            std::string test = "TEST" + std::to_string(iblock);
            fReadoutChipInterface->WriteChipReg(cChip, test, (0x0));
        }
    }
    if(cChip->getFrontEndType() == FrontEndType::SSA)
    {
        LOG(INFO) << BOLDRED << "SSADisable " << RESET;
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", 0x0);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", 0x0);
    }
}

float PSBiasCal::MeasureGnd(Chip* cChip, Chip* clpGBT, std::string dac_str)
{
    uint32_t gnd_val = 0;
    if(cChip->getFrontEndType() == FrontEndType::MPA)
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
    if(cChip->getFrontEndType() == FrontEndType::SSA)
    {
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB", (1 << 11) & 0xff);
        fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB", (1 << 11) >> 8);
        gnd_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT, dac_str);
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

                    float gndval = MeasureGnd(cChip, cOpticalReadout->flpGBT, dac_str);
                    if(cChip->getFrontEndType() == FrontEndType::SSA)
                    {
                        LOG(INFO) << BOLDRED << dac_str << " " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(cOpticalReadout->flpGBT, dac_str) << RESET;
                        LOG(INFO) << BOLDRED << "SSA " << +(cChip->getId()) << " Hyb " << +(cHybrid->getId()) << RESET;

                        std::vector<uint32_t> DAC_val{0xF, 0xF, 0xF, 0xF, 0xF, 0xF};
                        std::vector<float>    exp_val{0.082, 0.082, 0.115, 0.082, 0.082, 0.086};
                        for(int ipoint = 0; ipoint < 5; ipoint++)
                        {
                            LOG(INFO) << BOLDRED << "SSA " << ipoint << RESET;
                            LOG(DEBUG) << BOLDRED << CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, 0, DAC_val[ipoint], exp_val[ipoint], gndval, dac_str) << RESET;
                        }
                    }

                    else if(cChip->getFrontEndType() == FrontEndType::MPA)
                    {
                        std::vector<uint32_t> DAC_val{0xF, 0xF, 0xF, 0xF, 0xF};           //, 0xFF, 0xFF};
                        std::vector<float>    exp_val{0.082, 0.082, 0.108, 0.082, 0.082}; //, 1.0, 1.0};

                        for(int ipoint = 0; ipoint < 5; ipoint++)
                        {
                            for(int iblock = 0; iblock < 7; iblock++)
                            {
                                LOG(INFO) << BOLDRED << "MPA " << ipoint << "," << iblock << RESET;
                                LOG(DEBUG) << BOLDRED << CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, iblock, DAC_val[ipoint], exp_val[ipoint], gndval, dac_str) << RESET;
                            }
                        }
                    }

                    DisableTest(cChip);
                }
            } // chip
        }     // hybrid
    }         // optica]l group
}
void PSBiasCal::writeObjects() {}
// State machine control functions
void PSBiasCal::Running() {}

void PSBiasCal::Stop()
{
    dumpConfigFiles();

    // Destroy();
}

void PSBiasCal::Pause() {}

void PSBiasCal::Resume() {}
