/*!
  \file                  GenericDataArray.h
  \brief                 Generic data array for DAQ
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef GenericDataArray_H
#define GenericDataArray_H

#include "Utils/ConsoleColor.h"
#include "Utils/easylogging++.h"
#include <iostream>
#include <array>

template <typename T, size_t N>
class GenericDataArray : public std::array<T, N>
{
  public:
    GenericDataArray()
    {
        for(size_t i = 0; i < N; ++i) (*this)[i] = T();
    }
    ~GenericDataArray() {}

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& theArchive, const unsigned int version)
    {
        for(size_t i = 0; i < N; ++i) theArchive& (*this)[i];
    }
};

template <typename T, size_t N, size_t M>
class GenericDataArray_2D : public GenericDataArray<GenericDataArray<T, M>, N> {};

#endif
