/*!
 *
 * \file OTalignBoardDataWord.h
 * \brief OTalignBoardDataWord class
 * \author Fabio Ravera
 * \date 19/01/24
 *
 */

#ifndef OTalignBoardDataWord_h__
#define OTalignBoardDataWord_h__

#include "Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histrgrammer here
#include "DQMUtils/DQMHistogramOTalignBoardDataWord.h"
#endif

class OTalignBoardDataWord : public Tool
{
  public:
    OTalignBoardDataWord();
    ~OTalignBoardDataWord();

    void Initialise(void);

    // State machine
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;
    void Pause() override;
    void Resume() override;
    void Reset();

    static std::string fCalibrationDescription;
    
  private:
  //
    
#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTalignBoardDataWord fDQMHistogramOTalignBoardDataWord;
#endif
};

#endif
