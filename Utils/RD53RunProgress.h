/*!
  \file                  RD53RunProgress.h
  \brief                 Keeps track of run progress
  \author                Mauro DINARDO
  \version               1.0
  \date                  28/06/18
  Support:               email to mauro.dinardo@cern.ch
*/

#ifndef RD53RunProgress_H
#define RD53RunProgress_H

#include "easylogging++.h"

class RD53RunProgress
{
  public:
    static size_t& total()
    {
        static size_t value = 0;
        return value;
    }

    static size_t& current()
    {
        static size_t value = 0;
        return value;
    }

    static void update(size_t dataSize, bool display = false)
    {
        RD53RunProgress::current()++;
        if(display == true)
        {
            float fraction = 1. * RD53RunProgress::current() / RD53RunProgress::total();
            LOG(INFO) << CYAN << "---------------------------" << RESET;
            LOG(INFO) << GREEN << "****** Reading  data ******" << RESET;
            LOG(INFO) << GREEN << "n. 32 bit words : " << std::setw(9) << std::fixed << dataSize << RESET;
            LOG(INFO) << BOLDMAGENTA << ">>>> Progress : " << std::setw(5) << std::setprecision(1) << std::fixed << fraction * 100 << "% <<<<" << std::setprecision(-1) << RESET;
            LOG(INFO) << CYAN << "---------------------------" << RESET;
            if(fraction != 1)
                for(int i = 0; i < 5; i++) std::cout << "\x1b[A";
        }
    }
};

#endif
