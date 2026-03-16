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
#include <regex>

using std::cregex_iterator;
using std::regex;

using namespace Ph2_HwDescription;
using namespace Ph2_HwInterface;

void RD53BMuxReader::ConfigureCalibration() { LOG(INFO) << GREEN << "[RD53BMuxReader::ConfigureCalibration]" << RESET; }

void RD53BMuxReader::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[RD53BMuxReader::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    RD53BMuxReader::run();
    // RD53BMuxReader::analyze();
    // RD53BMuxReader::sendData();
}

void RD53BMuxReader::sendData() {}

void RD53BMuxReader::Stop()
{
    LOG(INFO) << GREEN << "[RD53BMuxReader::Stop] Stopping" << RESET;

    Tool::Stop();

    RD53BMuxReader::draw();
    // this->closeFileHandler();

    RD53RunProgress::reset();
}

RD53BMuxReader::adc_result RD53BMuxReader::get_adc(Ph2_HwInterface::RD53Interface* chipInterface, Ph2_HwDescription::ReadoutChip* chip, const std::string& name, size_t nsample)
{
    unsigned int       sum_adc = 0;
    std::stringstream  line;
    std::vector<float> adc_values;
    for(unsigned int n = 0; n < nsample; n++)
    {
        const auto value = chipInterface->ReadChipADC(chip, name);
        if(value < 4096)
        {
            sum_adc += value;
            line << std::fixed << std::setw(5) << value;
            adc_values.push_back(value);
        }
        else { line << RED << std::fixed << std::setw(5) << value << YELLOW; }
    }
    // basic outlier rejection
    float        adc_mean = sum_adc / adc_values.size();
    float        adc_rms  = 1;
    float        T        = 200;
    unsigned int n_valid  = 0;
    while(T > 10)
    {
        float sum = 0, rms = 0;
        n_valid = 0;
        for(const auto v: adc_values)
        {
            if(abs(v - adc_mean) < T)
            {
                sum += v;
                rms += pow(v - adc_mean, 2);
                n_valid++;
            }
        }
        if(n_valid > 2)
        {
            adc_mean = float(sum) / n_valid;
            adc_rms  = float(rms) / n_valid;
            T        = T / 2;
        }
        else { return {adc_mean, adc_rms, line.str()}; }
    }

    if(n_valid > 0) { return {adc_mean, adc_rms, line.str()}; }
    else { return {0, 0, ""}; }
}

float RD53BMuxReader::measure_adc_offset(Ph2_HwInterface::RD53Interface* chipInterface, Ph2_HwDescription::ReadoutChip* chip, size_t nsample)
{
    // esitmate the adc offset assuming the ntc current dac has no offset (i.e. dac =0 means current = 0)
    float        x_sum = 0, y_sum = 0, x2_sum = 0, xy_sum = 0;
    unsigned int n = 0;
    for(unsigned int idac = 0; idac < 100; idac += 10)
    {
        chipInterface->WriteChipReg(chip, "DAC_NTC", idac);
        usleep(100000);
        const auto  adc_result = get_adc(chipInterface, chip, "NTC_CURR", nsample);
        const float adc        = adc_result.value;
        // const auto adc = chipInterface->ReadChipADC(chip, "NTC_CURR");

        if((adc > 0) && (adc < 4096))
        {
            n += 1;
            x_sum += idac;
            y_sum += adc;
            x2_sum += idac * idac;
            xy_sum += adc * idac;
        }
        if(n > 5) break;
    }
    float offset_adc = (x2_sum * y_sum - x_sum * xy_sum) / (x2_sum * n - x_sum * x_sum);

    // // restore the original vaue
    chipInterface->WriteChipReg(chip, "DAC_NTC", 100);

    return offset_adc; // convert to millivolts using  -test_offset_adc * adc_slope * 1e3;
}

void RD53BMuxReader::configure(const std::string& args)
{
    // see HWInterface/RD53BInterface.cc
    std::vector<std::string> all         = {"Iref",
                                            "NTC_VOLT",
                                            "NTC_CURR",
                                            "ANA_IN_CURR",
                                            "ANA_SHUNT_CURR",
                                            "DIG_IN_CURR",
                                            "DIG_SHUNT_CURR",
                                            "VIND",
                                            "VINA",
                                            "VDDD",
                                            "VDDA",
                                            "VOFS",
                                            "VrefA",
                                            "VrefD",
                                            "Vref_CORE",
                                            "Vref_PRE",
                                            "POLY_ABS_TEMPSENS_TOP",
                                            "POLY_ABS_TEMPSENS_BOTTOM",
                                            "TEMPSENS_ANA_SLDO",
                                            "TEMPSENS_DIG_SLDO",
                                            "TEMPSENS_CENTER",
                                            "RADSENS_ANA_SLDO",
                                            "RADSENS_DIG_SLDO",
                                            "RADSENS_CENTER",
                                            "VCAL_HI",
                                            "VCAL_MED",
                                            "LIN_FE_REF_KRUMCURR",
                                            "LIN_FE_GDAC_MAIN",
                                            "LIN_FE_GDAC_LEFT",
                                            "LIN_FE_GDAC_RIGHT",
                                            "LIN_FE_PREAMP_MAIN",
                                            "LIN_FE_PREAMP_LEFT",
                                            "LIN_FE_PREAMP_RIGHT",
                                            "LIN_FE_PREAMP_TOP_LEFT",
                                            "LIN_FE_PREAMP_TOP",
                                            "LIN_FE_PREAMP_TOP_RIGHT",
                                            "ANA_GND_0",
                                            "ANA_GND_1",
                                            "ANA_GND_2",
                                            "ANA_GND_3",
                                            "ANA_GND_4",
                                            "ANA_GND_5",
                                            "ANA_GND_6",
                                            "ANA_GND_7",
                                            "ANA_GND_8",
                                            "ANA_GND_9",
                                            "ANA_GND_10",
                                            "ANA_GND_11",
                                            "HIGH_Z"};
    std::vector<std::string> default_adc = {"VIND", "VINA", "VDDD", "VDDA", "VOFS", "ANA_IN_CURR", "ANA_SHUNT_CURR", "DIG_IN_CURR", "DIG_SHUNT_CURR"};

    muxlist.clear();
    use_wlt_calibration       = true;
    do_adc_offset_measurement = true;
    read_temperatures         = false; // override by adding 'temperatures' to the adc list
    do_poly_test              = false; // override by adding 'polytest' to the adc list

    if(args == "")
    {
        for(size_t i = 0; i < default_adc.size(); i++) { muxlist.push_back(default_adc[i]); }
        return;
    }

    // comma separated string, split
    std::vector<std::string> v;
    std::stringstream        ss(args);

    while(ss.good())
    {
        std::string arg;
        getline(ss, arg, ',');
        if(arg == "default")
        {
            for(size_t i = 0; i < default_adc.size(); i++) { muxlist.push_back(default_adc[i]); }
        }
        else if(arg == "no_wlt_calibration") { use_wlt_calibration = false; }
        else if(arg == "temperatures") { read_temperatures = true; }
        else if(arg == "polytest") { do_poly_test = true; }
        else if(std::find(all.begin(), all.end(), arg) != all.end()) { muxlist.push_back(arg); }
        else
        {
            // possible wildcard
            bool       found_match = false;
            const auto pattern     = std::regex_replace(arg, std::regex(R"(\*)"), ".*");
            regex      re("^" + pattern + "$");
            for(size_t i = 0; i < all.size(); i++)
            {
                const auto adc = all[i];
                if(std::regex_match(adc, re))
                {
                    muxlist.push_back(adc);
                    found_match = true;
                }
            }
            if(!found_match) { LOG(WARNING) << RED << "[RD53BMuxReader::configure]  unknown adc " << arg << RESET; }
        }
    }
}

void RD53BMuxReader::localConfigure(const std::string& histoFileName, int currentRun)
{
    LOG(INFO) << GREEN << "[RD53BMuxReader::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;
}

void RD53BMuxReader::run()
{
    auto chipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    std::vector<std::string> tempsens_list  = {"POLY_TEMPSENS_TOP",
                                               "POLY_TEMPSENS_BOTTOM",
                                               "TEMPSENS_ANA_SLDO",
                                               "TEMPSENS_CENTER",
                                               "TEMPSENS_DIG_SLDO",
                                               "RADSENS_ANA_SLDO",
                                               "RADSENS_DIG_SLDO",
                                               "RADSENS_CENTER",
                                               "POLY_ABS_TEMPSENS_TOP",
                                               "POLY_ABS_TEMPSENS_BOTTOM",
                                               "INTERNAL_NTC_ABS"};
    const unsigned int       mux_name_width = 26;

    CalibBase::prepareChipQueryForEnDis("chipSubset"); //  ??
    const float R_IMUX = 4.99;                         // kOhm, R17(ABCD) on TEPX hdis
    const float V_REF  = 0.845;                        // TEPX  84.5 k x Iref x 2.5, nominal according to RD53B manual
    // =>  the factor appearing in all current measurements, V_REF / 4096 / R_IMUX
    // is equal to  R_VREF_ADC / R_IMUX / 4096 x I_REF

    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
            {
                std::vector<unsigned int>                                         chip_ids;
                std::vector<float>                                                chip_currents;
                std::map<std::string, std::vector<std::pair<float, std::string>>> summary;
                for(const auto cChip: *cHybrid)
                {
                    LOG(INFO) << GREEN << "[RD53BMuxReader::run]  board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
                              << +cChip->getId() << RESET;
                    chip_ids.push_back(cChip->getId());
                    float Icroc  = 0;
                    float offset = 0;
                    float slope  = V_REF / 4096;

                    if(use_wlt_calibration)
                    {
                        offset = cChip->getRegItem("ADC_OFFSET_VOLT").fValue * 1e-4;                     // [V]
                        slope  = (cChip->getRegItem("ADC_MAXIMUM_VOLT").fValue * 1e-3 - offset) / 4096.; // [V/ADC];
                        if(use_wlt_calibration and (muxlist.size() > 0))
                        {
                            LOG(INFO) << "applying wlt calibration    slope = " << std::setw(8) << std::setprecision(3) << slope * 1e3 << " (mV/ADC),   offset =" << offset * 1e3 << " (mV)";
                        }
                    }

                    // override multiple sampling, local averaging with outlier rejection instead
                    const auto sampleNtimes                    = cChip->getRegItem("SAMPLE_N_TIMES").fValue; // restore later
                    cChip->getRegItem("SAMPLE_N_TIMES").fValue = 1;

                    if(do_adc_offset_measurement)
                    {
                        const auto  adc_offset_adc = measure_adc_offset(chipInterface, cChip, 2);
                        const float adc_offset_mV  = -adc_offset_adc * slope * 1e3;
                        LOG(INFO) << "adc offset measurement = " << std::setw(10) << std::setprecision(2) << adc_offset_adc << " (ADC)"
                                  << "           <->           " << adc_offset_mV << " (mV)";
                        summary["adc_offset_measured"].push_back(std::make_pair(adc_offset_mV, "mV"));
                    }

                    for(const auto& mux: muxlist)
                    {
                        const auto result = get_adc(chipInterface, cChip, mux, 12);
                        float      value  = offset + slope * result.value;

                        std::string unit = "V ";
                        if(mux == "Iref")
                        {
                            value *= 1e3 / R_IMUX;
                            unit = "uA";
                        }
                        else if((mux == "ANA_IN_CURR") || (mux == "DIG_IN_CURR"))
                        {
                            value *= 21.0 / R_IMUX; // scale factor from RD53B manual, table 27
                            unit = "A ";
                            Icroc += value;
                        }
                        else if((mux == "ANA_SHUNT_CURR") || (mux == "DIG_SHUNT_CURR"))
                        {
                            if(value > 0)
                            {
                                value *= 21.52 / R_IMUX; // scale factor from RD53B manual, table 27
                            }
                            else { value = 0; }
                            unit = "A ";
                        }
                        else if((mux == "VINA") || (mux == "VIND") || (mux == "VOFS"))
                        {
                            value *= 4;
                            unit = "V ";
                        }
                        else if((mux == "VDDA") || (mux == "VDDD"))
                        {
                            value *= 2;
                            unit = "V ";
                        }
                        else if(mux.rfind("LIN_FE_PREAMP", 0) == 0)
                        {
                            value *= 1 / R_IMUX; // factor not known to me at this point
                            unit = "A ";
                        }

                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << mux << " ADC = " << BOLDYELLOW << result.line << "  |   " << std::setw(7) << std::setprecision(1) << result.value << "   "
                                  << std::setw(9) << std::setprecision(3) << value << " " << unit << RESET;
                        summary[mux].push_back(make_pair(value, unit));

                    } // mux list

                    chip_currents.push_back(Icroc);

                    cChip->getRegItem("SAMPLE_N_TIMES").fValue = sampleNtimes; // restore the original vaue

                    if(read_temperatures) // test
                    {
                        // read poly sensor ntc style (simplified, temporary)
                        float const        R_ref              = 4.99;    // TEPX HDI
                        float const        TC_poly            = 0.22e-2; //  0.22 %/C
                        float const        R_poly_top_reftemp = cChip->getRegItem("RES_MEAS_TOP").fValue / 1e3;
                        float const        R_poly_bot_reftemp = cChip->getRegItem("RES_MEAS_BOTTOM").fValue / 1e3;
                        float const        reftemp            = cChip->getRegItem("REFTEMP").fValue;
                        const unsigned int dac_1              = 100;
                        const unsigned int dac_2              = 400;

                        chipInterface->WriteChipReg(cChip, "DAC_NTC", dac_1);
                        usleep(1000000);
                        const auto ref_adc_1  = get_adc(chipInterface, cChip, "NTC_CURR", 5).value;
                        const auto poly_top_1 = get_adc(chipInterface, cChip, "POLY_ABS_TEMPSENS_TOP", 5).value;
                        const auto rgnd_top_1 = get_adc(chipInterface, cChip, "ANA_GND_1", 5).value; // 0x14 =  20      VGNDA_RPOLYTSENS_TOP
                        const auto poly_bot_1 = get_adc(chipInterface, cChip, "POLY_ABS_TEMPSENS_BOTTOM", 5).value;
                        const auto rgnd_bot_1 = get_adc(chipInterface, cChip, "ANA_GND_0", 5).value; // 0x13 = 19 VGNDA_RPOLYTSENS_BOTTOM

                        chipInterface->WriteChipReg(cChip, "DAC_NTC", dac_2);
                        usleep(1000000);
                        const auto ref_adc_2  = get_adc(chipInterface, cChip, "NTC_CURR", 5).value;
                        const auto poly_top_2 = get_adc(chipInterface, cChip, "POLY_ABS_TEMPSENS_TOP", 5).value;
                        const auto rgnd_top_2 = get_adc(chipInterface, cChip, "ANA_GND_1", 5).value; // 0x14 =  20      VGNDA_RPOLYTSENS_TOP
                        const auto poly_bot_2 = get_adc(chipInterface, cChip, "POLY_ABS_TEMPSENS_BOTTOM", 5).value;
                        const auto rgnd_bot_2 = get_adc(chipInterface, cChip, "ANA_GND_0", 5).value; // (0x13 = 19 VGNDA_RPOLYTSENS_BOTTOM )

                        const float R_poly_top = float(poly_top_2 - poly_top_1 - rgnd_top_2 + rgnd_top_1) / float(ref_adc_2 - ref_adc_1) * R_ref;
                        const float R_poly_bot = float(poly_bot_2 - poly_bot_1 - rgnd_bot_2 + rgnd_bot_1) / float(ref_adc_2 - ref_adc_1) * R_ref;
                        const float T_poly_top = reftemp + (R_poly_top / R_poly_top_reftemp - 1.0) / TC_poly;
                        const float T_poly_bot = reftemp + (R_poly_bot / R_poly_bot_reftemp - 1.0) / TC_poly;

                        LOG(INFO) << "ref    " << ref_adc_2 << " " << ref_adc_1 << RESET;
                        LOG(INFO) << "top    " << poly_top_2 << " - " << rgnd_top_2 << "    " << poly_top_1 << " - " << rgnd_top_1 << "   R= " << std::setprecision(3) << R_poly_top
                                  << "   Rref= " << std::setprecision(3) << R_poly_top_reftemp << "   T=" << T_poly_top << " C" << RESET;
                        LOG(INFO) << "bottom " << poly_bot_2 << " - " << rgnd_bot_2 << "    " << poly_bot_1 << " - " << rgnd_bot_1 << "   R= " << std::setprecision(3) << R_poly_bot
                                  << "   Rref= " << std::setprecision(3) << R_poly_bot_reftemp << "   T=" << T_poly_bot << " C" << RESET;

                        summary["POLY_TEMPSENS_TOP_T"].push_back(std::make_pair(T_poly_top, "C "));
                        summary["POLY_TEMPSENS_BOTTOM_T"].push_back(std::make_pair(T_poly_bot, "C "));

                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "POLY_TEMPSENS_TOP_R"
                                  << "   R = " << BOLDYELLOW << std::setw(9) << std::setprecision(3) << R_poly_top << RESET;
                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "POLY_TEMPSENS_BOTTOM_R"
                                  << "   R = " << BOLDYELLOW << std::setw(9) << std::setprecision(3) << R_poly_bot << RESET;
                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "POLY_TEMPSENS_TOP_T"
                                  << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << T_poly_top << RESET;
                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "POLY_TEMPSENS_BOTTOM_T"
                                  << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << T_poly_bot << RESET;

                        chipInterface->WriteChipReg(cChip, "DAC_NTC", 100);
                    } // test

                    if(read_temperatures)
                    {
                        // all other temperature sensors
                        for(const auto& tempsens: tempsens_list)
                        {
                            if((tempsens == "POLY_TEMPSENS_TOP") || (tempsens == "POLY_TEMPSENS_BOTTOM")) continue;
                            float value = chipInterface->ReadChipMonitor(cChip, tempsens, true);
                            summary[tempsens + "_T"].push_back(std::make_pair(value, "C "));
                            LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << tempsens + "_T" << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << value << RESET;
                        }
                    }
                } // chips

                std::stringstream header, current;
                float             module_current = 0;
                for(unsigned int c = 0; c < chip_ids.size(); c++)
                {
                    header << std::setw(9) << chip_ids[c] << "   ";
                    current << std::setw(9) << std::setprecision(3) << chip_currents[c] << " A ";
                    module_current += chip_currents[c];
                }
                LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "chip id"
                          << "  | " << header.str() << RESET;
                if(module_current > 0)
                {
                    LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "chip current"
                              << "  | " << current.str() << "  sum = " << std::setw(9) << std::setprecision(3) << module_current << " A" << RESET;
                }

                if(do_adc_offset_measurement)
                {
                    std::stringstream line;
                    for(const auto& result: summary["adc_offset_measured"]) { line << std::setw(9) << std::setprecision(3) << result.first << " " << std::setw(2) << result.second; }
                    LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << "adc_offset_measured" << "  | " << line.str() << RESET;
                }

                for(const auto& mux: muxlist)
                {
                    std::stringstream line;
                    for(const auto& result: summary[mux]) { line << std::setw(9) << std::setprecision(3) << result.first << " " << std::setw(2) << result.second; }
                    LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << mux << "  | " << line.str() << RESET;
                }

                if(read_temperatures)
                {
                    for(const auto& tempsens: tempsens_list)
                    {
                        std::stringstream line;
                        for(const auto& result: summary[tempsens + "_T"]) { line << std::setw(9) << std::setprecision(3) << result.first << " " << std::setw(2) << result.second; }
                        LOG(INFO) << BOLDBLUE << std::setw(mux_name_width) << tempsens + "_T" << "  | " << line.str() << RESET;
                    }
                }
            }

    // ##################
    // # Reset sequence #
    // ##################
    LOG(INFO) << "RD53BMuxReader resetting/reconfiguring board" << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ResetBoard();
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureBoard(cBoard);
        this->ConfigureIT(cBoard);
        this->ConfigureFrontendIT(cBoard);
    }

    LOG(INFO) << "RD53BMuxReader more resetting" << RESET;

    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
    LOG(INFO) << "RD53BMuxReader done" << RESET;
}

void RD53BMuxReader::draw(bool saveData) { LOG(INFO) << GREEN << "[RD53BMuxReader::draw]" << RESET; }

void RD53BMuxReader::analyze() { LOG(INFO) << GREEN << "[RD53BMuxReader::analyze]" << RESET; }

void RD53BMuxReader::fillHisto() { LOG(INFO) << GREEN << "[RD53BMuxReader::fillHisto]" << RESET; }
