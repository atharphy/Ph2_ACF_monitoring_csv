#ifndef __CHAR_ARRAY_H__
#define __CHAR_ARRAY_H__

#include "HWDescription/Definition.h"

class CharArray //: public streammable
{
  public:
    CharArray(){};
    CharArray(std::string theString) { setString(theString); };

    void setString(const std::string& theString)
    {
        stringLength = theString.size();
        if(stringLength > MAX_LENGHT_PARAMETER_STRING)
        {
            std::string errorMessage = std::string(__PRETTY_FUNCTION__) + " " + std::to_string(__LINE__) + " size of registerName is greater that MAX_LENGHT_PARAMETER_STRING (" +
                                       std::to_string(MAX_LENGHT_PARAMETER_STRING) + ")";
            throw std::runtime_error(errorMessage);
        }
        std::copy(theString.begin(), theString.end(), fCharArray.begin());
    }

    std::string getString() const
    {
        char ouputChar[MAX_LENGHT_PARAMETER_STRING];
        for(size_t i = 0; i < MAX_LENGHT_PARAMETER_STRING; ++i) ouputChar[i] = fCharArray.at(i);
        std::string output(ouputChar);
        return output.substr(0, stringLength);
    }

  private:
    uint16_t                                      stringLength{0};
    std::array<char, MAX_LENGHT_PARAMETER_STRING> fCharArray;
};

#endif
