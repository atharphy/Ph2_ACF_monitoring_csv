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

//float RD53BMuxReader::get_adc(
RD53BMuxReader::adc_result RD53BMuxReader::get_adc(
			      Ph2_HwInterface::RD53Interface* chipInterface,
			      Ph2_HwDescription::ReadoutChip* chip,
			      const std::string & name,
			      size_t nsample
			      )
{
  unsigned int sum_adc = 0;
  std::stringstream line;
  unsigned int n =0;
  std::vector<float> adc_values;
  for(unsigned int n = 0; n < nsample; n++){
    const auto value = chipInterface->ReadChipADC(chip, name);
    if (value <  4096){
      sum_adc += value;
      line << std::fixed << std::setw(5) << value;
      adc_values.push_back(value);
    }else{
      line << RED << std::fixed << std::setw(5) << value << YELLOW;
    }
  }
  // basic outlier rejection
  float sum=0, rms=0;
  unsigned int n_valid = 0;
  for(const auto v : adc_values){
    if (abs(v-adc_mean) < 10){
      sum += v;
      var += pow(v-adc_mean, 2);
      n_valid++;
    }
  }
  
  if (n_valid > 0){
    return {float(sum)/n_valid, sqrt(var)/n_valid, line.str()};
  }else{
    return {0,0,""};
  }
}

void RD53BMuxReader::configure(const std::string & args){
  // see HWInterface/RD53BInterface.cc
    std::vector<std::string> all = {
      "Iref", "NTC_VOLT", "NTC_CURR",
      "ANA_IN_CURR", "ANA_SHUNT_CURR", "DIG_IN_CURR", "DIG_SHUNT_CURR",
      "VIND", "VINA","VDDD", "VDDA","VOFS","VrefA","VrefD","Vref_CORE", "Vref_PRE",
      "POLY_TEMPSENS_TOP", "POLY_TEMPSENS_BOTTOM",
      "TEMPSENS_ANA_SLDO", "TEMPSENS_DIG_SLDO", "TEMPSENS_CENTER", "RADSENS_ANA_SLDO", "RADSENS_DIG_SLDO", "RADSENS_CENTER",
      "VCAL_HI", "VCAL_MED", "LIN_FE_REF_KRUMCURR", "LIN_FE_GDAC_MAIN","LIN_FE_GDAC_LEFT", "LIN_FE_GDAC_RIGHT",
      "LIN_FE_PREAMP_MAIN", "LIN_FE_PREAMP_LEFT", "LIN_FE_PREAMP_RIGHT", "LIN_FE_PREAMP_TOP_LEFT", "LIN_FE_PREAMP_TOP", "LIN_FE_PREAMP_TOP_RIGHT",
      "ANA_GND_0", "ANA_GND_1",	"ANA_GND_2", "ANA_GND_3", "ANA_GND_4", "ANA_GND_5", "ANA_GND_6", "ANA_GND_7", "ANA_GND_8", "ANA_GND_9", "ANA_GND_10", "ANA_GND_11",
      "HIGH_Z"
    };
    std::vector<std::string> default_adc= {
      "VIND", "VINA","VDDD", "VDDA","VOFS",
      "ANA_IN_CURR", "ANA_SHUNT_CURR", "DIG_IN_CURR", "DIG_SHUNT_CURR"
    };

    muxlist.clear();
    use_wlt_calibration = true;
    read_temperatures = false;    // override by adding 'temperatures' to the adc list
    
    if (args == ""){
      for(size_t i=0; i< default_adc.size(); i++){
	muxlist.push_back(default_adc[i]);
      }
      return;
    }
    
    // comma separated string, split
    std::vector<std::string> v;
    std::stringstream ss(args);

    while (ss.good()) {
      std::string arg;
       getline(ss, arg, ',');
       if (arg == "default"){
	 for(size_t i=0; i< default_adc.size(); i++){
	   muxlist.push_back(default_adc[i]);
	 }
       }else if (arg=="no_wlt_calibration"){
	 use_wlt_calibration = false;
       }else if (arg=="temperatures"){
	 read_temperatures = true;
       }else if (std::find(all.begin(), all.end(), arg) != all.end()){
	   muxlist.push_back(arg);
       }else{
	 //possible wildcard
	 bool found_match = false;
	 const auto pattern = std::regex_replace(arg, std::regex(R"(\*)"), ".*");
	 regex re("^" + pattern + "$");
	 for(size_t i=0; i< all.size(); i++){
	   const auto  adc = all[i];
	   if (std::regex_match(adc, re)){
	     muxlist.push_back(adc);
	     found_match = true;
	   }
	 }
	 if (!found_match){
	   LOG(WARNING) << RED << "[RD53BMuxReader::configure]  unknown adc " << arg << RESET;
	 }
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

    std::vector<std::string>  tempsens_list = {
      "TEMPSENS_ANA_SLDO", "TEMPSENS_CENTER", "TEMPSENS_DIG_SLDO",
      "RADSENS_ANA_SLDO", "RADSENS_DIG_SLDO", "RADSENS_CENTER",
      "POLY_TEMPSENS_TOP", "POLY_TEMPSENS_BOTTOM", "INTERNAL_NTC_ABS"
    };
    
    CalibBase::prepareChipQueryForEnDis("chipSubset"); //  ??
    const float              R_IMUX  = 4.99;  // kOhm, R17(ABCD) on TEPX hdis
    const float              V_REF   = 0.845; // TEPX  84.5 k x Iref x 2.5, nominal according to RD53B manual
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
                    float Icroc = 0;
		    
		    float offset = 0;
		    float slope  = V_REF / 4096;
		    
		    if (use_wlt_calibration){
		      offset = cChip->getRegItem("ADC_OFFSET_VOLT").fValue * 1e-4; // [V]
		      slope = (cChip->getRegItem("ADC_MAXIMUM_VOLT").fValue * 1e-3 -  offset) / 4096.; // [V/ADC];
		      if(use_wlt_calibration and (muxlist.size()>0)){
			LOG(INFO) << "applying wlt calibration    slope = " << std::setw(8) << std::setprecision(3) << slope*1e3 << " (mV/ADC),   offset =" << offset*1e3 << " (mV)";
		      }

		    // esitmate the adc offset assuming the ntc current dac has no offset (i.e. dac =0 means current = 0)
		    float x_sum = 0, y_sum = 0, x2_sum = 0, xy_sum = 0;
		    unsigned int n = 0;
		    for(unsigned int idac = 0; idac < 50; idac += 10)
		      {
                        chipInterface->WriteChipReg(cChip, "DAC_NTC", idac);
                        const auto adc = chipInterface->ReadChipADC(cChip, "NTC_CURR");
                        if((adc > 0) && (adc < 4096))
			  {
			    n += 1;
                            x_sum += idac;
                            y_sum += adc;
                            x2_sum += idac * idac;
                            xy_sum += adc * idac;
			  }
		      }
		    float test_offset_adc = (x2_sum * y_sum - x_sum * xy_sum) / (x2_sum * n - x_sum * x_sum);
		    LOG(INFO) << "adc offset measurement = " << std::setw(10) << std::setprecision(2) << test_offset_adc <<  " (ADC)"
			      << "           <->           " << -test_offset_adc * slope * 1e3 << " (mV)";
		      
		    // "standard" value for the current dac, affects NTC and POLY_SENS
		    chipInterface->WriteChipReg(cChip, "DAC_NTC", 100);
		    
                    // raw ADC: for a list of "observables" see RD53BInterface::getADCobservable  in HWInterface/RD53BInterface.cc
                    const auto sampleNtimes                    = cChip->getRegItem("SAMPLE_N_TIMES").fValue;
                    cChip->getRegItem("SAMPLE_N_TIMES").fValue = 1; // local averaging
		    
                    for(const auto& mux: muxlist)
                    {
			/*
                        uint32_t           adc_sum       = 0;
                        const unsigned int n_measurement = 12; // TODO configurable
                        unsigned int       n_valid       = 0;
                        std::stringstream  line;
                        // get the mean value of n_measurement tries
                        std::vector<float> adc_values;
                        for(unsigned int n = 0; n < n_measurement; n++)
                        {
                            const auto value = chipInterface->ReadChipADC(cChip, mux);
                            if(value < 4096)
                            {
                                line << std::fixed << std::setw(5) << value;
                                adc_values.push_back(value);
                                adc_sum += value;
                                n_valid += 1;
                            }
                            else { line << RED << std::fixed << std::setw(5) << value << YELLOW; }
                        }

                        float adc_mean = 0;
                        if(n_valid > 0)
                        {
                            adc_mean = float(adc_sum) / n_valid;
                            // further outlier rejection
                            for(float T = 32; T > 0.9; T /= 2)
                            {
                                float sum_w = 0, sum_wadc = 0;
                                for(const auto& adc: adc_values)
                                {
                                    float q = 0.5 * pow((adc - adc_mean) / (4 * T), 2);
                                    if(q < 10)
                                    {
                                        float w = 1. / (1. + exp(q));
                                        sum_w += w;
                                        sum_wadc += adc * w;
                                    }
                                }

                                if(sum_w > 0) { adc_mean = sum_wadc / sum_w; }
                                else
                                {
                                    line << RED << " no valid mean found" << YELLOW;
                                    break;
                                }
                            }
                        }
			*/
			  
		        const auto result = read_adc(chipInterface, cChip, mux, 12);
			float value = result.value * slope + offset;
			float value = adc_mean * slope + offset;
                        std::string unit = "V ";
                        if(mux == "Iref")
                        {
			  value *= 1e3 / R_IMUX;
			  unit  = "uA";
                        }
                        else if((mux == "ANA_IN_CURR") || (mux == "DIG_IN_CURR"))
                        {
			  value *= 21.0 / R_IMUX;
			  unit  = "A ";
			  if (value > 0){
			    Icroc += value;
			  }
                        }
                        else if((mux == "ANA_SHUNT_CURR") || (mux == "DIG_SHUNT_CURR"))
                        {
			  value *= 21.52 / R_IMUX;
			  unit = "A ";
                        }
                        else if((mux == "VINA") || (mux == "VIND") || (mux == "VOFS"))
                        {
			  value *= 4;
			  unit  = "V ";
                        }
                        else if( (mux == "VDDA") || (mux == "VDDD"))
                        {
			  value *= 2;
			  unit  = "V ";
                        }
                        else if(mux.rfind("LIN_FE_PREAMP", 0) == 0)
                        {
			  value *= 1/R_IMUX;  // factor not known to me at this point
			  unit  = "A "; 
                        }
			
			  //LOG(INFO) << BOLDBLUE << std::setw(24) << mux << " ADC = " << BOLDYELLOW << line.str() << "  |   " << std::setw(7) << std::setprecision(1) << adc_mean << "   " << std::setw(9)
                          //        << std::setprecision(3) << value << " " << unit << RESET;
                        LOG(INFO) << BOLDBLUE << std::setw(24) << mux << " ADC = " << BOLDYELLOW << result.lline << "  |   " << std::setw(7) << std::setprecision(1) << result.value << "   " << std::setw(9)
                                  << std::setprecision(3) << value << " " << unit << RESET;
                        summary[mux].push_back(make_pair(value, unit));

                    } // mux list
                    chip_currents.push_back(Icroc);
                    cChip->getRegItem("SAMPLE_N_TIMES").fValue = sampleNtimes; // restore the original vaue

		    if (read_temperatures){
		      {
			// read poly sensor ntc style (simplified, temporary)
			float const R_ref = 4.99; // TEPX HDI
			float const R_poly_27C = 11.07; //10.8;    //  kOhm : nominal, should come from WLT
			float const TC_poly = 0.22e-2;     //  0.22 %/C  
			// R(T) = R(27 C) * (1+ TC * (T-27))
			// R(T)/R(27C) = 1  +  TC * (T-27)
			// T = 27 + (R(T)/R(27C) -1 )/ TC
			const unsigned int dac_1 = 100;
			const unsigned int dac_2 = 400;

			chipInterface->WriteChipReg(cChip, "DAC_NTC", dac_1);
			usleep(1000000);
			const auto ref_adc_1  = get_adc(chipInterface, cChip, "NTC_CURR", 5).value;
			const auto poly_top_1 = get_adc(chipInterface, cChip, "POLY_TEMPSENS_TOP", 5).value;
			const auto rgnd_top_1 = get_adc(chipInterface, cChip, "ANA_GND_1", 5).value;// 0x14 =  20      VGNDA_RPOLYTSENS_TOP
			const auto poly_bot_1 = get_adc(chipInterface, cChip, "POLY_TEMPSENS_BOTTOM", 5).value;
			const auto rgnd_bot_1 = get_adc(chipInterface, cChip, "ANA_GND_0", 5).value; // 0x13 = 19 VGNDA_RPOLYTSENS_BOTTOM 
			
			chipInterface->WriteChipReg(cChip, "DAC_NTC", dac_2);
			usleep(1000000);
			const auto ref_adc_2  = get_adc(chipInterface, cChip, "NTC_CURR", 5).value;
			const auto poly_top_2 = get_adc(chipInterface, cChip, "POLY_TEMPSENS_TOP", 5).value;
			const auto rgnd_top_2 = get_adc(chipInterface, cChip, "ANA_GND_1", 5).value;// 0x14 =  20      VGNDA_RPOLYTSENS_TOP
			const auto poly_bot_2 = get_adc(chipInterface, cChip, "POLY_TEMPSENS_BOTTOM", 5).value;
			const auto rgnd_bot_2 = get_adc(chipInterface, cChip, "ANA_GND_0", 5).value; // (0x13 = 19 VGNDA_RPOLYTSENS_BOTTOM )

			const float R_poly_top = float(poly_top_2 - poly_top_1 - rgnd_top_2 + rgnd_top_1) / float(ref_adc_2 - ref_adc_1) * R_ref;
			const float R_poly_bot = float(poly_bot_2 - poly_bot_1 - rgnd_bot_2 + rgnd_bot_1) / float(ref_adc_2 - ref_adc_1) * R_ref;
			const float T_poly_top = 27.0 + (R_poly_top / R_poly_27C - 1.0) / TC_poly;
			const float T_poly_bot = 27.0 + (R_poly_bot / R_poly_27C - 1.0) / TC_poly;

			LOG(INFO) << "ref    " << ref_adc_2  << " " << ref_adc_1  << RESET;
			LOG(INFO) << "top    " << poly_top_2 << " - " << rgnd_top_2 << "    " << poly_top_1  << " - " << rgnd_top_1 << "   " << R_poly_top << "   " << T_poly_top << " C" << RESET;
			LOG(INFO) << "bottom " << poly_bot_2  << " - " << rgnd_bot_2 << "    " << poly_bot_1   << " - " << rgnd_bot_1  << "   " << R_poly_bot << "   " << T_poly_bot << " C" << RESET;
			
			summary["POLY_TEMPSENS_TOP_T"].push_back(std::make_pair(T_poly_top, "C "));
			summary["POLY_TEMPSENS_BOTTOM_T"].push_back(std::make_pair(T_poly_bot, "C "));

			LOG(INFO) << BOLDBLUE << std::setw(24) << "POLY_TEMPSENS_TOP_R"
				  << "   R = " << BOLDYELLOW << std::setw(9) << std::setprecision(3) << R_poly_top << RESET;
			LOG(INFO) << BOLDBLUE << std::setw(24) << "POLY_TEMPSENS_BOTTOM_R"
				  << "   R = " << BOLDYELLOW << std::setw(9) << std::setprecision(3) << R_poly_bot << RESET;
			LOG(INFO) << BOLDBLUE << std::setw(24) << "POLY_TEMPSENS_TOP_T"
				  << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << T_poly_top << RESET;
			LOG(INFO) << BOLDBLUE << std::setw(24) << "POLY_TEMPSENS_BOTTOM_T"
				  << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << T_poly_bot << RESET;
			
			chipInterface->WriteChipReg(cChip, "DAC_NTC", 100);
		      }

		      
		      // all other temperature sensors
		      for(const auto& tempsens: tempsens_list){
			if ((tempsens == "POLY_TEMPSENS_TOP") || (tempsens == "POLY_TEMPSENS_BOTTOM")) continue;
			float value = chipInterface->ReadChipMonitor(cChip, tempsens, true);
			summary[tempsens+"_T"].push_back(std::make_pair(value, "C "));
			LOG(INFO) << BOLDBLUE << std::setw(24) << tempsens+"_T" << "   T = " << BOLDYELLOW << std::setw(7) << std::setprecision(1) << value << RESET;
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
                LOG(INFO) << BOLDBLUE << std::setw(24) << "chip id"
                          << "  | " << header.str() << RESET;
		if (module_current > 0){
		  LOG(INFO) << BOLDBLUE << std::setw(24) << "chip current"
			    << "  | " << current.str() << "  sum = " << std::setw(9) << std::setprecision(3) << module_current << " A" << RESET;
		}
                for(const auto& mux: muxlist)
                {
                    std::stringstream line;
                    for(const auto& result: summary[mux]) { line << std::setw(9) << std::setprecision(3) << result.first << " " << std::setw(2) << result.second; }
                    LOG(INFO) << BOLDBLUE << std::setw(24) << mux << "  | " << line.str() << RESET;
                }
		if (read_temperatures){
		  for(const auto& tempsens: tempsens_list)
		    {
		      std::stringstream line;
		      for(const auto& result: summary[tempsens + "_T"]) { line << std::setw(9) << std::setprecision(3) << result.first << " " << std::setw(2) << result.second; }
                    LOG(INFO) << BOLDBLUE << std::setw(24) << tempsens+"_T" << "  | " << line.str() << RESET;
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
