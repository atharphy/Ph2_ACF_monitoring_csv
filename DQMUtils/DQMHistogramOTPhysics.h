/*!
 *
 * \file DQMHistogramOTPhysics.h
 * \brief DQMHistogramOTPhysics class
 * \author Fabio Ravera
 * \date 06/11/25
 *
 */

#ifndef DQMHistogramOTPhysics_H
#define DQMHistogramOTPhysics_H

#include "DQMHistogramBase.h"

#include <TH1F.h>
#include <TH2F.h>

class DQMHistogramOTPhysics : public DQMHistogramBase
{
  public:
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& settingsMap) override;
    void process() override;
    bool fill(std::string& inputStream) override;
    void reset() override {};

  private:
    DetectorContainer* fDetectorContainer;
    std::string        fResultDirectoryName;
    int                fRunNumber;
};

#endif
