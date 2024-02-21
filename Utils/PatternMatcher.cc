#include "Utils/PatternMatcher.h"
#include <iostream>

PatternMatcher::PatternMatcher(){}

PatternMatcher::~PatternMatcher(){}

void PatternMatcher::addToPattern(uint32_t thePattern, uint32_t thePatternMask, uint8_t thePatternBitLenght)
{
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << std::hex << thePattern << " " << thePatternMask << std::dec << " " << +thePatternBitLenght << std::endl;
    
    uint8_t numberOfBitsInWord = (sizeof(uint32_t))*8;
    uint8_t availableBitNumber = numberOfBitsInWord - fPatternNumberOfBits % numberOfBitsInWord;
    if(availableBitNumber == numberOfBitsInWord)  fPatternAndMaskVector.push_back({0, 0});
    auto& thePatternCurrentWord     = fPatternAndMaskVector.back();

    if(availableBitNumber >= thePatternBitLenght)
    {
        uint8_t bitShift = availableBitNumber - thePatternBitLenght;
        thePatternCurrentWord.first |= (thePattern << bitShift);
        thePatternCurrentWord.second |= (thePatternMask << bitShift);
    }
    else
    {
        uint8_t numberOfRemainingBits = thePatternBitLenght - availableBitNumber;
        thePatternCurrentWord.first |= (thePattern >> numberOfRemainingBits);
        thePatternCurrentWord.second |= (thePatternMask >> numberOfRemainingBits);

        uint32_t newWord = thePattern << (numberOfBitsInWord - numberOfRemainingBits);
        uint32_t newMask = thePatternMask << (numberOfBitsInWord - numberOfRemainingBits);

        fPatternAndMaskVector.push_back({newWord, newMask});
    }
    fPatternNumberOfBits += thePatternBitLenght;

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Pattern  -> ";
    // for(const auto word : fPatternAndMaskVector) std::cout << std::hex << word.first << std::dec << " ";
    // std::cout << std::endl;

}

bool PatternMatcher::isMatched(const std::vector<uint32_t>& theWordVector)
{
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Incoming -> ";
    // for(const auto word : theWordVector) std::cout << std::hex << word << std::dec << " ";
    // std::cout << std::endl;

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Pattern  -> ";
    // for(const auto word : fPatternAndMaskVector) std::cout << std::hex << word.first << std::dec << " ";
    // std::cout << std::endl;

    for(size_t patternIndex = 0; patternIndex<fPatternAndMaskVector.size(); ++patternIndex)
    {
        auto thePatternAndMaskWord = fPatternAndMaskVector[patternIndex];
        auto theIncomigMaskedWord = theWordVector[patternIndex] & thePatternAndMaskWord.second;
        auto thePatternMaskedWord = thePatternAndMaskWord.first & thePatternAndMaskWord.second;
        if(theIncomigMaskedWord != thePatternMaskedWord) return false;
    }

    return true;
}

