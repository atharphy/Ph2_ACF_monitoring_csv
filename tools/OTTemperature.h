/*!
 *
 * \file LinkAlignment.h
 * \brief Link alignment class, automated alignment procedure for CIC-lpGBT-BE
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef OTTemperature_h__
#define OTTemperature_h__

#include "OTTool.h"

#ifdef __USE_ROOT__
#endif


using namespace Ph2_HwDescription;

class OTTemperature : public OTTool
{
  public:
    OTTemperature();
    ~OTTemperature();

    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    // void Reset();
    void writeObjects();
    // configure setttings for reading 
    void SetGain(uint8_t pGain){ fGain = pGain; }
    void SetVref(float pVref){ fVref = pVref; }
    void Set2SInputVoltage(float pInput){ fVinput2S = pInput; }
    
  protected:
    
  private:
    float ReadThermistor(const Ph2_HwDescription::OpticalGroup* pOpticalGroup, std::string pADC, float pR0, float pB ) ;
    void ReadModuleTemperatures(); 
    float fVref{0.87};// reference voltage for lpgBT 
    uint8_t fGain{0};// gain 
    float fVinput2S{10.4};// input voltage to 2S-SEH
    std::vector<uint8_t> fCurrentDACs{0x01, 0x02 , 0x03 , 0x04, 0x05 , 0x07, 0x10, 0x12 , 0x15};
            
};
#endif
