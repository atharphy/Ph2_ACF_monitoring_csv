#include <cstdint>

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
