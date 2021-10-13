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

#include "../Utils/ConsoleColor.h"
#include "../Utils/easylogging++.h"
#include <iostream>
#include <vector>

template <size_t size, typename T = float>
class GenericDataArray
{
  public:
    GenericDataArray() {}
    ~GenericDataArray() {}

    size_t getSize(){ return size;} 
    T& operator[](size_t position) { return data[position]; }

    T data[size];
};

template <size_t size, typename T = float>
inline GenericDataArray<size, T> fromVectorToGenericDataArray(const std::vector<T>& theInputVector)
{
    if(theInputVector.size() > size)
    {
        LOG(WARNING) << BOLDRED << __PRETTY_FUNCTION__ << " input vector size (" << theInputVector.size() << ") is greater than the array size (" << size
                     << ")\nSome data may be lost in the conversion";
    }
    GenericDataArray<size, T> theOutputVector;

    for(size_t it = 0; it < std::min(theInputVector.size(), size); ++it) { theOutputVector[it] = theInputVector[it]; }

    return theOutputVector;
}

#endif
