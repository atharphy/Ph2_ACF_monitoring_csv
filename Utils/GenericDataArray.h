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

    T& operator[](size_t position) { return data[position]; }

    T data[size];
};

template <size_t size, typename T = float>
inline std::vector<GenericDataArray<size, T>>&& fromVectorToGenericDataArray(const std::vector<T>& theInputVector)
{
    std::vector<GenericDataArray<size, T>> theOutputVector(theInputVector.size() / size + theInputVector.size() % size > 0 ? 1 : 0);

    for(size_t it = 0; it < theOutputVector.size() * size; ++it)
    {
        size_t outputVectorPosition = it / size;
        size_t outputArrayPosition  = it % size;
        if(it >= theInputVector.size())
            theOutputVector[outputVectorPosition][outputArrayPosition] = T();
        else
            theOutputVector[outputVectorPosition][outputArrayPosition] = theInputVector[it];
    }

    return std::move(theOutputVector);
}

#endif
