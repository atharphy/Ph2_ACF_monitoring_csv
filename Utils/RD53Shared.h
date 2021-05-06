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
#include <iostream>
#include <sstream>
#include <thread>
#include <vector>

namespace RD53Shared
{
const char     RESULTDIR[]   = "Results";                                       // Directory containing the results
const double   ISDISABLED    = -1.0;                                            // Encoding disabled channels
const double   FITERROR      = -2.0;                                            // Encoding fit errors
const int      NLATENCYBINS  = 2;                                               // Number of latencies spanned
const int      MAXBITCHIPREG = 16;                                              // Maximum number of bits of a chp register
const size_t   NTHREADS      = round(std::thread::hardware_concurrency() / 2.); // Number of potential threads for the current CPU (removing hyper-threading)
const uint32_t DEEPSLEEP     = 100000;                                          // [microseconds]
const int      MINSTEPS      = 10;                                              // Minimum number of steps for a scan

std::string fromInt2Str(int val);
std::string composeFileName(const std::string& configFileName, const std::string& fName2Add);
size_t      countBitsOne(size_t num);
void        resetDefaultFloat();

constexpr size_t setBits(size_t nBit2Set) { return (1 << nBit2Set) - 1; }

template <typename T>
inline void myMove(std::vector<T> source, std::vector<T>& destination)
{
    destination.insert(destination.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}

} // namespace RD53Shared

#endif
