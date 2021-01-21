/*

    \file                          Utilities.h
    \brief                         Some objects that might come in handy
    \author                        Nicolas PIERRE
    \version                       1.0
    \date                          10/06/14
    Support :                      mail to : nicolas.pierre@icloud.com

 */

#ifndef __UTILITIES_H__
#define __UTILITIES_H__

#include "../HWDescription/Definition.h"
#include <bitset>
#include <cstdio>
#include <fstream>
#include <ios>
#include <iostream>
#include <istream>
#include <limits>
#include <math.h>
#include <memory>
#include <stdint.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <vector>

#include <tuple> // new

template <typename... Args>
std::string string_format(const std::string& format, Args... args)
{
    size_t                  size = snprintf(nullptr, 0, format.c_str(), args...) + 1; // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), args...);
    return std::string(buf.get(), buf.get() + size - 1); // We don't want the '\0' inside
}
/*!
 * \brief Get time took since the start
 * \param pStart : Variable taking the start
 * \param pMili : Result in milliseconds/microseconds -> 1/0
 * \return The time took
 */
long getTimeTook(struct timeval& pStart, bool pMili);
/*!
 * \brief Flush the content of the input stream
 * \param in : input stream
 */
void myflush(std::istream& in);
/*!
 * \brief Wait for Enter key press
 */
void mypause();
/*!
 * \brief get Current Time & Date
 */
const std::string currentDateTime();
/*!
 * \brief Error Function for SCurve Fit
 * \param x: array of values
 * \param p: parameter array
 * \return function value
 */
double MyErf(double* x, double* par);
/*!
 * \brief Gamma peak with charge sharing
 * \param x: array of values
 * \param p: parameter array
 * \return function value
 */
double MyGammaSignal(double* x, double* par);
/*!
 * \brief Exponentially Modified Gaussian
 * \param x: array of values
 * \param p: parameter array
 * \return function value
 */
// double MyExGauss( double *x, double *par );
/*!
 * \brief converts any char array to int by automatically detecting if it is hex or dec
 * \param pRegValue: parsed xml parmaeter char*
 * \return converted integer
 */
uint32_t convertAnyInt(const char* pRegValue);
// uint8_t convertAnyInt ( const char* pRegValue );

double convertAnyDouble(const char* pRegValue);

uint8_t reverseBits(const uint8_t cValue);

// tokenize string
void tokenize(const std::string& str, std::vector<std::string>& tokens, const std::string& delimiters);

/*! \brief Expand environment variables in string
 * \param s input string
 * \return Result with variables expanded */
std::string expandEnvironmentVariables(std::string s);

// get run number from file
void getRunNumber(const std::string& pPath, int& pRunNumber, bool pIncrement = true);

#endif
