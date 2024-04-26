/*!
 *
 * \file OTCICphaseAlignmentForBypass.h
 * \brief OTCICphaseAlignmentForBypass class
 * \author Fabio Ravera
 * \date 22/03/24
 *
 */

#ifndef OTCICphaseAlignmentForBypass_h__
#define OTCICphaseAlignmentForBypass_h__

#include "tools/OTCICphaseAlignment.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTCICphaseAlignmentForBypass.h"
#endif

class OTCICphaseAlignmentForBypass : public OTCICphaseAlignment
{
  public:
    OTCICphaseAlignmentForBypass();
    ~OTCICphaseAlignmentForBypass();

    void Initialise(void) override;

    static std::string fCalibrationDescription;

  private:
};

#endif
