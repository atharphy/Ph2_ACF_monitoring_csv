/*!
 * \file DQMHistogramOTTimeCorrelation.h
 * \brief DQM class for OTTimeCorrelations
 * \author [Your Name]
 * \date [Date]
 */

#ifndef DQMOTTimeCorrelation_h
#define DQMOTTimeCorrelation_h

#include "DQMUtils/DQMHistogramBase.h"
#include "TH1F.h"
#include "TH2F.h"
#include "TH3F.h"
#include "TLegend.h"
#include "TStopwatch.h"
#include "Utils/Container.h"
#include "Utils/D19cCic2Event.h"
#include "Utils/DataContainer.h"
#include "Utils/GenericDataArray.h"

struct CorrelationResult
{
    std::vector<int> channels;
    std::vector<int> indexes;

    void clear()
    {
        channels.clear();
        indexes.clear();
    }
};

struct TS3DCorrelationResult
{
    // time index, pair ic channels hit
    std::vector<std::vector<std::pair<int, int>>> slices;

    void allocate(size_t depth)
    {
        slices.assign(depth, {});
        for(auto& v: slices) v.reserve(2000);
    }

    void clear()
    {
        for(auto& v: slices) v.clear();
    }
};

// data structure with things in common for both memories
class OTTimeCorrelationConfig
{
  public:
    inline static uint32_t currentBX        = 0xFFFFFFFF;
    inline static uint32_t previousBX       = 0xFFFFFFFF;
    inline static size_t   memoryDepth      = 8;
    inline static int      triggerPerBurst  = 16;
    inline static uint32_t triggerDelay     = 0;
    inline static size_t   minHitsThreshold = 50;
    inline static int      currentBurstSize = 1;
    inline static bool     isNewBurst       = false;
    inline static int      totBursts        = 0;
    inline static int      mismatchedBursts = 0;
    inline static int      last_wrong_bx    = 0;

    static void updateBX(uint32_t newBX)
    {
        if(currentBX == 0xFFFFFFFF)
        { // first ev after reset
            // currentBX = newBX;
            previousBX       = currentBX;
            currentBX        = newBX;
            currentBurstSize = 1;
            return;
        }

        previousBX = currentBX;
        currentBX  = newBX;
        // actual BX spacing should be 16 bits
        constexpr uint32_t bxMask = 0xFFFF;
        uint32_t           prevBX = previousBX & bxMask;
        uint32_t           currBX = currentBX & bxMask;
        uint32_t           diff   = (currBX >= prevBX) ? (currBX - prevBX) : ((bxMask - prevBX) + currBX + 1);

        // check that the number of triggers in burst is the correct one
        // difference between tgrs in the same burst should be del+1
        // LOG(INFO)<<"Difference is "<< diff;
        if(getTriggerPerBurst() > 1)
        { // only check if we are in multi-trigger mode
            if((diff) == triggerDelay + 1)
            { // it's the same burst
                currentBurstSize += 1;
                isNewBurst = false;
            }
            else
            {
                isNewBurst = true;
                totBursts += 1;
                // it's a new one: check if last one had the right number of triggers
                if(currentBurstSize != triggerPerBurst)
                {
                    mismatchedBursts += 1;
                    LOG(ERROR) << RED << "Desired size: " << triggerPerBurst << " size: " << currentBurstSize << "; masked bx: " << currBX << " bx: " << currentBX << ", diff w prev wrong "
                               << currBX - last_wrong_bx << RESET;
                    last_wrong_bx = currBX;
                    // throw std::runtime_error("mismatch in burst size");
                }
                // reset counter
                currentBurstSize = 1;
            }
        }
    }
    static void reset()
    {
        currentBX        = 0xFFFFFFFF;
        previousBX       = 0xFFFFFFFF;
        memoryDepth      = 8;
        triggerPerBurst  = 16;
        triggerDelay     = 0;
        minHitsThreshold = 50;
        currentBurstSize = 1;
        isNewBurst       = false;
        totBursts        = 0;
        mismatchedBursts = 0;
        last_wrong_bx    = 0;
    }
    static size_t&   getN() { return memoryDepth; }
    static int&      getTriggerPerBurst() { return triggerPerBurst; }
    static uint32_t& getTriggerDelay() { return triggerDelay; }
    static size_t&   getMinHits() { return minHitsThreshold; }

    static int nSlices() { return (memoryDepth >= 9) ? 9 : (int)memoryDepth; }

    static void setN(size_t n)
    {
        LOG(INFO) << "Setting memory depth to " << n;
        memoryDepth = n;
    }
    static void setTriggerPerBurst(int n) { triggerPerBurst = n; }
    static void setTriggerDelay(int trgDel) { triggerDelay = trgDel; }
    static void setMinHits(size_t mh) { minHitsThreshold = mh; }
};

// data structure with memory and correlation results
template <typename T>
class OTTimeCorrelationDataBase : public OTTimeCorrelationConfig
{
  public:
    inline static std::vector<int> data; // vector containing hit data

    // Stopwatches
    inline static TStopwatch fStopwatch_Update;
    inline static TStopwatch fStopwatch_SameTCorr;
    inline static TStopwatch fStopwatch_MinHits;
    inline static TStopwatch fStopwatch_3DCorr;

    static void reportComputationTimingStats(std::string typeName)
    {
        LOG(INFO) << "=== " << typeName << " Computation Timing Statistics ===";
        LOG(INFO) << "Update:           \t" << fStopwatch_Update.RealTime() << " s (CPU: " << fStopwatch_Update.CpuTime() << " s)";
        LOG(INFO) << "Same TCorr Comp:  \t" << fStopwatch_SameTCorr.RealTime() << " s (CPU: " << fStopwatch_SameTCorr.CpuTime() << " s)";
        LOG(INFO) << "MinHits Comp:     \t" << fStopwatch_MinHits.RealTime() << " s (CPU: " << fStopwatch_MinHits.CpuTime() << " s)";
        LOG(INFO) << "3D Corr Comp:     \t" << fStopwatch_3DCorr.RealTime() << " s (CPU: " << fStopwatch_3DCorr.CpuTime() << " s)";

        double total = fStopwatch_Update.RealTime() + fStopwatch_SameTCorr.RealTime() + fStopwatch_MinHits.RealTime() + fStopwatch_3DCorr.RealTime();
        LOG(INFO) << "Total " << typeName << " Computation: \t" << total << " s";
        LOG(INFO) << "===========================================";

        fStopwatch_Update.Reset();
        fStopwatch_SameTCorr.Reset();
        fStopwatch_MinHits.Reset();
        fStopwatch_3DCorr.Reset();
    }

    static std::deque<std::vector<int>>& getMemory()
    {
        // maintain a static deque to hold the memory
        static std::deque<std::vector<int>> mem;
        return mem;
    }

    static CorrelationResult& getLastFWCorrelation()
    {
        static CorrelationResult r;
        return r;
    }
    static CorrelationResult& getLastBWCorrelation()
    {
        static CorrelationResult r;
        return r;
    }
    static TS3DCorrelationResult& getLastFW3DCorrelation()
    {
        static TS3DCorrelationResult r;
        return r;
    }
    static TS3DCorrelationResult& getLastBW3DCorrelation()
    {
        static TS3DCorrelationResult r;
        return r;
    }
    static std::vector<int>& getLastFWMinHits()
    {
        static std::vector<int> v;
        return v;
    }
    static std::vector<int>& getLastBWMinHits()
    {
        static std::vector<int> v;
        return v;
    }

    static void reset()
    {
        data.clear();
        getMemory().clear();

        getLastFWCorrelation().clear();
        getLastBWCorrelation().clear();

        getLastFWMinHits().clear();
        getLastBWMinHits().clear();

        getLastFW3DCorrelation().allocate(nSlices());
        getLastBW3DCorrelation().allocate(nSlices());

        // Ensure stopwatches are reset to avoid accumulating time since construction
        fStopwatch_Update.Stop();
        fStopwatch_Update.Reset();
        fStopwatch_SameTCorr.Stop();
        fStopwatch_SameTCorr.Reset();
        fStopwatch_MinHits.Stop();
        fStopwatch_MinHits.Reset();
        fStopwatch_3DCorr.Stop();
        fStopwatch_3DCorr.Reset();
    }

    static void update()
    {
        fStopwatch_Update.Start(false);
        // update the memory with the new event data
        auto& mem = getMemory();

        if(getTriggerPerBurst() > 1 && isNewBurst)
        {
            mem.clear();
            LOG(DEBUG) << "New burst detected, resetting memory.";
            isNewBurst = false;
        }

        if(mem.size() >= getN())
        { // remove the first element if max depth is reached
            mem.pop_front();
        }

        mem.push_back(data);
        fStopwatch_Update.Stop();
    }

    static void compute_same_tcorr()
    {
        fStopwatch_SameTCorr.Start(false);
        auto&        mem = getMemory();
        const size_t S   = mem.size();

        if(S < getN())
        {
            fStopwatch_SameTCorr.Stop();
            return;
        } // if deque not full, return

        auto& lastFWCorr = getLastFWCorrelation();
        auto& lastBWCorr = getLastBWCorrelation();

        lastFWCorr.clear();
        lastBWCorr.clear();

        // COMPARE TO NEXT EVENTS: loop on the channels of the first element in memory
        const auto& firstEvent = mem.front();
        // COMPARE TO PREVIOUS EVENTS: loop on the channels that are on (current event)
        const auto& lastEvent = mem.back();

        for(size_t idx = 0; idx < S; ++idx)
        {
            for(int chFW: firstEvent)
            {
                if(std::binary_search(mem[idx].begin(), mem[idx].end(), chFW))
                {
                    lastFWCorr.channels.push_back(chFW);
                    lastFWCorr.indexes.push_back(idx);
                }
            }

            for(auto chBW: lastEvent)
            {
                // check if channel was on also in this past event
                if(std::binary_search(mem[idx].begin(), mem[idx].end(), chBW))
                {
                    // reversed index: newest=0 (the current event), oldest=S-1 (N events ago)
                    int rev_idx = (int)((S - 1) - idx);
                    lastBWCorr.channels.push_back(chBW);
                    lastBWCorr.indexes.push_back(rev_idx);
                }
            }
        }
        fStopwatch_SameTCorr.Stop();
    }

    static void compute_min_hits()
    {
        fStopwatch_MinHits.Start(false);
        // the interesting thing is the number of hits in each event, so the size of each vector in memory
        auto&        mem = getMemory();
        const size_t S   = mem.size();

        auto& lastFWHists = getLastFWMinHits();
        auto& lastBWHists = getLastBWMinHits();
        lastFWHists.clear();
        lastBWHists.clear();

        if(S < getN())
        {
            fStopwatch_MinHits.Stop();
            return;
        }
        // Forward: require first event has enough hits
        if(mem[0].size() > getMinHits())
        {
            for(size_t idx = 0; idx < S; ++idx) { lastFWHists.push_back(mem[idx].size()); }
        }
        // Backward: require last event has enough hits
        if(mem[S - 1].size() > getMinHits())
        {
            for(size_t idx = 0; idx < S; ++idx) { lastBWHists.push_back(mem[S - 1 - idx].size()); }
        }
        fStopwatch_MinHits.Stop();
        return;
    }

    static void compute_3D_corr()
    {
        fStopwatch_3DCorr.Start(false);
        auto&        mem = getMemory();
        const size_t S   = mem.size();
        if(S < getN())
        {
            fStopwatch_3DCorr.Stop();
            return;
        }
        size_t depth3D      = std::min<size_t>(9, S);
        auto&  lastFW3DCorr = getLastFW3DCorrelation();
        auto&  lastBW3DCorr = getLastBW3DCorrelation();
        lastFW3DCorr.clear();
        lastBW3DCorr.clear();

        // Forward: first event vs next events
        const auto& firstEvent = mem.front();
        // Backward: last event vs previous events (reversed index)
        const auto& lastEvent = mem.back();

        // Ensure vectors are large enough before accessing them by index []
        if(lastFW3DCorr.slices.size() < depth3D) lastFW3DCorr.allocate(depth3D);
        if(lastBW3DCorr.slices.size() < depth3D) lastBW3DCorr.allocate(depth3D);

        for(size_t idx = 0; idx < depth3D; ++idx)
        {
            const auto& currEvent = mem[idx];
            int         rev_idx   = (int)((depth3D - 1) - idx);
            for(int cy: currEvent)
            {
                for(int cx: firstEvent) { lastFW3DCorr.slices[idx].push_back({cx, cy}); }

                for(int cx: lastEvent) { lastBW3DCorr.slices[rev_idx].push_back({cx, cy}); }
            }
        }
        fStopwatch_3DCorr.Stop();
    }
};

struct StripTag
{
};
struct PixelTag
{
};

class OTTimeCorrelationStripData : public OTTimeCorrelationDataBase<StripTag>
{
  public:
    inline static std::vector<int>& stripData = OTTimeCorrelationDataBase<StripTag>::data;
};

class OTTimeCorrelationPixelData : public OTTimeCorrelationDataBase<PixelTag>
{
  public:
    inline static std::vector<int>& pixelData = OTTimeCorrelationDataBase<PixelTag>::data;
};

class ChipErrorData
{
  public:
    inline static std::map<int, int> mpaErrors; // map of MPA chip ID to error count
    inline static std::map<int, int> ssaErrors; // map of SSA chip ID to error count

    static void reset()
    {
        mpaErrors.clear();
        ssaErrors.clear();
        for(int i = 0; i < 8; ++i)
        {
            mpaErrors[i] = 0;
            ssaErrors[i] = 0;
        }
    }
};

class DQMHistogramOTTimeCorrelation : public DQMHistogramBase
{
  public:
    DQMHistogramOTTimeCorrelation();
    ~DQMHistogramOTTimeCorrelation();
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap) override;
    void book(TFile* theOutputFile, DetectorContainer& theDetectorStructure, const Ph2_Parser::SettingsMap& pSettingsMap, std::string suffix, float sigma);
    void process() override;
    bool fill(std::string& inputStream) override;
    void reset() override;

    // Custom fill function for our specific data structure
    void fillSSAData(const OTTimeCorrelationStripData& theData, uint32_t trgBurst, uint32_t trgDel, const std::string suffix);
    void fillMPAData(const OTTimeCorrelationPixelData& theData, uint32_t trgBurst, uint32_t trgDel, const std::string suffix);
    void fillErrorHist(const ChipErrorData& ChipErrorData, uint32_t trgBurst, uint32_t trgDel, std::string suffix);
    void fill3Dhistograms(const OTTimeCorrelationStripData& sData, const OTTimeCorrelationPixelData& pData, uint32_t trgBurst, uint32_t trgDel, std::string suffix);
    void fill2DhistSlices(const OTTimeCorrelationStripData& sData, const OTTimeCorrelationPixelData& pData, uint32_t trgBurst, uint32_t trgDel, std::string suffix);

    void fillModuleHitPlots(DetectorDataContainer& theHitData, bool isStrip, std::string suffix);
    void reportTimingStats();

    template <size_t T2>
    void fillEventsVsHitsHist(const BaseDataContainer* ChipContainer, TH1F& theHistogram)
    {
        const GenericDataArray<uint32_t, T2>& cDataSummary = ChipContainer->getSummary<GenericDataArray<uint32_t, T2>>();
        for(uint16_t iChan = 0; iChan < T2; iChan++) { theHistogram.SetBinContent(iChan + 1, cDataSummary.at(iChan)); }
        theHistogram.Sumw2();
    }

  private:
    DetectorContainer*      fDetectorContainer;
    TFile*                  fOutputFile;
    Ph2_Parser::SettingsMap fSettingsMap;

    //  containers for optical group histograms for SSA
    std::map<std::string, DetectorDataContainer>              fSameEvCorrSSAHistogramsMap;       // strip correlation in the same event
    std::map<std::string, DetectorDataContainer>              fSameStripFWTCorrSSAHistogramsMap; // strip with itself time correlation
    std::map<std::string, DetectorDataContainer>              fSameStripBWTCorrSSAHistogramsMap; // strip with itself time correlation
    std::map<std::string, DetectorDataContainer>              fMinHitsFWSSAHistogramsMap;        // minimum hits histograms
    std::map<std::string, DetectorDataContainer>              fMinHitsBWSSAHistogramsMap;        // backward min hits histograms
    std::map<std::string, DetectorDataContainer>              f3DTSFWCorrSSAHistogramsMap;       // time and space corr histograms
    std::map<std::string, DetectorDataContainer>              f3DTSBWCorrSSAHistogramsMap;       // time and space corr histograms
    std::map<std::string, std::vector<DetectorDataContainer>> fTSCorrSliceFWSSAMap;              // 2D slice histograms for each z-bin
    std::map<std::string, std::vector<DetectorDataContainer>> fTSCorrSliceBWSSAMap;              // backward 2D slice histograms for each z-bin
    std::map<std::string, DetectorDataContainer>              fStripModuleHitHistograms;         // strip hits per module
    std::map<std::string, DetectorDataContainer>              fPixelModuleHitHistograms;         // pixel hits per module

    //  containers for optical group histograms for MPA
    std::map<std::string, DetectorDataContainer>              fSameEvCorrMPAHistogramsMap;       // strip correlation in the same event
    std::map<std::string, DetectorDataContainer>              fSamePixelFWTCorrMPAHistogramsMap; // strip with itself time correlation
    std::map<std::string, DetectorDataContainer>              fSamePixelBWTCorrMPAHistogramsMap; // strip with itself time correlation
    std::map<std::string, DetectorDataContainer>              fMinHitsFWMPAHistogramsMap;        // minimum hits histograms
    std::map<std::string, DetectorDataContainer>              fMinHitsBWMPAHistogramsMap;        // backward min hits histograms
    std::map<std::string, DetectorDataContainer>              f3DTSFWCorrMPAHistogramsMap;       // time and space corr histograms
    std::map<std::string, DetectorDataContainer>              f3DTSBWCorrMPAHistogramsMap;       // time and space corr histograms
    std::map<std::string, std::vector<DetectorDataContainer>> fTSCorrSliceFWMPAMap;              // 2D slice histograms for each z-bin
    std::map<std::string, std::vector<DetectorDataContainer>> fTSCorrSliceBWMPAMap;              // backward 2D slice histograms for each z-bin

    // error histograms
    std::map<std::string, DetectorDataContainer> fMPAErrorHistogramsMap; // MPA error histograms
    std::map<std::string, DetectorDataContainer> fSSAErrorHistogramsMap; // SSA error histograms

    // Stopwatches for timing histogram filling operations
    TStopwatch fStopwatch_SSA_SameEv;  // SSA Same Event timing
    TStopwatch fStopwatch_SSA_FWTC;    // SSA FW Time Correlation timing
    TStopwatch fStopwatch_SSA_BWTC;    // SSA BW Time Correlation timing
    TStopwatch fStopwatch_SSA_MinHits; // SSA Min Hits timing
    TStopwatch fStopwatch_SSA_3D;      // SSA 3D correlation timing
    TStopwatch fStopwatch_SSA_Slices;  // SSA Slices timing
    TStopwatch fStopwatch_MPA_SameEv;  // MPA Same Event timing
    TStopwatch fStopwatch_MPA_FWTC;    // MPA FW Time Correlation timing
    TStopwatch fStopwatch_MPA_BWTC;    // MPA BW Time Correlation timing
    TStopwatch fStopwatch_MPA_MinHits; // MPA Min Hits timing
    TStopwatch fStopwatch_MPA_3D;      // MPA 3D correlation timing
    TStopwatch fStopwatch_MPA_Slices;  // MPA Slices timing
};

#endif
