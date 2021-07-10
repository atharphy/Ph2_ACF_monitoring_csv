/*!
 *
 * \file PSAlignment.h
 * \brief CIC FE alignment class, automated alignment procedure for CICs
 * connected to FEs
 * \author Sarah SEIF EL NASR-STOREY
 * \date 28 / 06 / 19
 *
 * \Support : sarah.storey@cern.ch
 *
 */

#ifndef PSAlignment_h__
#define PSAlignment_h__

#include "Tool.h"

#include <map>

class PSAlignment : public Tool
{
  public:
    PSAlignment();
    ~PSAlignment();

    void Initialise();
    bool AlignStubInputs(Ph2_HwDescription::BeBoard* pBoard);
    bool AlignL1Inputs(Ph2_HwDescription::BeBoard* pBoard);
    void MapMPAOutputs(std::string pSetupType = "PSModule");
    bool Align();
    std::vector<std::pair<uint8_t, uint8_t>> AlignChip(Ph2_HwDescription::ReadoutChip* pChip, std::vector<Ph2_HwInterface::Injection> pInjections, uint16_t pLatency);
    void Running() override;
    void Stop() override;
    void Pause() override;
    void Resume() override;
    void writeObjects();
    void Reset();

    // get alignment results
    bool getStatus() const { return fSuccess; }

  protected:
  private:
    // status
    bool fSuccess;
    // Containers
    DetectorDataContainer fRegMapContainer;
    DetectorDataContainer fBoardRegContainer;

// booking histograms
#ifdef __USE_ROOT__
//  DQMHistogramCic fDQMHistogram;
#endif
};
#endif
