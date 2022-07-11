/*!
 *
 * \file CheckCbcNeighbors.h
 * \brief CheckCbcNeighbors class
 * \author Lesya Horyn
 * \date 11/07/22
 *
 */

#ifndef CheckCbcNeighbors_h__
#define CheckCbcNeighbors_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "../DQMUtils/DQMHistogramCheckCbcNeighbors.h"
#endif

class CheckCbcNeighbors : public Tool
{
  public:
    CheckCbcNeighbors();
    ~CheckCbcNeighbors();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

  private:
  //
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramCheckCbcNeighbors fDQMHistogramCheckCbcNeighbors;
#endif
};

#endif
