/*!
 *
 * \file OTPhysics.h
 * \brief OTPhysics class
 * \author Fabio Ravera
 * \date 06/11/25
 *
 */

#ifndef OTPhysics_H
#define OTPhysics_H

#include "tools/Tool.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramOTPhysics.h"
#endif

#define RESULTDIR "Results" // Directory containing the results

// #######################
// # OTPhysics data taking #
// #######################
class OTPhysics : public Tool
{
  public:
    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;

    static std::string fCalibrationDescription;

  private:
    unsigned int getDataFromBoards();

#ifdef __USE_ROOT__
    DQMHistogramOTPhysics fDQMHistogramOTPhysics;
#endif

  protected:
    bool         fSaveRawData;
    unsigned int fTotalDataSize = 0;
};

#endif
