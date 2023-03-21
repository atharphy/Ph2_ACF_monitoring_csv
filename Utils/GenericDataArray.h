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
#include <vector>

template <size_t size, typename T = float>
class GenericDataArray
{
  public:
    GenericDataArray()
    {
        for(size_t i = 0; i < size; ++i) data[i] = T();
    }
    ~GenericDataArray() {}

    size_t getSize() { return size; }
    T&     operator[](size_t position) { return data[position]; }

    T data[size];

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& theArchive, const unsigned int version)
    {
        for(size_t i = 0; i < size; ++i) theArchive& data[i];
    }
};

// 2D generic array, accessed with () instead of [] to make overloading easier
template <size_t size_0, size_t size_1, typename T = float>
class GenericDataArray_2D
{
  public:
    GenericDataArray_2D()
    {
        for(size_t i = 0; i < size_0; ++i)
        {
            for(size_t j = 0; j < size_1; ++j) { data[i][j] = T(); }
        }
    }
    ~GenericDataArray_2D() {}

    size_t getSize_0() { return size_0; }
    size_t getSize_1() { return size_1; }
    T&     operator()(size_t position_0, size_t position_1) { return data[position_0][position_1]; }

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& theArchive, const unsigned int version)
    {
        for(size_t i = 0; i < size_0; ++i)
        {
            for(size_t j = 0; j < size_1; ++j) theArchive& data[i][j];
        }
    }

    T data[size_0][size_1];
};

#endif
