#include "Utils/PatternMatcher.h"
#include <iostream>
#include <bitset>

PatternMatcher::PatternMatcher() {}

PatternMatcher::~PatternMatcher() {}

void PatternMatcher::addToPattern(uint32_t thePattern, uint32_t thePatternMask, uint8_t thePatternBitLenght)
{
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] " << std::hex << thePattern << " " << thePatternMask << std::dec << " " << +thePatternBitLenght << std::endl;

    uint8_t numberOfBitsInWord = (sizeof(uint32_t)) * 8;
    uint8_t availableBitNumber = numberOfBitsInWord - fPatternNumberOfBits % numberOfBitsInWord;
    if(availableBitNumber == numberOfBitsInWord) fPatternAndMaskVector.push_back({0, 0});
    auto& thePatternCurrentWord = fPatternAndMaskVector.back();

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
    if(theWordVector.size() < fPatternAndMaskVector.size()) return false;
    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Incoming -> ";
    // for(const auto& word : theWordVector) std::cout << std::hex << word << std::dec << " ";
    // std::cout << std::endl;

    // std::cout<< __PRETTY_FUNCTION__ << " [" << __LINE__ << "] Pattern  -> ";
    // for(const auto& word : fPatternAndMaskVector) std::cout << std::hex << word.first << std::dec << " ";
    // std::cout << std::endl;

    for(size_t patternIndex = 0; patternIndex < fPatternAndMaskVector.size(); ++patternIndex)
    {
        auto thePatternAndMaskWord = fPatternAndMaskVector[patternIndex];
        auto theIncomigMaskedWord  = theWordVector[patternIndex] & thePatternAndMaskWord.second;
        auto thePatternMaskedWord  = thePatternAndMaskWord.first & thePatternAndMaskWord.second;
        if(theIncomigMaskedWord != thePatternMaskedWord) return false;
    }

    return true;
}

uint32_t PatternMatcher::countMatchingBits(const std::vector<uint32_t>& theWordVector) const
{
    uint32_t numberOfMatchingBits = 0;
    
    for(size_t index = 0; index<=fPatternAndMaskVector.size(); ++index)
    {
        if(index >= theWordVector.size()) break;
        uint32_t patternAndWordXOR = theWordVector[index] ^ fPatternAndMaskVector[index].first;
        std::bitset<32> patternAndWordXORBitset(patternAndWordXOR);
        std::bitset<32> maskBitset(fPatternAndMaskVector[index].second);
        patternAndWordXORBitset.flip();
        auto patternAndWordXORBitsetMasked = patternAndWordXORBitset & maskBitset;
        numberOfMatchingBits += patternAndWordXORBitsetMasked.count();
    }

    return numberOfMatchingBits;
}

std::vector<uint32_t> PatternMatcher::getPattern() const
{
    std::vector<uint32_t> thePatternVector;
    for(const auto& thePatternAndMaskWord: fPatternAndMaskVector) thePatternVector.push_back(thePatternAndMaskWord.first);
    return thePatternVector;
}

std::vector<uint32_t> PatternMatcher::getMask() const
{
    std::vector<uint32_t> theMaskVector;
    for(const auto& thePatternAndMaskWord: fPatternAndMaskVector) theMaskVector.push_back(thePatternAndMaskWord.second);
    return theMaskVector;
}

void PatternMatcher::maskStubFor2Skickoff()
{
    std::vector<uint32_t> theMaskVector(fPatternAndMaskVector.size(), 0);

    size_t lineCounter = 0;
    for(auto& theMask: theMaskVector)
    {
        for(int8_t bitCounter = sizeof(uint32_t) * 8 - 1; bitCounter >= 0; --bitCounter)
        {
            if(lineCounter == 4)
            {
                lineCounter = 0;
                continue;
            }
            theMask |= 0x1 << bitCounter;
            ++lineCounter;
        }
    }

    for(uint8_t patternIndex = 0; patternIndex < fPatternAndMaskVector.size(); ++patternIndex) { fPatternAndMaskVector[patternIndex].second &= theMaskVector[patternIndex]; }
}

uint32_t PatternMatcher::getNumberOfMaskedBits()
{
    uint32_t numberOfUnmaskedBits = 0;
    for(auto thePatterAndMask: fPatternAndMaskVector)
    {
        std::bitset<32> theMaskBitset(thePatterAndMask.second);
        numberOfUnmaskedBits += theMaskBitset.count();
    }

    return numberOfUnmaskedBits;
}

void PatternMatcher::clear()
{
    fPatternNumberOfBits = 0;
    fPatternAndMaskVector.clear();
}
