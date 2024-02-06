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

template <size_t N, typename T = float>
class GenericDataArray
{
  public:
    GenericDataArray()
    {
        for(size_t i = 0; i < N; ++i) data[i] = T();
    }
    GenericDataArray(const GenericDataArray& theGenericDataArray)
    {
        for(size_t i = 0; i < N; ++i) data[i] = theGenericDataArray.data[i];
    }
    GenericDataArray(GenericDataArray&& theGenericDataArray)
    {
        std::copy(theGenericDataArray.begin(),theGenericDataArray.end(),begin());
    }
    GenericDataArray& operator=(const GenericDataArray& theGenericDataArray)
    {
        for(size_t i = 0; i < N; ++i) data[i] = theGenericDataArray.data[i];
        return *this;
    }
    GenericDataArray& operator=(GenericDataArray&& theGenericDataArray)
    {
        std::copy(theGenericDataArray.begin(),theGenericDataArray.end(),begin());
        return *this;
    }

    ~GenericDataArray() {}

    size_t size() { return N; }
    T&     operator[](size_t position) { return data[position]; }

    T data[N];

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& theArchive, const unsigned int version)
    {
        for(size_t i = 0; i < N; ++i) theArchive& data[i];
    }

    struct Iterator 
    {
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = T;
        using pointer           = T*;  // or also value_type*
        using reference         = T&;  // or also value_type&

        Iterator(pointer ptr) : m_ptr(ptr) {}

        reference operator*() const { return *m_ptr; }
        pointer operator->() { return m_ptr; }

        // Prefix increment
        Iterator& operator++() { m_ptr++; return *this; }  

        // Postfix increment
        Iterator operator++(int) { Iterator tmp = *this; ++(*this); return tmp; }

        friend bool operator== (const Iterator& a, const Iterator& b) { return a.m_ptr == b.m_ptr; };
        friend bool operator!= (const Iterator& a, const Iterator& b) { return a.m_ptr != b.m_ptr; };     

      private:
        pointer m_ptr;
    };

    Iterator begin() { return Iterator(&data[0]); }
    Iterator end()   { return Iterator(&data[N]); }
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
