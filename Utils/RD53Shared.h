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

#define BOOST_UBLAS_NDEBUG 1
#include <boost/numeric/ublas/lu.hpp>
#include <boost/numeric/ublas/matrix.hpp>

namespace RD53Shared
{
const char     RESULTDIR[]   = "Results";                                       // Directory containing the results
const double   ISDISABLED    = -1.0;                                            // Encoding disabled channels
const double   FITERROR      = -2.0;                                            // Encoding fit errors
const int      NLATENCYBINS  = 2;                                               // Number of latencies spanned
const int      MAXBITCHIPREG = 16;                                              // Maximum number of bits of a chp register
const size_t   NTHREADS      = round(std::thread::hardware_concurrency() / 2.); // Number of potential threads for the current CPU (removing hyper-threading)
const uint32_t DEEPSLEEP     = 100000;                                          // [microseconds]
const uint8_t  READOUTSLEEP  = 50;                                              // [microseconds]
const uint8_t  MAXATTEMPTS   = 40;                                              // Maximum number of attempts
const int      MAXSTEPS      = 10;                                              // Maximum number of steps for a scan

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

template <typename T>
T mtxInversion(const boost::numeric::ublas::matrix<T>& input, boost::numeric::ublas::matrix<T>& inverse)
{
    if(input.size1() != input.size2()) return 0;

    boost::numeric::ublas::matrix<T>                  mLU(input);
    boost::numeric::ublas::permutation_matrix<size_t> pivots(mLU.size1());

    bool isSingular;
    if((isSingular = boost::numeric::ublas::lu_factorize(mLU, pivots)) != false) return 0;

    T det = 1;
    for(auto i = 0u; i < pivots.size(); i++)
    {
        if(pivots(i) != i) det *= -1;
        det *= mLU(i, i);
    }

    inverse.assign(boost::numeric::ublas::identity_matrix<T>(mLU.size1()));
    boost::numeric::ublas::lu_substitute(mLU, pivots, inverse);

    return det;
}

} // namespace RD53Shared

#endif
