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

#include <atomic>

class RD53RunProgress
{
  public:
    static std::atomic<size_t>& total()
    {
        static std::atomic<size_t> value{0};
        return value;
    }

    static std::atomic<size_t>& current()
    {
        static std::atomic<size_t> value{0};
        return value;
    }

    static double fraction()
    {
        const size_t totalValue = RD53RunProgress::total().load();
        if(totalValue == 0) return -1;
        const double value = static_cast<double>(RD53RunProgress::current().load()) / totalValue;
        return value > 1 ? 1 : value;
    }

    static void reset()
    {
        RD53RunProgress::total()   = 0;
        RD53RunProgress::current() = 0;
        RD53RunProgress::cursor()  = 0;
    }

    static bool& status()
    {
        static bool value = true;
        return value;
    }

    static size_t& cursor()
    {
        static size_t value = 0;
        return value;
    }

    static void turnON() { RD53RunProgress::status() = true; }
    static void turnOFF() { RD53RunProgress::status() = false; }

    static std::string makeProgressBar(const size_t nColumns)
    {
        static int        delta = 1;
        std::stringstream myString("");

        myString << BOLDBLUE << "[";
        for(auto i = 1u; i <= nColumns; i++)
        {
            if(i == RD53RunProgress::cursor())
                myString << BOLDYELLOW << "=";
            else
                myString << BOLDBLUE << "-";
        }
        myString << BOLDBLUE << "]";

        RD53RunProgress::cursor() += delta;
        if(RD53RunProgress::cursor() == nColumns + 1)
        {
            delta = -1;
            RD53RunProgress::cursor() += delta;
        }
        else if(RD53RunProgress::cursor() == 0)
        {
            delta = 1;
            RD53RunProgress::cursor() += delta;
        }

        return myString.str();
    }

    static void update(size_t dataSize, bool display = false)
    {
        const unsigned int nLines   = 5;      // Number of printed lines
        const unsigned int nColumns = 28 - 2; // Number of printed columns

        if(RD53RunProgress::status() == true)
        {
            RD53RunProgress::current()++;

            if(display == true)
            {
                const double fraction = RD53RunProgress::fraction();

                LOG(INFO) << CYAN << "******* " << GREEN << "Reading data" << CYAN << " *******" << RESET;
                LOG(INFO) << GREEN << "n. 32-bit words: " << BOLDYELLOW << std::setw(11) << std::fixed << dataSize << RESET;
                LOG(INFO) << BOLDMAGENTA << ">>> Progress: " << BOLDYELLOW << std::setw(9) << std::setprecision(1) << std::fixed << fraction * 100 << BOLDMAGENTA << "% <<<" << std::setprecision(-1)
                          << RESET;
                LOG(INFO) << BOLDBLUE << makeProgressBar(nColumns) << RESET;
                LOG(INFO) << CYAN << "****************************" << RESET;
                RD53Shared::resetDefaultFloat();

                if(fraction < 1)
                    for(auto i = 0u; i < nLines; i++) std::cout << "\x1b[A";
            }
        }
    }
};

#endif
