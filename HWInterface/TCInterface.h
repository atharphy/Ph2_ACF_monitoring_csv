/*!

        \file                                            CICInterface.h
        \brief                                           User Interface to the Cics
        \version                                         1.0

 */

#ifdef __TCUSB__

#ifndef __TCINTERFACE_H__
#define __TCINTERFACE_H__

#pragma once

#include "USB_a.h"
#include "USB_libusb.h"
/*!
 * \namespace Ph2_HwInterface
 * \brief Namespace regrouping all the interfaces to the hardware
 */
namespace Ph2_HwInterface
{
template <class T>
class TCInterface
{
  public:
    TCInterface();
    ~TCInterface() { delete[] fPtr; }
    T getInterface() const { return *fPtr; }
    // user-defined copy assignment (copy-and-swap idiom)
    // T& operator=(const T original) { *fPtr = *original.fPtr; return *this; }
    // user defined assignment operator
    TCInterface& operator=(const TCInterface& rhs)
    {
        *fPtr = *rhs.fPtr;
        return *this;
    }

  private:
    T* fPtr;
};

// constructor
template <class T>
TCInterface<T>::TCInterface()
{
    fPtr = new T();
}

} // namespace Ph2_HwInterface
#endif // TCINTERFACE_H
#endif
