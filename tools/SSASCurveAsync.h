/*!
  \file                  SSASCurveAsync.h
  \brief                 Implementaion of SCurve scan
  \author                Kevin Nash
  \version               1.0
  \date                  28/06/18
  Support:               email to knash@gmail.com
*/

#ifndef SSASCurve_h__
#define SSASCurve_h__

#include "../Utils/Container.h"
#include "../Utils/ContainerFactory.h"
#include "../Utils/ContainerRecycleBin.h"
#include "../Utils/ThresholdAndNoise.h"
#include "Tool.h"

#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramSSASCurveAsync.h"
#include "TApplication.h"
#endif

// #############
// # CONSTANTS #
// #############
#define RESULTDIR "ResultsSSA" // Directory containing the results

// #####################
// # SCurve test suite #
// #####################
class SSASCurve : public Tool
{
  public:
    SSASCurve();
    ~SSASCurve();

    void Initialise(void);
    void run(void);
    void writeObjects(void);

  private:
    size_t StartTHDAC = this->findValueInSettings("StartTHDAC");
    size_t StopTHDAC  = this->findValueInSettings("StopTHDAC");
    size_t NMsec      = this->findValueInSettings("NMsec");
    size_t NMpulse    = this->findValueInSettings("NMpulse");
    size_t Res        = this->findValueInSettings("Res");
    size_t Nlvl       = this->findValueInSettings("Nlvl");
    bool   SyncDebug  = this->findValueInSettings("SyncDebug");

    float Mrms                   = this->findValueInSettings("Mrms");
    float Vfac                   = this->findValueInSettings("Vfac");
    float TestPulsePotentiometer = this->findValueInSettings("TestPulsePotentiometer");

    float globalmax = 0;

    std::vector<uint32_t> localmax = decltype(localmax)(120, 0);
    std::vector<float>    normvals = decltype(normvals)(120, 0.0);
    std::vector<uint32_t> lastval  = decltype(lastval)(120, 0);
    std::vector<bool>     pastmax;

    bool cWithMPA;
    bool cWithSSA;

#ifdef __USE_ROOT__
    DQMHistogramSSASCurveAsync fDQMHistogramSSASCurveAsync;
#endif
};

#endif
