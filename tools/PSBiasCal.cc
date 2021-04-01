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
                            if(false)
                            { LOG(INFO) << BOLDMAGENTA << "\t...Will NOT set " << cReg.first << " back to original value. " << RESET; }
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
    LOG(INFO) << BOLDRED << "INIT "<< RESET;
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

uint32_t PSBiasCal::CalibrateChipBias(Chip*  cChip ,Chip* clpGBT,uint32_t point, uint32_t block, uint32_t DAC_val, float exp_val, float gnd_corr, std::string dac_str)
	{
	std::vector<std::string> nameDAC{"A", "B", "C", "D", "E", "ThDAC", "CalDAC"};


    fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX", 0x1 << block);
    fReadoutChipInterface->WriteChipReg(cChip, "TEST" + std::to_string(block), 0x1 << point);
	std::string DAC=nameDAC[point] + std::to_string(block);
 


    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    fReadoutChipInterface->WriteChipReg(cChip,DAC, 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    uint32_t off_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,dac_str);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    fReadoutChipInterface->WriteChipReg(cChip,DAC, DAC_val);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
	uint32_t act_val = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,dac_str);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

        /*LOG(INFO) << BOLDRED << "0 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC0")<< RESET;
        LOG(INFO) << BOLDRED << "1 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC1")<< RESET;
        LOG(INFO) << BOLDRED << "2 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC2")<< RESET;
        LOG(INFO) << BOLDRED << "3 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC3")<< RESET;
        LOG(INFO) << BOLDRED << "4 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC4")<< RESET;
        LOG(INFO) << BOLDRED << "5 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC5")<< RESET;
        LOG(INFO) << BOLDRED << "6 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC6")<< RESET;
        LOG(INFO) << BOLDRED << "7 " << static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,"ADC7")<< RESET;*/
	//uint32_t LSB = (act_val - off_val) / DAC_val;
    //LOG(INFO) << BOLDRED << DAC<<" act,off " << act_val<<","<<off_val<< RESET;
	LOG(INFO) << BOLDRED <<"DAC name:"<<dac_str<<" DAC_val "<<DAC_val<<" point:"<<point<<", block:"<<block<<", on value:"<< act_val<<", off value:"<<off_val<< RESET;
	uint32_t DAC_new_val = 15;//DAC_val - uint32_t(std::round((act_val - exp_val - gnd_corr)/LSB));
	DAC_new_val=std::max(uint32_t(0),DAC_new_val);
	DAC_new_val=std::min(uint32_t(31),DAC_new_val);
    fReadoutChipInterface->WriteChipReg(cChip,DAC, DAC_new_val);
	LOG(INFO) << BOLDRED <<"Done"<< RESET;
	//uint32_t new_val = static_cast<PSInterface*>(fReadoutChipInterface)->ReadADC0();
	//if ((new_val - gnd_corr < exp_val + exp_val*0.02 )&&(new_val - gnd_corr > exp_val - exp_val*0.052))
      //  LOG(INFO) << BOLDRED << "Calibration bias point " << point << " of test point " << block << " --> Done (" << new_val << "V for " <<   DAC_new_val << " DAC)"<< RESET;
	//else
      //  LOG(INFO) << BOLDRED << "Calibration bias point " << point << " of test point " << block << " --> Failed (" << new_val << "V for " << DAC_new_val << " DAC)"<< RESET;
	return DAC_new_val;
	}

void PSBiasCal::DisableTest(Chip* cChip)
	{
    if(cChip->getFrontEndType() == FrontEndType::MPA)
		{
    	LOG(INFO) << BOLDRED << "MPADisable "<< RESET;
		
		
			fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST0",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST1",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST2",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST3",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST4",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST5",0x0);
			fReadoutChipInterface->WriteChipReg(cChip, "TEST6",0x0);
		}
    if(cChip->getFrontEndType() == FrontEndType::SSA)
		{
    	LOG(INFO) << BOLDRED << "SSADisable "<< RESET;
    	fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB",0x0);
    	fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB",0x8);
		}
	}

float PSBiasCal::MeasureGnd(Chip* cChip, Chip* clpGBT,std::string dac_str)
	{
	DisableTest(cChip);
    std::vector<float> data(7,0);
	for(int iblock = 0; iblock < 7; iblock++)
		{
		std::string test = "TEST" + std::to_string(iblock);
    	fReadoutChipInterface->WriteChipReg(cChip, "TESTMUX",(0x1 << iblock));
    	fReadoutChipInterface->WriteChipReg(cChip, test,(0x1 << 7));

    	data[iblock] = static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(clpGBT,dac_str);

        LOG(INFO) << BOLDRED << "gndval " << data[iblock]<< RESET;
		}
	DisableTest(cChip);
	return accumulate( data.begin(), data.end(), 0.0) / data.size(); 
	}




void PSBiasCal::CalibrateBias(BeBoard* pBoard)
{
    LOG(INFO) << BOLDRED << "START "<< RESET;
    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid) 
            {
				DisableTest(cChip);
            } // chip
        }     // hybrid
    }         // optica]l group



    for(auto cOpticalReadout: *pBoard)
    {
        for(auto cHybrid: *cOpticalReadout)
        {
            for(auto cChip: *cHybrid) 
            {
				std::string dac_str="ADC3";
                if (cHybrid->getId()==1) dac_str="ADC0";
                if(cChip->getFrontEndType() == FrontEndType::SSA)
                {


               

					LOG(INFO) << BOLDRED << "SSA "<<+(cChip->getId())<<" Hyb "<<+(cHybrid->getId()) <<RESET;
					for(uint32_t iMUX = 0; iMUX < 12; iMUX++)
						{
						uint32_t MtoWr=(1<<iMUX);
						fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_LSB",MtoWr&0xff);
						fReadoutChipInterface->WriteChipReg(cChip, "Bias_TEST_MSB",MtoWr>>8);
						LOG(INFO) << BOLDRED <<dac_str<<" "<<iMUX<<" "<< static_cast<D19clpGBTInterface*>(flpGBTInterface)->ReadADC(cOpticalReadout->flpGBT,dac_str)<< RESET;
						}
					DisableTest(cChip);

				}
                if(cChip->getFrontEndType() == FrontEndType::MPA)
                {
    				LOG(INFO) << BOLDRED << "MPAMG "<< RESET;
					float gndval = MeasureGnd(cChip, cOpticalReadout->flpGBT, dac_str);
					std::vector<uint32_t> DAC_val{0xF, 0xF, 0xF, 0xF, 0xF, 0xFF, 0xFF};
					std::vector<float> exp_val{0.082, 0.082, 0.108, 0.082, 0.082,1.0,1.0};
					//std::vector<std::vector<uint32_t>> data{7,std::vector<uint32_t>(5,0)};
					for(int ipoint = 0; ipoint < 7; ipoint++)
					{
						for(int iblock = 0; iblock < 7; iblock++)
							{
        					LOG(INFO) << BOLDRED << ipoint<<","<<iblock<< RESET;
        					LOG(INFO) << BOLDRED << CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, iblock, DAC_val[ipoint], exp_val[ipoint],  gndval, dac_str)<< RESET;
							//data[ipoint][iblock] = CalibrateChipBias(cChip, cOpticalReadout->flpGBT, ipoint, iblock, DAC_val[ipoint], exp_val[ipoint],  gndval, dac_str);

							}
					}
                }

            } // chip
        }     // hybrid
    }         // optica]l group
}
void PSBiasCal::writeObjects() {}
// State machine control functions
void PSBiasCal::Running()
{
}

void PSBiasCal::Stop()
{
    dumpConfigFiles();

    // Destroy();
}

void PSBiasCal::Pause() {}

void PSBiasCal::Resume() {}
