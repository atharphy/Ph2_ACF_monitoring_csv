/*!
 *
 * \file OTTimeCorrelations.h
 * \brief OTTimeCorrelations class
 * \author Carmen Selicato
 * \date 01/02/26
 *
 */

#ifndef OTTimeCorrelations_H
#define OTTimeCorrelations_H

#include "tools/Tool.h"
#ifdef __USE_ROOT__
#include "DQMUtils/DQMHistogramOTTimeCorrelation.h"
#endif

#include "Utils/FileHandler.h"

class OTTimeCorrelations : public Tool
{
  public:
    OTTimeCorrelations();
    ~OTTimeCorrelations();

    void Running() override;
    void Stop() override;
    void ConfigureCalibration() override;

    static std::string fCalibrationDescription;

    void SetThresholds(float stripSigma, float pixelSigma);
    void SetTriggerSource(uint8_t pTriggerSource);

    void setIterationSettings(size_t iteration);

  private:
    std::vector<float> theDelayBetweenTriggers{0, 1};
    std::vector<float> theMPASigma{3.0, 3.0};
    std::vector<float> theSSASigma{3.0, 3.0};
    std::vector<float> theNTriggerPerBurst{4, 4};
    std::vector<float> theAverageFrequency{400, 400};

#ifdef __USE_ROOT__
    DQMHistogramOTTimeCorrelation fDQMHistogramOTTimeCorrelation;
#endif

  protected:
    unsigned int fTotalDataSize = 0;
    uint32_t     fNeventsConf   = 1000; // value read from config file
    uint32_t     fNevents       = 1000;
    std::string  iterationSettingsName;
    bool         fSaveRawData;
};

#endif
