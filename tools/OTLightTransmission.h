#ifndef OTLightTransmission_h__
#define OTLightTransmission_h__

#include "OTTool.h"

#ifdef __USE_ROOT__
#endif

using namespace Ph2_HwDescription;

class OTLightTransmission : public OTTool
{
  public:
    OTLightTransmission();
    ~OTLightTransmission();

    void Initialise();
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;

    static std::string fCalibrationDescription;

    void ReadRegisters();
};
#endif
