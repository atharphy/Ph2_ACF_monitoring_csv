/*!
 *
 * \file OTcountSSASpuriousClusters.h
 * \brief OTcountSSASpuriousClusters class
 * \author Fabio Ravera
 * \date 07/03/24
 *
 */

#ifndef OTcountSSASpuriousClusters_h__
#define OTcountSSASpuriousClusters_h__

#include "tools/Tool.h"
#include <map>
#ifdef __USE_ROOT__
// Calibration is not running on the SoC: I need to instantiate the DQM histogrammer here
#include "DQMUtils/DQMHistogramOTcountSSASpuriousClusters.h"
#endif

class PatternMatcher;
namespace Ph2_HwInterface
{
class D19cFWInterface;
}

class OTcountSSASpuriousClusters : public Tool
{
  public:
    OTcountSSASpuriousClusters();
    ~OTcountSSASpuriousClusters();

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
    uint8_t                fFirstStrip{15};
    uint8_t                fStripGap{10};
    std::map<int, uint8_t> fBendingToCode{{0, 7}};
    uint32_t               fNumberOfEvents;

    std::vector<Cluster>  produceMatchingPixelClusterList(uint8_t stubRow, uint8_t stubSeed);
    std::vector<Cluster>  produceStripClusterList();
    void                  setStubLogicParameters(Ph2_HwDescription::ReadoutChip* theMPA);
    void                  setStripOffsetParameters(Ph2_HwDescription::ReadoutChip* theSSA);
    void                  prepareForStubInjection(Ph2_HwDescription::BeBoard* theBoard);
    DetectorDataContainer fStubMissingCountContainer;
    std::vector<Cluster>  fListOfInjectedStrips;

    void                           fillHistograms();
    void                           injectStubsPS(Ph2_HwDescription::ReadoutChip* theMPA, uint8_t numberOfBytesInSinglePacket, const std::vector<Stub>& listOfStubs);
    std::vector<std::vector<Stub>> createPSstubList();
    void                           runIntegrityTest();
    void                           runStubIntegrityTest(Ph2_HwDescription::BeBoard* theBoard, Ph2_HwInterface::D19cFWInterface* theFWInterface);
    uint8_t                        prepareCICforStubIntegrityTest(Ph2_HwDescription::Hybrid* theHybrid, uint8_t chipId);

#ifdef __USE_ROOT__
    // Calibration is not running on the SoC: Histogrammer is handeld by the calibration itself
    DQMHistogramOTcountSSASpuriousClusters fDQMHistogramOTcountSSASpuriousClusters;
#endif
};

#endif
