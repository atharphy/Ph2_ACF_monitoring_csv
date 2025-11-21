#include <cstdint>
#include <vector>
struct HybridL1EventInfo
{
    uint8_t  fErrorCode{0};
    uint8_t  fHybridId{0};
    uint8_t  fChipId{0};
    uint8_t  fChipType{0};
    uint16_t fFrameDelay{0};
    uint16_t fStatusBits{0};
    uint16_t fL1counter{0};
    uint8_t  fNumberOfStripClusters{0};
    uint8_t  fNumberOfPixelClusters{0};
};

struct ChipL1EventInfo
{
    uint8_t fChipId{0xFF};
};

struct StripClusterPS
{
    uint8_t      fAddress{0xFF};
    uint8_t      fMip{0xFF};
    uint8_t      fWidth{0xFF};
};

struct HybridL1EventPS
{
    HybridL1EventInfo fHybridL1EventInfo;
    std::vector<std::pair<ChipL1EventInfo, std::vector<StripClusterPS>>> fCBCHits;
};
