#ifndef __PATTERN_MATCHER_H__
#define __PATTERN_MATCHER_H__

#include <vector>
#include <cstdint>
#include <cstddef>

class PatternMatcher
{
  public:
    PatternMatcher();
    ~PatternMatcher();

    void addToPattern(uint32_t thePattern, uint32_t thePatternMask, uint8_t thePatternBitLenght);
    bool isMatched(const std::vector<uint32_t>& theWordVector);
    size_t getNumberOfPatternBits() const {return fPatternNumberOfBits;}

  private:
    size_t fPatternNumberOfBits {0};
    std::vector<std::pair<uint32_t, uint32_t>> fPatternAndMaskVector;
};

#endif