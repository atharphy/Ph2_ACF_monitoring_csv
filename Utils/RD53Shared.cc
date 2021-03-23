/*!
  \file                  RD53Shared.cc
  \brief                 Shared constants/functions across RD53 classes
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#include "RD53Shared.h"

std::string RD53Shared::fromInt2Str(int val)
{
    std::stringstream myString;
    myString << std::setfill('0') << std::setw(6) << val;
    return myString.str();
}

std::string RD53Shared::composeFileName(const std::string& configFileName, const std::string& fName2Add)
{
    std::string output = configFileName;
    output.insert(output.find_last_of("/\\") + 1, fName2Add);
    return output;
}

size_t RD53Shared::countBitsOne(size_t num)
{
    auto count = 0u;
    while(num != 0)
    {
        count += (num & 1);
        num >>= 1;
    }
    return count;
}

void RD53Shared::resetDefaultFloat() { std::cout.setf(std::ios_base::fmtflags(0), std::ios_base::floatfield); }
