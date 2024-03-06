#ifndef __PATTERN_MATCHER_H__
#define __PATTERN_MATCHER_H__

#include <cstddef>
#include <cstdint>
#include <vector>

/*!
 * \class PatternMatcher
 * \brief Class to create a pattern and check whether if it matches with data
 */
class PatternMatcher
{
  public:
    /*!
    * \brief Constructor of the PatternMatcher Class
    */
    PatternMatcher();
    /*!
    * \brief Destructor of the PatternMatcher Class
    */
    ~PatternMatcher();

    /*!
    * \brief Add more information in the pattern
    * \param thePattern: the new piece of pattern
    * \param thePatternMask: the mask for the new piece of pattern
    * \param thePatternBitLenght: the number of bits composing the new piece of pattern
    */
    void   addToPattern(uint32_t thePattern, uint32_t thePatternMask, uint8_t thePatternBitLenght);

    /*!
    * \brief returns true if the pattern matches with the theWordVector vector provided
    * \param theWordVector: the data to match
    * \return is the patttern matched
    */
    bool   isMatched(const std::vector<uint32_t>& theWordVector);

    /*!
    * \brief get the number of bits composing the pattern
    * \return number of bits in the pattern
    */
    size_t getNumberOfPatternBits() const { return fPatternNumberOfBits; }


    /*!
    * \brief get the pattern vector
    *  \returns pattern vector
    */
    std::vector<uint32_t> getPattern() const;

    /*!
    * \brief get the pattern mask
    * \returns pattern mask
    */
    std::vector<uint32_t> getMask() const;

    /*!
    * \brief temporary function to ignore bug on stub line 4 for 2S kickoff FEHR
    */
    void maskStubFor2Skickoff();

  private:
    size_t                                     fPatternNumberOfBits{0};
    std::vector<std::pair<uint32_t, uint32_t>> fPatternAndMaskVector;
};

#endif