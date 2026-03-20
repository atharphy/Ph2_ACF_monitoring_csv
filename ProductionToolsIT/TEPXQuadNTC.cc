/*!
  \file                  TEPXQuadNTC.cc
  \brief                 Read out TEPX Quad NTC
  \author                PSI
  \version               1.0
  \date                  04/03/24
  Support:               none
*/

#include "TEPXQuadNTC.h"
#include "Utils/ContainerSerialization.h"
#include <vector>

using namespace Ph2_HwInterface;

// define function for Linear Regression with least mean squares method
std::pair<double, double> LinReg(const std::vector<double>& x, const std::vector<double>& y, bool verbose = false)
{
    int    n = x.size();
    int    i;
    double x_sum = 0, x2_sum = 0, y_sum = 0, xy_sum = 0;

    // iterating through x and y
    for(i = 0; i < n; i++)
    {
        x_sum += x[i];
        y_sum += y[i];
        x2_sum += pow(x[i], 2);
        xy_sum += x[i] * y[i];
    }

    double slope, intercept;
    slope     = (n * xy_sum - x_sum * y_sum) / (n * x2_sum - x_sum * x_sum);
    intercept = (x2_sum * y_sum - x_sum * xy_sum) / (x2_sum * n - x_sum * x_sum);
    return std::make_pair(slope, intercept);
}

float ntc_funct(double voltage, double current)
{
    const float T0C         = 273.15; // [Kelvin]
    const float T25C        = 298.15; // [Kelvin]
    const float R25C        = 10;     // [kOhm]
    const int   beta        = 3435;
    float       resistance  = 1e3 * voltage / current;                                // [kOhm]
    float       temperature = 1. / (1. / T25C + log(resistance / R25C) / beta) - T0C; // [Celsius]
    return temperature;
}

void TEPXQuadNTC::ConfigureCalibration() { LOG(INFO) << GREEN << "[TEPXQuadNTC::ConfigureCalibration]" << RESET; }

void TEPXQuadNTC::Running()
{
    theCurrentRun = this->fRunNumber;
    LOG(INFO) << GREEN << "[TEPXQuadNTC::Running] Starting run: " << BOLDYELLOW << theCurrentRun << RESET;

    TEPXQuadNTC::run();
    // TEPXQuadNTC::analyze();
    // TEPXQuadNTC::sendData();
}

void TEPXQuadNTC::sendData() {}

void TEPXQuadNTC::Stop()
{
    LOG(INFO) << GREEN << "[TEPXQuadNTC::Stop] Stopping" << RESET;

    Tool::Stop();

    TEPXQuadNTC::draw();
    // this->closeFileHandler();

    RD53RunProgress::reset();
}

void TEPXQuadNTC::localConfigure(const std::string& histoFileName, int currentRun) { LOG(INFO) << GREEN << "[TEPXQuadNTC::localConfigure] Starting run: " << BOLDYELLOW << theCurrentRun << RESET; }


float TEPXQuadNTC::TfromR(float RkOhm, const float& R25C, const float& beta)
{
    const float T0C  = 273.15; // [Kelvin]
    const float T25C = 298.15; // [Kelvin]

    if((abs(R25C - 10.) < 0.001) && (abs(beta - 3380.) < 0.1))
    {
        // Murata NTHCG83, use fit to vendor data
        double y  = RkOhm - R25C;
        float  RB = 10.0 + 1.01884 * y + 0.00238757 * pow(y, 2) - 1.5062e-05 * pow(y, 3) + 5.02968e-08 * pow(y, 4);
        return 1. / (1. / T25C + log(RB / R25C) / beta) - T0C; // [Celsius]
    }
    else
    {
        return 1. / (1. / T25C + log(RkOhm / R25C) / beta) - T0C; // [Celsius]
    }
}

void TEPXQuadNTC::run()
{
    fVerbose           = true;
    auto chipInterface = static_cast<RD53Interface*>(this->fReadoutChipInterface);

    CalibBase::prepareChipQueryForEnDis("chipSubset"); //  ??
    for(const auto cBoard: *fDetectorContainer)
        for(const auto cOpticalGroup: *cBoard)
            for(const auto cHybrid: *cOpticalGroup)
            {
                std::map<int, double> ntc_temp_from_slope_map;
                std::map<int, float> ntc_temperature_map;
                std::map<int, float> top_temperature_map;
                std::map<int, float> bot_temperature_map;
                for(const auto cChip: *cHybrid)
                {
		  float const        TC_poly            = 0.22e-2; //  0.22 %/C
		  float const        R_poly_top_reftemp = cChip->getRegItem("RES_MEAS_TOP").fValue / 1e3;
		  float const        R_poly_bot_reftemp = cChip->getRegItem("RES_MEAS_BOTTOM").fValue / 1e3;
		  float const        reftemp            = cChip->getRegItem("REFTEMP").fValue;
		  float const R25NTC = cChip->getRegItem("RNTCAT25C").fValue / 1000.;
		  float beta   = cChip->getRegItem("NTCBETA").fValue;
		  LOG(INFO) << GREEN << "[TEPXQuadNTC::run]  board/opticalGroup/hybrid/chip = " << BOLDYELLOW << cBoard->getId() << "/" << cOpticalGroup->getId() << "/" << cHybrid->getId() << "/"
			    << +cChip->getId() << "   R25NTC=" << R25NTC << "   NTCBETA=" << beta << RESET;
		  if(!((abs(R25NTC - 10.) < 0.001) && (abs(beta - 3380.) < 0.1)) && !((abs(R25NTC - 1.5) < 0.001) && (abs(beta - 3500.) < 0.1)))
                    {
		      LOG(WARNING) << RED << "[TEPXQuadNTC::run]  NTC parameters, "
				   << "   R25NTC=" << R25NTC << "   NTCBETA=" << beta << ",  do not correspond to a known NTC type." << RESET;
                    }
		  // raw ADC: for a list of "observables" see RD53BInterface::getADCobservable  in HWInterface/RD53BInterface.cc
		  unsigned int adc_max_lin = 2048;    // nonlinearities increase above this adc value
		  unsigned int adc_min = 500;         // try to reach this adc_value
		  unsigned int dac_max_lin = 350;     // nonlinearities above this dac value, stop here if adc_min has been reached
		  unsigned int dac_max = 850;         // never go above thisinitail
		  unsigned int dac_step  = 50;
		  
		  std::vector<std::string> adc_names = {"NTC_CURR", "NTC_VOLT", };
		  if (fReadPoly){
		    adc_names.push_back("POLY_ABS_TEMPSENS_TOP");
		    adc_names.push_back("POLY_ABS_TEMPSENS_BOTTOM");
		  }
		  
		  // adc_names are the inner loop to minimize changing the dac , keep all values
		  std::map<std::string, std::vector<double>> adc_lists;
		  std::map<std::string, std::vector<double>> dac_lists;
		  for (const auto & adc_name : adc_names)
		    {
		      adc_lists[adc_name] = std::vector<double>();
		      dac_lists[adc_name] = std::vector<double>();
		    }
		  if(fReadPoly) LOG(INFO) << "  dac    ref    ntc    top bottom" << RESET;  
		  // acquire data
		  for(unsigned int dac = dac_step; dac <= dac_max; dac += dac_step)
		    {
		      std::stringstream  line;
		      if(fReadPoly) line << std::setw(5) << dac;
		      chipInterface->WriteChipReg(cChip, "DAC_NTC", dac);
		      usleep(100000);
		      for (const auto & adc_name : adc_names)
			{
			  const auto last_adc_value = adc_lists[adc_name].size() > 0 ? adc_lists[adc_name].back() : 0;
			  if ( (adc_lists[adc_name].size() < 3) || ((dac <= dac_max_lin) && (last_adc_value < adc_max_lin)) ||  (last_adc_value <= adc_min))
			    {
			      const auto adc = chipInterface->ReadChipADC(cChip, adc_name);
			      if (adc > 0)
				{
				  adc_lists[adc_name].push_back(adc);
				  dac_lists[adc_name].push_back(dac);
				  if(fReadPoly) line << "," << std::setw(6) << adc;
				}
			      else{
				if(fReadPoly) line << ",      ";
			      }
			    }
			  else
			    {
			      if(fReadPoly) line << ",      ";
			    }
			}
		      if(fReadPoly) LOG(INFO)  << line.str() << RESET;
		    }


		  // determine slope, temperatures, the refrence , NTC_CURR, must be the first in the list of adc_names
		  float slope_to_R = 0;
		  for(const auto & adc_name : adc_names)
		    {
		      float slope = 0;
		      if (adc_lists[adc_name].size() < 2)
			{
			  LOG(INFO) << "Unable to acquire slope for " << adc_name << RESET;
			  slope = 0;
			}
		      else
			{
			  const auto fit = LinReg(dac_lists[adc_name], adc_lists[adc_name]);
			  slope = fit.first;
			}

		      if (adc_name == "NTC_CURR")
			{
			  if (slope > 0)  slope_to_R = fR_ref / slope;
			}
		      else if (adc_name == "NTC_VOLT")
			{
			  const auto Rntc = slope * slope_to_R;
			  ntc_temperature_map[cChip->getId()] = TfromR(Rntc, R25NTC, beta);
			}
		      else if(adc_name == "POLY_ABS_TEMPSENS_TOP")
			{
			  const float R_poly_top = slope * slope_to_R;
			  const float T_poly_top = reftemp + (R_poly_top / R_poly_top_reftemp - 1.0) / TC_poly;
			  top_temperature_map[cChip->getId()] = T_poly_top;
			}
		      else if(adc_name == "POLY_ABS_TEMPSENS_BOTTOM")
			{
			  const float R_poly_bot = slope * slope_to_R;
			  const float T_poly_bot = reftemp + (R_poly_bot / R_poly_bot_reftemp - 1.0) / TC_poly;
			  bot_temperature_map[cChip->getId()] = T_poly_bot;
			}
		    } 

		  chipInterface->WriteChipReg(cChip, "DAC_NTC", 100);
		    
		}// chip
		
                LOG(INFO) << "NTC result for ChipID 15-12  (C)   : " << std::setw(8) << std::setprecision(1) << std::fixed << ntc_temperature_map[15] << std::setw(8) << std::setprecision(1)
                          << std::fixed << ntc_temperature_map[14] << std::setw(8) << std::setprecision(1) << std::fixed << ntc_temperature_map[13] << std::setw(8) << std::setprecision(1)
                          << std::fixed << ntc_temperature_map[12];
		if (fReadPoly){
		  LOG(INFO) << "BOT result for ChipID 15-12  (C)   : " << std::setw(8) << std::setprecision(1) << std::fixed << bot_temperature_map[15] << std::setw(8) << std::setprecision(1)
			    << std::fixed << bot_temperature_map[14] << std::setw(8) << std::setprecision(1) << std::fixed << bot_temperature_map[13] << std::setw(8) << std::setprecision(1)
			    << std::fixed << bot_temperature_map[12];
		  LOG(INFO) << "TOP result for ChipID 15-12  (C)   : " << std::setw(8) << std::setprecision(1) << std::fixed << top_temperature_map[15] << std::setw(8) << std::setprecision(1)
			    << std::fixed << top_temperature_map[14] << std::setw(8) << std::setprecision(1) << std::fixed << top_temperature_map[13] << std::setw(8) << std::setprecision(1)
			    << std::fixed << top_temperature_map[12];
		}
            }
    // ##################
    // # Reset sequence #
    // ##################
    LOG(INFO) << "TEPXQuadNTC resetting/reconfiguring board" << RESET;
    for(const auto cBoard: *fDetectorContainer)
    {
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ResetBoard();
        static_cast<RD53FWInterface*>(this->fBeBoardFWMap[cBoard->getId()])->ConfigureBoard(cBoard);
        this->ConfigureIT(cBoard);
        this->ConfigureFrontendIT(cBoard); // spits out an error about HITOR_2_CNT readback
    }
    fDetectorContainer->resetReadoutChipQueryFunction();
    fDetectorContainer->setEnabledAll(true);
    LOG(INFO) << "TEPXQuadNTC done" << RESET;
}

void TEPXQuadNTC::draw(bool saveData) { LOG(INFO) << GREEN << "[TEPXQuadNTC::draw]" << RESET; }

void TEPXQuadNTC::analyze() { LOG(INFO) << GREEN << "[TEPXQuadNTC::analyze]" << RESET; }

void TEPXQuadNTC::fillHisto() { LOG(INFO) << GREEN << "[TEPXQuadNTC::fillHisto]" << RESET; }
