/*!
  \file                  RD53Shared.h
  \brief                 Shared constants/functions across RD53 classes
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53Shared_H
#define RD53Shared_H

#include <cmath>
#include <iomanip>
#include <sstream>
#include <thread>

namespace RD53Shared
{
const char   NAMESEARCHinPATH[] = "CMSIT";                                         // Search for this name in config file name for manipulation
const char   RESULTDIR[]        = "Results";                                       // Directory containing the results
const double ISDISABLED         = -1.0;                                            // Encoding disabled channels
const double FITERROR           = -2.0;                                            // Encoding fit errors
const int    NLATENCYBINS       = 2;                                               // Number of latencies spanned
const int    MAXBITCHIPREG      = 16;                                              // Maximum number of bits of a chp register
const size_t NTHREADS           = round(std::thread::hardware_concurrency() / 2.); // Number of potential threads for the current CPU (removing hyper-threading)

std::string fromInt2Str(int val);
std::string composeFileName(const std::string& configFileName, const std::string& fName2Add);
size_t      countBitsOne(size_t num);

constexpr size_t setBits(size_t nBit2Set) { return (1 << nBit2Set) - 1; }
} // namespace RD53Shared

#endif
